cmake_minimum_required(VERSION 3.15)
project(AlgoTradingSystem VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic)
elseif(MSVC)
  add_compile_options(/W4 /permissive-)
endif()

# Assumes csv2 headers are in external/csv2/include/csv2/reader.hpp
set(CSV2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/csv2/include")
if(NOT EXISTS "${CSV2_INCLUDE_DIR}/csv2/reader.hpp")
    message(FATAL_ERROR "csv2 header not found at expected location: ${CSV2_INCLUDE_DIR}/csv2/reader.hpp\n"
                        "Please ensure 'reader.hpp' is inside '<project_root>/external/csv2/include/csv2/'")
endif()
message(STATUS "Found csv2 headers at: ${CSV2_INCLUDE_DIR}")

add_executable(trading_system
    src/main.cpp
    src/DataManager.cpp
)

target_include_directories(trading_system PRIVATE
    src # Project headers
    ${CSV2_INCLUDE_DIR} # csv2 headers
)

# Link stdc++fs if needed (GCC < 9.0)
if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(trading_system PRIVATE stdc++fs)
  message(STATUS "Linking against stdc++fs for older GCC")
endif()