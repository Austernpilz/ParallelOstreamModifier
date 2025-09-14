cmake_minimum_required(VERSION 3.20)
project(QtBenchmarkApp LANGUAGES CXX)

# Enable C++17 or later
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

#list(APPEND CMAKE_PREFIX_PATH "/buffer/ag_bsc/pmsb/jermac/zlib/build/")

# Add a CMake option for custom compiler flags
set(CUSTOM_CXX_FLAGS "-march=native" CACHE STRING "Custom C++ compiler flags")

find_package(ZLIB)

set(SOURCES
    test2.cpp
    src/StreamBuffer_t.cpp
)

# Setup executable

# Setup executable
add_executable(main ${SOURCES})

target_link_libraries(main PRIVATE ZLIB::ZLIB)

# Apply custom compiler flags if enabled
target_compile_options(main PRIVATE ${CUSTOM_CXX_FLAGS})
