include(GoogleTest)

add_executable(tp_core_tests
    engine/engine_determinism_test.cpp
    engine/engine_test.cpp
    hash/fnv1a_test.cpp
    math/curve_test.cpp
    time/fixed_step_clock_test.cpp)
target_link_libraries(tp_core_tests PRIVATE tp::core GTest::gtest_main)
tp_configure_target(tp_core_tests)
gtest_discover_tests(tp_core_tests DISCOVERY_MODE PRE_TEST)

if(TP_BUILD_DATA_IO)
    add_executable(tp_data_tests
        data/engine_loader_test.cpp)
    target_link_libraries(tp_data_tests PRIVATE tp::data GTest::gtest_main)
    target_compile_definitions(tp_data_tests PRIVATE
        TP_DATA_DIR="${PROJECT_SOURCE_DIR}/data")
    tp_configure_target(tp_data_tests)
    gtest_discover_tests(tp_data_tests DISCOVERY_MODE PRE_TEST)
endif()
