#pragma once

#include "Strategy.h"
#include "core/Event.h"
#include "core/EventQueue.h"
#include "core/Utils.h"
#include "core/Portfolio.h"
#include <string>
#include <map>
#include <deque>    // For rolling windows
#include <vector>
#include <numeric>  // For std::accumulate
#include <limits>   // For numeric_limits
#include <iostream>
#include <cmath>    // For std::abs

class MomentumIgnition : public Strategy {
private:
    // --- Parameters ---
    size_t price_breakout_window_ = 5;   // Look back 5 bars for high/low breakout
    size_t volume_avg_window_ = 10;  // Rolling average volume window
    double volume_multiplier_ = 2.0;   // Volume must be > 2x average
    size_t return_delta_window_ = 3;   // Look back 3 bars for positive/negative return delta
    double target_position_size_ = 100.0;

    // --- State per Symbol ---
    struct SymbolState {
        std::deque<PriceBar> history; // Store recent bars for price/volume/return checks
    };
    std::map<std::string, SymbolState> symbol_state_;
    std::map<std::string, SignalDirection> current_signal_state_;

public:
    MomentumIgnition(size_t price_window = 5, size_t vol_window = 10, double vol_mult = 2.0, size_t ret_window = 3, double target_pos_size = 100.0)
        : price_breakout_window_(price_window), volume_avg_window_(vol_window),
          volume_multiplier_(vol_mult), return_delta_window_(ret_window), target_position_size_(target_pos_size)
    {
        if (price_window == 0 || vol_window == 0 || vol_mult <= 0 || ret_window == 0 || target_pos_size <= 0) {
             throw std::invalid_argument("Invalid parameters for MomentumIgnition");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        if (!portfolio_) return;

        size_t max_lookback = std::max({price_breakout_window_, volume_avg_window_, return_delta_window_}) + 1; // Max history needed + current bar

        for (const auto& pair : event.data) {
            const std::string& symbol = pair.first;
            const PriceBar& current_bar = pair.second;

            // Initialize or update history
            SymbolState& state = symbol_state_[symbol];
            state.history.push_back(current_bar);
            if (state.history.size() > max_lookback) {
                state.history.pop_front();
            }

            // Need enough history to perform calculations
            if (state.history.size() < max_lookback) {
                continue;
            }

            // --- Condition Checks ---
            bool price_breakout_up = false;
            bool price_breakout_down = false;
            double recent_high = -std::numeric_limits<double>::max();
            double recent_low = std::numeric_limits<double>::max();
            // Check previous N bars (excluding current)
            for (size_t i = 0; i < price_breakout_window_; ++i) {
                 size_t index = state.history.size() - 2 - i; // Index of previous bars
                 recent_high = std::max(recent_high, state.history[index].High);
                 recent_low = std::min(recent_low, state.history[index].Low);
            }
            if (current_bar.Close > recent_high) price_breakout_up = true;
            if (current_bar.Close < recent_low) price_breakout_down = true;


            double current_volume = static_cast<double>(current_bar.Volume);
            double volume_sum = 0.0;
            // Average volume of previous N bars
             for (size_t i = 0; i < volume_avg_window_; ++i) {
                 size_t index = state.history.size() - 2 - i;
                 volume_sum += static_cast<double>(state.history[index].Volume);
             }
            double avg_volume = (volume_avg_window_ > 0) ? volume_sum / volume_avg_window_ : 0.0;
            bool volume_surge = current_volume > (volume_multiplier_ * avg_volume) && avg_volume > 0; // Avoid division by zero


            double return_delta = 0.0;
            // Sum of returns over last N bars (including current)
            if (state.history.size() >= return_delta_window_ + 1) { // Need N+1 bars for N returns
                for (size_t i = 0; i < return_delta_window_; ++i) {
                    size_t index_now = state.history.size() - 1 - i;
                    size_t index_prev = index_now - 1;
                    if (state.history[index_prev].Close > 1e-9) { // Avoid division by zero
                         return_delta += (state.history[index_now].Close / state.history[index_prev].Close) - 1.0;
                    }
                }
            }
            bool positive_delta = return_delta > 0;
            bool negative_delta = return_delta < 0;


            // --- Signal Logic ---
            SignalDirection desired_signal = SignalDirection::FLAT;
            if (price_breakout_up && volume_surge && positive_delta) {
                desired_signal = SignalDirection::LONG;
            } else if (price_breakout_down && volume_surge && negative_delta) {
                desired_signal = SignalDirection::SHORT;
            }
             // Add exit logic? Currently only enters/flips.

            // --- Generate Orders ---
            if (desired_signal != current_signal_state_[symbol] && desired_signal != SignalDirection::FLAT) {
                std::cout << "MOMENTUM IGNITION: " << symbol << " @ " << formatTimestampUTC(event.timestamp)
                          << " PriceBreakUp=" << price_breakout_up << " PriceBreakDown=" << price_breakout_down
                          << " VolSurge=" << volume_surge << " RetDelta=" << return_delta
                          << " Signal=" << (desired_signal == SignalDirection::LONG ? "LONG" : "SHORT")
                          << std::endl;

                double target_quantity = (desired_signal == SignalDirection::LONG) ? target_position_size_ : -target_position_size_;
                double current_quantity = portfolio_->get_position_quantity(symbol);
                double order_quantity_needed = target_quantity - current_quantity;

                if (std::abs(order_quantity_needed) > 1e-9) {
                    OrderDirection direction = (order_quantity_needed > 0) ? OrderDirection::BUY : OrderDirection::SELL;
                    double quantity_to_order = std::abs(order_quantity_needed);

                    std::cout << " -> Target: " << target_quantity << ", Current: " << current_quantity
                              << ", Order Qty: " << quantity_to_order << " " << (direction==OrderDirection::BUY?"BUY":"SELL") << std::endl;

                    send_event(std::make_shared<OrderEvent>(event.timestamp, symbol, OrderType::MARKET, direction, quantity_to_order), queue);
                    current_signal_state_[symbol] = desired_signal;
                }
            } else if (desired_signal == SignalDirection::FLAT && current_signal_state_[symbol] != SignalDirection::FLAT) {
                // If conditions no longer met, flatten position
                 double current_quantity = portfolio_->get_position_quantity(symbol);
                 if (std::abs(current_quantity) > 1e-9) {
                      std::cout << "MOMENTUM IGNITION EXIT: " << symbol << " @ " << formatTimestampUTC(event.timestamp) << " Flattening position." << std::endl;
                      OrderDirection direction = (current_quantity > 0) ? OrderDirection::SELL : OrderDirection::BUY;
                      send_event(std::make_shared<OrderEvent>(event.timestamp, symbol, OrderType::MARKET, direction, std::abs(current_quantity)), queue);
                      current_signal_state_[symbol] = SignalDirection::FLAT;
                 }
            }
        } // end loop over symbols
    } // end handle_market_event
};