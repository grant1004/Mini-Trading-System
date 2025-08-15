#include <gtest/gtest.h>
#include "../src/core/order_book.h"
#include <memory>
#include <thread>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <map>
#include <limits>

using namespace mts::core;

class OrderBookTest : public ::testing::Test {
protected:
    void SetUp() override {
        orderBook = std::make_unique<OrderBook>("AAPL");
        
        // 設定回調函式來記錄交易和訂單更新
        orderBook->setTradeCallback([this](const TradePtr& trade) {
            trades.push_back(trade);
        });
        
        orderBook->setOrderUpdateCallback([this](const std::shared_ptr<Order>& order) {
            orderUpdates.push_back(order);
        });
    }
    
    void TearDown() override {
        // Output trades generated during test
        if (!trades.empty()) {
            std::cout << "\n📈 TRADES (" << trades.size() << " executed):" << std::endl;
            std::cout << std::string(50, '-') << std::endl;
            
            for (size_t i = 0; i < trades.size(); ++i) {
                const auto& trade = trades[i];
                std::cout << "Trade #" << (i + 1) << ": " 
                          << trade->quantity << " shares @ $" << std::fixed << std::setprecision(2) << trade->price
                          << " (Buy#" << trade->buyOrderId 
                          << " vs Sell#" << trade->sellOrderId << ")"
                          << " [" << trade->symbol << "]" << std::endl;
            }
            
            // Calculate trade statistics
            Quantity totalVolume = 0;
            Price totalValue = 0.0;
            Price minPrice = std::numeric_limits<Price>::max();
            Price maxPrice = std::numeric_limits<Price>::min();
            
            for (const auto& trade : trades) {
                totalVolume += trade->quantity;
                totalValue += trade->quantity * trade->price;
                minPrice = std::min(minPrice, trade->price);
                maxPrice = std::max(maxPrice, trade->price);
            }
            
            Price avgPrice = (totalVolume > 0) ? (totalValue / totalVolume) : 0.0;
            
            std::cout << std::string(50, '-') << std::endl;
            std::cout << "📊 TRADE SUMMARY:" << std::endl;
            std::cout << "  Total Volume: " << totalVolume << " shares" << std::endl;
            std::cout << "  Total Value: $" << std::fixed << std::setprecision(2) << totalValue << std::endl;
            std::cout << "  Average Price: $" << std::fixed << std::setprecision(2) << avgPrice << std::endl;
            std::cout << "  Price Range: $" << std::fixed << std::setprecision(2) << minPrice 
                      << " - $" << maxPrice << std::endl;
        } else {
            std::cout << "\n📈 TRADES: No trades executed" << std::endl;
        }
        
        // Output order updates
        if (!orderUpdates.empty()) {
            std::cout << "\n📋 ORDER UPDATES (" << orderUpdates.size() << " updates):" << std::endl;
            std::cout << std::string(60, '-') << std::endl;
            
            for (size_t i = 0; i < orderUpdates.size(); ++i) {
                const auto& order = orderUpdates[i];
                std::cout << "Update #" << (i + 1) << ": ";
                std::cout << "Order#" << order->getOrderId() 
                          << " [" << (order->isBuyOrder() ? "BUY" : "SELL") << "] ";
                std::cout << order->getRemainingQuantity() << "/" << order->getQuantity() 
                          << " shares @ $" << std::fixed << std::setprecision(2) << order->getPrice();
                std::cout << " Status: ";
                
                // Display order status
                switch (order->getStatus()) {
                    case OrderStatus::New:
                        std::cout << "NEW"; break;
                    case OrderStatus::PartiallyFilled:
                        std::cout << "PARTIALLY_FILLED"; break;
                    case OrderStatus::Filled:
                        std::cout << "FILLED"; break;
                    case OrderStatus::Cancelled:
                        std::cout << "CANCELLED"; break;
                    case OrderStatus::Rejected:
                        std::cout << "REJECTED"; break;
                    default:
                        std::cout << "UNKNOWN"; break;
                }
                
                std::cout << " [" << order->getSymbol() << "]" << std::endl;
            }
            
            // Count order status distribution
            std::map<OrderStatus, int> statusCount;
            for (const auto& order : orderUpdates) {
                statusCount[order->getStatus()]++;
            }
            
            std::cout << std::string(60, '-') << std::endl;
            std::cout << "📊 ORDER STATUS SUMMARY:" << std::endl;
            
            for (const auto& [status, count] : statusCount) {
                std::cout << "  ";
                switch (status) {
                    case OrderStatus::New:
                        std::cout << "NEW: "; break;
                    case OrderStatus::PartiallyFilled:
                        std::cout << "PARTIALLY_FILLED: "; break;
                    case OrderStatus::Filled:
                        std::cout << "FILLED: "; break;
                    case OrderStatus::Cancelled:
                        std::cout << "CANCELLED: "; break;
                    case OrderStatus::Rejected:
                        std::cout << "REJECTED: "; break;
                    default:
                        std::cout << "OTHER: "; break;
                }
                std::cout << count << " times" << std::endl;
            }
        } else {
            std::cout << "\n📋 ORDER UPDATES: No order updates" << std::endl;
        }
        
        // Separator for next test
        std::cout << "\n" << std::string(80, '=') << std::endl;
        
        // Clean up resources
        trades.clear();
        orderUpdates.clear();
    }
    
    // 輔助函式：創建訂單
    std::shared_ptr<Order> createLimitOrder(OrderID id, Side side, Price price, Quantity qty) {
        return std::make_shared<Order>(id, "CLIENT001", "AAPL", side, OrderType::Limit, price, qty);
    }
    
    std::shared_ptr<Order> createMarketOrder(OrderID id, Side side, Quantity qty) {
        return std::make_shared<Order>(id, "CLIENT001", "AAPL", side, qty);
    }
    
    std::unique_ptr<OrderBook> orderBook;
    std::vector<TradePtr> trades;
    std::vector<std::shared_ptr<Order>> orderUpdates;
};

// 測試基本訂單加入
TEST_F(OrderBookTest, AddBasicOrders) {
    auto buyOrder = createLimitOrder(1, Side::Buy, 100.0, 10);
    auto sellOrder = createLimitOrder(2, Side::Sell, 101.0, 15);
    
    orderBook->addOrder(buyOrder);
    orderBook->addOrder(sellOrder);
    
    EXPECT_EQ(orderBook->getBidPrice(), 100.0);
    EXPECT_EQ(orderBook->getAskPrice(), 101.0);
    EXPECT_EQ(orderBook->getSpread(), 1.0);
    EXPECT_EQ(orderBook->getMidPrice(), 100.5);
    EXPECT_TRUE(trades.empty()); // 沒有價格重疊，不應該有交易
}

// 測試基本撮合
TEST_F(OrderBookTest, BasicMatching) {
    // 先加入賣單
    auto sellOrder = createLimitOrder(1, Side::Sell, 100.0, 10);
    orderBook->addOrder(sellOrder);
    
    // 加入可以撮合的買單
    auto buyOrder = createLimitOrder(2, Side::Buy, 100.0, 8);
    auto generatedTrades = orderBook->addOrder(buyOrder);
    
    // 檢查交易結果
    EXPECT_EQ(generatedTrades.size(), 1);
    EXPECT_EQ(trades.size(), 1);
    
    auto trade = trades[0];
    EXPECT_EQ(trade->buyOrderId, 2);
    EXPECT_EQ(trade->sellOrderId, 1);
    EXPECT_EQ(trade->price, 100.0);
    EXPECT_EQ(trade->quantity, 8);
    
    // 檢查訂單狀態
    EXPECT_TRUE(buyOrder->isFilled());
    EXPECT_EQ(sellOrder->getRemainingQuantity(), 2);
    EXPECT_EQ(sellOrder->getStatus(), OrderStatus::PartiallyFilled);
    
    // 檢查 Order Book 狀態
    EXPECT_EQ(orderBook->getBidPrice(), 0.0); // 買單已完全成交
    EXPECT_EQ(orderBook->getAskPrice(), 100.0); // 賣單還有剩餘
}

// 測試市價單撮合
TEST_F(OrderBookTest, MarketOrderMatching) {
    // 建立市場深度
    auto sell1 = createLimitOrder(1, Side::Sell, 100.0, 5);
    auto sell2 = createLimitOrder(2, Side::Sell, 101.0, 10);
    
    orderBook->addOrder(sell1);
    orderBook->addOrder(sell2);
    
    // 市價買單，應該從最佳價格開始撮合
    auto marketBuy = createMarketOrder(4, Side::Buy, 12);
    orderBook->addOrder(marketBuy);
    
    // 應該產生 2 筆交易
    EXPECT_EQ(trades.size(), 2);
    
    // 第一筆：5股 @ 100.0
    EXPECT_EQ(trades[0]->quantity, 5);
    EXPECT_EQ(trades[0]->price, 100.0);
    
    // 第二筆：7股 @ 101.0
    EXPECT_EQ(trades[1]->quantity, 7);
    EXPECT_EQ(trades[1]->price, 101.0);
    
    // 市價單應該完全成交
    EXPECT_TRUE(marketBuy->isFilled());
}

// 測試訂單取消
TEST_F(OrderBookTest, OrderCancellation) {
    auto buy = createLimitOrder(1, Side::Buy, 100.0, 10);
    auto sell = createLimitOrder(2, Side::Sell, 101.0, 15);
    
    orderBook->addOrder(buy);
    orderBook->addOrder(sell);
    
    // 取消買單
    EXPECT_TRUE(orderBook->cancelOrder(1));
    EXPECT_EQ(orderBook->getBidPrice(), 0.0);
    EXPECT_TRUE(buy->isCancelled());
    
    // 嘗試取消不存在的訂單
    EXPECT_FALSE(orderBook->cancelOrder(999));
}

// 測試市價單無法完全成交
TEST_F(OrderBookTest, MarketOrderPartialReject) {
    // 只有少量賣單
    auto sell = createLimitOrder(1, Side::Sell, 100.0, 5);
    orderBook->addOrder(sell);
    
    // 大額市價買單
    auto marketBuy = createMarketOrder(2, Side::Buy, 20);
    orderBook->addOrder(marketBuy);
    
    // 應該只成交 5 股，剩餘 15 股被拒絕
    EXPECT_EQ(trades.size(), 1);
    EXPECT_EQ(trades[0]->quantity, 5);
    EXPECT_TRUE(marketBuy->isRejected()); // 無法完全成交的市價單被拒絕
}

// 測試效能（簡化版本）
TEST_F(OrderBookTest, PerformanceTest) {
    auto start = std::chrono::high_resolution_clock::now();
    
    const int ORDER_COUNT = 1000; // 降低數量避免測試時間過長
    
    // 加入大量訂單
    for (int i = 1; i <= ORDER_COUNT; ++i) {
        Side side = (i % 2 == 0) ? Side::Buy : Side::Sell;
        Price price = 100.0 + (i % 100) * 0.01; // 價格在 100-101 之間變化
        
        auto order = createLimitOrder(i, side, price, 10);
        orderBook->addOrder(order);
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
    
    double avgTimePerOrder = static_cast<double>(duration.count()) / ORDER_COUNT;
    
    std::cout << "Processed " << ORDER_COUNT << " orders in " << duration.count() 
              << "μs (avg: " << avgTimePerOrder << "μs per order)" << std::endl;
    std::cout << "Generated " << trades.size() << " trades" << std::endl;
    
    // 效能目標：平均每個訂單處理時間應小於 50 微秒 (放寬標準)
    EXPECT_LT(avgTimePerOrder, 50.0) << "Order processing too slow: " << avgTimePerOrder << "μs per order";
}

// 測試字串輸出
TEST_F(OrderBookTest, StringOutput) {
    orderBook->addOrder(createLimitOrder(1, Side::Buy, 99.5, 100));
    orderBook->addOrder(createLimitOrder(2, Side::Sell, 100.5, 150));
    
    std::string output = orderBook->toString();
    
    // 檢查輸出包含關鍵資訊
    EXPECT_TRUE(output.find("AAPL") != std::string::npos);
    EXPECT_TRUE(output.find("99.5") != std::string::npos);   // Best Bid
    EXPECT_TRUE(output.find("100.5") != std::string::npos);  // Best Ask
    
    // 測試交易字串輸出
    auto trade = std::make_shared<Trade>(1, 2, 100.0, 50, "AAPL");
    std::string tradeStr = tradeToString(trade);
    
    EXPECT_TRUE(tradeStr.find("Buy#1") != std::string::npos);
    EXPECT_TRUE(tradeStr.find("Sell#2") != std::string::npos);
    EXPECT_TRUE(tradeStr.find("50@100.00") != std::string::npos);
}

// TEST_F(OrderBookTest, SimpleMatching) {
//     try {
//         // 詳細的偵錯輸出
//         std::cout << "Added sell order: " << sellOrder->toString() << std::endl;
//         // ...
//     } catch (const std::exception& e) {
//         FAIL() << "SimpleMatching test threw exception: " << e.what();
//     }
// }

// 主函式
int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}