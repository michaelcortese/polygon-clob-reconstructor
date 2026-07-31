#pragma once
#include <cstdint>
#include <string>
#include <vector>

struct MarketSnapshot {
    uint64_t block_number;
    std::string token_id;
    uint32_t best_bid;
    uint32_t best_ask;
    uint32_t mid_price;
    uint32_t spread;
    uint64_t bid_depth_1pct;   // total bid qty within 1% of best bid
    uint64_t ask_depth_1pct;   // total ask qty within 1% of best ask
    uint64_t total_volume;     // cumulative volume
    size_t trade_count;
    double vwap;               // volume-weighted average price
};

class MarketStats {
public:
    // Compute a snapshot from an OrderBook at the given block
    static MarketSnapshot snapshot(const class OrderBook& book, uint64_t block_number);
};