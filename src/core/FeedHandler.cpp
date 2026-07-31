#include "core/FeedHandler.hpp"
#include "logger/Logger.hpp"
#include <iostream>
#include <csignal>
#include <iomanip>

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

    // Open event CSV output if requested
    if (!config_.output_file.empty()) {
        output_stream_.open(config_.output_file);
        if (output_stream_.is_open()) {
            output_stream_ << "block_number,match_id,price,quantity,is_buy\n";
            Logger::getInstance().log("Writing events to " + config_.output_file);
        } else {
            std::cerr << "Warning: could not open " << config_.output_file << " for writing\n";
        }
    }

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

FeedHandler::~FeedHandler() {
    if (output_stream_.is_open()) {
        output_stream_.close();
    }
}

void FeedHandler::run() {
    // Set up signal handling for graceful shutdown
    std::signal(SIGINT, signal_handler);
    std::signal(SIGTERM, signal_handler);

    std::cout << "\n🔍 CLOB Reconstructor — Processing blocks...\n";
    std::cout << "   Press Ctrl+C for graceful shutdown with summary\n\n";

    while (parser_->has_next() && !g_shutdown) {
        auto events = parser_->next_events();
        uint64_t block_num = parser_->current_block();

        // Write events to CSV before dispatching (so we get raw event data)
        if (output_stream_.is_open() && !events.empty()) {
            for (const auto& e : events) {
                output_stream_ << block_num << ","
                               << e.match_id << ","
                               << e.price << ","
                               << e.quantity << ","
                               << (e.is_buy ? "1" : "0") << "\n";
            }
        }

        // Route to order book manager
        if (!events.empty()) {
            obm_->process_events(events, block_num);
        }

        // Update dispatcher stats
        dispatcher_->stats().blocks_processed++;
        dispatcher_->stats().total_events += events.size();

        // Advance parser
        parser_->advance();

        // Progress reporting
        if (dispatcher_->stats().blocks_processed % config_.progress_interval == 0) {
            dispatcher_->print_stats();
        }
    }

    // Final output
    std::cout << "\n✅ Processing complete.\n";
    dispatcher_->print_stats();
    obm_->log_summary();

    if (output_stream_.is_open()) {
        output_stream_.flush();
        std::cout << "Events written to: " << config_.output_file << "\n";
    }

    Logger::getInstance().log("CLOB Reconstructor finished.");
    Logger::getInstance().flush();
}
