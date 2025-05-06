#include "backtester/Backtester.h"
#include "backtester/Strategy.h"
// --- Include ALL implemented strategy headers ---
#include "strategies/MovingAverageCrossover.h"
#include "strategies/VWAPReversion.h"
#include "strategies/OpeningRangeBreakout.h"
#include "strategies/MomentumIgnition.h"
#include "strategies/PairsTrading.h"
#include "strategies/LeadLagStrategy.h"
// --- DRL Includes (can be commented out if not running stub) ---
// #include "models/DRLStrategy.h"
// #include "models/DRLInferenceEngine.h"
// #include "features/FeatureCalculator.h"


#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <stdexcept>
#include <map>
#include <iomanip>
#include <functional>
#include <filesystem>

// --- StrategyResult struct defined in Portfolio.h ---
#include "backtester/Portfolio.h" // Use core/ path

// --- Helper Function to Build Data Path ---
std::string build_data_path(const std::string& base_dir, const std::string& subdir_name) {
    std::filesystem::path path = std::filesystem::path(base_dir) / subdir_name;
    return path.string();
}


int main(int argc, char* argv[]) {
    std::cout << "--- HFT Backtesting System - Comprehensive Multi-Strategy & Multi-Dataset Run ---" << std::endl;

    // --- Configuration ---
    std::string data_base_dir = "../data";
    double initial_cash = 100000.0; // <-- Variable name is initial_cash

    // --- Define Datasets to Test ---
    std::vector<std::string> datasets_to_test = {
        "stocks_april",
        "2024_only",
        "2024_2025"
    };

    // --- Map to Store All Results ---
    std::map<std::string, Backtester::StrategyResult> all_results;


    // --- OUTER LOOP: Iterate Through Datasets ---
    for (const std::string& target_dataset_subdir : datasets_to_test) {

        std::cout << "\n\n///////////////////////////////////////////////////////////" << std::endl;
        std::cout << "///// Starting Tests for Dataset: " << target_dataset_subdir << " /////" << std::endl;
        std::cout << "///////////////////////////////////////////////////////////" << std::endl;

        std::string data_path = build_data_path(data_base_dir, target_dataset_subdir);
        std::cout << "Using data path: " << data_path << std::endl;
        if (!std::filesystem::exists(data_path) || !std::filesystem::is_directory(data_path)) {
            std::cerr << "ERROR: Data directory '" << data_path << "' not found. Skipping dataset." << std::endl;
            continue;
        }

        // --- Define Symbol Names BASED ON CURRENT DATASET ---
        std::string msft_sym = "", nvda_sym = "", goog_sym = "";
        std::string btc_sym = "", eth_sym = "", sol_sym = "", ada_sym = "";
        // --- Define drl_symbols HERE ---
        std::vector<std::string> drl_symbols; // Define before the if/else block

        if (target_dataset_subdir == "stocks_april") { /* ... define stock symbols ... */
            msft_sym = "quant_seconds_data_MSFT"; nvda_sym = "quant_seconds_data_NVDA"; goog_sym = "quant_seconds_data_google";
            drl_symbols = {msft_sym, nvda_sym, goog_sym}; // Populate here
        } else if (target_dataset_subdir == "2024_only") { /* ... define 2024 crypto symbols ... */
             btc_sym = "btc_2024_data"; eth_sym = "eth_2024_data"; sol_sym = "sol_2024_data"; ada_sym = "ada_2024_data";
             drl_symbols = {btc_sym, eth_sym, sol_sym, ada_sym}; // Populate here
        } else if (target_dataset_subdir == "2024_2025") { /* ... define 2024-2025 crypto symbols ... */
             btc_sym = "2024_to_april_2025_btc_data"; eth_sym = "2024_to_april_2025_eth_data"; sol_sym = "2024_to_april_2025_solana_data"; ada_sym = "2024_to_april_2025_ada_data";
             drl_symbols = {btc_sym, eth_sym, sol_sym, ada_sym}; // Populate here
        } else { continue; }


        // --- Define All Available Strategy Configurations ---
        struct StrategyConfig {
            std::string name;
            std::function<std::unique_ptr<Backtester::StrategyBase>()> factory;
            std::vector<std::string> required_datasets;
        };
        std::vector<StrategyConfig> available_strategies_this_iteration;

        // Add standard strategies (Removed size parameter from constructors)
        available_strategies_this_iteration.push_back({"MACrossover_5_20", [](){ return std::make_unique<Backtester::MovingAverageCrossover>(5, 20); }, {"stocks_april", "2024_only", "2024_2025"}}); // 2 args
        available_strategies_this_iteration.push_back({"VWAP_2.0", [](){ return std::make_unique<Backtester::VWAPReversion>(2.0); }, {"stocks_april", "2024_only", "2024_2025"}}); // 1 arg
        available_strategies_this_iteration.push_back({"ORB_30", [](){ return std::make_unique<Backtester::OpeningRangeBreakout>(30); }, {"stocks_april", "2024_only", "2024_2025"}}); // 1 arg
        available_strategies_this_iteration.push_back({"Momentum_5_10_2_3", [](){ return std::make_unique<Backtester::MomentumIgnition>(5, 10, 2.0, 3); }, {"stocks_april", "2024_only", "2024_2025"}}); // 4 args

        // Add Pairs Trading
        double pairs_trade_value = 10000.0; size_t pairs_lookback = 60; double pairs_entry_z = 2.0, pairs_exit_z = 0.5;
        // ... (Pairs configs using [&] capture and Backtester::PairsTrading) ...
        if (!msft_sym.empty() && !nvda_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_MSFT_NVDA", [&](){ return std::make_unique<Backtester::PairsTrading>(msft_sym, nvda_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"stocks_april"}});
        if (!nvda_sym.empty() && !goog_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_NVDA_GOOG", [&](){ return std::make_unique<Backtester::PairsTrading>(nvda_sym, goog_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"stocks_april"}});
        if (!msft_sym.empty() && !goog_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_MSFT_GOOG", [&](){ return std::make_unique<Backtester::PairsTrading>(msft_sym, goog_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"stocks_april"}});
        if (!btc_sym.empty() && !eth_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_BTC_ETH", [&](){ return std::make_unique<Backtester::PairsTrading>(btc_sym, eth_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"2024_only", "2024_2025"}});
        if (!eth_sym.empty() && !sol_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_ETH_SOL", [&](){ return std::make_unique<Backtester::PairsTrading>(eth_sym, sol_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"2024_only", "2024_2025"}});
        if (!btc_sym.empty() && !sol_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_BTC_SOL", [&](){ return std::make_unique<Backtester::PairsTrading>(btc_sym, sol_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"2024_only", "2024_2025"}});
        if (!eth_sym.empty() && !ada_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_ETH_ADA", [&](){ return std::make_unique<Backtester::PairsTrading>(eth_sym, ada_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"2024_only", "2024_2025"}});
        if (!sol_sym.empty() && !ada_sym.empty()) available_strategies_this_iteration.push_back({"Pairs_SOL_ADA", [&](){ return std::make_unique<Backtester::PairsTrading>(sol_sym, ada_sym, pairs_lookback, pairs_entry_z, pairs_exit_z, pairs_trade_value); }, {"2024_only", "2024_2025"}});


        // Add Lead-Lag Strategies (Removed size parameter)
        size_t leadlag_window = 30; size_t leadlag_lag = 1; double leadlag_corr = 0.5, leadlag_ret = 0.0002;
        // ... (LeadLag configs using [&] capture and Backtester::LeadLagStrategy) ...
        if (!msft_sym.empty() && !nvda_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_MSFT->NVDA", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(msft_sym, nvda_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"stocks_april"}});
        if (!nvda_sym.empty() && !msft_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_NVDA->MSFT", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(nvda_sym, msft_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"stocks_april"}});
        if (!btc_sym.empty() && !eth_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_BTC->ETH", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(btc_sym, eth_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"2024_only", "2024_2025"}});
        if (!eth_sym.empty() && !btc_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_ETH->BTC", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(eth_sym, btc_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"2024_only", "2024_2025"}});
        if (!eth_sym.empty() && !sol_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_ETH->SOL", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(eth_sym, sol_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"2024_only", "2024_2025"}});
        if (!sol_sym.empty() && !eth_sym.empty()) available_strategies_this_iteration.push_back({"LeadLag_SOL->ETH", [&](){ return std::make_unique<Backtester::LeadLagStrategy>(sol_sym, eth_sym, leadlag_window, leadlag_lag, leadlag_corr, leadlag_ret); }, {"2024_only", "2024_2025"}});


        // --- DRL Strategy Stub Config (COMMENTED OUT UNTIL IMPLEMENTED) ---
        /*
        if (!drl_symbols.empty()) {
             auto feature_calculator = std::make_shared<Backtester::FeatureCalculator>();
             auto drl_engine = std::make_unique<Backtester::DRLInferenceEngine>(); // Default constructor
             // *** NEED TO ADD load_model method to DRLInferenceEngine ***
             // std::string model_file = "path/to/your/drl_model.onnx";
             // bool model_loaded = drl_engine->load_model(model_file);
             bool model_loaded = true; // Assume loaded for stub

             if (model_loaded) {
                 available_strategies_this_iteration.push_back({"DRL_Agent_Stub",
                    // Correct capture & instantiation depends on DRLStrategy constructor
                    // Example assumes DRLStrategy takes FeatureCalculator&, DRLInferenceEngine&, vector<string>, double size
                    [fc_ref = *feature_calculator, engine_ref = *drl_engine, syms = drl_symbols, size = 100.0]() mutable -> std::unique_ptr<Backtester::StrategyBase> {
                         return std::make_unique<Backtester::DRLStrategy>(fc_ref, engine_ref, syms, size); // Pass refs
                    },
                    {"stocks_april", "2024_only", "2024_2025"}
                 });
             } else { // Handle model not loaded }
        }
        */
        // --- END DRL Config ---


        // --- Filter strategies applicable to the CURRENT dataset ---
        std::vector<StrategyConfig> strategies_to_run_this_dataset;
        // ... (Filtering logic remains the same) ...
        for (const auto& config : available_strategies_this_iteration) {
            bool applicable = false;
            for (const auto& ds_name : config.required_datasets) { if (ds_name == target_dataset_subdir) { applicable = true; break; } }
            if (applicable && config.factory != nullptr) strategies_to_run_this_dataset.push_back(config);
        }
        if (strategies_to_run_this_dataset.empty()) { /* ... skip dataset ... */ continue; }
        std::cout << "Preparing to run " << strategies_to_run_this_dataset.size() << " strategies for dataset '" << target_dataset_subdir << "'." << std::endl;


        // --- INNER LOOP: Iterate Through Applicable Strategies ---
        for (const auto& config : strategies_to_run_this_dataset) {
            std::cout << "\n\n===== Running Strategy: " << config.name << " on Dataset: " << target_dataset_subdir << " =====" << std::endl;
            std::unique_ptr<Backtester::StrategyBase> strategy = nullptr;
            try { strategy = config.factory(); }
            catch (...) { /* ... error handling ... */ continue; }
            if (!strategy) { /* ... error handling ... */ continue; }

            // --- Create components INSIDE the strategy loop ---
            std::unique_ptr<Backtester::DataManager> data_manager = Backtester::create_csv_data_manager();
            if (!data_manager || !data_manager->load_data(data_path)) { continue; }

            // --- CORRECTED: Use initial_cash variable ---
            Backtester::Portfolio portfolio(initial_cash);
            Backtester::ExecutionSimulator execution_simulator;
            // DRL components needed if running DRL stub
            // Backtester::FeatureCalculator feature_calc_instance;
            // Backtester::DRLInferenceEngine drl_engine_instance;
            // --- Needs strategy->set_portfolio if strategy requires it ---
            // strategy->set_portfolio(&portfolio); // Need to add this method to base/derived strategies

            Backtester::Backtester backtester(*data_manager, *strategy, portfolio, execution_simulator);
            Backtester::Portfolio const* result_portfolio = nullptr;

            try {
                 backtester.run();
                 result_portfolio = &portfolio;
            } catch (...) { /* ... error handling ... */ continue; }

            if (result_portfolio) {
                std::string result_key = config.name + "_on_" + target_dataset_subdir;
                all_results[result_key] = result_portfolio->get_results_summary();
            } else { /* ... warning ... */ }
            std::cout << "===== Finished Strategy: " << config.name << " on " << target_dataset_subdir << " =====" << std::endl;
        } // End INNER strategy loop

    } // End OUTER dataset loop


    // --- Print Combined Comparison Table ---
    if (!all_results.empty()) { /* ... Printing logic ... */ }
    else { /* ... no results ... */ }


    std::cout << "\n--- Comprehensive Run Invocation Complete ---" << std::endl;
    return 0;
}