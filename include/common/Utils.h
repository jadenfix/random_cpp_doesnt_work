#pragma once // Use include guards

#include <string>         // For std::string
#include <chrono>         // For std::chrono::system_clock::time_point, to_time_t
#include <sstream>        // For std::stringstream
#include <iomanip>        // For std::put_time
#include <ctime>          // For std::time_t, std::tm, std::gmtime

// Define a namespace for utility functions to avoid polluting the global namespace
namespace Backtester::Utils {

    // Utility function to format time_point for printing (using UTC)
    // Declared 'inline' as it's defined directly in the header.
    inline std::string formatTimestampUTC(const std::chrono::system_clock::time_point& tp) {
        // Handle special time_point values representing minimum or maximum
        if (tp == std::chrono::system_clock::time_point::min() || tp == std::chrono::system_clock::time_point::max()) {
            return "N/A"; // Return "N/A" for uninitialized or end-of-data markers
        }

        // Convert time_point to time_t (seconds since epoch)
        std::time_t time = std::chrono::system_clock::to_time_t(tp);

        // Convert time_t to a UTC time structure (std::tm)
        // Use std::gmtime for UTC, std::localtime for local time zone
        std::tm utc_tm = *std::gmtime(&time); // Dereference pointer returned by gmtime

        // Use a stringstream and std::put_time to format the tm struct
        std::stringstream ss;
        // Example Format: YYYY-MM-DD HH:MM:SS UTC (ISO 8601 like)
        ss << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S UTC");

        return ss.str(); // Return the formatted string
    }

    // --- Add other common utility functions below as needed ---
    // Example: Function to calculate percentage change
    // inline double calculate_percentage_change(double old_value, double new_value) {
    //     if (std::abs(old_value) < 1e-9) return 0.0; // Avoid division by zero
    //     return ((new_value / old_value) - 1.0) * 100.0;
    // }

} // namespace Backtester::Utils