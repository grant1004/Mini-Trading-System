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

enum class SessionState {
    Disconnected,
    PendingLogon,
    LoggedIn,
    PendingLogout,
    LoggedOut,
    Error
};

// FIX Session 管理客戶端連線狀態
class FixSession {
public:
    using MessageHandler = std::function<void(const FixMessage&)>;
    using ErrorHandler = std::function<void(const std::string&)>;
    using SendFunction = std::function<bool(const std::string&)>;

private:
    // Session 識別
    std::string senderCompID_;
    std::string targetCompID_;
    std::string sessionID_;
    
    // 序號管理
    std::atomic<uint32_t> outgoingSeqNum_{1};
    std::atomic<uint32_t> expectedIncomingSeqNum_{1};
    SessionState state_{SessionState::Disconnected};
    
    // Heartbeat 管理
    std::chrono::seconds heartbeatInterval_{30};
    std::chrono::steady_clock::time_point lastHeartbeat_;
    std::chrono::steady_clock::time_point lastReceivedMessage_;
    std::chrono::steady_clock::time_point sessionStartTime_;
    
    // 訊息佇列（用於重送）
    std::queue<FixMessage> outgoingMessageQueue_;
    mutable std::mutex queueMutex_;
    
    // 回調函式
    MessageHandler applicationMessageHandler_;
    ErrorHandler errorHandler_;
    SendFunction sendFunction_;
    
    // 統計資訊
    std::atomic<uint64_t> messagesReceived_{0};
    std::atomic<uint64_t> messagesSent_{0};

public:
    FixSession(const std::string& senderCompID, const std::string& targetCompID);
    ~FixSession() = default;
    
    // ===== Session 生命週期 =====
    bool initiate(const std::string& username = "", const std::string& password = "");
    bool accept(const FixMessage& logonMsg);
    void logout(const std::string& reason = "Normal shutdown");
    void reset();
    void forceDisconnect();
    
    // ===== 訊息處理 =====
    bool processIncomingMessage(const std::string& rawMessage);
    bool processIncomingMessage(const FixMessage& msg);
    bool sendApplicationMessage(const FixMessage& msg);
    
    // ===== Heartbeat 機制 =====
    bool checkHeartbeat();
    bool needsHeartbeat() const;
    bool sendHeartbeat(const std::string& testReqID = "");
    bool sendTestRequest();
    
    // ===== 狀態查詢 =====
    SessionState getState() const { return state_; }
    bool isLoggedIn() const { return state_ == SessionState::LoggedIn; }
    bool isActive() const { 
        return state_ == SessionState::LoggedIn || state_ == SessionState::PendingLogon; 
    }
    
    uint32_t getNextOutgoingSeqNum() { return outgoingSeqNum_.fetch_add(1); }
    uint32_t getCurrentOutgoingSeqNum() const { return outgoingSeqNum_.load(); }
    uint32_t getExpectedIncomingSeqNum() const { return expectedIncomingSeqNum_.load(); }
    
    const std::string& getSenderCompID() const { return senderCompID_; }
    const std::string& getTargetCompID() const { return targetCompID_; }
    const std::string& getSessionID() const { return sessionID_; }
    
    // ===== 統計資訊 =====
    uint64_t getMessagesReceived() const { return messagesReceived_.load(); }
    uint64_t getMessagesSent() const { return messagesSent_.load(); }
    std::chrono::seconds getSessionDuration() const;
    
    // ===== 設定回調 =====
    void setApplicationMessageHandler(MessageHandler handler) { 
        applicationMessageHandler_ = handler; 
    }
    void setErrorHandler(ErrorHandler handler) { 
        errorHandler_ = handler; 
    }
    void setSendFunction(SendFunction func) { 
        sendFunction_ = func; 
    }
    void setHeartbeatInterval(std::chrono::seconds interval) { 
        heartbeatInterval_ = interval; 
    }
    
    // ===== 工具方法 =====
    std::string toString() const;
    std::string getStateString() const;

private:
    // ===== 內部訊息處理 =====
    bool handleAdminMessage(const FixMessage& msg);
    void handleLogon(const FixMessage& msg);
    void handleLogout(const FixMessage& msg);
    void handleHeartbeat(const FixMessage& msg);
    void handleTestRequest(const FixMessage& msg);
    void handleResendRequest(const FixMessage& msg);
    void handleSequenceReset(const FixMessage& msg);
    
    // ===== 序號驗證 =====
    bool validateSequenceNumber(const FixMessage& msg);
    void handleSequenceGap(uint32_t expectedSeqNum, uint32_t receivedSeqNum);
    void sendResendRequest(uint32_t beginSeqNum, uint32_t endSeqNum);
    
    // ===== 訊息發送 =====
    bool sendMessage(const FixMessage& msg);
    bool sendAdminMessage(const FixMessage& msg);
    FixMessage createBaseMessage(char msgType) const;
    
    // ===== 狀態管理 =====
    void setState(SessionState newState);
    void updateHeartbeatTimers();
    void notifyError(const std::string& error);
    
    // ===== 輔助方法 =====
    std::string generateSessionID() const;
    bool isHeartbeatExpired() const;
    bool shouldSendHeartbeat() const;
    void sendLogonResponse() ; 
}; // FixSession 

// ===== 工具函式 =====
std::string sessionStateToString(SessionState state);

} // namespace protocol