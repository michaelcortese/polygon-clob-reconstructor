#include "parser/PolygonParser.hpp"
#include "logger/Logger.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <stdexcept>
#include <cstdlib>
#include <climits>
#include <cstring>

using json = nlohmann::json;

// ────────────────────────────────────────────────────────────────────
//  Static helpers
// ────────────────────────────────────────────────────────────────────

std::string PolygonParser::extract_word(const std::string& data, int word_index) {
    // data format: "0x" + N × 64 hex chars
    // Skip "0x" (2 chars), then word_index * 64 chars
    // Returns the 64-char hex chunk (without "0x")
    size_t start = 2 + static_cast<size_t>(word_index) * 64;
    if (start + 64 > data.size()) {
        return "0";
    }
    return data.substr(start, 64);
}

uint64_t PolygonParser::hex_to_uint64(const std::string& hex) {
    std::string s = hex;
    // Strip leading "0x" / "0X"
    if (s.size() >= 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s = s.substr(2);
    }
    if (s.empty()) return 0;

    // For 64-char hex strings (256-bit), take only the lower 16 chars (64-bit)
    if (s.size() > 16) {
        s = s.substr(s.size() - 16);
    }

    errno = 0;
    unsigned long long val = strtoull(s.c_str(), nullptr, 16);
    if (errno == ERANGE || (val == ULLONG_MAX && errno != 0)) {
        return UINT64_MAX;
    }
    return static_cast<uint64_t>(val);
}

std::string PolygonParser::uint64_to_hex(uint64_t val) {
    std::ostringstream oss;
    oss << "0x" << std::hex << val;
    return oss.str();
}

// ────────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────────

PolygonParser::PolygonParser(const ParserConfig& config)
    : client_(config.rpc_url)
    , config_(config)
    , current_block_(0)
    , end_block_(0)
    , total_events_(0)
    , cache_valid_(false) {
    // Determine current_block_
    if (config_.start_block > 0) {
        current_block_ = config_.start_block;
    } else {
        // Get latest block then subtract lookback
        std::string latest_hex = client_.get_block_number();
        uint64_t latest = hex_to_uint64(latest_hex);
        current_block_ = (latest > config_.lookback_blocks)
                             ? (latest - config_.lookback_blocks)
                             : 0;
        Logger::getInstance().log("PolygonParser: latest=" + latest_hex +
                                  " starting from " + uint64_to_hex(current_block_) +
                                  " (lookback=" + std::to_string(config_.lookback_blocks) + ")");
    }

    // Determine end_block_
    end_block_ = config_.end_block;

    Logger::getInstance().log("PolygonParser: initialized, start=" + uint64_to_hex(current_block_) +
                              " end=" + (end_block_ == 0 ? std::string("stream") : uint64_to_hex(end_block_)));
}

// ────────────────────────────────────────────────────────────────────
//  Streaming interface
// ────────────────────────────────────────────────────────────────────

bool PolygonParser::has_next() const {
    if (end_block_ == 0) return true;          // infinite streaming
    return current_block_ <= end_block_;
}

std::vector<Execution> PolygonParser::next_events() {
    if (cache_valid_) {
        return cached_events_;
    }

    cached_events_.clear();

    std::string from_hex = uint64_to_hex(current_block_);
    std::string to_hex   = uint64_to_hex(current_block_);

    // Build the single-topic filter
    std::string topics = "[\"" + config_.order_filled_topic + "\"]";

    std::string response = client_.get_logs(
        from_hex, to_hex, config_.ctf_exchange_address, topics
    );

    auto j = json::parse(response);
    if (!j.contains("result") || !j["result"].is_array()) {
        Logger::getInstance().log("PolygonParser: unexpected response at block " + from_hex);
        cache_valid_ = true;
        return cached_events_;
    }

    for (const auto& log : j["result"]) {
        try {
            Execution exec = decode_order_filled(log);
            cached_events_.push_back(exec);
            total_events_++;
        } catch (const std::exception& e) {
            Logger::getInstance().log(
                "PolygonParser: decode error at block " + from_hex + ": " + e.what());
        }
    }

    cache_valid_ = true;
    return cached_events_;
}

void PolygonParser::advance() {
    current_block_++;
    cache_valid_ = false;
    cached_events_.clear();
}

// ────────────────────────────────────────────────────────────────────
//  ABI decode: OrderFilled event
// ────────────────────────────────────────────────────────────────────

Execution PolygonParser::decode_order_filled(const json& log) const {
    Execution exec{};

    // ── block number ────────────────────────────────────────────────
    if (log.contains("blockNumber")) {
        std::string block_hex = log["blockNumber"].get<std::string>();
        exec.block_number = hex_to_uint64(block_hex);
    }

    // ── orderHash from topics[1] (topics[0] is the event signature) ─
    if (log.contains("topics") && log["topics"].is_array() && log["topics"].size() > 1) {
        std::string order_hash = log["topics"][1].get<std::string>();
        // match_id = first 8 bytes (16 hex chars after "0x")
        exec.match_id = hex_to_uint64(order_hash);
    }

    // ── data field: 5 × uint256 non-indexed params ──────────────────
    // layout: makerAssetId | takerAssetId | makerAmountFilled | takerAmountFilled | fee
    std::string data = log.value("data", "0x");

    uint64_t taker_asset_id    = hex_to_uint64("0x" + extract_word(data, 1));
    uint64_t maker_amount      = hex_to_uint64("0x" + extract_word(data, 2));
    uint64_t taker_amount      = hex_to_uint64("0x" + extract_word(data, 3));
    // uint64_t fee               = hex_to_uint64("0x" + extract_word(data, 4)); // reserved, unused

    // ── Determine side ──────────────────────────────────────────────
    // takerAssetId == 0 → taker is posting USDC → taker is BUYER
    exec.is_buy = (taker_asset_id == 0);

    // ── Price (in USDC cents, scaled by 100 to avoid floats) ────────
    // If buyer: price = makerAmountFilled * 100 / takerAmountFilled
    //           (maker posted tokens, taker posted USDC → tokens/USDC)
    // If seller: price = takerAmountFilled * 100 / makerAmountFilled
    //           (maker is the seller posting tokens, taker is buying)
    if (exec.is_buy) {
        // taker buys token → price = makerAsset / takerAsset ≈ tokens per USDC
        if (taker_amount > 0) {
            exec.price = static_cast<uint32_t>((maker_amount * 100) / taker_amount);
        } else {
            exec.price = 0;
        }
    } else {
        // taker sells token → price = takerAsset / makerAsset
        if (maker_amount > 0) {
            exec.price = static_cast<uint32_t>((taker_amount * 100) / maker_amount);
        } else {
            exec.price = 0;
        }
    }

    // ── Quantity = the token-side amount (non-USDC side) ───────────
    // The token side is the non-zero asset id, or the smaller amount
    if (exec.is_buy) {
        // buyer: taker posts USDC (assetId 0), maker posts tokens
        exec.quantity = static_cast<uint32_t>(maker_amount);
    } else {
        // seller: taker sells tokens, maker posts USDC
        exec.quantity = static_cast<uint32_t>(taker_amount);
    }

    // Clamp quantity to uint32_t max (already static_cast but guard overflow)
    if (maker_amount > UINT32_MAX || taker_amount > UINT32_MAX) {
        // Values are already clamped to UINT64_MAX in hex_to_uint64
        exec.quantity = UINT32_MAX;
    }

    // ── timestamp ───────────────────────────────────────────────────
    exec.timestamp = 0; // filled by caller if needed (requires block timestamp)

    return exec;
}
