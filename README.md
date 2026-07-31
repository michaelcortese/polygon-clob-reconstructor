# Polygon CLOB Reconstructor

[![CI](https://github.com/michaelcortese/polygon-clob-reconstructor/actions/workflows/ci.yml/badge.svg)](https://github.com/michaelcortese/polygon-clob-reconstructor/actions/workflows/ci.yml)
[![License: MIT](https://img.shields.io/badge/license-MIT-blue.svg)](#license)

A C++20 command-line tool for reading Polymarket CTF Exchange fills from
Polygon. It queries `OrderFilled` logs over JSON-RPC, decodes the V1 or V2 event
format, and processes the results in block order. Decoded fills can also be
written to CSV for analysis elsewhere.

## Status

This is an experimental fill-replay tool. An `OrderFilled` event records a
trade, but it does not include every resting order or cancellation. The data is
therefore useful as an on-chain trade tape and for fill-derived statistics, not
as a complete L2 or L3 order book.

Per-market routing is still a work in progress: the current
`OrderBookManager` uses a bucket derived from the match ID rather than the full
token ID. Keep that limitation in mind when interpreting its book summaries.

## Build

You will need:

- a C++20 compiler (GCC 11+ or Clang 14+)
- CMake 3.20 or newer
- the libcurl development package
- Git, so CMake can fetch Catch2 for the test target

On Debian or Ubuntu, install libcurl with:

```bash
sudo apt-get install libcurl4-openssl-dev
```

Then clone and build the project:

```bash
git clone https://github.com/michaelcortese/polygon-clob-reconstructor.git
cd polygon-clob-reconstructor
cmake -S . -B build
cmake --build build
```

`nlohmann/json` is vendored in `third_party/`; CMake fetches Catch2 3.6.0 while
configuring the build.

## Run

Pass a Polygon JSON-RPC endpoint and the block range you want to read:

```bash
./build/polygon_clob_reconstructor \
  --rpc-url https://polygon-rpc.com \
  --exchange v2 \
  --start-block 50000000 \
  --end-block 50001000 \
  --output fills.csv
```

The V2 exchange is the default. If `--start-block` is omitted, the program
starts 100 blocks behind the latest block. If `--end-block` is omitted, it keeps
running until interrupted. Use `Ctrl+C` to stop cleanly and print a summary.

The most useful options are:

| Option | Meaning | Default |
| --- | --- | --- |
| `--rpc-url <url>` | Polygon JSON-RPC endpoint | `http://localhost:8545` |
| `--exchange <v1\|v2>` | CTF Exchange contract and event layout | `v2` |
| `--start-block <n>` | First block to read | latest block minus lookback |
| `--end-block <n>` | Last block to read | no limit |
| `--lookback <n>` | Lookback when no start block is supplied | `100` |
| `--batch-size <n>` | Blocks requested by each `eth_getLogs` call | `100` |
| `--progress <n>` | Blocks between progress reports | `100` |
| `--output <file>` | Write decoded fills to CSV | disabled |

Run `./build/polygon_clob_reconstructor --help` for the same list in the
terminal. The CSV contains `block_number`, `match_id`, `price`, `quantity`, and
`is_buy`.

## How it is put together

The main loop asks `PolygonParser` for a batch of logs, decodes each fill into
an `Execution`, and hands the executions to `OrderBookManager`. The manager
accumulates fill-side price levels and trade history; the analytics classes can
then calculate snapshots such as VWAP, spread, volume, and depth from that
state.

```text
Polygon RPC -> RpcClient -> PolygonParser -> OrderBookManager -> analytics / CSV
```

## Tests

```bash
ctest --test-dir build --output-on-failure
```

Catch2 tags can be run directly when you only need part of the suite:

```bash
./build/polygon_clob_reconstructor_tests "[orderbook]"
./build/polygon_clob_reconstructor_tests "[integration]"
./build/polygon_clob_reconstructor_tests "[benchmark]"
```

## License

MIT
