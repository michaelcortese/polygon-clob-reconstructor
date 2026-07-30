#include <catch2/catch_test_macros.hpp>
#include "book/OrderBookManager.hpp"

TEST_CASE("OrderBookManager creates books on demand", "[orderbook]") {
    OrderBookManager mgr;
    
    auto book1 = mgr.get_or_create_book("token_abc");
    auto book2 = mgr.get_or_create_book("token_abc"); // same token
    auto book3 = mgr.get_or_create_book("token_xyz"); // different token
    
    REQUIRE(book1 == book2);          // same pointer
    REQUIRE(book1 != book3);          // different pointer
    REQUIRE(mgr.book_count() == 2);   // two unique tokens
}

TEST_CASE("OrderBookManager processes events into correct book", "[orderbook]") {
    OrderBookManager mgr;
    
    Execution e1{};
    e1.match_id = 1;
    e1.quantity = 100;
    e1.price = 55;
    e1.is_buy = true;
    e1.block_number = 42;
    
    Execution e2{};
    e2.match_id = 2;
    e2.quantity = 50;
    e2.price = 60;
    e2.is_buy = false;
    e2.block_number = 42;
    
    std::vector<Execution> events = {e1, e2};
    mgr.process_events(events, 42);
    
    REQUIRE(mgr.total_events_processed() == 2);
    REQUIRE(mgr.book_count() > 1);  // Different buckets due to match_id % 100
}

TEST_CASE("OrderBookManager empty events is safe", "[orderbook]") {
    OrderBookManager mgr;
    std::vector<Execution> events;
    mgr.process_events(events, 100);
    
    REQUIRE(mgr.total_events_processed() == 0);
    REQUIRE(mgr.book_count() == 0);
}
