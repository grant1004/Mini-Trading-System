#include "fix_message_builder.h"
#include <string>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <stdexcept>

namespace mts::protocol {

// 靜態變數：執行報告序號
static std::atomic<uint64_t> g_execIdCounter{1};

// ===== 實例方法：Session 資訊設定 =====

FixMessageBuilder& FixMessageBuilder::setSenderCompID(const std::string& sender) {
    senderCompID_ = sender;
    return *this;
}

FixMessageBuilder& FixMessageBuilder::setTargetCompID(const std::string& target) {
    targetCompID_ = target;
    return *this;
}

FixMessageBuilder& FixMessageBuilder::setMsgSeqNum(int seqNum) {
    msgSeqNum_ = seqNum;
    return *this;
}

// ===== 管理訊息建構 =====

FixMessage FixMessageBuilder::createLogon(const std::string& username, 
                                          const std::string& password) {
    FixMessage msg = createBaseMessage('A');  // Logon message
    
    // 設定登入相關欄位
    if (!username.empty()) {
        msg.setField(553, username);  // Username
    }
    if (!password.empty()) {
        msg.setField(554, password);  // Password
    }
    
    // 設定加密方式為無加密
    msg.setField(98, "0");  // EncryptMethod = None
    
    // 設定心跳間隔（30秒）
    msg.setField(108, "30");  // HeartBtInt
    
    return msg;
}

FixMessage FixMessageBuilder::createLogout(const std::string& text) {
    FixMessage msg = createBaseMessage('5');  // Logout message
    
    if (!text.empty()) {
        msg.setField(58, text);  // Text
    }
    
    return msg;
}

FixMessage FixMessageBuilder::createHeartbeat(const std::string& testReqID) {
    FixMessage msg = createBaseMessage('0');  // Heartbeat message
    
    if (!testReqID.empty()) {
        msg.setField(112, testReqID);  // TestReqID
    }
    
    return msg;
}

FixMessage FixMessageBuilder::createTestRequest(const std::string& testReqID) {
    FixMessage msg = createBaseMessage('1');  // TestRequest message
    
    // TestReqID 是必填欄位
    msg.setField(112, testReqID);  // TestReqID
    
    return msg;
}

// ===== 業務訊息建構 =====

FixMessage FixMessageBuilder::createNewOrderSingle(
    const std::string& clOrdId,
    const std::string& symbol,
    mts::core::Side side,
    uint64_t quantity,
    mts::core::OrderType orderType,
    double price,
    mts::core::TimeInForce tif) {
    
    FixMessage msg = createBaseMessage('D');  // NewOrderSingle message
    
    // 必填欄位
    msg.setField(11, clOrdId);                           // ClOrdID
    msg.setField(55, symbol);                            // Symbol
    msg.setField(54, std::string(1, sideToFixChar(side)));              // Side
    msg.setField(38, std::to_string(quantity));          // OrderQty
    msg.setField(40, std::string(1, orderTypeToFixChar(orderType)));    // OrdType
    msg.setField(59, std::string(1, tifToFixChar(tif)));                // TimeInForce
    
    // 價格欄位（限價單才需要）
    if (orderType == mts::core::OrderType::Limit || 
        orderType == mts::core::OrderType::StopLimit) {
        std::ostringstream priceStr;
        priceStr << std::fixed << std::setprecision(2) << price;
        msg.setField(44, priceStr.str());               // Price
    }
    
    // 停損價格（停損單才需要）
    if (orderType == mts::core::OrderType::Stop || 
        orderType == mts::core::OrderType::StopLimit) {
        std::ostringstream stopPriceStr;
        stopPriceStr << std::fixed << std::setprecision(2) << price;
        msg.setField(99, stopPriceStr.str());           // StopPx
    }
    
    // 交易時間（當前時間）
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream timeStr;
    timeStr << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    msg.setField(60, timeStr.str());                    // TransactTime
    
    return msg;
}

FixMessage FixMessageBuilder::createOrderCancelRequest(
    const std::string& origClOrdId,
    const std::string& clOrdId,
    const std::string& symbol,
    mts::core::Side side) {
    
    FixMessage msg = createBaseMessage('F');  // OrderCancelRequest message
    
    // 必填欄位
    msg.setField(41, origClOrdId);                       // OrigClOrdID
    msg.setField(11, clOrdId);                           // ClOrdID
    msg.setField(55, symbol);                            // Symbol
    msg.setField(54, std::string(1, sideToFixChar(side)));              // Side
    
    // 交易時間（當前時間）
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream timeStr;
    timeStr << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    msg.setField(60, timeStr.str());                    // TransactTime
    
    return msg;
}

FixMessage FixMessageBuilder::createExecutionReport(
    const mts::core::Order& order,
    const std::string& execId,
    char execType,
    uint64_t lastQty,
    double lastPx) {
    
    FixMessage msg = createBaseMessage('8');  // ExecutionReport message
    
    // 訂單識別
    msg.setField(11, std::to_string(order.getOrderId()));  // ClOrdID (使用OrderID)
    msg.setField(17, execId);                               // ExecID
    msg.setField(150, std::string(1, execType));           // ExecType
    
    // 訂單狀態對應
    char ordStatus;
    switch (order.getStatus()) {
        case mts::core::OrderStatus::New:
            ordStatus = '0'; break;
        case mts::core::OrderStatus::PartiallyFilled:
            ordStatus = '1'; break;
        case mts::core::OrderStatus::Filled:
            ordStatus = '2'; break;
        case mts::core::OrderStatus::Cancelled:
            ordStatus = '4'; break;
        case mts::core::OrderStatus::Rejected:
            ordStatus = '8'; break;
        default:
            ordStatus = '0'; break;
    }
    msg.setField(39, std::string(1, ordStatus));           // OrdStatus
    
    // 訂單基本資訊
    msg.setField(55, order.getSymbol());                   // Symbol
    msg.setField(54, std::string(1, sideToFixChar(order.getSide())));      // Side
    msg.setField(38, std::to_string(order.getQuantity())); // OrderQty
    msg.setField(40, std::string(1, orderTypeToFixChar(order.getOrderType()))); // OrdType
    
    // 價格資訊
    if (order.getOrderType() != mts::core::OrderType::Market) {
        std::ostringstream priceStr;
        priceStr << std::fixed << std::setprecision(2) << order.getPrice();
        msg.setField(44, priceStr.str());                 // Price
    }
    
    // 執行資訊
    msg.setField(151, std::to_string(order.getRemainingQuantity())); // LeavesQty
    msg.setField(14, std::to_string(order.getFilledQuantity()));     // CumQty
    
    // 最後成交資訊（如果有成交）
    if (lastQty > 0) {
        msg.setField(32, std::to_string(lastQty));         // LastQty
        
        if (lastPx > 0.0) {
            std::ostringstream lastPxStr;
            lastPxStr << std::fixed << std::setprecision(2) << lastPx;
            msg.setField(31, lastPxStr.str());             // LastPx
        }
    }
    
    // 時效性
    msg.setField(59, std::string(1, tifToFixChar(order.getTimeInForce()))); // TimeInForce
    
    // 交易時間（當前時間）
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    std::ostringstream timeStr;
    timeStr << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    msg.setField(60, timeStr.str());                      // TransactTime
    
    return msg;
}

// ===== 私有輔助方法 =====

FixMessage FixMessageBuilder::createBaseMessage(char msgType) {
    return FixMessage(msgType);  // 使用 FixMessage 建構函式
}

std::string FixMessageBuilder::generateExecID() {
    // 產生唯一的執行ID：時間戳 + 序號
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    uint64_t execNum = g_execIdCounter.fetch_add(1);
    
    std::ostringstream oss;
    oss << "EXEC_" << timestamp << "_" << execNum;
    return oss.str();
}

char FixMessageBuilder::orderTypeToFixChar(mts::core::OrderType type) {
    switch (type) {
        case mts::core::OrderType::Market:
            return '1';
        case mts::core::OrderType::Limit:
            return '2';
        case mts::core::OrderType::Stop:
            return '3';
        case mts::core::OrderType::StopLimit:
            return '4';
        default:
            throw std::invalid_argument("Invalid order type: " + std::to_string(static_cast<int>(type)));
    }
}

char FixMessageBuilder::sideToFixChar(mts::core::Side side) {
    switch (side) {
        case mts::core::Side::Buy:
            return '1';
        case mts::core::Side::Sell:
            return '2';
        default:
            throw std::invalid_argument("Invalid side: " + std::to_string(static_cast<int>(side)));
    }
}

char FixMessageBuilder::tifToFixChar(mts::core::TimeInForce tif) {
    switch (tif) {
        case mts::core::TimeInForce::Day:
            return '0';
        case mts::core::TimeInForce::GTC:
            return '1';
        case mts::core::TimeInForce::IOC:
            return '3';
        case mts::core::TimeInForce::FOK:
            return '4';
        default:
            throw std::invalid_argument("Invalid time in force: " + std::to_string(static_cast<int>(tif)));
    }
}

} // namespace mts::protocol