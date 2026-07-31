#pragma once
#include <string>
#include <vector>
#include <memory>
#include <unordered_map>
#include <nlohmann/json.hpp>
#include "parser/RpcClient.hpp"
#include "book/Execution.hpp"

/// Which CTF Exchange contract to query.
enum class ExchangeVersion {
    V1,  // 0x4bFb... — original exchange (deprecated, low activity)
    V2   // 0xE111... — current exchange (active)
};

/// Configuration for the Polygon block parser.
struct ParserConfig {
    std::string rpc_url;

    /// Which exchange version to target.
    ExchangeVersion exchange_version = ExchangeVersion::V2;

    /// CTF Exchange contract address (set automatically from exchange_version).
    std::string ctf_exchange_address;

    /// Event signature for OrderFilled (set automatically from exchange_version).
    std::string order_filled_topic;

    /// Start block (0 = latest minus lookback_blocks).
    uint64_t start_block = 0;

    /// End block (0 = stream indefinitely).
    uint64_t end_block = 0;

    /// Number of blocks to look back when start_block == 0.
    uint64_t lookback_blocks = 100;

    /// Number of blocks to fetch in a single eth_getLogs call.
    /// Batching amortizes RPC latency. Default 100 (drpc.org limit).
    /// Set to 1 for single-block mode (legacy behaviour).
    uint64_t batch_size = 100;
};

/// Streams Polygon blocks, fetches OrderFilled logs, and decodes them
/// into Execution structs via ABI decoding.
///
/// Supports both V1 and V2 CTF Exchange contracts with automatic
/// ABI layout detection.
///
/// Batching: fetches up to batch_size blocks per eth_getLogs call
/// and caches per-block results, reducing RPC round-trips by ~100x.
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
    /// Decode a single V1 log JSON object into an Execution.
    Execution decode_order_filled_v1(const nlohmann::json& log) const;

    /// Decode a single V2 log JSON object into an Execution.
    Execution decode_order_filled_v2(const nlohmann::json& log) const;

    /// Fetch events for a range of blocks and populate the batch cache.
    void fetch_batch(uint64_t from, uint64_t to);

    /// Extract one 256-bit word (64 hex chars) from the event data field.
    static std::string extract_word(const std::string& data, int word_index);

    /// Convert hex string (with or without "0x") to uint64_t.
    /// Returns 0 on parsing failure; clamps overflow to UINT64_MAX.
    static uint64_t hex_to_uint64(const std::string& hex);

    /// Convert uint64_t to hex string with "0x" prefix.
    static std::string uint64_to_hex(uint64_t val);

    /// Convert address to uint64_t (takes lower 8 bytes).
    static uint64_t address_to_uint64(const std::string& addr);

    RpcClient client_;
    ParserConfig config_;
    ExchangeVersion version_;
    uint64_t current_block_;
    uint64_t end_block_;
    uint64_t total_events_;

    // ── Batch state ─────────────────────────────────────────────────
    uint64_t batch_from_ = 0;   // first block in current batch
    uint64_t batch_to_ = 0;     // last block in current batch (inclusive)
    std::unordered_map<uint64_t, std::vector<Execution>> batch_cache_;
    std::vector<Execution> cached_events_;
    bool cache_valid_ = false;
};
