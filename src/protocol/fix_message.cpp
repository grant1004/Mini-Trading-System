#include "fix_message.h"
#include "../core/order.h"  // 引入 Side enum
#include <string>
#include <chrono>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <algorithm>
#include <iostream>
#include <ctime>
#include <optional>

// ===== DEBUG 配置 =====
#ifdef ENABLE_FIX_DEBUG
    #define FIX_DEBUG(msg) std::cout << "[FIX_DEBUG] " << msg << std::endl
#else
    #define FIX_DEBUG(msg) do {} while(0)
#endif

#ifdef ENABLE_FIX_CHECKSUM_DEBUG
    #define FIX_CHECKSUM_DEBUG(msg) std::cout << "[FIX_CHECKSUM] " << msg << std::endl
#else
    #define FIX_CHECKSUM_DEBUG(msg) do {} while(0)
#endif

#ifdef ENABLE_FIX_PARSE_DEBUG
    #define FIX_PARSE_DEBUG(msg) std::cout << "[FIX_PARSE] " << msg << std::endl
#else
    #define FIX_PARSE_DEBUG(msg) do {} while(0)
#endif

#ifdef ENABLE_FIX_SERIALIZE_DEBUG
    #define FIX_SERIALIZE_DEBUG(msg) std::cout << "[FIX_SERIALIZE] " << msg << std::endl
#else
    #define FIX_SERIALIZE_DEBUG(msg) do {} while(0)
#endif

#ifdef ENABLE_FIX_VALIDATION_DEBUG
    #define FIX_VALIDATION_DEBUG(msg) std::cout << "[FIX_VALIDATION] " << msg << std::endl
#else
    #define FIX_VALIDATION_DEBUG(msg) do {} while(0)
#endif

#ifdef ENABLE_FIX_FACTORY_DEBUG
    #define FIX_FACTORY_DEBUG(msg) std::cout << "[FIX_FACTORY] " << msg << std::endl
#else
    #define FIX_FACTORY_DEBUG(msg) do {} while(0)
#endif

namespace mts {
namespace protocol {

// 靜態變數：訊息序號
static std::atomic<uint32_t> g_msgSeqNum{1};

// 取得當前時間的 FIX 格式字串 (YYYYMMDD-HH:MM:SS.sss)
static std::string getCurrentFixTime() {
    auto now = std::chrono::system_clock::now();
    auto time_t = std::chrono::system_clock::to_time_t(now);
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()) % 1000;
    
    std::ostringstream oss;
    oss << std::put_time(std::gmtime(&time_t), "%Y%m%d-%H:%M:%S");
    oss << '.' << std::setfill('0') << std::setw(3) << ms.count();
    
    FIX_DEBUG("Generated FIX timestamp: " << oss.str());
    return oss.str();
}

// ===== FixMessage 類別實作 =====

// 建構函式
FixMessage::FixMessage(char msgType) {
    FIX_DEBUG("Creating FixMessage with MsgType: " << msgType);
    
    setField(BeginString, "FIX.4.2");
    setField(MsgType, std::string(1, msgType));
    setField(MsgSeqNum, std::to_string(g_msgSeqNum.fetch_add(1)));
    setField(SendingTime, getCurrentFixTime());
    
    FIX_DEBUG("FixMessage created with " << fields_.size() << " fields");
}

// 解析 FIX 訊息 (預設驗證 checksum)
FixMessage FixMessage::parse(const std::string& rawMessage) {
    return parseWithValidation(rawMessage, true);
}

// 解析 FIX 訊息但不驗證 checksum (測試用)
FixMessage FixMessage::parseUnsafe(const std::string& rawMessage) {
    return parseWithValidation(rawMessage, false);
}

// 內部解析方法，可控制是否驗證 checksum
FixMessage FixMessage::parseWithValidation(const std::string& rawMessage, bool validateChecksum) {
    FIX_PARSE_DEBUG("Starting to parse FIX message of length: " << rawMessage.length() 
                   << " (validateChecksum=" << (validateChecksum ? "YES" : "NO") << ")");
    
    if (rawMessage.empty()) {
        FIX_PARSE_DEBUG("ERROR: Empty FIX message");
        throw std::runtime_error("Empty FIX message");
    }
    
    FixMessage msg;
    size_t pos = 0;
    int fieldCount = 0;
    
    while (pos < rawMessage.length()) {
        // 尋找等號
        size_t equalPos = rawMessage.find('=', pos);
        if (equalPos == std::string::npos) {
            FIX_PARSE_DEBUG("No more '=' found, ending parse at position: " << pos);
            break;
        }
        
        // 尋找 SOH 分隔符 (可能是 \x01 或 | 用於測試)
        size_t sohPos = rawMessage.find(SOH, equalPos);
        if (sohPos == std::string::npos) {
            // 嘗試尋找 | 分隔符 (測試用)
            sohPos = rawMessage.find('|', equalPos);
            if (sohPos == std::string::npos) {
                sohPos = rawMessage.length();  // 最後一個欄位
                FIX_PARSE_DEBUG("Last field detected (no SOH found)");
            }
        }
        
        // 提取 tag 和 value
        std::string tagStr = rawMessage.substr(pos, equalPos - pos);
        std::string value = rawMessage.substr(equalPos + 1, sohPos - equalPos - 1);
        
        FIX_PARSE_DEBUG("Field #" << ++fieldCount << ": Tag=" << tagStr << ", Value=" << value);
        
        try {
            int tag = std::stoi(tagStr);
            msg.setField(tag, value);
        } catch (const std::exception& e) {
            FIX_PARSE_DEBUG("ERROR: Invalid tag '" << tagStr << "': " << e.what());
            throw std::runtime_error("Invalid tag in FIX message: " + tagStr);
        }
        
        pos = sohPos + 1;  // 移到下一個欄位
    }
    
    FIX_PARSE_DEBUG("Parse completed. Total fields: " << msg.getFieldCount());
    
    // 🎯 關鍵改進：解析後立即驗證 checksum
    if (validateChecksum) {
        FIX_PARSE_DEBUG("Performing checksum validation...");
        
        if (!msg.hasField(CheckSum)) {
            FIX_PARSE_DEBUG("ERROR: Missing CheckSum field");
            throw std::runtime_error("FIX message missing CheckSum field");
        }
        
        if (!msg.validateChecksum()) {
            FIX_PARSE_DEBUG("ERROR: Checksum validation failed");
            throw std::runtime_error("FIX message checksum validation failed");
        }
        
        FIX_PARSE_DEBUG("Checksum validation PASSED");
    } else {
        FIX_PARSE_DEBUG("Skipping checksum validation (unsafe mode)");
    }
    
    return msg;
}

// 序列化為 FIX 字串
std::string FixMessage::serialize() const {
    FIX_SERIALIZE_DEBUG("Starting serialization of " << fields_.size() << " fields");
    
    // 檢查必要欄位
    if (!hasField(BeginString) || !hasField(MsgType)) {
        FIX_SERIALIZE_DEBUG("ERROR: Missing required fields (BeginString or MsgType)");
        throw std::runtime_error("Missing required fields for serialization");
    }
    
    std::ostringstream oss;
    
    // 1. BeginString (8)
    oss << BeginString << "=" << getField(BeginString) << SOH;
    
    // 2. BodyLength (9) - 稍後計算
    std::ostringstream bodyStream;
    
    // 3. MsgType (35)
    bodyStream << MsgType << "=" << getField(MsgType) << SOH;
    
    // 4. 其他欄位（按 tag 排序，除了標準標頭和 CheckSum）
    std::vector<std::pair<int, std::string>> sortedFields;
    for (const auto& [tag, value] : fields_) {
        if (tag != BeginString && tag != BodyLength && tag != MsgType && tag != CheckSum) {
            sortedFields.emplace_back(tag, value);
        }
    }
    
    // 按 tag 排序 (FIX 建議)
    std::sort(sortedFields.begin(), sortedFields.end());
    
    for (const auto& [tag, value] : sortedFields) {
        bodyStream << tag << "=" << value << SOH;
        // FIX_SERIALIZE_DEBUG("Added field: " << tag << "=" << value);
    }
    
    std::string bodyContent = bodyStream.str();
    
    // 計算 BodyLength
    size_t bodyLength = bodyContent.length();
    oss << BodyLength << "=" << bodyLength << SOH;
    
    // 加入 Body 內容
    oss << bodyContent;
    
    // 計算 CheckSum (排除 CheckSum 欄位本身)
    std::string messageWithoutChecksum = oss.str();
    std::string checksum = calculateChecksum(messageWithoutChecksum);
    
    // 加入 CheckSum
    oss << CheckSum << "=" << checksum << SOH;
    
    std::string result = oss.str();
    FIX_SERIALIZE_DEBUG("Serialization completed. Total length: " << result.length());
    
    return result;
}

// 欄位操作
void FixMessage::setField(FieldTag tag, const FieldValue& value) {
    // FIX_DEBUG("Setting field: " << tag << "=" << value);
    fields_[tag] = value;
}

FieldValue FixMessage::getField(FieldTag tag) const {
    auto it = fields_.find(tag);
    if (it != fields_.end()) {
        return it->second;
    }
    return "";  // 欄位不存在時返回空字串
}

std::optional<FieldValue> FixMessage::getFieldOptional(FieldTag tag) const {
    auto it = fields_.find(tag);
    if (it != fields_.end()) {
        return it->second;
    }
    return std::nullopt;
}

bool FixMessage::hasField(FieldTag tag) const {
    return fields_.find(tag) != fields_.end();
}

void FixMessage::removeField(FieldTag tag) {
    FIX_DEBUG("Removing field: " << tag);
    fields_.erase(tag);
}

// ===== 驗證方法 =====

bool FixMessage::isValid() const {
    auto [valid, reason] = validateWithDetails();
    return valid;
}

std::pair<bool, std::string> FixMessage::validateWithDetails() const {
    FIX_VALIDATION_DEBUG("Starting detailed validation");
    
    // 檢查必要欄位
    std::vector<int> requiredFields = {BeginString, BodyLength, MsgType, CheckSum};
    
    for (int tag : requiredFields) {
        if (!hasField(tag) || getField(tag).empty()) {
            std::string error = "Missing required field: " + std::to_string(tag);
            FIX_VALIDATION_DEBUG("VALIDATION FAILED: " << error);
            return {false, error};
        }
    }
    
    // 檢查 BeginString 版本
    std::string beginString = getField(BeginString);
    if (beginString != "FIX.4.2" && beginString != "FIX.4.4" && beginString != "FIX.5.0") {
        std::string error = "Invalid BeginString: " + beginString;
        FIX_VALIDATION_DEBUG("VALIDATION FAILED: " << error);
        return {false, error};
    }
    
    // 檢查 MsgType 是否有效
    std::string msgType = getField(MsgType);
    if (msgType.empty()) {
        std::string error = "Empty MsgType";
        FIX_VALIDATION_DEBUG("VALIDATION FAILED: " << error);
        return {false, error};
    }
    
    // 檢查 BodyLength 是否為數字
    try {
        std::stoi(getField(BodyLength));
    } catch (...) {
        std::string error = "Invalid BodyLength: " + getField(BodyLength);
        FIX_VALIDATION_DEBUG("VALIDATION FAILED: " << error);
        return {false, error};
    }
    
    // 驗證 Checksum
    if (!validateChecksum()) {
        std::string error = "Invalid checksum";
        FIX_VALIDATION_DEBUG("VALIDATION FAILED: " << error);
        return {false, error};
    }
    
    FIX_VALIDATION_DEBUG("Validation PASSED");
    return {true, "Valid"};
}

bool FixMessage::validateChecksum() const {
    FIX_CHECKSUM_DEBUG("Validating checksum");
    
    if (!hasField(CheckSum)) {
        FIX_CHECKSUM_DEBUG("No checksum field found");
        return false;
    }
    
    try {
        // 重新序列化，計算 checksum
        std::string currentChecksum = getField(CheckSum);
        
        // 建構不含 checksum 的訊息
        std::string messageWithoutChecksum = buildMessageWithoutChecksum();
        FIX_CHECKSUM_DEBUG("Message without checksum: " << messageWithoutChecksum);

        std::string calculatedChecksum = calculateChecksum(messageWithoutChecksum);
        
        bool valid = (currentChecksum == calculatedChecksum);
        FIX_CHECKSUM_DEBUG("Checksum validation: current=" << currentChecksum 
                           << ", calculated=" << calculatedChecksum 
                           << ", valid=" << (valid ? "YES" : "NO"));
        
        return valid;
    } catch (...) {
        FIX_CHECKSUM_DEBUG("Exception during checksum validation");
        return false;
    }
}

// ===== 便利方法 =====

std::optional<char> FixMessage::getMsgType() const {
    auto opt = getFieldOptional(MsgType);
    if (opt && !opt->empty()) {
        return opt->at(0);
    }
    return std::nullopt;
}

std::optional<std::string> FixMessage::getSenderCompID() const {
    return getFieldOptional(SenderCompID);
}

std::optional<std::string> FixMessage::getTargetCompID() const {
    return getFieldOptional(TargetCompID);
}

std::optional<int> FixMessage::getMsgSeqNum() const {
    auto opt = getFieldOptional(MsgSeqNum);
    if (opt) {
        try {
            return std::stoi(*opt);
        } catch (...) {
            return std::nullopt;
        }
    }
    return std::nullopt;
}

bool FixMessage::isAdminMessage() const {
    auto msgType = getMsgType();
    if (!msgType) return false;
    
    // 管理訊息：Heartbeat, TestRequest, Logon, Logout 等
    return *msgType == Heartbeat || *msgType == TestRequest || 
           *msgType == Logon || *msgType == Logout;
}

bool FixMessage::isApplicationMessage() const {
    auto msgType = getMsgType();
    if (!msgType) return false;
    
    // 應用訊息：訂單相關
    return *msgType == NewOrderSingle || *msgType == ExecutionReport || 
           *msgType == OrderCancelRequest;
}

// ===== 工具方法 =====

std::string FixMessage::toString() const {
    std::ostringstream oss;
    oss << "FixMessage[";
    
    // 顯示關鍵欄位
    if (auto msgType = getMsgType()) {
        oss << "MsgType=" << *msgType;
    }
    
    if (auto sender = getSenderCompID()) {
        oss << ", Sender=" << *sender;
    }
    
    if (auto target = getTargetCompID()) {
        oss << ", Target=" << *target;
    }
    
    if (auto seqNum = getMsgSeqNum()) {
        oss << ", SeqNum=" << *seqNum;
    }
    
    oss << ", Fields=" << fields_.size();
    
    // 顯示驗證狀態
    auto [valid, reason] = validateWithDetails();
    oss << ", Valid=" << (valid ? "YES" : "NO");
    if (!valid) {
        oss << " (Reason: " << reason << ")";
    }
    
    oss << "]";
    return oss.str();
}

// ===== 私有輔助方法 =====


std::string FixMessage::calculateChecksum(const std::string& messageBody) const {
    // FIX_CHECKSUM_DEBUG("Calculating checksum for message length: " << messageBody.length() << " characters : "  << messageBody  );
    
    int sum = 0;
    for (char c : messageBody) {
        // std::cout << "Character: " << c << " (ASCII: " << static_cast<int>(c) << ")" << std::endl;
        sum += static_cast<unsigned char>(c);
    }
    int checksum = sum % 256;
    
    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(3) << checksum;
    
    FIX_CHECKSUM_DEBUG("Calculated checksum: " << checksum << " -> " << oss.str());
    return oss.str();
}

std::string FixMessage::getCurrentFixTimestamp() const {
    return getCurrentFixTime();
}

bool FixMessage::validateRequiredFields() const {
    std::vector<int> requiredFields = {BeginString, BodyLength, MsgType, CheckSum};
    
    for (int tag : requiredFields) {
        if (!hasField(tag) || getField(tag).empty()) {
            return false;
        }
    }
    return true;
}

std::string FixMessage::buildBodyContent() const {
    FIX_SERIALIZE_DEBUG("Building body content");
    
    std::ostringstream oss;
    
    // 按照 FIX 標準順序：MsgType 先出現，然後是其他欄位
    // 1. 先加入 MsgType (35)
    if (hasField(MsgType)) {
        oss << MsgType << "=" << getField(MsgType) << SOH;
        // FIX_CHECKSUM_DEBUG("Added " << MsgType << " field: " << getField(MsgType));
    }
    
    // 2. 加入其他欄位（排除標頭和 CheckSum）
    std::vector<std::pair<int, std::string>> sortedFields;
    for (const auto& [tag, value] : fields_) {
        if (tag != BeginString && tag != BodyLength && tag != MsgType && tag != CheckSum) {
            sortedFields.emplace_back(tag, value);
        }
    }
    
    // 按 tag 順序排序
    std::sort(sortedFields.begin(), sortedFields.end());
    
    for (const auto& [tag, value] : sortedFields) {
        oss << tag << "=" << value << SOH;
        // FIX_CHECKSUM_DEBUG("Added " << tag << " field: " << value);
    }
    
    return oss.str(); // 保持 SOH，讓 BodyLength 正確
}

std::string FixMessage::buildMessageWithoutChecksum() const {
    FIX_SERIALIZE_DEBUG("Building message without checksum");
    
    std::ostringstream oss;
    
    // BeginString
    oss << BeginString << "=" << getField(BeginString) << SOH;
    // FIX_CHECKSUM_DEBUG("Added "<< BeginString <<" field: " << getField(BeginString));
    
    // Body Content
    std::string bodyContent = buildBodyContent();
    
    // BodyLength
    oss << BodyLength << "=" << bodyContent.length() << SOH;
    // FIX_CHECKSUM_DEBUG("Added "<< BodyLength <<" field: " << bodyContent.length());
    
    // Body
    oss << bodyContent;
    
    return oss.str();
}

} // namespace protocol
} // namespace mts