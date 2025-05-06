#pragma once // Use include guards

#include <vector>
#include <string>
#include <memory> // For potential use with event queue/smart pointers

// Include necessary common types used in the interface
#include "../common/Event.h" // Needs MarketEvent definition

// Forward declare Portfolio to avoid circular dependency
namespace Backtester { class Portfolio; }

namespace Backtester {

    // Abstract Base Class for all trading strategies
    class StrategyBase {
    public:
        virtual ~StrategyBase() = default;

        // Primary method called by the Backtester for each new market event.
        // The strategy analyzes the event and interacts with the Portfolio
        // to potentially generate orders.
        virtual void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) = 0;

        // Optional: Method to handle fill events if strategy needs to react to fills
        //           (e.g., update internal state, trailing stops)
        virtual void handle_fill_event(const Common::FillEvent& event, Portfolio& portfolio) {
             // Default implementation does nothing
             (void)event; // Suppress unused warning
             (void)portfolio; // Suppress unused warning
        }

        // Optional: Method to handle signal events (if using a separate signal step)
        // virtual void handle_signal_event(const Common::SignalEvent& event, Portfolio& portfolio) {}

    }; // End class StrategyBase

} // namespace Backtester