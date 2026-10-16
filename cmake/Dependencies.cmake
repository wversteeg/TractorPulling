# Third-party dependencies, pinned to release commits. FIND_PACKAGE_ARGS lets
# an installed package (e.g. from a system package manager) satisfy each one.
# SYSTEM keeps their headers out of our warning flags.

include(FetchContent)

if(TP_BUILD_TESTS)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(BUILD_GMOCK ON CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG 52eb8108c5bdec04579160ae17225d66034bd723 # v1.17.0
        SYSTEM
        FIND_PACKAGE_ARGS 1.17.0 NAMES GTest)
    FetchContent_MakeAvailable(googletest)
endif()

if(TP_BUILD_BENCHMARKS)
    set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_GTEST_TESTS OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_ENABLE_WERROR OFF CACHE BOOL "" FORCE)
    set(BENCHMARK_INSTALL_DOCS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(benchmark
        GIT_REPOSITORY https://github.com/google/benchmark.git
        GIT_TAG eddb0241389718a23a42db6af5f0164b6e0139af # v1.9.4
        SYSTEM
        FIND_PACKAGE_ARGS 1.9.4)
    FetchContent_MakeAvailable(benchmark)
endif()

if(TP_BUILD_TESTS OR TP_BUILD_TOOLS)
    FetchContent_Declare(tomlplusplus
        GIT_REPOSITORY https://github.com/marzer/tomlplusplus.git
        GIT_TAG 30172438cee64926dc41fdd9c11fb3ba5b2ba9de # v3.4.0
        SYSTEM
        FIND_PACKAGE_ARGS 3.4.0)
    FetchContent_MakeAvailable(tomlplusplus)
endif()
