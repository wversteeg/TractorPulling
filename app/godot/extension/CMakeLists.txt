# The GDExtension: tp::session and the data loaders behind Godot classes,
# built as a shared library into the Godot project's bin/ where
# tractor_pulling.gdextension names it, with the shipped data/ copied next to
# it as res://data.

set(TP_GODOT_PROJECT_DIR "${PROJECT_SOURCE_DIR}/app/godot")

add_library(tp_godot SHARED
    src/godot_files.cpp
    src/register_types.cpp
    src/tractor_pull_run.cpp)
target_link_libraries(tp_godot PRIVATE tp::core tp::data godot::cpp)
tp_configure_target(tp_godot)

# godot-cpp's suffix: platform, build target and architecture, e.g.
# libtp_godot.linux.template_debug.x86_64.so, as the .gdextension file lists.
get_target_property(TP_GODOT_SUFFIX godot::cpp GODOTCPP_SUFFIX)
set_target_properties(tp_godot PROPERTIES
    PREFIX "lib"
    OUTPUT_NAME "tp_godot${TP_GODOT_SUFFIX}"
    CXX_VISIBILITY_PRESET hidden
    LIBRARY_OUTPUT_DIRECTORY "${TP_GODOT_PROJECT_DIR}/bin")

# The game reads the same data/ files as the tools, as res://data.
add_custom_target(tp_godot_data ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/data"
            "${TP_GODOT_PROJECT_DIR}/data"
    COMMENT "Copying data/ into the Godot project")
add_dependencies(tp_godot tp_godot_data)
