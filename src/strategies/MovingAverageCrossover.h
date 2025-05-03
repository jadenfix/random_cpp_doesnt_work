#pragma once

#include "Strategy.h"
#include "core/Event.h"
#include "core/EventQueue.h"
#include "core/Utils.h"
#include "core/Portfolio.h" // Make sure Portfolio is included
#include <deque>
#include <vector>
#include <string>
#include <numeric>
#include <iostream>
#include <map>
#include <cmath> // For std::abs

class MovingAverageCrossover : public Strategy {
private:
    size_t short_window_;
    size_t long_window_;
    double target_position_size_ = 100.0; // --- NEW: Target size for long/short ---

    std::map<std::string, std::deque<double>> price_history_;
    std::map<std::string, double> short_sma_;
    std::map<std::string, double> long_sma_;
    std::map<std::string, SignalDirection> current_signal_state_; // Renamed from last_signal_

public:
    MovingAverageCrossover(size_t short_window, size_t long_window, double target_pos_size = 100.0)
        : short_window_(short_window), long_window_(long_window), target_position_size_(target_pos_size)
    {
        if (short_window_ == 0 || long_window_ <= short_window_ || target_position_size_ <= 0) {
            throw std::invalid_argument("Invalid parameters for MovingAverageCrossover");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        if (!portfolio_) { // Safety check
            std::cerr << "ERROR: Portfolio not set for strategy!" << std::endl;
            return;
        }

        for (const auto& pair : event.data) {
            const std::string& symbol = pair.first;
            const PriceBar& bar = pair.second;
            double price = bar.Close;

            // Initialize state if symbol is new
            if (price_history_.find(symbol) == price_history_.end()) {
                price_history_[symbol]; // Creates deque
                short_sma_[symbol] = 0.0; long_sma_[symbol] = 0.0;
                current_signal_state_[symbol] = SignalDirection::FLAT;
            }

            // Update history and SMAs
            std::deque<double>& history = price_history_[symbol];
            history.push_back(price);
            if (history.size() > long_window_) { history.pop_front(); }

            if (history.size() >= short_window_) {
                double short_sum = std::accumulate(history.end() - short_window_, history.end(), 0.0);
                short_sma_[symbol] = short_sum / short_window_;
            } else continue; // Need short window data

            if (history.size() >= long_window_) {
                double long_sum = std::accumulate(history.begin(), history.end(), 0.0);
                long_sma_[symbol] = long_sum / long_window_;

                // Determine desired signal based on SMA crossover
                SignalDirection desired_signal = SignalDirection::FLAT;
                constexpr double tolerance = 1e-9;
                if (short_sma_[symbol] > long_sma_[symbol] + tolerance) {
                    desired_signal = SignalDirection::LONG;
                } else if (short_sma_[symbol] < long_sma_[symbol] - tolerance) {
                    desired_signal = SignalDirection::SHORT;
                }

                // --- Generate Orders Based on Target Position ---
                if (desired_signal != current_signal_state_[symbol]) {
                    std::cout << "CROSSOVER: " << symbol << " @ " << formatTimestampUTC(event.timestamp)
                              << " ShortSMA=" << short_sma_[symbol] << " LongSMA=" << long_sma_[symbol]
                              << " Signal=" << (desired_signal == SignalDirection::LONG ? "LONG" : desired_signal == SignalDirection::SHORT ? "SHORT" : "FLAT")
                              << std::endl;

                    double target_quantity = 0.0;
                    if (desired_signal == SignalDirection::LONG) {
                        target_quantity = target_position_size_;
                    } else if (desired_signal == SignalDirection::SHORT) {
                        target_quantity = -target_position_size_;
                    } // else target is 0.0 for FLAT

                    // Get current position from portfolio
                    double current_quantity = portfolio_->get_position_quantity(symbol);
                    double order_quantity_needed = target_quantity - current_quantity;

                    // Only send order if quantity needed is significant
                    if (std::abs(order_quantity_needed) > 1e-9) {
                        OrderDirection direction = (order_quantity_needed > 0) ? OrderDirection::BUY : OrderDirection::SELL;
                        double quantity_to_order = std::abs(order_quantity_needed);

                         std::cout << " -> Target: " << target_quantity << ", Current: " << current_quantity
                                   << ", Order Qty: " << quantity_to_order << " " << (direction==OrderDirection::BUY?"BUY":"SELL") << std::endl;

                        send_event(std::make_shared<OrderEvent>(event.timestamp, symbol, OrderType::MARKET, direction, quantity_to_order), queue);
                    } else {
                         std::cout << " -> Target: " << target_quantity << ", Current: " << current_quantity << ". No order needed." << std::endl;
                    }
                    current_signal_state_[symbol] = desired_signal; // Update state *after* decision
                }
            } // end if history.size >= long_window
        } // end loop over symbols in market event
    } // end handle_market_event
};