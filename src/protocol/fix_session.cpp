// src/protocol/fix_session.cpp
#include "fix_session.h"
#include <iostream>
#include <sstream>
#include <iomanip>

// Debug 巨集
#ifdef ENABLE_FIX_DEBUG
    #define SESSION_DEBUG(msg) std::cout << "[SESSION_DEBUG] " << sessionID_ << ": " << msg << std::endl
#else
    #define SESSION_DEBUG(msg) do {} while(0)
#endif


// ===== 建構函式 =====
namespace mts::protocol {

FixSession::FixSession(const std::string& senderCompID, const std::string& targetCompID)
    : senderCompID_(senderCompID)
    , targetCompID_(targetCompID)
    , sessionID_(generateSessionID())
    , sessionStartTime_(std::chrono::steady_clock::now())
{
    SESSION_DEBUG("Created session: " << senderCompID_ << " -> " << targetCompID_);
    updateHeartbeatTimers();
}

// ===== Session 生命週期 =====
bool FixSession::initiate(const std::string& username, const std::string& password) {
    SESSION_DEBUG("Initiating logon");
    
    if (state_ != SessionState::Disconnected) {
        notifyError("Cannot initiate logon from state: " + getStateString());
        return false;
    }
    
    setState(SessionState::PendingLogon);
    
    // 建立 Logon 訊息
    auto logonMsg = FixMessageBuilder::createLogon(username, password);
    logonMsg.setField(FixMessage::SenderCompID, senderCompID_);
    logonMsg.setField(FixMessage::TargetCompID, targetCompID_);
    logonMsg.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    logonMsg.setField(108, std::to_string(heartbeatInterval_.count())); // HeartBtInt
    
    return sendAdminMessage(logonMsg);
}

bool FixSession::accept(const FixMessage& logonMsg) {
    SESSION_DEBUG("Accepting logon");
    
    if (state_ != SessionState::Disconnected) {
        notifyError("Cannot accept logon from state: " + getStateString());
        return false;
    }
    
    // 驗證 Logon 訊息
    if (!logonMsg.hasField(FixMessage::SenderCompID) || 
        !logonMsg.hasField(FixMessage::TargetCompID)) {
        notifyError("Invalid logon message: missing SenderCompID or TargetCompID");
        return false;
    }
    
    // 提取並設定 CompID
    std::string msgSender = logonMsg.getField(FixMessage::SenderCompID);
    std::string msgTarget = logonMsg.getField(FixMessage::TargetCompID);
    
    // 如果 targetCompID_ 為空，則從 LOGON 訊息中提取對方的 CompID
    if (targetCompID_.empty()) {
        targetCompID_ = msgSender;
        sessionID_ = generateSessionID(); // 重新生成 SessionID
        SESSION_DEBUG("Dynamic CompID assignment: target=" + targetCompID_);
    }
    
    // 驗證 CompID 對應關係
    if (msgSender != targetCompID_ || msgTarget != senderCompID_) {
        notifyError("CompID mismatch: expected " + targetCompID_ + "->" + senderCompID_ + 
                   ", got " + msgSender + "->" + msgTarget);
        return false;
    }
    
    // 處理 HeartBeat 間隔
    if (logonMsg.hasField(108)) { // HeartBtInt
        try {
            int interval = std::stoi(logonMsg.getField(108));
            if (interval > 0) {
                heartbeatInterval_ = std::chrono::seconds(interval);
                SESSION_DEBUG("HeartBeat interval set to: " << interval << " seconds");
            }
        } catch (...) {
            SESSION_DEBUG("Invalid HeartBtInt field, using default");
        }
    }
    
    setState(SessionState::LoggedIn);
    updateHeartbeatTimers();
    
    // 發送 Logon 回應
    sendLogonResponse();
    
    return true;
}

void FixSession::logout(const std::string& reason) {
    SESSION_DEBUG("Initiating logout: " << reason);
    
    if (state_ != SessionState::LoggedIn) {
        SESSION_DEBUG("Cannot logout from state: " << getStateString());
        return;
    }
    
    setState(SessionState::PendingLogout);
    
    // 發送 Logout 訊息
    auto logoutMsg = FixMessageBuilder::createLogout(reason);
    logoutMsg.setField(FixMessage::SenderCompID, senderCompID_);
    logoutMsg.setField(FixMessage::TargetCompID, targetCompID_);
    logoutMsg.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    
    sendAdminMessage(logoutMsg);
    
    // 設定超時後自動變為 LoggedOut
    setState(SessionState::LoggedOut);
}

void FixSession::reset() {
    SESSION_DEBUG("Resetting session");
    
    setState(SessionState::Disconnected);
    outgoingSeqNum_.store(1);
    expectedIncomingSeqNum_.store(1);
    
    // 清空訊息佇列
    std::lock_guard<std::mutex> lock(queueMutex_);
    std::queue<FixMessage> empty;
    outgoingMessageQueue_.swap(empty);
    
    updateHeartbeatTimers();
    
    messagesReceived_.store(0);
    messagesSent_.store(0);
}

void FixSession::forceDisconnect() {
    SESSION_DEBUG("Force disconnect");
    setState(SessionState::Disconnected);
}

// ===== 訊息處理 =====
bool FixSession::processIncomingMessage(const std::string& rawMessage) {
    try {
        FixMessage msg = FixMessage::parse(rawMessage);
        return processIncomingMessage(msg);
    } catch (const std::exception& e) {
        notifyError("Failed to parse incoming message: " + std::string(e.what()));
        return false;
    }
}

bool FixSession::processIncomingMessage(const FixMessage& msg) {
    SESSION_DEBUG("Processing incoming message: " << msg.toString());
    
    messagesReceived_.fetch_add(1);
    updateHeartbeatTimers();
    
    // 驗證訊息格式
    if (!msg.isValid()) {
        auto [valid, reason] = msg.validateWithDetails();
        notifyError("Invalid message: " + reason);
        return false;
    }
    
    // 檢查 CompID
    auto msgSender = msg.getSenderCompID();
    auto msgTarget = msg.getTargetCompID();
    
    if (!msgSender || !msgTarget) {
        notifyError("Message missing SenderCompID or TargetCompID");
        return false;
    }
    
    // 如果 targetCompID_ 為空且這是第一個訊息，動態設定 CompID
    if (targetCompID_.empty()) {
        targetCompID_ = *msgSender;
        sessionID_ = generateSessionID(); // 重新生成 SessionID
        SESSION_DEBUG("Dynamic CompID assignment from message: target=" + targetCompID_);
    }
    
    if (*msgSender != targetCompID_ || *msgTarget != senderCompID_) {
        notifyError("CompID mismatch in message");
        return false;
    }
    
    // 驗證序號
    if (!validateSequenceNumber(msg)) {
        return false;
    }
    
    // 更新期望的下一個序號
    if (auto seqNum = msg.getMsgSeqNum()) {
        expectedIncomingSeqNum_.store(*seqNum + 1);
    }
    
    // 根據訊息類型處理
    auto msgType = msg.getMsgType();
    if (!msgType) {
        notifyError("Message missing MsgType");
        return false;
    }
    
    if (msg.isAdminMessage()) {
        return handleAdminMessage(msg);
    } else {
        // 只有在登入狀態才能處理應用訊息
        if (state_ != SessionState::LoggedIn) {
            notifyError("Received application message but not logged in");
            return false;
        }
        
        if (applicationMessageHandler_) {
            applicationMessageHandler_(msg);
        }
        return true;
    }
}

bool FixSession::sendApplicationMessage(const FixMessage& msg) {
    if (state_ != SessionState::LoggedIn) {
        notifyError("Cannot send application message: not logged in");
        return false;
    }
    
    return sendMessage(msg);
}

// ===== Heartbeat 機制 =====
bool FixSession::checkHeartbeat() {
    if (state_ != SessionState::LoggedIn) {
        return true; // 未登入時不需要檢查
    }
    
    auto now = std::chrono::steady_clock::now();
    
    // 檢查是否需要發送 Heartbeat
    if (shouldSendHeartbeat()) {
        SESSION_DEBUG("Sending periodic heartbeat");
        sendHeartbeat();
    }
    
    // 檢查是否超時未收到訊息
    if (isHeartbeatExpired()) {
        notifyError("Heartbeat timeout - no message received");
        setState(SessionState::Error);
        return false;
    }
    
    return true;
}

bool FixSession::needsHeartbeat() const {
    return shouldSendHeartbeat();
}

bool FixSession::sendHeartbeat(const std::string& testReqID) {
    SESSION_DEBUG("Sending heartbeat" << (testReqID.empty() ? "" : " (response to TestRequest)"));
    
    auto heartbeatMsg = FixMessageBuilder::createHeartbeat(testReqID);
    heartbeatMsg.setField(FixMessage::SenderCompID, senderCompID_);
    heartbeatMsg.setField(FixMessage::TargetCompID, targetCompID_);
    heartbeatMsg.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    
    return sendAdminMessage(heartbeatMsg);
}

bool FixSession::sendTestRequest() {
    SESSION_DEBUG("Sending test request");
    
    std::string testReqID = "TR" + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count());
    
    auto testReqMsg = FixMessageBuilder::createTestRequest(testReqID);
    testReqMsg.setField(FixMessage::SenderCompID, senderCompID_);
    testReqMsg.setField(FixMessage::TargetCompID, targetCompID_);
    testReqMsg.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    
    return sendAdminMessage(testReqMsg);
}

// ===== 統計資訊 =====
std::chrono::seconds FixSession::getSessionDuration() const {
    return std::chrono::duration_cast<std::chrono::seconds>(
        std::chrono::steady_clock::now() - sessionStartTime_);
}

// ===== 工具方法 =====
std::string FixSession::toString() const {
    std::ostringstream oss;
    oss << "FixSession[" << sessionID_ << "] "
        << senderCompID_ << "->" << targetCompID_ << " "
        << "State=" << getStateString() << " "
        << "SeqOut=" << outgoingSeqNum_.load() << " "
        << "SeqIn=" << expectedIncomingSeqNum_.load() << " "
        << "MsgRx=" << messagesReceived_.load() << " "
        << "MsgTx=" << messagesSent_.load() << " "
        << "Duration=" << getSessionDuration().count() << "s";
    return oss.str();
}

std::string FixSession::getStateString() const {
    return sessionStateToString(state_);
}

// ===== 內部訊息處理 =====
bool FixSession::handleAdminMessage(const FixMessage& msg) {
    auto msgType = msg.getMsgType();
    if (!msgType) return false;
    
    SESSION_DEBUG("Handling admin message type: " << *msgType);
    
    switch (*msgType) {
        case FixMessage::Logon:
            handleLogon(msg);
            break;
        case FixMessage::Logout:
            handleLogout(msg);
            break;
        case FixMessage::Heartbeat:
            handleHeartbeat(msg);
            break;
        case FixMessage::TestRequest:
            handleTestRequest(msg);
            break;
        default:
            SESSION_DEBUG("Unhandled admin message type: " << *msgType);
            return false;
    }
    
    return true;
}

void FixSession::handleLogon(const FixMessage& msg) {
    SESSION_DEBUG("Handling Logon message");
    
    if (state_ == SessionState::PendingLogon) {
        // 我們發起的登入收到回應
        setState(SessionState::LoggedIn);
        updateHeartbeatTimers();
        SESSION_DEBUG("Logon successful (initiated by us)");
    } else if (state_ == SessionState::Disconnected) {
        // 對方發起登入
        accept(msg);
        SESSION_DEBUG("Logon accepted (initiated by peer)");
    } else {
        notifyError("Unexpected Logon message in state: " + getStateString());
    }
}

void FixSession::handleLogout(const FixMessage& msg) {
    SESSION_DEBUG("Handling Logout message");
    
    if (state_ == SessionState::PendingLogout) {
        // 我們發起的登出收到回應
        setState(SessionState::LoggedOut);
        SESSION_DEBUG("Logout confirmed");
    } else if (state_ == SessionState::LoggedIn) {
        // 對方發起登出，回應並斷線
        auto logoutResp = FixMessageBuilder::createLogout("Logout acknowledged");
        logoutResp.setField(FixMessage::SenderCompID, senderCompID_);
        logoutResp.setField(FixMessage::TargetCompID, targetCompID_);
        logoutResp.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
        
        sendAdminMessage(logoutResp);
        setState(SessionState::LoggedOut);
        SESSION_DEBUG("Logout response sent");
    } else {
        SESSION_DEBUG("Logout message in unexpected state: " + getStateString());
    }
}

void FixSession::handleHeartbeat(const FixMessage& msg) {
    SESSION_DEBUG("Handling Heartbeat message");
    
    // Heartbeat 主要用於維持連線，不需特殊處理
    // 時間戳已在 processIncomingMessage 中更新
    
    if (msg.hasField(FixTags::TestReqID)) {
        SESSION_DEBUG("Heartbeat is response to our TestRequest");
    }
}

void FixSession::handleTestRequest(const FixMessage& msg) {
    SESSION_DEBUG("Handling TestRequest message");
    
    std::string testReqID;
    if (msg.hasField(FixTags::TestReqID)) {
        testReqID = msg.getField(FixTags::TestReqID);
    }
    
    // 立即回應 Heartbeat
    sendHeartbeat(testReqID);
}

void FixSession::handleResendRequest(const FixMessage& msg) {
    SESSION_DEBUG("Handling ResendRequest message");
    
    // 簡化實作：記錄但不實際重送
    // 生產環境需要保存已發送的訊息並重送
    if (msg.hasField(7) && msg.hasField(16)) { // BeginSeqNo, EndSeqNo
        std::string beginSeqNo = msg.getField(7);
        std::string endSeqNo = msg.getField(16);
        SESSION_DEBUG("ResendRequest for messages " << beginSeqNo << " to " << endSeqNo);
        
        // TODO: 實作訊息重送邏輯
        notifyError("ResendRequest received but not implemented");
    }
}

void FixSession::handleSequenceReset(const FixMessage& msg) {
    SESSION_DEBUG("Handling SequenceReset message");
    
    // 簡化實作：重設期望序號
    if (msg.hasField(36)) { // NewSeqNo
        try {
            uint32_t newSeqNo = std::stoul(msg.getField(36));
            expectedIncomingSeqNum_.store(newSeqNo);
            SESSION_DEBUG("Sequence reset to: " << newSeqNo);
        } catch (...) {
            notifyError("Invalid NewSeqNo in SequenceReset");
        }
    }
}

// ===== 序號驗證 =====
bool FixSession::validateSequenceNumber(const FixMessage& msg) {
    auto seqNumOpt = msg.getMsgSeqNum();
    if (!seqNumOpt) {
        notifyError("Message missing sequence number");
        return false;
    }
    
    uint32_t receivedSeqNum = static_cast<uint32_t>(*seqNumOpt);
    uint32_t expectedSeqNum = expectedIncomingSeqNum_.load();
    
    if (receivedSeqNum == expectedSeqNum) {
        // 正確的序號
        return true;
    } else if (receivedSeqNum < expectedSeqNum) {
        // 重複的訊息，忽略
        SESSION_DEBUG("Duplicate message: received=" << receivedSeqNum << ", expected=" << expectedSeqNum);
        return false;
    } else {
        // 序號跳躍，可能遺失訊息
        SESSION_DEBUG("Sequence gap detected: received=" << receivedSeqNum << ", expected=" << expectedSeqNum);
        handleSequenceGap(expectedSeqNum, receivedSeqNum);
        return true; // 先處理這個訊息，後續會請求重送
    }
}

void FixSession::handleSequenceGap(uint32_t expectedSeqNum, uint32_t receivedSeqNum) {
    SESSION_DEBUG("Requesting resend from " << expectedSeqNum << " to " << (receivedSeqNum - 1));
    sendResendRequest(expectedSeqNum, receivedSeqNum - 1);
}

void FixSession::sendResendRequest(uint32_t beginSeqNum, uint32_t endSeqNum) {
    // FIX 4.2 Resend Request (MsgType = 2)
    FixMessage resendReq('2');
    resendReq.setField(FixMessage::SenderCompID, senderCompID_);
    resendReq.setField(FixMessage::TargetCompID, targetCompID_);
    resendReq.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    resendReq.setField(7, std::to_string(beginSeqNum));  // BeginSeqNo
    resendReq.setField(16, std::to_string(endSeqNum));   // EndSeqNo
    
    sendAdminMessage(resendReq);
}

// ===== 訊息發送 =====
bool FixSession::sendMessage(const FixMessage& msg) {
    if (!sendFunction_) {
        notifyError("No send function configured");
        return false;
    }
    
    // 複製訊息並設定 Session 資訊
    FixMessage outMsg = msg;
    outMsg.setField(FixMessage::SenderCompID, senderCompID_);
    outMsg.setField(FixMessage::TargetCompID, targetCompID_);
    
    // 如果沒有序號，自動分配
    if (!outMsg.hasField(FixMessage::MsgSeqNum)) {
        outMsg.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    }
    
    std::string serialized = outMsg.serialize();
    
    if (sendFunction_(serialized)) {
        messagesSent_.fetch_add(1);
        updateHeartbeatTimers();
        SESSION_DEBUG("Message sent successfully");
        return true;
    } else {
        notifyError("Failed to send message");
        return false;
    }
}

bool FixSession::sendAdminMessage(const FixMessage& msg) {
    return sendMessage(msg);
}

FixMessage FixSession::createBaseMessage(char msgType) const {
    FixMessage msg(msgType);
    msg.setField(FixMessage::SenderCompID, senderCompID_);
    msg.setField(FixMessage::TargetCompID, targetCompID_);
    return msg;
}

// ===== 狀態管理 =====
void FixSession::setState(SessionState newState) {
    if (state_ != newState) {
        SESSION_DEBUG("State change: " << sessionStateToString(state_) << " -> " << sessionStateToString(newState));
        state_ = newState;
        
        if (newState == SessionState::LoggedIn) {
            sessionStartTime_ = std::chrono::steady_clock::now();
        }
    }
}

void FixSession::updateHeartbeatTimers() {
    auto now = std::chrono::steady_clock::now();
    lastHeartbeat_ = now;
    lastReceivedMessage_ = now;
}

void FixSession::notifyError(const std::string& error) {
    SESSION_DEBUG("ERROR: " << error);
    if (errorHandler_) {
        errorHandler_(error);
    }
}

// ===== 輔助方法 =====
std::string FixSession::generateSessionID() const {
    std::ostringstream oss;
    oss << senderCompID_ << "-" << targetCompID_ << "-" 
        << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
    return oss.str();
}

bool FixSession::isHeartbeatExpired() const {
    if (state_ != SessionState::LoggedIn) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastReceivedMessage_);
    
    // 允許 1.2 倍的容忍度
    return elapsed > heartbeatInterval_ * 1.2;
}

bool FixSession::shouldSendHeartbeat() const {
    if (state_ != SessionState::LoggedIn) {
        return false;
    }
    
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - lastHeartbeat_);
    
    return elapsed >= heartbeatInterval_;
}

void FixSession::sendLogonResponse() {
    auto logonResp = FixMessageBuilder::createLogon();
    logonResp.setField(FixMessage::SenderCompID, senderCompID_);
    logonResp.setField(FixMessage::TargetCompID, targetCompID_);
    logonResp.setField(FixMessage::MsgSeqNum, std::to_string(getNextOutgoingSeqNum()));
    logonResp.setField(108, std::to_string(heartbeatInterval_.count())); // HeartBtInt
    
    sendAdminMessage(logonResp);
}

// ===== 工具函式 =====
std::string sessionStateToString(SessionState state) {
    switch (state) {
        case SessionState::Disconnected: return "Disconnected";
        case SessionState::PendingLogon: return "PendingLogon";
        case SessionState::LoggedIn: return "LoggedIn";
        case SessionState::PendingLogout: return "PendingLogout";
        case SessionState::LoggedOut: return "LoggedOut";
        case SessionState::Error: return "Error";
        default: return "Unknown";
    }
}

} // namespace mts::protocol