#pragma once
#include <string>
#include <map>
#include <vector>

// 前向宣告，避免循環引用
namespace mts {
namespace core {
    enum class Side : char;
}
}

namespace mts {
namespace protocol {

using FieldTag = int;
using FieldValue = std::string;

class FixMessage {
public:
    static constexpr char SOH = '\x01';  // FIX 分隔符
    
    // 常用 FIX Tags
    enum Tags {
        BeginString = 8,
        BodyLength = 9,
        MsgType = 35,
        SenderCompID = 49,
        TargetCompID = 56,
        MsgSeqNum = 34,
        SendingTime = 52,
        CheckSum = 10,
        
        // 訂單相關
        ClOrdID = 11,      // Client Order ID
        Symbol = 55,
        Side = 54,
        OrderQty = 38,
        OrdType = 40,
        Price = 44,
        TimeInForce = 59,
        
        // 執行回報
        ExecID = 17,
        ExecType = 150,
        OrdStatus = 39,
        LeavesQty = 151,
        CumQty = 14
    };
    
    // 訊息類型
    enum MsgTypes {
        NewOrderSingle = 'D',
        ExecutionReport = '8',
        OrderCancelRequest = 'F',
        Logon = 'A',
        Logout = '5',
        Heartbeat = '0',
        TestRequest = '1'
    };
    
private:
    std::map<FieldTag, FieldValue> fields_;
    
public:
    // 建構函式
    FixMessage() = default;
    explicit FixMessage(char msgType);
    
    // 解析 FIX 訊息
    static FixMessage parse(const std::string& rawMessage);
    
    // 建構 FIX 訊息
    std::string serialize() const;
    
    // 欄位操作
    void setField(FieldTag tag, const FieldValue& value);
    FieldValue getField(FieldTag tag) const;
    bool hasField(FieldTag tag) const;
    
    // 訊息驗證
    bool isValid() const;
    
    // ===== 原始的輔助方法（會拋出異常）=====
    char getMsgType() const;
    std::string getSymbol() const;
    mts::core::Side getSide() const;
    double getPrice() const;
    uint64_t getQuantity() const;
    
    // ===== 新增：安全的 Or 方法（不會拋出異常）=====
    char getMsgTypeOr(char defaultValue = '?') const noexcept;
    std::string getSymbolOr(const std::string& defaultValue = "") const noexcept;
    double getPriceOr(double defaultValue = 0.0) const noexcept;
    uint64_t getQuantityOr(uint64_t defaultValue = 0) const noexcept;
    
    // 便利方法
    bool isNewOrder() const { return getMsgTypeOr() == NewOrderSingle; }
    bool isExecutionReport() const { return getMsgTypeOr() == ExecutionReport; }
    bool isLogon() const { return getMsgTypeOr() == Logon; }
    
    // 字串表示
    std::string toString() const;
    
    // 靜態工廠方法
    static FixMessage createNewOrderSingle(
        const std::string& clOrdId,
        const std::string& symbol,
        char side,
        const std::string& orderQty,
        char ordType,
        const std::string& price = "");
        
    static FixMessage createExecutionReport(
        const std::string& orderID,
        const std::string& execID,
        char execType,
        char ordStatus,
        const std::string& symbol,
        char side,
        const std::string& leavesQty,
        const std::string& cumQty);
};

} // namespace protocol
} // namespace mts