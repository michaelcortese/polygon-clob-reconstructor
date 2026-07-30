#pragma once
#include <string>
#include <unordered_map>
#include <memory>
#include <vector>
#include "OrderBook.hpp"
#include "Execution.hpp"

class OrderBookManager {
public:
    // Get or create an OrderBook for the given tokenId
    std::shared_ptr<OrderBook> get_or_create_book(const std::string& token_id);

    // Process a batch of executions, routing each to the correct book
    void process_events(const std::vector<Execution>& events, uint64_t block_number);

    // Getters
    const std::unordered_map<std::string, std::shared_ptr<OrderBook>>& books() const { return books_; }
    size_t book_count() const { return books_.size(); }
    uint64_t total_events_processed() const { return total_events_; }

    // Summary: per-market stats
    void log_summary() const;

private:
    std::unordered_map<std::string, std::shared_ptr<OrderBook>> books_;
    uint64_t total_events_ = 0;
};
