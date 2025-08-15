#include "Order.h"
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <cmath>

namespace mts {
namespace core {

// 完整建構函式
Order::Order(OrderID orderId,
             const ClientID& clientId,
             const Symbol& symbol,
             Side side,
             OrderType orderType,
             Price price,
             Quantity quantity,
             TimeInForce timeInForce)
    : orderId_(orderId)
    , clientId_(clientId)
    , symbol_(symbol)
    , side_(side)
    , orderType_(orderType)
    , price_(price)
    , quantity_(quantity)
    , remainingQuantity_(quantity)
    , status_(OrderStatus::New)
    , timeInForce_(timeInForce)
    , timestamp_(std::chrono::high_resolution_clock::now())
{
    // 市價單價格應為 0
    if (orderType == OrderType::Market) {
        price_ = 0.0;
    }
    
    // 限價單必須有有效價格
    if (orderType == OrderType::Limit && price <= 0.0) {
        throw std::invalid_argument("Limit order must have valid price > 0");
    }
    
    if (quantity == 0) {
        throw std::invalid_argument("Order quantity must be > 0");
    }
    
    if (symbol.empty()) {
        throw std::invalid_argument("Order symbol cannot be empty");
    }
}

// 市價單建構函式( OrderType = Market = 1 )
Order::Order(OrderID orderId,
             const ClientID& clientId,
             const Symbol& symbol,
             Side side,
             Quantity quantity,
             TimeInForce timeInForce)
    : Order(orderId, clientId, symbol, side, OrderType::Market, 0.0, quantity, timeInForce)
{
}

// 部分成交處理
void Order::fillQuantity(Quantity filledQty) {
    if (filledQty == 0) {
        return;
    }
    
    if (filledQty > remainingQuantity_) {
        throw std::invalid_argument("Filled quantity cannot exceed remaining quantity");
    }
    
    remainingQuantity_ -= filledQty;
    
    // 更新訂單狀態
    if (remainingQuantity_ == 0) {
        status_ = OrderStatus::Filled;
    } else {
        status_ = OrderStatus::PartiallyFilled;
    }
}

bool Order::canFill(Quantity quantity) const noexcept {
    return quantity > 0 && quantity <= remainingQuantity_ && isActive();
}

// 比較運算子
bool Order::operator==(const Order& other) const noexcept {
    return orderId_ == other.orderId_;
}

bool Order::operator!=(const Order& other) const noexcept {
    return !(*this == other);
}

// 價格比較器 (用於 OrderBook 排序)
bool Order::PriceComparator::operator()(const Order& lhs, const Order& rhs) const noexcept {
    // 買單：價格高的優先 (降序)
    if (lhs.isBuyOrder() && rhs.isBuyOrder()) {
        return lhs.getPrice() > rhs.getPrice();
    }
    // 賣單：價格低的優先 (升序)
    else if (lhs.isSellOrder() && rhs.isSellOrder()) {
        return lhs.getPrice() < rhs.getPrice();
    }
    // 不同方向的訂單不應該比較
    return false;
}

// 時間比較器 (FIFO - 先進先出)
bool Order::TimeComparator::operator()(const Order& lhs, const Order& rhs) const noexcept {
    return lhs.getTimestamp() < rhs.getTimestamp();
}

// 字串轉換
std::string Order::toString() const {
    std::stringstream ss;
    ss << std::fixed << std::setprecision(2);
    
    ss << "Order["
       << "ID=" << orderId_
       << ", Client=" << clientId_
       << ", Symbol=" << symbol_
       << ", Side=" << sideToString(side_)
       << ", Type=" << orderTypeToString(orderType_)
       << ", Price=" << price_
       << ", Qty=" << quantity_
       << ", Remaining=" << remainingQuantity_
       << ", Status=" << orderStatusToString(status_)
       << ", TIF=" << timeInForceToString(timeInForce_)
       << "]";
    
    return ss.str();
}

// 驗證訂單有效性
bool Order::isValid() const noexcept {
    // 基本欄位檢查
    if (orderId_ == 0 || symbol_.empty() || quantity_ == 0) {
        return false;
    }
    
    // 限價單必須有有效價格
    if (orderType_ == OrderType::Limit && price_ <= 0.0) {
        return false;
    }
    
    // 市價單價格應為 0
    if (orderType_ == OrderType::Market && price_ != 0.0) {
        return false;
    }
    
    // 剩餘數量不能超過總數量
    if (remainingQuantity_ > quantity_) {
        return false;
    }
    
    return true;
}

// 輔助函式實作
std::string sideToString(Side side) {
    switch (side) {
        case Side::Buy: return "BUY";
        case Side::Sell: return "SELL";
        default: return "UNKNOWN";
    }
}

std::string orderTypeToString(OrderType type) {
    switch (type) {
        case OrderType::Market: return "MARKET";
        case OrderType::Limit: return "LIMIT";
        case OrderType::Stop: return "STOP";
        case OrderType::StopLimit: return "STOP_LIMIT";
        default: return "UNKNOWN";
    }
}

std::string orderStatusToString(OrderStatus status) {
    switch (status) {
        case OrderStatus::New: return "NEW";
        case OrderStatus::PartiallyFilled: return "PARTIALLY_FILLED";
        case OrderStatus::Filled: return "FILLED";
        case OrderStatus::Cancelled: return "CANCELLED";
        case OrderStatus::Rejected: return "REJECTED";
        default: return "UNKNOWN";
    }
}

std::string timeInForceToString(TimeInForce tif) {
    switch (tif) {
        case TimeInForce::Day: return "DAY";
        case TimeInForce::GTC: return "GTC";
        case TimeInForce::IOC: return "IOC";
        case TimeInForce::FOK: return "FOK";
        default: return "UNKNOWN";
    }
}

Side stringToSide(const std::string& str) {
    if (str == "BUY" || str == "1") return Side::Buy;
    if (str == "SELL" || str == "2") return Side::Sell;
    throw std::invalid_argument("Invalid side string: " + str);
}

OrderType stringToOrderType(const std::string& str) {
    if (str == "MARKET" || str == "1") return OrderType::Market;
    if (str == "LIMIT" || str == "2") return OrderType::Limit;
    if (str == "STOP" || str == "3") return OrderType::Stop;
    if (str == "STOP_LIMIT" || str == "4") return OrderType::StopLimit;
    throw std::invalid_argument("Invalid order type string: " + str);
}

OrderStatus stringToOrderStatus(const std::string& str) {
    if (str == "NEW" || str == "0") return OrderStatus::New;
    if (str == "PARTIALLY_FILLED" || str == "1") return OrderStatus::PartiallyFilled;
    if (str == "FILLED" || str == "2") return OrderStatus::Filled;
    if (str == "CANCELLED" || str == "4") return OrderStatus::Cancelled;
    if (str == "REJECTED" || str == "8") return OrderStatus::Rejected;
    throw std::invalid_argument("Invalid order status string: " + str);
}

TimeInForce stringToTimeInForce(const std::string& str) {
    if (str == "DAY" || str == "0") return TimeInForce::Day;
    if (str == "GTC" || str == "1") return TimeInForce::GTC;
    if (str == "IOC" || str == "3") return TimeInForce::IOC;
    if (str == "FOK" || str == "4") return TimeInForce::FOK;
    throw std::invalid_argument("Invalid time in force string: " + str);
}

} // namespace core
} // namespace mts