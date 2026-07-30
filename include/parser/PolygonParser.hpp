#pragma once
#include <string>
#include <vector>
#include <memory>
#include <nlohmann/json.hpp>
#include "parser/RpcClient.hpp"
#include "book/Execution.hpp"

/// Configuration for the Polygon block parser.
struct ParserConfig {
    std::string rpc_url;

    /// CTF Exchange contract on Polygon (V1).
    std::string ctf_exchange_address = "0x4bFb41d5B3570DeFd03C39a9A4D8dE6Bd8B8982E";

    /// Event signature keccak256("OrderFilled(bytes32,uint256,uint256,uint256,uint256,uint256,uint256,uint256)")
    std::string order_filled_topic =
        "0xd0a08e8c493f9c94f29311604c9de1b4e8c8d4c06bd0c789af57f2d65bfec0f6";

    /// Start block (0 = latest minus lookback_blocks).
    uint64_t start_block = 0;

    /// End block (0 = stream indefinitely).
    uint64_t end_block = 0;

    /// Number of blocks to look back when start_block == 0.
    uint64_t lookback_blocks = 100;
};

/// Streams Polygon blocks, fetches OrderFilled logs, and decodes them
/// into Execution structs via ABI decoding.
class PolygonParser {
public:
    explicit PolygonParser(const ParserConfig& config);
    ~PolygonParser() = default;

    /// Are there more blocks to process?
    bool has_next() const;

    /// Return all OrderFilled events for the *current* block.
    /// Cached — calling this multiple times for the same block returns
    /// the same result without re-fetching.
    std::vector<Execution> next_events();

    /// Advance to the next block.
    void advance();

    // ── Stats ────────────────────────────────────────────────────────
    uint64_t current_block() const { return current_block_; }
    uint64_t total_events()  const { return total_events_; }

private:
    /// Decode a single log JSON object into an Execution.
    Execution decode_order_filled(const nlohmann::json& log) const;

    /// Extract one 256-bit word (64 hex chars) from the event data field.
    static std::string extract_word(const std::string& data, int word_index);

    /// Convert hex string (with or without "0x") to uint64_t.
    /// Returns 0 on parsing failure; clamps overflow to UINT64_MAX.
    static uint64_t hex_to_uint64(const std::string& hex);

    /// Convert uint64_t to hex string with "0x" prefix.
    static std::string uint64_to_hex(uint64_t val);

    RpcClient client_;
    ParserConfig config_;
    uint64_t current_block_;
    uint64_t end_block_;
    uint64_t total_events_;
    std::vector<Execution> cached_events_;
    bool cache_valid_;
};
