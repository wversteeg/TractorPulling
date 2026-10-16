include(GoogleTest)

add_executable(tp_core_tests
    engine/engine_test.cpp
    math/curve_test.cpp
    time/fixed_step_clock_test.cpp
    toml_smoke_test.cpp)
target_link_libraries(tp_core_tests PRIVATE
    tp::core
    GTest::gtest_main
    tomlplusplus::tomlplusplus)
tp_configure_target(tp_core_tests)

gtest_discover_tests(tp_core_tests DISCOVERY_MODE PRE_TEST)
