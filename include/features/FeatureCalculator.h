#pragma once

#include <vector>
#include <string>
#include <unordered_map>
#include <stdexcept> // For std::runtime_error
#include <iostream> // For std::cout, std::cerr
#include "../common/Event.h" // Include DataSnapshot (which is std::unordered_map<std::string, double>)

// Forward declare TA-Lib types if needed, or include ta_libc.h if using directly
// struct TA_RetCode; // Example forward declaration
// #include <ta-lib/ta_libc.h> // Uncomment if TA-Lib is integrated

namespace Backtester {

    // Define FeatureMap type alias within this namespace
    using FeatureMap = std::unordered_map<std::string, double>;

    // Calculates technical features from market data
    // This is a stub implementation. Real implementation would use TA-Lib or similar.
    class FeatureCalculator {
    public:
        FeatureCalculator() {
            // Placeholder: Initialize TA-Lib here if needed
            // TA_RetCode retCode = TA_Initialize();
            // if (retCode!= TA_SUCCESS) {
            //     throw std::runtime_error("Failed to initialize TA-Lib");
            // }
            std::cout << "FeatureCalculator Stub Initialized." << std::endl;
        }

        virtual ~FeatureCalculator() {
            // Placeholder: Shutdown TA-Lib here if needed
            // TA_Shutdown();
        }

        // Calculates features based on the provided market data snapshot
        virtual FeatureMap calculate_features(const Common::DataSnapshot& data) {
            FeatureMap features; // Create the map to store results

            // Determine the correct key for the closing price ('Close' or 'close')
            std::string close_key = "";
            if (data.count("Close")) { // Check uppercase first (common convention)
                close_key = "Close";
            } else if (data.count("close")) { // Check lowercase as fallback
                close_key = "close";
            }

            if (!close_key.empty()) {
                 double close_price = data.at(close_key); // Get the close price once

                 // --- Corrected Assignments using Map Keys ---
                 features["price"] = close_price;       // Assign close price to "price" feature

                 // Placeholder for SMA calculation - Assign to a named key
                 // features["SMA_10"] = calculate_sma(close_price, 10); // If calculate_sma was implemented
                 features["SMA_10_stub"] = close_price; // Stub: Assign close price to "SMA_10_stub"

                 // Placeholder for RSI calculation - Assign to a named key
                 // features["RSI_14"] = calculate_rsi(close_price, 14); // If calculate_rsi was implemented
                 features["RSI_14_stub"] = 50.0;        // Stub: Assign 50.0 to "RSI_14_stub"

            } else {
                 // Handle case where no 'Close' or 'close' price is found
                 std::cerr << "Warning: 'Close'/'close' price not found in DataSnapshot for feature calculation." << std::endl;
                 // Assign default/zero values to expected keys
                 features["price"] = 0.0;
                 features["SMA_10_stub"] = 0.0;
                 features["RSI_14_stub"] = 50.0; // Default RSI often considered 50
            }

            // Assign the dummy feature
            features["dummy_feature"] = 1.0;

            // --- TA-Lib Lookback Handling Comment ---
            // ... (comment remains the same) ...

            return features; // Return the populated map
        }

        // Add private helper methods for actual calculations if needed
        // private:
        // double calculate_sma(...)
        // double calculate_rsi(...)
    }; // End of FeatureCalculator class

} // End of namespace Backtester