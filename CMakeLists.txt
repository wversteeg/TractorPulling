cmake_minimum_required(VERSION 3.25)

project(TractorPulling
    VERSION 0.1.0
    DESCRIPTION "Deterministic tractor pulling simulation"
    LANGUAGES CXX)

if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_STANDARD 20)
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
endif()

option(TP_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TP_BUILD_DATA_IO "Build the data loading library (needs toml++)" ON)
option(TP_BUILD_TESTS "Build the unit tests" ${PROJECT_IS_TOP_LEVEL})
option(TP_BUILD_BENCHMARKS "Build the micro-benchmarks" ${PROJECT_IS_TOP_LEVEL})
option(TP_BUILD_TOOLS "Build the command line tools" ${PROJECT_IS_TOP_LEVEL})
option(TP_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
# Where the tests and benchmarks read data/ when they run: the source tree,
# or for a cross build (Android) the directory the files are copied to on
# the device.
set(TP_DATA_DIR "${PROJECT_SOURCE_DIR}/data" CACHE PATH
    "Where tests and benchmarks find the data/ files at run time")

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerOptions)
include(Dependencies)

add_subdirectory(core)

if(TP_BUILD_DATA_IO)
    add_subdirectory(data_io)
endif()

if(TP_BUILD_TOOLS)
    add_subdirectory(tools)
endif()

if(TP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

if(TP_BUILD_BENCHMARKS)
    add_subdirectory(bench)
endif()
