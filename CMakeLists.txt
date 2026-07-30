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

# ── Library (shared sources, excludes main.cpp) ───────────────────────────────
file(GLOB LIB_SOURCES src/logger/*.cpp src/book/*.cpp src/parser/*.cpp)

add_library(polygon_clob_lib STATIC ${LIB_SOURCES})
target_include_directories(polygon_clob_lib PUBLIC include)
target_link_libraries(polygon_clob_lib PUBLIC
  nlohmann_json::nlohmann_json
  Threads::Threads
)

if(CURL_FOUND)
  target_compile_definitions(polygon_clob_lib PUBLIC POLYGON_CLOB_HAS_CURL)
  target_link_libraries(polygon_clob_lib PUBLIC CURL::libcurl)
else()
  set(CURL_SO_LIB "/usr/lib/x86_64-linux-gnu/libcurl.so.4")
  if(EXISTS "${CURL_SO_LIB}")
    target_compile_definitions(polygon_clob_lib PUBLIC POLYGON_CLOB_HAS_CURL)
    target_link_libraries(polygon_clob_lib PUBLIC ${CURL_SO_LIB})
    message(STATUS "Using system libcurl: ${CURL_SO_LIB}")
  endif()
endif()

# ── Main executable ───────────────────────────────────────────────────────────
add_executable(polygon_clob_reconstructor src/main.cpp)
target_link_libraries(polygon_clob_reconstructor PRIVATE polygon_clob_lib)

# ── Tests ─────────────────────────────────────────────────────────────────────
file(GLOB_RECURSE TEST_SOURCES tests/*.cpp)

if(TEST_SOURCES)
  add_executable(polygon_clob_reconstructor_tests ${TEST_SOURCES})
  target_link_libraries(polygon_clob_reconstructor_tests PRIVATE
    polygon_clob_lib
    Catch2::Catch2WithMain
  )
  include(CTest)
  add_test(NAME unit_tests COMMAND polygon_clob_reconstructor_tests)
endif()

# ── Compiler warnings ─────────────────────────────────────────────────────────
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(polygon_clob_lib PUBLIC -Wall -Wextra -Wpedantic)
endif()