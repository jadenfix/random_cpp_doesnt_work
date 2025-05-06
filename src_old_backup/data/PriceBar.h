#ifndef PRICEBAR_H
#define PRICEBAR_H

#include <chrono>
#include <string>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <stdexcept>
#include <iostream>
#include <cctype> // For isspace

// Define PriceBar within appropriate namespace if needed, e.g. Backtester::Data
// Assuming global or within a namespace accessible by DataManager.cpp

struct PriceBar {
    std::chrono::system_clock::time_point timestamp;
    double Open = 0.0;
    double High = 0.0;
    double Low = 0.0;
    double Close = 0.0;
    long long Volume = 0;

    std::string timestampToString(const std::string& format = "%Y-%m-%d %H:%M:%S") const {
        // ... (implementation remains the same) ...
        if (timestamp == std::chrono::system_clock::time_point::min() || timestamp == std::chrono::system_clock::time_point::max()) return "N/A";
        std::time_t time_t_val = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_val = *std::gmtime(&time_t_val);
        std::stringstream ss;
        ss << std::put_time(&tm_val, format.c_str());
        return ss.str();
    }

    static std::chrono::system_clock::time_point stringToTimestamp(const std::string& dateStr, const std::string& timeStr) {
        std::tm tm = {};
        std::string datetime_str = dateStr + " " + timeStr;
        std::stringstream ss(datetime_str);

        // ***** CORRECTED FORMAT STRING *****
        const char* format = "%m/%d/%y %H:%M:%S"; // Matches M/D/YY H:MM:SS
        // ***** END CORRECTION *****

        ss >> std::get_time(&tm, format);

        if (ss.fail()) {
             throw std::runtime_error("Failed to parse timestamp string '" + datetime_str + "' with format '" + format + "'. Check failbit.");
        }

        // Check remaining characters more strictly
        char remaining_char = 0;
        bool non_space_found = false;
        // Read trailing characters including potential whitespace
        while(ss.get(remaining_char)) {
            if (!std::isspace(static_cast<unsigned char>(remaining_char))) {
                non_space_found = true;
                break; // Found a non-space character
            }
        }
        // Only throw error if a non-whitespace character was found after successful parsing
        if (non_space_found) {
             throw std::runtime_error("Failed to parse timestamp string completely: '" + datetime_str + "'. Extra characters found after format '" + format + "'.");
        }
        // Clear eof/fail bits if only whitespace was left
        // Note: ss.eof() might be true even if only whitespace read by get(), so clear regardless
        ss.clear();


        std::time_t time = std::mktime(&tm);
        if (time == -1) {
             throw std::runtime_error("mktime failed for timestamp string: '" + datetime_str + "' (Invalid date/time components? Check %y interpretation: 25 -> " + std::to_string(1900+tm.tm_year) + ")");
        }

        return std::chrono::system_clock::from_time_t(time);
    }
};

#endif // PRICEBAR_H