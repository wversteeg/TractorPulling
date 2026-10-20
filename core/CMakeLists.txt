add_library(tp_core STATIC
    src/chassis/chassis.cpp
    src/clutch/clutch.cpp
    src/controls/controls.cpp
    src/driveline/driveline.cpp
    src/driver/driver.cpp
    src/engine/engine.cpp
    src/event/event.cpp
    src/field/field.cpp
    src/field/play.cpp
    src/math/curve.cpp
    src/math/exp.cpp
    src/pull/pull.cpp
    src/rules/class_rules.cpp
    src/run/run.cpp
    src/season/season.cpp
    src/sled/sled.cpp
    src/soil/soil.cpp
    src/tire/tire.cpp
    src/time/fixed_step_clock.cpp
    src/track/track.cpp
    src/vehicle/chain_solver.cpp
    src/vehicle/vehicle.cpp)
add_library(tp::core ALIAS tp_core)

target_include_directories(tp_core PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_compile_features(tp_core PUBLIC cxx_std_20)

tp_configure_target(tp_core)
target_compile_options(tp_core
    PUBLIC ${TP_DETERMINISM_FLAGS}
    PRIVATE ${TP_CORE_FLAGS})
