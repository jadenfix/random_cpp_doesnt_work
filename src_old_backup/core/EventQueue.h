#pragma once

#include "Event.h"
#include <queue>
#include <memory>   // For std::shared_ptr
#include <mutex>    // For potential thread safety later (not strictly needed now)
#include <condition_variable> // For potential thread safety later

class EventQueue {
private:
    // Using std::deque as the underlying container for std::queue is often efficient
    std::queue<EventPtr, std::deque<EventPtr>> queue_;
    // Mutex and condition variable for thread safety (optional for single-threaded backtest)
    // std::mutex mutex_;
    // std::condition_variable cond_var_;

public:
    EventQueue() = default;

    // Pushes an event pointer onto the queue
    void push(EventPtr event) {
        // std::lock_guard<std::mutex> lock(mutex_); // Lock if multithreaded
        queue_.push(std::move(event)); // Move the shared_ptr into the queue
        // cond_var_.notify_one(); // Notify waiting threads if multithreaded
    }

    // Pops an event pointer from the queue
    // Returns nullptr if the queue is empty
    EventPtr pop() {
        // std::lock_guard<std::mutex> lock(mutex_); // Lock if multithreaded
        if (queue_.empty()) {
            return nullptr; // Indicate empty queue
        }
        EventPtr event = std::move(queue_.front()); // Move ownership from queue
        queue_.pop();
        return event;
    }

     // Pops an event pointer, waiting if necessary (for multithreaded use)
    /*
    EventPtr wait_and_pop() {
        std::unique_lock<std::mutex> lock(mutex_);
        cond_var_.wait(lock, [this]{ return !queue_.empty(); }); // Wait until queue is not empty
        EventPtr event = std::move(queue_.front());
        queue_.pop();
        return event;
    }
    */


    // Checks if the queue is empty
    bool empty() const {
        // std::lock_guard<std::mutex> lock(mutex_); // Lock if multithreaded
        return queue_.empty();
    }

    // Gets the current size of the queue
    size_t size() const {
        // std::lock_guard<std::mutex> lock(mutex_); // Lock if multithreaded
        return queue_.size();
    }
};