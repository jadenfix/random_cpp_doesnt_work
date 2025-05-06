#pragma once

#include "backtester/Strategy.h"    // Correct path to base class
#include "common/Event.h"         // Correct path
#include "common/Signal.h"        // Correct path
#include "common/Utils.h"         // Correct path
#include "backtester/Portfolio.h" // Correct path
#include <string>
#include <map>
#include <chrono>
#include <limits> // For numeric_limits
#include <iostream>
#include <cmath>    // For std::abs
#include <stdexcept> // For invalid_argument
#include <algorithm> // For std::max/min

namespace Backtester {

    class OpeningRangeBreakout : public StrategyBase { // Inherit from StrategyBase
    private:
        int opening_range_minutes_;
        // Sizing handled by Portfolio

        // --- State per Symbol ---
        struct SymbolState {
            // ***** ADDED MISSING MEMBERS *****
            std::chrono::system_clock::time_point start_time;
            double range_high = -std::numeric_limits<double>::max();
            double range_low = std::numeric_limits<double>::max();
            bool range_established = false;
            bool trade_taken = false;
            // ***** END ADDED MEMBERS *****
        };
        std::map<std::string, SymbolState> symbol_state_;
        std::map<std::string, Common::SignalDirection> last_signal_direction_;

         // Helper to get OHLC prices
         bool get_ohlc(const Common::DataSnapshot& data, double& o, double& h, double& l, double& c) const {
             bool found_all = true;
             auto get_val = [&](const std::string& key_upper, const std::string& key_lower, double& out_val) {
                 if (data.count(key_upper)) out_val = data.at(key_upper);
                 else if (data.count(key_lower)) out_val = data.at(key_lower);
                 else { out_val = 0.0; found_all = false; }
             };
             get_val("Open", "open", o);
             get_val("High", "high", h);
             get_val("Low", "low", l);
             get_val("Close", "close", c);
             // Only require High, Low, Close for this strategy
             return (data.count("High") || data.count("high")) &&
                    (data.count("Low") || data.count("low")) &&
                    (data.count("Close") || data.count("close"));
        }

    public:
        OpeningRangeBreakout(int range_minutes = 30)
            : opening_range_minutes_(range_minutes) {
            if (opening_range_minutes_ <= 0) {
                 throw std::invalid_argument("Opening range minutes must be positive");
            }
        }

        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
            const std::string& symbol = event.symbol;
            double open, high, low, close;

            if (!get_ohlc(event.marketData, open, high, low, close)) {
                 std::cerr << "Warning (ORB): Missing HLC data for symbol " << symbol << std::endl;
                 return;
            }

            auto current_timestamp = event.timestamp;

            // Initialize state for new symbols
            if (symbol_state_.find(symbol) == symbol_state_.end()) {
                SymbolState& new_state = symbol_state_[symbol]; // Get reference to new state
                new_state.start_time = current_timestamp;
                new_state.range_high = high;
                new_state.range_low = low;
                new_state.range_established = false; // Explicitly set
                new_state.trade_taken = false;       // Explicitly set
                last_signal_direction_[symbol] = Common::SignalDirection::FLAT;
                // ***** CORRECTED NAMESPACE *****
                std::cout << "ORB INIT: " << symbol << " @ " << Utils::formatTimestampUTC(current_timestamp) << std::endl;
            }

            SymbolState& state = symbol_state_[symbol]; // Get reference to state

            // Add daily reset logic here if needed based on timestamp comparison

            auto time_since_start = std::chrono::duration_cast<std::chrono::minutes>(current_timestamp - state.start_time);

            // Establish Opening Range
            if (!state.range_established) {
                if (time_since_start.count() < opening_range_minutes_) {
                   state.range_high = std::max(state.range_high, high);
                   state.range_low = std::min(state.range_low, low);
                } else {
                    state.range_established = true;
                    // ***** CORRECTED NAMESPACE *****
                    std::cout << "ORB ESTABLISHED: " << symbol << " @ " << Utils::formatTimestampUTC(current_timestamp)
                              << " High=" << state.range_high << " Low=" << state.range_low << std::endl;
                }
            }

            // Trade Breakouts
            if (state.range_established && !state.trade_taken) {
                Common::SignalDirection desired_signal_direction = Common::SignalDirection::FLAT;
                if (close > state.range_high) { desired_signal_direction = Common::SignalDirection::LONG; }
                else if (close < state.range_low) { desired_signal_direction = Common::SignalDirection::SHORT; }

                if (desired_signal_direction != Common::SignalDirection::FLAT) {
                     // ***** CORRECTED NAMESPACE *****
                     std::cout << "ORB BREAKOUT: " << symbol << " @ " << Utils::formatTimestampUTC(event.timestamp)
                               << " Close=" << close << " Range=[" << state.range_low << ", " << state.range_high << "]"
                               << " Signal=" << Common::to_string(desired_signal_direction)
                               << std::endl;

                    Common::Signal signal(event.timestamp, symbol, desired_signal_direction);
                    Common::SignalEvent signal_event(event.timestamp, signal);
                    portfolio.generate_order(signal_event); // Portfolio handles sizing/order

                    state.trade_taken = true; // Mark trade taken for this session/day
                    last_signal_direction_[symbol] = desired_signal_direction;
                }
            }
        }
    }; // End class OpeningRangeBreakout

} // namespace Backtester