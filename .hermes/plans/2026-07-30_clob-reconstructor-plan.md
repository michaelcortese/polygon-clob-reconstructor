# On-Chain CLOB Reconstructor — Implementation Plan

> **For Hermes:** Use multi-agent-orchestration skill to implement this plan in waves — each subagent owns a branch, commits, pushes, and opens a PR.

**Goal:** Build a high-performance C++ engine that streams Polygon blocks, decodes CTF Exchange `OrderFilled` events, and reconstructs full limit order books per prediction market — achieving 100% ground-truth accuracy vs Polymarket's unreliable WebSocket feed.

**Architecture:** Same pattern as MarketDataFeedHandler — parser → dispatcher → order book manager → analytics. But instead of memory-mapped ITCH files, the parser connects to a Polygon RPC endpoint and streams raw blocks. Instead of ITCH binary structs, the dispatcher decodes ABI-encoded Ethereum events.

**Tech Stack:** C++20, CMake, libcurl (RPC), nlohmann/json (for JSON-RPC), Catch2 (testing), GitHub Actions (CI)

**Repo:** https://github.com/michaelcortese/polygon-clob-reconstructor

---

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                    main.cpp                              │
│  (orchestrates FeedHandler with Polygon source)          │
├─────────────────────────────────────────────────────────┤
│                  FeedHandler (core/)                     │
│  Loop: fetch block → decode events → dispatch → advance  │
├──────────────────────┬──────────────────────────────────┤
│   PolygonParser      │      Dispatcher                  │
│   (parser/)          │      (core/)                     │
│   - JSON-RPC calls   │      Switch on event type        │
│   - Block streaming  │      → OrderBookManager          │
│   - ABI event decode │                                  │
├──────────────────────┴──────────────────────────────────┤
│              OrderBookManager (book/)                    │
│   Per-market order book: bids/asks, trade history        │
├─────────────────────────────────────────────────────────┤
│              Analytics (analytics/)                      │
│   VWAP, spread, depth, price impact, convergence         │
└─────────────────────────────────────────────────────────┘
```

## CTF Exchange OrderFilled Event (ABI)

```
event OrderFilled(
    bytes32 orderHash,      // indexed
    address maker,          // indexed
    address taker,          // indexed
    uint256 makerAssetId,   // 0 = maker posted USDC (seller)
    uint256 takerAssetId,   // 0 = taker posted USDC (buyer)
    uint256 makerAmountFilled,
    uint256 takerAmountFilled,
    uint256 fee
);
```

**Trade direction logic:**
- `takerAssetId == 0` → taker posted USDC → taker is BUYER (bid-side aggressor)
- `makerAssetId == 0` → maker posted USDC → taker is SELLER (ask-side aggressor)

---

## Wave 0: Foundation (PR #1)

### Task 0.1: Project skeleton + CMake + CI
**Files:** `CMakeLists.txt`, `.github/workflows/ci.yml`, `README.md`, `.gitignore`
**Branch:** `feat/project-skeleton`

### Task 0.2: Core data structures (Order, OrderBook, Execution)
**Files:** `include/book/Order.hpp`, `include/book/OrderBook.hpp`, `include/book/Execution.hpp`
**Branch:** `feat/core-data-structures`

### Task 0.3: Logger (thread-safe singleton)
**Files:** `include/logger/Logger.hpp`, `src/logger/Logger.cpp`
**Branch:** `feat/logger`

---

## Wave 1: Parser Layer (PR #2)

### Task 1.1: Polygon RPC client (JSON-RPC over HTTP)
**Files:** `include/parser/RpcClient.hpp`, `src/parser/RpcClient.cpp`
- `get_block_number()` → latest block
- `get_logs(fromBlock, toBlock, contractAddress, eventTopic)` → raw logs

### Task 1.2: ABI event decoder
**Files:** `include/parser/EventDecoder.hpp`, `src/parser/EventDecoder.cpp`
- Decode `OrderFilled` events from raw hex logs
- Parse topic hash, indexed params, data field

### Task 1.3: PolygonParser (streaming block iterator)
**Files:** `include/parser/PolygonParser.hpp`, `src/parser/PolygonParser.cpp`
- `has_next()` → more blocks available
- `next_events()` → vector of OrderFilled events for current block
- `advance()` → move to next block

---

## Wave 2: Order Book Layer (PR #3)

### Task 2.1: OrderBook core logic
**Files:** `src/book/OrderBook.cpp`
- Add/remove orders from price-level maps
- Track bid/ask depth, best bid/ask
- Support for multiple markets per book

### Task 2.2: OrderBookManager (multi-market registry)
**Files:** `include/book/OrderBookManager.hpp`, `src/book/OrderBookManager.cpp`
- Map tokenId → OrderBook
- Route OrderFilled events to correct book
- Trade history tracking per market

---

## Wave 3: Core Loop + Dispatcher (PR #4)

### Task 3.1: Dispatcher
**Files:** `include/core/Dispatcher.hpp`, `src/core/Dispatcher.cpp`
- Switch on event → delegate to OrderBookManager
- Stats tracking (event counts per market)

### Task 3.2: FeedHandler main loop
**Files:** `include/core/FeedHandler.hpp`, `src/core/FeedHandler.cpp`
- `while (parser.has_next()) { dispatch(parser.next_events()); parser.advance(); }`
- Throughput reporting, progress logging

### Task 3.3: main.cpp entry point
**Files:** `main.cpp`
- CLI args: RPC URL, contract address, start/end block range
- Wire everything together, run, print summary

---

## Wave 4: Analytics + Output (PR #5)

### Task 4.1: VWAP computation
**Files:** `include/analytics/VWAP.hpp`, `src/analytics/VWAP.cpp`
- Per-market volume-weighted average price
- Sliding window and session-wide

### Task 4.2: Market stats (spread, depth, volume)
**Files:** `include/analytics/MarketStats.hpp`, `src/analytics/MarketStats.cpp`
- Effective spread, quoted spread
- Depth at N levels from mid
- Total volume traded

### Task 4.3: Output formatter (CSV + JSON)
**Files:** `include/analytics/OutputWriter.hpp`, `src/analytics/OutputWriter.cpp`
- Dump order book state snapshots
- Export analytics to CSV for analysis

---

## Wave 5: Integration Testing + Docs (PR #6)

### Task 5.1: End-to-end test with sample block data
**Files:** `tests/test_integration.cpp`, `tests/fixtures/`
- Replay known OrderFilled events from a real Polygon block
- Verify reconstructed book matches expected state

### Task 5.2: Performance benchmarks
**Files:** `tests/test_benchmark.cpp`
- Events/second throughput
- Memory usage profiling

### Task 5.3: Documentation
**Files:** `README.md` (final), `docs/ARCHITECTURE.md`
- How it works, how to run, expected output

---

## Verification Criteria (End-to-End)

1. Compiles clean with `cmake --build .` — zero warnings
2. All unit tests pass: `ctest --output-on-failure`
3. Benchmark: >50K events/sec on a single core (RPC-bound)
4. Integration test: replay known block, verify 100% match with on-chain ground truth
5. Output: per-market VWAP, spread, depth, and order book snapshot to CSV

---

## Risks

| Risk | Mitigation |
|------|-----------|
| Polygon RPC rate limits | Cache blocks locally, respect rate limits, use archive node for historical |
| Large block sizes (many events) | Streaming decode, don't load entire block into memory |
| ABI decoding complexity | Use known event signature, fixed-size topic/data layout |
| Missing trade context (no resting order info on-chain) | Build "tape" (trade history) rather than full L3 book — documented limitation |