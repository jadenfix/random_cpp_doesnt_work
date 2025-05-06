#pragma once // Use include guards

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <memory>
#include <numeric>
#include <iostream>
#include <optional>

// --- Include necessary COMMON types directly ---
// These are needed for member variables or parameter/return types in this header
#include "../common/Position.h"       // Needs full Position definition for positions_ member
#include "../common/FillEvent.h"    // Needs FillDetails for update_fill parameter
#include "../common/OrderRequest.h" // Needs OrderRequest for generate_order return type

// --- Forward declarations with FULL namespaces ---
// Use the actual namespace where the types are defined (common)
namespace Backtester::Common {
    class MarketEvent; // Forward declare Common::MarketEvent
    class SignalEvent; // Forward declare Common::SignalEvent
} // namespace Backtester::Common


namespace Backtester {

    // --- Define StrategyResult Struct (Moved from Portfolio.h in previous answer for clarity) ---
    // This struct is used by main.cpp to collect results. Defining it here avoids
    // main.cpp needing to include Portfolio.h just for this struct.
    // Alternatively, put it in its own header like common/Metrics.h
    struct StrategyResult {
        double total_return_pct = 0.0;
        double max_drawdown_pct = 0.0;
        double realized_pnl = 0.0;
        double total_commission = 0.0;
        long num_fills = 0;
        double final_equity = 0.0;
    };


    // Manages portfolio state
    class Portfolio {
    public:
        Portfolio(double initial_capital); // Implementation in .cpp
        virtual ~Portfolio() = default;

        // Method signatures use fully qualified names
        void update_fill(const Common::FillDetails& fill); // Implementation in .cpp
        void update_market_value(const Common::MarketEvent& event); // Implementation in .cpp
        std::optional<Common::OrderRequest> generate_order(const Common::SignalEvent& signal_event); // Implementation in .cpp

        // Accessors
        double get_cash() const { return cash_; }
        double get_initial_capital() const { return initial_capital_; }
        double get_total_market_value() const; // Implementation in .cpp
        double get_total_unrealized_pnl() const; // Implementation in .cpp
        double get_total_realized_pnl() const; // Implementation in .cpp
        double get_equity() const; // Implementation in .cpp
        const std::unordered_map<std::string, Common::Position>& get_positions() const { return positions_; }
        Common::Position get_position(const std::string& symbol) const; // Implementation in .cpp

        // --- Performance Metrics ---
        // Added these back as they were in the previous correct version
        void calculate_and_print_metrics() const; // Declaration
        void print_final_summary() const;         // Declaration
        StrategyResult get_results_summary() const; // Declaration


    private:
        double initial_capital_;
        double cash_;
        std::unordered_map<std::string, Common::Position> positions_;
        // Added members needed for metrics calculation back
        double total_commission_ = 0.0;
        double realized_pnl_ = 0.0; // Portfolio-level tracking
        long num_fills_ = 0;
        std::vector<std::pair<std::chrono::system_clock::time_point, double>> equity_curve_;
        void record_equity(const std::chrono::system_clock::time_point& timestamp); // Declaration

    }; // End class Portfolio

} // namespace Backtester