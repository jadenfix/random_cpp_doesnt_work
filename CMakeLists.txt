# Specify the minimum version of CMake required
cmake_minimum_required(VERSION 3.14 FATAL_ERROR) # Research doc used 3.14, 3.15 also fine

# Define the project name and language(s)
project(FinancialBacktester LANGUAGES CXX)

# --- C++ Standard ---
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# --- Compiler Warnings ---
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
  add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)
elseif(MSVC)
  add_compile_options(/W4 /permissive-)
endif()

# --- Find External Libraries (csv2) ---
set(CSV2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/csv2/include")
if(NOT EXISTS "${CSV2_INCLUDE_DIR}/csv2/reader.hpp")
    message(FATAL_ERROR "csv2 header 'reader.hpp' not found at expected location: ${CSV2_INCLUDE_DIR}/csv2/\n"
                        "Please ensure the csv2 library is correctly placed in 'external/csv2/include/csv2/'")
endif()
message(STATUS "Found csv2 headers at: ${CSV2_INCLUDE_DIR}")

# --- Source Files ---
# List all .cpp files that need compilation
set(SOURCES
    src/main.cpp
    src/Backtester.cpp
    src/DataManager.cpp
    src/Portfolio.cpp
    src/ExecutionSimulator.cpp
    src/DRLStrategy.cpp
    src/FeatureCalculator.cpp   # Keep even if empty
    src/DRLInferenceEngine.cpp  # Keep even if empty
)

# --- Executable Definition ---
# Define the target FIRST
add_executable(${PROJECT_NAME} ${SOURCES})

# --- Include Directories ---
# NOW specify include directories for the defined target
target_include_directories(${PROJECT_NAME} PUBLIC # Using PUBLIC makes these includes available if other targets link to this one
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/external/csv2/include"
)

# --- External Library Placeholders ---
# ... (TA-Lib placeholder) ...
# ... (ONNX Runtime placeholder) ...

# --- Linking ---
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND CMAKE_CXX_COMPILER_VERSION VERSION_LESS 9.0)
  target_link_libraries(${PROJECT_NAME} PRIVATE stdc++fs)
  message(STATUS "Linking against stdc++fs for older GCC on Linux")
endif()

# --- Final Messages ---
message(STATUS "Project Name: ${PROJECT_NAME}")
message(STATUS "Configuring Build for Target: ${PROJECT_NAME}")