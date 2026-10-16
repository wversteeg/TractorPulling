# Loads data/ definitions into core parameter structs. Kept out of core: it
# parses files, allocates and reports errors as strings.
add_library(tp_data STATIC
    src/engine/engine_loader.cpp)
add_library(tp::data ALIAS tp_data)

target_include_directories(tp_data PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(tp_data
    PUBLIC tp::core
    PRIVATE tomlplusplus::tomlplusplus)
tp_configure_target(tp_data)
