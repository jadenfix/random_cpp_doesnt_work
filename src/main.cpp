#include "DataManager.h"
#include "PriceBar.h"

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip> // For formatting output
#include <ctime>   // For time formatting helpers
#include <limits>

// Helper function to format time_point for printing (using UTC)
std::string formatTimestampUTC(const std::chrono::system_clock::time_point& tp) {
    // Use PriceBar's own formatter if available and suitable, or keep this helper
    if (tp == std::chrono::system_clock::time_point::min() || tp == std::chrono::system_clock::time_point::max()) {
        return "N/A";
    }
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm = *std::gmtime(&time); // Use gmtime for UTC
    std::stringstream ss;
    ss << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

int main() {
    std::cout << "--- DataManager Demonstration ---" << std::endl;

    DataManager dataManager;
    const std::string dataPath = "../data"; // Relative path from build dir

    std::cout << "\nAttempting to load data from: " << dataPath << std::endl;
    if (!dataManager.loadData(dataPath)) {
        std::cerr << "Error: Failed to load data successfully. Exiting." << std::endl;
        return 1;
    }

    auto symbols = dataManager.getAllSymbols();
    if (symbols.empty()) {
        std::cerr << "Warning: Data loading reported success, but no symbols were loaded/retained. Check CSV files, format, and parsing warnings." << std::endl;
        return 1; // Exit if no symbols loaded, as nothing can be processed
    }

    std::cout << "\nSuccessfully loaded data for symbols: ";
    for (size_t i = 0; i < symbols.size(); ++i) {
        std::cout << symbols[i] << (i == symbols.size() - 1 ? "" : ", ");
    }
    std::cout << std::endl;
    std::cout << "Initial Simulation Time: " << formatTimestampUTC(dataManager.getCurrentTime()) << std::endl;

    std::cout << "\n--- Iterating through first few time steps ---" << std::endl;
    const int maxStepsToShow = 25; // Show a few more steps
    int stepCount = 0;

    while (!dataManager.isDataFinished() && stepCount < maxStepsToShow) {
        stepCount++;
        DataSnapshot snapshot = dataManager.getNextBars();
        auto currentTime = dataManager.getCurrentTime();

        std::cout << "\n[" << std::setw(2) << stepCount << "] Timestamp: " << formatTimestampUTC(currentTime) << std::endl;

        if (snapshot.empty()) {
             // Check again if finished AFTER getting snapshot, as getNextBars might set finished state
             if (dataManager.isDataFinished()) {
                 std::cout << "  (Snapshot empty - End of data likely reached)" << std::endl;
                 break; // Exit loop cleanly if finished
             } else {
                 // This case means the current timestamp had no bars for any loaded symbol.
                 // This *could* happen with sparse data, but less likely with dense market data.
                 std::cout << "  (Snapshot empty for this timestamp, continuing...)" << std::endl;
                 continue;
             }
        }

        std::cout << "  Bars present (" << snapshot.size() << " symbols):" << std::endl;
        // Iterate sorted symbols for consistent print order
        for (const auto& symbol : dataManager.getAllSymbols()) {
            auto it = snapshot.find(symbol);
            if (it != snapshot.end()) {
                const auto& bar = it->second;
                std::cout << "    " << std::left << std::setw(8) << symbol << ": "
                          << "O=" << std::fixed << std::setprecision(2) << bar.Open << ", "
                          << "H=" << std::fixed << std::setprecision(2) << bar.High << ", "
                          << "L=" << std::fixed << std::setprecision(2) << bar.Low << ", "
                          << "C=" << std::fixed << std::setprecision(2) << bar.Close << ", "
                          << "V=" << bar.Volume
                          << std::endl;
            }
        }
    } // End while loop

    std::cout << "\n--- Demonstration Complete ---" << std::endl;
    if (dataManager.isDataFinished()) {
        std::cout << "(All data processed or end reached within loop)" << std::endl;
    } else if (stepCount >= maxStepsToShow) {
        std::cout << "(Stopped after " << maxStepsToShow << " steps)" << std::endl;
        std::cout << "Last processed timestamp was: " << formatTimestampUTC(dataManager.getCurrentTime()) << std::endl;
    }

    return 0;
}