#include <catch2/catch_test_macros.hpp>
#include <catch2/benchmark/catch_benchmark.hpp>
#include "book/OrderBookManager.hpp"
#include "book/OrderBook.hpp"

TEST_CASE("Benchmark: OrderBook trade processing", "[benchmark]") {
    OrderBook book("btc");
    
    BENCHMARK("process 1000 trades") {
        for (int i = 0; i < 1000; i++) {
            Execution exec{};
            exec.match_id = i;
            exec.quantity = 100;
            exec.price = 50 + (i % 40);  // prices between 50-89
            exec.is_buy = (i % 2 == 0);
            exec.block_number = 1;
            book.process_trade(exec);
        }
        return book.trade_count();
    };
}

TEST_CASE("Benchmark: OrderBookManager event routing", "[benchmark]") {
    OrderBookManager mgr;
    
    std::vector<Execution> events(1000);
    for (int i = 0; i < 1000; i++) {
        events[i].match_id = i;
        events[i].quantity = 100;
        events[i].price = 50 + (i % 40);
        events[i].is_buy = (i % 2 == 0);
        events[i].block_number = 1;
    }
    
    BENCHMARK("process 1000 events across books") {
        mgr.process_events(events, 1);
        return mgr.total_events_processed();
    };
}
