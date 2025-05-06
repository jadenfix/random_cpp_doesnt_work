#pragma once

#include "backtester/Strategy.h"        // Base class StrategyBase (Corrected include path)
#include "common/Event.h"         // Needs MarketEvent, DataSnapshot (Corrected include path)
#include "common/Signal.h"        // Needs Signal struct definition (Corrected include path)
#include "common/Utils.h"         // For formatTimestampUTC (Corrected include path)
#include "backtester/Portfolio.h" // Needs Portfolio class definition for interaction (Corrected include path)
#include <deque>
#include <vector>
#include <string>
#include <numeric>
#include <iostream>
#include <map>
#include <cmath>
#include <stdexcept> // For invalid_argument

// Assuming all common headers like OrderTypes.h are included via the above headers

namespace Backtester {

    class MovingAverageCrossover : public StrategyBase {
    private:
        size_t short_window_;
        size_t long_window_;
        // Portfolio handles sizing based on signal

        std::map<std::string, std::deque<double>> price_history_;
        std::map<std::string, double> short_sma_;
        std::map<std::string, double> long_sma_;
        std::map<std::string, Common::SignalDirection> last_signal_direction_;

        // Helper to get close price, handling case variations
        bool get_close_price(const Common::DataSnapshot& data, double& close_price) const {
             // Check both common capitalizations for the key
             if (data.count("Close")) { close_price = data.at("Close"); return true; }
             if (data.count("close")) { close_price = data.at("close"); return true; }
             return false; // Return false if neither key is found
        }

    public:
        MovingAverageCrossover(size_t short_window, size_t long_window)
            : short_window_(short_window), long_window_(long_window) {
            if (short_window_ == 0 || long_window_ <= short_window_) {
                throw std::invalid_argument("Invalid window sizes for MovingAverageCrossover");
            }
        }

        // Override the virtual function from StrategyBase
        void handle_market_event(const Common::MarketEvent& event, Portfolio& portfolio) override {
            double price = 0.0;
            // Try to get the close price from the market data snapshot
            if (!get_close_price(event.marketData, price)) {
                 std::cerr << "Warning (MACrossover): No 'Close' price found for symbol " << event.symbol
                           << " at timestamp " << Utils::formatTimestampUTC(event.timestamp) << std::endl;
                 return; // Cannot proceed without a closing price
            }

            const std::string& symbol = event.symbol;

            // Initialize state map entries for a symbol if seen for the first time
            if (price_history_.find(symbol) == price_history_.end()) {
                price_history_[symbol]; // Creates an empty deque
                short_sma_[symbol] = 0.0;
                long_sma_[symbol] = 0.0;
                last_signal_direction_[symbol] = Common::SignalDirection::FLAT; // Default state is flat
            }

            // Update historical price data
            std::deque<double>& history = price_history_[symbol];
            history.push_back(price);
            // Keep the history buffer trimmed to the required maximum lookback size
            if (history.size() > long_window_) {
                history.pop_front();
            }

            // Calculate Short SMA if enough history is available
            if (history.size() >= short_window_) {
                // Calculate sum of the last 'short_window_' elements
                double short_sum = std::accumulate(history.end() - short_window_, history.end(), 0.0);
                short_sma_[symbol] = short_sum / short_window_;
            } else {
                // Not enough data yet for short SMA, cannot proceed further
                return;
            }

            // Calculate Long SMA if enough history is available
            if (history.size() >= long_window_) {
                // Use the full history deque for the long SMA sum
                double long_sum = std::accumulate(history.begin(), history.end(), 0.0);
                long_sma_[symbol] = long_sum / long_window_;

                // Determine desired signal based on SMA crossover
                Common::SignalDirection desired_signal_direction = Common::SignalDirection::FLAT;
                constexpr double tolerance = 1e-9; // Tolerance for floating point comparison noise

                if (short_sma_[symbol] > long_sma_[symbol] + tolerance) {
                    desired_signal_direction = Common::SignalDirection::LONG;
                } else if (short_sma_[symbol] < long_sma_[symbol] - tolerance) {
                    desired_signal_direction = Common::SignalDirection::SHORT;
                }
                // If they are very close (within tolerance), signal remains FLAT

                // Generate signal ONLY if the desired direction changes from the last recorded one
                if (desired_signal_direction != last_signal_direction_[symbol]) {

                    // --- CORRECTED NAMESPACE for Utility Function ---
                    std::cout << "CROSSOVER: " << symbol << " @ " << Utils::formatTimestampUTC(event.timestamp) // Use Utils:: directly
                              << " ShortSMA=" << std::fixed << std::setprecision(4) << short_sma_[symbol]
                              << " LongSMA=" << std::fixed << std::setprecision(4) << long_sma_[symbol]
                              << " Signal=" << Common::to_string(desired_signal_direction) // Common::to_string is correct
                              << std::endl;
                    // --- END CORRECTION ---

                    // Create a Signal struct payload
                    Common::Signal signal(event.timestamp, symbol, desired_signal_direction);
                    // Create a SignalEvent (wraps the Signal struct)
                    Common::SignalEvent signal_event(event.timestamp, signal);

                    // Ask the Portfolio component to generate an order based on this signal
                    // The Portfolio implementation will handle sizing, risk checks, etc.
                    portfolio.generate_order(signal_event);

                    // Update the last recorded signal direction for this symbol
                    last_signal_direction_[symbol] = desired_signal_direction;
                }
            } // end if enough history for long SMA
        } // end handle_market_event

    }; // End class MovingAverageCrossover

} // End namespace Backtester