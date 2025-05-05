#pragma once

// Core includes
#include "EventQueue.h"
#include "ExecutionHandler.h"
#include "Portfolio.h" // Includes StrategyResult struct definition
#include "core/Utils.h" // Utility functions like formatTimestampUTC

// Component includes (using paths relative to src/)
#include "data/DataManager.h"
#include "strategies/Strategy.h"

// Standard library includes
#include <vector>
#include <memory> // For std::unique_ptr, std::shared_ptr
#include <string>
#include <chrono>
#include <iostream>
#include <map>
#include <stdexcept> // For std::runtime_error

class Backtester {
private:
    // --- Configuration & State ---
    std::string data_dir_;
    double initial_cash_;
    std::unique_ptr<Strategy> strategy_; // Owns the strategy object

    // --- Core Components ---
    EventQueue event_queue_;
    DataManager data_manager_; // Owns the data manager
    std::unique_ptr<Portfolio> portfolio_; // Owns the portfolio object
    std::unique_ptr<ExecutionHandler> execution_handler_; // Owns the execution handler

    // --- Runtime Variables ---
    std::vector<std::string> symbols_; // Symbols actually loaded from data
    std::chrono::system_clock::time_point current_time_; // Tracks simulation time
    bool continue_backtest_ = true; // Flag to control the main loop (now used correctly)
    long event_count_ = 0;          // Counter for processed events
    // Stores orders waiting for the next market tick to simulate execution
    std::map<std::chrono::system_clock::time_point, std::vector<EventPtr>> pending_orders_;
    // --- Risk Management Setting ---
    double minimum_equity_buffer_ = 1000.0; // Minimum equity required to place new orders


public:
    // Constructor: Initializes components and links portfolio to strategy
    Backtester(
        std::string data_dir,
        std::unique_ptr<Strategy> strategy, // Takes ownership of strategy
        double initial_cash = 100000.0,
        double min_equity_buffer = 1000.0) // Optional: Allow setting buffer
        : data_dir_(std::move(data_dir)),
          initial_cash_(initial_cash),
          strategy_(std::move(strategy)), // Declaration order matched in initializer list
          minimum_equity_buffer_(min_equity_buffer)
          // event_queue_, data_manager_ are default constructed
    {
        // Initialize components dependent on others after the initializer list
        portfolio_ = std::make_unique<Portfolio>(initial_cash_);
        execution_handler_ = std::make_unique<ExecutionHandler>(event_queue_); // Pass queue reference

        // Link portfolio to strategy (essential for position awareness)
        if (strategy_) {
             strategy_->set_portfolio(portfolio_.get()); // Pass raw pointer (Strategy does not own Portfolio)
        } else {
             // Throw if strategy is null, indicating a setup error
             throw std::runtime_error("Strategy provided to Backtester is null!");
        }
    }

    // --- Original Run Method (can keep or remove) ---
    void run() {
        if (!setup()) {
            std::cerr << "Backtester setup failed. Aborting." << std::endl;
            return;
        }
        loop(); // Run the main event loop
        finish(); // Print final results
    }

    // --- NEW: Run method that returns portfolio pointer for results ---
    Portfolio* run_and_get_portfolio() { // Return raw pointer (caller doesn't own)
        if (!setup()) {
            std::cerr << "Backtester setup failed. Aborting." << std::endl;
            return nullptr; // Return null on setup failure
        }
        loop(); // Run the main event loop
        finish(); // Print final results
        return portfolio_.get(); // Return pointer to the portfolio for result extraction
    }

private:
    // Loads data and prepares the simulation environment
    bool setup() {
        std::cout << "--- Backtester Setup ---" << std::endl;
        // Reset state for potentially running multiple times
        event_queue_ = EventQueue();
        pending_orders_.clear();
        data_manager_ = DataManager();
        portfolio_ = std::make_unique<Portfolio>(initial_cash_);
        if(strategy_) strategy_->set_portfolio(portfolio_.get());
        else return false;
        execution_handler_ = std::make_unique<ExecutionHandler>(event_queue_);
        continue_backtest_ = true;
        event_count_ = 0;


        if (!data_manager_.loadData(data_dir_)) {
             std::cerr << "Failed to load market data from: " << data_dir_ << std::endl;
             return false;
        }
        symbols_ = data_manager_.getAllSymbols();
        if (symbols_.empty()) {
             std::cerr << "No symbols loaded from data directory." << std::endl;
             return false;
        }
        std::cout << "Loaded symbols: ";
        for(const auto& s : symbols_) std::cout << s << " ";
        std::cout << std::endl;

        current_time_ = data_manager_.getCurrentTime();
        if (current_time_ == std::chrono::system_clock::time_point::min()) {
             std::cerr << "Warning: Initial simulation time not set (no valid data found?)." << std::endl;
             return false;
        } else {
             std::cout << "Initial backtest time: " << formatTimestampUTC(current_time_) << std::endl;
        }
        std::cout << "------------------------" << std::endl;
        return true;
    }

    // The main event processing loop
    void loop() {
        std::cout << "\n--- Running Backtest Loop ---" << std::endl;
        while (continue_backtest_) { // Uses the flag correctly
            event_count_++;
            if (event_count_ % 10000 == 0) {
                std::cout << "... " << event_count_ << " events. Time: " << formatTimestampUTC(current_time_) << std::endl;
            }

            update_market_data(); // Add MARKET event if available

            bool processed_event_this_cycle = false;
            while (true) {
                 EventPtr event = event_queue_.pop();
                 if (!event) break; // Queue empty for now
                 processed_event_this_cycle = true;
                 current_time_ = event->timestamp;
                 handle_event(event); // Dispatch
            }

            // Check termination condition
            if (!processed_event_this_cycle && data_manager_.isDataFinished()) {
                 std::cout << "Termination condition met: Data finished and event queue exhausted for current time." << std::endl;
                 continue_backtest_ = false; // Set flag to exit loop
            }
        }
        std::cout << "--- Backtest Loop Finished ---" << std::endl;
    }

    // Fetches next market data snapshot and puts it on the event queue
    void update_market_data() {
        if (!data_manager_.isDataFinished()) {
            DataSnapshot snapshot = data_manager_.getNextBars();
            if (!snapshot.empty()) {
                auto market_timestamp = data_manager_.getCurrentTime();
                EventPtr market_ev = std::make_shared<MarketEvent>(market_timestamp, std::move(snapshot));
                event_queue_.push(std::move(market_ev));
            }
        }
    }

    // Routes events to the correct handlers based on type
    void handle_event(const EventPtr& event) {
        switch (event->type) {
            case EventType::MARKET: {
                auto market_event = std::dynamic_pointer_cast<MarketEvent>(event);
                if (market_event) {
                    execute_pending_orders(*market_event); // Execute orders based on *this* market data
                    portfolio_->update_market_values(market_event->data); // Update portfolio values
                    portfolio_->record_equity(market_event->timestamp);  // Record equity
                    strategy_->handle_market_event(*market_event, event_queue_); // Let strategy react
                }
                break;
            }
            case EventType::SIGNAL: {
                // Primarily informational, Portfolio/Risk would act on these in a real system
                auto signal_event = std::dynamic_pointer_cast<SignalEvent>(event);
                if(signal_event) {
                    std::cout << "SIGNAL Received: " << signal_event->symbol << " "
                           << (signal_event->direction == SignalDirection::LONG ? "LONG" : signal_event->direction == SignalDirection::SHORT ? "SHORT" : "FLAT")
                           << " @ " << formatTimestampUTC(signal_event->timestamp) << std::endl;
                }
                break;
            }
            case EventType::ORDER: {
                auto order_event = std::dynamic_pointer_cast<OrderEvent>(event);
                if(order_event) {
                    // --- ADDED: Basic Equity Check Before Queuing Order ---
                    if (portfolio_ && portfolio_->get_total_equity() < minimum_equity_buffer_) {
                         std::cout << "ORDER REJECTED (Low Equity): Cannot queue order for "
                                   << order_event->symbol << ". Equity " << portfolio_->get_total_equity()
                                   << " < " << minimum_equity_buffer_ << std::endl;
                         break; // Discard order event, do not queue
                    }
                    // --- END Equity Check ---

                    // If equity check passes, queue the order
                     std::cout << "ORDER Queued: " << order_event->symbol << " "
                               << (order_event->direction == OrderDirection::BUY ? "BUY" : "SELL")
                               << " Qty: " << order_event->quantity
                               << " @ " << formatTimestampUTC(order_event->timestamp) << std::endl;
                     // Store order based on its timestamp, waiting for execution trigger (next MARKET event)
                     pending_orders_[order_event->timestamp].push_back(event);
                 }
                break;
            }
            case EventType::FILL: {
                auto fill_event = std::dynamic_pointer_cast<FillEvent>(event);
                if (fill_event) {
                    portfolio_->handle_fill_event(*fill_event); // Update portfolio
                    strategy_->handle_fill_event(*fill_event); // Notify strategy
                }
                break;
            }
            default:
                break; // Ignore unknown event types
        }
    }

    // Processes orders that were placed at or before the current market event's time
    void execute_pending_orders(const MarketEvent& current_market_event) {
         auto end_it = pending_orders_.upper_bound(current_market_event.timestamp);
         for (auto it = pending_orders_.begin(); it != end_it; ++it) {
             for (const auto& order_ptr : it->second) {
                  auto order_event = std::dynamic_pointer_cast<OrderEvent>(order_ptr);
                  if(order_event) {
                     // Pass the order and the *current* market data (representing the "next tick" state)
                     // to the execution handler for simulated filling.
                     execution_handler_->handle_order_event(*order_event, current_market_event);
                  }
             }
         }
         // Remove the processed orders from the pending map
         pending_orders_.erase(pending_orders_.begin(), end_it);
    }

    // Prints the final portfolio summary and performance metrics
    void finish() {
        std::cout << "\n--- Backtest Finished ---" << std::endl;
        std::cout << "Total events processed: " << event_count_ << std::endl;
        std::cout << "Final time: " << formatTimestampUTC(current_time_) << std::endl;
        if (portfolio_) {
             portfolio_->print_final_summary(); // Calls Portfolio's summary/metrics method
        } else {
             std::cerr << "Error: Portfolio is null during finish()." << std::endl;
        }
    }
}; // End of Backtester class definition