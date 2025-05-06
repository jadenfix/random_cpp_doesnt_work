#pragma once

#include "../core/Event.h"
#include "../core/EventQueue.h"
#include "../core/Portfolio.h" // Include Portfolio header
#include <string>
#include <vector>
#include <map>
#include <memory> // For std::shared_ptr

class Strategy {
protected:
    Portfolio* portfolio_ = nullptr; // Pointer to the portfolio (non-owning)

public:
    virtual ~Strategy() = default;

    // --- NEW: Method to link portfolio ---
    // Called by the Backtester during setup
    virtual void set_portfolio(Portfolio* portfolio) {
        portfolio_ = portfolio;
    }

    // Called by the Backtester to process new market data
    virtual void handle_market_event(const MarketEvent& event, EventQueue& queue) = 0;

    // Called by the Backtester when one of the strategy's orders gets filled
    virtual void handle_fill_event(const FillEvent& event) {
        (void)event; // Suppress unused variable warning by default
    }

    // Helper to push events onto the queue
    void send_event(EventPtr event, EventQueue& queue) {
        queue.push(std::move(event));
    }
};