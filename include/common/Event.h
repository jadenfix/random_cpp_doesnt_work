#pragma once
#include <chrono>
#include <string>
#include <variant>
#include <unordered_map>
#include <memory>
#include "OrderTypes.h"
// --- Include the payload structs ---
#include "Signal.h"
#include "OrderRequest.h"
#include "FillEvent.h" // Contains FillDetails definition

namespace Backtester::Common {
    // Use unordered_map for DataSnapshot as per research doc
    using DataSnapshot = std::unordered_map<std::string, double>;

    enum class EventType { MARKET, SIGNAL, ORDER, FILL };

    class Event { /* ... base class as provided ... */
    public:
        virtual ~Event() = default;
        virtual EventType getType() const = 0;
        std::chrono::time_point<std::chrono::system_clock> timestamp;
    protected:
        Event() : timestamp(std::chrono::system_clock::now()) {}
        explicit Event(std::chrono::time_point<std::chrono::system_clock> ts) : timestamp(ts) {}
    };

    class MarketEvent : public Event { /* ... as provided ... */
    public:
        MarketEvent(std::chrono::time_point<std::chrono::system_clock> ts, std::string sym, DataSnapshot data)
            : Event(ts), symbol(std::move(sym)), marketData(std::move(data)) {}
        EventType getType() const override { return EventType::MARKET; }
        std::string symbol;
        DataSnapshot marketData;
    };

    class SignalEvent : public Event { /* ... as provided ... */
    public:
        SignalEvent(std::chrono::time_point<std::chrono::system_clock> ts, const Common::Signal& sig)
            : Event(ts), signalDetails(sig) {}
        EventType getType() const override { return EventType::SIGNAL; }
        const Common::Signal& signalDetails; // Use const& for payload
    };

    class OrderEvent : public Event { /* ... as provided ... */
     public:
        OrderEvent(std::chrono::time_point<std::chrono::system_clock> ts, const Common::OrderRequest& req)
            : Event(ts), orderRequest(req) {}
        EventType getType() const override { return EventType::ORDER; }
        const Common::OrderRequest& orderRequest; // Use const& for payload
    };

    class FillEvent : public Event { /* ... as provided ... */
     public:
        FillEvent(std::chrono::time_point<std::chrono::system_clock> ts, const Common::FillDetails& fill)
            : Event(ts), fillDetails(fill) {}
        EventType getType() const override { return EventType::FILL; }
        const Common::FillDetails& fillDetails; // Use const& for payload
    };

     // --- Event Pointer Alias (Optional but convenient) ---
     using EventPtr = std::unique_ptr<Event>; // Use unique_ptr if ownership is transferred

}