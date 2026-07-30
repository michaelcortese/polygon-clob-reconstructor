#pragma once
#include "Dispatcher.hpp"
#include <string>

struct FeedConfig {
    std::string rpc_url;
    uint64_t start_block = 0;
    uint64_t end_block = 0;
    uint64_t lookback_blocks = 100;
    uint64_t progress_interval = 100;  // print stats every N blocks
    std::string output_file;           // optional: path for CSV output
};

class FeedHandler {
public:
    explicit FeedHandler(const FeedConfig& config);
    
    // Main loop: process blocks until end_block or indefinitely
    void run();
    
private:
    FeedConfig config_;
    std::unique_ptr<PolygonParser> parser_;
    std::unique_ptr<OrderBookManager> obm_;
    std::unique_ptr<Dispatcher> dispatcher_;
};