#pragma once
#include <chrono>
#include <string>
#include <atomic>
#include "OrderTypes.h"

namespace Backtester::Common {
    inline long long generate_unique_fill_id() { /* ... as provided ... */
         static std::atomic<long long> id_counter{0}; return ++id_counter;
     }
    struct FillDetails {
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        long long fill_id;
        long long order_id;
        std::string symbol;
        OrderDirection direction;
        double quantity;
        double fill_price;
        double commission;

        FillDetails(std::chrono::time_point<std::chrono::system_clock> ts, long long original_order_id,
                    std::string sym, OrderDirection dir, double qty, double price, double comm)
            : timestamp(ts), fill_id(generate_unique_fill_id()), order_id(original_order_id),
              symbol(std::move(sym)), direction(dir), quantity(qty), fill_price(price), commission(comm) {}
    };
}