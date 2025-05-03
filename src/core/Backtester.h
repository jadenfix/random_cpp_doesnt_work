#pragma once

// Core includes
#include "EventQueue.h"
#include "ExecutionHandler.h"
#include "Portfolio.h"
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
    bool continue_backtest_ = true; // Flag to control the main loop
    long event_count_ = 0;          // Counter for processed events
    // Stores orders waiting for the next market tick to simulate execution
    std::map<std::chrono::system_clock::time_point, std::vector<EventPtr>> pending_orders_;

public:
    // Constructor: Initializes components and links portfolio to strategy
    Backtester(
        std::string data_dir,
        std::unique_ptr<Strategy> strategy, // Takes ownership of strategy
        double initial_cash = 100000.0)
        : data_dir_(std::move(data_dir)),
          initial_cash_(initial_cash),
          strategy_(std::move(strategy)) // Declaration order matched in initializer list
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

    // Public entry point to start the backtest
    void run() {
        if (!setup()) {
            std::cerr << "Backtester setup failed. Aborting." << std::endl;
            return;
        }
        loop(); // Run the main event loop
        finish(); // Print final results
    }

private:
    // Loads data and prepares the simulation environment
    bool setup() {
        std::cout << "--- Backtester Setup ---" << std::endl;
        if (!data_manager_.loadData(data_dir_)) {
             std::cerr << "Failed to load market data from: " << data_dir_ << std::endl;
             return false;
        }
        symbols_ = data_manager_.getAllSymbols(); // Get symbols successfully loaded
        if (symbols_.empty()) {
             std::cerr << "No symbols loaded from data directory." << std::endl;
             return false;
        }
        std::cout << "Loaded symbols: ";
        for(const auto& s : symbols_) std::cout << s << " ";
        std::cout << std::endl;

        current_time_ = data_manager_.getCurrentTime(); // Set initial simulation time
        if (current_time_ == std::chrono::system_clock::time_point::min()) {
             std::cerr << "Warning: Initial simulation time not set (no valid data found?)." << std::endl;
        } else {
             std::cout << "Initial backtest time: " << formatTimestampUTC(current_time_) << std::endl;
        }
        std::cout << "------------------------" << std::endl;
        return true;
    }

    // The main event processing loop
    void loop() {
        std::cout << "\n--- Running Backtest Loop ---" << std::endl;
        // Loop continues as long as the flag is true
        while (continue_backtest_) {
            event_count_++;
            // Optional: Print progress update periodically
            if (event_count_ % 10000 == 0) { // Adjusted frequency
                std::cout << "... " << event_count_ << " events. Time: " << formatTimestampUTC(current_time_) << std::endl;
            }

            // 1. Fetch new market data (if available) and add MARKET event to queue
            update_market_data();

            // 2. Process all events currently in the queue
            bool processed_event_this_cycle = false;
            while (true) {
                 EventPtr event = event_queue_.pop();
                 if (!event) {
                     break; // Queue is empty for now
                 }
                 processed_event_this_cycle = true; // Mark that we processed something
                 current_time_ = event->timestamp; // Update simulation time to event time
                 handle_event(event);              // Dispatch the event to appropriate handlers
            }

            // 3. Check termination condition (Corrected logic using the flag)
            // Stop if data source is finished AND we didn't process any events this cycle
            // (meaning the queue was also empty or only had future events)
            if (!processed_event_this_cycle && data_manager_.isDataFinished()) {
                 std::cout << "Termination condition met: Data finished and event queue exhausted for current time." << std::endl;
                 continue_backtest_ = false; // Set the flag to exit the loop
            }
        } // End of while(continue_backtest_)
        std::cout << "--- Backtest Loop Finished ---" << std::endl;
    }

    // Fetches next market data snapshot and puts it on the event queue
    void update_market_data() {
        if (!data_manager_.isDataFinished()) {
            DataSnapshot snapshot = data_manager_.getNextBars(); // Advances DataManager's internal time
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
                    // Execute orders pending *for this market update* first
                    execute_pending_orders(*market_event);
                    // Update portfolio market values based on new prices
                    portfolio_->update_market_values(market_event->data);
                    // Record equity *after* market value update
                    portfolio_->record_equity(market_event->timestamp);
                    // Let the strategy react to the new market data
                    strategy_->handle_market_event(*market_event, event_queue_);
                }
                break;
            }
            case EventType::SIGNAL: {
                // Signals are informational; Orders are actionable
                auto signal_event = std::dynamic_pointer_cast<SignalEvent>(event);
                if(signal_event) {
                    std::cout << "SIGNAL Received: " << signal_event->symbol << " "
                           << (signal_event->direction == SignalDirection::LONG ? "LONG" : signal_event->direction == SignalDirection::SHORT ? "SHORT" : "FLAT")
                           << " @ " << formatTimestampUTC(signal_event->timestamp) << std::endl;
                    // In a more complex system, Portfolio/Risk Manager might convert Signal->Order
                }
                break;
            }
            case EventType::ORDER: {
                // Queue the order; it will be executed on the next MARKET event
                auto order_event = std::dynamic_pointer_cast<OrderEvent>(event);
                if(order_event) {
                     std::cout << "ORDER Queued: " << order_event->symbol << " "
                               << (order_event->direction == OrderDirection::BUY ? "BUY" : "SELL")
                               << " Qty: " << order_event->quantity
                               << " @ " << formatTimestampUTC(order_event->timestamp) << std::endl;
                     // Store order based on its timestamp, waiting for execution trigger
                     pending_orders_[order_event->timestamp].push_back(event);
                 }
                break;
            }
            case EventType::FILL: {
                auto fill_event = std::dynamic_pointer_cast<FillEvent>(event);
                if (fill_event) {
                    // Update portfolio with the execution results
                    portfolio_->handle_fill_event(*fill_event);
                    // Notify the strategy about the fill (it might adjust targets, etc.)
                    strategy_->handle_fill_event(*fill_event);
                }
                break;
            }
            default:
                // Optional: Log unknown event types
                // std::cerr << "Warning: Unknown event type encountered!" << std::endl;
                break;
        }
    }

    // Processes orders that were placed at or before the current market event's time
    void execute_pending_orders(const MarketEvent& current_market_event) {
         // Find orders placed at or before this market event's timestamp
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
        // Calls Portfolio's method which includes summary and metrics
        portfolio_->print_final_summary();
    }
}; // End of Backtester class definition