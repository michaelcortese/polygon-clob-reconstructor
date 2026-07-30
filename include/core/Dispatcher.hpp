#pragma once
#include <chrono>
#include <unordered_map>
#include "book/OrderBookManager.hpp"
#include "parser/PolygonParser.hpp"

struct DispatchStats {
    std::unordered_map<std::string, size_t> event_counts_per_book;
    size_t blocks_processed = 0;
    size_t total_events = 0;
    std::chrono::steady_clock::time_point start_time;
    
    double elapsed_seconds() const {
        return std::chrono::duration<double>(
            std::chrono::steady_clock::now() - start_time).count();
    }
};

class Dispatcher {
public:
    Dispatcher(PolygonParser& parser, OrderBookManager& obm);
    
    // Process one block: fetch events, route to OrderBookManager
    // Returns the number of events processed for this block
    size_t dispatch_next_block();
    
    // Print current stats
    void print_stats() const;
    
    // Getters
    const DispatchStats& stats() const { return stats_; }
    
private:
    PolygonParser& parser_;
    OrderBookManager& obm_;
    DispatchStats stats_;
};