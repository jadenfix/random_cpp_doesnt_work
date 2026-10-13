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
set(CSV2_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/external/csv2/include" CACHE PATH "Directory containing csv2 headers")
if(NOT EXISTS "${CSV2_INCLUDE_DIR}/csv2/reader.hpp")
    # The old gitlink has no .gitmodules entry. Fetch the same pinned revision
    # on a clean checkout; offline builders can supply CSV2_INCLUDE_DIR.
    include(FetchContent)
    FetchContent_Declare(csv2
        GIT_REPOSITORY https://github.com/p-ranav/csv2.git
        GIT_TAG 4f3c41db6457465e94b92b91fc560b911c16a16a
    )
    FetchContent_GetProperties(csv2)
    if(NOT csv2_POPULATED)
        FetchContent_Populate(csv2)
    endif()
    set(CSV2_INCLUDE_DIR "${csv2_SOURCE_DIR}/include")
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
    # The DRL prototype is not used by main and depends on missing interfaces.
    # Keep it out of the runnable traditional-strategy target.
)

# --- Executable Definition ---
# Define the target FIRST
add_executable(${PROJECT_NAME} ${SOURCES})

# --- Include Directories ---
# NOW specify include directories for the defined target
target_include_directories(${PROJECT_NAME} PUBLIC # Using PUBLIC makes these includes available if other targets link to this one
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CSV2_INCLUDE_DIR}"
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

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 COMPONENTS Interpreter)
    if(Python3_Interpreter_FOUND)
        add_executable(csv_data_manager_probe tests/data_manager_probe.cpp src/DataManager.cpp)
        target_include_directories(csv_data_manager_probe PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/include" "${CSV2_INCLUDE_DIR}")
        add_test(NAME backtester_smoke
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/smoke.py"
                    "$<TARGET_FILE:${PROJECT_NAME}>")
        add_test(NAME csv_file_headers
            COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/tests/csv_headers.py"
                    "$<TARGET_FILE:csv_data_manager_probe>" "$<TARGET_FILE:${PROJECT_NAME}>")
    endif()
endif()
