#include "core/Dispatcher.hpp"
#include <iostream>
#include <iomanip>

Dispatcher::Dispatcher(PolygonParser& parser, OrderBookManager& obm)
    : parser_(parser), obm_(obm) {
    stats_.start_time = std::chrono::steady_clock::now();
}

size_t Dispatcher::dispatch_next_block() {
    if (!parser_.has_next()) return 0;
    
    // Get events for current block
    auto events = parser_.next_events();
    uint64_t block_num = parser_.current_block();
    
    // Route to order book manager
    if (!events.empty()) {
        obm_.process_events(events, block_num);
    }
    
    // Update stats
    stats_.blocks_processed++;
    stats_.total_events += events.size();
    
    // Advance parser to next block
    parser_.advance();
    
    return events.size();
}

void Dispatcher::print_stats() const {
    std::cout << "\n═══════════════════════════════════════\n";
    std::cout << "  CLOB Reconstructor — Live Stats\n";
    std::cout << "═══════════════════════════════════════\n";
    std::cout << "  Blocks processed:  " << stats_.blocks_processed << "\n";
    std::cout << "  Total events:      " << stats_.total_events << "\n";
    std::cout << "  Elapsed:           " << std::fixed << std::setprecision(2) 
              << stats_.elapsed_seconds() << "s\n";
    
    if (stats_.elapsed_seconds() > 0) {
        double rate = stats_.total_events / stats_.elapsed_seconds();
        std::cout << "  Events/sec:        " << std::fixed << std::setprecision(0) << rate << "\n";
        double blocks_per_sec = stats_.blocks_processed / stats_.elapsed_seconds();
        std::cout << "  Blocks/sec:        " << std::fixed << std::setprecision(1) << blocks_per_sec << "\n";
    }
    
    std::cout << "  Active books:      " << obm_.book_count() << "\n";
    std::cout << "  Current block:     " << parser_.current_block() << "\n";
    std::cout << "═══════════════════════════════════════\n\n";
}