#include "book/OrderBookManager.hpp"
#include "logger/Logger.hpp"
#include <iomanip>
#include <sstream>

std::shared_ptr<OrderBook> OrderBookManager::get_or_create_book(const std::string& token_id) {
    auto it = books_.find(token_id);
    if (it != books_.end()) return it->second;
    
    auto book = std::make_shared<OrderBook>(token_id);
    books_[token_id] = book;
    return book;
}

void OrderBookManager::process_events(const std::vector<Execution>& events, uint64_t block_number) {
    for (const auto& exec : events) {
        // Use match_id as a surrogate token identifier
        // In practice, we'd derive token_id from the makerAssetId/takerAssetId
        // For now: hash the match_id to create a token bucket
        std::string token_id = "market_" + std::to_string(exec.match_id % 100); // simple bucketing
        
        auto book = get_or_create_book(token_id);
        book->process_trade(exec);
        total_events_++;
    }
    
    std::ostringstream oss;
    oss << "Block " << block_number << ": processed " << events.size() 
        << " events across " << books_.size() << " books";
    Logger::getInstance().log(oss.str());
}

void OrderBookManager::log_summary() const {
    Logger::getInstance().log("===== OrderBookManager Summary =====");
    Logger::getInstance().log("Total events processed: " + std::to_string(total_events_));
    Logger::getInstance().log("Active order books: " + std::to_string(books_.size()));
    
    for (const auto& [token_id, book] : books_) {
        if (book->trade_count() == 0) continue;
        std::ostringstream oss;
        oss << "  " << token_id 
            << " | trades: " << book->trade_count()
            << " | volume: " << book->total_volume()
            << " | mid: " << book->mid_price()
            << " | spread: " << book->spread()
            << " | best_bid: " << book->best_bid()
            << " | best_ask: " << book->best_ask();
        Logger::getInstance().log(oss.str());
    }
    Logger::getInstance().log("====================================");
}
