#include <catch2/catch_test_macros.hpp>
#include "book/OrderBook.hpp"
#include "book/Execution.hpp"

TEST_CASE("OrderBook processes buy trade correctly", "[orderbook]") {
    OrderBook book("test_token");
    
    Execution exec{};
    exec.match_id = 100;
    exec.quantity = 50;
    exec.price = 65;   // $0.65
    exec.is_buy = true;
    exec.block_number = 1000;
    
    book.process_trade(exec);
    
    REQUIRE(book.trade_count() == 1);
    REQUIRE(book.total_volume() == 50);
    REQUIRE(book.best_bid() == 65);
    REQUIRE(book.best_ask() == 0);   // no sells yet
}

TEST_CASE("OrderBook tracks multiple trades and computes mid", "[orderbook]") {
    OrderBook book("test_token");
    
    Execution buy{};
    buy.match_id = 1; buy.quantity = 100; buy.price = 55;
    buy.is_buy = true; buy.block_number = 1;
    book.process_trade(buy);
    
    Execution sell{};
    sell.match_id = 2; sell.quantity = 50; sell.price = 60;
    sell.is_buy = false; sell.block_number = 1;
    book.process_trade(sell);
    
    REQUIRE(book.trade_count() == 2);
    REQUIRE(book.total_volume() == 150);
    REQUIRE(book.best_bid() == 55);
    REQUIRE(book.best_ask() == 60);
    REQUIRE(book.mid_price() == 57);  // (55+60)/2 = 57
    REQUIRE(book.spread() == 5);      // 60-55
}

TEST_CASE("OrderBook mid_price returns 0 with no data", "[orderbook]") {
    OrderBook book("empty");
    REQUIRE(book.mid_price() == 0);
    REQUIRE(book.spread() == 0);
    REQUIRE(book.best_bid() == 0);
    REQUIRE(book.best_ask() == 0);
}
