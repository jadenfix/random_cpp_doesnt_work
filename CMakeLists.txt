cmake_minimum_required(VERSION 3.15)
project(AlgoTradingSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler Warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter) # Added -Wno-unused-parameter
elseif(MSVC)
  add_compile_options(/W4 /permissive-)
endif()

# --- Find External Libraries (csv2) ---
set(CSV2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/csv2/include")
if(NOT EXISTS "${CSV2_INCLUDE_DIR}/csv2/reader.hpp")
    message(FATAL_ERROR "csv2 header not found at ${CSV2_INCLUDE_DIR}/csv2/reader.hpp")
endif()
message(STATUS "Found csv2 headers at: ${CSV2_INCLUDE_DIR}")

# --- Define Source Files ---
# Add .cpp files IF they contain implementation. If logic is header-only, leave empty.
set(CORE_SOURCES
    # src/core/Portfolio.cpp          # Uncomment if Portfolio.cpp has code
    # src/core/ExecutionHandler.cpp   # Uncomment if ExecutionHandler.cpp has code
    # src/core/Backtester.cpp         # Uncomment if Backtester.cpp has code
)

set(DATA_SOURCES
    src/data/DataManager.cpp
)

set(STRATEGY_SOURCES
    # src/strategies/MovingAverageCrossover.cpp # Uncomment if MovingAverageCrossover.cpp has code
)

set(MAIN_SOURCE src/main.cpp)


# --- Build Target ---
add_executable(trading_system
    ${MAIN_SOURCE}
    ${CORE_SOURCES}
    ${DATA_SOURCES}
    ${STRATEGY_SOURCES}
)

# --- Include Directories ---
# This allows #include "core/Event.h", "data/PriceBar.h" etc. from any src file
target_include_directories(trading_system PRIVATE
    src
    ${CSV2_INCLUDE_DIR}
)

# --- Link Libraries ---
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(trading_system PRIVATE stdc++fs)
  message(STATUS "Linking against stdc++fs for older GCC")
endif()