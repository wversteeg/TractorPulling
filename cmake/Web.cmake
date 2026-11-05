# Settings for WebAssembly builds with Emscripten (the web and godot-web
# presets). Included before the dependencies, so that the flags set here
# reach GoogleTest and Google Benchmark too.

if(TP_BUILD_GODOT)
    # The extension is a side module that Godot's web export loads into its
    # own main module (Godot's "extensions support" templates). godot-cpp
    # compiles with -sSIDE_MODULE, and every static library linked in must
    # be position independent (CMakeLists.txt sets it). CMake has no shared
    # libraries for Emscripten, and links a SHARED library into a static
    # object: this links it as a side module instead, named .wasm as Godot
    # expects, as godot-cpp's cmake/emsdkHack.cmake does for its own
    # project. Mode 2, not godot-cpp's 1: mode 1 links every object of every
    # static library in (and fails on tp_core, which CMake lists twice),
    # mode 2 only what the entry point Godot looks up reaches, which is
    # smaller. It throws nothing, so it keeps Emscripten's default of no
    # exception support, like Godot's own web build.
    set_property(GLOBAL PROPERTY TARGET_SUPPORTS_SHARED_LIBS TRUE)
    set(CMAKE_SHARED_LIBRARY_CREATE_CXX_FLAGS
        "-sSIDE_MODULE=2 -sEXPORTED_FUNCTIONS=_tractor_pulling_init")
    set(CMAKE_SHARED_LIBRARY_SUFFIX ".wasm")
    return()
endif()

# The tests, benchmarks and tp_pull run with Node, which the Emscripten
# toolchain sets as the emulator (ctest and gtest_discover_tests use it):
# - -fwasm-exceptions: tests and tools throw and catch (the core stays
#   -fno-exceptions); Emscripten otherwise aborts on any throw. Wasm
#   exceptions need setjmp/longjmp done the same way (the default is
#   Emscripten's older JavaScript-based one).
# - NODERAWFS: the files are the host's, so data/ is read where it lies and
#   tp_pull writes its CSV where it is told.
# - ALLOW_MEMORY_GROWTH: the heap grows beyond Emscripten's 16 MiB default
#   as the tests need it (the simulation step itself never allocates).
# - STACK_SIZE: Godot's web build's 5 MiB, so the code runs on the stack it
#   gets in the game; Emscripten's default of 64 KiB is too small (the
#   tests overflow it).
string(APPEND CMAKE_CXX_FLAGS " -fwasm-exceptions -sSUPPORT_LONGJMP=wasm")
string(APPEND CMAKE_EXE_LINKER_FLAGS
    " -fwasm-exceptions -sSUPPORT_LONGJMP=wasm -sNODERAWFS=1 -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=1"
    " -sSTACK_SIZE=5MB")
