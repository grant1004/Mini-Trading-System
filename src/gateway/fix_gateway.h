// src/gateway/fix_gateway.h
#pragma once

#include "../protocol/fix_message.h"
#include "../protocol/fix_session.h"
#include "../core/order_book.h"
#include "../core/order.h"
#include "../core/matching_engine.h"
#include <string>
#include <memory>
#include <map>
#include <vector>
#include <functional>
#include <mutex>
#include <atomic>
#include <queue>
#include <shared_mutex>

namespace mts::gateway {

// 前置宣告
class SessionManager;
class ApplicationLayer;

/**
 * @brief 客戶端連線資訊
 * 
 * 儲存每個 TCP 連線的相關資訊
 */
struct ClientConnection {
    int socketId;                    ///< TCP Socket ID
    std::string ipAddress;           ///< 客戶端 IP 位址
    uint16_t port;                   ///< 客戶端 Port
    std::chrono::steady_clock::time_point connectTime;  ///< 連線時間
    std::string buffer;              ///< 接收緩衝區（處理 TCP 分包）
    std::atomic<bool> isActive{true}; ///< 連線是否活躍
    
    ClientConnection(int id, const std::string& ip, uint16_t p)
        : socketId(id), ipAddress(ip), port(p)
        , connectTime(std::chrono::steady_clock::now()) {}
};

using ClientConnectionPtr = std::shared_ptr<ClientConnection>;

/**
 * @brief FIX Gateway 主類別
 * 
 * 職責：
 * 1. 作為 TCP Server 和 FIX Protocol 之間的橋樑
 * 2. 處理 FIX 訊息的解析和路由
 * 3. 管理客戶端連線生命週期
 * 4. 協調 Session Manager 和 Application Layer
 */
class FixGateway {
public:
    // ===== 回調函式類型 =====
    
    /// TCP 傳送回調（由 TCP Server 提供）
    using SendFunction = std::function<bool(int clientId, const std::string& data)>;
    
    /// 客戶端斷線通知回調
    using DisconnectCallback = std::function<void(int clientId)>;
    
    /// 錯誤處理回調
    using ErrorCallback = std::function<void(int clientId, const std::string& error)>;
    
    /// 統計資訊回調
    using StatsCallback = std::function<void(const std::string& stats)>;

public:
    /**
     * @brief 建構函式
     * @param maxConnections 最大連線數限制
     */
    explicit FixGateway(size_t maxConnections = 1000);
    
    /**
     * @brief 解構函式
     */
    ~FixGateway();
    
    // ===== 生命週期管理 =====
    
    /**
     * @brief 啟動 Gateway
     * @return 是否成功啟動
     */
    bool start();
    
    /**
     * @brief 停止 Gateway
     */
    void stop();
    
    /**
     * @brief 檢查是否正在運行
     */
    bool isRunning() const { return running_.load(); }
    
    // ===== TCP Server 介面 =====
    
    /**
     * @brief 處理新的客戶端連線
     * @param clientId TCP Socket ID
     * @param ipAddress 客戶端 IP
     * @param port 客戶端 Port
     * @return 是否接受連線
     */
    bool onClientConnected(int clientId, const std::string& ipAddress, uint16_t port);
    
    /**
     * @brief 處理客戶端斷線
     * @param clientId TCP Socket ID
     */
    void onClientDisconnected(int clientId);
    
    /**
     * @brief 處理收到的原始資料
     * @param clientId TCP Socket ID  
     * @param rawData 原始 TCP 資料
     */
    void onDataReceived(int clientId, const std::string& rawData);
    
    // ===== 組件整合介面 =====
    
    /**
     * @brief 設定 Session Manager
     */
    void setSessionManager(std::shared_ptr<SessionManager> sessionMgr);
    
    /**
     * @brief 設定 Application Layer
     */
    void setApplicationLayer(std::shared_ptr<ApplicationLayer> appLayer);
    
    /**
     * @brief 設定 TCP 傳送函式
     */
    void setSendFunction(SendFunction sendFunc);
    
    // ===== 回調設定 =====
    
    void setDisconnectCallback(DisconnectCallback callback) { disconnectCallback_ = callback; }
    void setErrorCallback(ErrorCallback callback) { errorCallback_ = callback; }
    void setStatsCallback(StatsCallback callback) { statsCallback_ = callback; }
    
    // ===== 訊息發送介面 =====
    
    /**
     * @brief 發送 FIX 訊息給指定客戶端
     * @param clientId 目標客戶端
     * @param fixMessage FIX 訊息
     * @return 是否成功發送
     */
    bool sendFixMessage(int clientId, const mts::protocol::FixMessage& fixMessage);
    
    /**
     * @brief 廣播 FIX 訊息給所有連線客戶端
     * @param fixMessage FIX 訊息
     * @param excludeClientId 排除的客戶端 ID（可選）
     */
    void broadcastFixMessage(const mts::protocol::FixMessage& fixMessage, 
                            int excludeClientId = -1);
    
    // ===== 查詢介面 =====
    
    /**
     * @brief 取得連線客戶端數量
     */
    size_t getConnectionCount() const;
    
    /**
     * @brief 取得客戶端連線資訊
     */
    ClientConnectionPtr getClientConnection(int clientId) const;
    
    /**
     * @brief 取得所有客戶端連線
     */
    std::vector<ClientConnectionPtr> getAllConnections() const;
    
    /**
     * @brief 取得統計資訊
     */
    std::string getStatistics() const;
    
    // ===== 管理介面 =====
    
    /**
     * @brief 強制斷開指定客戶端
     */
    bool disconnectClient(int clientId, const std::string& reason = "Admin disconnect");
    
    /**
     * @brief 設定最大連線數
     */
    void setMaxConnections(size_t maxConnections) { maxConnections_ = maxConnections; }

private:
    // ===== 內部處理方法 =====
    
    /**
     * @brief 解析緩衝區中的完整 FIX 訊息
     * @param buffer 客戶端接收緩衝區
     * @return 解析出的完整 FIX 訊息列表
     */
    std::vector<mts::protocol::FixMessage> parseFixMessages(std::string& buffer);
    
    /**
     * @brief 提取單個完整的 FIX 訊息
     * @param buffer 緩衝區
     * @return 完整的 FIX 訊息（如果有），否則返回空
     */
    std::optional<mts::protocol::FixMessage> extractSingleFixMessage(std::string& buffer);
    
    /**
     * @brief 路由 FIX 訊息到對應的 Session
     * @param clientId 來源客戶端
     * @param fixMessage FIX 訊息
     */
    void routeFixMessage(int clientId, const mts::protocol::FixMessage& fixMessage);
    
    /**
     * @brief 處理 FIX 協議錯誤
     * @param clientId 客戶端 ID
     * @param error 錯誤描述
     */
    void handleProtocolError(int clientId, const std::string& error);
    
    /**
     * @brief 清理客戶端資源
     * @param clientId 客戶端 ID
     */
    void cleanupClient(int clientId);
    
    /**
     * @brief 驗證連線限制
     * @return 是否可以接受新連線
     */
    bool canAcceptConnection() const;
    
    /**
     * @brief 更新統計資訊
     */
    void updateStatistics();

private:
    // ===== 核心狀態 =====
    std::atomic<bool> running_{false};               ///< 運行狀態
    size_t maxConnections_;                          ///< 最大連線數限制
    
    // ===== 連線管理 =====
    std::map<int, ClientConnectionPtr> connections_; ///< 客戶端連線映射
    mutable std::shared_mutex connectionsMutex_;     ///< 連線映射的讀寫鎖
    
    // ===== 組件引用 =====
    std::shared_ptr<SessionManager> sessionManager_; ///< Session 管理器
    std::shared_ptr<ApplicationLayer> appLayer_;     ///< 應用層
    
    // ===== 回調函式 =====
    SendFunction sendFunction_;                      ///< TCP 傳送函式
    DisconnectCallback disconnectCallback_;         ///< 斷線回調
    ErrorCallback errorCallback_;                   ///< 錯誤回調
    StatsCallback statsCallback_;                   ///< 統計回調
    
    // ===== 統計資訊 =====
    struct Statistics {
        std::atomic<uint64_t> totalConnections{0};    ///< 總連線數
        std::atomic<uint64_t> currentConnections{0};  ///< 當前連線數
        std::atomic<uint64_t> messagesReceived{0};    ///< 收到的訊息數
        std::atomic<uint64_t> messagesSent{0};        ///< 發送的訊息數
        std::atomic<uint64_t> protocolErrors{0};      ///< 協議錯誤數
        std::chrono::steady_clock::time_point startTime; ///< 啟動時間
    } stats_;
};

} // namespace mts::gateway

// =============================================================================
// src/gateway/session_manager.h  
// =============================================================================

namespace mts::gateway {

/**
 * @brief Session 管理器
 * 
 * 職責：
 * 1. 管理所有 FIX Session 的生命週期
 * 2. 根據 CompID 路由訊息到對應的 Session
 * 3. 處理 Session 的建立、維護和銷毀
 * 4. 定期檢查 Session 健康度（Heartbeat）
 */
class SessionManager {
public:
    /**
     * @brief Session 事件回調類型
     */
    enum class SessionEvent {
        CREATED,      ///< Session 已建立
        LOGGED_IN,    ///< Session 已登入
        LOGGED_OUT,   ///< Session 已登出
        TIMEOUT,      ///< Session 超時
        ERROR         ///< Session 錯誤
    };
    
    using SessionEventCallback = std::function<void(const std::string& sessionId, 
                                                   SessionEvent event, 
                                                   const std::string& details)>;

public:
    explicit SessionManager();
    ~SessionManager();
    
    // ===== 生命週期 =====
    bool start();
    void stop();
    
    // ===== Session 管理 =====
    
    /**
     * @brief 根據客戶端和訊息找到或建立 Session
     * @param clientId TCP 客戶端 ID
     * @param fixMessage 收到的 FIX 訊息
     * @return Session 指標
     */
    std::shared_ptr<mts::protocol::FixSession> getOrCreateSession(
        int clientId, 
        const mts::protocol::FixMessage& fixMessage);
    
    /**
     * @brief 根據 Session ID 查找 Session
     */
    std::shared_ptr<mts::protocol::FixSession> findSession(const std::string& sessionId);
    
    /**
     * @brief 移除 Session
     */
    bool removeSession(const std::string& sessionId);
    
    /**
     * @brief 移除指定客戶端的所有 Session
     */
    void removeClientSessions(int clientId);
    
    // ===== 查詢介面 =====
    size_t getSessionCount() const;
    std::vector<std::string> getAllSessionIds() const;
    
    // ===== 回調設定 =====
    void setSessionEventCallback(SessionEventCallback callback) { 
        sessionEventCallback_ = callback; 
    }

private:
    // ===== 內部方法 =====
    void heartbeatCheckLoop();
    std::string generateSessionId(const std::string& senderCompID, 
                                 const std::string& targetCompID) const;

private:
    std::atomic<bool> running_{false};
    std::map<std::string, std::shared_ptr<mts::protocol::FixSession>> sessions_;
    std::map<int, std::vector<std::string>> clientSessions_; // clientId -> sessionIds
    mutable std::shared_mutex sessionsMutex_;
    
    std::thread heartbeatThread_;
    SessionEventCallback sessionEventCallback_;
};

} // namespace mts::gateway

// =============================================================================
// src/gateway/application_layer.h
// =============================================================================

namespace mts::gateway {

/**
 * @brief 應用層 - 連接 FIX 協議和業務邏輯
 * 
 * 職責：
 * 1. 將 FIX 訊息轉換為業務物件（Order）
 * 2. 調用撮合引擎處理訂單
 * 3. 將撮合結果轉換為 FIX 回報
 * 4. 管理市場數據分發
 */
class ApplicationLayer {
public:
    explicit ApplicationLayer();
    ~ApplicationLayer();
    
    // ===== 組件設定 =====
    void setMatchingEngine(std::shared_ptr<mts::core::MatchingEngine> engine);
    void setGateway(std::shared_ptr<FixGateway> gateway);
    
    // ===== 訊息處理入口 =====
    
    /**
     * @brief 處理來自 Session 的應用訊息
     * @param clientId 客戶端 ID
     * @param fixMessage FIX 訊息
     */
    void processApplicationMessage(int clientId, const mts::protocol::FixMessage& fixMessage);
    
    // ===== 具體業務處理 =====
    void handleNewOrderSingle(int clientId, const mts::protocol::FixMessage& msg);
    void handleOrderCancelRequest(int clientId, const mts::protocol::FixMessage& msg);
    void handleOrderCancelReplaceRequest(int clientId, const mts::protocol::FixMessage& msg);
    
private:
    // ===== 轉換方法 =====
    std::shared_ptr<mts::core::Order> fixMessageToOrder(const mts::protocol::FixMessage& msg);
    mts::protocol::FixMessage orderToExecutionReport(const mts::core::Order& order, 
                                                     const std::string& execType);
    
    // ===== 回調處理 =====
    void onTradeExecuted(const mts::core::TradePtr& trade);
    void onOrderUpdated(const std::shared_ptr<mts::core::Order>& order);

private:
    std::shared_ptr<mts::core::MatchingEngine> matchingEngine_;
    std::weak_ptr<FixGateway> gateway_;
    
    // 客戶端訂單映射（用於回報路由）
    std::map<mts::core::OrderID, int> orderToClient_;
    std::mutex orderMappingMutex_;
};

} // namespace mts::gateway