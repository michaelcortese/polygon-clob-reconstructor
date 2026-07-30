#pragma once
#include <cstdint>
#include <string>

struct PriceLevel {
    uint32_t price;     // price in USDC cents
    uint32_t size;      // total quantity at this price level

    bool operator<(const PriceLevel& other) const { return price < other.price; }
    bool operator>(const PriceLevel& other) const { return price > other.price; }
};