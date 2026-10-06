cmake_minimum_required(VERSION 3.21)

# The only place the version is defined, passed to the code as "FIX_VERSION_STRING" (see "FIX_VERSION"
# in "src/core/constants.hpp") and checked against the tag by the release workflow
project(BGEWidescreenFix VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Emit build/compile_commands.json, which is what clangd reads to resolve includes and flags
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Static CRT everywhere (dependencies included), the ".asi" is dropped into the game folder and
# must not depend on a specific MSVC runtime being installed
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

# Enable LTO/IPO for "Release" builds so the optimizer can inline across translation
# unit boundaries, scoped per-target (not global) so it doesn't propagate to dependencies
cmake_policy(SET CMP0069 NEW)
include(CheckIPOSupported)
check_ipo_supported(RESULT _ipo_ok OUTPUT _ipo_err LANGUAGES CXX)
if(NOT _ipo_ok)
    message(WARNING "IPO/LTO not supported by toolchain: ${_ipo_err}")
endif()

# Compiler flags and target helpers
include(cmake/compilerFlags.cmake)
include(cmake/runtimeDlls.cmake)

# Dependencies
include(cmake/dependencies.cmake)

# Core library (all sources except dllmain.cpp, shared by the ASI and tests)
# Note: Uses "CONFIGURE_DEPENDS" to automatically re-run CMake when new files are added
file(GLOB_RECURSE LIB_SOURCES CONFIGURE_DEPENDS src/*.cpp)
list(FILTER LIB_SOURCES EXCLUDE REGEX ".*/dllmain\\.cpp$")

# Create the library and apply compiler flags
add_library(BGEWidescreenFixLib STATIC ${LIB_SOURCES})
apply_compiler_flags(BGEWidescreenFixLib)
if(_ipo_ok)
    set_target_properties(BGEWidescreenFixLib PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# Include directories
target_include_directories(BGEWidescreenFixLib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Trimmed Windows headers, without the "min"/"max" macros that break "std::min"/"std::max",
# and the project version for "FIX_VERSION"
target_compile_definitions(BGEWidescreenFixLib PUBLIC
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    FIX_VERSION_STRING="${PROJECT_VERSION}"
)

# Link libraries (FetchContent targets from "cmake/dependencies.cmake")
target_link_libraries(BGEWidescreenFixLib PUBLIC safetyhook::safetyhook)

# The ASI itself (a plain DLL renamed to ".asi", loaded by Ultimate ASI Loader)
add_library(BGEWidescreenFix SHARED src/dllmain.cpp)
apply_compiler_flags(BGEWidescreenFix)
target_link_libraries(BGEWidescreenFix PRIVATE BGEWidescreenFixLib)
set_target_properties(BGEWidescreenFix PROPERTIES PREFIX "" SUFFIX ".asi")
if(_ipo_ok)
    set_target_properties(BGEWidescreenFix PROPERTIES INTERPROCEDURAL_OPTIMIZATION_RELEASE TRUE)
endif()

# Ship the default configuration next to the built ASI, copied at configure time (CMake re-runs
# automatically when the file changes) rather than through a post-build command, post-build commands
# go through "cmd.exe /C" with nested quotes, which breaks on paths containing "&"
configure_file(BGEWidescreenFix.ini ${CMAKE_CURRENT_BINARY_DIR}/BGEWidescreenFix.ini COPYONLY)

# Install the ASI and its configuration, ready to be copied into the game folder
install(TARGETS BGEWidescreenFix RUNTIME DESTINATION .)
install(FILES BGEWidescreenFix.ini DESTINATION .)

# Tests
enable_testing()

file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS tests/*.cpp)

add_executable(BGEWidescreenFixTests ${TEST_SOURCES})
apply_compiler_flags(BGEWidescreenFixTests)
target_link_libraries(BGEWidescreenFixTests PRIVATE BGEWidescreenFixLib Catch2::Catch2WithMain)

# Must happen before "catch_discover_tests" so DLLs are present when the exe is run at build time
copy_runtime_dlls(BGEWidescreenFixTests)

# Discover tests when "ctest" runs instead of right after linking, avoiding one more post-build command
catch_discover_tests(BGEWidescreenFixTests DISCOVERY_MODE PRE_TEST)
