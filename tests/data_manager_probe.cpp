#include "backtester/DataManager.h"
#include <iomanip>
#include <iostream>

// A test-only probe of the production CSV reader, without strategy calculations.
int main(int argc, char** argv) {
    if (argc != 2) return 2;
    auto reader = Backtester::create_csv_data_manager();
    if (!reader->load_data(argv[1])) return 2;
    while (auto event = reader->get_next_bar()) {
        std::cout << "BAR\t" << event->symbol << '\t' << std::setprecision(17)
                  << event->marketData.at("close") << '\t'
                  << event->marketData.at("volume") << '\n';
    }
}
