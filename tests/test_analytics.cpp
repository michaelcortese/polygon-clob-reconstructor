#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include "analytics/MarketStats.hpp"
#include "analytics/CsvWriter.hpp"
#include "book/OrderBook.hpp"
#include "book/Execution.hpp"
#include <fstream>
#include <cstdio>

// ── MarketStats::snapshot() ───────────────────────────────────────────

TEST_CASE("MarketStats snapshot: empty book returns zero snapshot", "[analytics][marketstats]") {
    OrderBook book("EMPTY");
    MarketSnapshot snap = MarketStats::snapshot(book, 42);

    REQUIRE(snap.block_number == 42);
    REQUIRE(snap.token_id == "EMPTY");
    REQUIRE(snap.best_bid == 0);
    REQUIRE(snap.best_ask == 0);
    REQUIRE(snap.mid_price == 0);
    REQUIRE(snap.spread == 0);
    REQUIRE(snap.bid_depth_1pct == 0);
    REQUIRE(snap.ask_depth_1pct == 0);
    REQUIRE(snap.total_volume == 0);
    REQUIRE(snap.trade_count == 0);
    REQUIRE(snap.vwap == 0.0);
}

TEST_CASE("MarketStats snapshot: basic populated book", "[analytics][marketstats]") {
    OrderBook book("TOKEN_A");

    // Insert bids at various prices
    book.bids_[100] = 500;   // price 100, size 500
    book.bids_[99]  = 300;   // price 99, size 300
    book.bids_[95]  = 200;   // price 95, size 200

    // Insert asks at various prices
    book.asks_[102] = 400;   // price 102, size 400
    book.asks_[103] = 250;   // price 103, size 250
    book.asks_[110] = 100;   // price 110, size 100

    // No trades yet
    MarketSnapshot snap = MarketStats::snapshot(book, 1);

    REQUIRE(snap.block_number == 1);
    REQUIRE(snap.token_id == "TOKEN_A");
    REQUIRE(snap.best_bid == 100);
    REQUIRE(snap.best_ask == 102);
    REQUIRE(snap.mid_price == 101);          // (100+102)/2 = 101
    REQUIRE(snap.spread == 2);               // 102-100 = 2
    REQUIRE(snap.total_volume == 0);
    REQUIRE(snap.trade_count == 0);
    REQUIRE(snap.vwap == 0.0);

    // Depth: best_bid=100, 1% threshold = 100 - 1 = 99
    // Bids >= 99: {100: 500, 99: 300} → total 800
    REQUIRE(snap.bid_depth_1pct == 800);

    // Depth: best_ask=102, 1% threshold = 102 + 1 = 103
    // Asks <= 103: {102: 400, 103: 250} → total 650
    REQUIRE(snap.ask_depth_1pct == 650);
}

TEST_CASE("MarketStats snapshot: VWAP computation", "[analytics][marketstats]") {
    OrderBook book("VWAP_TEST");

    // Trade 1: 100 shares @ 50  → 5000
    Execution t1{};
    t1.match_id = 1; t1.quantity = 100; t1.price = 50;
    t1.is_buy = true; t1.block_number = 1;
    book.process_trade(t1);

    // Trade 2: 200 shares @ 55  → 11000
    Execution t2{};
    t2.match_id = 2; t2.quantity = 200; t2.price = 55;
    t2.is_buy = false; t2.block_number = 1;
    book.process_trade(t2);

    // Trade 3: 150 shares @ 52  → 7800
    Execution t3{};
    t3.match_id = 3; t3.quantity = 150; t3.price = 52;
    t3.is_buy = true; t3.block_number = 1;
    book.process_trade(t3);

    // Expected VWAP: (5000+11000+7800) / (100+200+150) = 23800/450 ≈ 52.8889
    MarketSnapshot snap = MarketStats::snapshot(book, 1);

    REQUIRE(snap.trade_count == 3);
    REQUIRE(snap.total_volume == 450);
    REQUIRE_THAT(snap.vwap, Catch::Matchers::WithinRel(52.8889, 1e-4));
}

TEST_CASE("MarketStats snapshot: single trade VWAP equals price", "[analytics][marketstats]") {
    OrderBook book("SINGLE");

    Execution t{};
    t.match_id = 1; t.quantity = 10; t.price = 75;
    t.is_buy = true; t.block_number = 1;
    book.process_trade(t);

    MarketSnapshot snap = MarketStats::snapshot(book, 1);
    REQUIRE(snap.trade_count == 1);
    REQUIRE(snap.total_volume == 10);
    REQUIRE(snap.vwap == 75.0);
}

TEST_CASE("MarketStats snapshot: depth within 1% at boundary", "[analytics][marketstats]") {
    OrderBook book("BOUNDARY");

    book.bids_[100] = 500;    // best bid
    book.bids_[99]  = 300;    // exactly at 1% below → included
    book.bids_[98]  = 200;    // below 1% threshold → excluded

    book.asks_[100] = 500;    // best ask
    book.asks_[101] = 300;    // exactly at 1% above → included
    book.asks_[102] = 200;    // above 1% threshold → excluded

    MarketSnapshot snap = MarketStats::snapshot(book, 1);

    // best_bid=100, threshold=99 → bids >= 99 included
    REQUIRE(snap.bid_depth_1pct == 800);  // 500 + 300
    // best_ask=100, threshold=101 → asks <= 101 included
    REQUIRE(snap.ask_depth_1pct == 800);  // 500 + 300
}

TEST_CASE("MarketStats snapshot: zero best bid results in zero depth", "[analytics][marketstats]") {
    OrderBook book("NO_BID");
    book.asks_[100] = 500;   // only asks, no bids

    MarketSnapshot snap = MarketStats::snapshot(book, 1);
    REQUIRE(snap.best_bid == 0);
    REQUIRE(snap.bid_depth_1pct == 0);
    REQUIRE(snap.ask_depth_1pct > 0);  // asks still computed
}

TEST_CASE("MarketStats snapshot: zero best ask results in zero depth", "[analytics][marketstats]") {
    OrderBook book("NO_ASK");
    book.bids_[100] = 500;   // only bids, no asks

    MarketSnapshot snap = MarketStats::snapshot(book, 1);
    REQUIRE(snap.best_ask == 0);
    REQUIRE(snap.ask_depth_1pct == 0);
    REQUIRE(snap.bid_depth_1pct > 0);  // bids still computed
}

TEST_CASE("MarketStats snapshot: zero quantity trades yield zero VWAP", "[analytics][marketstats]") {
    OrderBook book("ZERO_QTY");

    Execution t{};
    t.match_id = 1; t.quantity = 0; t.price = 100;
    t.is_buy = true; t.block_number = 1;
    book.process_trade(t);

    MarketSnapshot snap = MarketStats::snapshot(book, 1);
    REQUIRE(snap.vwap == 0.0);
}

TEST_CASE("MarketStats snapshot: large depth values", "[analytics][marketstats]") {
    OrderBook book("LARGE");

    // Best bid at high price, many levels within 1%
    book.bids_[100000] = 1000000;
    book.bids_[99900]  = 500000;
    book.bids_[98500]  = 250000;   // 98.5% of best → excluded (below 99% threshold)
    book.bids_[50000]  = 100000;   // far below 1% → excluded

    MarketSnapshot snap = MarketStats::snapshot(book, 1);

    // best_bid=100000, threshold = 100000 - 1000 = 99000
    // bids >= 99000: {100000: 1000000, 99900: 500000} → 1500000
    REQUIRE(snap.bid_depth_1pct == 1500000);
}

// ── CsvWriter ─────────────────────────────────────────────────────────

TEST_CASE("CsvWriter: write_header produces correct CSV header", "[analytics][csvwriter]") {
    const std::string path = "/tmp/test_csv_header.csv";

    {
        CsvWriter writer(path);
        REQUIRE(writer.is_open());
        writer.write_header();
    }  // destructor closes file

    std::ifstream fin(path);
    REQUIRE(fin.is_open());
    std::string line;
    std::getline(fin, line);

    REQUIRE(line == "block_number,token_id,best_bid,best_ask,mid_price,spread,"
                    "bid_depth_1pct,ask_depth_1pct,total_volume,trade_count,vwap");

    std::remove(path.c_str());
}

TEST_CASE("CsvWriter: write_snapshot produces correct CSV row", "[analytics][csvwriter]") {
    const std::string path = "/tmp/test_csv_snapshot.csv";

    MarketSnapshot snap{};
    snap.block_number = 12345;
    snap.token_id = "TOKEN_X";
    snap.best_bid = 100;
    snap.best_ask = 102;
    snap.mid_price = 101;
    snap.spread = 2;
    snap.bid_depth_1pct = 800;
    snap.ask_depth_1pct = 650;
    snap.total_volume = 10000;
    snap.trade_count = 42;
    snap.vwap = 51.75;

    {
        CsvWriter writer(path);
        REQUIRE(writer.is_open());
        writer.write_snapshot(snap);
    }

    std::ifstream fin(path);
    REQUIRE(fin.is_open());
    std::string line;
    std::getline(fin, line);

    REQUIRE(line == "12345,TOKEN_X,100,102,101,2,800,650,10000,42,51.75");

    std::remove(path.c_str());
}

TEST_CASE("CsvWriter: write_snapshots writes header then rows", "[analytics][csvwriter]") {
    const std::string path = "/tmp/test_csv_batch.csv";

    MarketSnapshot s1{};
    s1.block_number = 1;
    s1.token_id = "A";
    s1.best_bid = 50;
    s1.best_ask = 51;
    s1.mid_price = 50;
    s1.spread = 1;
    s1.bid_depth_1pct = 100;
    s1.ask_depth_1pct = 200;
    s1.total_volume = 1000;
    s1.trade_count = 5;
    s1.vwap = 50.50;

    MarketSnapshot s2{};
    s2.block_number = 2;
    s2.token_id = "B";
    s2.best_bid = 60;
    s2.best_ask = 62;
    s2.mid_price = 61;
    s2.spread = 2;
    s2.bid_depth_1pct = 300;
    s2.ask_depth_1pct = 400;
    s2.total_volume = 2000;
    s2.trade_count = 10;
    s2.vwap = 61.00;

    {
        CsvWriter writer(path);
        REQUIRE(writer.is_open());
        writer.write_snapshots({s1, s2});
    }

    std::ifstream fin(path);
    REQUIRE(fin.is_open());

    std::string header, row1, row2;
    std::getline(fin, header);
    std::getline(fin, row1);
    std::getline(fin, row2);

    REQUIRE(header == "block_number,token_id,best_bid,best_ask,mid_price,spread,"
                      "bid_depth_1pct,ask_depth_1pct,total_volume,trade_count,vwap");
    REQUIRE(row1 == "1,A,50,51,50,1,100,200,1000,5,50.50");
    REQUIRE(row2 == "2,B,60,62,61,2,300,400,2000,10,61.00");

    // Verify no extra rows
    std::string extra;
    bool has_more = static_cast<bool>(std::getline(fin, extra));
    REQUIRE(!has_more);

    std::remove(path.c_str());
}

TEST_CASE("CsvWriter: write_snapshots with empty vector", "[analytics][csvwriter]") {
    const std::string path = "/tmp/test_csv_empty.csv";

    {
        CsvWriter writer(path);
        REQUIRE(writer.is_open());
        writer.write_snapshots({});
    }

    std::ifstream fin(path);
    REQUIRE(fin.is_open());

    std::string header;
    std::getline(fin, header);
    REQUIRE(!header.empty());  // header still written

    std::string extra;
    bool has_more = static_cast<bool>(std::getline(fin, extra));
    REQUIRE(!has_more);  // no data rows

    std::remove(path.c_str());
}

TEST_CASE("CsvWriter: VWAP formatted with 2 decimal places", "[analytics][csvwriter]") {
    const std::string path = "/tmp/test_csv_vwap_fmt.csv";

    MarketSnapshot snap{};
    snap.block_number = 1;
    snap.token_id = "T";
    snap.vwap = 51.333;  // should format as "51.33"

    {
        CsvWriter writer(path);
        writer.write_header();
        writer.write_snapshot(snap);
    }

    std::ifstream fin(path);
    std::string header, row;
    std::getline(fin, header);
    std::getline(fin, row);

    // The VWAP field is the last column after 10 others
    // Find it by splitting on comma
    size_t last_comma = row.find_last_of(',');
    std::string vwap_str = row.substr(last_comma + 1);
    REQUIRE(vwap_str == "51.33");

    std::remove(path.c_str());
}