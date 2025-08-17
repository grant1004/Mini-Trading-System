#include "trading_system.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <chrono>
#include <thread>
   
    



TradingSystem::TradingSystem(int port) 
    : serverPort_(port) {
    std::cout << "🌐 Trading System created on port " << port << std::endl;
}

TradingSystem:: ~TradingSystem(){
    stop();
    std::cout << "🧹 Trading System destroyed" << std::endl;
}

// ===== 系統生命週期 =====

bool TradingSystem::start() {
    std::cout << "🚀 Starting Trading System on port " << serverPort_ << std::endl;
    
    // 1. 初始化撮合引擎
    if (!initializeMatchingEngine()) {
        std::cerr << "❌ Failed to initialize MatchingEngine" << std::endl;
        return false;
    }
    
    // 2. 初始化 TCP 服務器
    if (!initializeTcpServer()) {
        std::cerr << "❌ Failed to initialize TCP Server" << std::endl;
        return false;
    }
    
    running_ = true;
    std::cout << "✅ Trading System started successfully!" << std::endl;
    std::cout << "📊 Waiting for client connections..." << std::endl;
    return true;
}

void TradingSystem::stop() {
    if (!running_.load()) {
        return;
    }
    
    std::cout << "🛑 Stopping Trading System..." << std::endl;
    running_ = false;
    
    // 1. 停止 TCP 服務器 (不再接受新連線)
    if (tcpServer_) {
        tcpServer_->stop();
    }
    
    // 2. 清理所有客戶端 Session
    cleanupResources();
    
    // 3. 停止撮合引擎
    if (matchingEngine_) {
        matchingEngine_->stop();
    }
    
    std::cout << "✅ Trading System stopped" << std::endl;
}

// ===== 初始化方法 =====

bool TradingSystem::initializeMatchingEngine() {
    try {
        matchingEngine_ = std::make_unique<MatchingEngine>();
        
        // 設定回調函式
        matchingEngine_->setExecutionCallback(
            [this](const ExecutionReportPtr& report) {
                handleExecutionReport(report);
            }
        );
        
        matchingEngine_->setErrorCallback(
            [this](const std::string& error) {
                handleMatchingEngineError(error);
            }
        );
        
        // 設定風險參數
        matchingEngine_->setMaxOrderPrice(10000.0);
        matchingEngine_->setMaxOrderQuantity(1000000);
        matchingEngine_->enableRiskCheck(true);
        matchingEngine_->enableMarketData(true);
        
        // 啟動撮合引擎
        return matchingEngine_->start();
        
    } catch (const std::exception& e) {
        std::cerr << "MatchingEngine initialization error: " << e.what() << std::endl;
        return false;
    }
}


bool TradingSystem::initializeTcpServer() {
    try {
        std::cout << "🌐 初始化增強版 TCP 服務器..." << std::endl;
        
        // 建立增強版 TCP 服務器
        tcpServer_ = std::make_unique<TCPServer>(serverPort_);
        
        // 🔄 修改：連線回調參數改為 SOCKET
        tcpServer_->setConnectionCallback([this](SOCKET clientSocket) {  // 改為 SOCKET
            std::cout << "🎉 新客戶端連線: " << clientSocket << std::endl;
            handleNewConnection(clientSocket);
        });
        
        // 🔄 修改：訊息回調參數改為 SOCKET
        tcpServer_->setMessageCallback([this](SOCKET clientSocket, const std::string& message) {  // 改為 SOCKET
            std::cout << "📨 收到客戶端 " << clientSocket << " 訊息: " << message << std::endl;
            handleClientMessage(clientSocket, message);
        });
        
        // 🔄 修改：斷線回調參數改為 SOCKET
        tcpServer_->setDisconnectionCallback([this](SOCKET clientSocket) {  // 改為 SOCKET
            std::cout << "📴 客戶端斷線: " << clientSocket << std::endl;
            handleClientDisconnection(clientSocket);
        });
        
        // 錯誤回調保持不變
        tcpServer_->setErrorCallback([this](const std::string& error) {
            std::cerr << "🚨 TCP 服務器錯誤: " << error << std::endl;
        });
        
        // 啟動服務器
        bool success = tcpServer_->start();
        if (success) {
            std::cout << "✅ TCP 服務器啟動成功，監聽 port " << serverPort_ << std::endl;
        } else {
            std::cerr << "❌ TCP 服務器啟動失敗" << std::endl;
        }
        
        return success;
        
    } catch (const std::exception& e) {
        std::cerr << "TCP Server initialization error: " << e.what() << std::endl;
        return false;
    }
}


// ===== TCP 連線處理 =====
void TradingSystem::handleNewConnection(SOCKET clientSocket) {
    std::cout << "📞 New client connected: " << clientSocket << std::endl;
    
    try {
        // 更新統計
        totalConnections_.fetch_add(1);
        
        // 建立 FIX Session
        std::string senderCompID = "SERVER";
        std::string targetCompID = "CLIENT_" + std::to_string(static_cast<int64_t>(clientSocket));
        
        auto fixSession = std::make_unique<FixSession>(senderCompID, targetCompID);
        
        // 設定 FIX Session 回調
        fixSession->setApplicationMessageHandler(
            [this, clientSocket](const FixMessage& msg) {
                try {
                    handleFixApplicationMessage(clientSocket, msg);
                } catch (const std::exception& e) {
                    std::cerr << "❌ Error in application message handler: " << e.what() << std::endl;
                }
            }
        );
        
        fixSession->setErrorHandler(
            [this, clientSocket](const std::string& error) {
                std::cerr << "🚨 Session " << clientSocket << " error: " << error << std::endl;
                // 可以考慮在嚴重錯誤時斷開連線
            }
        );
        
        // 設定發送函式
        fixSession->setSendFunction(
            [this, clientSocket](const std::string& message) -> bool {
                if (!tcpServer_ || !tcpServer_->isRunning()) {
                    std::cerr << "❌ TCP Server not available" << std::endl;
                    return false;
                }
                
                try {
                    return tcpServer_->sendMessage(clientSocket, message);
                } catch (const std::exception& e) {
                    std::cerr << "❌ Send error: " << e.what() << std::endl;
                    return false;
                }
            }
        );
        
        // 設定心跳間隔
        fixSession->setHeartbeatInterval(std::chrono::seconds(30));
        
        // 建立並保存 Session
        std::string clientInfo = "Socket_" + std::to_string(static_cast<int64_t>(clientSocket));
        
        {
            std::lock_guard<std::mutex> lock(sessionsMutex_);
            sessions_[clientSocket] = std::make_unique<ClientSession>(
                std::move(fixSession), 
                clientInfo
            );
        }
        
        std::cout << "✅ FIX Session created for client " << clientSocket 
                  << " (" << senderCompID << " -> " << targetCompID << ")" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ Error handling new connection " << clientSocket 
                  << ": " << e.what() << std::endl;
        
        // 清理可能已建立的資源
        cleanupSession(clientSocket);
    }
}


void TradingSystem::handleClientDisconnection(SOCKET clientSocket) {  // 參數類型改為 SOCKET
    std::cout << "📴 Client disconnected: " << clientSocket << std::endl;
    cleanupSession(clientSocket);
}

void TradingSystem::handleClientMessage(SOCKET clientSocket, const std::string& rawMessage) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    
    auto it = sessions_.find(clientSocket);
    if (it == sessions_.end()) {
        std::cerr << "No session found for client: " << clientSocket << std::endl;
        return;
    }
    
    // 交給 FIX Session 處理
    try {
        it->second->fixSession->processIncomingMessage(rawMessage);
    } catch (const std::exception& e) {
        std::cerr << "Error processing message from " << clientSocket << ": " << e.what() << std::endl;
    }
}

// ===== FIX 訊息處理 =====

void TradingSystem::handleFixApplicationMessage(SOCKET clientSocket, const FixMessage& fixMsg) {
    auto msgType = fixMsg.getMsgType();
    if (!msgType) {
        std::cerr << "Invalid message type from client " << clientSocket << std::endl;
        return;
    }
    
    std::cout << "📨 Received FIX message type '" << *msgType << "' from client " << clientSocket << std::endl;
    
    switch (*msgType) {
        case FixMessage::NewOrderSingle:
            handleNewOrderSingle(clientSocket, fixMsg);
            break;
            
        case FixMessage::OrderCancelRequest:
            handleOrderCancelRequest(clientSocket, fixMsg);
            break;
            
        default:
            std::cerr << "Unsupported message type: " << *msgType << std::endl;
            break;
    }
}

void TradingSystem::handleNewOrderSingle(SOCKET clientSocket, const FixMessage& fixMsg) {
    try {
        std::cout << "📋 Processing New Order Single from client " << clientSocket << std::endl;
        
        // 轉換 FIX 訊息為 Order 物件
        auto order = convertFixToOrder(fixMsg, clientSocket);
        
        // 提交到撮合引擎
        if (matchingEngine_->submitOrder(order)) {
            std::cout << "✅ Order " << order->getOrderId() << " submitted to MatchingEngine" << std::endl;
        } else {
            std::cout << "❌ Failed to submit order to MatchingEngine" << std::endl;
            sendOrderReject(clientSocket, fixMsg, "MatchingEngine unavailable");
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing new order: " << e.what() << std::endl;
        sendOrderReject(clientSocket, fixMsg, e.what());
    }
}

void TradingSystem::handleOrderCancelRequest(SOCKET clientSocket, const FixMessage& fixMsg) {
    try {
        std::cout << "❌ Processing Order Cancel Request from client " << clientSocket << std::endl;
        
        std::string origClOrdId = fixMsg.getField(41);  // OrigClOrdID
        
        // 找到對應的 OrderID
        OrderID targetOrderId = 0;
        {
            std::lock_guard<std::mutex> lock(mappingsMutex_);
            for (const auto& pair : orderMappings_) {
                if (pair.second.clientSocket == clientSocket && 
                    pair.second.clOrdId == origClOrdId) {
                    targetOrderId = pair.first;
                    break;
                }
            }
        }
        
        if (targetOrderId == 0) {
            sendOrderReject(clientSocket, fixMsg, "Original order not found");
            return;
        }
        
        // 提交取消請求
        if (matchingEngine_->cancelOrder(targetOrderId, "Client requested")) {
            std::cout << "✅ Cancel request for Order " << targetOrderId << " submitted" << std::endl;
        } else {
            sendOrderReject(clientSocket, fixMsg, "Failed to submit cancel request");
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error processing cancel request: " << e.what() << std::endl;
        sendOrderReject(clientSocket, fixMsg, e.what());
    }
}

// ===== 撮合引擎回調 =====

void TradingSystem::handleExecutionReport(const ExecutionReportPtr& report) {
    std::cout << "📊 Received ExecutionReport: " << report->toString() << std::endl;
    
    try {
        // 找到對應的客戶端
        OrderMapping mapping{0, "", ""};
        {
            std::lock_guard<std::mutex> lock(mappingsMutex_);
            auto it = orderMappings_.find(report->orderId);
            if (it == orderMappings_.end()) {
                std::cerr << "No mapping found for OrderID: " << report->orderId << std::endl;
                return;
            }
            mapping = it->second;
            
            // 如果訂單已完成，清理映射
            if (report->status == OrderStatus::Filled || 
                report->status == OrderStatus::Cancelled ||
                report->status == OrderStatus::Rejected) {
                orderMappings_.erase(it);
            }
        }
        
        // 轉換為 FIX ExecutionReport
        auto fixReport = convertReportToFix(report);
        
        // 設定客戶端特定的欄位
        fixReport.setField(11, mapping.clOrdId);  // ClOrdID
        
        // 發送給對應的客戶端
        if (!sendFixMessage(mapping.clientSocket, fixReport)) {
            std::cerr << "Failed to send ExecutionReport to client " << mapping.clientSocket << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "Error handling execution report: " << e.what() << std::endl;
    }
}

void TradingSystem::handleMatchingEngineError(const std::string& error) {
    std::cerr << "🚨 MatchingEngine Error: " << error << std::endl;
    // 這裡可以加入更多的錯誤處理邏輯，例如：
    // - 記錄到日誌文件
    // - 發送系統警報
    // - 統計錯誤率
}

// ===== 訊息轉換 =====

std::shared_ptr<Order> TradingSystem::convertFixToOrder(const FixMessage& fixMsg, SOCKET clientSocket) {
    // 提取 FIX 欄位
    std::string clOrdId = fixMsg.getField(11);      // ClOrdID
    std::string symbol = fixMsg.getField(55);       // Symbol
    std::string sideStr = fixMsg.getField(54);      // Side
    std::string qtyStr = fixMsg.getField(38);       // OrderQty
    std::string typeStr = fixMsg.getField(40);      // OrdType
    std::string priceStr = fixMsg.getField(44);     // Price (限價單才有)
    
    // 驗證必要欄位
    if (clOrdId.empty() || symbol.empty() || sideStr.empty() || qtyStr.empty() || typeStr.empty()) {
        throw std::invalid_argument("Missing required FIX fields");
    }
    
    // 轉換為業務物件
    OrderID orderId = generateOrderId();
    Side side = parseFixSide(sideStr);
    OrderType orderType = parseFixOrderType(typeStr);
    Quantity quantity = std::stoull(qtyStr);
    Price price = (orderType == OrderType::Market) ? 0.0 : std::stod(priceStr);
    
    // 建立 Order 物件
    auto order = std::make_shared<Order>(
        orderId,
        std::to_string(clientSocket), // 使用 clientSocket 作為 ClientID
        symbol,
        side,
        orderType,
        price,
        quantity
    );
    
    // 保存映射關係
    {
        std::lock_guard<std::mutex> lock(mappingsMutex_);
        orderMappings_.emplace(orderId, OrderMapping(clientSocket, clOrdId, symbol));
    }
    
    std::cout << "🔄 Converted FIX → Order: " << order->toString() << std::endl;
    return order;
}

FixMessage TradingSystem::convertReportToFix(const ExecutionReportPtr& report) {
    // 建立基本的 ExecutionReport
    FixMessage fixMsg('8');  // MsgType = ExecutionReport
    
    // 設定標準欄位
    fixMsg.setField(17, generateExecId());                    // ExecID
    fixMsg.setField(150, std::string(1, getFixExecType(report->status))); // ExecType
    fixMsg.setField(39, std::string(1, getFixOrdStatus(report->status)));  // OrdStatus
    fixMsg.setField(55, report->symbol);                      // Symbol
    fixMsg.setField(54, std::string(1, (report->side == Side::Buy) ? '1' : '2')); // Side
    fixMsg.setField(38, std::to_string(report->originalQuantity)); // OrderQty
    fixMsg.setField(151, std::to_string(report->remainingQuantity)); // LeavesQty
    fixMsg.setField(14, std::to_string(report->filledQuantity));     // CumQty
    
    // 設定價格欄位
    if (report->price > 0.0) {
        std::ostringstream priceStr;
        priceStr << std::fixed << std::setprecision(2) << report->price;
        fixMsg.setField(44, priceStr.str());                  // Price
    }
    
    // 如果有成交，設定成交資訊
    if (report->executionQuantity > 0) {
        fixMsg.setField(32, std::to_string(report->executionQuantity)); // LastQty
        
        if (report->executionPrice > 0.0) {
            std::ostringstream execPriceStr;
            execPriceStr << std::fixed << std::setprecision(2) << report->executionPrice;
            fixMsg.setField(31, execPriceStr.str());          // LastPx
        }
    }
    
    // 如果有拒絕原因，設定 Text 欄位
    if (!report->rejectReason.empty()) {
        fixMsg.setField(58, report->rejectReason);            // Text
    }
    
    // 設定時間欄位
    fixMsg.setField(60, formatCurrentTime());                // TransactTime
    
    return fixMsg;
}

// ===== 發送方法 =====

bool TradingSystem::sendFixMessage(SOCKET clientSocket, const FixMessage& fixMsg) {
    try {
        std::string serialized = fixMsg.serialize();
        std::cout << "📤 Sending FIX message to client " << clientSocket << ": " << serialized << std::endl;
        
        return tcpServer_->sendMessage(static_cast<SOCKET>(clientSocket), serialized);
        
    } catch (const std::exception& e) {
        std::cerr << "Error sending FIX message: " << e.what() << std::endl;
        return false;
    }
}

void TradingSystem::sendOrderReject(SOCKET clientSocket, const FixMessage& originalMsg, const std::string& reason) {
    try {
        std::cout << "❌ Sending Order Reject to client " << clientSocket << ": " << reason << std::endl;
        
        // 建立 ExecutionReport 表示拒絕
        FixMessage rejectMsg('8');  // ExecutionReport
        
        // 複製原始訊息的關鍵欄位
        rejectMsg.setField(11, originalMsg.getField(11));     // ClOrdID
        rejectMsg.setField(55, originalMsg.getField(55));     // Symbol
        rejectMsg.setField(54, originalMsg.getField(54));     // Side
        rejectMsg.setField(38, originalMsg.getField(38));     // OrderQty
        
        // 設定拒絕狀態
        rejectMsg.setField(17, generateExecId());             // ExecID
        rejectMsg.setField(150, "8");                         // ExecType = Rejected
        rejectMsg.setField(39, "8");                          // OrdStatus = Rejected
        rejectMsg.setField(151, "0");                         // LeavesQty = 0
        rejectMsg.setField(14, "0");                          // CumQty = 0
        rejectMsg.setField(58, reason);                       // Text (拒絕原因)
        rejectMsg.setField(60, formatCurrentTime());          // TransactTime
        
        sendFixMessage(clientSocket, rejectMsg);
        
    } catch (const std::exception& e) {
        std::cerr << "Error sending order reject: " << e.what() << std::endl;
    }
}

// ===== 工具方法 =====

std::string TradingSystem::generateExecId() {
    uint64_t execNum = nextExecId_.fetch_add(1);
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    std::ostringstream oss;
    oss << "EXEC_" << timestamp << "_" << execNum;
    return oss.str();
}

char TradingSystem::getFixExecType(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return '0';
        case OrderStatus::PartiallyFilled: return '1';
        case OrderStatus::Filled: return '2';
        case OrderStatus::Cancelled: return '4';
        case OrderStatus::Rejected: return '8';
        default: return '0';
    }
}

char TradingSystem::getFixOrdStatus(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return '0';
        case OrderStatus::PartiallyFilled: return '1';
        case OrderStatus::Filled: return '2';
        case OrderStatus::Cancelled: return '4';
        case OrderStatus::Rejected: return '8';
        default: return '0';
    }
}

// ===== 清理方法 =====

void TradingSystem::cleanupSession(SOCKET clientSocket) {
    std::lock_guard<std::mutex> lock(sessionsMutex_);
    sessions_.erase(clientSocket);
}

void TradingSystem::cleanupResources() {
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        sessions_.clear();
    }
    
    {
        std::lock_guard<std::mutex> lock(mappingsMutex_);
        orderMappings_.clear();
    }
}

// ===== 統計資訊 =====

void TradingSystem::printStatistics() {
    std::cout << "\n📊 Trading System Statistics:" << std::endl;
    std::cout << "================================" << std::endl;
    
    if (matchingEngine_) {
        std::cout << matchingEngine_->getStatistics().toString() << std::endl;
    }
    
    {
        std::lock_guard<std::mutex> lock(sessionsMutex_);
        std::cout << "Active Sessions: " << sessions_.size() << std::endl;
    }
    
    {
        std::lock_guard<std::mutex> lock(mappingsMutex_);
        std::cout << "Pending Orders: " << orderMappings_.size() << std::endl;
    }
    
    std::cout << "================================\n" << std::endl;
}

// ===== 工具函式 =====

Side parseFixSide(const std::string& sideStr) {
    if (sideStr == "1") return Side::Buy;
    if (sideStr == "2") return Side::Sell;
    throw std::invalid_argument("Invalid FIX side: " + sideStr);
}

OrderType parseFixOrderType(const std::string& typeStr) {
    if (typeStr == "1") return OrderType::Market;
    if (typeStr == "2") return OrderType::Limit;
    if (typeStr == "3") return OrderType::Stop;
    if (typeStr == "4") return OrderType::StopLimit;
    throw std::invalid_argument("Invalid FIX order type: " + typeStr);
}

std::string formatCurrentTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    return oss.str();
}