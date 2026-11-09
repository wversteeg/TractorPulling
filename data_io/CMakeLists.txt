# Loads data/ definitions into core parameter structs. Kept out of core: it
# parses files, allocates and reports errors as strings.
add_library(tp_data STATIC
    src/career/career_loader.cpp
    src/clutch/clutch_loader.cpp
    src/controls/controls_loader.cpp
    src/common/toml_reader.cpp
    src/driveline/driveline_loader.cpp
    src/engine/engine_loader.cpp
    src/event/event_loader.cpp
    src/files/files.cpp
    src/garage/garage_loader.cpp
    src/rules/class_rules_loader.cpp
    src/track/track_loader.cpp
    src/season/season_loader.cpp
    src/season/season_save.cpp
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
# toml++ without exceptions: a syntax error is a parse_result, not a throw.
# Godot's web export is built without exception support, so a throw there
# would end the game instead of reporting the file (detail::parse_then).
target_compile_definitions(tp_data PRIVATE TOML_EXCEPTIONS=0)
tp_configure_target(tp_data)
