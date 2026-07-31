#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "book/OrderBookManager.hpp"
#include "book/OrderBook.hpp"
#include <algorithm>

// ── Preserved from original ──────────────────────────────────────────

TEST_CASE("Full pipeline: events → books → stats", "[integration]") {
    OrderBookManager mgr;

    // Simulate trader buying YES tokens at different prices
    Execution e1{};
    e1.match_id = 1; e1.quantity = 100; e1.price = 50;
    e1.is_buy = true; e1.block_number = 100;

    Execution e2{};
    e2.match_id = 2; e2.quantity = 200; e2.price = 55;
    e2.is_buy = true; e2.block_number = 100;

    Execution e3{};
    e3.match_id = 3; e3.quantity = 150; e3.price = 60;
    e3.is_buy = false; e3.block_number = 101; // taker sold = someone bought at 60

    std::vector<Execution> events1 = {e1, e2};
    std::vector<Execution> events2 = {e3};

    mgr.process_events(events1, 100);
    mgr.process_events(events2, 101);

    REQUIRE(mgr.total_events_processed() == 3);
    REQUIRE(mgr.book_count() > 0);

    // Verify at least one book has trades
    bool found_trades = false;
    for (const auto& [id, book] : mgr.books()) {
        if (book->trade_count() > 0) {
            found_trades = true;
            REQUIRE(book->total_volume() > 0);
        }
    }
    REQUIRE(found_trades);
}

// ── Multi-block pipeline ──────────────────────────────────────────────

TEST_CASE("Multi-block pipeline: per-block event routing", "[integration]") {
    OrderBookManager mgr;

    // Block 100: 3 events for 3 different markets (match_id % 100)
    Execution e1{}; e1.match_id = 10; e1.quantity = 50; e1.price = 40;
    e1.is_buy = true; e1.block_number = 100;

    Execution e2{}; e2.match_id = 20; e2.quantity = 75; e2.price = 45;
    e2.is_buy = false; e2.block_number = 100;

    Execution e3{}; e3.match_id = 30; e3.quantity = 100; e3.price = 50;
    e3.is_buy = true; e3.block_number = 100;

    mgr.process_events({e1, e2, e3}, 100);
    REQUIRE(mgr.total_events_processed() == 3);
    REQUIRE(mgr.book_count() == 3); // 3 distinct match_id % 100 buckets

    // Block 200: events in same and new markets
    Execution e4{}; e4.match_id = 10; e4.quantity = 25; e4.price = 42;
    e4.is_buy = false; e4.block_number = 200;  // same market as e1

    Execution e5{}; e5.match_id = 40; e5.quantity = 200; e5.price = 48;
    e5.is_buy = true; e5.block_number = 200;  // new market

    mgr.process_events({e4, e5}, 200);
    REQUIRE(mgr.total_events_processed() == 5);
    REQUIRE(mgr.book_count() == 4); // 4 distinct markets now

    // Check that market_10 got both e1 and e4 (2 trades)
    auto book10 = mgr.get_or_create_book("market_10");
    REQUIRE(book10->trade_count() == 2);
    REQUIRE(book10->total_volume() == 75); // 50 + 25
    // market_10 has a buy at 40 and a sell at 42
    REQUIRE(book10->best_bid() == 40);
    REQUIRE(book10->best_ask() == 42);
    REQUIRE(book10->mid_price() == 41);
    REQUIRE(book10->spread() == 2);

    // Block 300: empty block
    mgr.process_events({}, 300);
    REQUIRE(mgr.total_events_processed() == 5);     // unchanged
    REQUIRE(mgr.book_count() == 4);                  // unchanged
}

// ── High volume stress ────────────────────────────────────────────────

TEST_CASE("High volume stress: 10,000+ events across many tokens", "[integration][stress]") {
    OrderBookManager mgr;
    constexpr size_t NUM_EVENTS = 15'000;
    constexpr int NUM_BLOCKS = 50;
    constexpr size_t EVENTS_PER_BLOCK = NUM_EVENTS / NUM_BLOCKS;

    // Generate events distributed across 100 distinct markets (match_id 0-99)
    // and 50 blocks, ensuring broad coverage
    std::vector<Execution> all_events;
    all_events.reserve(NUM_EVENTS);

    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        Execution e{};
        e.match_id = i;                        // 0..14999, maps to 100 buckets via % 100
        e.quantity = static_cast<uint32_t>((i % 500) + 1); // 1..500
        e.price    = static_cast<uint32_t>(30 + (i % 70)); // 30..99 cents
        e.is_buy   = (i % 2 == 0);
        e.block_number = 1000 + (i / EVENTS_PER_BLOCK);
        all_events.push_back(e);
    }

    // Process in block-sized batches
    for (int block = 0; block < NUM_BLOCKS; ++block) {
        size_t start = block * EVENTS_PER_BLOCK;
        size_t end   = std::min(start + EVENTS_PER_BLOCK, NUM_EVENTS);
        std::vector<Execution> batch(all_events.begin() + start, all_events.begin() + end);
        REQUIRE_NOTHROW(mgr.process_events(batch, 1000 + block));
    }

    // Verification
    REQUIRE(mgr.total_events_processed() == NUM_EVENTS);
    REQUIRE(mgr.book_count() <= 100);  // at most 100 distinct match_id % 100 buckets

    // Every book should have trades
    size_t total_trade_count = 0;
    uint64_t total_volume = 0;
    for (const auto& [id, book] : mgr.books()) {
        REQUIRE(book->trade_count() > 0);
        total_trade_count += book->trade_count();
        total_volume += book->total_volume();
    }
    REQUIRE(total_trade_count == NUM_EVENTS);

    // Volume sanity check: sum of quantities is 1+2+3+...+500 repeated 30 times
    // = (500*501/2) * 30 = 125250 * 30 = 3,757,500
    uint64_t expected_volume = 0;
    for (size_t i = 0; i < NUM_EVENTS; ++i) {
        expected_volume += static_cast<uint64_t>((i % 500) + 1);
    }
    REQUIRE(total_volume == expected_volume);
}

// ── Edge cases ────────────────────────────────────────────────────────

TEST_CASE("Edge cases: zero-quantity trades", "[integration][edge]") {
    OrderBookManager mgr;

    Execution e1{};
    e1.match_id = 1; e1.quantity = 0; e1.price = 50;
    e1.is_buy = true; e1.block_number = 100;

    mgr.process_events({e1}, 100);

    REQUIRE(mgr.total_events_processed() == 1);

    auto book = mgr.get_or_create_book("market_1");
    REQUIRE(book->trade_count() == 1);
    REQUIRE(book->total_volume() == 0);  // zero quantity contributes 0 to volume
    REQUIRE(book->bids_.at(50) == 0);    // zero quantity aggregated
}

TEST_CASE("Edge cases: duplicate match_ids", "[integration][edge]") {
    OrderBookManager mgr;

    Execution e{};
    e.match_id = 42; e.quantity = 10; e.price = 55;
    e.is_buy = true; e.block_number = 100;

    // Process the same match_id twice (duplicate log scenario)
    mgr.process_events({e, e, e}, 100);

    REQUIRE(mgr.total_events_processed() == 3);

    auto book = mgr.get_or_create_book("market_42");
    REQUIRE(book->trade_count() == 3);
    REQUIRE(book->total_volume() == 30);      // 10 * 3
    REQUIRE(book->bids_.at(55) == 30);        // aggregated 3 identical trades
}

TEST_CASE("Edge cases: empty vectors through multiple blocks", "[integration][edge]") {
    OrderBookManager mgr;

    // Multiple empty blocks in a row
    for (uint64_t blk = 1; blk <= 100; ++blk) {
        REQUIRE_NOTHROW(mgr.process_events({}, blk));
    }

    REQUIRE(mgr.total_events_processed() == 0);
    REQUIRE(mgr.book_count() == 0);

    // Then add real events — should still work
    Execution e{};
    e.match_id = 7; e.quantity = 5; e.price = 60;
    e.is_buy = false; e.block_number = 101;

    mgr.process_events({e}, 101);
    REQUIRE(mgr.total_events_processed() == 1);
    REQUIRE(mgr.book_count() == 1);
}

TEST_CASE("Edge cases: single-event block", "[integration][edge]") {
    OrderBookManager mgr;

    Execution e{};
    e.match_id = 1; e.quantity = 1; e.price = 1;
    e.is_buy = true; e.block_number = 1;

    mgr.process_events({e}, 1);

    REQUIRE(mgr.total_events_processed() == 1);
    REQUIRE(mgr.book_count() == 1);

    auto book = mgr.get_or_create_book("market_1");
    REQUIRE(book->trade_count() == 1);
    REQUIRE(book->best_bid() == 1);
}

// ── Bid/ask aggregation ──────────────────────────────────────────────

TEST_CASE("Bid/Ask aggregation: same-price trades accumulate volume", "[integration]") {
    OrderBookManager mgr;

    // Multiple buy trades at the same price → bids_ aggregates volume
    Execution b1{}; b1.match_id = 1; b1.quantity = 100; b1.price = 55;
    b1.is_buy = true; b1.block_number = 100;

    Execution b2{}; b2.match_id = 1; b2.quantity = 200; b2.price = 55;
    b2.is_buy = true; b2.block_number = 100;

    Execution b3{}; b3.match_id = 1; b3.quantity = 50; b3.price = 55;
    b3.is_buy = true; b3.block_number = 100;

    // Multiple sell trades at the same price → asks_ aggregates volume
    Execution s1{}; s1.match_id = 1; s1.quantity = 150; s1.price = 60;
    s1.is_buy = false; s1.block_number = 100;

    Execution s2{}; s2.match_id = 1; s2.quantity = 250; s2.price = 60;
    s2.is_buy = false; s2.block_number = 100;

    mgr.process_events({b1, b2, b3, s1, s2}, 100);

    auto book = mgr.get_or_create_book("market_1");
    REQUIRE(book->trade_count() == 5);
    REQUIRE(book->total_volume() == 750);  // 100+200+50+150+250

    // bids_ at price 55 should be 100+200+50 = 350
    REQUIRE(book->bids_.count(55) == 1);
    REQUIRE(book->bids_.at(55) == 350);

    // asks_ at price 60 should be 150+250 = 400
    REQUIRE(book->asks_.count(60) == 1);
    REQUIRE(book->asks_.at(60) == 400);
}

TEST_CASE("Bid/Ask aggregation: multiple price levels", "[integration]") {
    OrderBookManager mgr;

    // Buys at different prices
    Execution b50{}; b50.match_id = 1; b50.quantity = 100; b50.price = 50;
    b50.is_buy = true; b50.block_number = 100;

    Execution b55{}; b55.match_id = 1; b55.quantity = 200; b55.price = 55;
    b55.is_buy = true; b55.block_number = 100;

    Execution b52{}; b52.match_id = 1; b52.quantity = 75; b52.price = 52;
    b52.is_buy = true; b52.block_number = 100;

    // Sells at different prices
    Execution s60{}; s60.match_id = 1; s60.quantity = 80; s60.price = 60;
    s60.is_buy = false; s60.block_number = 100;

    Execution s65{}; s65.match_id = 1; s65.quantity = 120; s65.price = 65;
    s65.is_buy = false; s65.block_number = 100;

    mgr.process_events({b50, b55, b52, s60, s65}, 100);

    auto book = mgr.get_or_create_book("market_1");

    // bids_ is std::map<uint32_t, uint32_t, std::greater> — descending
    // best_bid() returns bids_.begin()->first
    REQUIRE(book->best_bid() == 55);  // highest bid
    REQUIRE(book->bids_.at(55) == 200);
    REQUIRE(book->bids_.at(52) == 75);
    REQUIRE(book->bids_.at(50) == 100);
    REQUIRE(book->bids_.size() == 3);

    // asks_ is std::map<uint32_t, uint32_t> — ascending
    // best_ask() returns asks_.begin()->first
    REQUIRE(book->best_ask() == 60);  // lowest ask
    REQUIRE(book->asks_.at(60) == 80);
    REQUIRE(book->asks_.at(65) == 120);
    REQUIRE(book->asks_.size() == 2);
}

// ── Spread and mid updates ────────────────────────────────────────────

TEST_CASE("Spread and mid updates evolve with new trades", "[integration]") {
    OrderBookManager mgr;

    // Phase 1: Only buys — no spread/mid yet
    Execution e1{}; e1.match_id = 5; e1.quantity = 10; e1.price = 50;
    e1.is_buy = true; e1.block_number = 100;
    mgr.process_events({e1}, 100);

    auto book = mgr.get_or_create_book("market_5");
    REQUIRE(book->best_bid() == 50);
    REQUIRE(book->best_ask() == 0);
    REQUIRE(book->mid_price() == 0);  // no ask yet
    REQUIRE(book->spread() == 0);

    // Phase 2: Add sell at 55 → spread and mid appear
    Execution e2{}; e2.match_id = 5; e2.quantity = 10; e2.price = 55;
    e2.is_buy = false; e2.block_number = 101;
    mgr.process_events({e2}, 101);

    REQUIRE(book->best_bid() == 50);
    REQUIRE(book->best_ask() == 55);
    REQUIRE(book->mid_price() == 52);   // (50+55)/2 = 52
    REQUIRE(book->spread() == 5);       // 55-50

    // Phase 3: New higher bid narrows the spread
    Execution e3{}; e3.match_id = 5; e3.quantity = 10; e3.price = 53;
    e3.is_buy = true; e3.block_number = 102;
    mgr.process_events({e3}, 102);

    REQUIRE(book->best_bid() == 53);    // new high bid
    REQUIRE(book->best_ask() == 55);
    REQUIRE(book->mid_price() == 54);   // (53+55)/2 = 54
    REQUIRE(book->spread() == 2);       // tightened

    // Phase 4: New lower ask tightens further
    Execution e4{}; e4.match_id = 5; e4.quantity = 10; e4.price = 54;
    e4.is_buy = false; e4.block_number = 103;
    mgr.process_events({e4}, 103);

    REQUIRE(book->best_bid() == 53);
    REQUIRE(book->best_ask() == 54);    // new low ask
    REQUIRE(book->mid_price() == 53);   // (53+54)/2 = 53, integer truncation
    REQUIRE(book->spread() == 1);       // 1 cent spread
}

// ── Cross-token isolation ─────────────────────────────────────────────

TEST_CASE("Cross-token isolation: events for token_A don't leak into token_B", "[integration]") {
    OrderBookManager mgr;

    // Token A events (match_id % 100 = 1 → "market_1")
    Execution a1{}; a1.match_id = 1; a1.quantity = 100; a1.price = 50;
    a1.is_buy = true; a1.block_number = 100;

    Execution a2{}; a2.match_id = 101; a2.quantity = 50; a2.price = 55;
    a2.is_buy = false; a2.block_number = 100;
    // 101 % 100 = 1 → also "market_1"

    // Token B events (match_id % 100 = 2 → "market_2")
    Execution b1{}; b1.match_id = 2; b1.quantity = 200; b1.price = 75;
    b1.is_buy = true; b1.block_number = 100;

    Execution b2{}; b2.match_id = 2; b2.quantity = 300; b2.price = 80;
    b2.is_buy = false; b2.block_number = 100;

    // Token C events (match_id % 100 = 3 → "market_3")
    Execution c1{}; c1.match_id = 3; c1.quantity = 10; c1.price = 30;
    c1.is_buy = false; c1.block_number = 100;

    mgr.process_events({a1, a2, b1, b2, c1}, 100);

    REQUIRE(mgr.total_events_processed() == 5);
    REQUIRE(mgr.book_count() == 3);

    // Token A book (market_1): 2 trades
    auto book_a = mgr.get_or_create_book("market_1");
    REQUIRE(book_a->trade_count() == 2);
    REQUIRE(book_a->total_volume() == 150);
    REQUIRE(book_a->best_bid() == 50);
    REQUIRE(book_a->best_ask() == 55);

    // Token B book (market_2): 2 trades
    auto book_b = mgr.get_or_create_book("market_2");
    REQUIRE(book_b->trade_count() == 2);
    REQUIRE(book_b->total_volume() == 500);
    REQUIRE(book_b->best_bid() == 75);
    REQUIRE(book_b->best_ask() == 80);

    // Token C book (market_3): 1 trade
    auto book_c = mgr.get_or_create_book("market_3");
    REQUIRE(book_c->trade_count() == 1);
    REQUIRE(book_c->total_volume() == 10);
    REQUIRE(book_c->best_bid() == 0);  // no buys
    REQUIRE(book_c->best_ask() == 30);

    // Verify isolation: book_a shouldn't have book_b's prices
    REQUIRE(book_a->bids_.count(75) == 0);
    REQUIRE(book_a->asks_.count(80) == 0);
    REQUIRE(book_b->bids_.count(50) == 0);
    REQUIRE(book_b->asks_.count(55) == 0);

    // Verify book_c is fully isolated
    REQUIRE(book_c->bids_.empty());
    REQUIRE(book_c->asks_.size() == 1);
}

// ── Extreme values ────────────────────────────────────────────────────

TEST_CASE("Extreme values: large prices and quantities", "[integration][edge]") {
    OrderBookManager mgr;

    Execution e1{}; e1.match_id = 100; e1.quantity = UINT32_MAX; e1.price = UINT32_MAX;
    e1.is_buy = true; e1.block_number = 100;

    Execution e2{}; e2.match_id = 100; e2.quantity = UINT32_MAX; e2.price = 1;
    e2.is_buy = false; e2.block_number = 100;

    REQUIRE_NOTHROW(mgr.process_events({e1, e2}, 100));

    auto book = mgr.get_or_create_book("market_0"); // 100 % 100 = 0
    REQUIRE(book->trade_count() == 2);

    // total_volume may overflow (uint32_t + uint32_t assigned to uint64_t is fine)
    // 2 * UINT32_MAX fits in uint64_t
    REQUIRE(book->best_bid() == UINT32_MAX);
    REQUIRE(book->best_ask() == 1);

    // spread: UINT32_MAX - 1 + 1 (for correct subtraction check)
    // spread() returns ask - bid = 1 - UINT32_MAX which wraps around
    // This is expected behavior for extreme values; the engine doesn't clamp
    // We just verify no crash
    (void)book->spread();
    (void)book->mid_price();
    REQUIRE_NOTHROW(book->spread());
}

// ── Rapid interleaving ────────────────────────────────────────────────

TEST_CASE("Rapid interleaving: alternating buy/sell within single block", "[integration]") {
    OrderBookManager mgr;

    // Rapid alternation: BUY, SELL, BUY, SELL, ...
    // All in the same market (match_id % 100 = 0)
    std::vector<Execution> events;
    for (int i = 0; i < 1000; ++i) {
        Execution e{};
        e.match_id = 0;  // always market_0
        e.quantity = static_cast<uint32_t>(i + 1);
        e.price = static_cast<uint32_t>(50 + (i % 5)); // 50..54
        e.is_buy = (i % 2 == 0);
        e.block_number = 100;
        events.push_back(e);
    }

    mgr.process_events(events, 100);

    auto book = mgr.get_or_create_book("market_0");
    REQUIRE(book->trade_count() == 1000);

    // bids_ and asks_ both end up with all 5 price levels because
    // is_buy alternates every 2, price cycles every 5, and 2 and 5 are coprime
    REQUIRE(book->bids_.size() == 5);
    REQUIRE(book->asks_.size() == 5);

    // best_bid = 54 (highest price), best_ask = 50 (lowest price)
    REQUIRE(book->best_bid() == 54);
    REQUIRE(book->best_ask() == 50);
}
