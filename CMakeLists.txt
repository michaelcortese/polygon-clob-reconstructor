cmake_minimum_required(VERSION 3.20)
project(PolygonCLOBReconstructor VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(POLYGON_CLOB_USE_CURL "Enable libcurl support" ON)

# ── FetchContent ──────────────────────────────────────────────────────────────
include(FetchContent)

FetchContent_Declare(
  nlohmann_json
  GIT_REPOSITORY https://github.com/nlohmann/json.git
  GIT_TAG        v3.11.3
)
FetchContent_MakeAvailable(nlohmann_json)

FetchContent_Declare(
  Catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG        v3.6.0
)
FetchContent_MakeAvailable(Catch2)

# ── libcurl ───────────────────────────────────────────────────────────────────
if(POLYGON_CLOB_USE_CURL)
  find_package(CURL QUIET)
  if(NOT CURL_FOUND)
    message(WARNING "libcurl not found — building without HTTP streaming support. "
                    "Install libcurl-dev or set -DPOLYGON_CLOB_USE_CURL=OFF to suppress.")
  endif()
endif()

# ── Threads ───────────────────────────────────────────────────────────────────
find_package(Threads REQUIRED)

# ── Source files ──────────────────────────────────────────────────────────────
file(GLOB_RECURSE SOURCES src/*.cpp)

add_executable(polygon_clob_reconstructor ${SOURCES})

target_include_directories(polygon_clob_reconstructor PRIVATE include)

target_link_libraries(polygon_clob_reconstructor PRIVATE
  nlohmann_json::nlohmann_json
  Threads::Threads
)

if(CURL_FOUND)
  target_compile_definitions(polygon_clob_reconstructor PRIVATE POLYGON_CLOB_HAS_CURL)
  target_link_libraries(polygon_clob_reconstructor PRIVATE CURL::libcurl)
endif()

# ── Tests ─────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)

if(TEST_SOURCES)
  add_executable(polygon_clob_reconstructor_tests ${TEST_SOURCES})
  target_include_directories(polygon_clob_reconstructor_tests PRIVATE include)
  target_link_libraries(polygon_clob_reconstructor_tests PRIVATE
    Catch2::Catch2WithMain
  )
  include(CTest)
  add_test(NAME unit_tests COMMAND polygon_clob_reconstructor_tests)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(polygon_clob_reconstructor PRIVATE -Wall -Wextra -Wpedantic)
endif()