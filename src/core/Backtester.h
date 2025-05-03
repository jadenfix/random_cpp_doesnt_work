#pragma once

#include "EventQueue.h"
#include "ExecutionHandler.h"
#include "Portfolio.h"
#include "data/DataManager.h" // Corrected include path
#include "strategies/Strategy.h" // Corrected include path
#include "core/Utils.h" // Include for formatting timestamp
#include <vector>
#include <memory>
#include <string>
#include <chrono>
#include <iostream>
#include <map> // Include map for pending orders example

class Backtester {
private:
    // Declare members roughly in initialization order
    std::string data_dir_;
    double initial_cash_;
    std::unique_ptr<Strategy> strategy_; // Strategy ownership

    EventQueue event_queue_;
    DataManager data_manager_;
    std::unique_ptr<Portfolio> portfolio_;
    std::unique_ptr<ExecutionHandler> execution_handler_;

    std::vector<std::string> symbols_; // Symbols actually loaded
    std::chrono::system_clock::time_point current_time_;
    bool continue_backtest_ = true;
    long event_count_ = 0;

    // --- Simple mechanism to hold orders until next market event ---
    std::map<std::chrono::system_clock::time_point, std::vector<EventPtr>> pending_orders_;


public:
    // Constructor: Match initializer list order to declaration order
    Backtester(
        std::string data_dir,
        std::unique_ptr<Strategy> strategy, // Pass ownership
        double initial_cash = 100000.0)
        : data_dir_(std::move(data_dir)),     // 1st declared
          initial_cash_(initial_cash),       // 2nd declared
          strategy_(std::move(strategy))     // 3rd declared
          // event_queue_, data_manager_ are default constructed
          // portfolio_, execution_handler_ initialized below
    {
        portfolio_ = std::make_unique<Portfolio>(initial_cash_);
        execution_handler_ = std::make_unique<ExecutionHandler>(event_queue_); // Pass queue ref
    }

    void run() { /* ... same as before ... */
        if (!setup()) { std::cerr << "Backtester setup failed.\n"; return; }
        loop();
        finish();
    }

private:
    bool setup() { /* ... same as before ... */
         std::cout << "--- Backtester Setup ---" << std::endl;
        if (!data_manager_.loadData(data_dir_)) { return false; }
        symbols_ = data_manager_.getAllSymbols();
        if (symbols_.empty()) { std::cerr << "No symbols loaded.\n"; return false; }
        std::cout << "Loaded symbols: "; for(const auto& s : symbols_) std::cout << s << " "; std::cout << std::endl;
        current_time_ = data_manager_.getCurrentTime();
        std::cout << "Initial backtest time: " << formatTimestampUTC(current_time_) << std::endl; // Use formatter
        std::cout << "------------------------" << std::endl;
        return true;
    }

    void loop() { /* ... same outer loop structure ... */
        std::cout << "\n--- Running Backtest Loop ---" << std::endl;
        while (continue_backtest_) {
            event_count_++;
            if (event_count_ % 5000 == 0) { // Progress update
                std::cout << "... " << event_count_ << " events. Time: " << formatTimestampUTC(current_time_) << std::endl;
            }

            update_market_data(); // Potentially adds MARKET event to queue

            while (true) { // Process events for this time step
                 EventPtr event = event_queue_.pop();
                 if (!event) break;
                 current_time_ = event->timestamp; // Update time based on event
                 handle_event(event); // Dispatch
            }

            if (event_queue_.empty() && data_manager_.isDataFinished()) {
                continue_backtest_ = false;
            }
        }
        std::cout << "--- Backtest Loop Finished ---" << std::endl;
    }

    void update_market_data() { /* ... same as before ... */
         if (!data_manager_.isDataFinished()) {
            DataSnapshot snapshot = data_manager_.getNextBars();
            if (!snapshot.empty()) {
                auto market_timestamp = data_manager_.getCurrentTime();
                EventPtr market_ev = std::make_shared<MarketEvent>(market_timestamp, std::move(snapshot));
                event_queue_.push(std::move(market_ev));
            }
        }
    }

    // Event Dispatch Logic
    void handle_event(const EventPtr& event) {
        switch (event->type) {
            case EventType::MARKET: {
                auto market_event = std::dynamic_pointer_cast<MarketEvent>(event);
                if (market_event) {
                    // Execute pending orders *before* updating portfolio/strategy
                    execute_pending_orders(*market_event);

                    // Update portfolio market values & record equity
                    portfolio_->update_market_values(market_event->data);
                    portfolio_->record_equity(market_event->timestamp);

                    // Pass data to strategy
                    strategy_->handle_market_event(*market_event, event_queue_);
                }
                break;
            }
            case EventType::SIGNAL: {
                // Portfolio/Risk management would handle signals to generate orders
                // For now, Strategy generates Orders directly, so this is mostly logged
                 auto signal_event = std::dynamic_pointer_cast<SignalEvent>(event);
                 std::cout << "SIGNAL Received: " << signal_event->symbol << " "
                           << (signal_event->direction == SignalDirection::LONG ? "LONG" : signal_event->direction == SignalDirection::SHORT ? "SHORT" : "FLAT")
                           << " @ " << formatTimestampUTC(signal_event->timestamp) << std::endl;
                break;
            }
            case EventType::ORDER: {
                // Queue the order to be processed on the *next* market event
                auto order_event = std::dynamic_pointer_cast<OrderEvent>(event);
                if(order_event) {
                     std::cout << "ORDER Queued: " << order_event->symbol << " "
                               << (order_event->direction == OrderDirection::BUY ? "BUY" : "SELL")
                               << " Qty: " << order_event->quantity
                               << " @ " << formatTimestampUTC(order_event->timestamp) << std::endl;
                     // We queue based on the order's timestamp. Execution happens using the NEXT market event.
                     pending_orders_[order_event->timestamp].push_back(event);
                 }
                break;
            }
            case EventType::FILL: {
                auto fill_event = std::dynamic_pointer_cast<FillEvent>(event);
                if (fill_event) {
                    portfolio_->handle_fill_event(*fill_event);
                    strategy_->handle_fill_event(*fill_event); // Notify strategy
                }
                break;
            }
            default: break; // Ignore unknown
        }
    }

    // Process orders queued before or at the current market event time
    void execute_pending_orders(const MarketEvent& current_market_event) {
         // Iterate through orders placed at or before the current market time
         auto end_it = pending_orders_.upper_bound(current_market_event.timestamp);
         for (auto it = pending_orders_.begin(); it != end_it; ++it) {
             for (const auto& order_ptr : it->second) {
                  auto order_event = std::dynamic_pointer_cast<OrderEvent>(order_ptr);
                  if(order_event) {
                     // Execute using the current market data (represents "next tick")
                     execution_handler_->handle_order_event(*order_event, current_market_event);
                  }
             }
         }
         // Remove processed orders from the pending map
         pending_orders_.erase(pending_orders_.begin(), end_it);
    }


    // Suppress warning for unused function (until implemented)
    void handle_market_event_for_execution(const MarketEvent& market_event) {
        (void)market_event; // Mark as unused
    }

    void finish() { /* ... same as before, uses portfolio_->print_summary() ... */
        std::cout << "\n--- Backtest Finished ---" << std::endl;
        std::cout << "Total events processed: " << event_count_ << std::endl;
        std::cout << "Final time: " << formatTimestampUTC(current_time_) << std::endl; // Use formatter
        portfolio_->print_summary();
    }
};