#include <gtest/gtest.h>
#include "../src/core/Order.h"
#include <stdexcept>
#include <thread>
#include <chrono>

using namespace mts::core;

class OrderTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 測試資料準備
        orderId = 12345;
        clientId = "CLIENT001";
        symbol = "AAPL";
        price = 150.50;
        quantity = 100;
    }
    
    void TearDown() override {
        // 清理資源 (如果需要)
    }
    
    // 測試資料
    OrderID orderId;
    ClientID clientId;
    Symbol symbol;
    Price price;
    Quantity quantity;
};

// 測試基本建構函式
TEST_F(OrderTest, BasicConstructor) {
    Order order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    EXPECT_EQ(order.getOrderId(), orderId);
    EXPECT_EQ(order.getClientId(), clientId);
    EXPECT_EQ(order.getSymbol(), symbol);
    EXPECT_EQ(order.getSide(), Side::Buy);
    EXPECT_EQ(order.getOrderType(), OrderType::Limit);
    EXPECT_EQ(order.getPrice(), price);
    EXPECT_EQ(order.getQuantity(), quantity);
    EXPECT_EQ(order.getRemainingQuantity(), quantity);
    EXPECT_EQ(order.getFilledQuantity(), 0);
    EXPECT_EQ(order.getStatus(), OrderStatus::New);
    EXPECT_EQ(order.getTimeInForce(), TimeInForce::Day);
}

// 測試市價單建構函式
TEST_F(OrderTest, MarketOrderConstructor) {
    Order order(orderId, clientId, symbol, Side::Sell, quantity);
    
    EXPECT_EQ(order.getOrderType(), OrderType::Market);
    EXPECT_EQ(order.getPrice(), 0.0);
    EXPECT_TRUE(order.isMarketOrder());
    EXPECT_FALSE(order.isLimitOrder());
}

// 測試訂單狀態檢查
TEST_F(OrderTest, OrderStatusChecks) {
    Order order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    // 初始狀態
    EXPECT_TRUE(order.isActive());
    EXPECT_FALSE(order.isFilled());
    EXPECT_FALSE(order.isCancelled());
    EXPECT_FALSE(order.isRejected());
    EXPECT_TRUE(order.isBuyOrder());
    EXPECT_FALSE(order.isSellOrder());
    
    // 測試狀態變更
    order.setStatus(OrderStatus::Filled);
    EXPECT_TRUE(order.isFilled());
    EXPECT_FALSE(order.isActive());
}

// 測試部分成交功能
TEST_F(OrderTest, PartialFill) {
    Order order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    // 部分成交 30 股
    EXPECT_TRUE(order.canFill(30));
    order.fillQuantity(30);
    
    EXPECT_EQ(order.getRemainingQuantity(), 70);
    EXPECT_EQ(order.getFilledQuantity(), 30);
    EXPECT_EQ(order.getStatus(), OrderStatus::PartiallyFilled);
    EXPECT_TRUE(order.isActive());
    
    // 再成交 70 股 (完全成交)
    EXPECT_TRUE(order.canFill(70));
    order.fillQuantity(70);
    
    EXPECT_EQ(order.getRemainingQuantity(), 0);
    EXPECT_EQ(order.getFilledQuantity(), 100);
    EXPECT_EQ(order.getStatus(), OrderStatus::Filled);
    EXPECT_FALSE(order.isActive());
    EXPECT_TRUE(order.isFilled());
}

// 測試成交數量超出限制
TEST_F(OrderTest, OverFillException) {
    Order order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    // 嘗試成交超過剩餘數量
    EXPECT_THROW(order.fillQuantity(101), std::invalid_argument);
    
    // 部分成交後再嘗試超量成交
    order.fillQuantity(50);
    EXPECT_THROW(order.fillQuantity(51), std::invalid_argument);
}

// 測試無效訂單建構
TEST_F(OrderTest, InvalidOrderConstruction) {
    
    // 數量為 0
    EXPECT_THROW(Order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, 0),
                 std::invalid_argument);
    
    // 限價單價格為 0 或負數
    EXPECT_THROW(Order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, 0.0, quantity),
                 std::invalid_argument);
    
    EXPECT_THROW(Order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, -10.0, quantity),
                 std::invalid_argument);
    
    // 空白交易標的
    EXPECT_THROW(Order(orderId, clientId, "", Side::Buy, OrderType::Limit, price, quantity),
                 std::invalid_argument);
}

// 測試訂單比較
TEST_F(OrderTest, OrderComparison) {
    Order order1(1, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    Order order2(2, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    Order order3(1, "CLIENT002", "TSLA", Side::Sell, OrderType::Market, 0, 50);
    
    // 相同 OrderID 的訂單相等
    EXPECT_EQ(order1, order3);  // 只比較 OrderID
    EXPECT_NE(order1, order2);  // 不同 OrderID
}

// 測試價格比較器
TEST_F(OrderTest, PriceComparator) {
    Order::PriceComparator comp;
    
    // 買單測試 (價格高的優先)
    Order buyHigh(1, clientId, symbol, Side::Buy, OrderType::Limit, 151.0, quantity);
    Order buyLow(2, clientId, symbol, Side::Buy, OrderType::Limit, 150.0, quantity);
    
    EXPECT_TRUE(comp(buyHigh, buyLow));   // 高價買單 > 低價買單
    EXPECT_FALSE(comp(buyLow, buyHigh));  // 低價買單 < 高價買單
    
    // 賣單測試 (價格低的優先)
    Order sellHigh(3, clientId, symbol, Side::Sell, OrderType::Limit, 151.0, quantity);
    Order sellLow(4, clientId, symbol, Side::Sell, OrderType::Limit, 150.0, quantity);
    
    EXPECT_TRUE(comp(sellLow, sellHigh));   // 低價賣單 > 高價賣單
    EXPECT_FALSE(comp(sellHigh, sellLow));  // 高價賣單 < 低價賣單
}

// 測試時間比較器
TEST_F(OrderTest, TimeComparator) {
    Order::TimeComparator comp;
    
    Order order1(1, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    // 等待一小段時間確保時間戳不同
    std::this_thread::sleep_for(std::chrono::microseconds(1));
    
    Order order2(2, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    // 較早的訂單應該優先
    EXPECT_TRUE(comp(order1, order2));
    EXPECT_FALSE(comp(order2, order1));
}

// 測試訂單驗證
TEST_F(OrderTest, OrderValidation) {
    // 有效的限價單
    Order validLimit(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    EXPECT_TRUE(validLimit.isValid());
    
    // 有效的市價單
    Order validMarket(orderId, clientId, symbol, Side::Sell, quantity);
    EXPECT_TRUE(validMarket.isValid());
    
    // 無效訂單測試需要手動創建 (繞過建構函式檢查)
    Order invalid;
    EXPECT_FALSE(invalid.isValid());  // 預設建構的訂單無效
}

// 測試字串轉換功能
TEST_F(OrderTest, StringConversion) {
    Order order(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    
    std::string str = order.toString();
    
    // 檢查字串包含關鍵資訊
    EXPECT_TRUE(str.find("12345") != std::string::npos);      // OrderID
    EXPECT_TRUE(str.find("CLIENT001") != std::string::npos);  // ClientID
    EXPECT_TRUE(str.find("AAPL") != std::string::npos);       // Symbol
    EXPECT_TRUE(str.find("BUY") != std::string::npos);        // Side
    EXPECT_TRUE(str.find("LIMIT") != std::string::npos);      // OrderType
    EXPECT_TRUE(str.find("150.50") != std::string::npos);     // Price
}

// 測試輔助函式
TEST_F(OrderTest, HelperFunctions) {
    // 測試 Side 轉換
    EXPECT_EQ(sideToString(Side::Buy), "BUY");
    EXPECT_EQ(sideToString(Side::Sell), "SELL");
    EXPECT_EQ(stringToSide("BUY"), Side::Buy);
    EXPECT_EQ(stringToSide("1"), Side::Buy);
    
    // 測試 OrderType 轉換
    EXPECT_EQ(orderTypeToString(OrderType::Market), "MARKET");
    EXPECT_EQ(orderTypeToString(OrderType::Limit), "LIMIT");
    EXPECT_EQ(stringToOrderType("MARKET"), OrderType::Market);
    EXPECT_EQ(stringToOrderType("1"), OrderType::Market);
    
    // 測試無效字串轉換
    EXPECT_THROW(stringToSide("INVALID"), std::invalid_argument);
    EXPECT_THROW(stringToOrderType("INVALID"), std::invalid_argument);
}

// 測試複製和移動語意
TEST_F(OrderTest, CopyAndMoveSemantics) {
    Order original(orderId, clientId, symbol, Side::Buy, OrderType::Limit, price, quantity);
    original.fillQuantity(20);
    
    // 複製建構
    Order copied(original);
    EXPECT_EQ(copied.getOrderId(), original.getOrderId());
    EXPECT_EQ(copied.getFilledQuantity(), 20);
    EXPECT_EQ(copied.getStatus(), OrderStatus::PartiallyFilled);
    
    // 複製賦值
    Order assigned;
    assigned = original;
    EXPECT_EQ(assigned.getOrderId(), original.getOrderId());
    EXPECT_EQ(assigned.getRemainingQuantity(), 80);
    
    // 移動建構
    Order moved(std::move(original));
    EXPECT_EQ(moved.getOrderId(), orderId);
    EXPECT_EQ(moved.getFilledQuantity(), 20);
}

// 效能測試 (簡單版本)
TEST_F(OrderTest, PerformanceBasic) {
    auto start = std::chrono::high_resolution_clock::now();
    
    // 建立大量訂單
    constexpr int ORDER_COUNT = 10000;
    std::vector<Order> orders;
    orders.reserve(ORDER_COUNT);
    
    for (int i = 0; i < ORDER_COUNT; ++i) {
        orders.emplace_back(i, clientId, symbol, Side::Buy, OrderType::Limit, 
                           price + i * 0.01, quantity);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    // 平均每個訂單建立時間應小於 10 微秒
    double avgTimePerOrder = static_cast<double>(duration.count()) / ORDER_COUNT;
    EXPECT_LT(avgTimePerOrder, 10.0) << "Order creation too slow: " << avgTimePerOrder << "μs per order";
    
    std::cout << "Created " << ORDER_COUNT << " orders in " << duration.count() 
              << "μs (avg: " << avgTimePerOrder << "μs per order)" << std::endl;
}

// 主函式 (如果直接運行此檔案)
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}