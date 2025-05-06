#include "../include/backtester/ExecutionSimulator.h"
#include <iostream> // For std::cerr/cout
#include <cmath> // For std::abs
#include <algorithm> // For std::max

namespace Backtester {

    // Implement simulate_order based on the research doc stub logic
    std::optional<Common::FillDetails> ExecutionSimulator::simulate_order(
        const Common::OrderRequest& order,
        const Common::DataSnapshot& current_market_data)
    {
        // Simplified model constants (make these configurable later)
        const double slippage_per_unit = 0.01; // Example: 1 cent slippage per unit price
        const double commission_per_share = 0.005;
        const double min_commission = 1.0;

        // --- Get Market Price ---
        double market_price = 0.0;
        std::string price_key = "Close"; // Default key
         if (current_market_data.count("Close")) {
             market_price = current_market_data.at("Close");
         } else if (current_market_data.count("close")) { // Check lowercase too
             market_price = current_market_data.at("close");
             price_key = "close"; // Store the actual key found
         } else {
             std::cerr << "ExecutionSimulator Error: Market data missing 'Close' price for " << order.symbol << std::endl;
             return std::nullopt; // Cannot simulate without price
         }

        // --- Determine Fill Price ---
        double fill_price = 0.0;
        bool filled = false;

        if (order.order_type == Common::OrderType::MARKET) {
            fill_price = market_price; // Base price
            // Apply simple slippage
            if (order.direction == Common::OrderDirection::BUY) {
                fill_price += slippage_per_unit; // Buy slightly higher
            } else {
                fill_price -= slippage_per_unit; // Sell slightly lower
            }
            // Ensure fill price isn't negative
            fill_price = std::max(0.0, fill_price);
            filled = true;

        } else if (order.order_type == Common::OrderType::LIMIT) {
            if (!order.limit_price.has_value()) {
                 std::cerr << "ExecutionSimulator Error: Limit order for " << order.symbol << " has no limit price." << std::endl;
                 return std::nullopt;
            }
            double limit = order.limit_price.value();
            // Check if limit order can be filled at the limit price or better
            if (order.direction == Common::OrderDirection::BUY && market_price <= limit) {
                // Buy limit fills at the limit price (or market if better, simplified here)
                fill_price = limit;
                filled = true;
            } else if (order.direction == Common::OrderDirection::SELL && market_price >= limit) {
                // Sell limit fills at the limit price (or market if better, simplified here)
                fill_price = limit;
                filled = true;
            } else {
                // Limit order not triggered at current market price
                filled = false;
                 std::cout << "ExecutionSimulator Info: Limit order for " << order.symbol << " not filled (Market: "
                           << market_price << ", Limit: " << limit << ")" << std::endl;
            }
        } else {
            std::cerr << "ExecutionSimulator Error: Unsupported order type for " << order.symbol << std::endl;
            return std::nullopt;
        }

        // --- Calculate Commission ---
        double commission = 0.0;
        if (filled) {
            commission = std::max(min_commission, std::abs(order.quantity) * commission_per_share);

             std::cout << "ExecutionSimulator: Simulating fill for OrderID " << order.order_id << " ("
                       << Common::to_string(order.direction) << " " << order.quantity << " " << order.symbol
                       << ") at price " << fill_price << " (Market was " << market_price << "), Comm: " << commission << std::endl;

            // --- Create FillDetails ---
            // Use current market event timestamp for fill timestamp (simplification)
             auto fill_timestamp = std::chrono::system_clock::now(); // Or pass from Backtester
             // For backtesting, better to use the market event timestamp that triggered the fill
             // We don't have it directly here, Backtester should ideally pass it or handle fill creation


            // Return fill details if filled
            return Common::FillDetails(
                 std::chrono::system_clock::now(), // Placeholder: Use timestamp from triggering market event
                 order.order_id,
                 order.symbol,
                 order.direction,
                 order.quantity, // Assume full fill
                 fill_price,
                 commission
            );
        } else {
            // Return empty optional if not filled
            return std::nullopt;
        }
    }

} // namespace Backtester