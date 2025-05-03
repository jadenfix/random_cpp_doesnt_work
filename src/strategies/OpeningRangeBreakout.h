#pragma once

#include "Strategy.h"
#include "core/Event.h"
#include "core/EventQueue.h"
#include "core/Utils.h"
#include "core/Portfolio.h"
#include <string>
#include <map>
#include <chrono>
#include <limits> // For numeric_limits
#include <iostream>
#include <cmath> // For std::abs

class OpeningRangeBreakout : public Strategy {
private:
    // --- Parameters ---
    int opening_range_minutes_ = 30; // Duration of ORB period
    double target_position_size_ = 100.0;
    // Add filter parameters later (e.g., volume multiplier)

    // --- State per Symbol ---
    struct SymbolState {
        std::chrono::system_clock::time_point start_time; // Market open time (approx)
        double range_high = -std::numeric_limits<double>::max();
        double range_low = std::numeric_limits<double>::max();
        bool range_established = false;
    };
    std::map<std::string, SymbolState> symbol_state_;
    std::map<std::string, SignalDirection> current_signal_state_; // Track LONG/SHORT/FLAT breakout state

public:
    OpeningRangeBreakout(int range_minutes = 30, double target_pos_size = 100.0)
        : opening_range_minutes_(range_minutes), target_position_size_(target_pos_size)
    {
        if (opening_range_minutes_ <= 0 || target_pos_size <= 0) {
             throw std::invalid_argument("Invalid parameters for OpeningRangeBreakout");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        if (!portfolio_) return;

        auto current_timestamp = event.timestamp;

        for (const auto& pair : event.data) {
            const std::string& symbol = pair.first;
            const PriceBar& bar = pair.second;

            // Initialize state (detects first bar for the symbol)
            if (symbol_state_.find(symbol) == symbol_state_.end()) {
                symbol_state_[symbol].start_time = current_timestamp;
                symbol_state_[symbol].range_high = bar.High; // Initial range
                symbol_state_[symbol].range_low = bar.Low;
                current_signal_state_[symbol] = SignalDirection::FLAT;
            }

            SymbolState& state = symbol_state_[symbol];
            auto time_since_start = std::chrono::duration_cast<std::chrono::minutes>(current_timestamp - state.start_time);

            // --- Phase 1: Establish Opening Range ---
            if (!state.range_established) {
                if (time_since_start.count() <= opening_range_minutes_) {
                    // Update range high and low during the period
                    state.range_high = std::max(state.range_high, bar.High);
                    state.range_low = std::min(state.range_low, bar.Low);
                } else {
                    // Opening range period finished
                    state.range_established = true;
                    std::cout << "ORB ESTABLISHED: " << symbol << " @ " << formatTimestampUTC(current_timestamp)
                              << " High=" << state.range_high << " Low=" << state.range_low << std::endl;
                }
            }

            // --- Phase 2: Trade Breakouts ---
            if (state.range_established) {
                SignalDirection desired_signal = current_signal_state_[symbol]; // Default to current state

                // Check for breakout
                if (bar.Close > state.range_high) {
                    desired_signal = SignalDirection::LONG;
                } else if (bar.Close < state.range_low) {
                    desired_signal = SignalDirection::SHORT;
                }
                // Note: No exit logic defined here (e.g., EOD close, trailing stop) - only entries/flips

                // --- Generate Orders ---
                if (desired_signal != current_signal_state_[symbol] && desired_signal != SignalDirection::FLAT) {
                    // Only trade on the *first* breakout signal after range established
                     std::cout << "ORB BREAKOUT: " << symbol << " @ " << formatTimestampUTC(event.timestamp)
                               << " Close=" << bar.Close << " Range=[" << state.range_low << ", " << state.range_high << "]"
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
                        current_signal_state_[symbol] = desired_signal; // Update state only after sending order
                    }
                 }
             } // end if range established
         } // end loop over symbols
     } // end handle_market_event
};