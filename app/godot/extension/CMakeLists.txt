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
#
# The extension is also listed in Godot's .godot/extension_list.cfg, so
# Godot loads it at startup. Otherwise the editor finds it while scanning
# the project and loads it then, and Godot 4.7.2 then crashes as it quits
# (any registered class; an import run, headless or not). Godot keeps the
# file up to date itself afterwards.
set(TP_GODOT_EXTENSION_LIST "${CMAKE_CURRENT_BINARY_DIR}/extension_list.cfg")
file(WRITE "${TP_GODOT_EXTENSION_LIST}" "res://tractor_pulling.gdextension\n")
# The copy starts afresh, so a file removed or renamed in data/ leaves
# nothing stale behind for res:// to load.
add_custom_target(tp_godot_data ALL
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${TP_GODOT_PROJECT_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${PROJECT_SOURCE_DIR}/data"
            "${TP_GODOT_PROJECT_DIR}/data"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${TP_GODOT_PROJECT_DIR}/.godot"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${TP_GODOT_EXTENSION_LIST}"
            "${TP_GODOT_PROJECT_DIR}/.godot/extension_list.cfg"
    COMMENT "Copying data/ into the Godot project and listing the extension")
add_dependencies(tp_godot tp_godot_data)
