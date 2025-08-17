#pragma once
#include "order.h"
#include <map>
#include <queue>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <limits>

namespace mts {
namespace core {

// 交易記錄
struct Trade {
    OrderID buyOrderId;
    OrderID sellOrderId;
    Price price;
    Quantity quantity;
    Timestamp timestamp;
    Symbol symbol;
    
    Trade(OrderID bid, OrderID ask, Price p, Quantity q, const Symbol& sym)
        : buyOrderId(bid), sellOrderId(ask), price(p), quantity(q)
        , symbol(sym), timestamp(std::chrono::high_resolution_clock::now()) {}
};

using TradePtr = std::shared_ptr<Trade>;

// Order Book 的一邊（買單或賣單）
class OrderBookSide {
public:
    using OrderPtr = std::shared_ptr<Order>;
    using PriceLevel = std::queue<OrderPtr>;
    using PriceLevelMap = std::map<Price, PriceLevel>;
    
    OrderBookSide(Side side);
    
    // 基本操作
    void addOrder(OrderPtr order);
    bool removeOrder(OrderID orderId);
    OrderPtr findOrder(OrderID orderId) const;
    
    // 撮合相關
    OrderPtr getBestOrder() const;
    Price getBestPrice() const;
    Quantity getTotalQuantityAtPrice(Price price) const;
    Quantity getTotalQuantity() const;
    
    // 查詢操作
    bool isEmpty() const { return orders_.empty(); }
    size_t getOrderCount() const;
    std::vector<std::pair<Price, Quantity>> getPriceLevels(size_t depth = 10) const;
    
    // 清理操作
    void clear();
    
private:
    Side side_;
    PriceLevelMap priceLevels_;  // 價格層級 (價格 -> 訂單佇列)
    std::map<OrderID, std::pair<Price, OrderPtr>> orders_;  // 快速查找: OrderID -> (Price, Order)
    
    // 根據買賣方向決定價格比較邏輯
    bool isPriceBetter(Price newPrice, Price existingPrice) const;
    void removeEmptyPriceLevel(Price price);
};

// 完整的 Order Book
class OrderBook {
public:
    using OrderPtr = std::shared_ptr<Order>;
    using TradeCallback = std::function<void(const TradePtr&)>;
    using OrderUpdateCallback = std::function<void(const OrderPtr&)>;
    
    explicit OrderBook(const Symbol& symbol);
    ~OrderBook() = default;
    
    // 基本操作
    std::vector<TradePtr> addOrder(OrderPtr order);
    bool cancelOrder(OrderID orderId);
    OrderPtr findOrder(OrderID orderId) const;
    
    // 市場資訊
    Price getBidPrice() const;      // 最佳買價
    Price getAskPrice() const;      // 最佳賣價
    Price getSpread() const;        // 買賣價差
    Price getMidPrice() const;      // 中間價
    
    Quantity getBidQuantity() const;  // 最佳買價數量
    Quantity getAskQuantity() const;  // 最佳賣價數量
    
    // 深度資訊
    std::vector<std::pair<Price, Quantity>> getBidDepth(size_t depth = 10) const;
    std::vector<std::pair<Price, Quantity>> getAskDepth(size_t depth = 10) const;
    
    // 統計資訊
    size_t getTotalOrderCount() const;
    size_t getBidOrderCount() const;
    size_t getAskOrderCount() const;
    
    // 回調設定
    void setTradeCallback(TradeCallback callback) { tradeCallback_ = callback; }
    void setOrderUpdateCallback(OrderUpdateCallback callback) { orderUpdateCallback_ = callback; }
    
    // 工具函式
    std::string toString() const;
    void clear();
    
    // 執行緒安全
    mutable std::mutex mutex_;
    
private:
    Symbol symbol_;
    OrderBookSide bidSide_;   // 買單側
    OrderBookSide askSide_;   // 賣單側
    
    // 回調函式
    TradeCallback tradeCallback_;
    OrderUpdateCallback orderUpdateCallback_;
    
    // 撮合邏輯
    std::vector<TradePtr> matchOrder(OrderPtr order);
    std::vector<TradePtr> matchLimitOrder(OrderPtr order);
    std::vector<TradePtr> matchMarketOrder(OrderPtr order);
    
    // 執行交易
    TradePtr executeTrade(OrderPtr buyOrder, OrderPtr sellOrder, Price price, Quantity quantity);
    
    // 通知回調
    void notifyTrade(const TradePtr& trade);
    void notifyOrderUpdate(const OrderPtr& order);
    
    // 價格驗證
    bool canMatch(Price bidPrice, Price askPrice) const;
};

// 工具函式
std::string tradeToString(const TradePtr& trade);

} // namespace core
} // namespace mts