# Loads data/ definitions into core parameter structs. Kept out of core: it
# parses files, allocates and reports errors as strings.
add_library(tp_data STATIC
    src/clutch/clutch_loader.cpp
    src/controls/controls_loader.cpp
    src/common/toml_reader.cpp
    src/driveline/driveline_loader.cpp
    src/engine/engine_loader.cpp
    src/event/event_loader.cpp
    src/files/files.cpp
    src/rules/class_rules_loader.cpp
    src/track/track_loader.cpp
    src/season/season_loader.cpp
    src/session/run_setup.cpp
    src/sled/sled_loader.cpp
    src/tire/tire_fields.cpp
    src/tire/tire_loader.cpp
    src/vehicle/tractor_loader.cpp)
add_library(tp::data ALIAS tp_data)

target_include_directories(tp_data PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(tp_data
    PUBLIC tp::core
    PRIVATE tomlplusplus::tomlplusplus)
tp_configure_target(tp_data)
