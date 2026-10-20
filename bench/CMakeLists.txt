add_executable(core_bench
    math/trig_bench.cpp
    track/track_bench.cpp
    time/fixed_step_clock_bench.cpp)
target_link_libraries(core_bench PRIVATE tp::core benchmark::benchmark_main)

if(TP_BUILD_DATA_IO)
    target_sources(core_bench PRIVATE
        chassis/chassis_bench.cpp
        clutch/clutch_bench.cpp
        controls/controls_bench.cpp
        driveline/driveline_bench.cpp
        engine/engine_bench.cpp
        pull/pull_bench.cpp
        run/run_bench.cpp
        session/session_bench.cpp
        tire/tire_bench.cpp
        vehicle/vehicle_bench.cpp)
    target_link_libraries(core_bench PRIVATE tp::data)
    target_compile_definitions(core_bench PRIVATE TP_DATA_DIR="${TP_DATA_DIR}")
endif()

tp_configure_target(core_bench)
