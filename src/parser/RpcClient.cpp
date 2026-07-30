#include "parser/RpcClient.hpp"
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <sstream>

using json = nlohmann::json;

// ────────────────────────────────────────────────────────────────────
//  write_callback
// ────────────────────────────────────────────────────────────────────
size_t RpcClient::write_callback(void* contents, size_t size, size_t nmemb, std::string* output) {
    size_t total = size * nmemb;
    output->append(static_cast<char*>(contents), total);
    return total;
}

// ────────────────────────────────────────────────────────────────────
//  Constructor / destructor
// ────────────────────────────────────────────────────────────────────
RpcClient::RpcClient(const std::string& rpc_url)
    : rpc_url_(rpc_url), curl_(nullptr) {
    curl_ = curl_easy_init();
    if (!curl_) {
        throw std::runtime_error("RpcClient: curl_easy_init() failed");
    }

    curl_easy_setopt(curl_, CURLOPT_URL, rpc_url_.c_str());
    curl_easy_setopt(curl_, CURLOPT_POST, 1L);

    // JSON content-type
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl_, CURLOPT_HTTPHEADER, headers);

    // Write callback
    curl_easy_setopt(curl_, CURLOPT_WRITEFUNCTION, write_callback);

    // Timeouts
    curl_easy_setopt(curl_, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl_, CURLOPT_TIMEOUT, 30L);
}

RpcClient::~RpcClient() {
    if (curl_) {
        // Free the header list stored on the easy handle
        struct curl_slist* headers = nullptr;
        curl_easy_getinfo(curl_, CURLINFO_PRIVATE, &headers);
        (void)headers; // CURLOPT_HTTPHEADER list is tied to the easy handle lifetime

        curl_easy_cleanup(curl_);
    }
}

// ────────────────────────────────────────────────────────────────────
//  call
// ────────────────────────────────────────────────────────────────────
std::string RpcClient::call(const std::string& method, const std::string& params) {
    // Build JSON-RPC 2.0 request body
    json request;
    request["jsonrpc"] = "2.0";
    request["method"]  = method;
    request["id"]      = 1;

    // params may be a raw string like "[]" or a JSON object/array string
    if (!params.empty()) {
        request["params"] = json::parse(params);
    }

    std::string body = request.dump();

    curl_easy_setopt(curl_, CURLOPT_POSTFIELDS, body.c_str());
    curl_easy_setopt(curl_, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

    // Capture response
    std::string response;
    curl_easy_setopt(curl_, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl_);
    if (res != CURLE_OK) {
        throw std::runtime_error(std::string("RpcClient::call curl error: ") + curl_easy_strerror(res));
    }

    // Check HTTP status
    long http_code = 0;
    curl_easy_getinfo(curl_, CURLINFO_RESPONSE_CODE, &http_code);
    if (http_code != 200) {
        throw std::runtime_error("RpcClient::call HTTP " + std::to_string(http_code) + ": " + response);
    }

    return response;
}

// ────────────────────────────────────────────────────────────────────
//  Convenience methods
// ────────────────────────────────────────────────────────────────────
std::string RpcClient::get_block_number() {
    std::string resp = call("eth_blockNumber", "[]");
    auto j = json::parse(resp);
    if (j.contains("error") && !j["error"].is_null()) {
        throw std::runtime_error("eth_blockNumber error: " + j["error"]["message"].get<std::string>());
    }
    return j["result"].get<std::string>();
}

std::string RpcClient::get_logs(
    const std::string& from_block,
    const std::string& to_block,
    const std::string& address,
    const std::string& topics) {
    // Build the filter object
    json filter;
    filter["fromBlock"] = from_block;
    filter["toBlock"]   = to_block;
    filter["address"]   = address;

    // topics is already a JSON array string, parse it
    filter["topics"] = json::parse(topics);

    // Wrap in JSON-RPC params: [filter]
    json params_array = json::array({filter});
    std::string params_str = params_array.dump();

    std::string resp = call("eth_getLogs", params_str);
    auto j = json::parse(resp);
    if (j.contains("error") && !j["error"].is_null()) {
        throw std::runtime_error("eth_getLogs error: " + j["error"]["message"].get<std::string>());
    }
    return resp;
}

std::string RpcClient::get_block_by_number(const std::string& block_num, bool full_tx) {
    json params_array = json::array({block_num, full_tx});
    std::string params_str = params_array.dump();

    std::string resp = call("eth_getBlockByNumber", params_str);
    auto j = json::parse(resp);
    if (j.contains("error") && !j["error"].is_null()) {
        throw std::runtime_error("eth_getBlockByNumber error: " + j["error"]["message"].get<std::string>());
    }
    return resp;
}
