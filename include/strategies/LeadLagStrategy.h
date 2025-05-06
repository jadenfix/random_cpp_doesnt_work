#pragma once

#include "backtester/Strategy.h" // Correct path
#include "common/Event.h"
#include "common/Signal.h"
#include "common/Utils.h"
#include "backtester/Portfolio.h"
#include <string>
#include <vector>
#include <deque>
#include <numeric>
#include <cmath>
#include <iostream>
#include <map>
#include <stdexcept>
#include <algorithm> // For std::reverse

namespace Backtester {

    class LeadLagStrategy : public StrategyBase { // Inherit from StrategyBase
    private:
        std::string leading_symbol_;
        std::string lagging_symbol_;
        size_t correlation_window_;
        size_t lag_period_;
        double correlation_threshold_;
        double leader_return_threshold_;
        // Sizing handled by Portfolio

        // State Management
        struct PriceInfo {
            double close = 0.0;
            double previous_close = 0.0; // Store previous close to calculate return
            bool has_current = false;
        };
        std::map<std::string, PriceInfo> latest_prices_; // Tracks latest prices for pair
        std::deque<std::pair<double, double>> return_history_; // (leader_return, lagger_return)

        // Track signal state for the LAGGING symbol
        std::map<std::string, Common::SignalDirection> last_signal_direction_;

         // Helper to get close price
         bool get_close_price(const Common::DataSnapshot& data, double& close_price) const {
             if (data.count("Close")) { close_price = data.at("Close"); return true; }
             if (data.count("close")) { close_price = data.at("close"); return true; }
             return false;
        }

    public:
        LeadLagStrategy(std::string leader, std::string lagger,
                        size_t corr_window = 30, size_t lag = 1,
                        double corr_thresh = 0.6, double leader_ret_thresh = 0.0002)
            : leading_symbol_(std::move(leader)), lagging_symbol_(std::move(lagger)),
              correlation_window_(corr_window), lag_period_(lag),
              correlation_threshold_(corr_thresh), leader_return_threshold_(leader_ret_thresh)
              { /* Validation */ }

        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
            const std::string& current_symbol = event.symbol;
            double current_close = 0.0;
            if (!get_close_price(event.marketData, current_close)) return;

            // Update the price info for the current symbol, storing the previous close
            PriceInfo& info = latest_prices_[current_symbol]; // Get/create entry
            info.previous_close = info.close; // Store the last known close as previous
            info.close = current_close;       // Update with current close
            info.has_current = true;          // Mark that we have data for this tick

            // Check if we have updated data for BOTH symbols in this logical time step
            if (latest_prices_.count(leading_symbol_) && latest_prices_[leading_symbol_].has_current &&
                latest_prices_.count(lagging_symbol_) && latest_prices_[lagging_symbol_].has_current)
            {
                 // Calculate returns using previous_close stored in the PriceInfo struct
                 double leader_return = 0.0;
                 double lagger_return = 0.0;
                 if (latest_prices_[leading_symbol_].previous_close > 1e-9) {
                      leader_return = (latest_prices_[leading_symbol_].close / latest_prices_[leading_symbol_].previous_close) - 1.0;
                 }
                  if (latest_prices_[lagging_symbol_].previous_close > 1e-9) {
                      lagger_return = (latest_prices_[lagging_symbol_].close / latest_prices_[lagging_symbol_].previous_close) - 1.0;
                 }

                 // Store return pair
                 return_history_.emplace_back(leader_return, lagger_return);
                 if (return_history_.size() > correlation_window_ + lag_period_) {
                     return_history_.pop_front();
                 }

                 // Calculate correlation if enough history
                 if (return_history_.size() >= correlation_window_ + lag_period_) {
                     double correlation = calculate_lagged_correlation(lag_period_);
                     double leader_lagged_return = return_history_[return_history_.size() - 1 - lag_period_].first;

                     // Signal Generation
                     Common::SignalDirection desired_signal = Common::SignalDirection::FLAT;
                     if (correlation > correlation_threshold_) {
                         if (leader_lagged_return > leader_return_threshold_) desired_signal = Common::SignalDirection::LONG;
                         else if (leader_lagged_return < -leader_return_threshold_) desired_signal = Common::SignalDirection::SHORT;
                     }
                     // Add logic for negative correlation if desired

                     // Generate Orders for Lagging Symbol if signal changes
                     std::string signal_key = lagging_symbol_;
                     if (desired_signal != last_signal_direction_[signal_key]) {
                         // ***** CORRECTED NAMESPACE *****
                          std::cout << "LEADLAG (" << leading_symbol_ << "->" << lagging_symbol_ << "): "
                                    << " @ " << Utils::formatTimestampUTC(event.timestamp) // Use Utils::
                                    << " Corr=" << correlation << " LeadRet(" << lag_period_ << ")=" << leader_lagged_return
                                    << " Signal=" << Common::to_string(desired_signal) << std::endl;

                         Common::Signal signal(event.timestamp, lagging_symbol_, desired_signal);
                         Common::SignalEvent signal_event(event.timestamp, signal);
                         portfolio.generate_order(signal_event);

                         last_signal_direction_[signal_key] = desired_signal;
                     }
                 } // End if enough history for correlation

                // Mark prices as 'used' for this time step calculation
                 latest_prices_[leading_symbol_].has_current = false;
                 latest_prices_[lagging_symbol_].has_current = false;
            } // end if have prices for both
        } // end handle_market_event

    private:
         double calculate_lagged_correlation(size_t lag) { /* ... implementation same as before ... */
             if (return_history_.size() < correlation_window_ + lag) return 0.0;
             std::vector<double> lagger_returns_now, leader_returns_lagged;
             lagger_returns_now.reserve(correlation_window_); leader_returns_lagged.reserve(correlation_window_);
             for (size_t i = 0; i < correlation_window_; ++i) {
                 size_t current_index = return_history_.size() - 1 - i; size_t lagged_index = current_index - lag;
                 lagger_returns_now.push_back(return_history_[current_index].second);
                 leader_returns_lagged.push_back(return_history_[lagged_index].first);
             }
             std::reverse(lagger_returns_now.begin(), lagger_returns_now.end());
             std::reverse(leader_returns_lagged.begin(), leader_returns_lagged.end());
             double mean_lagger = std::accumulate(lagger_returns_now.begin(), lagger_returns_now.end(), 0.0) / correlation_window_;
             double mean_leader_lagged = std::accumulate(leader_returns_lagged.begin(), leader_returns_lagged.end(), 0.0) / correlation_window_;
             double cov_sum = 0.0, stddev_lagger_sq_sum = 0.0, stddev_leader_lagged_sq_sum = 0.0;
             for (size_t i = 0; i < correlation_window_; ++i) {
                  double lagger_dev = lagger_returns_now[i] - mean_lagger; double leader_lagged_dev = leader_returns_lagged[i] - mean_leader_lagged;
                  cov_sum += lagger_dev * leader_lagged_dev;
                  stddev_lagger_sq_sum += std::pow(lagger_dev, 2); stddev_leader_lagged_sq_sum += std::pow(leader_lagged_dev, 2);
             }
             double stddev_lagger = std::sqrt(stddev_lagger_sq_sum); double stddev_leader_lagged = std::sqrt(stddev_leader_lagged_sq_sum);
             if (stddev_lagger < 1e-9 || stddev_leader_lagged < 1e-9) return 0.0;
             return cov_sum / (stddev_lagger * stddev_leader_lagged);
         }

    }; // End Class LeadLagStrategy

} // namespace Backtester