#include "core/Backtester.h"
#include "strategies/Strategy.h"
#include "strategies/MovingAverageCrossover.h"
#include <iostream>
#include <string>
#include <vector>
#include <memory>

int main() {
    std::cout << "--- HFT Backtesting System ---" << std::endl;

    // --- Configuration ---
    std::string data_dir = "../data";
    double initial_cash = 100000.0;
    size_t short_window = 5; // Use size_t
    size_t long_window = 20; // Use size_t
    double target_pos_size = 100.0; // Example target size

    // --- Instantiate Strategy ---
    auto strategy = std::make_unique<MovingAverageCrossover>(short_window, long_window, target_pos_size);

    // --- Instantiate Backtester ---
    Backtester backtester(data_dir, std::move(strategy), initial_cash);

    // --- Run Backtest ---
    try {
        backtester.run();
    } catch (const std::exception& e) { /* ... error handling ... */ }
      catch (...) { /* ... error handling ... */ }

    std::cout << "\n--- Backtest Run Invocation Complete ---" << std::endl;
    return 0;
}