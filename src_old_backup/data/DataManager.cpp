// --- Include necessary headers at the top ---
#include "DataManager.h" // Correct relative path from src/ to include/backtester/
#include "PriceBar.h"    // Correct relative path from src/ to include/data/ (Assuming PriceBar.h is there)
#include <filesystem>
#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <iomanip>
#include <cctype>
#include <csv2/reader.hpp>

namespace fs = std::filesystem;

// --- getSymbolFromFilename remains the same ---
std::string DataManager::getSymbolFromFilename(const fs::path& filePath) {
    if (filePath.has_stem()) { return filePath.stem().string(); }
    return "";
}


// --- REVISED parseCsvFile ---
bool DataManager::parseCsvFile(const fs::path& filePath, const std::string& symbol) {
    // ***** CORRECTED: Use TAB delimiter *****
    csv2::Reader<csv2::delimiter<'\t'>, // <--- CHANGE DELIMITER HERE
                 csv2::quote_character<'"'>,
                 csv2::first_row_is_header<true>,
                 csv2::trim_policy::trim_whitespace> csv;

    if (!csv.mmap(filePath.string())) {
        std::cerr << "      Error: Failed to memory map file: " << filePath << std::endl;
        return false;
    }

    std::vector<PriceBar> barsForSymbol;
    size_t rowNumber = 1; // After header

    // Define expected column indices based on CSV sample
    const int OPEN_IDX = 0, HIGH_IDX = 1, LOW_IDX = 2, CLOSE_IDX = 3,
              VOLUME_IDX = 4, DATE_IDX = 5, TIME_IDX = 6;
    const size_t EXPECTED_COLUMNS = 7;

    // Check header consistency AFTER loading it
    std::vector<std::string> header_names;
    size_t header_col_count = 0;
    try {
         const auto& header = csv.header();
         for (const auto& cell : header) {
              std::string col_name;
              cell.read_value(col_name);
              header_names.push_back(col_name);
              header_col_count++;
         }
         if (header_col_count != EXPECTED_COLUMNS) {
              std::cerr << "      Error: Expected " << EXPECTED_COLUMNS << " header columns in " << filePath.filename().string() << ", found " << header_col_count << "." << std::endl;
              return false; // Treat header mismatch as critical for this structure
         }
         // Optionally check for specific header names if needed here

    } catch (const std::exception& header_ex) {
         std::cerr << "      Error reading header: " << header_ex.what() << std::endl;
         return false;
    }


    // Iterate through each data row provided by the reader
    for (const auto& row : csv) {
        rowNumber++;
        std::vector<std::string> cells;
        cells.reserve(EXPECTED_COLUMNS);
        size_t actual_cell_count = 0;
        bool cell_read_error = false;

        try {
            for (const auto& cell : row) { // Iterate cells within the row
                std::string cellValue;
                cell.read_value(cellValue);
                cells.push_back(cellValue);
                actual_cell_count++;
            }
        } catch (const std::exception& cell_ex) {
             std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string() << ". Error reading cells: " << cell_ex.what() << std::endl;
             cell_read_error = true;
        }

        if (cell_read_error) continue;


        // Now check if we got the expected number of columns based on cells read
        if (actual_cell_count != EXPECTED_COLUMNS) {
            std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                      << ". Expected " << EXPECTED_COLUMNS << " columns, found " << actual_cell_count << "." << std::endl;
            continue; // Skip this row
        }

        try {
            // Access data from the 'cells' vector using indices
            const std::string& openStr   = cells[OPEN_IDX];
            const std::string& highStr   = cells[HIGH_IDX];
            const std::string& lowStr    = cells[LOW_IDX];
            const std::string& closeStr  = cells[CLOSE_IDX];
            const std::string& volumeStr = cells[VOLUME_IDX];
            const std::string& dateStr   = cells[DATE_IDX];
            const std::string& timeStr   = cells[TIME_IDX];

            // --- Convert strings ---
            // Calls PriceBar::stringToTimestamp which expects M/D/YY H:MM:SS (after next correction)
            auto timestamp = PriceBar::stringToTimestamp(dateStr, timeStr);
            double open = std::stod(openStr);
            double high = std::stod(highStr);
            double low = std::stod(lowStr);
            double close = std::stod(closeStr);
            long long volume = std::stoll(volumeStr);

            // --- Basic Data Validation ---
            bool valid = true;
            std::string validationError;
            if (open <= 0 || high <= 0 || low <= 0 || close <= 0 || volume < 0) {
                valid = false; validationError = "Non-positive price or negative volume.";
            } else if (high < low) {
                valid = false; validationError = "High < Low.";
            } else if (high < open || high < close || low > open || low > close) {
                 valid = false; validationError = "O/C outside H/L range.";
            }

            if (!valid) {
                 std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                           << ". Validation failed: " << validationError << " (O=" << openStr << ", H=" << highStr
                           << ", L=" << lowStr << ", C=" << closeStr << ", V=" << volumeStr << ")" << std::endl;
                 continue;
            }

            barsForSymbol.emplace_back(PriceBar{timestamp, open, high, low, close, volume});

        } catch (const std::exception& e) { // Catch exceptions from stod, stoll, stringToTimestamp
             std::cerr << "      Warning: Skipping row " << rowNumber << " in " << filePath.filename().string()
                       << ". Exception during processing: " << e.what() << std::endl;
        }
    } // End row loop

    if (!barsForSymbol.empty()) {
        std::sort(barsForSymbol.begin(), barsForSymbol.end(),
                  [](const PriceBar& a, const PriceBar& b) { return a.timestamp < b.timestamp; });
        historicalData_[symbol] = std::move(barsForSymbol);
        symbols_.push_back(symbol);
        std::cout << "      Successfully parsed and stored " << historicalData_[symbol].size() << " valid bars for " << symbol << "." << std::endl;
        return true;
    } else {
         std::cerr << "      Warning: No valid price bars stored from file: " << filePath.filename().string() << std::endl;
         return true;
    }
}

// --- Rest of DataManager methods (initializeSimulationState, loadData, etc.) ---
// --- Paste the rest of the correct methods from previous answers here ---
// ... (initializeSimulationState implementation) ...
// ... (loadData implementation - uses directory iteration and calls parseCsvFile) ...
// ... (getAssetData implementation) ...
// ... (getAllSymbols implementation) ...
// ... (getNextBars implementation) ...
// ... (getCurrentTime implementation) ...
// ... (isDataFinished implementation) ...