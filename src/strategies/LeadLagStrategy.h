#pragma once

#include "Strategy.h"
#include "core/Event.h"
#include "core/EventQueue.h"
#include "core/Utils.h"
#include "core/Portfolio.h"
#include <string>
#include <vector>
#include <deque>
#include <numeric> // For std::accumulate, std::inner_product
#include <cmath>   // For std::sqrt, std::abs, std::pow
#include <iostream>
#include <map>
#include <stdexcept>

class LeadLagStrategy : public Strategy {
private:
    // --- Parameters ---
    std::string leading_symbol_;
    std::string lagging_symbol_;
    size_t correlation_window_;  // Window for rolling correlation
    size_t lag_period_;          // How many bars back to look at leader (typically 1)
    double correlation_threshold_; // Minimum correlation to consider the signal valid
    double leader_return_threshold_; // Minimum abs return of leader to trigger signal
    double target_position_size_;  // Size to trade in the lagging symbol

    // --- State ---
    // Store returns: deque<(leader_return, lagger_return)>
    std::deque<std::pair<double, double>> return_history_;
    // Store recent close prices to calculate returns
    std::map<std::string, double> last_close_price_;
    // Current signal state for the lagging symbol
    SignalDirection current_signal_state_ = SignalDirection::FLAT;

public:
    LeadLagStrategy(std::string leader, std::string lagger,
                    size_t corr_window = 30, size_t lag = 1,
                    double corr_thresh = 0.6, double leader_ret_thresh = 0.0005, // 0.05%
                    double target_size = 100.0)
        : leading_symbol_(std::move(leader)), lagging_symbol_(std::move(lagger)),
          correlation_window_(corr_window), lag_period_(lag),
          correlation_threshold_(corr_thresh), leader_return_threshold_(leader_ret_thresh),
          target_position_size_(target_size)
    {
        if (corr_window <= lag || lag == 0 || corr_thresh < -1.0 || corr_thresh > 1.0 || leader_ret_thresh < 0 || target_size <= 0) {
            throw std::invalid_argument("Invalid parameters for LeadLagStrategy");
        }
         if (leading_symbol_ == lagging_symbol_) {
             throw std::invalid_argument("Lead/Lag symbols cannot be the same");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        if (!portfolio_) return;

        auto it_leader = event.data.find(leading_symbol_);
        auto it_lagger = event.data.find(lagging_symbol_);

        // Need data for both symbols
        if (it_leader == event.data.end() || it_lagger == event.data.end()) {
            return;
        }

        double leader_close = it_leader->second.Close;
        double lagger_close = it_lagger->second.Close;

        // Calculate returns (handle first observation)
        double leader_return = 0.0;
        double lagger_return = 0.0;

        if (last_close_price_.count(leading_symbol_)) {
            if (last_close_price_[leading_symbol_] > 1e-9) {
                leader_return = (leader_close / last_close_price_[leading_symbol_]) - 1.0;
            }
        }
         if (last_close_price_.count(lagging_symbol_)) {
            if (last_close_price_[lagging_symbol_] > 1e-9) {
                 lagger_return = (lagger_close / last_close_price_[lagging_symbol_]) - 1.0;
            }
        }

        // Update last prices
        last_close_price_[leading_symbol_] = leader_close;
        last_close_price_[lagging_symbol_] = lagger_close;

        // Store return pair (only if both returns are validly calculated)
        if (last_close_price_.size() >= 2) { // Ensure we had previous prices for both
             return_history_.emplace_back(leader_return, lagger_return);
             if (return_history_.size() > correlation_window_ + lag_period_) { // Keep buffer large enough
                 return_history_.pop_front();
             }
        }

        // Need enough history for correlation calculation
        if (return_history_.size() < correlation_window_ + lag_period_) {
            return;
        }

        // --- Calculate Rolling Lagged Correlation ---
        // Correlation between lagger's return now and leader's return 'lag_period_' bars ago
        double correlation = calculate_lagged_correlation(lag_period_);

        // --- Get Leader's Lagged Return ---
        double leader_lagged_return = return_history_[return_history_.size() - 1 - lag_period_].first;

        // --- Signal Generation ---
        SignalDirection desired_signal = SignalDirection::FLAT;

        if (correlation > correlation_threshold_) { // Positive correlation implies lagger follows leader
            if (leader_lagged_return > leader_return_threshold_) {
                desired_signal = SignalDirection::LONG; // Leader went up, expect lagger to go up
            } else if (leader_lagged_return < -leader_return_threshold_) {
                desired_signal = SignalDirection::SHORT; // Leader went down, expect lagger to go down
            }
        }
        // Can add logic for negative correlation if needed (desired_signal would be opposite)


        // --- Generate Orders for Lagging Symbol ---
        if (desired_signal != current_signal_state_) {
            std::cout << "LEADLAG (" << leading_symbol_ << "->" << lagging_symbol_ << "): "
                      << " @ " << formatTimestampUTC(event.timestamp)
                      << " Corr=" << correlation << " LeadRet(" << lag_period_ << ")=" << leader_lagged_return
                      << " Signal=" << (desired_signal == SignalDirection::LONG ? "LONG" : desired_signal == SignalDirection::SHORT ? "SHORT" : "FLAT")
                      << std::endl;

            double target_quantity = 0.0;
            if (desired_signal == SignalDirection::LONG) target_quantity = target_position_size_;
            else if (desired_signal == SignalDirection::SHORT) target_quantity = -target_position_size_;

            double current_quantity = portfolio_->get_position_quantity(lagging_symbol_); // Trade the lagger
            double order_quantity_needed = target_quantity - current_quantity;

            if (std::abs(order_quantity_needed) > 1e-9) {
                OrderDirection direction = (order_quantity_needed > 0) ? OrderDirection::BUY : OrderDirection::SELL;
                double quantity_to_order = std::abs(order_quantity_needed);

                std::cout << " -> Target(" << lagging_symbol_ << "): " << target_quantity << ", Current: " << current_quantity
                          << ", Order Qty: " << quantity_to_order << " " << (direction==OrderDirection::BUY?"BUY":"SELL") << std::endl;

                send_event(std::make_shared<OrderEvent>(event.timestamp, lagging_symbol_, OrderType::MARKET, direction, quantity_to_order), queue);
            } else {
                 std::cout << " -> Target(" << lagging_symbol_ << "): " << target_quantity << ", Current: " << current_quantity << ". No order needed." << std::endl;
            }
            current_signal_state_ = desired_signal;
        }
    } // end handle_market_event


private:
    // Helper to calculate rolling Pearson correlation between
    // current lagger returns and lagged leader returns
    double calculate_lagged_correlation(size_t lag) {
        if (return_history_.size() < correlation_window_ + lag) {
            return 0.0; // Not enough data
        }

        // Extract the relevant series from history
        std::vector<double> lagger_returns_now; // Current returns of lagger
        std::vector<double> leader_returns_lagged; // Lagged returns of leader

        lagger_returns_now.reserve(correlation_window_);
        leader_returns_lagged.reserve(correlation_window_);

        // Iterate over the window ending at the most recent data point
        for (size_t i = 0; i < correlation_window_; ++i) {
            size_t current_index = return_history_.size() - 1 - i;
            size_t lagged_index = current_index - lag;

            lagger_returns_now.push_back(return_history_[current_index].second); // Lagger return at time t-i
            leader_returns_lagged.push_back(return_history_[lagged_index].first); // Leader return at time t-i-lag
        }

        // Reverse vectors because we added them backwards
         std::reverse(lagger_returns_now.begin(), lagger_returns_now.end());
         std::reverse(leader_returns_lagged.begin(), leader_returns_lagged.end());


        // Calculate means
        double mean_lagger = std::accumulate(lagger_returns_now.begin(), lagger_returns_now.end(), 0.0) / correlation_window_;
        double mean_leader_lagged = std::accumulate(leader_returns_lagged.begin(), leader_returns_lagged.end(), 0.0) / correlation_window_;

        // Calculate standard deviations and covariance (using std::inner_product for covariance sum)
        double cov_sum = 0.0;
        double stddev_lagger_sq_sum = 0.0;
        double stddev_leader_lagged_sq_sum = 0.0;

        for (size_t i = 0; i < correlation_window_; ++i) {
             double lagger_dev = lagger_returns_now[i] - mean_lagger;
             double leader_lagged_dev = leader_returns_lagged[i] - mean_leader_lagged;
             cov_sum += lagger_dev * leader_lagged_dev;
             stddev_lagger_sq_sum += std::pow(lagger_dev, 2);
             stddev_leader_lagged_sq_sum += std::pow(leader_lagged_dev, 2);
        }


        // Calculate correlation coefficient
        double stddev_lagger = std::sqrt(stddev_lagger_sq_sum);
        double stddev_leader_lagged = std::sqrt(stddev_leader_lagged_sq_sum);

        if (stddev_lagger < 1e-9 || stddev_leader_lagged < 1e-9) {
            return 0.0; // Avoid division by zero if standard deviation is negligible
        }

        return cov_sum / (stddev_lagger * stddev_leader_lagged);
    }

}; // End Class