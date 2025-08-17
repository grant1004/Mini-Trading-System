#pragma once
#include "core/matching_engine.h"
#include "protocol/fix_message.h"
#include "protocol/fix_message_builder.h"
#include "protocol/fix_session.h"
#include "network/tcp_server.cpp" 
#include <map>
#include <memory>
#include <mutex>
#include <atomic>
#include <thread>

using namespace mts::core;
using namespace mts::protocol;

// 訂單映射結構 (Order ID → FIX 資訊)
struct OrderMapping {
    int clientSocket;
    std::string clOrdId;
    std::string symbol;
    
    OrderMapping(int socket, const std::string& clOrd, const std::string& sym)
        : clientSocket(socket), clOrdId(clOrd), symbol(sym) {}
};

// 客戶端 Session 資訊
struct ClientSession {
    std::unique_ptr<FixSession> fixSession;
    std::thread* handlerThread;
    std::atomic<bool> active{true};
    
    ClientSession(std::unique_ptr<FixSession> session, std::thread* thread)
        : fixSession(std::move(session)), handlerThread(thread) {}
    
    ~ClientSession() {
        active = false;
        if (handlerThread && handlerThread->joinable()) {
            handlerThread->join();
            delete handlerThread;
        }
    }
};

class TradingSystem {
private:
    // 核心組件
    std::unique_ptr<MatchingEngine> matchingEngine_;
    std::unique_ptr<TCPServer> tcpServer_;
    
    // Session 管理
    std::map<int, std::unique_ptr<ClientSession>> sessions_;
    std::mutex sessionsMutex_;
    
    // 訂單映射 (用於回報路由)
    std::map<OrderID, OrderMapping> orderMappings_;
    std::mutex mappingsMutex_;
    
    // ID 生成器
    std::atomic<OrderID> nextOrderId_{1};
    std::atomic<uint64_t> nextExecId_{1};
    
    // 系統狀態
    std::atomic<bool> running_{false};
    int serverPort_;

public:
    explicit TradingSystem(int port = 8080) : serverPort_(port) {}
    
    ~TradingSystem() {
        stop();
    }
    
    // ===== 系統生命週期 =====
    bool start();
    void stop();
    bool isRunning() const { return running_.load(); }
    
    // ===== 統計資訊 =====
    void printStatistics();
    
private:
    // ===== 初始化方法 =====
    bool initializeMatchingEngine();
    bool initializeTcpServer();
    
    // ===== TCP 連線處理 =====
    void handleNewConnection(int clientSocket);
    void handleClientDisconnection(int clientSocket);
    void handleClientMessage(int clientSocket, const std::string& rawMessage);
    
    // ===== FIX 訊息處理 =====
    void handleFixApplicationMessage(int clientSocket, const FixMessage& fixMsg);
    void handleNewOrderSingle(int clientSocket, const FixMessage& fixMsg);
    void handleOrderCancelRequest(int clientSocket, const FixMessage& fixMsg);
    
    // ===== 撮合引擎回調 =====
    void handleExecutionReport(const ExecutionReportPtr& report);
    void handleMatchingEngineError(const std::string& error);
    
    // ===== 訊息轉換 =====
    std::shared_ptr<Order> convertFixToOrder(const FixMessage& fixMsg, int clientSocket);
    FixMessage convertReportToFix(const ExecutionReportPtr& report);
    
    // ===== 發送方法 =====
    bool sendFixMessage(int clientSocket, const FixMessage& fixMsg);
    void sendOrderReject(int clientSocket, const FixMessage& originalMsg, const std::string& reason);
    
    // ===== 工具方法 =====
    OrderID generateOrderId() { return nextOrderId_.fetch_add(1); }
    std::string generateExecId();
    char getFixExecType(OrderStatus status);
    char getFixOrdStatus(OrderStatus status);
    std::shared_ptr<Order> findOrderById(OrderID orderId);
    
    // ===== 清理方法 =====
    void cleanupSession(int clientSocket);
    void cleanupResources();
};

// ===== 工具函式 =====
Side parseFixSide(const std::string& sideStr);
OrderType parseFixOrderType(const std::string& typeStr);
std::string formatCurrentTime();