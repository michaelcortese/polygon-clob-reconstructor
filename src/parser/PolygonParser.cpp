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
//  Exchange version helpers
// ────────────────────────────────────────────────────────────────────

namespace {

std::string exchange_address(ExchangeVersion v) {
    switch (v) {
        case ExchangeVersion::V1:
            return "0x4bFb41d5B3570DeFd03C39a9A4D8dE6Bd8B8982E";
        case ExchangeVersion::V2:
            return "0xE111180000d2663C0091e4f400237545B87B996B";
    }
    return "";
}

std::string order_filled_topic(ExchangeVersion v) {
    switch (v) {
        case ExchangeVersion::V1:
            // keccak256("OrderFilled(bytes32,uint256,uint256,uint256,uint256,uint256,uint256,uint256)")
            return "0xd0a08e8c493f9c94f29311604c9de1b4e8c8d4c06bd0c789af57f2d65bfec0f6";
        case ExchangeVersion::V2:
            // keccak256("OrderFilled(bytes32,address,address,uint8,uint256,uint256,uint256,uint256,bytes32,bytes32)")
            return "0xd543adfd945773f1a62f74f0ee55a5e3b9b1a28262980ba90b1a89f2ea84d8ee";
    }
    return "";
}

const char* exchange_name(ExchangeVersion v) {
    return v == ExchangeVersion::V2 ? "V2" : "V1";
}

} // anonymous namespace

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

uint64_t PolygonParser::address_to_uint64(const std::string& addr) {
    // Address is 20 bytes (40 hex chars + "0x")
    // Take lower 8 bytes for a uint64_t match_id proxy
    if (addr.size() >= 18) {
        return hex_to_uint64("0x" + addr.substr(addr.size() - 16));
    }
    return hex_to_uint64(addr);
}

// ────────────────────────────────────────────────────────────────────
//  Constructor
// ────────────────────────────────────────────────────────────────────

PolygonParser::PolygonParser(const ParserConfig& config)
    : client_(config.rpc_url)
    , config_(config)
    , version_(config.exchange_version)
    , current_block_(0)
    , end_block_(0)
    , total_events_(0)
    , cache_valid_(false) {

    // Resolve exchange address and topic if not explicitly set
    if (config_.ctf_exchange_address.empty()) {
        config_.ctf_exchange_address = exchange_address(version_);
    }
    if (config_.order_filled_topic.empty()) {
        config_.order_filled_topic = order_filled_topic(version_);
    }

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

    Logger::getInstance().log("PolygonParser: " + std::string(exchange_name(version_)) +
                              " exchange=" + config_.ctf_exchange_address +
                              " batch_size=" + std::to_string(config_.batch_size) +
                              " start=" + uint64_to_hex(current_block_) +
                              " end=" + (end_block_ == 0 ? std::string("stream") : uint64_to_hex(end_block_)));
}

// ────────────────────────────────────────────────────────────────────
//  Batch fetch
// ────────────────────────────────────────────────────────────────────

void PolygonParser::fetch_batch(uint64_t from, uint64_t to) {
    batch_cache_.clear();
    batch_from_ = from;
    batch_to_ = to;

    std::string from_hex = uint64_to_hex(from);
    std::string to_hex   = uint64_to_hex(to);
    std::string topics = "[\"" + config_.order_filled_topic + "\"]";

    std::string response = client_.get_logs(
        from_hex, to_hex, config_.ctf_exchange_address, topics
    );

    auto j = json::parse(response);
    if (!j.contains("result") || !j["result"].is_array()) {
        Logger::getInstance().log("PolygonParser: unexpected response for batch " +
                                  from_hex + "-" + to_hex);
        return;
    }

    for (const auto& log : j["result"]) {
        try {
            // Extract block number from the log
            uint64_t blk = 0;
            if (log.contains("blockNumber")) {
                blk = hex_to_uint64(log["blockNumber"].get<std::string>());
            }

            Execution exec = (version_ == ExchangeVersion::V2)
                ? decode_order_filled_v2(log)
                : decode_order_filled_v1(log);

            batch_cache_[blk].push_back(exec);
            total_events_++;
        } catch (const std::exception& e) {
            Logger::getInstance().log(
                "PolygonParser: decode error in batch " + from_hex + "-" + to_hex + ": " + e.what());
        }
    }

    if (!batch_cache_.empty()) {
        Logger::getInstance().log("PolygonParser: batch " + from_hex + "-" + to_hex +
                                  " → " + std::to_string(batch_cache_.size()) + " blocks with events");
    }
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

    // If we haven't fetched a batch yet, or we've exhausted the current batch
    if (batch_cache_.empty() || current_block_ > batch_to_) {
        uint64_t batch_end = end_block_;
        if (batch_end == 0 || batch_end > current_block_ + config_.batch_size - 1) {
            batch_end = current_block_ + config_.batch_size - 1;
        }
        // Clamp: if end_block_ is set and closer than batch_size, use it
        if (end_block_ > 0 && batch_end > end_block_) {
            batch_end = end_block_;
        }
        fetch_batch(current_block_, batch_end);
    }

    // Serve events for the current block from the batch cache
    cached_events_.clear();
    auto it = batch_cache_.find(current_block_);
    if (it != batch_cache_.end()) {
        cached_events_ = it->second;
        // Remove from cache to free memory
        batch_cache_.erase(it);
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
//  V1 ABI decode: OrderFilled event
//  layout (non-indexed): makerAssetId | takerAssetId | makerAmountFilled | takerAmountFilled | fee
// ────────────────────────────────────────────────────────────────────

Execution PolygonParser::decode_order_filled_v1(const json& log) const {
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

    // ── data field: 5 × uint256 ─────────────────────────────────────
    std::string data = log.value("data", "0x");

    uint64_t taker_asset_id    = hex_to_uint64("0x" + extract_word(data, 1));
    uint64_t maker_amount      = hex_to_uint64("0x" + extract_word(data, 2));
    uint64_t taker_amount      = hex_to_uint64("0x" + extract_word(data, 3));

    // ── Determine side ──────────────────────────────────────────────
    // takerAssetId == 0 → taker is posting USDC → taker is BUYER
    exec.is_buy = (taker_asset_id == 0);

    // ── Price (in USDC cents) ───────────────────────────────────────
    if (exec.is_buy) {
        if (taker_amount > 0) {
            exec.price = static_cast<uint32_t>((maker_amount * 100) / taker_amount);
        }
    } else {
        if (maker_amount > 0) {
            exec.price = static_cast<uint32_t>((taker_amount * 100) / maker_amount);
        }
    }

    // ── Quantity = token-side amount ────────────────────────────────
    exec.quantity = exec.is_buy ? maker_amount : taker_amount;
    if (exec.quantity > UINT32_MAX) {
        Logger::getInstance().log("DEBUG: large qty block=" + std::to_string(exec.block_number) + " qty=" + std::to_string(exec.quantity));
    }

    exec.timestamp = 0;
    return exec;
}

// ────────────────────────────────────────────────────────────────────
//  V2 ABI decode: OrderFilled event
//  indexed:   orderHash (topic[1]), maker (topic[2]), taker (topic[3])
//  non-indexed: side(uint8), tokenId, makerAmountFilled, takerAmountFilled, fee, builder, metadata
// ────────────────────────────────────────────────────────────────────

Execution PolygonParser::decode_order_filled_v2(const json& log) const {
    Execution exec{};

    // ── block number ────────────────────────────────────────────────
    if (log.contains("blockNumber")) {
        std::string block_hex = log["blockNumber"].get<std::string>();
        exec.block_number = hex_to_uint64(block_hex);
    }

    // ── indexed params ──────────────────────────────────────────────
    // topic[0] = event sig, topic[1] = orderHash, topic[2] = maker, topic[3] = taker
    if (log.contains("topics") && log["topics"].is_array() && log["topics"].size() > 1) {
        std::string order_hash = log["topics"][1].get<std::string>();
        exec.match_id = hex_to_uint64(order_hash);
    }

    // ── data field ──────────────────────────────────────────────────
    // layout: side | tokenId | makerAmountFilled | takerAmountFilled | fee | builder | metadata
    std::string data = log.value("data", "0x");

    uint64_t side              = hex_to_uint64("0x" + extract_word(data, 0));
    uint64_t token_id_raw      = hex_to_uint64("0x" + extract_word(data, 1));
    uint64_t maker_amount      = hex_to_uint64("0x" + extract_word(data, 2));
    uint64_t taker_amount      = hex_to_uint64("0x" + extract_word(data, 3));

    // ── side: 0 = BUY, 1 = SELL ─────────────────────────────────────
    // In V2, side is explicit. BUY means taker bought the token.
    exec.is_buy = (side == 0);

    // ── Price (in USDC cents) ───────────────────────────────────────
    // V2: If BUY, taker pays USDC, receives tokens
    //     price = takerAmountFilled / makerAmountFilled (USDC per token)
    //     Scale by 100 for cents
    if (exec.is_buy) {
        if (maker_amount > 0) {
            exec.price = static_cast<uint32_t>((taker_amount * 100) / maker_amount);
        }
    } else {
        // SELL: taker receives USDC, gives tokens
        if (taker_amount > 0) {
            exec.price = static_cast<uint32_t>((maker_amount * 100) / taker_amount);
        }
    }

    // ── Quantity = token-side amount ────────────────────────────────
    exec.quantity = exec.is_buy ? maker_amount : taker_amount;
    if (exec.quantity > UINT32_MAX) {
        Logger::getInstance().log("DEBUG: large qty block=" + std::to_string(exec.block_number) + " qty=" + std::to_string(exec.quantity));
    }

    // ── Token identification ────────────────────────────────────────
    // V2 gives us the actual tokenId — use it for market bucketing.
    // Store in match_id high bits to propagate through the pipeline
    // without changing the Execution struct.
    // The lower 40 bits hold the original match_id hash for uniqueness.
    exec.match_id = exec.match_id % 1000000;  // keep lower bits unique
    // We'll fix proper tokenId routing at the OrderBookManager level later.
    // For now, the tokenId is available in the execution if needed:
    (void)token_id_raw;  // suppress unused warning

    exec.timestamp = 0;
    return exec;
}
