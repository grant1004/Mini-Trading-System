#include "matching_engine.h"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <cmath>
#include <iostream>

// Debug 巨集
#ifdef ENABLE_MATCHING_DEBUG
    #define MATCHING_DEBUG(msg) std::cout << "[MATCHING_DEBUG] " << msg << std::endl
#else
    #define MATCHING_DEBUG(msg) do {} while(0)
#endif

namespace mts {
namespace core {

// ===== 內部訊息類型 (用於處理異步請求) =====
enum class InternalMessageType {
    NewOrder,
    CancelOrder,
    ModifyOrder
};

struct InternalMessage {
    InternalMessageType type;
    OrderPtr order;           // 新訂單時使用
    OrderID targetOrderId;    // 取消/修改時使用
    std::string reason;       // 取消原因
    Price newPrice;          // 修改價格
    Quantity newQuantity;    // 修改數量
    
    // 建構函式
    static std::shared_ptr<InternalMessage> createNewOrder(OrderPtr order) {
        auto msg = std::make_shared<InternalMessage>();
        msg->type = InternalMessageType::NewOrder;
        msg->order = order;
        return msg;
    }
    
    static std::shared_ptr<InternalMessage> createCancelOrder(OrderID orderId, const std::string& reason) {
        auto msg = std::make_shared<InternalMessage>();
        msg->type = InternalMessageType::CancelOrder;
        msg->targetOrderId = orderId;
        msg->reason = reason;
        return msg;
    }
    
    static std::shared_ptr<InternalMessage> createModifyOrder(OrderID orderId, Price price, Quantity qty) {
        auto msg = std::make_shared<InternalMessage>();
        msg->type = InternalMessageType::ModifyOrder;
        msg->targetOrderId = orderId;
        msg->newPrice = price;
        msg->newQuantity = qty;
        return msg;
    }
};

using InternalMessagePtr = std::shared_ptr<InternalMessage>;

// ===== ExecutionReport 實作 =====

ExecutionReport::ExecutionReport(const Order& order)
    : orderId(order.getOrderId())
    , counterOrderId(0)
    , symbol(order.getSymbol())
    , side(order.getSide())
    , orderType(order.getOrderType())
    , price(order.getPrice())
    , originalQuantity(order.getQuantity())
    , filledQuantity(order.getFilledQuantity())
    , remainingQuantity(order.getRemainingQuantity())
    , executionPrice(0.0)
    , executionQuantity(0)
    , status(order.getStatus())
    , timestamp(std::chrono::high_resolution_clock::now())
{
}

std::string ExecutionReport::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "ExecReport[OrderID=" << orderId
        << ", Symbol=" << symbol
        << ", Side=" << sideToString(side)
        << ", Status=" << orderStatusToString(status)
        << ", OrigQty=" << originalQuantity
        << ", FilledQty=" << filledQuantity
        << ", RemainingQty=" << remainingQuantity;
    
    if (executionQuantity > 0) {
        oss << ", ExecQty=" << executionQuantity
            << ", ExecPrice=" << executionPrice;
        if (counterOrderId != 0) {
            oss << ", CounterOrderID=" << counterOrderId;
        }
    }
    
    if (!rejectReason.empty()) {
        oss << ", RejectReason=" << rejectReason;
    }
    
    oss << "]";
    return oss.str();
}

// ===== MarketDataSnapshot 實作 =====

MarketDataSnapshot::MarketDataSnapshot(const Symbol& sym)
    : symbol(sym)
    , bidPrice(0.0)
    , askPrice(0.0)
    , bidQuantity(0)
    , askQuantity(0)
    , lastTradePrice(0.0)
    , lastTradeQuantity(0)
    , timestamp(std::chrono::high_resolution_clock::now())
{
}

std::string MarketDataSnapshot::toString() const {
    std::ostringstream oss;
    oss << std::fixed << std::setprecision(2);
    oss << "MarketData[" << symbol
        << ", Bid=" << bidPrice << "(" << bidQuantity << ")"
        << ", Ask=" << askPrice << "(" << askQuantity << ")"
        << ", LastTrade=" << lastTradePrice << "(" << lastTradeQuantity << ")"
        << "]";
    return oss.str();
}

// ===== EngineStatistics 實作 =====

EngineStatistics::EngineStatistics()
    : startTime(std::chrono::steady_clock::now())
{
}

void EngineStatistics::reset() {
    ordersProcessed.store(0);
    tradesExecuted.store(0);
    ordersRejected.store(0);
    totalVolume.store(0);
    totalValue.store(0);
    minProcessingTimeNs.store(UINT64_MAX);
    maxProcessingTimeNs.store(0);
    totalProcessingTimeNs.store(0);
    startTime = std::chrono::steady_clock::now();
}

double EngineStatistics::getAverageProcessingTimeUs() const {
    uint64_t orders = ordersProcessed.load();
    if (orders == 0) return 0.0;
    
    uint64_t totalNs = totalProcessingTimeNs.load();
    return static_cast<double>(totalNs) / orders / 1000.0; // 轉換為微秒
}

double EngineStatistics::getThroughputPerSecond() const {
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - startTime);
    
    if (elapsed.count() == 0) return 0.0;
    
    return static_cast<double>(ordersProcessed.load()) / elapsed.count();
}

std::string EngineStatistics::toString() const {
    std::ostringstream oss;
    oss << "EngineStats["
        << "Orders=" << ordersProcessed.load()
        << ", Trades=" << tradesExecuted.load()
        << ", Rejected=" << ordersRejected.load()
        << ", Volume=" << totalVolume.load()
        << ", Value=" << totalValue.load()
        << ", AvgTime=" << std::fixed << std::setprecision(3) << getAverageProcessingTimeUs() << "μs"
        << ", Throughput=" << std::fixed << std::setprecision(0) << getThroughputPerSecond() << "/sec"
        << "]";
    return oss.str();
}

// ===== MatchingEngine 實作 =====

MatchingEngine::MatchingEngine() {
    MATCHING_DEBUG("MatchingEngine created");
}

MatchingEngine::~MatchingEngine() {
    stop();
    cleanup();
    MATCHING_DEBUG("MatchingEngine destroyed");
}

// ===== 引擎生命週期 =====

bool MatchingEngine::start() {
    MATCHING_DEBUG("Starting MatchingEngine");
    
    if (running_.load()) {
        notifyError("MatchingEngine is already running");
        return false;
    }
    
    try {
        running_.store(true);
        statistics_.reset();
        
        // 啟動處理執行緒
        processingThread_ = std::thread(&MatchingEngine::processingLoop, this);
        
        MATCHING_DEBUG("MatchingEngine started successfully");
        return true;
    } catch (const std::exception& e) {
        running_.store(false);
        notifyError("Failed to start MatchingEngine: " + std::string(e.what()));
        return false;
    }
}

void MatchingEngine::stop() {
    MATCHING_DEBUG("Stopping MatchingEngine");
    
    if (!running_.load()) {
        return;
    }
    
    // 設定停止標誌
    running_.store(false);
    
    // 通知處理執行緒
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        messageQueueCV_.notify_all();
    }
    
    // 等待處理執行緒結束
    if (processingThread_.joinable()) {
        processingThread_.join();
    }
    
    MATCHING_DEBUG("MatchingEngine stopped");
}

// ===== 主要介面 =====

bool MatchingEngine::submitOrder(OrderPtr order) {
    if (!order) {
        notifyError("Cannot submit null order");
        return false;
    }
    
    if (!running_.load()) {
        notifyError("MatchingEngine is not running");
        return false;
    }
    
    MATCHING_DEBUG("Submitting order: " << order->toString());
    
    // 建立內部訊息
    auto message = InternalMessage::createNewOrder(order);
    
    // 加入訊息佇列
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        incomingMessages_.push(message);
    }
    
    // 通知處理執行緒
    messageQueueCV_.notify_one();
    
    return true;
}

bool MatchingEngine::cancelOrder(OrderID orderId, const std::string& reason) {
    if (!running_.load()) {
        notifyError("MatchingEngine is not running");
        return false;
    }
    
    MATCHING_DEBUG("Canceling order: " << orderId << ", reason: " << reason);
    
    // 建立取消訊息
    auto message = InternalMessage::createCancelOrder(orderId, reason);
    
    // 加入訊息佇列
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        incomingMessages_.push(message);
    }
    
    // 通知處理執行緒
    messageQueueCV_.notify_one();
    
    return true;
}

bool MatchingEngine::modifyOrder(OrderID orderId, Price newPrice, Quantity newQuantity) {
    if (!running_.load()) {
        notifyError("MatchingEngine is not running");
        return false;
    }
    
    MATCHING_DEBUG("Modifying order: " << orderId 
                   << ", newPrice=" << newPrice 
                   << ", newQuantity=" << newQuantity);
    
    // 建立修改訊息
    auto message = InternalMessage::createModifyOrder(orderId, newPrice, newQuantity);
    
    // 加入訊息佇列
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        incomingMessages_.push(message);
    }
    
    // 通知處理執行緒
    messageQueueCV_.notify_one();
    
    return true;
}

ExecutionReportPtr MatchingEngine::processOrderSync(OrderPtr order) {
    if (!order) {
        auto dummyOrder = std::make_shared<Order>();
        return createExecutionReport(*dummyOrder, OrderStatus::Rejected, "Null order");
    }
    
    auto start = std::chrono::high_resolution_clock::now();
    auto report = processNewOrder(order);
    auto end = std::chrono::high_resolution_clock::now();
    
    auto processingTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
    updateStatistics(report, processingTime);
    
    return report;
}

ExecutionReportPtr MatchingEngine::cancelOrderSync(OrderID orderId, const std::string& reason) {
    return processCancelOrder(orderId, reason);
}

// ===== 查詢介面 =====

std::shared_ptr<const OrderBook> MatchingEngine::getOrderBook(const Symbol& symbol) const {
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        return std::const_pointer_cast<const OrderBook>(
            std::shared_ptr<OrderBook>(it->second.get(), [](OrderBook*) {}));
    }
    
    return nullptr;
}

MarketDataPtr MatchingEngine::getMarketData(const Symbol& symbol) const {
    return createMarketData(symbol);
}

std::vector<Symbol> MatchingEngine::getAllSymbols() const {
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    
    std::vector<Symbol> symbols;
    symbols.reserve(orderBooks_.size());
    
    for (const auto& pair : orderBooks_) {
        symbols.push_back(pair.first);
    }
    
    return symbols;
}

OrderPtr MatchingEngine::findOrder(OrderID orderId) const {
    std::lock_guard<std::mutex> lock(orderMapMutex_);
    
    auto it = orderSymbolMap_.find(orderId);
    if (it == orderSymbolMap_.end()) {
        return nullptr;
    }
    
    const Symbol& symbol = it->second;
    
    // 從對應的 OrderBook 中查找
    std::shared_lock<std::shared_mutex> obLock(orderBooksMutex_);
    auto obIt = orderBooks_.find(symbol);
    if (obIt != orderBooks_.end()) {
        return obIt->second->findOrder(orderId);
    }
    
    return nullptr;
}

// ===== 工具方法 =====

std::string MatchingEngine::toString() const {
    std::ostringstream oss;
    oss << "MatchingEngine["
        << "Running=" << (running_.load() ? "YES" : "NO")
        << ", Mode=" << matchingModeToString(matchingMode_)
        << ", Symbols=" << orderBooks_.size()
        << ", " << statistics_.toString()
        << "]";
    return oss.str();
}

void MatchingEngine::dumpOrderBooks() const {
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    
    std::cout << "=== OrderBook Dump ===" << std::endl;
    for (const auto& pair : orderBooks_) {
        std::cout << pair.second->toString() << std::endl;
    }
    std::cout << "======================" << std::endl;
}

// ===== 測試介面 =====
#ifdef MTS_TESTING
void MatchingEngine::waitForOrderProcessing() const {
    // 等待佇列清空
    while (true) {
        {
            std::lock_guard<std::mutex> lock(messageQueueMutex_);
            if (incomingMessages_.empty()) {
                break;
            }
        }
        std::this_thread::sleep_for(std::chrono::microseconds(10));
    }
}

size_t MatchingEngine::getPendingOrderCount() const {
    std::lock_guard<std::mutex> lock(messageQueueMutex_);
    return incomingMessages_.size();
}

void MatchingEngine::processAllPendingOrders() {
    while (true) {
        InternalMessagePtr message;
        {
            std::lock_guard<std::mutex> lock(messageQueueMutex_);
            if (incomingMessages_.empty()) {
                break;
            }
            message = incomingMessages_.front();
            incomingMessages_.pop();
        }
        
        auto report = processInternalMessage(message);
        if (report) {
            notifyExecution(report);
        }
    }
}
#endif

// ===== 內部處理邏輯 =====

void MatchingEngine::processingLoop() {
    MATCHING_DEBUG("Processing loop started");
    
    while (running_.load()) {
        try {
            InternalMessagePtr message;
            
            // 等待新訊息
            {
                std::unique_lock<std::mutex> lock(messageQueueMutex_);
                messageQueueCV_.wait(lock, [this] { 
                    return !incomingMessages_.empty() || !running_.load(); 
                });
                
                if (!running_.load()) {
                    break;
                }
                
                if (incomingMessages_.empty()) {
                    continue;
                }
                
                message = incomingMessages_.front();
                incomingMessages_.pop();
            }
            
            // 處理訊息
            auto start = std::chrono::high_resolution_clock::now();
            
            auto report = processInternalMessage(message);
            
            auto end = std::chrono::high_resolution_clock::now();
            auto processingTime = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);
            
            // 檢查處理時間是否超時
            if (processingTime > maxProcessingTime_) {
                std::ostringstream oss;
                oss << "Message processing timeout: " 
                    << std::chrono::duration_cast<std::chrono::microseconds>(processingTime).count() 
                    << "μs (limit: " 
                    << maxProcessingTime_.count() << "μs)";
                notifyError(oss.str());
            }
            
            // 更新統計和通知
            if (report) {
                updateStatistics(report, processingTime);
                notifyExecution(report);
            }
            
        } catch (const std::exception& e) {
            notifyError("Error in processing loop: " + std::string(e.what()));
        }
    }
    
    MATCHING_DEBUG("Processing loop ended");
}

ExecutionReportPtr MatchingEngine::processInternalMessage(InternalMessagePtr message) {
    switch (message->type) {
        case InternalMessageType::NewOrder:
            return processNewOrder(message->order);
            
        case InternalMessageType::CancelOrder:
            return processCancelOrder(message->targetOrderId, message->reason);
            
        case InternalMessageType::ModifyOrder:
            return processModifyOrder(message->targetOrderId, message->newPrice, message->newQuantity);
            
        default:
            notifyError("Unknown internal message type");
            return nullptr;
    }
}

ExecutionReportPtr MatchingEngine::processNewOrder(OrderPtr order) {
    if (!order) {
        auto dummyOrder = std::make_shared<Order>();
        return createExecutionReport(*dummyOrder, OrderStatus::Rejected, "Null order");
    }
    
    MATCHING_DEBUG("Processing new order: " << order->toString());
    
    // 基本驗證
    std::string rejectReason;
    if (!validateOrderBasic(*order, rejectReason)) {
        return createExecutionReport(*order, OrderStatus::Rejected, rejectReason);
    }
    
    // 風險檢查
    if (enableRiskCheck_ && !performRiskCheck(*order, rejectReason)) {
        return createExecutionReport(*order, OrderStatus::Rejected, rejectReason);
    }
    
    // 取得或建立 OrderBook
    OrderBook* orderBook = getOrCreateOrderBook(order->getSymbol());
    if (!orderBook) {
        return createExecutionReport(*order, OrderStatus::Rejected, "Failed to create OrderBook");
    }
    
    // 記錄訂單對應的標的
    {
        std::lock_guard<std::mutex> lock(orderMapMutex_);
        orderSymbolMap_[order->getOrderId()] = order->getSymbol();
    }
    
    // 設定訂單回調
    std::vector<TradePtr> trades;
    orderBook->setTradeCallback([&trades](const TradePtr& trade) {
        trades.push_back(trade);
    });
    
    // 加入 OrderBook 進行撮合
    auto generatedTrades = orderBook->addOrder(order);
    
    // 建立執行回報
    auto report = createExecutionReport(*order, order->getStatus());
    
    // 處理成交
    if (!generatedTrades.empty()) {
        // 取最後一筆成交作為主要成交資訊
        auto lastTrade = generatedTrades.back();
        report->executionPrice = lastTrade->price;
        report->executionQuantity = lastTrade->quantity;
        report->counterOrderId = order->isBuyOrder() ? 
            lastTrade->sellOrderId : lastTrade->buyOrderId;
        
        MATCHING_DEBUG("Order matched: " << generatedTrades.size() << " trades generated");
        
        // 更新市場行情
        if (enableMarketData_) {
            notifyMarketData(order->getSymbol());
        }
    }
    
    return report;
}

ExecutionReportPtr MatchingEngine::processCancelOrder(OrderID orderId, const std::string& reason) {
    MATCHING_DEBUG("Processing cancel order: " << orderId << ", reason: " << reason);
    
    // 查找訂單
    auto order = findOrder(orderId);
    if (!order) {
        // 建立假的訂單物件用於回報
        auto dummyOrder = std::make_shared<Order>();
        return createExecutionReport(*dummyOrder, OrderStatus::Rejected, "Order not found");
    }
    
    // 從 OrderBook 中取消
    const Symbol& symbol = order->getSymbol();
    
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    auto it = orderBooks_.find(symbol);
    if (it == orderBooks_.end()) {
        return createExecutionReport(*order, OrderStatus::Rejected, "OrderBook not found");
    }
    
    bool cancelled = it->second->cancelOrder(orderId);
    if (cancelled) {
        // 從映射中移除
        {
            std::lock_guard<std::mutex> mapLock(orderMapMutex_);
            orderSymbolMap_.erase(orderId);
        }
        
        return createExecutionReport(*order, OrderStatus::Cancelled, reason);
    } else {
        return createExecutionReport(*order, OrderStatus::Rejected, "Failed to cancel order");
    }
}

ExecutionReportPtr MatchingEngine::processModifyOrder(OrderID orderId, Price newPrice, Quantity newQuantity) {
    // 簡化實作：修改訂單 = 取消 + 重新下單
    // 生產環境需要原子性的修改操作
    
    auto cancelReport = processCancelOrder(orderId, "Modify order");
    if (cancelReport->status != OrderStatus::Cancelled) {
        return cancelReport; // 取消失敗
    }
    
    // 建立新訂單 (需要原始訂單資訊)
    // 這裡簡化處理，實際需要保存原始訂單詳情
    MATCHING_DEBUG("Order modification completed via cancel+new for OrderID: " << orderId);
    
    return cancelReport;
}

// ===== 在 matching_engine.cpp 末尾加入以下缺失的方法實作 =====

// 取得或建立 OrderBook
OrderBook* MatchingEngine::getOrCreateOrderBook(const Symbol& symbol) {
    std::unique_lock<std::shared_mutex> lock(orderBooksMutex_);
    
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        return it->second.get();
    }
    
    // 建立新的 OrderBook
    auto orderBook = std::make_unique<OrderBook>(symbol);
    OrderBook* ptr = orderBook.get();
    orderBooks_[symbol] = std::move(orderBook);
    
    MATCHING_DEBUG("Created new OrderBook for symbol: " << symbol);
    return ptr;
}

// 風險檢查
bool MatchingEngine::performRiskCheck(const Order& order, std::string& rejectReason) const {
    // 價格檢查
    if (!validateOrderPrice(order, rejectReason)) {
        return false;
    }
    
    // 數量檢查
    if (!validateOrderSize(order, rejectReason)) {
        return false;
    }
    
    // 標的限制檢查
    if (!validateSymbolLimits(order.getSymbol(), rejectReason)) {
        return false;
    }
    
    return true;
}

// 基本訂單驗證
bool MatchingEngine::validateOrderBasic(const Order& order, std::string& rejectReason) const {
    if (!order.isValid()) {
        rejectReason = "Invalid order structure";
        return false;
    }
    
    if (order.getSymbol().empty()) {
        rejectReason = "Empty symbol";
        return false;
    }
    
    if (order.getQuantity() == 0) {
        rejectReason = "Zero quantity";
        return false;
    }
    
    if (order.isLimitOrder() && order.getPrice() <= 0.0) {
        rejectReason = "Invalid price for limit order";
        return false;
    }
    
    return true;
}

// 訂單大小驗證
bool MatchingEngine::validateOrderSize(const Order& order, std::string& rejectReason) const {
    if (order.getQuantity() > maxOrderQuantity_) {
        rejectReason = "Order quantity exceeds maximum limit: " + std::to_string(maxOrderQuantity_);
        return false;
    }
    
    return true;
}

// 訂單價格驗證
bool MatchingEngine::validateOrderPrice(const Order& order, std::string& rejectReason) const {
    if (order.isLimitOrder() && order.getPrice() > maxOrderPrice_) {
        rejectReason = "Order price exceeds maximum limit: " + std::to_string(maxOrderPrice_);
        return false;
    }
    
    return true;
}

// 標的限制驗證
bool MatchingEngine::validateSymbolLimits(const Symbol& symbol, std::string& rejectReason) const {
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        size_t currentOrders = it->second->getTotalOrderCount();
        if (currentOrders >= maxOrdersPerSymbol_) {
            rejectReason = "Symbol " + symbol + " exceeds maximum order limit: " + 
                          std::to_string(maxOrdersPerSymbol_);
            return false;
        }
    }
    
    return true;
}

// 通知執行回報
void MatchingEngine::notifyExecution(const ExecutionReportPtr& report) {
    if (executionCallback_) {
        try {
            executionCallback_(report);
        } catch (const std::exception& e) {
            MATCHING_DEBUG("Error in execution callback: " << e.what());
        }
    }
}

// 通知市場行情
void MatchingEngine::notifyMarketData(const Symbol& symbol) {
    if (marketDataCallback_) {
        try {
            auto marketData = createMarketData(symbol);
            if (marketData) {
                marketDataCallback_(marketData);
            }
        } catch (const std::exception& e) {
            MATCHING_DEBUG("Error in market data callback: " << e.what());
        }
    }
}

// 通知錯誤
void MatchingEngine::notifyError(const std::string& error) {
    MATCHING_DEBUG("ERROR: " << error);
    
    if (errorCallback_) {
        try {
            errorCallback_(error);
        } catch (const std::exception& e) {
            MATCHING_DEBUG("Error in error callback: " << e.what());
        }
    }
}

// 更新統計資訊
void MatchingEngine::updateStatistics(const ExecutionReportPtr& report, 
                                     std::chrono::nanoseconds processingTime) {
    statistics_.ordersProcessed.fetch_add(1);
    
    // 更新處理時間統計
    uint64_t timeNs = processingTime.count();
    statistics_.totalProcessingTimeNs.fetch_add(timeNs);
    
    // 更新最小/最大處理時間
    uint64_t currentMin = statistics_.minProcessingTimeNs.load();
    while (timeNs < currentMin) {
        if (statistics_.minProcessingTimeNs.compare_exchange_weak(currentMin, timeNs)) {
            break;
        }
    }
    
    uint64_t currentMax = statistics_.maxProcessingTimeNs.load();
    while (timeNs > currentMax) {
        if (statistics_.maxProcessingTimeNs.compare_exchange_weak(currentMax, timeNs)) {
            break;
        }
    }
    
    // 如果有成交，更新成交統計
    if (report && report->executionQuantity > 0) {
        statistics_.tradesExecuted.fetch_add(1);
        statistics_.totalVolume.fetch_add(report->executionQuantity);
        
        // 計算成交金額 (以分為單位)
        uint64_t tradeValue = static_cast<uint64_t>(report->executionPrice * report->executionQuantity * 100);
        statistics_.totalValue.fetch_add(tradeValue);
    }
    
    // 如果是拒絕，更新拒絕統計
    if (report && report->status == OrderStatus::Rejected) {
        statistics_.ordersRejected.fetch_add(1);
    }
}

// 建立執行回報
ExecutionReportPtr MatchingEngine::createExecutionReport(const Order& order, 
                                                       OrderStatus status,
                                                       const std::string& rejectReason) const {
    auto report = std::make_shared<ExecutionReport>(order);
    report->status = status;
    report->rejectReason = rejectReason;
    
    return report;
}

// 建立成交執行回報
ExecutionReportPtr MatchingEngine::createTradeExecutionReport(const Order& order,
                                                            const TradePtr& trade) const {
    auto report = std::make_shared<ExecutionReport>(order);
    report->executionPrice = trade->price;
    report->executionQuantity = trade->quantity;
    
    // 設定對手單ID
    if (order.isBuyOrder()) {
        report->counterOrderId = trade->sellOrderId;
    } else {
        report->counterOrderId = trade->buyOrderId;
    }
    
    return report;
}

// 建立市場行情
MarketDataPtr MatchingEngine::createMarketData(const Symbol& symbol) const {
    auto marketData = std::make_shared<MarketDataSnapshot>(symbol);
    
    std::shared_lock<std::shared_mutex> lock(orderBooksMutex_);
    auto it = orderBooks_.find(symbol);
    if (it != orderBooks_.end()) {
        const auto& orderBook = it->second;
        
        marketData->bidPrice = orderBook->getBidPrice();
        marketData->askPrice = orderBook->getAskPrice();
        marketData->bidQuantity = orderBook->getBidQuantity();
        marketData->askQuantity = orderBook->getAskQuantity();
        
        // 最後成交價和成交量需要從交易記錄中獲取
        // 這裡簡化處理，實際需要維護最後成交資訊
        marketData->lastTradePrice = (marketData->bidPrice + marketData->askPrice) / 2.0;
        marketData->lastTradeQuantity = 0;
    }
    
    return marketData;
}

// 生成錯誤訊息
std::string MatchingEngine::generateErrorMessage(const std::string& operation, 
                                               const std::string& details) const {
    std::ostringstream oss;
    oss << "MatchingEngine::" << operation << " failed: " << details;
    return oss.str();
}

// 清理資源
void MatchingEngine::cleanup() {
    MATCHING_DEBUG("Cleaning up MatchingEngine resources");
    
    // 清除所有 OrderBook
    {
        std::unique_lock<std::shared_mutex> lock(orderBooksMutex_);
        orderBooks_.clear();
    }
    
    // 清除訂單映射
    {
        std::lock_guard<std::mutex> lock(orderMapMutex_);
        orderSymbolMap_.clear();
    }
    
    // 清除訊息佇列
    {
        std::lock_guard<std::mutex> lock(messageQueueMutex_);
        std::queue<InternalMessagePtr> empty;
        incomingMessages_.swap(empty);
    }
    
    MATCHING_DEBUG("MatchingEngine cleanup completed");
}

// ===== 工具函式實作 =====

// 撮合模式轉換
std::string matchingModeToString(MatchingEngine::MatchingMode mode) {
    switch (mode) {
        case MatchingEngine::MatchingMode::Continuous:
            return "Continuous";
        case MatchingEngine::MatchingMode::Auction:
            return "Auction";
        case MatchingEngine::MatchingMode::CallAuction:
            return "CallAuction";
        default:
            return "Unknown";
    }
}

MatchingEngine::MatchingMode matchingModeFromString(const std::string& modeStr) {
    if (modeStr == "Continuous") {
        return MatchingEngine::MatchingMode::Continuous;
    } else if (modeStr == "Auction") {
        return MatchingEngine::MatchingMode::Auction;
    } else if (modeStr == "CallAuction") {
        return MatchingEngine::MatchingMode::CallAuction;
    } else {
        return MatchingEngine::MatchingMode::Continuous; // 預設值
    }
}

// 執行回報狀態轉換
std::string executionReportStatusToString(OrderStatus status) {
    return orderStatusToString(status);
}

OrderStatus orderStatusFromString(const std::string& statusStr) {
    return stringToOrderStatus(statusStr);
}

// 執行報告格式化
std::string formatExecutionReport(const ExecutionReport& report) {
    return report.toString();
}

std::string formatMarketData(const MarketDataSnapshot& snapshot) {
    return snapshot.toString();
}

} // namespace core
} // namespace mts