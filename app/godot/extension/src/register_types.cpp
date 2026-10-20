// The extension's entry point: Godot calls tractor_pulling_init (named in
// tractor_pulling.gdextension) when it loads the library, and the classes are
// registered with the scene level.

#include "tractor_pull_run.hpp"
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/godot.hpp>

namespace {

void initialize(godot::ModuleInitializationLevel level) {
    if (level != godot::MODULE_INITIALIZATION_LEVEL_SCENE) {
        return;
    }
    GDREGISTER_CLASS(tp::godot_binding::TractorPullRun);
}

void uninitialize(godot::ModuleInitializationLevel /*level*/) {}

}  // namespace

extern "C" {

GDExtensionBool GDE_EXPORT tractor_pulling_init(GDExtensionInterfaceGetProcAddress get_proc_address,
                                                GDExtensionClassLibraryPtr library,
                                                GDExtensionInitialization* initialization) {
    const godot::GDExtensionBinding::InitObject init{get_proc_address, library, initialization};
    init.register_initializer(initialize);
    init.register_terminator(uninitialize);
    init.set_minimum_library_initialization_level(godot::MODULE_INITIALIZATION_LEVEL_SCENE);
    return init.init();
}

}  // extern "C"
