#include "core/FeedHandler.hpp"
#include "parser/PolygonParser.hpp"
#include <iostream>
#include <cstdlib>
#include <getopt.h>
#include <cstring>

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " [OPTIONS]\n\n"
              << "On-Chain CLOB Reconstructor for Polymarket CTF Exchange\n\n"
              << "Options:\n"
              << "  -u, --rpc-url <url>     Polygon RPC endpoint (default: http://localhost:8545)\n"
              << "  -x, --exchange <v1|v2>  CTF Exchange version (default: v2)\n"
              << "  -s, --start-block <n>   Start block number (default: latest - lookback)\n"
              << "  -e, --end-block <n>     End block number (default: 0 = stream forever)\n"
              << "  -l, --lookback <n>      Blocks to look back from latest (default: 100)\n"
              << "  -b, --batch-size <n>    Blocks per RPC call (default: 100, 1 = no batching)\n"
              << "  -p, --progress <n>      Print stats every N blocks (default: 100)\n"
              << "  -o, --output <file>     Output file path (default: none)\n"
              << "  -h, --help              Show this help\n\n"
              << "Examples:\n"
              << "  " << prog << " --rpc-url https://polygon.drpc.org --start-block 50000000 --end-block 50010000\n"
              << "  " << prog << " -u https://polygon.drpc.org -x v2 -l 5000 -b 100 -p 500\n";
}

int main(int argc, char* argv[]) {
    FeedConfig config;
    ParserConfig parser_config;  // additional parser config passed via FeedConfig

    // Parse CLI arguments
    static struct option long_opts[] = {
        {"rpc-url",     required_argument, nullptr, 'u'},
        {"exchange",    required_argument, nullptr, 'x'},
        {"start-block", required_argument, nullptr, 's'},
        {"end-block",   required_argument, nullptr, 'e'},
        {"lookback",    required_argument, nullptr, 'l'},
        {"batch-size",  required_argument, nullptr, 'b'},
        {"progress",    required_argument, nullptr, 'p'},
        {"output",      required_argument, nullptr, 'o'},
        {"help",        no_argument,       nullptr, 'h'},
        {nullptr, 0, nullptr, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "u:x:s:e:l:b:p:o:h", long_opts, nullptr)) != -1) {
        switch (opt) {
            case 'u': config.rpc_url = optarg; break;
            case 'x':
                if (std::strcmp(optarg, "v1") == 0) {
                    config.exchange_version = ExchangeVersion::V1;
                } else {
                    config.exchange_version = ExchangeVersion::V2;
                }
                break;
            case 's': config.start_block = std::strtoull(optarg, nullptr, 10); break;
            case 'e': config.end_block = std::strtoull(optarg, nullptr, 10); break;
            case 'l': config.lookback_blocks = std::strtoull(optarg, nullptr, 10); break;
            case 'b': config.batch_size = std::strtoull(optarg, nullptr, 10); break;
            case 'p': config.progress_interval = std::strtoull(optarg, nullptr, 10); break;
            case 'o': config.output_file = optarg; break;
            case 'h': print_usage(argv[0]); return 0;
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    // Default RPC URL if not specified
    if (config.rpc_url.empty()) {
        config.rpc_url = "http://localhost:8545";
        std::cout << "No RPC URL specified, using default: " << config.rpc_url << "\n";
    }

    try {
        FeedHandler handler(config);
        handler.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
