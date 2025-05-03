#include "core/Backtester.h"           // Include the Backtester
#include "strategies/Strategy.h"       // Include the base Strategy
#include "strategies/MovingAverageCrossover.h" // Include a specific strategy header
#include <iostream>
#include <string>
#include <vector>
#include <memory> // For std::unique_ptr

int main() {
    std::cout << "--- HFT Backtesting System ---" << std::endl;

    // --- Configuration ---
    std::string data_dir = "../data"; // Relative path to data
    double initial_cash = 100000.0;
    // std::vector<std::string> symbols_to_trade = {"AAPL", "MSFT"}; // TODO: Filter symbols if needed

    // --- Instantiate Strategy ---
    // TODO: Load parameters from config file
    // Example: Create a MovingAverageCrossover strategy instance
    auto strategy = std::make_unique<MovingAverageCrossover>(5, 20); // Example periods

    // --- Instantiate Backtester ---
    Backtester backtester(
        data_dir,
        std::move(strategy), // Pass ownership of the strategy object
        initial_cash
    );

    // --- Run Backtest ---
    try {
        backtester.run();
    } catch (const std::exception& e) {
        std::cerr << "FATAL ERROR during backtest execution: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "FATAL UNKNOWN ERROR during backtest execution." << std::endl;
        return 1;
    }


    std::cout << "\n--- Backtest Complete ---" << std::endl;
    return 0;
}