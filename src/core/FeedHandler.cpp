#include "core/FeedHandler.hpp"
#include "logger/Logger.hpp"
#include <iostream>
#include <csignal>

namespace {
    // Signal handler flag for graceful shutdown
    volatile std::sig_atomic_t g_shutdown = 0;
    void signal_handler(int) { g_shutdown = 1; }
}

FeedHandler::FeedHandler(const FeedConfig& config)
    : config_(config) {
    
    // Initialize logger
    Logger::getInstance().init("clob_reconstructor.log");
    Logger::getInstance().log("CLOB Reconstructor starting...");
    Logger::getInstance().log("RPC URL: " + config_.rpc_url);
    
    // Create parser config from feed config
    ParserConfig pconfig;
    pconfig.rpc_url          = config_.rpc_url;
    pconfig.exchange_version = config_.exchange_version;
    pconfig.start_block      = config_.start_block;
    pconfig.end_block        = config_.end_block;
    pconfig.lookback_blocks  = config_.lookback_blocks;
    pconfig.batch_size       = config_.batch_size;
    
    parser_ = std::make_unique<PolygonParser>(pconfig);
    obm_ = std::make_unique<OrderBookManager>();
    dispatcher_ = std::make_unique<Dispatcher>(*parser_, *obm_);
    
    Logger::getInstance().log("Starting from block " + 
        std::to_string(parser_->current_block()));
}

void FeedHandler::run() {
    // Set up signal handling for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);
    
    std::cout << "\n🔍 CLOB Reconstructor — Processing blocks...\n";
    std::cout << "   Press Ctrl+C for graceful shutdown with summary\n\n";
    
    while (parser_->has_next() && !g_shutdown) {
        dispatcher_->dispatch_next_block();
        
        // Progress reporting
        if (dispatcher_->stats().blocks_processed % config_.progress_interval == 0) {
            dispatcher_->print_stats();
        }
    }
    
    // Final output
    std::cout << "\n✅ Processing complete.\n";
    dispatcher_->print_stats();
    obm_->log_summary();
    Logger::getInstance().log("CLOB Reconstructor finished.");
    Logger::getInstance().flush();
}
