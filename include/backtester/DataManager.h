#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../common/Event.h" // Needs MarketEvent

namespace Backtester {
    class DataManager {
    public:
        virtual ~DataManager() = default;
        virtual bool load_data(const std::string& source) = 0;
        // Changed return type for better memory safety if MarketEvent contains complex data
        virtual std::optional<Common::MarketEvent> get_next_bar() = 0; // Use optional
        virtual void reset() = 0;
    };
    // Factory function declaration
    std::unique_ptr<DataManager> create_csv_data_manager();
}