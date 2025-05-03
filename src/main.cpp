#include "core/Backtester.h"
#include "strategies/Strategy.h"
// Include ALL strategy headers
#include "strategies/MovingAverageCrossover.h"
#include "strategies/VWAPReversion.h"
#include "strategies/OpeningRangeBreakout.h"
#include "strategies/MomentumIgnition.h"
#include "strategies/PairsTrading.h" // <-- Include new strategy

#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>
#include <iomanip>

// StrategyResult struct (defined in Portfolio.h now)
#include "core/Portfolio.h" // Make sure this is included


int main(int argc, char* argv[]) {
    std::cout << "--- HFT Backtesting System - Multi-Strategy Run ---" << std::endl;

    // --- Configuration ---
    std::string data_dir = "../data";
    double initial_cash = 100000.0;

    // --- Define Strategies and Parameters ---
    // Use a struct or map to hold strategy configurations cleanly
    struct StrategyConfig {
         std::string name;
         std::function<std::unique_ptr<Strategy>()> factory; // Function to create strategy
    };

    std::vector<StrategyConfig> strategies_to_test;

    // Add standard strategies
    strategies_to_test.push_back({"MACrossover_5_20", [](){ return std::make_unique<MovingAverageCrossover>(5, 20, 100.0); }});
    strategies_to_test.push_back({"VWAP_2.0", [](){ return std::make_unique<VWAPReversion>(2.0, 100.0); }});
    strategies_to_test.push_back({"ORB_30", [](){ return std::make_unique<OpeningRangeBreakout>(30, 100.0); }});
    strategies_to_test.push_back({"Momentum_5_10_2_3", [](){ return std::make_unique<MomentumIgnition>(5, 10, 2.0, 3, 100.0); }});

    // --- Add Pairs Trading Strategies ---
    // Use the actual symbol names derived from your filenames
    std::string msft_sym = "quant_seconds_data_MSFT";
    std::string nvda_sym = "quant_seconds_data_NVDA";
    std::string goog_sym = "quant_seconds_data_google";
    double pairs_trade_value = 10000.0; // $ value per leg
    size_t pairs_lookback = 60; // Lookback window (e.g., 60 minutes)
    double pairs_entry_z = 2.0;
    double pairs_exit_z = 0.5;

    strategies_to_test.push_back({"Pairs_MSFT_NVDA", [&](){ return std::make_unique<PairsTrading>(msft_sym, nvda_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }});
    strategies_to_test.push_back({"Pairs_NVDA_GOOG", [&](){ return std::make_unique<PairsTrading>(nvda_sym, goog_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }});
    strategies_to_test.push_back({"Pairs_MSFT_GOOG", [&](){ return std::make_unique<PairsTrading>(msft_sym, goog_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }});


    // --- Map to Store Results ---
    std::map<std::string, StrategyResult> results;

    // --- Loop Through Strategies ---
    for (const auto& config : strategies_to_test) {
        std::cout << "\n\n===== Running Strategy: " << config.name << " =====" << std::endl;

        std::unique_ptr<Strategy> strategy = nullptr;
        try {
             strategy = config.factory(); // Create strategy using the factory function
        } catch (const std::exception& e) { // Catch potential errors from factory/constructor
            std::cerr << "Error creating strategy '" << config.name << "': " << e.what() << ". Skipping." << std::endl;
            continue;
        }

        if (!strategy) {
             std::cerr << "Error: Failed to instantiate strategy '" << config.name << "'. Skipping." << std::endl;
             continue;
        }

        // Create a new Backtester for each run to reset state
        Backtester backtester(data_dir, std::move(strategy), initial_cash);
        Portfolio const* result_portfolio = nullptr;

        try {
            result_portfolio = backtester.run_and_get_portfolio();
        } catch (const std::exception& e) {
            std::cerr << "FATAL ERROR during backtest for '" << config.name << "': " << e.what() << std::endl;
            continue;
        } catch (...) {
            std::cerr << "FATAL UNKNOWN ERROR during backtest for '" << config.name << "'." << std::endl;
            continue;
        }

        // Store results
        if (result_portfolio) {
            results[config.name] = result_portfolio->get_results_summary();
        } else {
             std::cerr << "Warning: Backtest ran but portfolio pointer was null for " << config.name << "." << std::endl;
        }
        std::cout << "===== Finished Strategy: " << config.name << " =====" << std::endl;
    } // End loop


    // --- Print Comparison Table ---
    std::cout << "\n\n===== Strategy Comparison Results =====" << std::endl;
    std::cout << std::left << std::setw(25) << "Strategy" // Wider column for names
              << std::right << std::setw(15) << "Return (%)"
              << std::right << std::setw(15) << "Max DD (%)"
              << std::right << std::setw(15) << "Realized PnL"
              << std::right << std::setw(15) << "Commission"
              << std::right << std::setw(10) << "Fills"
              << std::right << std::setw(18) << "Final Equity"
              << std::endl;
    std::cout << std::string(113, '-') << std::endl; // Adjust separator width

    for (const auto& pair : results) {
        const std::string& name = pair.first;
        const StrategyResult& res = pair.second;
        std::cout << std::left << std::setw(25) << name
                  << std::fixed << std::setprecision(2)
                  << std::right << std::setw(15) << res.total_return_pct
                  << std::right << std::setw(15) << res.max_drawdown_pct
                  << std::right << std::setw(15) << res.realized_pnl
                  << std::right << std::setw(15) << res.total_commission
                  << std::right << std::setw(10) << res.num_fills
                  << std::right << std::setw(18) << res.final_equity
                  << std::endl;
    }
    std::cout << "============================================" << std::endl; // Adjust end separator


    std::cout << "\n--- Multi-Strategy Run Invocation Complete ---" << std::endl;
    return 0;
}