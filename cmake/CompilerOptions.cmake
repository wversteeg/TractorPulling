# Warning, determinism and sanitizer settings, applied per target so that
# fetched dependencies keep their own flags.

if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang|AppleClang)$")
    message(FATAL_ERROR "Unsupported compiler '${CMAKE_CXX_COMPILER_ID}': use GCC or Clang")
endif()

set(TP_WARNING_FLAGS
    -Wall
    -Wextra
    -Wpedantic
    -Wconversion
    -Wsign-conversion
    -Wshadow
    -Wold-style-cast
    -Wnon-virtual-dtor
    -Woverloaded-virtual
    -Wcast-align
    -Wcast-qual
    -Wdouble-promotion
    -Wnull-dereference
    -Wimplicit-fallthrough
    -Wformat=2
    -Wundef)

if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    list(APPEND TP_WARNING_FLAGS
        -Wduplicated-cond
        -Wlogical-op
        -Wuseless-cast)
    # GCC's -Wduplicated-branches compares both arms of every conditional and
    # blows up on sanitizer-instrumented code: a long stream expression in
    # tools/tp_pull.cpp took over 10 minutes to compile. Non-sanitizer builds
    # (and so CI's gcc jobs) still check it.
    if(NOT TP_ENABLE_SANITIZERS)
        list(APPEND TP_WARNING_FLAGS -Wduplicated-branches)
    endif()
endif()

if(TP_WARNINGS_AS_ERRORS)
    list(APPEND TP_WARNING_FLAGS -Werror)
endif()

if(TP_ENABLE_SANITIZERS)
    set(TP_SANITIZER_FLAGS
        -fsanitize=address,undefined
        -fno-sanitize-recover=all
        -fno-omit-frame-pointer)
endif()

# Common settings for every first-party target.
function(tp_configure_target target)
    target_compile_options(${target} PRIVATE ${TP_WARNING_FLAGS})
    if(TP_ENABLE_SANITIZERS)
        target_compile_options(${target} PRIVATE ${TP_SANITIZER_FLAGS})
        target_link_options(${target} PRIVATE ${TP_SANITIZER_FLAGS})
    endif()
endfunction()

# Flags that keep floating-point results bit-identical across compilers and
# architectures. They are PUBLIC on the core library so inline code in its
# headers is compiled the same way in every consumer.
#
# -ffp-contract=off: GCC (and Clang on some targets) would otherwise fuse
#   a*b+c into FMA where the hardware has it, e.g. on ARM64 but not baseline
#   x86-64, which changes rounding.
# No -ffast-math or -Ofast anywhere. x86-64 uses SSE2, never x87.
set(TP_DETERMINISM_FLAGS -ffp-contract=off -fno-fast-math)

# The simulation core is built without exceptions and RTTI.
set(TP_CORE_FLAGS -fno-exceptions -fno-rtti)
