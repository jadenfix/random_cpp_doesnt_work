#pragma once

#include "Event.h"
#include "Utils.h" // Include for formatting
#include <string>
#include <map>
#include <chrono>
#include <vector>
#include <numeric>
#include <iostream>
#include <iomanip>
#include <cmath> // For std::abs

class Portfolio {
public:
    struct Position {
        double quantity = 0.0;
        double average_price = 0.0;
        double market_value = 0.0;
        double unrealized_pnl = 0.0;
    };

private:
    double initial_cash_ = 100000.0;
    double current_cash_ = 0.0;
    std::map<std::string, Position> positions_;
    double total_commission_ = 0.0;
    double realized_pnl_ = 0.0; // Simplified
    std::vector<std::pair<std::chrono::system_clock::time_point, double>> equity_curve_;

public:
    explicit Portfolio(double initial_cash = 100000.0)
        : initial_cash_(initial_cash), current_cash_(initial_cash) {}

    void handle_fill_event(const FillEvent& event) {
        total_commission_ += event.commission;
        current_cash_ -= event.commission;
        Position& pos = positions_[event.symbol];
        double position_change_value = event.quantity * event.fill_price;
        double previous_quantity = pos.quantity;

        if (event.direction == OrderDirection::BUY) {
            current_cash_ -= position_change_value;
            pos.quantity += event.quantity;
        } else { // SELL
            current_cash_ += position_change_value;
            pos.quantity -= event.quantity;
        }

        // Update average price (more robust calculation needed for complex cases)
        if (std::abs(pos.quantity) < 1e-9) { // Position closed
             pos.average_price = 0.0;
        } else if ( (event.direction == OrderDirection::BUY && previous_quantity >= 0) || (event.direction == OrderDirection::SELL && previous_quantity > 0 && pos.quantity < 0) ) { // Adding to long or flipping short to long
             pos.average_price = ( (previous_quantity * pos.average_price) + position_change_value ) / pos.quantity; // Approximate for flips
        } else if ( (event.direction == OrderDirection::SELL && previous_quantity <= 0) || (event.direction == OrderDirection::BUY && previous_quantity < 0 && pos.quantity > 0) ) { // Adding to short or flipping long to short
             pos.average_price = ( (std::abs(previous_quantity) * pos.average_price) + position_change_value ) / std::abs(pos.quantity); // Approximate for flips
        }

        std::cout << "PORTFOLIO: Fill - Sym: " << event.symbol
                  << ", Dir: " << (event.direction == OrderDirection::BUY ? "BUY" : "SELL")
                  << ", Qty: " << event.quantity << " @ " << event.fill_price
                  << ", NewPos: " << pos.quantity << ", Cash: " << current_cash_ << std::endl;

        // Use fill price to immediately update market value after fill
        update_market_values({{event.symbol, PriceBar{event.timestamp, 0,0,0, event.fill_price, 0}}});
        record_equity(event.timestamp);
    }

    void update_market_values(const DataSnapshot& current_data) {
        for (auto& pair : positions_) {
            const std::string& symbol = pair.first;
            Position& pos = pair.second;
            if (pos.quantity == 0) {
                pos.market_value = 0.0;
                pos.unrealized_pnl = 0.0;
                continue;
            }
            auto it = current_data.find(symbol);
            if (it != current_data.end()) {
                double current_price = it->second.Close;
                pos.market_value = pos.quantity * current_price;
                double cost_basis = pos.quantity * pos.average_price;
                pos.unrealized_pnl = pos.market_value - cost_basis;
            }
        }
    }

     void record_equity(const std::chrono::system_clock::time_point& timestamp) {
         double total_market_value = 0.0;
         for(const auto& pair : positions_) { total_market_value += pair.second.market_value; }
         double total_equity = current_cash_ + total_market_value;
         // Avoid recording duplicate timestamps if events happen rapidly
         if (equity_curve_.empty() || equity_curve_.back().first != timestamp) {
              equity_curve_.emplace_back(timestamp, total_equity);
         } else {
              // Update the equity for the *latest* event at this exact timestamp
              equity_curve_.back().second = total_equity;
         }
     }

    // --- Getters ---
    double get_current_cash() const { return current_cash_; }
    double get_total_equity() const {
         double total_market_value = 0.0;
         for(const auto& pair : positions_) { total_market_value += pair.second.market_value; }
         return current_cash_ + total_market_value;
    }
    const std::map<std::string, Position>& get_positions() const { return positions_; }
    const std::vector<std::pair<std::chrono::system_clock::time_point, double>>& get_equity_curve() const { return equity_curve_; }
    double get_total_commission() const { return total_commission_; }
    double get_position_quantity(const std::string& symbol) const {
         auto it = positions_.find(symbol);
         return (it != positions_.end()) ? it->second.quantity : 0.0;
     }

    void print_summary() const { /* ... same as before, uses formatTimestampUTC internally ... */
        std::cout << "\n--- Portfolio Summary ---" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Initial Cash:    " << initial_cash_ << std::endl;
        std::cout << "Current Cash:    " << current_cash_ << std::endl;
        double total_market_value = 0.0;
        double total_unrealized_pnl = 0.0;
         for(const auto& pair : positions_) {
             total_market_value += pair.second.market_value;
             total_unrealized_pnl += pair.second.unrealized_pnl;
         }
        std::cout << "Market Value:    " << total_market_value << std::endl;
        std::cout << "Unrealized PnL:  " << total_unrealized_pnl << std::endl;
        std::cout << "Total Equity:    " << get_total_equity() << std::endl;
        std::cout << "Realized PnL:    " << realized_pnl_ << " (Simplified)" << std::endl;
        std::cout << "Total Commission:" << total_commission_ << std::endl;

        std::cout << "\nCurrent Positions:" << std::endl;
        bool has_positions = false;
        for (const auto& pair : positions_) {
            if (std::abs(pair.second.quantity) > 1e-9) {
                has_positions = true;
                std::cout << "  " << std::left << std::setw(30) << pair.first << ": " // Wider symbol name
                          << std::right << std::setw(10) << pair.second.quantity << " @ Avg Cost "
                          << std::setw(8) << pair.second.average_price
                          << " (MV: " << pair.second.market_value << ", UPL: " << pair.second.unrealized_pnl << ")"
                          << std::endl;
            }
        }
        if (!has_positions) {
            std::cout << "  (None)" << std::endl;
        }
        std::cout << "-------------------------" << std::endl;
    }
};