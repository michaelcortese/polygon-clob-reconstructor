# Polygon CLOB Reconstructor

[![CI](https://github.com/michaelcortese/polygon-clob-reconstructor/actions/workflows/ci.yml/badge.svg)](https://github.com/michaelcortese/polygon-clob-reconstructor/actions/workflows/ci.yml)
[![C++20](https://img.shields.io/badge/C%2B%2B-20-blue.svg)](https://en.cppreference.com/w/cpp/20)
[![CMake](https://img.shields.io/badge/CMake-%E2%89%A5%203.20-green.svg)](https://cmake.org/)
[![License](https://img.shields.io/badge/license-MIT-blue.svg)](LICENSE)

A high-performance C++ engine that streams Polygon blockchain blocks, decodes
[CTF Exchange](https://docs.polymarket.com) `OrderFilled` events, and reconstructs
limit order books in real time.

Polymarket's off-chain WebSocket feed captures about 59% of order book events.
The on-chain data is complete, deterministic, and verifiable — this engine fills
the gap.

---

## Quickstart

```bash
# Clone and build
git clone https://github.com/michaelcortese/polygon-clob-reconstructor.git
cd polygon-clob-reconstructor
cmake -B build -S .
cmake --build build

# Run (point at any Polygon RPC endpoint)
./build/polygon_clob_reconstructor --rpc-url https://polygon-rpc.com --start-block 50000000

# Run the tests
ctest --test-dir build --output-on-failure
```

That's it. Three commands from clone to running.

---

## Pipeline

```
Polygon RPC ──▶ RpcClient ──▶ PolygonParser ──▶ Dispatcher ──▶ OrderBookManager
                    │                │               │                │
                    │                │               │          ┌─────┴──────┐
                    │                │               │     OrderBook    OrderBook
                    │                │               │     (token_A)    (token_B)
                    │                │               │          │            │
                    │                │               │     MarketStats / VWAP / CSV
                    │                │               │
               libcurl HTTP    ABI decode      per-block        multi-market
               JSON-RPC 2.0    OrderFilled     coordination     event routing
```

Full architecture: [docs/ARCHITECTURE.md](./docs/ARCHITECTURE.md)

---

## Requirements

- **C++20** compiler (GCC 11+ or Clang 14+)
- **CMake** 3.20 or later
- **libcurl** (optional — builds without it for offline/stub mode)

Dependencies fetched automatically:
- [nlohmann/json](https://github.com/nlohmann/json) v3.11.3
- [Catch2](https://github.com/catchorg/Catch2) v3.6.0

### Install libcurl

```bash
# Debian/Ubuntu
sudo apt-get install libcurl4-openssl-dev

# macOS
brew install curl
```

---

## Build

```bash
cmake -B build -S .
cmake --build build
```

Build without libcurl (offline/stub mode):

```bash
cmake -B build -S . -DPOLYGON_CLOB_USE_CURL=OFF
cmake --build build
```

---

## Run

```bash
./build/polygon_clob_reconstructor --rpc-url https://polygon-rpc.com --start-block 50000000
```

The engine streams blocks starting from the given block number, decodes
`OrderFilled` events on the CTF Exchange contract, and reconstructs per-token
order books. Press `Ctrl+C` for a graceful shutdown with a summary of all books.

---

## Test

```bash
# All tests
ctest --test-dir build --output-on-failure

# Specific test tags
./build/polygon_clob_reconstructor_tests "[orderbook]"
./build/polygon_clob_reconstructor_tests "[integration]"

# Benchmarks (excluded from default ctest)
./build/polygon_clob_reconstructor_tests "[benchmark]"
```

---

## Project Structure

```
polygon-clob-reconstructor/
├── CMakeLists.txt              # Build configuration (FetchContent, C++20)
├── README.md                   # This file
├── docs/
│   └── ARCHITECTURE.md         # Full architecture documentation
├── include/
│   ├── book/
│   │   ├── Execution.hpp       # Trade execution struct
│   │   ├── Order.hpp           # PriceLevel struct
│   │   ├── OrderBook.hpp       # Per-token order book
│   │   └── OrderBookManager.hpp # Multi-market book registry
│   ├── core/
│   │   ├── Dispatcher.hpp      # Per-block event coordinator
│   │   └── FeedHandler.hpp     # Main loop with signal handling
│   ├── logger/
│   │   └── Logger.hpp          # Thread-safe logging
│   └── parser/
│       ├── PolygonParser.hpp   # Block streaming + ABI decoding
│       └── RpcClient.hpp       # JSON-RPC 2.0 over HTTP (libcurl)
├── src/
│   ├── main.cpp                # CLI entry point
│   ├── book/
│   │   ├── OrderBook.cpp
│   │   └── OrderBookManager.cpp
│   ├── core/
│   │   ├── Dispatcher.cpp
│   │   └── FeedHandler.cpp
│   ├── logger/
│   │   └── Logger.cpp
│   └── parser/
│       ├── PolygonParser.cpp
│       └── RpcClient.cpp
├── tests/
│   ├── test_order_book_manager.cpp
│   ├── test_order_book.cpp
│   ├── test_integration.cpp
│   └── test_benchmark.cpp
└── .github/
    └── workflows/
        └── ci.yml
```

---

## How It Works

The engine processes the Polygon PoS chain block by block:

1. **RpcClient** fetches raw block data through `eth_getBlockByNumber` and
   `eth_getLogs` (filtered to the CTF Exchange contract).

2. **PolygonParser** decodes `OrderFilled` events — trade direction
   (from `makerAssetId`/`takerAssetId`), price, and quantity.

3. **Dispatcher** coordinates per-block batches and routes them to the
   `OrderBookManager`.

4. **OrderBookManager** keeps a registry of per-token `OrderBook` instances,
   routing each trade to the right book.

5. Each **OrderBook** updates its bid/ask maps, computes best bid, best ask,
   mid price, spread, and accumulates trade history — ready for VWAP and
   analytics.

---

## License

MIT
