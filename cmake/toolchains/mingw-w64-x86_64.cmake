# Windows (x86-64) from Linux with MinGW-w64, as Godot builds its own
# Windows releases: the Godot extension for the Windows desktop export.
# Needs the POSIX-threads compilers (Debian/Ubuntu:
# g++-mingw-w64-x86-64-posix), which have std::thread and friends.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(TP_MINGW_PREFIX x86_64-w64-mingw32)
set(CMAKE_C_COMPILER ${TP_MINGW_PREFIX}-gcc-posix)
set(CMAKE_CXX_COMPILER ${TP_MINGW_PREFIX}-g++-posix)
set(CMAKE_RC_COMPILER ${TP_MINGW_PREFIX}-windres)

set(CMAKE_FIND_ROOT_PATH /usr/${TP_MINGW_PREFIX})
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)
