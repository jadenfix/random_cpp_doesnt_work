#include "../include/backtester/Backtester.h" // Self header first

// Standard library includes
#include <iostream>
#include <stdexcept>
#include <chrono>
#include <optional>
#include <iomanip> // For put_time
#include <ctime>   // For time_t, gmtime
#include <cmath>   // For std::abs if used

// --- Include FULL definitions needed for implementation ---
#include "../include/common/Event.h"           // <<< NEED FULL MarketEvent def HERE
#include "../include/backtester/Portfolio.h"     // Need full Portfolio def
#include "../include/backtester/DataManager.h"     // Need full DataManager def
#include "../include/backtester/Strategy.h"      // Need full StrategyBase def
#include "../include/backtester/ExecutionSimulator.h" // Need full ExecutionSimulator def


namespace Backtester {

    // Constructor implementation
    Backtester::Backtester( DataManager& dataManager, StrategyBase& strategy,
                            Portfolio& portfolio, ExecutionSimulator& executionSimulator)
        : data_manager_(dataManager), strategy_(strategy),
          portfolio_(portfolio), execution_simulator_(executionSimulator) {}

    // Functional run loop
    void Backtester::run() {
        std::cout << "Backtester: Starting simulation..." << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        long bar_count = 0;

        // (Optional) Suppress unused variable warning if simulator not used yet
        // (void)execution_simulator_;

        // Main Event Loop
        std::optional<Common::MarketEvent> market_event_opt; // From common namespace
        while ((market_event_opt = data_manager_.get_next_bar()).has_value()) {
            // Get the actual market event (type is Common::MarketEvent)
            const Common::MarketEvent& market_event = market_event_opt.value();
            bar_count++;

            // Optional: Print progress
            if (bar_count % 10000 == 0) {
                std::time_t tt = std::chrono::system_clock::to_time_t(market_event.timestamp);
                std::cout << "... Processing bar " << bar_count << " | Time: "
                          << std::put_time(std::gmtime(&tt), "%Y-%m-%d %H:%M:%S UTC") << std::endl;
            }

            try {
                // 1. Update Portfolio Market Value
                // Portfolio::update_market_value expects MarketEvent (implicitly Backtester::MarketEvent due to fwd decl)
                // But market_event is Common::MarketEvent. Compiler needs full def via Event.h to resolve.
                portfolio_.update_market_value(market_event); // <<< This call needs Event.h included

                // 2. Let Strategy react to Market Data
                // Strategy::handle_market_event expects Common::MarketEvent, which matches.
                strategy_.handle_market_event(market_event, portfolio_);

                // 3. Order/Fill Handling (Simplified Flow Explanation)
                 if (bar_count == 1) {
                     std::cout << "INFO: Backtester.run simplified flow - order/fill simulation triggered internally." << std::endl;
                 }

            } catch (const std::exception& e) {
                std::cerr << "Error during loop for bar " << bar_count << " timestamp "
                          << std::chrono::system_clock::to_time_t(market_event.timestamp) << ": " << e.what() << std::endl;
                 break; // Stop on error
            }
        } // End while loop

        // ... (Rest of run method - finish/summary printout) ...
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);

        std::cout << "Backtester: Simulation finished after processing " << bar_count << " bars." << std::endl;
        std::cout << "Backtester: Total duration: " << duration.count() << " ms" << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << "           Final Backtest Results           " << std::endl;
        std::cout << "----------------------------------------" << std::endl;
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Initial Capital:      $" << portfolio_.get_initial_capital() << std::endl;
        std::cout << "Final Cash:           $" << portfolio_.get_cash() << std::endl;
        std::cout << "Final Market Value:   $" << portfolio_.get_total_market_value() << std::endl;
        std::cout << "Final Equity:         $" << portfolio_.get_equity() << std::endl;
        std::cout << "Total Realized PnL:   $" << portfolio_.get_total_realized_pnl() << std::endl;
        std::cout << "Total Unrealized PnL: $" << portfolio_.get_total_unrealized_pnl() << std::endl;
        double initial_cap = portfolio_.get_initial_capital();
        double total_return = (initial_cap > 1e-9) ? ((portfolio_.get_equity() / initial_cap) - 1.0) : 0.0;
        std::cout << "Total Return:         " << total_return * 100.0 << "%" << std::endl;
        std::cout << "\nFinal Positions:" << std::endl;
        const auto& final_positions = portfolio_.get_positions();
        bool has_positions = false;
        for (const auto& pair : final_positions) {
             const auto& pos = pair.second;
             if (std::abs(pos.quantity) > 1e-9) {
                 has_positions = true;
                 std::cout << "  Symbol: " << pos.symbol << ", Qty: " << pos.quantity << ", AvgPx: " << pos.average_entry_price << ", MV: " << pos.market_value << ", UPL: " << pos.unrealized_pnl << ", RPL: " << pos.realized_pnl << std::endl;
             }
        }
         if (!has_positions) { std::cout << "  (None)" << std::endl; }
        std::cout << "----------------------------------------" << std::endl;
    }

} // namespace Backtester