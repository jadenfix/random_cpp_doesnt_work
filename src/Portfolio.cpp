#include "../include/backtester/Portfolio.h" // Include self header first

// --- Include FULL definitions needed for implementation ---
#include "../include/common/Event.h"        // Provides FULL SignalEvent, MarketEvent definitions
#include "../include/common/Position.h"
#include "../include/common/FillEvent.h"    // Provides FillDetails
#include "../include/common/OrderRequest.h" // Provides OrderRequest
#include "../include/common/Signal.h"       // Provides Signal struct
#include "../include/common/OrderTypes.h"   // Provides enums
#include "../include/common/Utils.h"          // For formatTimestampUTC

// Standard library includes used in implementation
#include <iostream>
#include <cmath>           // For std::abs
#include <numeric>         // For std::accumulate
#include <optional>        // For std::optional return type
#include <vector>
#include <stdexcept>       // For potential errors
#include <algorithm>       // For std::max, std::min
#include <iomanip>         // For print formatting

namespace Backtester {

    // Constructor implementation
    Portfolio::Portfolio(double initial_capital)
        : initial_capital_(initial_capital), cash_(initial_capital) {
        std::cout << "Portfolio Initialized with cash: $" << std::fixed << std::setprecision(2) << initial_capital << std::endl;
    }

    // update_fill implementation
    void Portfolio::update_fill(const Common::FillDetails& fill) {
        num_fills_++; // Increment fill counter
        total_commission_ += fill.commission; // Accumulate commission
        cash_ -= fill.commission;

        std::string symbol = fill.symbol;
        Common::Position& position = positions_[symbol];
        if (position.symbol.empty()) { position.symbol = symbol; } // Initialize symbol if new

        double transaction_value = fill.quantity * fill.fill_price;
        // Store previous state *before* calling update_on_fill
        double previous_position_rpl = position.realized_pnl; // Store previous RPL for portfolio calculation

        // Delegate detailed position update (AvgPx, quantity, internal RPL)
        position.update_on_fill(fill);

        // Update portfolio cash based on BUY/SELL *after* getting prev state
        if (fill.direction == Common::OrderDirection::BUY) {
            cash_ -= transaction_value;
        } else { // SELL
            cash_ += transaction_value;
        }

        // Accumulate the *change* in the position's realized PnL to the portfolio total
        realized_pnl_ += (position.realized_pnl - previous_position_rpl);

        std::cout << "Portfolio: Updated fill for OrderID " << fill.order_id << " (" << Common::to_string(fill.direction) << " "
                  << fill.quantity << " " << fill.symbol << " @ " << std::fixed << std::setprecision(4) << fill.fill_price << "). " // More precision for price
                  << "Comm: " << std::fixed << std::setprecision(2) << fill.commission << ". New Cash: " << cash_
                  << ". New Pos Qty: " << std::fixed << std::setprecision(4) << position.quantity // Allow fractional display
                  << ". Avg Px: " << std::fixed << std::setprecision(4) << position.average_entry_price
                  << ". Total RPL: " << realized_pnl_ << std::endl;

        // Update market value with fill price immediately
        position.update_market_value(fill.fill_price);
        record_equity(fill.timestamp); // Record equity after fill
    }

    // update_market_value implementation - takes Common::MarketEvent
    void Portfolio::update_market_value(const Common::MarketEvent& event) {
        auto it = positions_.find(event.symbol);
        if (it != positions_.end()) {
            Common::Position& position = it->second;
            std::string price_key = "Close";
            if (!event.marketData.count(price_key) && event.marketData.count("close")) {
                price_key = "close";
            }
            if (event.marketData.count(price_key)) {
                position.update_market_value(event.marketData.at(price_key));
            } else {
                std::cerr << "Warning: MarketEvent for " << event.symbol << " missing '" << price_key << "' price." << std::endl;
            }
        }
        // Record equity AFTER updating market value for the relevant symbol(s)
        record_equity(event.timestamp); // Record equity on market update
    }

    // generate_order implementation - takes Common::SignalEvent
    std::optional<Common::OrderRequest> Portfolio::generate_order(const Common::SignalEvent& signal_event) {
        const Common::Signal& signal = signal_event.signalDetails; // Access nested signal
        Common::Position current_pos = get_position(signal.symbol);
        double target_quantity = 0.0;
        std::optional<Common::OrderDirection> direction_opt;
        const double fixed_order_quantity = 100.0; // Simple fixed size - Replace later

        // Logic to determine order based on signal and current position
        if (signal.direction == Common::SignalDirection::LONG) {
            if (current_pos.quantity < fixed_order_quantity - 1e-9) {
                 target_quantity = fixed_order_quantity - current_pos.quantity;
                 direction_opt = Common::OrderDirection::BUY;
            }
        } else if (signal.direction == Common::SignalDirection::SHORT) {
             if (current_pos.quantity > -fixed_order_quantity + 1e-9) {
                 target_quantity = std::abs(-fixed_order_quantity - current_pos.quantity);
                 direction_opt = Common::OrderDirection::SELL;
             }
        } else if (signal.direction == Common::SignalDirection::FLAT) {
            if (std::abs(current_pos.quantity) > 1e-9) {
                target_quantity = std::abs(current_pos.quantity);
                direction_opt = (current_pos.quantity > 0)? Common::OrderDirection::SELL : Common::OrderDirection::BUY;
            }
        }

        if (direction_opt.has_value() && target_quantity > 1e-9) {
             Common::OrderRequest order_request(
                 signal_event.timestamp,
                 signal.symbol,
                 direction_opt.value(),
                 target_quantity
             );
             std::cout << "Portfolio: Generated MARKET order: " << Common::to_string(order_request.direction)
                       << " " << std::fixed << std::setprecision(4) << order_request.quantity << " " // Allow fractional display
                       << order_request.symbol << std::endl;
             return order_request;
         }
        return std::nullopt;
    }


    // --- Accessor Implementations ---
    double Portfolio::get_total_market_value() const {
        return std::accumulate(positions_.begin(), positions_.end(), 0.0,
           [](double sum, const auto& pair) { return sum + pair.second.market_value; });
    }
     double Portfolio::get_total_unrealized_pnl() const {
         return std::accumulate(positions_.begin(), positions_.end(), 0.0,
            [](double sum, const auto& pair) { return sum + pair.second.unrealized_pnl; });
     }
     double Portfolio::get_total_realized_pnl() const {
         // Return the portfolio-level accumulated RPL
         return realized_pnl_;
     }
    double Portfolio::get_equity() const {
        return cash_ + get_total_market_value();
    }
    // Return default-constructed Position if symbol not found
    Common::Position Portfolio::get_position(const std::string& symbol) const {
        auto it = positions_.find(symbol);
        if (it != positions_.end()) { return it->second; }
        return Common::Position(symbol);
    }

    // --- Equity Recording ---
    void Portfolio::record_equity(const std::chrono::system_clock::time_point& timestamp) {
        if (equity_curve_.empty() || equity_curve_.back().first < timestamp) {
             equity_curve_.emplace_back(timestamp, get_equity());
        } else if (equity_curve_.back().first == timestamp) {
             equity_curve_.back().second = get_equity();
        }
    }

    // --- Performance Metrics Calculation ---
    void Portfolio::calculate_and_print_metrics() const {
        std::cout << "\n--- Performance Metrics ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        if (equity_curve_.empty() && num_fills_ == 0) {
            std::cout << "No equity data or fills recorded. Cannot calculate metrics." << std::endl;
            return;
        }

        double final_equity = get_equity();
        double total_return_pct = (initial_capital_ > 1e-9) ? (((final_equity / initial_capital_) - 1.0) * 100.0) : 0.0;

        std::cout << "Ending Equity:       " << final_equity << std::endl;
        std::cout << "Total Return:        " << total_return_pct << "%" << std::endl;
        std::cout << "Realized PnL:        " << realized_pnl_ << " (Aggregated)" << std::endl; // Use portfolio RPL
        std::cout << "Total Commission:    " << total_commission_ << std::endl;
        std::cout << "Total Fills/Trades:  " << num_fills_ << std::endl;

        // Calculate Max Drawdown
        double peak_equity = initial_capital_;
        double max_drawdown = 0.0;
        if (!equity_curve_.empty()) {
            for (const auto& point : equity_curve_) {
                peak_equity = std::max(peak_equity, point.second);
                max_drawdown = std::max(max_drawdown, peak_equity - point.second);
            }
        } else {
             peak_equity = std::max(initial_capital_, final_equity);
             max_drawdown = std::max(0.0, peak_equity - final_equity);
        }

        double max_drawdown_pct = (peak_equity > 1e-9) ? (max_drawdown / peak_equity) * 100.0 : 0.0;
        std::cout << "Peak Equity Recorded: " << peak_equity << std::endl;
        std::cout << "Max Drawdown:        " << max_drawdown_pct << "%" << std::endl;
        std::cout << "--------------------------" << std::endl;
    }

    // --- Final Summary Printing ---
    void Portfolio::print_final_summary() const {
        std::cout << "\n--- Final Portfolio Summary ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Initial Capital:    " << initial_capital_ << std::endl;
        std::cout << "Ending Cash:     " << cash_ << std::endl;
         double total_market_value = get_total_market_value();
         double total_unrealized_pnl = get_total_unrealized_pnl();
        std::cout << "Market Value:    " << total_market_value << std::endl;
        std::cout << "Unrealized PnL:  " << total_unrealized_pnl << std::endl;
        std::cout << "Ending Positions:" << std::endl;
        bool has_positions = false;
        for (const auto& pair : positions_) {
            if (std::abs(pair.second.quantity) > 1e-9) {
                has_positions = true;
                const auto& pos = pair.second;
                std::cout << "  Symbol: " << std::left << std::setw(30) << pos.symbol << ": " // Wider field
                          << std::right << std::setw(12) << std::fixed << std::setprecision(4) << pos.quantity // More precision
                          << " @ AvgPx " << std::setw(10) << std::fixed << std::setprecision(4) << pos.average_entry_price
                          << " (MV: " << std::fixed << std::setprecision(2) << pos.market_value
                          << ", UPL: " << pos.unrealized_pnl << ", RPL(Pos): " << pos.realized_pnl << ")"
                          << std::endl;
            }
        }
        if (!has_positions) { std::cout << "  (None)" << std::endl; }
        std::cout << "-----------------------------" << std::endl;
        calculate_and_print_metrics(); // Calls metrics calculation
    }

     // --- Method to return results struct (Requires StrategyResult struct) ---
     StrategyResult Portfolio::get_results_summary() const {
         StrategyResult res;
         // ... (Implementation remains the same as the previous answer) ...
         if (equity_curve_.empty() && num_fills_ == 0) {
             res.final_equity = get_equity();
             res.total_return_pct = (initial_capital_ > 1e-9) ? (((res.final_equity / initial_capital_) - 1.0) * 100.0) : 0.0;
             res.realized_pnl = realized_pnl_;
             res.total_commission = total_commission_;
             res.num_fills = num_fills_;
             res.max_drawdown_pct = 0.0;
             return res;
         }
         res.final_equity = get_equity();
         res.total_return_pct = (initial_capital_ > 1e-9) ? (((res.final_equity / initial_capital_) - 1.0) * 100.0) : 0.0;
         res.realized_pnl = realized_pnl_;
         res.total_commission = total_commission_;
         res.num_fills = num_fills_;
         double peak_equity = initial_capital_;
         double max_drawdown = 0.0;
         if (!equity_curve_.empty()) {
             for (const auto& point : equity_curve_) {
                 peak_equity = std::max(peak_equity, point.second);
                 max_drawdown = std::max(max_drawdown, peak_equity - point.second);
             }
         } else {
             peak_equity = std::max(initial_capital_, res.final_equity);
             max_drawdown = std::max(0.0, peak_equity - res.final_equity);
         }
         res.max_drawdown_pct = (peak_equity > 1e-9) ? (max_drawdown / peak_equity) * 100.0 : 0.0;
         return res;
     }

} // namespace Backtester