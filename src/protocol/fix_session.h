#pragma once
#include "fix_message.h"
#include <string>
#include <chrono>
#include <atomic>

namespace mts {
namespace protocol {

class FixSession {
public:
    enum State {
        Disconnected,
        Connected,
        LoggedOn,
        LoggedOut
    };
    
private:
    std::string senderCompID_;
    std::string targetCompID_;
    std::atomic<int> outgoingSeqNum_{1};
    std::atomic<int> incomingSeqNum_{1};
    State state_{Disconnected};
    
public:
    FixSession(const std::string& sender, const std::string& target);
    
    // Session 管理
    FixMessage createLogon();
    FixMessage createLogout();
    FixMessage createHeartbeat();
    
    // 訊息處理
    bool validateMessage(const FixMessage& msg);
    FixMessage createExecutionReport(const mts::core::Order& order, 
                                   const std::string& execType);
    
    // Sequence 管理
    int getNextOutgoingSeqNum() { return outgoingSeqNum_++; }
    bool validateIncomingSeqNum(int seqNum);
    
    // State 管理
    State getState() const { return state_; }
    void setState(State state) { state_ = state; }
};

}} // namespace mts::protocol