#ifndef PRICEBAR_H
#define PRICEBAR_H

#include <chrono>   // For std::chrono::system_clock::time_point
#include <string>   // For std::string
#include <sstream>  // For std::stringstream
#include <iomanip>  // For std::get_time, std::put_time
#include <ctime>    // For std::mktime, std::gmtime, std::time_t, std::tm
#include <stdexcept> // For std::runtime_error
#include <iostream> // For potential debug output

/**
 * @brief Represents a single bar of price data (Open, High, Low, Close, Volume)
 *        for a specific asset at a specific point in time.
 */
struct PriceBar {
    std::chrono::system_clock::time_point timestamp; // Timestamp of the bar's beginning
    double Open = 0.0;                               // Opening price
    double High = 0.0;                               // Highest price during the bar
    double Low = 0.0;                                // Lowest price during the bar
    double Close = 0.0;                              // Closing price
    long long Volume = 0;                            // Volume traded during the bar

    /**
     * @brief Converts the PriceBar's timestamp to a string representation (UTC).
     * @param format The desired output format string (e.g., "%Y-%m-%d %H:%M:%S").
     * @return The formatted timestamp string (in UTC).
     */
    std::string timestampToString(const std::string& format = "%Y-%m-%d %H:%M:%S") const {
        if (timestamp == std::chrono::system_clock::time_point::min() || timestamp == std::chrono::system_clock::time_point::max()) {
            return "N/A";
        }
        std::time_t time_t_val = std::chrono::system_clock::to_time_t(timestamp);
        std::tm tm_val = *std::gmtime(&time_t_val); // Use gmtime for UTC
        std::stringstream ss;
        ss << std::put_time(&tm_val, format.c_str());
        return ss.str();
    }

    /**
     * @brief Converts separate date and time strings into a std::chrono::system_clock::time_point.
     *
     * ADAPTED: Assumes date format "M/D/YY" and time format "H:MM:SS" (matches input CSV).
     * Uses std::get_time for parsing and std::mktime for conversion.
     *
     * WARNING: The %y format specifier (year without century) is locale-dependent and
     * its interpretation (e.g., 70-99 as 19xx, 00-69 as 20xx) can vary. This might
     * lead to incorrect dates if your data spans across century boundaries handled
     * differently by your system's C library. Using YYYY-MM-DD in source data is safer.
     *
     * Note: std::mktime interprets the std::tm struct as local time. If your input data
     * represents UTC, this might lead to incorrect time_point values depending on your
     * system's timezone settings.
     *
     * Throws std::runtime_error on parsing or conversion failure.
     *
     * @param dateStr The date string (e.g., "4/1/25").
     * @param timeStr The time string (e.g., "4:00:00").
     * @return The corresponding std::chrono::system_clock::time_point.
     * @throws std::runtime_error if parsing or conversion fails.
     */
    static std::chrono::system_clock::time_point stringToTimestamp(const std::string& dateStr, const std::string& timeStr) {
        std::tm tm = {};
        std::string datetime_str = dateStr + " " + timeStr; // Combine date and time
        std::stringstream ss(datetime_str);

        // Format string matching the input CSV: Month/Day/Year(2-digit) Hour:Minute:Second
        const char* format = "%Y-%m-%d %H:%M:%S";

        // Attempt to parse the combined date and time string
        ss >> std::get_time(&tm, format);

        // Strict check: Parsing failed OR not the entire string was consumed
        if (ss.fail()) {
             throw std::runtime_error("Failed to parse timestamp string '" + datetime_str + "' with format '" + format + "'. Check failbit.");
        }

        // Check if there are any non-whitespace characters left after parsing
        char remaining_char;
        while (ss.get(remaining_char)) {
            if (!std::isspace(static_cast<unsigned char>(remaining_char))) {
                // Found a non-whitespace character after the expected format
                throw std::runtime_error("Failed to parse timestamp string completely: '" + datetime_str + "'. Extra characters found after format '" + format + "'.");
            }
        }
        // If we only consumed whitespace, clear potential eof errors from whitespace reads
         ss.clear();


        // std::mktime converts std::tm (interpreted as local time) to time_t (seconds since epoch)
        // Caution: Timezone interpretation and %y ambiguity apply.
        std::time_t time = std::mktime(&tm);

        if (time == -1) {
            // mktime returns -1 if the conversion is not possible
             throw std::runtime_error("mktime failed for timestamp string: '" + datetime_str + "' (Resulting tm struct invalid? Check %y interpretation and timezone).");
        }

        return std::chrono::system_clock::from_time_t(time);
    }
};

#endif // PRICEBAR_H