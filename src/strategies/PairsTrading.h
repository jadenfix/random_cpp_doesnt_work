#pragma once

#include "Strategy.h"
#include "core/Event.h"
#include "core/EventQueue.h"
#include "core/Utils.h"
#include "core/Portfolio.h"
#include <string>
#include <stdexcept>
#include <vector>
#include <deque>
#include <numeric> // For std::accumulate
#include <cmath>   // For std::sqrt, std::pow, std::abs
#include <iostream>

class PairsTrading : public Strategy {
private:
    // --- Parameters ---
    std::string symbol_a_; // First symbol in the pair
    std::string symbol_b_; // Second symbol in the pair
    size_t lookback_window_; // For calculating mean/std dev of ratio
    double entry_zscore_threshold_; // Z-score to enter a trade
    double exit_zscore_threshold_;  // Z-score to exit a trade
    double target_trade_dollar_value_; // Target $ value for each leg

    // --- State ---
    std::deque<double> ratio_history_; // Stores recent PriceA / PriceB ratios
    double ratio_mean_ = 0.0;
    double ratio_stddev_ = 0.0;
    SignalDirection current_signal_state_ = SignalDirection::FLAT; // FLAT, LONG_A_SHORT_B, SHORT_A_LONG_B

    // Helper enum for pair state
    enum class PairSignal {
        FLAT,
        LONG_A_SHORT_B, // Buy A, Sell B (Ratio expected to increase)
        SHORT_A_LONG_B  // Sell A, Buy B (Ratio expected to decrease)
    };
    PairSignal current_pair_signal_ = PairSignal::FLAT;


public:
    PairsTrading(std::string sym_a, std::string sym_b,
                   size_t lookback = 30, double entry_z = 2.0, double exit_z = 0.5,
                   double trade_value = 10000.0) // Trade $10k per leg by default
        : symbol_a_(std::move(sym_a)), symbol_b_(std::move(sym_b)),
          lookback_window_(lookback), entry_zscore_threshold_(entry_z),
          exit_zscore_threshold_(exit_z), target_trade_dollar_value_(trade_value)
    {
        if (lookback == 0 || entry_z <= exit_z || entry_z <= 0 || exit_z < 0 || trade_value <= 0) {
             throw std::invalid_argument("Invalid parameters for PairsTrading");
        }
        if (symbol_a_ == symbol_b_) {
             throw std::invalid_argument("Pair symbols cannot be the same");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        if (!portfolio_) return;

        // Find data for both symbols in the current snapshot
        auto it_a = event.data.find(symbol_a_);
        auto it_b = event.data.find(symbol_b_);

        // Need data for *both* symbols to calculate the ratio
        if (it_a == event.data.end() || it_b == event.data.end()) {
            // std::cout << "Pairs: Missing data for one or both symbols this tick." << std::endl;
            return; // Skip if either symbol is missing data
        }

        const PriceBar& bar_a = it_a->second;
        const PriceBar& bar_b = it_b->second;

        // Use closing prices for ratio calculation
        double price_a = bar_a.Close;
        double price_b = bar_b.Close;

        if (price_a <= 1e-9 || price_b <= 1e-9) {
             // std::cout << "Pairs: Zero or invalid price encountered." << std::endl;
             return; // Avoid division by zero or using invalid prices
        }

        // --- Calculate Ratio and Update History ---
        double current_ratio = price_a / price_b;
        ratio_history_.push_back(current_ratio);
        if (ratio_history_.size() > lookback_window_) {
            ratio_history_.pop_front();
        }

        // Need enough history to calculate stats
        if (ratio_history_.size() < lookback_window_) {
            // std::cout << "Pairs: Gathering history..." << std::endl;
            return;
        }

        // --- Calculate Mean and Standard Deviation ---
        double sum = std::accumulate(ratio_history_.begin(), ratio_history_.end(), 0.0);
        ratio_mean_ = sum / lookback_window_;

        double sq_sum = 0.0;
        for (double ratio : ratio_history_) {
            sq_sum += std::pow(ratio - ratio_mean_, 2);
        }
        // Use sample standard deviation (N-1), requires lookback >= 2
        ratio_stddev_ = (lookback_window_ > 1) ? std::sqrt(sq_sum / (lookback_window_ - 1)) : 0.0;

        if (ratio_stddev_ < 1e-9) {
            // std::cout << "Pairs: Zero std dev, cannot calculate Z-score." << std::endl;
             return; // Avoid division by zero if std dev is effectively zero
        }

        // --- Calculate Z-Score ---
        double current_zscore = (current_ratio - ratio_mean_) / ratio_stddev_;

        // --- Determine Desired Signal State ---
        PairSignal desired_signal = current_pair_signal_; // Assume no change initially

        if (current_pair_signal_ == PairSignal::FLAT) {
            // Entry conditions
            if (current_zscore > entry_zscore_threshold_) {
                desired_signal = PairSignal::SHORT_A_LONG_B; // Ratio is high, short A, long B
            } else if (current_zscore < -entry_zscore_threshold_) {
                desired_signal = PairSignal::LONG_A_SHORT_B; // Ratio is low, long A, short B
            }
        } else {
            // Exit conditions (reversion towards mean)
            if (current_pair_signal_ == PairSignal::SHORT_A_LONG_B && current_zscore < exit_zscore_threshold_) {
                 desired_signal = PairSignal::FLAT; // Exit short A / long B spread
            } else if (current_pair_signal_ == PairSignal::LONG_A_SHORT_B && current_zscore > -exit_zscore_threshold_) {
                 desired_signal = PairSignal::FLAT; // Exit long A / short B spread
            }
        }

        // --- Generate Orders if State Changes ---
        if (desired_signal != current_pair_signal_) {
            std::cout << "PAIRS (" << symbol_a_ << "/" << symbol_b_ << "): " << " @ " << formatTimestampUTC(event.timestamp)
                      << " Ratio=" << current_ratio << " Mean=" << ratio_mean_ << " Z=" << current_zscore
                      << " Signal=";
            if(desired_signal == PairSignal::FLAT) std::cout << "FLAT";
            else if(desired_signal == PairSignal::LONG_A_SHORT_B) std::cout << "LONG_A_SHORT_B";
            else std::cout << "SHORT_A_LONG_B";
            std::cout << std::endl;

            // --- Calculate Order Quantities to reach target state ---
            double target_qty_a = 0.0, target_qty_b = 0.0;
            double current_qty_a = portfolio_->get_position_quantity(symbol_a_);
            double current_qty_b = portfolio_->get_position_quantity(symbol_b_);

            if (desired_signal == PairSignal::LONG_A_SHORT_B) {
                target_qty_a = target_trade_dollar_value_ / price_a; // Approx shares for target $ value
                target_qty_b = -target_trade_dollar_value_ / price_b;
            } else if (desired_signal == PairSignal::SHORT_A_LONG_B) {
                target_qty_a = -target_trade_dollar_value_ / price_a;
                target_qty_b = target_trade_dollar_value_ / price_b;
            } // else target is 0 for FLAT

            double order_qty_a = target_qty_a - current_qty_a;
            double order_qty_b = target_qty_b - current_qty_b;

            // Send orders for each leg if needed
            if (std::abs(order_qty_a) > 1e-9) { // Use a small tolerance
                 OrderDirection dir_a = (order_qty_a > 0) ? OrderDirection::BUY : OrderDirection::SELL;
                 std::cout << " -> Order A (" << symbol_a_ << "): " << std::abs(order_qty_a) << " " << (dir_a == OrderDirection::BUY ? "BUY" : "SELL") << std::endl;
                 send_event(std::make_shared<OrderEvent>(event.timestamp, symbol_a_, OrderType::MARKET, dir_a, std::abs(order_qty_a)), queue);
            }
             if (std::abs(order_qty_b) > 1e-9) {
                 OrderDirection dir_b = (order_qty_b > 0) ? OrderDirection::BUY : OrderDirection::SELL;
                 std::cout << " -> Order B (" << symbol_b_ << "): " << std::abs(order_qty_b) << " " << (dir_b == OrderDirection::BUY ? "BUY" : "SELL") << std::endl;
                 send_event(std::make_shared<OrderEvent>(event.timestamp, symbol_b_, OrderType::MARKET, dir_b, std::abs(order_qty_b)), queue);
            }

            current_pair_signal_ = desired_signal; // Update the state
        } // end if signal changed
    } // end handle_market_event
};