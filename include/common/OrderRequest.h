#pragma once
#include <chrono>
#include <string>
#include <optional>
#include <atomic>
#include "OrderTypes.h"

namespace Backtester::Common {
    inline long long generate_unique_order_id() { /* ... as provided ... */
        static std::atomic<long long> id_counter{0}; return ++id_counter;
    }
    struct OrderRequest {
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        long long order_id;
        std::string symbol;
        OrderType order_type;
        OrderDirection direction;
        double quantity;
        std::optional<double> limit_price;

        // Market Order Constructor
        OrderRequest(std::chrono::time_point<std::chrono::system_clock> ts, std::string sym,
                     OrderDirection dir, double qty) : timestamp(ts), order_id(generate_unique_order_id()),
                     symbol(std::move(sym)), order_type(OrderType::MARKET), direction(dir),
                     quantity(qty), limit_price(std::nullopt) {}
        // Limit Order Constructor
        OrderRequest(std::chrono::time_point<std::chrono::system_clock> ts, std::string sym,
                     OrderDirection dir, double qty, double price) : timestamp(ts), order_id(generate_unique_order_id()),
                     symbol(std::move(sym)), order_type(OrderType::LIMIT), direction(dir),
                     quantity(qty), limit_price(price) {}
    };
}