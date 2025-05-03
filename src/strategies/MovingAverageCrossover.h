#pragma once

#include "Strategy.h"
#include "core/Event.h" // Use correct path
#include "core/EventQueue.h" // Use correct path
#include "core/Utils.h" // Include for formatting
#include <deque>
#include <vector>
#include <string>
#include <numeric>
#include <iostream>
#include <map>
#include <cmath> // For std::abs

class MovingAverageCrossover : public Strategy {
private:
    // Use size_t for window sizes as they should be non-negative
    size_t short_window_;
    size_t long_window_;

    std::map<std::string, std::deque<double>> price_history_;
    std::map<std::string, double> short_sma_;
    std::map<std::string, double> long_sma_;
    std::map<std::string, SignalDirection> last_signal_;

public:
    // Constructor uses size_t
    MovingAverageCrossover(size_t short_window, size_t long_window)
        : short_window_(short_window), long_window_(long_window) {
        if (short_window_ == 0 || long_window_ <= short_window_) { // Check for 0 window
            throw std::invalid_argument("Invalid window sizes for MovingAverageCrossover");
        }
    }

    void handle_market_event(const MarketEvent& event, EventQueue& queue) override {
        for (const auto& pair : event.data) {
            const std::string& symbol = pair.first;
            const PriceBar& bar = pair.second;
            double price = bar.Close;

            if (price_history_.find(symbol) == price_history_.end()) {
                 price_history_[symbol] = std::deque<double>();
                 short_sma_[symbol] = 0.0; long_sma_[symbol] = 0.0;
                 last_signal_[symbol] = SignalDirection::FLAT;
            }

            std::deque<double>& history = price_history_[symbol];
            history.push_back(price);
            // Use size_t directly for comparison (no more sign warning)
            if (history.size() > long_window_) {
                history.pop_front();
            }

            // Calculate SMAs
            if (history.size() >= short_window_) {
                double short_sum = std::accumulate(history.end() - short_window_, history.end(), 0.0);
                short_sma_[symbol] = short_sum / short_window_;
            } else {
                short_sma_[symbol] = 0.0;
            }

            if (history.size() >= long_window_) {
                double long_sum = std::accumulate(history.begin(), history.end(), 0.0);
                long_sma_[symbol] = long_sum / long_window_;

                SignalDirection current_signal = SignalDirection::FLAT;
                // Add tolerance to avoid flapping on minor differences
                constexpr double tolerance = 1e-9;
                if (short_sma_[symbol] > long_sma_[symbol] + tolerance) {
                     current_signal = SignalDirection::LONG;
                 } else if (short_sma_[symbol] < long_sma_[symbol] - tolerance) {
                     current_signal = SignalDirection::SHORT;
                 }

                if (current_signal != last_signal_[symbol]) {
                     // Use the formatter function for the timestamp
                     std::cout << "CROSSOVER: " << symbol << " @ " << formatTimestampUTC(event.timestamp) // <-- Use formatter
                               << " ShortSMA=" << short_sma_[symbol] << " LongSMA=" << long_sma_[symbol]
                               << " Signal=" << (current_signal == SignalDirection::LONG ? "LONG" : current_signal == SignalDirection::SHORT ? "SHORT" : "FLAT")
                               << std::endl;

                    // Generate Order event (example sizing, no portfolio check yet)
                    OrderDirection direction = (current_signal == SignalDirection::LONG) ? OrderDirection::BUY : OrderDirection::SELL;
                    if (current_signal != SignalDirection::FLAT) {
                        send_event(std::make_shared<OrderEvent>(event.timestamp, symbol, OrderType::MARKET, direction, 100), queue);
                    } else {
                         // TODO: Generate closing order if needed based on portfolio position
                    }
                    last_signal_[symbol] = current_signal;
                }
            } else {
                long_sma_[symbol] = 0.0;
            }
        }
    }
};