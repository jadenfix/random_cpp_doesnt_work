#pragma once
#include <string>
#include <cmath>
#include <algorithm> // Include for std::min/max if needed
#include "FillEvent.h" // Needs FillDetails
#include "OrderTypes.h" // Needs OrderDirection

namespace Backtester::Common {
    struct Position {
        std::string symbol;
        double quantity = 0.0;
        double average_entry_price = 0.0;
        double last_price = 0.0;
        double market_value = 0.0;
        double unrealized_pnl = 0.0;
        double realized_pnl = 0.0;

        Position(std::string sym = "") : symbol(std::move(sym)) {}

        // Use the detailed update_on_fill logic from the research doc
        void update_on_fill(const FillDetails& fill) { /* ... copy from research doc ... */
            double fill_cost = fill.quantity * fill.fill_price;
            double previous_quantity = quantity; // Store before modification
            double previous_average_entry_price = average_entry_price; // Store before modification

            if (fill.direction == OrderDirection::BUY) {
                if (previous_quantity < -1e-9 && (previous_quantity + fill.quantity) > 1e-9) { // Crossing zero (closing short, opening long)
                    realized_pnl += std::abs(previous_quantity) * (previous_average_entry_price - fill.fill_price);
                    average_entry_price = fill.fill_price;
                } else if (previous_quantity >= -1e-9) { // Adding to long or opening long from flat (check >= to handle initial buy)
                    if (std::abs(previous_quantity + fill.quantity) > 1e-9) { // Avoid division by zero
                        double current_total_cost = previous_quantity * previous_average_entry_price;
                        average_entry_price = (current_total_cost + fill_cost) / (previous_quantity + fill.quantity);
                    } else {
                        average_entry_price = fill.fill_price; // Initial buy
                    }
                } else { // Reducing short position
                     realized_pnl += fill.quantity * (previous_average_entry_price - fill.fill_price);
                }
                quantity += fill.quantity;
            } else { // SELL
                if (previous_quantity > 1e-9 && (previous_quantity - fill.quantity) < -1e-9) { // Crossing zero (closing long, opening short)
                    realized_pnl += previous_quantity * (fill.fill_price - previous_average_entry_price);
                    average_entry_price = fill.fill_price;
                } else if (previous_quantity <= 1e-9) { // Adding to short or opening short from flat (check <= to handle initial sell)
                     if (std::abs(previous_quantity - fill.quantity) > 1e-9) { // Avoid division by zero
                        double current_total_value = std::abs(previous_quantity) * previous_average_entry_price;
                        average_entry_price = (current_total_value + fill_cost) / std::abs(previous_quantity - fill.quantity);
                     } else {
                         average_entry_price = fill.fill_price; // Initial sell
                     }
                } else { // Reducing long position
                     realized_pnl += fill.quantity * (fill.fill_price - previous_average_entry_price);
                }
                quantity -= fill.quantity;
            }
            realized_pnl -= fill.commission;
            if (std::abs(quantity) < 1e-9) { average_entry_price = 0.0; }
            update_market_value(last_price); // Update immediately based on *last known* price
        }

        void update_market_value(double current_price) { /* ... copy from research doc ... */
             last_price = current_price;
             market_value = quantity * current_price;
             if (std::abs(quantity) > 1e-9) {
                  unrealized_pnl = quantity * (current_price - average_entry_price);
             } else {
                 unrealized_pnl = 0.0;
             }
        }
    };
}