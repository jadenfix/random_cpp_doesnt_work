#pragma once

#include <string>
#include <vector>
#include <unordered_map> // Still uses unordered_map internally
#include <chrono>
#include <optional>
#include <filesystem>
#include <functional>
#include <memory> // Include for EventPtr potentially later? Or just use Event.h?

#include "data/PriceBar.h" // Correct path
#include "core/Event.h"    // Include for DataSnapshot definition and Event types

// Removed the duplicate 'using DataSnapshot = ...;' line

class DataManager {
public:
    DataManager() = default;
    bool loadData(const std::string& dataPath);
    std::optional<std::reference_wrapper<const std::vector<PriceBar>>> getAssetData(const std::string& symbol) const;
    std::vector<std::string> getAllSymbols() const;

    // Changed: This now returns the snapshot directly for the Backtester to wrap in an event
    DataSnapshot getNextBars();

    std::chrono::system_clock::time_point getCurrentTime() const;
    bool isDataFinished() const;

private:
    // Internal storage remains unordered_map for performance
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