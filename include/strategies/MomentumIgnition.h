#pragma once

#include "backtester/Strategy.h" // Correct path
#include "common/Event.h"
#include "common/Signal.h"
#include "common/Utils.h"
#include "backtester/Portfolio.h"
#include <string>
#include <map>
#include <deque>
#include <vector>
#include <numeric>
#include <limits>
#include <iostream>
#include <cmath>
#include <algorithm> // For std::max/min
#include <stdexcept>

namespace Backtester {

    class MomentumIgnition : public StrategyBase {
    private:
        size_t price_breakout_window_;
        size_t volume_avg_window_;
        double volume_multiplier_;
        size_t return_delta_window_;
        // Sizing handled by Portfolio

        struct SymbolState {
            std::deque<double> close_history;
            std::deque<double> high_history;
            std::deque<double> low_history;
            std::deque<double> volume_history;
        };
        std::map<std::string, SymbolState> symbol_state_;
        std::map<std::string, Common::SignalDirection> last_signal_direction_;

         bool get_ohlcv(const Common::DataSnapshot& data, double& o, double& h, double& l, double& c, double& v) const {
             // ... (helper function as before) ...
             bool found_all = true;
             auto get_val = [&](const std::string& key_upper, const std::string& key_lower, double& out_val) {
                 if (data.count(key_upper)) out_val = data.at(key_upper);
                 else if (data.count(key_lower)) out_val = data.at(key_lower);
                 else { out_val = 0.0; found_all = false; }
             };
             get_val("Open", "open", o); get_val("High", "high", h); get_val("Low", "low", l);
             get_val("Close", "close", c); get_val("Volume", "volume", v);
             // Require H, L, C, V for this strategy
             return (data.count("High") || data.count("high")) && (data.count("Low") || data.count("low")) &&
                    (data.count("Close") || data.count("close")) && (data.count("Volume") || data.count("volume"));
         }

    public:
        MomentumIgnition(size_t price_window = 5, size_t vol_window = 10, double vol_mult = 2.0, size_t ret_window = 3)
            : price_breakout_window_(price_window), volume_avg_window_(vol_window),
              volume_multiplier_(vol_mult), return_delta_window_(ret_window) {
            if (price_window == 0 || vol_window == 0 || vol_mult <= 0 || ret_window == 0) {
                 throw std::invalid_argument("Invalid parameters for MomentumIgnition");
            }
        }

        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
            const std::string& symbol = event.symbol;
            double open, high, low, close, volume;

            if (!get_ohlcv(event.marketData, open, high, low, close, volume)) {
                 std::cerr << "Warning (Momentum): Missing OHLCV data for symbol " << symbol << std::endl;
                 return;
            }

            // Update state
            SymbolState& state = symbol_state_[symbol];
            size_t max_lookback = std::max({price_breakout_window_, volume_avg_window_, return_delta_window_}) + 1;
            state.close_history.push_back(close);
            state.high_history.push_back(high);
            state.low_history.push_back(low);
            state.volume_history.push_back(volume);
            while(state.close_history.size() > max_lookback) state.close_history.pop_front();
            while(state.high_history.size() > max_lookback) state.high_history.pop_front();
            while(state.low_history.size() > max_lookback) state.low_history.pop_front();
            while(state.volume_history.size() > max_lookback) state.volume_history.pop_front();

            if (state.close_history.size() < max_lookback) return; // Need enough history

            // Condition Checks
            bool price_breakout_up = false, price_breakout_down = false;
            double recent_high = -std::numeric_limits<double>::max();
            double recent_low = std::numeric_limits<double>::max();
            for (size_t i = 0; i < price_breakout_window_; ++i) {
                 size_t index = state.high_history.size() - 2 - i;
                 recent_high = std::max(recent_high, state.high_history[index]);
                 recent_low = std::min(recent_low, state.low_history[index]);
            }
            if (close > recent_high) price_breakout_up = true;
            if (close < recent_low) price_breakout_down = true;

            double current_volume = volume;
            double volume_sum = 0.0;
            for (size_t i = 0; i < volume_avg_window_; ++i) {
                 size_t index = state.volume_history.size() - 2 - i;
                 volume_sum += state.volume_history[index];
            }
            double avg_volume = (volume_avg_window_ > 0) ? volume_sum / volume_avg_window_ : 0.0;
            bool volume_surge = current_volume > (volume_multiplier_ * avg_volume) && avg_volume > 1e-9;

            double return_delta_sum = 0.0;
            if (state.close_history.size() >= return_delta_window_ + 1) {
                for (size_t i = 0; i < return_delta_window_; ++i) {
                    size_t index_now = state.close_history.size() - 1 - i;
                    size_t index_prev = index_now - 1;
                    if (std::abs(state.close_history[index_prev]) > 1e-9) {
                         return_delta_sum += (state.close_history[index_now] / state.close_history[index_prev]) - 1.0;
                    }
                }
            }
            bool positive_delta = return_delta_sum > 1e-9; // Use tolerance
            bool negative_delta = return_delta_sum < -1e-9; // Use tolerance

            // Signal Logic
            Common::SignalDirection desired_signal_direction = Common::SignalDirection::FLAT;
            if (price_breakout_up && volume_surge && positive_delta) desired_signal_direction = Common::SignalDirection::LONG;
            else if (price_breakout_down && volume_surge && negative_delta) desired_signal_direction = Common::SignalDirection::SHORT;

            // Generate Signal Event if State Changes
            if (desired_signal_direction != last_signal_direction_[symbol]) {
                 if (desired_signal_direction != Common::SignalDirection::FLAT || last_signal_direction_[symbol] != Common::SignalDirection::FLAT) {
                     // ***** CORRECTED NAMESPACE *****
                     std::cout << "MOMENTUM IGNITION: " << symbol << " @ " << Utils::formatTimestampUTC(event.timestamp)
                               << " PriceBreakUp=" << price_breakout_up << " PriceBreakDown=" << price_breakout_down
                               << " VolSurge=" << volume_surge << " RetDelta=" << return_delta_sum
                               << " Signal=" << Common::to_string(desired_signal_direction)
                               << std::endl;

                     Common::Signal signal(event.timestamp, symbol, desired_signal_direction);
                     Common::SignalEvent signal_event(event.timestamp, signal);
                     portfolio.generate_order(signal_event);
                 }
                 last_signal_direction_[symbol] = desired_signal_direction; // Update state
            }
        } // end handle_market_event

    }; // <--- ADDED MISSING SEMICOLON HERE

} // namespace Backtester