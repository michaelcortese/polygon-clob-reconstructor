# Polygon CLOB Reconstructor

A high-performance C++ engine that streams Polygon blockchain blocks, decodes CTF Exchange `OrderFilled` events, and reconstructs limit order books in real time.

## Requirements

- **C++20** compiler (GCC 11+ or Clang 14+)
- **CMake** ≥ 3.20
- **libcurl** (for HTTP streaming of Polygon blocks)
- Dependencies fetched automatically: [nlohmann/json](https://github.com/nlohmann/json), [Catch2](https://github.com/catchorg/Catch2)

## Build

```bash
cmake -B build -S .
cmake --build build
```

## Run

```bash
./build/polygon_clob_reconstructor
```

## Test

```bash
ctest --test-dir build --output-on-failure
```

## Project Structure

```
polygon-clob-reconstructor/
├── CMakeLists.txt          # Build configuration
├── include/                # Public headers
├── src/                    # Source files
├── tests/                  # Catch2 unit tests
└── .github/workflows/      # CI pipeline
```