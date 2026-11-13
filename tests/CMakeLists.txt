include(GoogleTest)

add_executable(tp_core_tests
    career/career_test.cpp
    chassis/chassis_test.cpp
    clutch/clutch_engine_test.cpp
    clutch/clutch_test.cpp
    controls/controls_test.cpp
    crew/crew_test.cpp
    driveline/driveline_chain_test.cpp
    driveline/driveline_test.cpp
    driver/driver_test.cpp
    engine/engine_determinism_test.cpp
    engine/engine_test.cpp
    event/event_test.cpp
    field/field_test.cpp
    garage/garage_choice_test.cpp
    garage/garage_test.cpp
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
    turn/turn_test.cpp
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
        data/career_loader_test.cpp
        data/career_names_test.cpp
        data/class_rules_loader_test.cpp
        data/clutch_loader_test.cpp
        data/controls_loader_test.cpp
        data/driveline_loader_test.cpp
        data/engine_loader_test.cpp
        data/event_loader_test.cpp
        data/field_spread_test.cpp
        data/files_test.cpp
        data/garage_loader_test.cpp
        data/run_setup_test.cpp
        data/season_loader_test.cpp
        data/season_save_test.cpp
        data/live_event_test.cpp
        data/live_season_test.cpp
        data/rival_difficulty_test.cpp
        data/load_downshift_runs_test.cpp
        data/shipped_tracks_test.cpp
        data/sled_loader_test.cpp
        data/syntax_error_test.cpp
        data/tire_loader_test.cpp
        data/track_loader_test.cpp
        data/tractor_loader_test.cpp
        data/track_setup_test.cpp
        data/tuning_runs_test.cpp
        data/turbo_choice_test.cpp
        data/wheelie_runs_test.cpp
        data/hard_spot_runs_test.cpp)
    target_link_libraries(tp_data_tests PRIVATE tp::data GTest::gtest_main)
    target_compile_definitions(tp_data_tests PRIVATE
        TP_DATA_DIR="${TP_DATA_DIR}")
    tp_configure_target(tp_data_tests)
    gtest_discover_tests(tp_data_tests DISCOVERY_MODE PRE_TEST)
endif()

# tp_pull's command line itself, where the tool runs on the build machine:
# --downshift shifts the standard tractor down under load near the end of a
# pull, as the scripted pullers do.
if(TARGET tp_pull AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME TpPull.RunShiftsDownUnderLoad
        COMMAND tp_pull run "${TP_DATA_DIR}/tractors/farm_stock_light.toml"
            "${TP_DATA_DIR}/sleds/light_class.toml" "${TP_DATA_DIR}/tracks/ideal_clay_sand.toml"
            --gear 5 --downshift 0.9 -o "${CMAKE_CURRENT_BINARY_DIR}/downshift_run.csv")
    # PASS_REGULAR_EXPRESSION ignores the exit code: a refused shift or a
    # failed run after the message fails it by its own words.
    set_tests_properties(TpPull.RunShiftsDownUnderLoad PROPERTIES
        PASS_REGULAR_EXPRESSION "shifted down under load to gear 4 at [78][0-9][.][0-9] m"
        FAIL_REGULAR_EXPRESSION "refused|cannot select|usage:|inconsistent")
    # --launch-hold launches as the events' drivers do: 5th gets the farm
    # tractor going on the 15 t sled. Not with --dump.
    add_test(NAME TpPull.RunLaunchHeld
        COMMAND tp_pull run "${TP_DATA_DIR}/tractors/farm_stock_light.toml"
            "${TP_DATA_DIR}/sleds/light_class.toml" "${TP_DATA_DIR}/tracks/ideal_clay_sand.toml"
            --gear 5 --launch-hold 0.8 -o "${CMAKE_CURRENT_BINARY_DIR}/launch_hold_run.csv")
    set_tests_properties(TpPull.RunLaunchHeld PROPERTIES
        PASS_REGULAR_EXPRESSION "run: [89][0-9][.][0-9]+ m"
        FAIL_REGULAR_EXPRESSION "usage:|inconsistent")
    add_test(NAME TpPull.LaunchHoldIsNotForADump
        COMMAND tp_pull run "${TP_DATA_DIR}/tractors/farm_stock_light.toml"
            "${TP_DATA_DIR}/sleds/light_class.toml" "${TP_DATA_DIR}/tracks/ideal_clay_sand.toml"
            --gear 5 --launch-hold 0.8 --dump)
    set_tests_properties(TpPull.LaunchHoldIsNotForADump PROPERTIES
        PASS_REGULAR_EXPRESSION "not with --dump or --easy")
    add_test(NAME TpPull.LaunchHoldIsNotForTheEasyLayout
        COMMAND tp_pull run "${TP_DATA_DIR}/tractors/farm_stock_light.toml"
            "${TP_DATA_DIR}/sleds/light_class.toml" "${TP_DATA_DIR}/tracks/ideal_clay_sand.toml"
            --gear 5 --launch-hold 0.8 --easy "${TP_DATA_DIR}/controls/easy.toml")
    set_tests_properties(TpPull.LaunchHoldIsNotForTheEasyLayout PROPERTIES
        PASS_REGULAR_EXPRESSION "not with --dump or --easy")
endif()
