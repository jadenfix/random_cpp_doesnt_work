#pragma once

#include <string>
#include <chrono>
#include <sstream>
#include <iomanip>
#include <ctime>

// Utility function to format time_point for printing (using UTC)
inline std::string formatTimestampUTC(const std::chrono::system_clock::time_point& tp) {
    if (tp == std::chrono::system_clock::time_point::min() || tp == std::chrono::system_clock::time_point::max()) {
        return "N/A";
    }
    std::time_t time = std::chrono::system_clock::to_time_t(tp);
    std::tm utc_tm = *std::gmtime(&time); // Use gmtime for UTC
    std::stringstream ss;
    // Use ISO 8601 like format for clarity
    ss << std::put_time(&utc_tm, "%Y-%m-%d %H:%M:%S UTC");
    return ss.str();
}

// You can add other common utility functions here later