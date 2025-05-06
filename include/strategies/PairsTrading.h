#pragma once

#include "backtester/Strategy.h"
#include "../common/Event.h"
#include "../common/Signal.h"
#include "../common/Utils.h"
#include "../backtester/Portfolio.h"
#include <string>
#include <stdexcept>
#include <vector>
#include <deque>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map> // Include map for symbol state storage

namespace Backtester {

    class PairsTrading : public StrategyBase {
    private:
        std::string symbol_a_;
        std::string symbol_b_;
        size_t lookback_window_;
        double entry_zscore_threshold_;
        double exit_zscore_threshold_;
        double target_trade_dollar_value_; // Portfolio generate_order handles sizing based on signal

        std::deque<double> ratio_history_;
        double ratio_mean_ = 0.0;
        double ratio_stddev_ = 0.0;

        enum class PairSignalState { FLAT, LONG_A_SHORT_B, SHORT_A_LONG_B };
        PairSignalState current_pair_state_ = PairSignalState::FLAT;

         // Helper to get close price
         bool get_close_price(const Common::DataSnapshot& data, double& close_price) const {
             if (data.count("Close")) { close_price = data.at("Close"); return true; }
             if (data.count("close")) { close_price = data.at("close"); return true; }
             return false;
        }

    public:
        PairsTrading(std::string sym_a, std::string sym_b, size_t lookback = 60,
                     double entry_z = 2.0, double exit_z = 0.5, double trade_value = 10000.0)
            : symbol_a_(std::move(sym_a)), symbol_b_(std::move(sym_b)),
              lookback_window_(lookback), entry_zscore_threshold_(entry_z),
              exit_zscore_threshold_(exit_z), target_trade_dollar_value_(trade_value) // Store for reference if needed, Portfolio handles sizing
              { /* Validation */ }

        // Note: Pairs trading needs data for BOTH symbols in the SAME market event.
        // The current event structure sends one MarketEvent per symbol per timestamp.
        // This strategy needs modification to buffer data or the Backtester needs
        // to send multi-symbol snapshots.
        // --- TEMPORARY WORKAROUND: Assume Backtester sends snapshot via Portfolio state? No.
        // --- Need to rethink event handling or strategy state management ---

        // --- Let's assume the strategy buffers the required data internally ---
        std::map<std::string, double> latest_prices_;

        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
             const std::string& current_symbol = event.symbol;
             double current_price = 0.0;
             if (!get_close_price(event.marketData, current_price)) return; // Skip if no close price

             latest_prices_[current_symbol] = current_price; // Store latest price

             // Check if we have prices for BOTH symbols now
             if (latest_prices_.count(symbol_a_) && latest_prices_.count(symbol_b_)) {
                  double price_a = latest_prices_[symbol_a_];
                  double price_b = latest_prices_[symbol_b_];

                  if (price_a <= 1e-9 || price_b <= 1e-9) return; // Avoid bad prices

                  // --- Calculations based on having both prices ---
                  double current_ratio = price_a / price_b;
                  ratio_history_.push_back(current_ratio);
                  if (ratio_history_.size() > lookback_window_) ratio_history_.pop_front();
                  if (ratio_history_.size() < lookback_window_) return; // Need history

                  double sum = std::accumulate(ratio_history_.begin(), ratio_history_.end(), 0.0);
                  ratio_mean_ = sum / lookback_window_;
                  double sq_sum = 0.0;
                  for (double r : ratio_history_) sq_sum += std::pow(r - ratio_mean_, 2);
                  ratio_stddev_ = (lookback_window_ > 1) ? std::sqrt(sq_sum / (lookback_window_ - 1)) : 0.0;

                  if (ratio_stddev_ < 1e-9) return; // Avoid division by zero

                  double current_zscore = (current_ratio - ratio_mean_) / ratio_stddev_;

                  // Determine desired state
                  PairSignalState desired_state = current_pair_state_;
                  if (current_pair_state_ == PairSignalState::FLAT) {
                      if (current_zscore > entry_zscore_threshold_) desired_state = PairSignalState::SHORT_A_LONG_B;
                      else if (current_zscore < -entry_zscore_threshold_) desired_state = PairSignalState::LONG_A_SHORT_B;
                  } else {
                      if (current_pair_state_ == PairSignalState::SHORT_A_LONG_B && current_zscore < exit_zscore_threshold_) desired_state = PairSignalState::FLAT;
                      else if (current_pair_state_ == PairSignalState::LONG_A_SHORT_B && current_zscore > -exit_zscore_threshold_) desired_state = PairSignalState::FLAT;
                  }

                  // If state changes, generate signals for BOTH legs
                  if (desired_state != current_pair_state_) {
                       std::cout << "PAIRS (" << symbol_a_ << "/" << symbol_b_ << "): Z=" << current_zscore
                                 << " Mean=" << ratio_mean_ << " StdD=" << ratio_stddev_;

                       Common::SignalDirection signal_dir_a = Common::SignalDirection::FLAT;
                       Common::SignalDirection signal_dir_b = Common::SignalDirection::FLAT;

                       if (desired_state == PairSignalState::LONG_A_SHORT_B) {
                            signal_dir_a = Common::SignalDirection::LONG;
                            signal_dir_b = Common::SignalDirection::SHORT;
                            std::cout << " -> Signal: LONG " << symbol_a_ << " / SHORT " << symbol_b_ << std::endl;
                       } else if (desired_state == PairSignalState::SHORT_A_LONG_B) {
                            signal_dir_a = Common::SignalDirection::SHORT;
                            signal_dir_b = Common::SignalDirection::LONG;
                            std::cout << " -> Signal: SHORT " << symbol_a_ << " / LONG " << symbol_b_ << std::endl;
                       } else { // desired_state == FLAT
                            std::cout << " -> Signal: FLAT " << symbol_a_ << " / FLAT " << symbol_b_ << std::endl;
                            // Signal FLAT for both to close positions
                            signal_dir_a = Common::SignalDirection::FLAT;
                            signal_dir_b = Common::SignalDirection::FLAT;
                       }

                       // Send signals to portfolio for order generation
                       Common::Signal signal_a(event.timestamp, symbol_a_, signal_dir_a);
                       Common::SignalEvent signal_event_a(event.timestamp, signal_a);
                       portfolio.generate_order(signal_event_a);

                       Common::Signal signal_b(event.timestamp, symbol_b_, signal_dir_b);
                       Common::SignalEvent signal_event_b(event.timestamp, signal_b);
                       portfolio.generate_order(signal_event_b);

                       current_pair_state_ = desired_state; // Update state
                  }
                  // Clear prices for next tick to ensure fresh data for both
                   latest_prices_.clear();

             } // end if have prices for both
        } // end handle_market_event
    }; // End class

} // namespace Backtester