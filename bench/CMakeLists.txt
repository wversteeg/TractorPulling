add_executable(core_bench time/fixed_step_clock_bench.cpp)
target_link_libraries(core_bench PRIVATE tp::core benchmark::benchmark_main)
tp_configure_target(core_bench)
