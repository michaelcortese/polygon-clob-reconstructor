#include "analytics/MarketStats.hpp"
#include "book/OrderBook.hpp"

MarketSnapshot MarketStats::snapshot(const OrderBook& book, uint64_t block_number) {
    MarketSnapshot snap{};
    snap.block_number = block_number;
    snap.token_id = book.token_id();
    snap.best_bid = book.best_bid();
    snap.best_ask = book.best_ask();
    snap.mid_price = book.mid_price();
    snap.spread = book.spread();
    snap.total_volume = book.total_volume();
    snap.trade_count = book.trade_count();
    
    // Compute VWAP from trade history
    uint64_t total_pv = 0;  // price * volume
    uint64_t total_qty = 0;
    for (const auto& exec : book.trades()) {
        total_pv += static_cast<uint64_t>(exec.price) * exec.quantity;
        total_qty += exec.quantity;
    }
    snap.vwap = (total_qty > 0) ? static_cast<double>(total_pv) / total_qty : 0.0;
    
    // Compute depth within 1% of best prices
    if (snap.best_bid > 0) {
        uint32_t bid_threshold = snap.best_bid - (snap.best_bid / 100); // 1% below best
        for (const auto& [price, size] : book.bids_) {
            if (price >= bid_threshold) {
                snap.bid_depth_1pct += size;
            }
        }
    }
    
    if (snap.best_ask > 0) {
        uint32_t ask_threshold = snap.best_ask + (snap.best_ask / 100); // 1% above best
        for (const auto& [price, size] : book.asks_) {
            if (price <= ask_threshold) {
                snap.ask_depth_1pct += size;
            }
        }
    }
    
    return snap;
}