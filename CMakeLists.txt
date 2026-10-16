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
option(TP_ENABLE_SANITIZERS "Build with AddressSanitizer and UndefinedBehaviorSanitizer" OFF)

list(APPEND CMAKE_MODULE_PATH "${PROJECT_SOURCE_DIR}/cmake")
include(CompilerOptions)

add_subdirectory(core)
