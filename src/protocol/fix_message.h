// ============================================================================
// 1. fix_message.h - 純粹的 FIX 協議載體
// ============================================================================
#pragma once
#include <string>
#include <map>
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <limits>
#include <optional>

namespace mts {

namespace protocol {

using FieldTag = int;
using FieldValue = std::string;

// 🎯 核心設計理念：FixMessage 是純粹的資料載體
class FixMessage {
public:
    static constexpr char SOH = '\x01';  // FIX 分隔符
    
    // FIX 標準 Tags（只定義協議層面的）
    enum StandardTags {
        BeginString = 8,
        BodyLength = 9,
        MsgType = 35,
        SenderCompID = 49,
        TargetCompID = 56,
        MsgSeqNum = 34,
        SendingTime = 52,
        CheckSum = 10
    };
    
    // 標準訊息類型（協議層面）
    enum StandardMsgTypes {

        Heartbeat = '0',
        TestRequest = '1',
        Logon = 'A',
        Logout = '5',
        NewOrderSingle = 'D',
        ExecutionReport = '8',
        OrderCancelRequest = 'F'
    
    };

private:
    std::map<FieldTag, FieldValue> fields_;

public:
    // ===== 核心功能：解析與序列化 =====
    FixMessage() = default;
    FixMessage(char msgType);
    
    // 從原始字串解析
    static FixMessage parse(const std::string& rawMessage);

    // 🆕 從原始字串解析 (不驗證 checksum，測試用)
    static FixMessage parseUnsafe(const std::string& rawMessage);
    
    // 序列化為 FIX 字串
    std::string serialize() const;

    // ===== 欄位操作 =====
    void setField(FieldTag tag, const FieldValue& value);
    FieldValue getField(FieldTag tag) const;
    std::optional<FieldValue> getFieldOptional(FieldTag tag) const;
    bool hasField(FieldTag tag) const;
    void removeField(FieldTag tag);
    
    // 取得所有欄位（用於偵錯）
    const std::map<FieldTag, FieldValue>& getAllFields() const { return fields_; }

    // ===== 基本驗證 =====
    bool isValid() const;
    std::pair<bool, std::string> validateWithDetails() const;
    bool validateChecksum() const;

    // ===== 便利方法（協議層面的）=====
    std::optional<char> getMsgType() const;
    std::optional<std::string> getSenderCompID() const;
    std::optional<std::string> getTargetCompID() const;
    std::optional<int> getMsgSeqNum() const;

    // 訊息類型判斷
    bool isAdminMessage() const;  // 系統管理訊息
    bool isApplicationMessage() const;  // 業務應用訊息

    // ===== 工具方法 =====
    std::string toString() const;
    size_t getFieldCount() const { return fields_.size(); }

private:
    // 內部輔助方法
    std::string calculateChecksum(const std::string& messageBody) const;
    std::string getCurrentFixTimestamp() const;
    bool validateRequiredFields() const;
    // 🆕 內部解析方法，可控制是否驗證 checksum
    static FixMessage parseWithValidation(const std::string& rawMessage, bool validateChecksum);
    std::string buildMessageWithoutChecksum() const ;
    std::string buildBodyContent() const ;
};


} // namespace mts::protocol

} // namespace mts