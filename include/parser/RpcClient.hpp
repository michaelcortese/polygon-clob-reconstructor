#pragma once
#include <string>
#include <memory>
#include <curl/curl.h>

class RpcClient {
public:
    explicit RpcClient(const std::string& rpc_url);
    ~RpcClient();

    // Non-copyable, non-movable (owns CURL handle)
    RpcClient(const RpcClient&) = delete;
    RpcClient& operator=(const RpcClient&) = delete;
    RpcClient(RpcClient&&) = delete;
    RpcClient& operator=(RpcClient&&) = delete;

    /// Send a JSON-RPC 2.0 request, return the raw response body as a string.
    std::string call(const std::string& method, const std::string& params);

    /// Convenience: eth_blockNumber → returns hex block number string.
    std::string get_block_number();

    /// eth_getLogs with a single-topic filter on one address.
    /// from_block / to_block are hex strings (e.g. "0x1A").
    /// address is the contract address.
    /// topics is a JSON array string, e.g. '["0xabcd..."]'.
    std::string get_logs(
        const std::string& from_block,
        const std::string& to_block,
        const std::string& address,
        const std::string& topics);

    /// eth_getBlockByNumber: full_tx = false returns tx hashes only.
    std::string get_block_by_number(const std::string& block_num, bool full_tx);

private:
    std::string rpc_url_;
    CURL* curl_;

    /// libcurl write callback — appends received data to the output string.
    static size_t write_callback(void* contents, size_t size, size_t nmemb, std::string* output);
};
