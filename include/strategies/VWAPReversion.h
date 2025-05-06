#pragma once

#include "backtester/Strategy.h"    // Correct path to base class
#include "common/Event.h"         // Correct path
#include "common/Signal.h"        // Correct path
#include "common/Utils.h"         // Correct path
#include "backtester/Portfolio.h" // Correct path
#include <string>
#include <map>
#include <vector>
#include <cmath>
#include <numeric>
#include <iostream>
#include <stdexcept>

namespace Backtester {

    class VWAPReversion : public StrategyBase { // Inherit from StrategyBase
    private:
        double deviation_multiplier_;
        // Sizing handled by Portfolio

        struct SymbolState {
            double cumulative_price_volume = 0.0;
            double cumulative_volume = 0.0;
            double current_vwap = 0.0;
            // For stddev calc (optional)
            // std::vector<double> price_vwap_diffs;
            // int rolling_stddev_window = 20;
        };
        std::map<std::string, SymbolState> symbol_state_;
        std::map<std::string, Common::SignalDirection> last_signal_direction_; // Renamed for clarity

        // Helper to get OHLCV prices
         bool get_ohlcv(const Common::DataSnapshot& data, double& o, double& h, double& l, double& c, double& v) const {
             bool found_all = true;
             auto get_val = [&](const std::string& key_upper, const std::string& key_lower, double& out_val) {
                 if (data.count(key_upper)) out_val = data.at(key_upper);
                 else if (data.count(key_lower)) out_val = data.at(key_lower);
                 else { out_val = 0.0; found_all = false; } // Default to 0 if missing
             };
             get_val("Open", "open", o);
             get_val("High", "high", h);
             get_val("Low", "low", l);
             get_val("Close", "close", c);
             get_val("Volume", "volume", v);
             // Only require Close and Volume to be present for this strategy
             return data.count("Close") || data.count("close") || data.count("Volume") || data.count("volume");
        }


    public:
        VWAPReversion(double deviation_multiplier = 2.0)
            : deviation_multiplier_(deviation_multiplier) {
             if (deviation_multiplier_ <= 0) {
                  throw std::invalid_argument("Deviation multiplier must be positive for VWAPReversion");
             }
        }

        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
            const std::string& symbol = event.symbol;
            double open, high, low, close, volume; // 'open' unused here

            if (!get_ohlcv(event.marketData, open, high, low, close, volume)) {
                 // Allow processing if at least Close and Volume are present
                 if (!event.marketData.count("Close") && !event.marketData.count("close")) {
                     std::cerr << "Warning (VWAP): Missing Close price for " << symbol << std::endl; return;
                 }
                 if (!event.marketData.count("Volume") && !event.marketData.count("volume")) {
                      std::cerr << "Warning (VWAP): Missing Volume for " << symbol << std::endl; return;
                 }
                 // Get the ones we have (might be partial)
                 get_ohlcv(event.marketData, open, high, low, close, volume);
            }

            if (volume < 1e-9) return; // Skip zero volume bars

            SymbolState& state = symbol_state_[symbol]; // Get/create state
            // Use typical price if H/L available, otherwise fallback to Close
            double typical_price = (high > 1e-9 && low > 1e-9) ? (high + low + close) / 3.0 : close;
            state.cumulative_price_volume += typical_price * volume;
            state.cumulative_volume += volume;
            state.current_vwap = (state.cumulative_volume > 1e-9) ? (state.cumulative_price_volume / state.cumulative_volume) : typical_price;

            // Placeholder Standard Deviation
            double standard_deviation = close * 0.005; // Example: 0.5% of price - NEEDS REPLACEMENT

            // Signal Generation
            Common::SignalDirection desired_signal_direction = Common::SignalDirection::FLAT;
            double upper_band = state.current_vwap + deviation_multiplier_ * standard_deviation;
            double lower_band = state.current_vwap - deviation_multiplier_ * standard_deviation;

            if (close > upper_band) desired_signal_direction = Common::SignalDirection::SHORT;
            else if (close < lower_band) desired_signal_direction = Common::SignalDirection::LONG;
            else { // Exit logic: Cross back over VWAP
                 if (last_signal_direction_[symbol] == Common::SignalDirection::SHORT && close < state.current_vwap) desired_signal_direction = Common::SignalDirection::FLAT;
                 else if (last_signal_direction_[symbol] == Common::SignalDirection::LONG && close > state.current_vwap) desired_signal_direction = Common::SignalDirection::FLAT;
                 // Else, stay in existing trade if between bands and not crossing VWAP exit
                 else desired_signal_direction = last_signal_direction_[symbol]; // Maintain position
            }

            // Generate signal event if direction changes
            if (desired_signal_direction != last_signal_direction_[symbol]) {
                 // ***** CORRECTED NAMESPACE *****
                 std::cout << "VWAP REVERSION: " << symbol << " @ " << Utils::formatTimestampUTC(event.timestamp) // Use Utils:: directly
                           << " Close=" << close << " VWAP=" << state.current_vwap
                           << " Signal=" << Common::to_string(desired_signal_direction)
                           << std::endl;

                Common::Signal signal(event.timestamp, symbol, desired_signal_direction);
                Common::SignalEvent signal_event(event.timestamp, signal);
                portfolio.generate_order(signal_event);

                last_signal_direction_[symbol] = desired_signal_direction;
            }
        }
    }; // End class VWAPReversion

} // namespace Backtester