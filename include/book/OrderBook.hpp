#pragma once
#include <string>
#include <map>
#include <vector>
#include "Execution.hpp"
#include "Order.hpp"

class OrderBook {
public:
    explicit OrderBook(const std::string& token_id);

    // Process a trade (from OrderFilled event)
    void process_trade(const Execution& exec);

    // Getters
    const std::string& token_id() const { return token_id_; }
    uint32_t best_bid() const;
    uint32_t best_ask() const;
    uint32_t mid_price() const;
    uint32_t spread() const;  // in cents
    uint64_t total_volume() const { return total_volume_; }
    size_t trade_count() const { return trades_.size(); }
    const std::vector<Execution>& trades() const { return trades_; }

    // Level2 depth
    std::map<uint32_t, uint32_t, std::greater<uint32_t>> bids_;  // price → size, descending
    std::map<uint32_t, uint32_t> asks_;                           // price → size, ascending

private:
    std::string token_id_;
    std::vector<Execution> trades_;
    uint64_t total_volume_ = 0;
};