#pragma once
#include <string>
#include <stdexcept> // For invalid_argument

namespace Backtester::Common {
    enum class OrderDirection { BUY, SELL };
    enum class OrderType { MARKET, LIMIT };
    enum class SignalDirection { LONG, SHORT, FLAT };

    inline std::string to_string(OrderDirection dir) { /* ... as provided ... */
        switch (dir) { case OrderDirection::BUY: return "BUY"; case OrderDirection::SELL: return "SELL"; default: return "UNKNOWN"; }
    }
    inline std::string to_string(OrderType type) { /* ... as provided ... */
        switch (type) { case OrderType::MARKET: return "MARKET"; case OrderType::LIMIT: return "LIMIT"; default: return "UNKNOWN"; }
    }
    inline std::string to_string(SignalDirection dir) { /* ... as provided ... */
         switch (dir) { case SignalDirection::LONG: return "LONG"; case SignalDirection::SHORT: return "SHORT"; case SignalDirection::FLAT: return "FLAT"; default: return "UNKNOWN"; }
    }
}