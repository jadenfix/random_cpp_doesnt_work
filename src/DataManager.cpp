// --- Include necessary headers at the top ---
#include "DataManager.h"
#include "PriceBar.h"
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include "csv2/reader.hpp"

namespace fs = std::filesystem;

// --- getSymbolFromFilename remains the same ---
std::string DataManager::getSymbolFromFilename(const fs::path& filePath) {
    if (filePath.has_stem()) {
        return filePath.stem().string();
    }
    return "";
}


// --- REVISED parseCsvFile ---
bool DataManager::parseCsvFile(const fs::path& filePath, const std::string& symbol) {
    csv2::Reader<csv2::delimiter<','>,
                 csv2::quote_character<'"'>,
                 csv2::first_row_is_header<true>,
                 csv2::trim_policy::trim_whitespace> csv;

    if (!csv.mmap(filePath.string())) {
        std::cerr << "      Error: Failed to memory map file: " << filePath << std::endl;
        return false;
    }

    std::vector<PriceBar> barsForSymbol;
    size_t rowNumber = 1; // After header

    const int OPEN_IDX = 0, HIGH_IDX = 1, LOW_IDX = 2, CLOSE_IDX = 3,
              VOLUME_IDX = 4, DATE_IDX = 5, TIME_IDX = 6;
    const size_t EXPECTED_COLUMNS = 7;

    // Iterate through each row provided by the reader
    for (const auto& row : csv) {
        rowNumber++;
        std::vector<std::string> cells; // Temporary vector to hold cell values for this row
        cells.reserve(EXPECTED_COLUMNS); // Optional: pre-allocate space

        try {
            // Iterate through cells *within* the current row
            for (const auto& cell : row) {
                std::string cellValue;
                cell.read_value(cellValue); // Read value from the cell object
                cells.push_back(cellValue);
            }

            // Now check if we got the expected number of columns
            if (cells.size() != EXPECTED_COLUMNS) {
                std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                          << ". Expected " << EXPECTED_COLUMNS << " columns, found " << cells.size() << "." << std::endl;
                continue; // Skip this row
            }

            // Access data from the temporary 'cells' vector using indices
            const std::string& openStr   = cells[OPEN_IDX];
            const std::string& highStr   = cells[HIGH_IDX];
            const std::string& lowStr    = cells[LOW_IDX];
            const std::string& closeStr  = cells[CLOSE_IDX];
            const std::string& volumeStr = cells[VOLUME_IDX];
            const std::string& dateStr   = cells[DATE_IDX];
            const std::string& timeStr   = cells[TIME_IDX];

            // --- Convert strings ---
            auto timestamp = PriceBar::stringToTimestamp(dateStr, timeStr); // Uses M/D/YY H:MM:SS
            double open = std::stod(openStr);
            double high = std::stod(highStr);
            double low = std::stod(lowStr);
            double close = std::stod(closeStr);
            long long volume = std::stoll(volumeStr);

            // --- Basic Data Validation ---
            bool valid = true;
            std::string validationError;
            if (open <= 0 || high <= 0 || low <= 0 || close <= 0 || volume < 0) {
                valid = false;
                validationError = "Non-positive price or negative volume.";
            } else if (high < low) {
                valid = false;
                validationError = "High (" + highStr + ") < Low (" + lowStr + ").";
            } else if (high < open || high < close || low > open || low > close) {
                 valid = false;
                 validationError = "O/C outside H/L range.";
            }

            if (!valid) {
                 std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                           << ". Validation failed: " << validationError << " (O=" << openStr << ", H=" << highStr
                           << ", L=" << lowStr << ", C=" << closeStr << ", V=" << volumeStr << ")" << std::endl;
                 continue;
            }

            barsForSymbol.emplace_back(PriceBar{timestamp, open, high, low, close, volume});

        // Catch exceptions from string conversions or timestamp parsing
        } catch (const std::exception& e) {
             std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                       << ". Exception during processing: " << e.what() << std::endl;
        }
    } // End row loop

    if (!barsForSymbol.empty()) {
        std::sort(barsForSymbol.begin(), barsForSymbol.end(),
                  [](const PriceBar& a, const PriceBar& b) {
                      return a.timestamp < b.timestamp;
                  });
        historicalData_[symbol] = std::move(barsForSymbol);
        symbols_.push_back(symbol);
        std::cout << "      Successfully parsed and stored " << historicalData_[symbol].size() << " valid bars for " << symbol << "." << std::endl;
        return true;
    } else {
         std::cerr << "      Warning: No valid price bars stored from file: " << filePath.filename().string() << std::endl;
         return true;
    }
}

// --- IMPORTANT: Make sure the rest of the DataManager methods ---
// --- (initializeSimulationState, loadData, getAssetData, getAllSymbols, ---
// ---  getNextBars, getCurrentTime, isDataFinished) ---
// --- are present and correct from the previous successful compilation attempt ---
// --- (specifically ensuring they use || for logical OR etc.) ---

// ... (Paste the rest of the DataManager methods here from the previous answer) ...

void DataManager::initializeSimulationState() {
    if (historicalData_.empty() || symbols_.empty()) {
        std::cerr << "Warning: No historical data loaded/symbols found. Cannot initialize simulation state." << std::endl;
        currentTime_ = std::chrono::system_clock::time_point::min();
        dataLoaded_ = false;
        return;
    }
    currentTime_ = std::chrono::system_clock::time_point::max();
    bool foundAnyData = false;
    for (const auto& symbol : symbols_) {
        if (historicalData_.count(symbol) && !historicalData_.at(symbol).empty()) {
            currentTime_ = std::min(currentTime_, historicalData_.at(symbol).front().timestamp);
            foundAnyData = true;
        }
    }
    if (!foundAnyData) {
        std::cerr << "Warning: Data files processed, but no valid bars found. Cannot initialize simulation time." << std::endl;
        currentTime_ = std::chrono::system_clock::time_point::min();
        dataLoaded_ = false;
        symbols_.clear();
        historicalData_.clear();
        return;
    }
    currentIndices_.clear();
    for (const auto& symbol : symbols_) {
        if (historicalData_.count(symbol)) {
             currentIndices_[symbol] = 0;
        }
    }
    std::sort(symbols_.begin(), symbols_.end());
    dataLoaded_ = true;
}

bool DataManager::loadData(const std::string& dataPath) {
    fs::path dirPath(dataPath);
    dataLoaded_ = false;
    historicalData_.clear();
    symbols_.clear();
    currentIndices_.clear();
    currentTime_ = std::chrono::system_clock::time_point::min();
    if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) {
        std::cerr << "Error: Data path does not exist or is not a directory: " << dataPath << std::endl;
        return false;
    }
    std::cout << "Loading data from: " << dataPath << std::endl;
    bool anyFileParsedSuccessfullyWithData = false;
    try {
        for (const auto& entry : fs::directory_iterator(dirPath)) {
            const auto& path = entry.path();
            if (entry.is_regular_file()) {
                std::string ext = path.extension().string();
                std::transform(ext.begin(), ext.end(), ext.begin(),
                              [](unsigned char c){ return std::tolower(c); });
                if (ext == ".csv") {
                    std::string symbol = getSymbolFromFilename(path);
                    if (!symbol.empty()) {
                        std::cout << "  Parsing file: " << path.filename().string() << " for symbol: " << symbol << std::endl;
                        if (parseCsvFile(path, symbol)) {
                            if (historicalData_.count(symbol) && !historicalData_.at(symbol).empty()) {
                                anyFileParsedSuccessfullyWithData = true;
                            }
                        } else {
                            std::cerr << "  Critical error parsing file: " << path.filename().string() << ". Skipping." << std::endl;
                             if (historicalData_.count(symbol)) {
                                 historicalData_.erase(symbol);
                                 symbols_.erase(std::remove(symbols_.begin(), symbols_.end(), symbol), symbols_.end());
                             }
                        }
                    } else {
                        std::cerr << "  Warning: Could not extract symbol from filename: " << path.filename().string() << ". Skipping." << std::endl;
                    }
                }
            }
        }
    } catch (const fs::filesystem_error& e) {
        std::cerr << "Filesystem error while iterating directory " << dataPath << ": " << e.what() << std::endl;
        return false;
    }
    if (anyFileParsedSuccessfullyWithData) {
        initializeSimulationState();
        if (dataLoaded_) {
             std::cout << "Data loading complete. Initial simulation time: ";
             if (currentTime_ != std::chrono::system_clock::time_point::min()) {
                 auto time_t_currentTime = std::chrono::system_clock::to_time_t(currentTime_);
                 std::cout << std::put_time(std::gmtime(&time_t_currentTime), "%Y-%m-%d %H:%M:%S UTC") << std::endl;
             } else {
                 std::cout << "N/A (No valid bars found)" << std::endl;
             }
        } else {
             std::cerr << "Error: Files parsed, but state initialization failed (likely no valid bars found across all files)." << std::endl;
        }
    } else {
        std::cerr << "Error: No valid CSV data files found or loaded successfully in " << dataPath << std::endl;
        dataLoaded_ = false;
    }
    return dataLoaded_;
}

std::optional<std::reference_wrapper<const std::vector<PriceBar>>> DataManager::getAssetData(const std::string& symbol) const {
    auto it = historicalData_.find(symbol);
    if (it != historicalData_.end()) {
        return std::cref(it->second);
    }
    return std::nullopt;
}

std::vector<std::string> DataManager::getAllSymbols() const {
    return symbols_;
}

DataSnapshot DataManager::getNextBars() {
    if (!dataLoaded_ || isDataFinished()) {
        return {};
    }
    auto nextTimestamp = std::chrono::system_clock::time_point::max();
    bool foundNextTimestamp = false;
    for (const auto& symbol : symbols_) {
        auto it_idx = currentIndices_.find(symbol);
        auto it_data = historicalData_.find(symbol);
        if (it_idx != currentIndices_.end() && it_data != historicalData_.end()) {
            const size_t currentIndex = it_idx->second;
            const auto& bars = it_data->second;
            if (currentIndex < bars.size()) {
                nextTimestamp = std::min(nextTimestamp, bars[currentIndex].timestamp);
                foundNextTimestamp = true;
            }
        }
    }
    if (!foundNextTimestamp) {
        currentTime_ = std::chrono::system_clock::time_point::max();
        return {};
    }
    currentTime_ = nextTimestamp;
    DataSnapshot snapshot;
    for (const auto& symbol : symbols_) {
        auto it_idx = currentIndices_.find(symbol);
        auto it_data = historicalData_.find(symbol);
        if (it_idx != currentIndices_.end() && it_data != historicalData_.end()) {
            size_t& currentIndex = it_idx->second;
            const auto& bars = it_data->second;
            if (currentIndex < bars.size() && bars[currentIndex].timestamp == currentTime_) {
                snapshot[symbol] = bars[currentIndex];
                currentIndex++;
            }
        }
    }
    return snapshot;
}

std::chrono::system_clock::time_point DataManager::getCurrentTime() const {
    return currentTime_;
}

bool DataManager::isDataFinished() const {
    if (!dataLoaded_) return true;
    if (symbols_.empty()) return true;
    return std::all_of(symbols_.begin(), symbols_.end(), [this](const std::string& symbol) {
        auto it_idx = currentIndices_.find(symbol);
        auto it_data = historicalData_.find(symbol);
        if (it_idx == currentIndices_.end() || it_data == historicalData_.end()) {
            return true;
        }
        return it_idx->second >= it_data->second.size();
    });
}