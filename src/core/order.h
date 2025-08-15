#pragma once
#include <string>
#include <chrono>
#include <atomic>

enum class OrderSide : uint8_t {
    BUY = 1,
    SELL = 2
};

enum class OrderType : uint8_t {
    MARKET = 1,
    LIMIT = 2
};

enum class OrderStatus : uint8_t {
    NEW = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 4,
    REJECTED = 8
};

struct Order {
    // 使用 alignas 優化快取行
    alignas(64) std::string order_id;
    std::string symbol;
    OrderSide side;
    OrderType type;
    double price;
    int quantity;
    std::atomic<int> remaining_quantity;
    std::atomic<OrderStatus> status;
    std::chrono::steady_clock::time_point timestamp;
    
    Order() 
        : price(0.0)
        , quantity(0)
        , remaining_quantity(0)
        , status(OrderStatus::NEW) {
        timestamp = std::chrono::steady_clock::now();
    }
    
    // 複製建構函式
    Order(const Order& other) 
        : order_id(other.order_id)
        , symbol(other.symbol)
        , side(other.side)
        , type(other.type)
        , price(other.price)
        , quantity(other.quantity)
        , remaining_quantity(other.remaining_quantity.load())
        , status(other.status.load())
        , timestamp(other.timestamp) {}
    
    // 移動建構函式
    Order(Order&& other) noexcept
        : order_id(std::move(other.order_id))
        , symbol(std::move(other.symbol))
        , side(other.side)
        , type(other.type)
        , price(other.price)
        , quantity(other.quantity)
        , remaining_quantity(other.remaining_quantity.load())
        , status(other.status.load())
        , timestamp(other.timestamp) {}
};