#pragma once
#include "order.h"
#include "order_book.h"
#include <string>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <thread>
#include <atomic>
#include <functional>
#include <queue>
#include <condition_variable>
#include <chrono>
#include <shared_mutex>
namespace mts {
namespace core {

// 前向宣告
struct ExecutionReport;
struct MarketDataSnapshot;
struct EngineStatistics;
struct InternalMessage;  // 新增：內部訊息結構

// 類型別名
using ExecutionReportPtr = std::shared_ptr<ExecutionReport>;
using MarketDataPtr = std::shared_ptr<MarketDataSnapshot>;
using OrderPtr = std::shared_ptr<Order>;

// 執行回報
struct ExecutionReport {
    OrderID orderId;
    OrderID counterOrderId;  // 對手單ID (若有撮合)
    Symbol symbol;
    Side side;
    OrderType orderType;
    Price price;
    Quantity originalQuantity;
    Quantity filledQuantity;
    Quantity remainingQuantity;
    Price executionPrice;    // 實際成交價
    Quantity executionQuantity; // 實際成交量
    OrderStatus status;
    std::string rejectReason;
    Timestamp timestamp;
    
    ExecutionReport(const Order& order);
    std::string toString() const;
};

// 市場行情快照
struct MarketDataSnapshot {
    Symbol symbol;
    Price bidPrice;
    Price askPrice;
    Quantity bidQuantity;
    Quantity askQuantity;
    Price lastTradePrice;
    Quantity lastTradeQuantity;
    Timestamp timestamp;
    
    MarketDataSnapshot(const Symbol& sym);
    std::string toString() const;
};

// 引擎統計資訊
struct EngineStatistics {
    std::atomic<uint64_t> ordersProcessed{0};
    std::atomic<uint64_t> tradesExecuted{0};
    std::atomic<uint64_t> ordersRejected{0};
    std::atomic<uint64_t> totalVolume{0};
    std::atomic<uint64_t> totalValue{0};  // 以分為單位
    
    // 效能統計
    std::atomic<uint64_t> minProcessingTimeNs{UINT64_MAX};
    std::atomic<uint64_t> maxProcessingTimeNs{0};
    std::atomic<uint64_t> totalProcessingTimeNs{0};
    
    std::chrono::steady_clock::time_point startTime;
    
    EngineStatistics();
    void reset();
    double getAverageProcessingTimeUs() const;
    double getThroughputPerSecond() const;
    std::string toString() const;
};

// 撮合引擎主類別
class MatchingEngine {
public:
    // 回調函式類型
    using ExecutionCallback = std::function<void(const ExecutionReportPtr&)>;
    using MarketDataCallback = std::function<void(const MarketDataPtr&)>;
    using ErrorCallback = std::function<void(const std::string&)>;
    
    // 撮合模式
    enum class MatchingMode {
        Continuous,     // 連續撮合
        Auction,        // 集合競價
        CallAuction     // 定期撮合
    };
    
private:
    // OrderBook 管理
    std::unordered_map<Symbol, std::unique_ptr<OrderBook>> orderBooks_;
    mutable std::shared_mutex orderBooksMutex_;
    
    // 訂單快取 (OrderID -> OrderBook Symbol)
    std::unordered_map<OrderID, Symbol> orderSymbolMap_;
    mutable std::mutex orderMapMutex_;
    
    // 執行緒模型
    std::atomic<bool> running_{false};
    std::thread processingThread_;
    
    // 內部訊息佇列 (修正設計衝突)
    std::queue<std::shared_ptr<struct InternalMessage>> incomingMessages_;
    std::mutex messageQueueMutex_;
    std::condition_variable messageQueueCV_;
    
    // 回調函式
    ExecutionCallback executionCallback_;
    MarketDataCallback marketDataCallback_;
    ErrorCallback errorCallback_;
    
    // 設定
    MatchingMode matchingMode_{MatchingMode::Continuous};
    bool enableRiskCheck_{true};
    bool enableMarketData_{true};
    std::chrono::microseconds maxProcessingTime_{1000}; // 1ms 超時
    
    // 統計
    mutable EngineStatistics statistics_;
    
    // 風險檢查參數
    Price maxOrderPrice_{10000.0};      // 最大訂單價格
    Quantity maxOrderQuantity_{1000000}; // 最大訂單數量
    uint32_t maxOrdersPerSymbol_{10000}; // 每個標的最大訂單數
    
public:
    MatchingEngine();
    ~MatchingEngine();
    
    // 禁用複製和移動
    MatchingEngine(const MatchingEngine&) = delete;
    MatchingEngine& operator=(const MatchingEngine&) = delete;
    MatchingEngine(MatchingEngine&&) = delete;
    MatchingEngine& operator=(MatchingEngine&&) = delete;
    
    // ===== 引擎生命週期 =====
    bool start();
    void stop();
    bool isRunning() const noexcept { return running_.load(); }
    
    // ===== 主要介面 =====
    
    // 處理新訂單 (異步)
    bool submitOrder(OrderPtr order);
    
    // 處理訂單取消 (異步)
    bool cancelOrder(OrderID orderId, const std::string& reason = "User requested");
    
    // 處理訂單修改 (異步)
    bool modifyOrder(OrderID orderId, Price newPrice, Quantity newQuantity);
    
    // 同步處理訂單 (主要用於測試)
    ExecutionReportPtr processOrderSync(OrderPtr order);
    ExecutionReportPtr cancelOrderSync(OrderID orderId, const std::string& reason = "User requested");
    
    // ===== 查詢介面 =====
    
    // 取得特定標的的 OrderBook
    std::shared_ptr<const OrderBook> getOrderBook(const Symbol& symbol) const;
    
    // 取得市場行情
    MarketDataPtr getMarketData(const Symbol& symbol) const;
    
    // 取得所有交易標的
    std::vector<Symbol> getAllSymbols() const;
    
    // 查詢訂單狀態
    OrderPtr findOrder(OrderID orderId) const;
    
    // ===== 回調設定 =====
    void setExecutionCallback(ExecutionCallback callback) { 
        executionCallback_ = std::move(callback); 
    }
    
    void setMarketDataCallback(MarketDataCallback callback) { 
        marketDataCallback_ = std::move(callback); 
    }
    
    void setErrorCallback(ErrorCallback callback) { 
        errorCallback_ = std::move(callback); 
    }
    
    // ===== 設定方法 =====
    void setMatchingMode(MatchingMode mode) { matchingMode_ = mode; }
    MatchingMode getMatchingMode() const { return matchingMode_; }
    
    void enableRiskCheck(bool enable) { enableRiskCheck_ = enable; }
    bool isRiskCheckEnabled() const { return enableRiskCheck_; }
    
    void enableMarketData(bool enable) { enableMarketData_ = enable; }
    bool isMarketDataEnabled() const { return enableMarketData_; }
    
    void setMaxProcessingTime(std::chrono::microseconds maxTime) { 
        maxProcessingTime_ = maxTime; 
    }
    
    // 風險檢查參數設定
    void setMaxOrderPrice(Price maxPrice) { maxOrderPrice_ = maxPrice; }
    void setMaxOrderQuantity(Quantity maxQty) { maxOrderQuantity_ = maxQty; }
    void setMaxOrdersPerSymbol(uint32_t maxOrders) { maxOrdersPerSymbol_ = maxOrders; }
    
    // ===== 統計資訊 =====
    const EngineStatistics& getStatistics() const { return statistics_; }
    void resetStatistics() { statistics_.reset(); }
    
    // ===== 工具方法 =====
    std::string toString() const;
    void dumpOrderBooks() const;  // 除錯用，印出所有 OrderBook 狀態
    
    // ===== 測試介面 =====
    #ifdef MTS_TESTING
    // 僅供測試使用的方法
    void waitForOrderProcessing() const;
    size_t getPendingOrderCount() const;
    void processAllPendingOrders();
    #endif

private:
    // ===== 內部處理邏輯 =====
    
    // 主處理執行緒
    void processingLoop();
    
    // 內部訊息處理
    ExecutionReportPtr processInternalMessage(std::shared_ptr<struct InternalMessage> message);
    
    // 訂單處理
    ExecutionReportPtr processNewOrder(OrderPtr order);
    ExecutionReportPtr processCancelOrder(OrderID orderId, const std::string& reason);
    ExecutionReportPtr processModifyOrder(OrderID orderId, Price newPrice, Quantity newQuantity);
    
    // 取得或建立 OrderBook
    OrderBook* getOrCreateOrderBook(const Symbol& symbol);
    
    // 風險檢查
    bool performRiskCheck(const Order& order, std::string& rejectReason) const;
    bool validateOrderBasic(const Order& order, std::string& rejectReason) const;
    bool validateOrderSize(const Order& order, std::string& rejectReason) const;
    bool validateOrderPrice(const Order& order, std::string& rejectReason) const;
    bool validateSymbolLimits(const Symbol& symbol, std::string& rejectReason) const;
    
    // 回調通知
    void notifyExecution(const ExecutionReportPtr& report);
    void notifyMarketData(const Symbol& symbol);
    void notifyError(const std::string& error);
    
    // 統計更新
    void updateStatistics(const ExecutionReportPtr& report, 
                         std::chrono::nanoseconds processingTime);
    
    // 建立執行回報
    ExecutionReportPtr createExecutionReport(const Order& order, 
                                           OrderStatus status,
                                           const std::string& rejectReason = "") const;
    
    ExecutionReportPtr createTradeExecutionReport(const Order& order,
                                                const TradePtr& trade) const;
    
    // 建立市場行情
    MarketDataPtr createMarketData(const Symbol& symbol) const;
    
    // 工具方法
    std::string generateErrorMessage(const std::string& operation, 
                                   const std::string& details) const;
    
    // 清理資源
    void cleanup();
};

// ===== 工具函式 =====

// 執行回報狀態轉換
std::string executionReportStatusToString(OrderStatus status);
OrderStatus orderStatusFromString(const std::string& statusStr);

// 撮合模式轉換
std::string matchingModeToString(MatchingEngine::MatchingMode mode);
MatchingEngine::MatchingMode matchingModeFromString(const std::string& modeStr);

// 執行報告格式化
std::string formatExecutionReport(const ExecutionReport& report);
std::string formatMarketData(const MarketDataSnapshot& snapshot);

} // namespace core
} // namespace mts