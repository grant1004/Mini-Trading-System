#pragma once

#include <string>
#include <chrono>
#include <memory>

namespace mts {
namespace core {

// 基礎型別定義
using OrderID = uint64_t;
using Price = double;
using Quantity = uint64_t;     // 數量
using Symbol = std::string;
using ClientID = std::string;
using Timestamp = std::chrono::high_resolution_clock::time_point;

// 訂單方向
enum class Side : char {
    Buy = '1',
    Sell = '2'
};

// 訂單類型
enum class OrderType : char {
    Market = '1',      // 市價單
    Limit = '2',       // 限價單
    Stop = '3',        // 停損單
    StopLimit = '4'    // 停損限價單
};

// 訂單狀態
enum class OrderStatus : char {
    New = '0',              // 新訂單
    PartiallyFilled = '1',  // 部分成交
    Filled = '2',           // 完全成交
    Cancelled = '4',        // 已取消
    Rejected = '8'          // 已拒絕
};

// 時效性類型
enum class TimeInForce : char {
    Day = '0',       // 當日有效
    GTC = '1',       // 取消前有效 (Good Till Cancel)
    IOC = '3',       // 立即成交否則取消 (Immediate Or Cancel)
    FOK = '4'        // 全部成交否則取消 (Fill Or Kill)
};

class Order {
public:
    // 建構函式
    Order() = default;
    
    // 完整建構函式
    Order(OrderID orderId,
          const ClientID& clientId,
          const Symbol& symbol,
          Side side,
          OrderType orderType,
          Price price,
          Quantity quantity,
          TimeInForce timeInForce = TimeInForce::Day);
    
    // 市價單建構函式 (無需價格)
    Order(OrderID orderId,
          const ClientID& clientId,
          const Symbol& symbol,
          Side side,
          Quantity quantity,
          TimeInForce timeInForce = TimeInForce::Day);
    
    // 複製建構函式和賦值運算子
    Order(const Order& other) = default;
    Order& operator=(const Order& other) = default;
    Order(Order&& other) noexcept = default;
    Order& operator=(Order&& other) noexcept = default;
    
    // 解構函式
    ~Order() = default;
    
    // Getter 方法
    OrderID getOrderId() const noexcept { return orderId_; }
    const ClientID& getClientId() const noexcept { return clientId_; }
    const Symbol& getSymbol() const noexcept { return symbol_; }
    Side getSide() const noexcept { return side_; }
    OrderType getOrderType() const noexcept { return orderType_; }
    Price getPrice() const noexcept { return price_; }
    Quantity getQuantity() const noexcept { return quantity_; }
    Quantity getRemainingQuantity() const noexcept { return remainingQuantity_; }
    Quantity getFilledQuantity() const noexcept { return quantity_ - remainingQuantity_; }
    OrderStatus getStatus() const noexcept { return status_; }
    TimeInForce getTimeInForce() const noexcept { return timeInForce_; }
    Timestamp getTimestamp() const noexcept { return timestamp_; }
    
    // Setter 方法 (主要用於訂單狀態更新)
    void setStatus(OrderStatus status) noexcept { status_ = status; }
    void setRemainingQuantity(Quantity quantity) noexcept { remainingQuantity_ = quantity; }
    
    // 業務邏輯方法
    bool isMarketOrder() const noexcept { return orderType_ == OrderType::Market; }
    bool isLimitOrder() const noexcept { return orderType_ == OrderType::Limit; }
    bool isBuyOrder() const noexcept { return side_ == Side::Buy; }
    bool isSellOrder() const noexcept { return side_ == Side::Sell; }
    bool isActive() const noexcept { 
        return status_ == OrderStatus::New || status_ == OrderStatus::PartiallyFilled; 
    }
    bool isFilled() const noexcept { return status_ == OrderStatus::Filled; }
    bool isCancelled() const noexcept { return status_ == OrderStatus::Cancelled; }
    bool isRejected() const noexcept { return status_ == OrderStatus::Rejected; }
    
    // 部分成交處理
    void fillQuantity(Quantity filledQty);
    bool canFill(Quantity quantity) const noexcept;
    
    // 比較運算子 (用於排序)
    bool operator==(const Order& other) const noexcept;
    bool operator!=(const Order& other) const noexcept;
    
    // 價格比較 (用於 OrderBook 排序)
    struct PriceComparator {
        bool operator()(const Order& lhs, const Order& rhs) const noexcept;
    };
    
    struct TimeComparator {
        bool operator()(const Order& lhs, const Order& rhs) const noexcept;
    };
    
    // 字串轉換
    std::string toString() const;
    
    // 驗證訂單有效性
    bool isValid() const noexcept;
    
private:
    OrderID orderId_{0};
    ClientID clientId_;
    Symbol symbol_;
    Side side_{Side::Buy};
    OrderType orderType_{OrderType::Limit};
    Price price_{0.0};
    Quantity quantity_{0};
    Quantity remainingQuantity_{0};
    OrderStatus status_{OrderStatus::New};
    TimeInForce timeInForce_{TimeInForce::Day};
    Timestamp timestamp_{std::chrono::high_resolution_clock::now()};
};

// 輔助函式
std::string sideToString(Side side);
std::string orderTypeToString(OrderType type);
std::string orderStatusToString(OrderStatus status);
std::string timeInForceToString(TimeInForce tif);

Side stringToSide(const std::string& str);
OrderType stringToOrderType(const std::string& str);
OrderStatus stringToOrderStatus(const std::string& str);
TimeInForce stringToTimeInForce(const std::string& str);

} // namespace core
} // namespace mts