#include <gtest/gtest.h>
#include "../src/protocol/fix_message.h"
#include <stdexcept>
#include <string>
#include <iostream>

using namespace mts::protocol;

class FixMessageTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 測試資料準備
        validFixString = "8=FIX.4.2|9=178|35=D|49=CLIENT001|56=SERVER|34=1|52=20250101-12:00:00|11=ORDER123|55=AAPL|54=1|38=100|40=2|44=150.50|59=0|10=156|";
        invalidChecksumFix = "8=FIX.4.2|9=178|35=D|49=CLIENT001|56=SERVER|34=1|52=20250101-12:00:00|11=ORDER123|55=AAPL|54=1|38=100|40=2|44=150.50|59=0|10=999|";
        malformedFix = "8=FIX.4.2|9=invalid|35=D|49=CLIENT001";
    }

    void TearDown() override {
        // 清理資源
    }

    // 測試資料
    std::string validFixString;
    std::string invalidChecksumFix;
    std::string malformedFix;
};

// ===== 基本建構測試 =====

TEST_F(FixMessageTest, DefaultConstructor) {
    FixMessage msg;
    
    // 預設建構的訊息應該是空的
    EXPECT_EQ(msg.getFieldCount(), 0);
    EXPECT_FALSE(msg.hasField(FixMessage::MsgType));
    EXPECT_FALSE(msg.isValid());
}

TEST_F(FixMessageTest, MsgTypeConstructor) {
    FixMessage msg('D');  // NewOrderSingle
    
    // 應該自動設定基本欄位
    EXPECT_TRUE(msg.hasField(FixMessage::BeginString));
    EXPECT_TRUE(msg.hasField(FixMessage::MsgType));
    EXPECT_TRUE(msg.hasField(FixMessage::MsgSeqNum));
    EXPECT_TRUE(msg.hasField(FixMessage::SendingTime));
    
    EXPECT_EQ(msg.getField(FixMessage::BeginString), "FIX.4.2");
    EXPECT_EQ(msg.getField(FixMessage::MsgType), "D");
    
    // 序號應該遞增
    FixMessage msg2('8');  // ExecutionReport
    int seqNum1 = std::stoi(msg.getField(FixMessage::MsgSeqNum));
    int seqNum2 = std::stoi(msg2.getField(FixMessage::MsgSeqNum));
    EXPECT_EQ(seqNum2, seqNum1 + 1);
}


// ===== 欄位操作測試 =====
TEST_F(FixMessageTest, FieldOperations) {
    FixMessage msg;
    
    // 設定欄位
    msg.setField(11, "ORDER123");  // ClOrdID
    msg.setField(55, "AAPL");      // Symbol
    msg.setField(44, "150.50");    // Price
    
    // 檢查欄位存在
    EXPECT_TRUE(msg.hasField(11));
    EXPECT_TRUE(msg.hasField(55));
    EXPECT_TRUE(msg.hasField(44));
    EXPECT_FALSE(msg.hasField(999));  // 不存在的欄位
    
    // 檢查欄位值
    EXPECT_EQ(msg.getField(11), "ORDER123");
    EXPECT_EQ(msg.getField(55), "AAPL");
    EXPECT_EQ(msg.getField(44), "150.50");
    EXPECT_EQ(msg.getField(999), "");  // 不存在欄位返回空字串
    
    // 欄位數量
    EXPECT_EQ(msg.getFieldCount(), 3);
    
    // 覆蓋欄位
    msg.setField(44, "151.00");
    EXPECT_EQ(msg.getField(44), "151.00");
    EXPECT_EQ(msg.getFieldCount(), 3);  // 數量不變
}


// ===== 解析測試 =====

TEST_F(FixMessageTest, BasicParsing) {
    // 注意：這裡使用正確的 checksum

    std::string testFix = "8=FIX.4.2|9=63|35=A|49=CLIENT|56=SERVER|34=1|52=20250117-12:00:00|98=0|108=30|10=187|";
    // 9=61
    // 35=D       : 5 
    // 49=CLIENT  : 10
    // 56=SERVER  : 10 
    // 34=1       : 5
    // 11=ORDER123: 12 
    // 55=AAPL    : 8
    // 54=1       : 5 
    // 38=100     : 7
    //           += 62 
    
    FixMessage msg = FixMessage::parse(testFix);
    
    // EXPECT_TRUE(msg.isValid());
    EXPECT_EQ(msg.getField(8), "FIX.4.2");      // BeginString
    EXPECT_EQ(msg.getField(35), "A");           // MsgType
    EXPECT_EQ(msg.getField(49), "CLIENT");      // SenderCompID
    EXPECT_EQ(msg.getField(56), "SERVER");      // TargetCompID
    // EXPECT_EQ(msg.getField(11), "ORDER123");    // ClOrdID
    EXPECT_EQ(msg.getField(34), "1");    
    EXPECT_EQ(msg.getField(52), "20250117-12:00:00");    
    EXPECT_EQ(msg.getField(98), "0");   
    EXPECT_EQ(msg.getField(108), "30");  
    // EXPECT_EQ(msg.getField(55), "AAPL");        // Symbol
    // EXPECT_EQ(msg.getField(54), "1");           // Side
    // EXPECT_EQ(msg.getField(38), "100");         // OrderQty
}

TEST_F(FixMessageTest, ParseUnsafe) {
    // 使用錯誤的 checksum 但跳過驗證
    FixMessage msg = FixMessage::parseUnsafe(invalidChecksumFix);
    
    // 應該能解析成功（雖然 checksum 錯誤）
    EXPECT_EQ(msg.getField(35), "D");
    EXPECT_EQ(msg.getField(11), "ORDER123");
    
    // 但驗證應該失敗
    EXPECT_FALSE(msg.validateChecksum());
}

TEST_F(FixMessageTest, ParseEmptyMessage) {
    EXPECT_THROW(FixMessage::parse(""), std::runtime_error);
    EXPECT_THROW(FixMessage::parseUnsafe(""), std::runtime_error);
}

TEST_F(FixMessageTest, ParseInvalidTag) {
    std::string invalidTagFix = "8=FIX.4.2|abc=invalid|35=D|10=123|";
    
    EXPECT_THROW(FixMessage::parse(invalidTagFix), std::runtime_error);
}

// ===== 序列化測試 =====

TEST_F(FixMessageTest, BasicSerialization) {
    FixMessage msg('D');
    msg.setField(49, "CLIENT");      // SenderCompID
    msg.setField(56, "SERVER");      // TargetCompID
    msg.setField(11, "ORDER123");    // ClOrdID
    msg.setField(55, "AAPL");        // Symbol
    
    std::string serialized = msg.serialize();
    
    // 檢查基本格式
    EXPECT_TRUE(serialized.find("8=FIX.4.2") != std::string::npos);
    EXPECT_TRUE(serialized.find("35=D") != std::string::npos);
    EXPECT_TRUE(serialized.find("49=CLIENT") != std::string::npos);
    EXPECT_TRUE(serialized.find("11=ORDER123") != std::string::npos);
    
    // 檢查必要欄位存在
    EXPECT_TRUE(serialized.find("9=") != std::string::npos);  // BodyLength
    EXPECT_TRUE(serialized.find("10=") != std::string::npos); // CheckSum
    
    // 應該以 SOH 結尾（在測試中用 | 代表）
    // 實際上序列化會用 \x01
}

TEST_F(FixMessageTest, SerializationOrder) {
    FixMessage msg('D');
    msg.setField(55, "AAPL");        // Symbol (後加)
    msg.setField(11, "ORDER123");    // ClOrdID (先加)
    msg.setField(38, "100");         // OrderQty (中間加)
    
    std::string serialized = msg.serialize();
    
    // 欄位應該按 tag 順序排列（除了標準標頭）
    size_t pos11 = serialized.find("11=ORDER123");
    size_t pos38 = serialized.find("38=100");
    size_t pos55 = serialized.find("55=AAPL");
    
    EXPECT_NE(pos11, std::string::npos);
    EXPECT_NE(pos38, std::string::npos);
    EXPECT_NE(pos55, std::string::npos);
    EXPECT_LT(pos11, pos38);  // 11 應該在 38 之前
    EXPECT_LT(pos38, pos55);  // 38 應該在 55 之前
}

TEST_F(FixMessageTest, SerializeIncompleteMessage) {
    FixMessage msg;  // 沒有必要欄位
    
    EXPECT_THROW(msg.serialize(), std::runtime_error);
}

// ===== 驗證測試 =====

TEST_F(FixMessageTest, ValidationRequiredFields) {
    FixMessage msg;
    
    // 空訊息無效
    EXPECT_FALSE(msg.isValid());
    
    auto [valid, reason] = msg.validateWithDetails();
    EXPECT_FALSE(valid);
    EXPECT_TRUE(reason.find("Missing required field") != std::string::npos);
    
    // 逐步加入必要欄位
    msg.setField(FixMessage::BeginString, "FIX.4.2");
    EXPECT_FALSE(msg.isValid());
    
    msg.setField(FixMessage::MsgType, "D");
    EXPECT_FALSE(msg.isValid());
    
    msg.setField(FixMessage::BodyLength, "50");
    EXPECT_FALSE(msg.isValid());
    
    msg.setField(FixMessage::CheckSum, "123");
    // 現在有所有必要欄位，但 checksum 可能不正確
    auto [valid2, reason2] = msg.validateWithDetails();
    // 可能因為 checksum 驗證失敗而無效，但不是因為缺少欄位
    EXPECT_TRUE(reason2.find("Missing required field") == std::string::npos);
}

TEST_F(FixMessageTest, ValidationBeginString) {
    FixMessage msg('D');
    msg.setField(FixMessage::BodyLength, "50");
    msg.setField(FixMessage::CheckSum, "123");
    
    // 有效的 BeginString
    msg.setField(FixMessage::BeginString, "FIX.4.2");
    auto [valid1, reason1] = msg.validateWithDetails();
    EXPECT_TRUE(reason1.find("Invalid BeginString") == std::string::npos);
    
    // 無效的 BeginString
    msg.setField(FixMessage::BeginString, "INVALID");
    auto [valid2, reason2] = msg.validateWithDetails();
    EXPECT_FALSE(valid2);
    EXPECT_TRUE(reason2.find("Invalid BeginString") != std::string::npos);
}

// ===== Checksum 測試 =====

TEST_F(FixMessageTest, ChecksumValidation) {
    // 建立一個簡單的訊息並序列化
    FixMessage msg('0');  // Heartbeat
    std::string serialized = msg.serialize();
    
    // 解析回來，checksum 應該有效
    FixMessage parsed = FixMessage::parse(serialized);
    EXPECT_TRUE(parsed.validateChecksum());
    EXPECT_TRUE(parsed.isValid());
    
    // 手動破壞 checksum
    parsed.setField(FixMessage::CheckSum, "000");
    EXPECT_FALSE(parsed.validateChecksum());
    EXPECT_FALSE(parsed.isValid());
}

// ===== 往返測試 (Round-trip) =====

TEST_F(FixMessageTest, RoundTripSerialization) {
    // 建立原始訊息
    FixMessage original('D');
    original.setField(49, "CLIENT001");
    original.setField(56, "SERVER001");
    original.setField(11, "ORDER123");
    original.setField(55, "AAPL");
    original.setField(54, "1");
    original.setField(38, "100");
    original.setField(40, "2");
    original.setField(44, "150.50");
    
    // 序列化
    std::string serialized = original.serialize();
    
    // 解析回來
    FixMessage roundTrip = FixMessage::parse(serialized);
    
    // 檢查關鍵欄位是否一致
    original.setField(FixMessage::BodyLength, roundTrip.getField(FixMessage::BodyLength));
    original.setField(FixMessage::CheckSum, roundTrip.getField(FixMessage::CheckSum));
    EXPECT_EQ(roundTrip.getField(35), "D");
    EXPECT_EQ(roundTrip.getField(49), "CLIENT001");
    EXPECT_EQ(roundTrip.getField(56), "SERVER001");
    EXPECT_EQ(roundTrip.getField(11), "ORDER123");
    EXPECT_EQ(roundTrip.getField(55), "AAPL");
    EXPECT_EQ(roundTrip.getField(54), "1");
    EXPECT_EQ(roundTrip.getField(38), "100");
    EXPECT_EQ(roundTrip.getField(40), "2");
    EXPECT_EQ(roundTrip.getField(44), "150.50");
    
    // 兩個訊息都應該有效
    EXPECT_TRUE(original.isValid());
    EXPECT_TRUE(roundTrip.isValid());
}

// ===== toString 測試 =====

TEST_F(FixMessageTest, ToStringOutput) {
    FixMessage msg('D');
    msg.setField(49, "CLIENT");
    msg.setField(56, "SERVER");
    msg.setField(11, "ORDER123");
    
    std::string output = msg.toString();
    
    // 檢查輸出包含關鍵資訊
    EXPECT_TRUE(output.find("FixMessage[") != std::string::npos);
    EXPECT_TRUE(output.find("MsgType=D") != std::string::npos);
    EXPECT_TRUE(output.find("Sender=CLIENT") != std::string::npos);
    EXPECT_TRUE(output.find("Target=SERVER") != std::string::npos);
    EXPECT_TRUE(output.find("Fields=") != std::string::npos);
}

// ===== 效能測試 =====

TEST_F(FixMessageTest, PerformanceBasic) {
    const int MESSAGE_COUNT = 10000;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < MESSAGE_COUNT; ++i) {
        FixMessage msg('D');
        msg.setField(11, "ORDER" + std::to_string(i));
        msg.setField(55, "AAPL");
        msg.setField(54, "1");
        msg.setField(38, "100");
        
        FixMessage parsed = FixMessage::parseUnsafe(msg.serialize());
        
        ASSERT_TRUE(parsed.isValid());
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTimePerMessage = static_cast<double>(duration.count()) / MESSAGE_COUNT;
    
    std::cout << "Processed " << MESSAGE_COUNT << " messages in " << duration.count() 
              << "μs (avg: " << avgTimePerMessage << "μs per message)" << std::endl;
    
    // 效能目標：平均每個訊息處理時間應小於 100 微秒
    EXPECT_LT(avgTimePerMessage, 100.0) << "Message processing too slow: " << avgTimePerMessage << "μs per message";
}

// ===== 錯誤處理測試 =====

TEST_F(FixMessageTest, ErrorHandling) {
    // 測試各種異常情況
    
    // 1. 空訊息
    EXPECT_THROW(FixMessage::parse(""), std::runtime_error);
    
    // 2. 無效的標籤
    EXPECT_THROW(FixMessage::parse("abc=def|"), std::runtime_error);
    
    // 3. checksum 驗證失敗（但 parseUnsafe 應該成功）
    std::string badChecksum = "8=FIX.4.2|9=10|35=0|10=999|";
    EXPECT_THROW(FixMessage::parse(badChecksum), std::runtime_error);
    EXPECT_NO_THROW(FixMessage::parseUnsafe(badChecksum));
    
    // 4. 序列化不完整的訊息
    FixMessage incomplete;
    incomplete.setField(35, "D");  // 只有 MsgType
    EXPECT_THROW(incomplete.serialize(), std::runtime_error);
}

// ===== 邊界測試 =====

TEST_F(FixMessageTest, BoundaryConditions) {
    FixMessage msg('D');
    
    // 測試極長的欄位值
    std::string longValue(1000, 'A');
    msg.setField(58, longValue);  // Text field
    
    EXPECT_EQ(msg.getField(58), longValue);
    
    // 序列化和解析應該仍然工作
    std::string serialized = msg.serialize();
    FixMessage parsed = FixMessage::parse(serialized);
    EXPECT_EQ(parsed.getField(58), longValue);
    
    // 測試空值
    msg.setField(100, "");
    EXPECT_EQ(msg.getField(100), "");
    EXPECT_TRUE(msg.hasField(100));
    
    // 測試數字邊界
    msg.setField(999999, "test");  // 大的 tag 編號
    EXPECT_EQ(msg.getField(999999), "test");
}

// 主函式
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "=== FIX Message Unit Tests ===" << std::endl;
    std::cout << "Testing FIX protocol parsing, serialization, and validation" << std::endl;
    std::cout << "=======================================" << std::endl;
    
    return RUN_ALL_TESTS();
}