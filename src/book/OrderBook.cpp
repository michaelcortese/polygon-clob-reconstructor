#include "book/OrderBook.hpp"

OrderBook::OrderBook(const std::string& token_id)
    : token_id_(token_id) {}

void OrderBook::process_trade(const Execution& exec) {
    // Record the trade
    trades_.push_back(exec);
    total_volume_ += static_cast<uint64_t>(exec.quantity);

    // Update bid/ask maps based on taker side
    if (exec.is_buy) {
        // Taker was buyer → add to bids (someone sold at this price)
        bids_[exec.price] += exec.quantity;
    } else {
        // Taker was seller → add to asks (someone bought at this price)
        asks_[exec.price] += exec.quantity;
    }
}

uint32_t OrderBook::best_bid() const {
    if (bids_.empty()) {
        return 0;
    }
    return bids_.begin()->first;
}

uint32_t OrderBook::best_ask() const {
    if (asks_.empty()) {
        return 0;
    }
    return asks_.begin()->first;
}

uint32_t OrderBook::mid_price() const {
    uint32_t bid = best_bid();
    uint32_t ask = best_ask();
    if (bid == 0 || ask == 0) {
        return 0;
    }
    return (bid + ask) / 2;
}

uint32_t OrderBook::spread() const {
    uint32_t bid = best_bid();
    uint32_t ask = best_ask();
    if (bid == 0 || ask == 0) {
        return 0;
    }
    return ask - bid;
}