#pragma once
#include <vector>
#include <memory>
#include <iostream>
#include <chrono>
#include <string>

// --- Include FULL definitions needed for members/constructor ---
#include "DataManager.h"
#include "Strategy.h"
#include "Portfolio.h"          // <<< Include full Portfolio definition
#include "ExecutionSimulator.h"
#include "../common/Event.h"    // For potential future use

namespace Backtester {
    // --- Forward declare components ONLY if they are just pointers/references ---
    // Since we included the headers above, these are not strictly necessary
    class DataManager;
    class StrategyBase;
    // class Portfolio; // <<< MUST BE REMOVED or COMMENTED OUT
    class ExecutionSimulator;
    // namespace Common { class MarketEvent; } // Example

    class Backtester {
    public:
        Backtester(
            DataManager& dataManager,
            StrategyBase& strategy,
            Portfolio& portfolio,           // Needs Portfolio definition
            ExecutionSimulator& executionSimulator
        );
        void run();

    private:
        DataManager& data_manager_;
        StrategyBase& strategy_;
        Portfolio& portfolio_;          // Member needs Portfolio definition
        ExecutionSimulator& execution_simulator_; // Warning: Unused (can ignore for now)
    };
} // namespace Backtester