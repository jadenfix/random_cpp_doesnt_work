#include "../include/models/DRLStrategy.h" // Self header first
#include "../include/features/FeatureCalculator.h"
#include "../include/models/DRLInferenceEngine.h"
#include "../include/backtester/Portfolio.h"
#include "../include/common/Event.h"
#include "../include/common/Signal.h"
#include "../include/common/OrderTypes.h"
#include "../include/common/Utils.h"
#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <memory>
#include <algorithm>
#include <cmath>
#include <stdexcept> // Include for std::invalid_argument

// Define types used (ensure consistency with InferenceEngine.h)
using FeatureVector = std::vector<double>;
using ModelOutput = std::vector<double>;
using FeatureMap = std::unordered_map<std::string, double>; // From FeatureCalculator

namespace Backtester {

    // Constructor Definition (Example taking references)
    DRLStrategy::DRLStrategy(
        FeatureCalculator& fc,
        DRLInferenceEngine& ie,
        const std::vector<std::string>& symbols,
        double target_size)
        : feature_calculator_(fc), inference_engine_(ie),
          symbols_to_trade_(symbols), target_position_size_(target_size)
    {
        if (symbols_to_trade_.empty() || target_position_size_ <= 0) {
            throw std::invalid_argument("Invalid symbols or target size for DRLStrategy");
        }
        for (const auto& sym : symbols_to_trade_) {
            current_signal_state_[sym] = Common::SignalDirection::FLAT;
        }
    }

    // handle_market_event implementation
    void DRLStrategy::handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) {
        bool trade_this_symbol = false;
        for (const auto& traded_sym : symbols_to_trade_) { if (event.symbol == traded_sym) { trade_this_symbol = true; break; } }
        if (!trade_this_symbol) return;

        // 1. Calculate features (returns FeatureMap)
        FeatureMap features = feature_calculator_.calculate_features(event.marketData);

        // 2. Prepare feature vector (MUST match model training!)
        FeatureVector model_input;
        // --- Use actual feature names defined in FeatureCalculator ---
        std::vector<std::string> feature_order = {"price", "SMA_10_stub", "RSI_14_stub", "dummy_feature"}; // EXAMPLE ORDER
        model_input.reserve(feature_order.size());
        for(const std::string& fname : feature_order) {
            if (features.count(fname)) model_input.push_back(features.at(fname));
            else model_input.push_back(0.0); // Use 0 for missing
        }

        if (model_input.empty()) { return; } // Skip if no features

        // 3. Get prediction (predict now takes FeatureVector, returns ModelOutput)
        ModelOutput predictions = inference_engine_.predict(model_input); // <-- Should match now

        // 4. Interpret predictions (vector of probabilities/scores)
        if (predictions.empty() || predictions.size() < 3) { // Check vector size
            std::cerr << "Warning (DRLStrategy): Invalid prediction output size for " << event.symbol << std::endl;
            return;
        }
        Common::SignalDirection desired_signal = Common::SignalDirection::FLAT;
        auto max_it = std::max_element(predictions.begin(), predictions.end());
        size_t action_index = std::distance(predictions.begin(), max_it);

        // Map action index to SignalDirection
        if (action_index == 0) desired_signal = Common::SignalDirection::LONG;
        else if (action_index == 1) desired_signal = Common::SignalDirection::SHORT;
        // else FLAT (default)

        // 5. Generate SignalEvent if state changes
        if (desired_signal != current_signal_state_[event.symbol]) {
             std::cout << "DRL Signal: " << event.symbol << " @ " << Utils::formatTimestampUTC(event.timestamp)
                       << " Action=" << Common::to_string(desired_signal);
             if (predictions.size() >= 3) { // Print probabilities
                  std::cout << " (Probs: B=" << std::fixed << std::setprecision(3) << predictions[0]
                            << ", S=" << predictions[1] << ", H=" << predictions[2] << ")";
             }
             std::cout << std::endl;

             Common::Signal signal(event.timestamp, event.symbol, desired_signal);
             Common::SignalEvent signal_event(event.timestamp, signal);
             portfolio.generate_order(signal_event); // Portfolio handles order generation

             current_signal_state_[event.symbol] = desired_signal; // Update state
        }
    }

} // namespace Backtester