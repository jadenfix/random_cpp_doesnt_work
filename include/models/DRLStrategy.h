#pragma once

#include "../backtester/Strategy.h" // Correct path for base class

// --- Include headers needed for FULL definition of parameters/members ---
#include <vector>
#include <string>
#include <map>      // <<< ADDED THIS INCLUDE for std::map
#include <memory>   // For std::unique_ptr (used in example constructor)
// Include necessary common types fully if needed by members, otherwise forward declare
#include "../common/OrderTypes.h" // Needed for SignalDirection member

// --- Forward declare dependencies used only as references/pointers IN THIS HEADER ---
namespace Backtester {
    class FeatureCalculator;
    class DRLInferenceEngine;
    class Portfolio; // Forward declare if only Portfolio& is used in method signatures
} // namespace Backtester

namespace Backtester::Common { // Forward declare event type if needed by signature
    class MarketEvent;
} // namespace Backtester::Common


namespace Backtester {

    class DRLStrategy : public StrategyBase { // Inherit from correct base
    public:
        // --- Constructor Signature Example ---
        // Takes references to dependencies managed elsewhere (e.g., created in main)
        // AND configuration parameters like symbols and size.
        DRLStrategy(
            FeatureCalculator& fc,
            DRLInferenceEngine& ie,
            const std::vector<std::string>& symbols,
            double target_size
        ); // Definition goes in .cpp file

        // Alternative constructor if taking ownership of engine:
        // DRLStrategy(
        //    FeatureCalculator& fc,
        //    std::unique_ptr<DRLInferenceEngine> engine, // Takes ownership
        //    const std::vector<std::string>& symbols,
        //    double target_size
        // );

        // Override base class method - ensure signature matches base
        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override; // Definition in .cpp

    private:
        // --- Member Variables ---
        // Store references to dependencies (ensure their lifetime exceeds this object's)
        FeatureCalculator& feature_calculator_;
        DRLInferenceEngine& inference_engine_;

        // Store parameters
        std::vector<std::string> symbols_to_trade_;
        double target_position_size_;

        // Internal state - uses std::map
        std::map<std::string, Common::SignalDirection> current_signal_state_;

    }; // End class DRLStrategy

} // namespace Backtester