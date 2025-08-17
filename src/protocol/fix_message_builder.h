// ============================================================================
// 2. fix_message_builder.h - 專門用於建構 FIX 訊息
// ============================================================================
#pragma once
#include "fix_message.h"
#include "../core/order.h"
#include <string>

namespace mts::protocol {

class FixMessageBuilder {
public:
    // Session 資訊
    FixMessageBuilder& setSenderCompID(const std::string& sender);
    FixMessageBuilder& setTargetCompID(const std::string& target);
    FixMessageBuilder& setMsgSeqNum(int seqNum);

    // ===== 管理訊息建構 =====
    static FixMessage createLogon(const std::string& username = "", 
                                 const std::string& password = "");
    static FixMessage createLogout(const std::string& text = "");
    static FixMessage createHeartbeat(const std::string& testReqID = "");
    static FixMessage createTestRequest(const std::string& testReqID);

    // ===== 業務訊息建構 =====
    static FixMessage createNewOrderSingle(
        const std::string& clOrdId,
        const std::string& symbol,
        mts::core::Side side,
        uint64_t quantity,
        mts::core::OrderType orderType,
        double price = 0.0,
        mts::core::TimeInForce tif = mts::core::TimeInForce::Day
    );

    static FixMessage createOrderCancelRequest(
        const std::string& origClOrdId,
        const std::string& clOrdId,
        const std::string& symbol,
        mts::core::Side side
    );

    static FixMessage createExecutionReport(
        const mts::core::Order& order,
        const std::string& execId,
        char execType,  // '0'=New, '1'=PartialFill, '2'=Fill, '4'=Canceled
        uint64_t lastQty = 0,
        double lastPx = 0.0
    );

private:
    std::string senderCompID_;
    std::string targetCompID_;
    int msgSeqNum_ = 1;

    // 輔助方法
    static FixMessage createBaseMessage(char msgType);
    static std::string generateExecID();
    static char orderTypeToFixChar(mts::core::OrderType type);
    static char sideToFixChar(mts::core::Side side);
    static char tifToFixChar(mts::core::TimeInForce tif);
};

} // namespace mts::protocol