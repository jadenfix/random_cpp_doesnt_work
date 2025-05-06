#pragma once
#include <chrono>
#include <string>
#include <optional>
#include "OrderTypes.h"

namespace Backtester::Common {
    struct Signal {
        std::chrono::time_point<std::chrono::system_clock> timestamp;
        std::string symbol;
        SignalDirection direction;
        std::optional<double> strength;

        Signal(std::chrono::time_point<std::chrono::system_clock> ts,
               std::string sym, SignalDirection dir,
               std::optional<double> str = std::nullopt)
            : timestamp(ts), symbol(std::move(sym)), direction(dir), strength(str) {}
    };
}