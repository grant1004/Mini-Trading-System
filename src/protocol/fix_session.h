// src/protocol/fix_session.h
#pragma once
#include "fix_message.h"
#include "fix_message_builder.h"
#include "fix_tags.h"
#include <string>
#include <chrono>
#include <atomic>
#include <functional>
#include <queue>
#include <mutex>
#include <iostream>

namespace mts::protocol {

/**
 * @brief FIX Session 的狀態列舉
 * 
 * FIX Session 的生命週期狀態，用於追蹤連線狀態
 */
enum class SessionState {
    Disconnected,   ///< 未連線狀態，初始狀態
    PendingLogon,   ///< 等待登入確認，已發送 Logon 訊息但未收到回應
    LoggedIn,       ///< 已登入，可以發送業務訊息
    PendingLogout,  ///< 等待登出確認，已發送 Logout 訊息但未收到回應
    LoggedOut,      ///< 已登出，Session 正常結束
    Error           ///< 錯誤狀態，連線異常或協議錯誤
};

/**
 * @brief FIX Session 管理類別
 * 
 * 負責管理 FIX 協議的 Session 生命週期，包括：
 * - 登入/登出流程
 * - 序號管理（防止訊息遺失或重複）
 * - Heartbeat 機制（保持連線活躍）
 * - 訊息路由（管理訊息 vs 業務訊息）
 * - 錯誤處理和恢復
 */
class FixSession {
public:
    // ===== 回調函式型別定義 =====
    
    /// 應用訊息處理回調（處理業務邏輯，如新訂單、執行回報）
    using MessageHandler = std::function<void(const FixMessage&)>;
    
    /// 錯誤處理回調（記錄錯誤、告警等）
    using ErrorHandler = std::function<void(const std::string&)>;
    
    /// 訊息發送回調（實際的網路傳輸層，如 TCP Socket）
    using SendFunction = std::function<bool(const std::string&)>;

private:
    // ===== Session 識別資訊 =====
    
    /// 本方的 CompID，在 FIX 訊息中作為 SenderCompID(49)
    std::string senderCompID_;
    
    /// 對方的 CompID，在 FIX 訊息中作為 TargetCompID(56)  
    std::string targetCompID_;
    
    /// Session 的唯一識別碼，用於日誌記錄和偵錯
    std::string sessionID_;
    
    // ===== 序號管理 =====
    
    /// 發送訊息的序號，每發送一個訊息就遞增（執行緒安全）
    std::atomic<uint32_t> outgoingSeqNum_{1};
    
    /// 期望接收的下一個訊息序號，用於檢測訊息遺失（執行緒安全）
    std::atomic<uint32_t> expectedIncomingSeqNum_{1};
    
    /// 當前 Session 狀態
    SessionState state_{SessionState::Disconnected};
    
    // ===== Heartbeat 管理 =====
    
    /// Heartbeat 間隔時間，預設 30 秒（根據 FIX 協議 HeartBtInt 欄位）
    std::chrono::seconds heartbeatInterval_{30};
    
    /// 最後一次發送 Heartbeat 的時間點
    std::chrono::steady_clock::time_point lastHeartbeat_;
    
    /// 最後一次收到任何訊息的時間點（用於檢測對方是否斷線）
    std::chrono::steady_clock::time_point lastReceivedMessage_;
    
    /// Session 開始時間（用於統計連線持續時間）
    std::chrono::steady_clock::time_point sessionStartTime_;
    
    // ===== 訊息佇列（訊息重送機制） =====
    
    /// 已發送但未確認的訊息佇列（用於 ResendRequest 處理）
    std::queue<FixMessage> outgoingMessageQueue_;
    
    /// 保護訊息佇列的互斥鎖
    mutable std::mutex queueMutex_;
    
    // ===== 回調函式 =====
    
    /// 應用訊息處理器（處理訂單、成交回報等業務訊息）
    MessageHandler applicationMessageHandler_;
    
    /// 錯誤處理器（處理協議錯誤、網路錯誤等）
    ErrorHandler errorHandler_;
    
    /// 訊息發送函式（由上層網路模組提供，負責實際傳輸）
    SendFunction sendFunction_;
    
    // ===== 統計資訊 =====
    
    /// 收到的訊息總數（執行緒安全）
    std::atomic<uint64_t> messagesReceived_{0};
    
    /// 發送的訊息總數（執行緒安全）
    std::atomic<uint64_t> messagesSent_{0};

public:
    /**
     * @brief 建構函式
     * @param senderCompID 本方的公司識別碼
     * @param targetCompID 對方的公司識別碼（可選，如果為空則從 LOGON 訊息中提取）
     */
    FixSession(const std::string& senderCompID, const std::string& targetCompID = "");
    ~FixSession() = default;
    
    // ===== Session 生命週期管理 =====
    
    /**
     * @brief 主動發起登入請求
     * @param username 登入用戶名（可選）
     * @param password 登入密碼（可選）
     * @return 是否成功發送登入請求
     * 
     * 建立 Logon(A) 訊息並發送，狀態變為 PendingLogon
     */
    bool initiate(const std::string& username = "", const std::string& password = "");
    
    /**
     * @brief 接受對方的登入請求
     * @param logonMsg 收到的 Logon 訊息
     * @return 是否成功處理登入請求
     * 
     * 驗證 Logon 訊息並回應，狀態變為 LoggedIn
     */
    bool accept(const FixMessage& logonMsg);
    
    /**
     * @brief 發起登出請求
     * @param reason 登出原因（可選）
     * 
     * 發送 Logout(5) 訊息，狀態變為 PendingLogout
     */
    void logout(const std::string& reason = "Normal shutdown");
    
    /**
     * @brief 重設 Session 狀態
     * 
     * 清空序號、佇列等，回到初始狀態
     */
    void reset();
    
    /**
     * @brief 強制斷線
     * 
     * 不發送 Logout 訊息，直接變為 Disconnected 狀態
     */
    void forceDisconnect();

    /**
     * @brief 重置 Session 以支援新的 CompID 登入
     * 用於同一個 TCP 連線中 Logout 後重新登入
     */
    void resetForNewLogin();
    
    /**
     * @brief 檢查是否可以接受新的登入
     */
    bool canAcceptNewLogin() const;
    
    // ===== 訊息處理 =====
    
    /**
     * @brief 處理收到的原始訊息字串
     * @param rawMessage FIX 格式的原始訊息
     * @return 是否成功處理
     * 
     * 解析 FIX 字串並調用 processIncomingMessage(FixMessage)
     */
    bool processIncomingMessage(const std::string& rawMessage);
    
    /**
     * @brief 處理收到的 FIX 訊息物件
     * @param msg 已解析的 FIX 訊息
     * @return 是否成功處理
     * 
     * 驗證序號、路由到對應處理器（管理訊息 vs 業務訊息）
     */
    bool processIncomingMessage(const FixMessage& msg);
    
    /**
     * @brief 發送業務訊息
     * @param msg 要發送的業務訊息（如新訂單、取消訂單）
     * @return 是否成功發送
     * 
     * 只能在 LoggedIn 狀態下發送業務訊息
     */
    bool sendApplicationMessage(const FixMessage& msg);
    
    // ===== Heartbeat 機制 =====
    
    /**
     * @brief 檢查 Heartbeat 狀態
     * @return Session 是否仍然健康
     * 
     * 檢查是否需要發送 Heartbeat，是否已超時未收到訊息
     */
    bool checkHeartbeat();
    
    /**
     * @brief 檢查是否需要發送 Heartbeat
     * @return 是否需要發送
     */
    bool needsHeartbeat() const;
    
    /**
     * @brief 發送 Heartbeat 訊息
     * @param testReqID 測試請求 ID（如果是回應 TestRequest）
     * @return 是否成功發送
     */
    bool sendHeartbeat(const std::string& testReqID = "");
    
    /**
     * @brief 發送測試請求
     * @return 是否成功發送
     * 
     * 當長時間未收到訊息時，主動發送 TestRequest(1) 測試連線
     */
    bool sendTestRequest();
    
    // ===== 狀態查詢 =====
    
    /// 取得當前 Session 狀態
    SessionState getState() const { return state_; }
    
    /// 檢查是否已登入
    bool isLoggedIn() const { return state_ == SessionState::LoggedIn; }
    
    /// 檢查 Session 是否活躍（可接收訊息）
    bool isActive() const { 
        return state_ == SessionState::LoggedIn || state_ == SessionState::PendingLogon; 
    }
    
    // ===== 序號管理 =====
    
    /// 取得下一個發送序號（原子操作，執行緒安全）
    uint32_t getNextOutgoingSeqNum() { return outgoingSeqNum_.fetch_add(1); }
    
    /// 取得當前發送序號
    uint32_t getCurrentOutgoingSeqNum() const { return outgoingSeqNum_.load(); }
    
    /// 取得期望的下一個接收序號
    uint32_t getExpectedIncomingSeqNum() const { return expectedIncomingSeqNum_.load(); }
    
    // ===== Session 資訊 =====
    
    /// 取得本方 CompID
    const std::string& getSenderCompID() const { return senderCompID_; }
    
    /// 取得對方 CompID
    const std::string& getTargetCompID() const { return targetCompID_; }
    
    /// 取得 Session ID
    const std::string& getSessionID() const { return sessionID_; }
    
    // ===== 統計資訊 =====
    
    /// 取得收到的訊息總數
    uint64_t getMessagesReceived() const { return messagesReceived_.load(); }
    
    /// 取得發送的訊息總數
    uint64_t getMessagesSent() const { return messagesSent_.load(); }
    
    /// 取得 Session 持續時間
    std::chrono::seconds getSessionDuration() const;
    
    // ===== 回調函式設定 =====
    
    /// 設定業務訊息處理器
    void setApplicationMessageHandler(MessageHandler handler) { 
        applicationMessageHandler_ = handler; 
    }
    
    /// 設定錯誤處理器
    void setErrorHandler(ErrorHandler handler) { 
        errorHandler_ = handler; 
    }
    
    /// 設定訊息發送函式
    void setSendFunction(SendFunction func) { 
        sendFunction_ = func; 
    }
    
    /// 設定 Heartbeat 間隔
    void setHeartbeatInterval(std::chrono::seconds interval) { 
        heartbeatInterval_ = interval; 
    }
    
    // ===== 工具方法 =====
    
    /// 轉換為可讀的字串格式（用於日誌記錄）
    std::string toString() const;
    
    /// 取得狀態的字串表示
    std::string getStateString() const;

private:
    // ===== 內部訊息處理 =====
    
    /**
     * @brief 處理管理訊息（Logon, Logout, Heartbeat 等）
     * @param msg 管理訊息
     * @return 是否成功處理
     */
    bool handleAdminMessage(const FixMessage& msg);
    
    /// 處理 Logon(A) 訊息
    void handleLogon(const FixMessage& msg);
    
    /// 處理 Logout(5) 訊息
    void handleLogout(const FixMessage& msg);
    
    /// 處理 Heartbeat(0) 訊息
    void handleHeartbeat(const FixMessage& msg);
    
    /// 處理 TestRequest(1) 訊息
    void handleTestRequest(const FixMessage& msg);
    
    /// 處理 ResendRequest(2) 訊息（訊息重送請求）
    void handleResendRequest(const FixMessage& msg);
    
    /// 處理 SequenceReset(4) 訊息（序號重設）
    void handleSequenceReset(const FixMessage& msg);
    
    // ===== 序號驗證 =====
    
    /**
     * @brief 驗證收到訊息的序號
     * @param msg 收到的訊息
     * @return 序號是否正確
     * 
     * 檢查是否有訊息遺失、重複或亂序
     */
    bool validateSequenceNumber(const FixMessage& msg);
    
    /**
     * @brief 處理序號間隔（訊息遺失）
     * @param expectedSeqNum 期望的序號
     * @param receivedSeqNum 實際收到的序號
     */
    void handleSequenceGap(uint32_t expectedSeqNum, uint32_t receivedSeqNum);
    
    /**
     * @brief 發送訊息重送請求
     * @param beginSeqNum 開始序號
     * @param endSeqNum 結束序號
     */
    void sendResendRequest(uint32_t beginSeqNum, uint32_t endSeqNum);
    
    // ===== 訊息發送 =====
    
    /**
     * @brief 內部訊息發送方法
     * @param msg 要發送的訊息
     * @return 是否成功發送
     * 
     * 設定 Session 資訊（SenderCompID, TargetCompID, MsgSeqNum）並發送
     */
    bool sendMessage(const FixMessage& msg);
    
    /**
     * @brief 發送管理訊息
     * @param msg 管理訊息
     * @return 是否成功發送
     */
    bool sendAdminMessage(const FixMessage& msg);
    
    /**
     * @brief 建立基礎訊息
     * @param msgType 訊息類型
     * @return 已設定基本欄位的訊息
     */
    FixMessage createBaseMessage(char msgType) const;
    
    // ===== 狀態管理 =====
    
    /// 設定新的 Session 狀態
    void setState(SessionState newState);
    
    /// 更新 Heartbeat 相關時間戳
    void updateHeartbeatTimers();
    
    /// 通知錯誤（調用錯誤處理回調）
    void notifyError(const std::string& error);
    
    // ===== 輔助方法 =====
    
    /// 產生唯一的 Session ID
    std::string generateSessionID() const;
    
    /// 檢查 Heartbeat 是否已超時
    bool isHeartbeatExpired() const;
    
    /// 檢查是否應該發送 Heartbeat
    bool shouldSendHeartbeat() const;
    
    /// 發送登入回應訊息
    void sendLogonResponse(); 
}; // FixSession 

// ===== 工具函式 =====

/**
 * @brief 將 Session 狀態轉換為字串
 * @param state Session 狀態
 * @return 狀態的字串表示
 */
std::string sessionStateToString(SessionState state);

} // namespace mts::protocol