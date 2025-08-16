#include "order_book.h"
#include <algorithm>
#include <sstream>
#include <iomanip>

namespace mts {
namespace core {

// OrderBookSide 實作
OrderBookSide::OrderBookSide(Side side) : side_(side) {}

void OrderBookSide::addOrder(OrderPtr order) {
    if (!order || order->getSide() != side_) {
        return;
    }
    
    Price price = order->getPrice();
    
    // 市價單特殊處理：使用極端價格
    if (order->isMarketOrder()) {
        price = (side_ == Side::Buy) ? std::numeric_limits<Price>::max() 
                                    : std::numeric_limits<Price>::min();
    }
    
    // 加入價格層級
    priceLevels_[price].push(order);
    
    // 加入快速查找表
    orders_[order->getOrderId()] = std::make_pair(price, order);
}

bool OrderBookSide::removeOrder(OrderID orderId) {
    auto it = orders_.find(orderId);
    if (it == orders_.end()) {
        return false;
    }
    
    Price price = it->second.first;
    orders_.erase(it);
    
    // 從價格層級中移除（需要重建該價格層級的佇列）
    auto& priceLevel = priceLevels_[price];
    std::queue<OrderPtr> newQueue;
    
    while (!priceLevel.empty()) {
        auto order = priceLevel.front();
        priceLevel.pop();
        
        if (order->getOrderId() != orderId) {
            newQueue.push(order);
        }
    }
    
    if (newQueue.empty()) {
        priceLevels_.erase(price);
    } else {
        priceLevels_[price] = std::move(newQueue);
    }
    
    return true;
}

OrderBookSide::OrderPtr OrderBookSide::findOrder(OrderID orderId) const {
    auto it = orders_.find(orderId);
    return (it != orders_.end()) ? it->second.second : nullptr;
}

OrderBookSide::OrderPtr OrderBookSide::getBestOrder() const {
    if (priceLevels_.empty()) {
        return nullptr;
    }
    
    if (side_ == Side::Buy) {
        // 買單：從最高價開始找（使用 reverse_iterator）
        for (auto it = priceLevels_.rbegin(); it != priceLevels_.rend(); ++it) {
            auto& priceLevel = const_cast<PriceLevel&>(it->second);
            
            // 清理無效訂單
            while (!priceLevel.empty() && !priceLevel.front()->isActive()) {
                priceLevel.pop();
            }
            
            if (!priceLevel.empty()) {
                return priceLevel.front();
            }
        }
    } else {
        // 賣單：從最低價開始找（使用 iterator）
        for (auto it = priceLevels_.begin(); it != priceLevels_.end(); ++it) {
            auto& priceLevel = const_cast<PriceLevel&>(it->second);
            
            // 清理無效訂單
            while (!priceLevel.empty() && !priceLevel.front()->isActive()) {
                priceLevel.pop();
            }
            
            if (!priceLevel.empty()) {
                return priceLevel.front();
            }
        }
    }
    
    return nullptr;
}

Price OrderBookSide::getBestPrice() const {
    auto bestOrder = getBestOrder();
    return bestOrder ? bestOrder->getPrice() : 0.0;
}

Quantity OrderBookSide::getTotalQuantityAtPrice(Price price) const {
    auto it = priceLevels_.find(price);
    if (it == priceLevels_.end()) {
        return 0;
    }
    
    const auto& priceLevel = it->second;
    std::queue<OrderPtr> tempQueue = priceLevel; // 複製以避免修改原始佇列
    Quantity total = 0;
    
    while (!tempQueue.empty()) {
        auto order = tempQueue.front();
        tempQueue.pop();
        if (order->isActive()) {
            total += order->getRemainingQuantity();
        }
    }
    
    return total;
}

Quantity OrderBookSide::getTotalQuantity() const {
    Quantity total = 0;
    for (const auto& pair : priceLevels_) {
        const auto& priceLevel = pair.second;
        std::queue<OrderPtr> tempQueue = priceLevel;
        
        while (!tempQueue.empty()) {
            auto order = tempQueue.front();
            tempQueue.pop();
            if (order->isActive()) {
                total += order->getRemainingQuantity();
            }
        }
    }
    return total;
}

size_t OrderBookSide::getOrderCount() const {
    return orders_.size();
}

std::vector<std::pair<Price, Quantity>> OrderBookSide::getPriceLevels(size_t depth) const {
    std::vector<std::pair<Price, Quantity>> result;
    result.reserve(depth);
    
    if (side_ == Side::Buy) {
        // 買單：從高價到低價
        for (auto it = priceLevels_.rbegin(); it != priceLevels_.rend() && result.size() < depth; ++it) {
            Quantity qty = getTotalQuantityAtPrice(it->first);
            if (qty > 0) {
                result.emplace_back(it->first, qty);
            }
        }
    } else {
        // 賣單：從低價到高價
        for (auto it = priceLevels_.begin(); it != priceLevels_.end() && result.size() < depth; ++it) {
            Quantity qty = getTotalQuantityAtPrice(it->first);
            if (qty > 0) {
                result.emplace_back(it->first, qty);
            }
        }
    }
    
    return result;
}

void OrderBookSide::clear() {
    priceLevels_.clear();
    orders_.clear();
}

void OrderBookSide::removeEmptyPriceLevel(Price price) {
    auto it = priceLevels_.find(price);
    if (it != priceLevels_.end() && it->second.empty()) {
        priceLevels_.erase(it);
    }
}

bool OrderBookSide::isPriceBetter(Price newPrice, Price existingPrice) const {
    if (side_ == Side::Buy) {
        return newPrice > existingPrice;  // 買單：價格越高越好
    } else {
        return newPrice < existingPrice;  // 賣單：價格越低越好
    }
}

// OrderBook 實作
OrderBook::OrderBook(const Symbol& symbol) 
    : symbol_(symbol), bidSide_(Side::Buy), askSide_(Side::Sell) {}

std::vector<TradePtr> OrderBook::addOrder(OrderPtr order) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (!order || order->getSymbol() != symbol_) {
        return {};
    }
    
    // 嘗試撮合
    auto trades = matchOrder(order);
    
    // 如果訂單還有剩餘數量，加入相應的 Order Book 側
    if (order->isActive() && order->getRemainingQuantity() > 0) {
        if (order->isBuyOrder()) {
            bidSide_.addOrder(order);
        } else {
            askSide_.addOrder(order);
        }
        
        notifyOrderUpdate(order);
    }
    
    return trades;
}

std::vector<TradePtr> OrderBook::matchOrder(OrderPtr order) {
    if (order->isMarketOrder()) {
        return matchMarketOrder(order);
    } else {
        return matchLimitOrder(order);
    }
}

std::vector<TradePtr> OrderBook::matchLimitOrder(OrderPtr order) {
    std::vector<TradePtr> trades;
    
    // 選擇對手方
    OrderBookSide& oppositeSide = order->isBuyOrder() ? askSide_ : bidSide_;
    
    while (order->isActive() && order->getRemainingQuantity() > 0) {
        auto bestOpposite = oppositeSide.getBestOrder();
        
        if (!bestOpposite || !bestOpposite->isActive()) {
            break;  // 沒有對手單
        }
        
        // 檢查價格是否可以撮合
        Price orderPrice = order->getPrice();
        Price oppositePrice = bestOpposite->getPrice();
        
        if (!canMatch(order->isBuyOrder() ? orderPrice : oppositePrice,
                     order->isBuyOrder() ? oppositePrice : orderPrice)) {
            break;  // 價格不匹配
        }
        
        // 確定成交價格（先來價格優先）
        Price tradePrice = bestOpposite->getPrice();
        
        // 確定成交數量
        Quantity tradeQty = std::min(order->getRemainingQuantity(), 
                                   bestOpposite->getRemainingQuantity());
        
        // 執行交易
        auto trade = executeTrade(
            order->isBuyOrder() ? order : bestOpposite,
            order->isSellOrder() ? order : bestOpposite,
            tradePrice, tradeQty
        );
        
        trades.push_back(trade);
        
        // 更新訂單
        order->fillQuantity(tradeQty);
        bestOpposite->fillQuantity(tradeQty);
        
        // 通知訂單更新
        notifyOrderUpdate(order);
        notifyOrderUpdate(bestOpposite);
        notifyTrade(trade);
        
        //如果對手單完全成交，從 Order Book 中移除 
        if (bestOpposite->isFilled()) {
            oppositeSide.removeOrder(bestOpposite->getOrderId());
        }
    }
    
    return trades;
}

std::vector<TradePtr> OrderBook::matchMarketOrder(OrderPtr order) {
    std::vector<TradePtr> trades;
    
    // 市價單與所有可用的對手單撮合
    OrderBookSide& oppositeSide = order->isBuyOrder() ? askSide_ : bidSide_;
    
    while (order->isActive() && order->getRemainingQuantity() > 0) {
        auto bestOpposite = oppositeSide.getBestOrder();
        
        if (!bestOpposite || !bestOpposite->isActive()) {
            // 市價單無法完全成交，標記為拒絕
            order->setStatus(OrderStatus::Rejected);
            break;
        }
        
        // 市價單以對手方最佳價格成交
        Price tradePrice = bestOpposite->getPrice();
        Quantity tradeQty = std::min(order->getRemainingQuantity(), 
                                   bestOpposite->getRemainingQuantity());
        
        // 執行交易
        auto trade = executeTrade(
            order->isBuyOrder() ? order : bestOpposite,
            order->isSellOrder() ? order : bestOpposite,
            tradePrice, tradeQty
        );
        
        trades.push_back(trade);
        
        // 更新訂單
        order->fillQuantity(tradeQty);
        bestOpposite->fillQuantity(tradeQty);
        
        // 通知訂單更新
        notifyOrderUpdate(order);
        notifyOrderUpdate(bestOpposite);
        notifyTrade(trade);
        
        // 如果對手單完全成交，從 Order Book 中移除
        if (bestOpposite->isFilled()) {
            oppositeSide.removeOrder(bestOpposite->getOrderId());
        }
    }
    
    return trades;
}

TradePtr OrderBook::executeTrade(OrderPtr buyOrder, OrderPtr sellOrder, Price price, Quantity quantity) {
    return std::make_shared<Trade>(
        buyOrder->getOrderId(),
        sellOrder->getOrderId(),
        price,
        quantity,
        symbol_
    );
}

bool OrderBook::canMatch(Price bidPrice, Price askPrice) const {
    return bidPrice >= askPrice;  // 買價 >= 賣價 才能撮合
}

// 市場資訊查詢
Price OrderBook::getBidPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bidSide_.getBestPrice();
}

Price OrderBook::getAskPrice() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return askSide_.getBestPrice();
}

Price OrderBook::getSpread() const {
    std::lock_guard<std::mutex> lock(mutex_);  // 🔒 只鎖定一次
    Price bid = bidSide_.getBestPrice();       // 直接調用，不再鎖定
    Price ask = askSide_.getBestPrice();       // 直接調用，不再鎖定
    return (bid > 0 && ask > 0) ? (ask - bid) : 0.0;
}

Price OrderBook::getMidPrice() const {
    Price bid = getBidPrice();
    Price ask = getAskPrice();
    return (bid > 0 && ask > 0) ? (bid + ask) / 2.0 : 0.0;
}

Quantity OrderBook::getBidQuantity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto bestOrder = bidSide_.getBestOrder();
    return bestOrder ? bestOrder->getRemainingQuantity() : 0;
}

Quantity OrderBook::getAskQuantity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto bestOrder = askSide_.getBestOrder();
    return bestOrder ? bestOrder->getRemainingQuantity() : 0;
}

std::vector<std::pair<Price, Quantity>> OrderBook::getBidDepth(size_t depth) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bidSide_.getPriceLevels(depth);
}

std::vector<std::pair<Price, Quantity>> OrderBook::getAskDepth(size_t depth) const {
    std::lock_guard<std::mutex> lock(mutex_);
    return askSide_.getPriceLevels(depth);
}

size_t OrderBook::getTotalOrderCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bidSide_.getOrderCount() + askSide_.getOrderCount();
}

size_t OrderBook::getBidOrderCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return bidSide_.getOrderCount();
}

size_t OrderBook::getAskOrderCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return askSide_.getOrderCount();
}

OrderBook::OrderPtr OrderBook::findOrder(OrderID orderId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 先在買單側查找
    auto order = bidSide_.findOrder(orderId);
    if (order) {
        return order;
    }
    
    // 再在賣單側查找
    return askSide_.findOrder(orderId);
}

bool OrderBook::cancelOrder(OrderID orderId) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 先在買單側尋找
    auto order = bidSide_.findOrder(orderId);
    if (order) {
        order->setStatus(OrderStatus::Cancelled);
        bidSide_.removeOrder(orderId);
        notifyOrderUpdate(order);
        return true;
    }
    
    // 再在賣單側尋找
    order = askSide_.findOrder(orderId);
    if (order) {
        order->setStatus(OrderStatus::Cancelled);
        askSide_.removeOrder(orderId);
        notifyOrderUpdate(order);
        return true;
    }
    
    return false;
}

void OrderBook::clear() {
    std::lock_guard<std::mutex> lock(mutex_);
    bidSide_.clear();
    askSide_.clear();
}

std::string OrderBook::toString() const {
    // 🎯 關鍵修正：只鎖定一次，直接訪問內部資料
    std::stringstream ss;
    
    Price bidPrice = 0.0;
    Price askPrice = 0.0;
    Quantity bidQty = 0;
    Quantity askQty = 0;
    
    // 手動獲取數據，避免遞迴鎖定
    {
        std::lock_guard<std::mutex> lock(mutex_);  // 🔒 只鎖定一次
        auto bestBid = bidSide_.getBestOrder();    // 直接調用，不再鎖定
        auto bestAsk = askSide_.getBestOrder();    // 直接調用，不再鎖定
        
        if (bestBid) {
            bidPrice = bestBid->getPrice();
            bidQty = bestBid->getRemainingQuantity();
        }
        
        if (bestAsk) {
            askPrice = bestAsk->getPrice();
            askQty = bestAsk->getRemainingQuantity();
        }
    } // 🔓 鎖在這裡釋放
    
    // 在鎖外進行字串運算
    Price spread = (bidPrice > 0 && askPrice > 0) ? (askPrice - bidPrice) : 0.0;
    Price midPrice = (bidPrice > 0 && askPrice > 0) ? (bidPrice + askPrice) / 2.0 : 0.0;
    
    ss << "OrderBook[" << symbol_ << "]:\n";
    ss << "  Best Bid: " << bidPrice << " (" << bidQty << ")\n";
    ss << "  Best Ask: " << askPrice << " (" << askQty << ")\n";
    ss << "  Spread: " << spread << "\n";
    ss << "  Mid Price: " << midPrice << "\n";
    
    return ss.str();
}

void OrderBook::notifyTrade(const TradePtr& trade) {
    if (tradeCallback_) {
        tradeCallback_(trade);
    }
}

void OrderBook::notifyOrderUpdate(const OrderPtr& order) {
    if (orderUpdateCallback_) {
        orderUpdateCallback_(order);
    }
}

// 工具函式
std::string tradeToString(const TradePtr& trade) {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    ss << "Trade[" << trade->symbol << "] "
       << "Buy#" << trade->buyOrderId << " "
       << "Sell#" << trade->sellOrderId << " "
       << trade->quantity << "@" << trade->price;
    return ss.str();
}

} // namespace core
} // namespace mts