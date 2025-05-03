#pragma once

#include "Event.h" // Includes PriceBar.h indirectly via Event.h using DataSnapshot
#include "Utils.h" // Include for formatting
#include <string>
#include <map>
#include <chrono>
#include <vector>
#include <numeric> // For std::accumulate
#include <iostream>
#include <iomanip> // For formatting output
#include <cmath>   // For std::abs
#include <limits>  // For std::numeric_limits
#include <algorithm> // For std::max

class Portfolio {
public: // Make Position public so methods can access it easily

    // --- Ensure this struct definition is present and correct ---
    struct Position {
        double quantity = 0.0;       // Can be negative for short positions
        double average_price = 0.0;  // Average entry price (cost basis)
        double market_value = 0.0;   // Current market value based on last price
        double unrealized_pnl = 0.0; // Unrealized profit/loss based on avg_price and market_value
    };
    // --- End of Position struct definition ---

private:
    double initial_cash_ = 100000.0;
    double current_cash_ = 0.0;
    // Map storing positions, keyed by symbol string
    std::map<std::string, Position> positions_;
    double total_commission_ = 0.0;
    double realized_pnl_ = 0.0; // Accumulated realized PnL
    long num_fills_ = 0;       // Counter for number of fills processed
    // Stores pairs of (timestamp, total_equity)
    std::vector<std::pair<std::chrono::system_clock::time_point, double>> equity_curve_;

public:
    // Constructor
    explicit Portfolio(double initial_cash = 100000.0)
        : initial_cash_(initial_cash), current_cash_(initial_cash) {}

    // Update portfolio state based on a fill event
    void handle_fill_event(const FillEvent& event) {
        num_fills_++;
        total_commission_ += event.commission;
        current_cash_ -= event.commission;

        // Get reference to position for the symbol (creates if doesn't exist)
        Position& pos = positions_[event.symbol]; // <-- Accesses Position struct members below
        double position_change_value = event.quantity * event.fill_price;
        double previous_quantity = pos.quantity;        // <-- Needs Position::quantity
        double previous_avg_price = pos.average_price;  // <-- Needs Position::average_price

        // --- Simple Realized PnL Logic (Attempt when crossing zero) ---
        bool closing_long = (previous_quantity > 1e-9 && (previous_quantity - event.quantity) < 1e-9 && event.direction == OrderDirection::SELL);
        bool closing_short = (previous_quantity < -1e-9 && (previous_quantity + event.quantity) > -1e-9 && event.direction == OrderDirection::BUY);

        if (closing_long || closing_short) {
             double quantity_closed = std::min(std::abs(previous_quantity), event.quantity);
             double pnl_per_share = (event.direction == OrderDirection::SELL) ? (event.fill_price - previous_avg_price) : (previous_avg_price - event.fill_price);
             double realized_for_this_trade = quantity_closed * pnl_per_share;
             realized_pnl_ += realized_for_this_trade;
             std::cout << "PORTFOLIO: Realized PnL on close: " << realized_for_this_trade << std::endl;
        }
        // --- End Simple Realized PnL ---

        // Update cash and quantity
        if (event.direction == OrderDirection::BUY) {
            current_cash_ -= position_change_value;
            pos.quantity += event.quantity; // <-- Needs Position::quantity
        } else { // SELL
            current_cash_ += position_change_value;
            pos.quantity -= event.quantity; // <-- Needs Position::quantity
        }

        // Update average price (simplified logic)
        if (std::abs(pos.quantity) < 1e-9) { // Position is now flat
             pos.average_price = 0.0; // <-- Needs Position::average_price
        } else {
             // Complex logic - simple approximation for now
             if ( (event.direction == OrderDirection::BUY && previous_quantity >= -1e-9) || (event.direction == OrderDirection::SELL && previous_quantity <= 1e-9) ) {
                 if (std::abs(previous_quantity) > 1e-9) { // Adding to existing or opening
                     pos.average_price = ( (std::abs(previous_quantity) * previous_avg_price) + std::abs(position_change_value) ) / std::abs(pos.quantity); // <-- Needs Position members
                 } else {
                      pos.average_price = event.fill_price; // <-- Needs Position::average_price
                 }
             } else if ( (event.direction == OrderDirection::BUY && previous_quantity < -1e-9) || (event.direction == OrderDirection::SELL && previous_quantity > 1e-9)) {
                 pos.average_price = event.fill_price; // <-- Needs Position::average_price
             }
        }

        std::cout << "PORTFOLIO: Fill - Sym: " << event.symbol
                  << ", Dir: " << (event.direction == OrderDirection::BUY ? "BUY" : "SELL")
                  << ", Qty: " << event.quantity << " @ " << event.fill_price
                  << ", NewPos: " << pos.quantity << ", AvgPx: " << pos.average_price << ", Cash: " << current_cash_ << std::endl; // <-- Needs Position members

        // Update market value immediately using fill price as current price estimate
        update_market_values({{event.symbol, PriceBar{event.timestamp, 0,0,0, event.fill_price, 0}}});
        record_equity(event.timestamp);
    }

    // Update market value of all holdings based on new market data snapshot
    void update_market_values(const DataSnapshot& current_data) {
        for (auto& pair : positions_) {
            const std::string& symbol = pair.first;
            Position& pos = pair.second; // Get reference to the Position struct

            if (std::abs(pos.quantity) < 1e-9) { // <-- Check Position::quantity
                pos.market_value = 0.0;   // <-- Update Position::market_value
                pos.unrealized_pnl = 0.0; // <-- Update Position::unrealized_pnl
                continue;
            }

            auto it = current_data.find(symbol);
            if (it != current_data.end()) {
                double current_price = it->second.Close; // Use closing price for marking
                pos.market_value = pos.quantity * current_price; // <-- Update Position::market_value

                // Calculate unrealized PnL
                double cost_basis = pos.quantity * pos.average_price; // <-- Needs Position members
                pos.unrealized_pnl = pos.market_value - cost_basis;   // <-- Update Position::unrealized_pnl
            }
        }
    }

     // Record total portfolio value at a specific time
     void record_equity(const std::chrono::system_clock::time_point& timestamp) {
         double total_market_value = 0.0;
         for(const auto& pair : positions_) {
             total_market_value += pair.second.market_value; // <-- Needs Position::market_value
         }
         double total_equity = current_cash_ + total_market_value;
         if (equity_curve_.empty() || equity_curve_.back().first != timestamp) {
              equity_curve_.emplace_back(timestamp, total_equity);
         } else {
              equity_curve_.back().second = total_equity; // Update latest value for this timestamp
         }
     }

    // --- Getters ---
    double get_current_cash() const { return current_cash_; }
    double get_total_equity() const {
         double total_market_value = 0.0;
         for(const auto& pair : positions_) {
             total_market_value += pair.second.market_value; // <-- Needs Position::market_value
         }
        return current_cash_ + total_market_value;
    }
    const std::map<std::string, Position>& get_positions() const { return positions_; }
    const std::vector<std::pair<std::chrono::system_clock::time_point, double>>& get_equity_curve() const { return equity_curve_; }
    double get_total_commission() const { return total_commission_; }

    double get_position_quantity(const std::string& symbol) const {
         auto it = positions_.find(symbol);
         if (it != positions_.end()) {
             return it->second.quantity; // <-- Needs Position::quantity
         }
         return 0.0;
     }

    // --- Performance Metrics Calculation ---
    void calculate_and_print_metrics() const {
        std::cout << "\n--- Performance Metrics ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        if (equity_curve_.empty()) {
            std::cout << "No equity data recorded." << std::endl; return;
        }
        double final_equity = get_total_equity();
        double total_return_pct = ((final_equity / initial_cash_) - 1.0) * 100.0;
        std::cout << "Ending Equity:       " << final_equity << std::endl;
        std::cout << "Total Return:        " << total_return_pct << "%" << std::endl;
        std::cout << "Realized PnL:        " << realized_pnl_ << " (Simplified)" << std::endl;
        std::cout << "Total Commission:    " << total_commission_ << std::endl;
        std::cout << "Total Fills/Trades:  " << num_fills_ << std::endl;

        double peak_equity = initial_cash_;
        double max_drawdown_pct = 0.0;
        for (const auto& point : equity_curve_) {
            peak_equity = std::max(peak_equity, point.second);
            double current_drawdown = (peak_equity > 1e-9) ? (peak_equity - point.second) / peak_equity : 0.0;
            max_drawdown_pct = std::max(max_drawdown_pct, current_drawdown);
        }
        std::cout << "Peak Equity Recorded: " << peak_equity << std::endl;
        std::cout << "Max Drawdown:        " << max_drawdown_pct * 100.0 << "%" << std::endl;
        std::cout << "--------------------------" << std::endl;
    }

    // --- Final Summary Printing ---
    void print_final_summary() const {
        std::cout << "\n--- Final Portfolio Summary ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Initial Cash:    " << initial_cash_ << std::endl;
        std::cout << "Ending Cash:     " << current_cash_ << std::endl;
         double total_market_value = 0.0;
         double total_unrealized_pnl = 0.0;
         for(const auto& pair : positions_) {
             total_market_value += pair.second.market_value;   // <-- Needs Position::market_value
             total_unrealized_pnl += pair.second.unrealized_pnl; // <-- Needs Position::unrealized_pnl
         }
        std::cout << "Market Value:    " << total_market_value << std::endl;
        std::cout << "Unrealized PnL:  " << total_unrealized_pnl << std::endl;
        std::cout << "Ending Positions:" << std::endl;
        bool has_positions = false;
        for (const auto& pair : positions_) {
            if (std::abs(pair.second.quantity) > 1e-9) { // <-- Needs Position::quantity
                has_positions = true;
                std::cout << "  " << std::left << std::setw(30) << pair.first << ": "
                          << std::right << std::setw(10) << pair.second.quantity   // <-- Needs Position::quantity
                          << " @ Avg Cost " << std::setw(8) << pair.second.average_price // <-- Needs Position::average_price
                          << " (MV: " << pair.second.market_value << ", UPL: " << pair.second.unrealized_pnl << ")" // <-- Needs Position members
                          << std::endl;
            }
        }
        if (!has_positions) { std::cout << "  (None)" << std::endl; }
        std::cout << "-----------------------------" << std::endl;
        calculate_and_print_metrics(); // Call metrics calculation
    }
}; // End of Portfolio class definition