add_executable(core_bench time/fixed_step_clock_bench.cpp)
target_link_libraries(core_bench PRIVATE tp::core benchmark::benchmark_main)

if(TP_BUILD_DATA_IO)
    target_sources(core_bench PRIVATE engine/engine_bench.cpp)
    target_link_libraries(core_bench PRIVATE tp::data)
    target_compile_definitions(core_bench PRIVATE TP_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
endif()

tp_configure_target(core_bench)
