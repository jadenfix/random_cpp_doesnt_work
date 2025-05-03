#pragma once // Use #pragma once for modern header guards

#include <string>
#include <vector>
#include <unordered_map>
#include <chrono>
#include <optional>
#include <filesystem> // Requires C++17
#include <functional> // For std::reference_wrapper

#include "PriceBar.h" // Include the PriceBar definition

/**
 * @brief Represents a snapshot of data for multiple assets at a single point in time.
 *        The map key is the asset symbol, and the value is the PriceBar for that asset
 *        at the snapshot's timestamp.
 */
using DataSnapshot = std::unordered_map<std::string, PriceBar>;

/**
 * @brief Manages loading, storing, and accessing historical financial data.
 *
 * Reads historical price data from CSV files, validates it, sorts it,
 * and provides methods to access the data chronologically across multiple assets.
 */
class DataManager {
public:
    DataManager() = default; // Default constructor

    /**
     * @brief Loads historical data from CSV files in the specified directory.
     * Assumes CSV header: open,high,low,close,volume,date_only,time_only
     * Assumes date format M/D/YY and time H:MM:SS in CSV (as per PriceBar::stringToTimestamp).
     * @param dataPath Path to the directory containing 'SYMBOL.csv' files.
     * @return True if data loading was successful (at least one file loaded with valid data), false otherwise.
     */
    bool loadData(const std::string& dataPath);

    /**
     * @brief Retrieves the complete historical data vector for a specific asset symbol.
     * @param symbol The asset symbol (e.g., "AAPL").
     * @return Optional containing a const reference wrapper to the vector if symbol exists, else std::nullopt.
     */
    std::optional<std::reference_wrapper<const std::vector<PriceBar>>> getAssetData(const std::string& symbol) const;

    /**
     * @brief Gets a list of all asset symbols for which data has been successfully loaded.
     * @return Vector of strings containing the loaded symbols, sorted alphabetically.
     */
    std::vector<std::string> getAllSymbols() const;

    /**
     * @brief Advances simulation time to the next timestamp and returns a snapshot of bars.
     * @return DataSnapshot containing PriceBars for all assets at the new current time.
     *         Returns an empty snapshot if all data has been processed.
     */
    DataSnapshot getNextBars();

    /**
     * @brief Gets the current simulation time managed by the DataManager.
     * @return The current simulation time. Returns time_point::min() if not initialized.
     */
    std::chrono::system_clock::time_point getCurrentTime() const;

    /**
     * @brief Checks if all historical data across all loaded symbols has been processed.
     * @return True if the simulation has reached the end of all data streams, false otherwise.
     */
    bool isDataFinished() const;

private:
    std::unordered_map<std::string, std::vector<PriceBar>> historicalData_;
    std::unordered_map<std::string, size_t> currentIndices_;
    std::chrono::system_clock::time_point currentTime_ = std::chrono::system_clock::time_point::min();
    std::vector<std::string> symbols_;
    bool dataLoaded_ = false;

    // --- Private Helper Methods ---
    std::string getSymbolFromFilename(const std::filesystem::path& filePath);
    bool parseCsvFile(const std::filesystem::path& filePath, const std::string& symbol);
    void initializeSimulationState();
};

// No #endif needed when using #pragma once