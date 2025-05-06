#include "backtester/DataManager.h" // Include the corresponding header first
#include "common/Event.h"           // Needs MarketEvent, DataSnapshot
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <chrono>
#include <iomanip>         // For std::get_time
#include <optional>
#include <vector>
#include <string>
#include <unordered_map>
#include <memory>
#include <ctime>           // For std::tm, std::mktime, std::gmtime
#include <filesystem>      // <<< NEED THIS for directory iteration
#include <algorithm>       // <<< NEED THIS for std::sort and std::transform
#include <cctype>          // <<< NEED THIS for std::tolower

#include <csv2/reader.hpp> // Include csv2 header

// Filesystem alias
namespace fs = std::filesystem;

namespace Backtester {

    // Concrete implementation using CSV files and iterating through cells
    class CsvDataManager : public DataManager {
    private:
        // --- Member Variables ---
        std::string data_directory_path_;
        size_t current_row_index_;
        std::vector<Common::MarketEvent> all_parsed_data_;
        std::unordered_map<std::string, size_t> header_map_;
        std::vector<std::string> header_names_;

        // --- Helper to get symbol from filename --- <<< ADDED BACK
        std::string getSymbolFromFilename(const fs::path& filePath) {
            if (filePath.has_stem()) { return filePath.stem().string(); }
            return "UNKNOWN_SYMBOL"; // Return default if stem cannot be found
        }

        // --- Helper Function to Parse Row ---
        Common::MarketEvent parse_row_cells_to_event(
            const std::vector<std::string>& cells,
            const std::string& symbol_for_this_row)
        {
            Common::DataSnapshot snapshot;
            std::chrono::time_point<std::chrono::system_clock> timestamp;

            // --- Define format string outside try block ---
            const char* timestamp_format = "%m/%d/%y %H:%M:%S"; // Format for M/D/YY H:MM:SS

            // --- Parse Timestamp ---
            if (header_map_.empty() || !header_map_.count("date_only") || !header_map_.count("time_only")) { // Check required headers
                 throw std::runtime_error("Header map not initialized or missing 'date_only'/'time_only' columns.");
            }
            size_t date_idx = header_map_.at("date_only"); // Use .at() for bounds check
            size_t time_idx = header_map_.at("time_only");

            if (date_idx >= cells.size() || time_idx >= cells.size()) {
                 throw std::runtime_error("Date/Time column index out of bounds for row. Cells size: " + std::to_string(cells.size()));
            }
            const std::string& dateStr = cells[date_idx];
            const std::string& timeStr = cells[time_idx];

            std::tm tm = {};
            std::string datetime_str = dateStr + " " + timeStr;
            std::stringstream ss(datetime_str);

            ss >> std::get_time(&tm, timestamp_format); // Use the defined format

            if (ss.fail()) {
                 throw std::runtime_error("Failed basic parse for timestamp string '" + datetime_str + "' with format '" + timestamp_format + "'. Check failbit.");
            }
            // Check trailing characters
            char remaining_char = 0;
            bool non_space_found = false;
            while(ss.get(remaining_char)) { if (!std::isspace(static_cast<unsigned char>(remaining_char))) { non_space_found = true; break; } }
            if (non_space_found) {
                 // Use timestamp_format variable in error message
                 throw std::runtime_error("Failed to parse timestamp completely: '" + datetime_str + "'. Extra characters found after format '" + std::string(timestamp_format) + "'."); // <-- CORRECTED: Use variable
            }
            ss.clear();

            std::time_t time_since_epoch = std::mktime(&tm);
            if (time_since_epoch == -1) {
                throw std::runtime_error("mktime failed for timestamp: " + datetime_str + " (Check %y interpretation: " + std::to_string(1900+tm.tm_year) + ")");
            }
            timestamp = std::chrono::system_clock::from_time_t(time_since_epoch);

            // --- Parse other standard columns (case-insensitive lookup) ---
            // We iterate through the *found header names* instead of predefined constants
            for (size_t i = 0; i < header_names_.size(); ++i) {
                const std::string& col_name = header_names_[i];
                // Skip timestamp columns as they are handled separately
                if (col_name == "date_only" || col_name == "time_only" || col_name == "Timestamp") continue;

                if (i >= cells.size()) {
                     throw std::runtime_error("Column index " + std::to_string(i) + " ('" + col_name + "') out of bounds for row.");
                }
                const std::string& cell_value_str = cells[i];
                try {
                    // Attempt conversion to double for OHLCV columns
                    // Store using the exact header name found in the file
                    snapshot[col_name] = std::stod(cell_value_str);
                } catch (...) {
                     std::cerr << "Warning: Could not parse numeric value for column '" << col_name << "' value '" << cell_value_str << "' in " << symbol_for_this_row << ". Using 0.0." << std::endl;
                     snapshot[col_name] = 0.0;
                }
            } // End loop over header_names_

            // Final check for Close price key (case-insensitive)
            bool close_found = false;
            std::string close_key_found = "";
             for(const auto& pair : header_map_) { // Check original header names
                  std::string lower_header = pair.first;
                  std::transform(lower_header.begin(), lower_header.end(), lower_header.begin(), ::tolower);
                  if(lower_header == "close") {
                       close_key_found = pair.first; // Store the actual key found
                       close_found = snapshot.count(close_key_found); // Check if it was parsed into snapshot
                       break;
                  }
             }
            if (!close_found) {
                 // Attempt lookup again in snapshot just in case (shouldn't be needed)
                 if (snapshot.count("Close")) close_found = true;
                 else if (snapshot.count("close")) close_found = true;

                 if (!close_found) {
                    throw std::runtime_error("Essential 'Close'/'close' price missing after parsing row for symbol " + symbol_for_this_row);
                 }
            }

            return Common::MarketEvent(timestamp, symbol_for_this_row, std::move(snapshot));
        } // End parse_row_cells_to_event


    public:
        CsvDataManager() : current_row_index_(0) {}

        // --- load_data handles DIRECTORY path ---
        bool load_data(const std::string& directory_source) override {
            data_directory_path_ = directory_source;
            current_row_index_ = 0;
            all_parsed_data_.clear();
            header_map_.clear();
            header_names_.clear();
            bool first_file_processed = false;
            long total_loaded_rows = 0;
            long total_skipped_rows = 0;

            try {
                fs::path dirPath(data_directory_path_);
                if (!fs::exists(dirPath) || !fs::is_directory(dirPath)) { /* ... error ... */ return false; }
                std::cout << "DataManager: Loading data from directory: " << data_directory_path_ << std::endl;

                for (const auto& entry : fs::directory_iterator(dirPath)) {
                     const auto& file_path = entry.path();
                     if (entry.is_regular_file()) {
                          std::string ext = file_path.extension().string();
                          std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
                          if (ext == ".csv") {
                              // --- Use getSymbolFromFilename --- <<< CORRECTED
                              std::string current_symbol = getSymbolFromFilename(file_path);
                              std::cout << "  --> Processing file: " << file_path.filename().string() << " for symbol: " << current_symbol << std::endl;

                              csv2::Reader<csv2::delimiter<'\t'>, // Assuming TAB delimiter
                                           csv2::quote_character<'"'>,
                                           csv2::first_row_is_header<true>,
                                           csv2::trim_policy::trim_whitespace> csv_reader;

                              if (!csv_reader.mmap(file_path.string())) { /* ... skip ... */ continue; }
                              if (csv_reader.rows() == 0) { /* ... skip ... */ continue; }

                              // --- Header Handling ---
                              size_t current_file_header_count = 0;
                              if (!first_file_processed) {
                                  header_map_.clear(); header_names_.clear();
                                  try {
                                       const auto& header = csv_reader.header();
                                       for (const auto& cell : header) {
                                            std::string col_name; cell.read_value(col_name);
                                            header_names_.push_back(col_name); header_map_[col_name] = current_file_header_count++;
                                       }
                                       // --- Check for date_only, time_only, close (case insensitive) ---
                                       bool has_date = header_map_.count("date_only");
                                       bool has_time = header_map_.count("time_only");
                                       bool has_close = header_map_.count("Close") || header_map_.count("close");
                                       if (!has_date || !has_time || !has_close) {
                                            std::cerr << "Error: First CSV file '" << file_path.filename().string() << "' is missing required 'date_only', 'time_only', and 'Close'/'close' columns. Aborting load." << std::endl;
                                            return false;
                                       }
                                       first_file_processed = true;
                                       std::cout << "      Header processed (" << header_names_.size() << " columns). Assuming consistent header." << std::endl;
                                  } catch (const std::exception& header_ex) { /* ... abort ... */ return false; }
                              } else {
                                  // Check consistency
                                   size_t temp_count = 0;
                                   try {
                                        const auto& header = csv_reader.header();
                                        for (const auto& cell : header) { (void)cell; temp_count++; } // <<< CORRECTED: Suppress warning
                                   } catch (...) { /* Ignore count error */ }
                                   current_file_header_count = temp_count; // Assign count
                                   if (current_file_header_count != header_names_.size()) {
                                       std::cerr << "      Warning: Header column count mismatch in file '" << file_path.filename().string() << "'. Skipping file." << std::endl;
                                       continue;
                                   }
                              } // End header handling


                              // --- Load and Parse Rows ---
                              size_t file_rows_parsed = 0;
                              size_t file_rows_skipped = 0;
                              size_t current_physical_row = 1;
                              for (const auto& row : csv_reader) { /* ... (Cell iteration, size check, call parse_row_cells_to_event) ... */
                                    current_physical_row++;
                                    std::vector<std::string> current_row_cells; current_row_cells.reserve(header_names_.size());
                                    bool cell_read_error = false; size_t actual_cell_count = 0;
                                    try { for(const auto& cell : row) { std::string value; cell.read_value(value); current_row_cells.push_back(value); actual_cell_count++; } }
                                    catch(...) { cell_read_error = true; }
                                    if(cell_read_error || actual_cell_count != header_names_.size()) { file_rows_skipped++; continue; }
                                    try { all_parsed_data_.push_back(parse_row_cells_to_event(current_row_cells, current_symbol)); file_rows_parsed++; }
                                    catch(const std::exception& parse_ex) { file_rows_skipped++; /* Log */ continue; }
                              }
                              std::cout << "      Parsed " << file_rows_parsed << " rows (skipped " << file_rows_skipped << ") for " << current_symbol << "." << std::endl;
                              total_loaded_rows += file_rows_parsed;
                              total_skipped_rows += file_rows_skipped;
                          } // End if ".csv"
                     } // End if regular file
                } // End loop over directory entries

            } catch (const std::exception& e) { /* ... */ return false; }

            if (total_loaded_rows == 0) { /* ... */ return false; }
            std::cout << "DataManager: Finished processing files. Total loaded: " << total_loaded_rows << ", Total skipped: " << total_skipped_rows << "." << std::endl;

            // --- Sort all loaded data by timestamp ---
            std::cout << "DataManager: Sorting " << all_parsed_data_.size() << " loaded events by timestamp..." << std::endl;
            std::sort(all_parsed_data_.begin(), all_parsed_data_.end(),
                      [](const Common::MarketEvent& a, const Common::MarketEvent& b) { return a.timestamp < b.timestamp; });
            std::cout << "DataManager: Data sorting complete." << std::endl;

            return true;
        }

        // Returns the next pre-parsed market event
        std::optional<Common::MarketEvent> get_next_bar() override {
            if (current_row_index_ >= all_parsed_data_.size()) { return std::nullopt; }
            return all_parsed_data_[current_row_index_++];
        }

        void reset() override {
            current_row_index_ = 0;
            std::cout << "Data stream reset to beginning for directory " << data_directory_path_ << std::endl;
        }
    }; // End of CsvDataManager class

    // Factory function implementation
    std::unique_ptr<DataManager> create_csv_data_manager() {
         return std::make_unique<CsvDataManager>();
    }

} // namespace Backtester