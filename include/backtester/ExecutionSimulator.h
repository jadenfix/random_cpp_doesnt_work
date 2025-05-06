#pragma once
#include <vector>
#include <stdexcept>
#include <optional> // For optional return if order doesn't fill
#include "../common/Event.h"
#include "../common/OrderRequest.h"
#include "../common/FillEvent.h" // Needs FillDetails

namespace Backtester {
    class ExecutionSimulator {
    public:
        virtual ~ExecutionSimulator() = default;
        // Changed return type to optional for unfilled orders
        virtual std::optional<Common::FillDetails> simulate_order(
            const Common::OrderRequest& order,
            const Common::DataSnapshot& current_market_data); // Define in .cpp
    };
}