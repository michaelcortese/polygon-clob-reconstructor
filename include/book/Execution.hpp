#pragma once
#include <cstdint>
#include <string>
#include <sstream>

struct Execution {
    uint64_t match_id;       // derived from orderHash
    uint32_t quantity;       // tokens traded
    uint32_t price;          // price in USDC cents (e.g. 55 = $0.55)
    bool is_buy;             // true if taker was buyer (takerAssetId == 0)
    uint64_t block_number;
    uint64_t timestamp;      // block timestamp

    std::string toString() const {
        std::ostringstream oss;
        oss << "Execution[match=" << match_id
            << " qty=" << quantity
            << " price=" << price
            << " side=" << (is_buy ? "BUY" : "SELL")
            << " block=" << block_number << "]";
        return oss.str();
    }
};