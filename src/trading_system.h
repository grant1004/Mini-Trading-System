#pragma once
#include "core/matching_engine.h"
#include "protocol/fix_message.h"
#include "protocol/fix_message_builder.h"
#include "protocol/fix_session.h"
#include "network/tcp_server.h"
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <chrono>

using namespace mts::core;
using namespace mts::protocol;
using namespace mts::tcp_server;

// ===== 簡化的 ClientSession =====
struct ClientSession {
    std::unique_ptr<FixSession> fixSession;
    std::atomic<bool> active{true};
    std::chrono::steady_clock::time_point connectTime;
    std::string clientInfo;  // 可選：客戶端資訊
    
    explicit ClientSession(std::unique_ptr<FixSession> session, const std::string& info = "")
        : fixSession(std::move(session))
        , connectTime(std::chrono::steady_clock::now())
        , clientInfo(info) {}
    
    ~ClientSession() {
        active = false;
        std::cout << "🧹 ClientSession destroyed for " << clientInfo << std::endl;
    }
    
    // 檢查 Session 是否健康
    bool isHealthy() const {
        return active.load() && fixSession && fixSession->isActive();
    }
    
    // 取得連線持續時間
    std::chrono::seconds getConnectionDuration() const {
        return std::chrono::duration_cast<std::chrono::seconds>(
            std::chrono::steady_clock::now() - connectTime
        );
    }
};

// ===== 訂單映射結構 =====
struct OrderMapping {
    SOCKET clientSocket;
    std::string clOrdId;
    std::string symbol;
    std::chrono::steady_clock::time_point createTime;
    
    OrderMapping(SOCKET socket, const std::string& clOrd, const std::string& sym)
        : clientSocket(socket), clOrdId(clOrd), symbol(sym)
        , createTime(std::chrono::steady_clock::now()) {}
};

class TradingSystem {
private:
    // 核心組件
    std::unique_ptr<MatchingEngine> matchingEngine_;
    std::unique_ptr<TCPServer> tcpServer_;
    
    // Session 管理
    std::map<SOCKET, std::unique_ptr<ClientSession>> sessions_;
    std::mutex sessionsMutex_;
    
    // 訂單映射
    std::map<OrderID, OrderMapping> orderMappings_;
    std::mutex mappingsMutex_;
    
    // ID 生成器
    std::atomic<OrderID> nextOrderId_{1};
    std::atomic<uint64_t> nextExecId_{1};
    
    // 系統狀態
    std::atomic<bool> running_{false};
    int serverPort_;
    
    // 統計資訊
    std::atomic<uint64_t> totalConnections_{0};
    std::atomic<uint64_t> totalOrders_{0};
    std::atomic<uint64_t> totalTrades_{0};

public:
    explicit TradingSystem(int port = 8080);
    ~TradingSystem();
    
    // 禁用複製和移動
    TradingSystem(const TradingSystem&) = delete;
    TradingSystem& operator=(const TradingSystem&) = delete;
    
    // ===== 系統生命週期 =====
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // ===== 統計和監控 =====
    void printStatistics();
    void printSessionDetails();
    size_t getActiveSessionCount();
    std::vector<SOCKET> getActiveSockets();

private:
    // ===== 初始化 =====
    bool initializeMatchingEngine();
    bool initializeTcpServer();
    
    // ===== 連線處理 =====
    void handleNewConnection(SOCKET clientSocket);
    void handleClientDisconnection(SOCKET clientSocket);
    void handleClientMessage(SOCKET clientSocket, const std::string& rawMessage);
    
    // ===== FIX 訊息處理 =====
    void handleFixApplicationMessage(SOCKET clientSocket, const FixMessage& fixMsg);
    void handleNewOrderSingle(SOCKET clientSocket, const FixMessage& fixMsg);
    void handleOrderCancelRequest(SOCKET clientSocket, const FixMessage& fixMsg);
    
    // ===== 撮合引擎回調 =====
    void handleExecutionReport(const ExecutionReportPtr& report);
    void handleMatchingEngineError(const std::string& error);
    
    // ===== 轉換和工具 =====
    std::shared_ptr<Order> convertFixToOrder(const FixMessage& fixMsg, SOCKET clientSocket);
    FixMessage convertReportToFix(const ExecutionReportPtr& report);
    bool sendFixMessage(SOCKET clientSocket, const FixMessage& fixMsg);
    void sendOrderReject(SOCKET clientSocket, const FixMessage& originalMsg, const std::string& reason);
    
    // ===== 輔助方法 =====
    OrderID generateOrderId() { return nextOrderId_.fetch_add(1); }
    std::string generateExecId();
    char getFixExecType(OrderStatus status);
    char getFixOrdStatus(OrderStatus status);
    
    // ===== 清理 =====
    void cleanupSession(SOCKET clientSocket);
    void cleanupResources();
    
    // ===== Session 健康檢查 =====
    void performSessionHealthCheck();
    void startPeriodicTasks();
    void stopPeriodicTasks();
    
private:
    // 週期性任務
    std::unique_ptr<std::thread> healthCheckThread_;
    std::atomic<bool> healthCheckRunning_{false};
};

// ===== 工具函式 =====
Side parseFixSide(const std::string& sideStr);
OrderType parseFixOrderType(const std::string& typeStr);
std::string formatCurrentTime();