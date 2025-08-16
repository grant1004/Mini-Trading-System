#include "fix_message.h"
#include "../core/order.h"  // 引入 Side enum
#include <string>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>

namespace mts {
namespace protocol {

// 靜態變數：訊息序號
static std::atomic<uint32_t> g_msgSeqNum{1};

// 計算 FIX Checksum
static std::string calculateChecksum(const std::string& message) {
    int sum = 0;
    for (char c : message) {
        sum += static_cast<unsigned char>(c);
    }
    int checksum = sum % 256;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << checksum;
    return oss.str();
}

// 取得當前時間的 FIX 格式字串
static std::string getCurrentFixTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    return oss.str();
}

// 建構函式
FixMessage::FixMessage(char msgType) {
    setField(BeginString, "FIX.4.2");
    setField(MsgType, std::string(1, msgType));
    setField(MsgSeqNum, std::to_string(g_msgSeqNum.fetch_add(1)));
    setField(SendingTime, getCurrentFixTime());
}

// 解析 FIX 訊息
FixMessage FixMessage::parse(const std::string& rawMessage) {
    FixMessage msg;
    
    if (rawMessage.empty()) {
        throw std::runtime_error("Empty FIX message");
    }
    
    size_t pos = 0;
    while (pos < rawMessage.length()) {
        // 尋找等號
        size_t equalPos = rawMessage.find('=', pos);
        if (equalPos == std::string::npos) {
            break;  // 沒有更多欄位
        }
        
        // 尋找 SOH 分隔符
        size_t sohPos = rawMessage.find(SOH, equalPos);
        if (sohPos == std::string::npos) {
            sohPos = rawMessage.length();  // 最後一個欄位
        }
        
        // 提取 tag 和 value
        std::string tagStr = rawMessage.substr(pos, equalPos - pos);
        std::string value = rawMessage.substr(equalPos + 1, sohPos - equalPos - 1);
        
        try {
            int tag = std::stoi(tagStr);
            msg.setField(tag, value);
        } catch (const std::exception& e) {
            throw std::runtime_error("Invalid tag in FIX message: " + tagStr);
        }
        
        pos = sohPos + 1;  // 移到下一個欄位
    }
    
    return msg;
}

// 建構 FIX 訊息
std::string FixMessage::serialize() const {
    std::ostringstream oss;
    
    // 必須按照特定順序輸出關鍵欄位
    std::vector<int> orderedTags = {BeginString, BodyLength, MsgType};
    
    // 先處理標準標頭欄位
    for (int tag : orderedTags) {
        auto it = fields_.find(tag);
        if (it != fields_.end()) {
            if (tag != BodyLength) {  // BodyLength 稍後計算
                oss << tag << "=" << it->second << SOH;
            }
        }
    }
    
    // 添加其他欄位（除了 CheckSum）
    for (const auto& [tag, value] : fields_) {
        if (tag != BeginString && tag != BodyLength && tag != MsgType && tag != CheckSum) {
            oss << tag << "=" << value << SOH;
        }
    }
    
    std::string bodyPart = oss.str();
    
    // 計算 BodyLength（從 MsgType 到 CheckSum 之前的長度）
    size_t msgTypePos = bodyPart.find(std::to_string(MsgType) + "=");
    if (msgTypePos != std::string::npos) {
        size_t bodyLength = bodyPart.length() - msgTypePos;
        
        // 重新建構訊息，加入 BodyLength
        std::ostringstream finalOss;
        finalOss << BeginString << "=" << getField(BeginString) << SOH;
        finalOss << BodyLength << "=" << bodyLength << SOH;
        finalOss << bodyPart.substr(msgTypePos);  // 從 MsgType 開始的部分
        
        std::string messageWithoutChecksum = finalOss.str();
        std::string checksum = calculateChecksum(messageWithoutChecksum);
        
        finalOss << CheckSum << "=" << checksum << SOH;
        
        return finalOss.str();
    }
    
    throw std::runtime_error("Failed to serialize FIX message");
}

// 欄位操作
void FixMessage::setField(FieldTag tag, const FieldValue& value) {
    fields_[tag] = value;
}

FieldValue FixMessage::getField(FieldTag tag) const {
    auto it = fields_.find(tag);
    if (it != fields_.end()) {
        return it->second;
    }
    return "";  // 欄位不存在時返回空字串
}

bool FixMessage::hasField(FieldTag tag) const {
    return fields_.find(tag) != fields_.end();
}

// 訊息驗證
bool FixMessage::isValid() const {
    // 檢查必要欄位
    std::vector<int> requiredFields = {BeginString, BodyLength, MsgType, CheckSum};
    
    for (int tag : requiredFields) {
        if (!hasField(tag) || getField(tag).empty()) {
            return false;
        }
    }
    
    // 檢查 BeginString 版本
    std::string beginString = getField(BeginString);
    if (beginString != "FIX.4.2" && beginString != "FIX.4.4") {
        return false;
    }
    
    // 檢查 MsgType 是否有效
    std::string msgType = getField(MsgType);
    if (msgType.empty()) {
        return false;
    }
    
    // 驗證 Checksum（簡化版本）
    try {
        std::string currentMsg = const_cast<FixMessage*>(this)->serialize();
        // 這裡可以進一步驗證 checksum 計算是否正確
        return true;
    } catch (...) {
        return false;
    }
}

// toString 方法
std::string FixMessage::toString() const {
    std::ostringstream oss;
    oss << "FixMessage[";
    
    // 顯示關鍵欄位
    if (hasField(MsgType)) {
        oss << "MsgType=" << getField(MsgType);
    }
    if (hasField(Symbol)) {
        oss << ", Symbol=" << getField(Symbol);
    }
    if (hasField(Side)) {
        oss << ", Side=" << getField(Side);
    }
    if (hasField(OrderQty)) {
        oss << ", Qty=" << getField(OrderQty);
    }
    if (hasField(Price)) {
        oss << ", Price=" << getField(Price);
    }
    
    oss << ", Fields=" << fields_.size() << "]";
    return oss.str();
}

// 輔助方法實作
char FixMessage::getMsgType() const {
    auto it = fields_.find(MsgType);
    if (it == fields_.end() || it->second.empty()) {
        throw std::runtime_error("Missing or empty MsgType field");
    }
    return it->second[0];  // 取第一個字元
}

std::string FixMessage::getSymbol() const {
    auto it = fields_.find(Symbol);
    if (it == fields_.end()) {
        return "";  // 或拋出異常，看需求
    }
    return it->second;
}

mts::core::Side FixMessage::getSide() const {
    auto it = fields_.find(Side);
    if (it == fields_.end() || it->second.empty()) {
        throw std::runtime_error("Missing or empty Side field");
    }
    
    const std::string& sideValue = it->second;
    if (sideValue == "1") {
        return mts::core::Side::Buy;
    } else if (sideValue == "2") {
        return mts::core::Side::Sell;
    } else {
        throw std::runtime_error("Invalid Side value: " + sideValue);
    }
}

double FixMessage::getPrice() const {
    auto it = fields_.find(Price);
    if (it == fields_.end() || it->second.empty()) {
        return 0.0;  // 市價單情況
    }
    
    try {
        return std::stod(it->second);
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid Price value: " + it->second);
    }
}

uint64_t FixMessage::getQuantity() const {
    auto it = fields_.find(OrderQty);
    if (it == fields_.end() || it->second.empty()) {
        throw std::runtime_error("Missing or empty OrderQty field");
    }
    
    try {
        return std::stoull(it->second);
    } catch (const std::exception& e) {
        throw std::runtime_error("Invalid OrderQty value: " + it->second);
    }
}

// 更安全的輔助方法版本
char FixMessage::getMsgTypeOr(char defaultValue) const noexcept {
    auto it = fields_.find(MsgType);
    if (it != fields_.end() && !it->second.empty()) {
        return it->second[0];
    }
    return defaultValue;
}

std::string FixMessage::getSymbolOr(const std::string& defaultValue) const noexcept {
    auto it = fields_.find(Symbol);
    if (it != fields_.end()) {
        return it->second;
    }
    return defaultValue;
}

double FixMessage::getPriceOr(double defaultValue) const noexcept {
    auto it = fields_.find(Price);
    if (it != fields_.end() && !it->second.empty()) {
        try {
            return std::stod(it->second);
        } catch (...) {
            // 轉換失敗，返回預設值
        }
    }
    return defaultValue;
}

uint64_t FixMessage::getQuantityOr(uint64_t defaultValue) const noexcept {
    auto it = fields_.find(OrderQty);
    if (it != fields_.end() && !it->second.empty()) {
        try {
            return std::stoull(it->second);
        } catch (...) {
            // 轉換失敗，返回預設值
        }
    }
    return defaultValue;
}

// 便利的訊息建構方法
FixMessage FixMessage::createNewOrderSingle(
    const std::string& clOrdId,
    const std::string& symbol,
    char side,
    const std::string& orderQty,
    char ordType,
    const std::string& price) {
    
    FixMessage msg(NewOrderSingle);
    msg.setField(ClOrdID, clOrdId);
    msg.setField(Symbol, symbol);
    msg.setField(Side, std::string(1, side));
    msg.setField(OrderQty, orderQty);
    msg.setField(OrdType, std::string(1, ordType));
    
    if (ordType == '2' && !price.empty()) {  // Limit Order
        msg.setField(Price, price);
    }
    
    return msg;
}

FixMessage FixMessage::createExecutionReport(
    const std::string& orderID,
    const std::string& execID,
    char execType,
    char ordStatus,
    const std::string& symbol,
    char side,
    const std::string& leavesQty,
    const std::string& cumQty) {
    
    FixMessage msg(ExecutionReport);
    msg.setField(ClOrdID, orderID);
    msg.setField(ExecID, execID);
    msg.setField(ExecType, std::string(1, execType));
    msg.setField(OrdStatus, std::string(1, ordStatus));
    msg.setField(Symbol, symbol);
    msg.setField(Side, std::string(1, side));
    msg.setField(LeavesQty, leavesQty);
    msg.setField(CumQty, cumQty);
    
    return msg;
}

} // namespace protocol
} // namespace mts