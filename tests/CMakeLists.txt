include(GoogleTest)

add_executable(tp_core_tests
    chassis/chassis_test.cpp
    clutch/clutch_engine_test.cpp
    clutch/clutch_test.cpp
    driveline/driveline_chain_test.cpp
    driveline/driveline_test.cpp
    engine/engine_determinism_test.cpp
    engine/engine_test.cpp
    event/event_test.cpp
    hash/fnv1a_test.cpp
    math/curve_test.cpp
    math/exp_test.cpp
    math/random_test.cpp
    math/trig_test.cpp
    pull/pull_test.cpp
    rules/class_rules_test.cpp
    run/run_test.cpp
    sled/sled_test.cpp
    soil/soil_test.cpp
    tire/tire_test.cpp
    time/fixed_step_clock_test.cpp
    track/track_test.cpp
    units/conversions_test.cpp
    vehicle/chain_solver_test.cpp
    vehicle/vehicle_test.cpp)
target_link_libraries(tp_core_tests PRIVATE tp::core GTest::gtest_main)
tp_configure_target(tp_core_tests)
gtest_discover_tests(tp_core_tests DISCOVERY_MODE PRE_TEST)

if(TP_BUILD_DATA_IO)
    add_executable(tp_data_tests
        data/class_rules_loader_test.cpp
        data/clutch_loader_test.cpp
        data/driveline_loader_test.cpp
        data/engine_loader_test.cpp
        data/shipped_tracks_test.cpp
        data/sled_loader_test.cpp
        data/tire_loader_test.cpp
        data/track_loader_test.cpp
        data/tractor_loader_test.cpp)
    target_link_libraries(tp_data_tests PRIVATE tp::data GTest::gtest_main)
    target_compile_definitions(tp_data_tests PRIVATE
        TP_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
    tp_configure_target(tp_data_tests)
    gtest_discover_tests(tp_data_tests DISCOVERY_MODE PRE_TEST)
endif()
