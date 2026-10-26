include(GoogleTest)

add_executable(tp_core_tests
    chassis/chassis_test.cpp
    clutch/clutch_engine_test.cpp
    clutch/clutch_test.cpp
    controls/controls_test.cpp
    driveline/driveline_chain_test.cpp
    driveline/driveline_test.cpp
    driver/driver_test.cpp
    engine/engine_determinism_test.cpp
    engine/engine_test.cpp
    event/event_test.cpp
    field/field_test.cpp
    hash/fnv1a_test.cpp
    math/curve_test.cpp
    math/exp_test.cpp
    math/random_test.cpp
    math/trig_test.cpp
    pull/pull_test.cpp
    rules/class_rules_test.cpp
    run/run_test.cpp
    season/season_test.cpp
    session/session_test.cpp
    sled/sled_test.cpp
    soil/soil_test.cpp
    tire/tire_test.cpp
    time/fixed_step_clock_test.cpp
    tools/parse_number_test.cpp
    track/track_test.cpp
    units/conversions_test.cpp
    vehicle/chain_solver_test.cpp
    vehicle/tuning_test.cpp
    vehicle/vehicle_test.cpp)
target_link_libraries(tp_core_tests PRIVATE tp::core GTest::gtest_main)
# The tool's command-line parsing is header-only and tested here.
target_include_directories(tp_core_tests PRIVATE ${PROJECT_SOURCE_DIR}/tools)
tp_configure_target(tp_core_tests)
gtest_discover_tests(tp_core_tests DISCOVERY_MODE PRE_TEST)

# Its own program: it replaces the global operator new to count allocations.
add_executable(tp_alloc_tests alloc/no_allocation_test.cpp)
target_link_libraries(tp_alloc_tests PRIVATE tp::core GTest::gtest_main)
tp_configure_target(tp_alloc_tests)
gtest_discover_tests(tp_alloc_tests DISCOVERY_MODE PRE_TEST)

if(TP_BUILD_DATA_IO)
    add_executable(tp_data_tests
        data/class_rules_loader_test.cpp
        data/clutch_loader_test.cpp
        data/controls_loader_test.cpp
        data/driveline_loader_test.cpp
        data/engine_loader_test.cpp
        data/event_loader_test.cpp
        data/files_test.cpp
        data/run_setup_test.cpp
        data/season_loader_test.cpp
        data/season_save_test.cpp
        data/live_event_test.cpp
        data/live_season_test.cpp
        data/load_downshift_runs_test.cpp
        data/shipped_tracks_test.cpp
        data/sled_loader_test.cpp
        data/tire_loader_test.cpp
        data/track_loader_test.cpp
        data/tractor_loader_test.cpp
        data/tuning_runs_test.cpp)
    target_link_libraries(tp_data_tests PRIVATE tp::data GTest::gtest_main)
    target_compile_definitions(tp_data_tests PRIVATE
        TP_DATA_DIR="${TP_DATA_DIR}")
    tp_configure_target(tp_data_tests)
    gtest_discover_tests(tp_data_tests DISCOVERY_MODE PRE_TEST)
endif()
