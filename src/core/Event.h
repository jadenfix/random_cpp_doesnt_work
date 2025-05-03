#pragma once

#include "data/PriceBar.h" // Use path relative to src/ include dir
#include <vector>
#include <string>
#include <chrono>
#include <variant>
#include <map> // Using std::map for potentially ordered processing later
#include <memory> // For std::shared_ptr

// --- Define DataSnapshot consistently here ---
// Use map for ordered iteration if needed, or std::unordered_map for performance
using DataSnapshot = std::map<std::string, PriceBar>;


// --- Event Types Enum ---
enum class EventType {
    MARKET,
    SIGNAL,
    ORDER,
    FILL
};

// --- Base Event Struct ---
struct BaseEvent {
    EventType type;
    std::chrono::system_clock::time_point timestamp;

    virtual ~BaseEvent() = default;
protected:
    BaseEvent(EventType t, std::chrono::system_clock::time_point ts) : type(t), timestamp(ts) {}
};

// --- Specific Event Structs ---
struct MarketEvent : public BaseEvent {
    DataSnapshot data;
    MarketEvent(std::chrono::system_clock::time_point ts, DataSnapshot d)
        : BaseEvent(EventType::MARKET, ts), data(std::move(d)) {}
};

enum class SignalDirection { LONG, SHORT, FLAT };
struct SignalEvent : public BaseEvent {
    std::string symbol;
    SignalDirection direction;
    SignalEvent(std::chrono::system_clock::time_point ts, std::string sym, SignalDirection dir)
        : BaseEvent(EventType::SIGNAL, ts), symbol(std::move(sym)), direction(dir) {}
};

enum class OrderType { MARKET, LIMIT };
enum class OrderDirection { BUY, SELL };
struct OrderEvent : public BaseEvent {
    std::string symbol;
    OrderType order_type;
    OrderDirection direction;
    double quantity;
    OrderEvent(std::chrono::system_clock::time_point ts, std::string sym, OrderType type, OrderDirection dir, double qty)
        : BaseEvent(EventType::ORDER, ts), symbol(std::move(sym)), order_type(type), direction(dir), quantity(qty) {}
};

struct FillEvent : public BaseEvent {
    std::string symbol;
    OrderDirection direction;
    double quantity;
    double fill_price;
    double commission = 0.0;
    FillEvent(std::chrono::system_clock::time_point ts, std::string sym, OrderDirection dir, double qty, double price, double comm = 0.0)
        : BaseEvent(EventType::FILL, ts), symbol(std::move(sym)), direction(dir), quantity(qty), fill_price(price), commission(comm) {}
};

// --- Event Pointer Alias ---
using EventPtr = std::shared_ptr<BaseEvent>;