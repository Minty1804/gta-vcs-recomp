cmake_minimum_required(VERSION 3.21)
project(vcs_recomp LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Generated C++ from the user's own SLUS_215.90 (tools/recompile.py). Never committed.
set(VCS_GENERATED_DIR "${CMAKE_SOURCE_DIR}/generated/ee" CACHE PATH "Recompiled EE code")

set(PS2X_LTO "THIN" CACHE STRING "Link-time optimisation for the game: THIN (default, the game to play), OFF (quick dev builds), FULL")
set(PS2X_PGO "" CACHE STRING "Profile-guided optimisation: empty (off), GEN (instrumented), or a .profdata path (tools/build.py --opt)")
set(PS2X_CPU "AVX2" CACHE STRING "The CPU the build runs on: AVX2 (Haswell / Zen and newer, the default), AVX (Sandy / Ivy Bridge, Bulldozer) or SSE4 (no AVX); tools/build.py picks it from this PC")
set_property(CACHE PS2X_CPU PROPERTY STRINGS AVX2 AVX SSE4)
option(VCS_BUILD_TOOLS "Build the PS2Recomp recompiler/analyzer" ON)

set(PS2X_BUILD_RECOMP ${VCS_BUILD_TOOLS} CACHE BOOL "" FORCE)
set(PS2X_BUILD_ANALYZER ${VCS_BUILD_TOOLS} CACHE BOOL "" FORCE)
set(PS2X_BUILD_TEST OFF CACHE BOOL "" FORCE)
set(PS2X_BUILD_STUDIO OFF CACHE BOOL "" FORCE)

# Linux (docs/LINUX.md) and Windows' llvm-mingw (DECISIONS.md 2026-11-03): clang 19+ only - the same compiler as
# clang-cl, and PS2_GUEST_CC needs its preserve_none. Linked with lld (ThinLTO).
# macOS (docs/MACOS.md): Apple's clang (AppleClang 17+ is LLVM 19) and Apple's linker (ld64; Xcode has no lld).
if(APPLE)
    enable_language(OBJC OBJCXX) # the Metal backend (src/rhi/metal)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 17)
        message(FATAL_ERROR "The game needs Apple clang 17 or newer (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}): "
                            "the Xcode Command Line Tools (xcode-select --install); python3 tools/build.py picks it up")
    endif()
    include(CheckCXXSourceCompiles)
    set(CMAKE_REQUIRED_FLAGS "-Werror")
    check_cxx_source_compiles("__attribute__((preserve_none)) void f(); int main() { return 0; }" VCS_HAVE_PRESERVE_NONE)
    unset(CMAKE_REQUIRED_FLAGS)
    if(NOT VCS_HAVE_PRESERVE_NONE)
        message(FATAL_ERROR "This clang has no preserve_none calling convention (PS2_GUEST_CC): update the Command Line Tools")
    endif()
elseif(NOT MSVC)
    if(NOT CMAKE_CXX_COMPILER_ID STREQUAL "Clang" OR CMAKE_CXX_COMPILER_VERSION VERSION_LESS 19)
        message(FATAL_ERROR "The game needs clang 19 or newer (found ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}). "
                            "python tools/build.py picks it up (docs/LINUX.md; Windows: llvm-mingw, downloaded)")
    endif()
    add_link_options(-fuse-ld=lld)
endif()
# llvm-mingw: mingw-w64's headers default to Windows 7, the Windows SDK's to Windows 10 - the same APIs as clang-cl's
# build. Every program is linked -static: libc++, libunwind and winpthread inside, only the system's UCRT left.
if(MINGW)
    add_compile_definitions(_WIN32_WINNT=0x0A00 WINVER=0x0A00)
    add_link_options(-static)
endif()
# Linux and macOS: compile and ThinLTO jobs follow the memory - a unity batch of generated code takes up to ~2 GB to compile.
if(NOT WIN32)
    cmake_host_system_information(RESULT _vcs_mem_mb QUERY TOTAL_PHYSICAL_MEMORY)
    cmake_host_system_information(RESULT _vcs_cores QUERY NUMBER_OF_LOGICAL_CORES)
    math(EXPR _vcs_jobs "${_vcs_mem_mb} / 2048")
    if(_vcs_jobs LESS 1)
        set(_vcs_jobs 1)
    endif()
    if(_vcs_jobs GREATER _vcs_cores)
        set(_vcs_jobs ${_vcs_cores})
    endif()
    set(VCS_COMPILE_JOBS ${_vcs_jobs} CACHE STRING "Parallel compile jobs (default: one per 2 GB of memory, at most one per core)")
    set(PS2X_LTO_JOBS ${_vcs_jobs} CACHE STRING "Parallel ThinLTO backend jobs at the game's link")
    set_property(GLOBAL APPEND PROPERTY JOB_POOLS vcs_compile=${VCS_COMPILE_JOBS})
    set(CMAKE_JOB_POOL_COMPILE vcs_compile)
    message(STATUS "${CMAKE_SYSTEM_NAME} build: ${VCS_COMPILE_JOBS} compile jobs, ${PS2X_LTO_JOBS} ThinLTO jobs (${_vcs_mem_mb} MB memory)")
endif()

set(VCS_HAVE_GENERATED OFF)
if(EXISTS "${VCS_GENERATED_DIR}/register_functions.cpp")
    set(VCS_HAVE_GENERATED ON)
endif()
set(PS2X_BUILD_RUNTIME ${VCS_HAVE_GENERATED} CACHE BOOL "" FORCE)

file(GLOB_RECURSE VCS_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
)
list(REMOVE_ITEM VCS_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
# The platform layer, RHI and renderer are their own libraries (src/*/CMakeLists.txt), linked into the runtime.
list(FILTER VCS_SOURCES EXCLUDE REGEX "/src/(platform|rhi|render)/")
# VU1 microprograms compiled to C++ by tools/vu1_recomp.py (from the user's ELF; see game/vcs/vu1_uploads.txt)
if(EXISTS "${CMAKE_SOURCE_DIR}/generated/vu1/vu1_compiled.cpp")
    list(APPEND VCS_SOURCES "${CMAKE_SOURCE_DIR}/generated/vu1/vu1_compiled.cpp")
endif()

# Kenney's Input Prompts (assets/prompts/kenney, CC0) for the button prompts and Oswald (assets/fonts/oswald, OFL) for
# the subtitles, embedded in the game: made again when an icon, the font or the tool changes (tools/kenney_prompts.py);
# without Python an empty table (the prompts' classic drawing, the game's own letters). From willzmkv's fork.
if(VCS_HAVE_GENERATED)
    find_package(Python3 COMPONENTS Interpreter) # (tools/build.py passes its own)
    set(_vcs_prompt_icons_cpp "${CMAKE_BINARY_DIR}/generated_assets/prompt_icons.cpp")
    file(GLOB _vcs_prompt_icons CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/assets/prompts/kenney/*.png"
                                                    "${CMAKE_SOURCE_DIR}/assets/fonts/oswald/*.ttf")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/tools/kenney_prompts.py" ${_vcs_prompt_icons})
    if(Python3_FOUND)
        execute_process(COMMAND "${Python3_EXECUTABLE}" -I "${CMAKE_SOURCE_DIR}/tools/kenney_prompts.py" embed "${_vcs_prompt_icons_cpp}"
                        OUTPUT_VARIABLE _vcs_icons_out ERROR_VARIABLE _vcs_icons_out)
        message(STATUS "${_vcs_icons_out}")
    endif()
    if(NOT EXISTS "${_vcs_prompt_icons_cpp}")
        file(WRITE "${_vcs_prompt_icons_cpp}" "#include <cstddef>\nnamespace vcs::glyphart\n{\n    struct EmbeddedIcon\n    {\n        const char *name;\n        const unsigned char *data;\n        size_t size;\n    };\n    extern const EmbeddedIcon kPromptIcons[] = {{nullptr, nullptr, 0}};\n    extern const size_t kPromptIconCount = 0;\n    extern const unsigned char *const kSubtitleFont = nullptr;\n    extern const size_t kSubtitleFontSize = 0;\n}\n")
    endif()
    list(APPEND VCS_SOURCES "${_vcs_prompt_icons_cpp}")
endif()

set(PS2X_RUNNER_SOURCE_DIR "${VCS_GENERATED_DIR}" CACHE PATH "" FORCE)
set(PS2X_RUNNER_EXTRA_SOURCES "${VCS_SOURCES}" CACHE STRING "" FORCE)
set(PS2X_RUNNER_MAIN "${CMAKE_SOURCE_DIR}/src/main.cpp" CACHE FILEPATH "" FORCE)

set(PS2X_PARALLEL_GS_DIR "${CMAKE_SOURCE_DIR}/third_party/parallel-gs" CACHE PATH "" FORCE)
if(VCS_HAVE_GENERATED)
    add_subdirectory(src/platform)
    add_subdirectory(src/rhi)
    add_subdirectory(src/render)
    add_library(vcs_host INTERFACE)
    target_link_libraries(vcs_host INTERFACE vcs_platform vcs_render)
    set(PS2X_HOST_PLATFORM_TARGET vcs_host CACHE STRING "" FORCE)
endif()
add_subdirectory(third_party/PS2Recomp)

if(VCS_HAVE_GENERATED)
    set_target_properties(ps2EntryRunner PROPERTIES OUTPUT_NAME "vcs")
    # The main build tree (build/release) puts vcs.exe, its DLLs and vcs.ini at the top of build/ instead of
    # build/release/third_party/PS2Recomp/ps2xRuntime; other trees (PGO training's build/pgo-gen) keep theirs.
    # VCS_GAME_DIR: the build folder chosen elsewhere (tools/toolchain.py passes it for the main tree; docs/LAUNCHER.md).
    set(VCS_GAME_DIR "" CACHE PATH "Where vcs.exe, its DLLs and vcs.ini go (empty: build/ for build/release)")
    string(TOLOWER "${CMAKE_BINARY_DIR}" _vcs_binary_dir)
    string(TOLOWER "${CMAKE_SOURCE_DIR}/build/release" _vcs_main_tree)
    if(VCS_GAME_DIR)
        set_target_properties(ps2EntryRunner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${VCS_GAME_DIR}")
    elseif(_vcs_binary_dir STREQUAL _vcs_main_tree)
        set_target_properties(ps2EntryRunner PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/build")
    endif()
    target_include_directories(ps2EntryRunner PRIVATE "${CMAKE_SOURCE_DIR}/src" "${VCS_XXHASH_DIR}"
                               "${CMAKE_SOURCE_DIR}/third_party/bcdec") # texture packs' DDS (BC1-BC7)
    target_link_libraries(ps2EntryRunner PRIVATE vcs_host) # main.cpp: settings menu (Dear ImGui), presenter aspect
    # Windows: our manifest in place of the linker's, for the UTF-8 code page (non-ASCII folders; src/vcs.manifest)
    # ... and the exe's icon and version information (src/vcs.rc): VERSION in the repository root, as a header only
    # the resource file sees - a compile definition would rebuild every generated file at each new version
    if(WIN32)
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_SOURCE_DIR}/VERSION")
        file(STRINGS "${CMAKE_SOURCE_DIR}/VERSION" VCS_VERSION LIMIT_COUNT 1)
        string(REGEX MATCH "^([0-9]+)\\.([0-9]+)\\.([0-9]+)" _vcs_version_numbers "${VCS_VERSION}")
        if(NOT _vcs_version_numbers)
            message(FATAL_ERROR "VERSION must start with major.minor.patch (it holds '${VCS_VERSION}')")
        endif()
        file(CONFIGURE OUTPUT "${CMAKE_BINARY_DIR}/vcs_version/vcs_version.h" CONTENT "#define VCS_VERSION_MAJOR ${CMAKE_MATCH_1}
#define VCS_VERSION_MINOR ${CMAKE_MATCH_2}
#define VCS_VERSION_PATCH ${CMAKE_MATCH_3}
#define VCS_VERSION_TEXT \"${VCS_VERSION}\"
")
        target_sources(ps2EntryRunner PRIVATE "${CMAKE_SOURCE_DIR}/src/vcs.rc")
        # the resource compiler doesn't report the files it embeds
        set_source_files_properties("${CMAKE_SOURCE_DIR}/src/vcs.rc" TARGET_DIRECTORY ps2EntryRunner
            PROPERTIES OBJECT_DEPENDS "${CMAKE_SOURCE_DIR}/src/vcs.manifest;${CMAKE_SOURCE_DIR}/src/vcs.ico;${CMAKE_BINARY_DIR}/vcs_version/vcs_version.h"
                       INCLUDE_DIRECTORIES "${CMAKE_BINARY_DIR}/vcs_version")
        if(MSVC)
            target_link_options(ps2EntryRunner PRIVATE /MANIFEST:NO)  # lld in mingw mode makes none of its own
        endif()
    endif()
    # Data files the game's options read next to vcs.exe, never committed: Project2DFX's light table for this game's PS2
    # version and its licence (ThirteenAG/WidescreenFixesPack, MIT, at a pinned commit; Enhancements > World > Distant
    # lights, src/game/vcs_lod_lights.cpp). Each: "<path next to vcs.exe>|<url>|<sha256>". Taken from deps/ when there
    # (tools/deps.py fetch: offline builds, releases), else downloaded once into the build tree; a failed download only
    # leaves the option without its table (it says so in the log and stays off).
    set(VCS_DATA_FILES
        "project2dfx/lodl.c|https://raw.githubusercontent.com/ThirteenAG/WidescreenFixesPack/37057d3883bd607ae047c9f031fe083c32b21c88/source/GTAVCS.PCSX2F.Project2DFX/lodl.c|6ee85ca581848fe060f2d89bac05563011c7b452876d74c52c055252d19549db"
        "project2dfx/LICENSE|https://raw.githubusercontent.com/ThirteenAG/WidescreenFixesPack/37057d3883bd607ae047c9f031fe083c32b21c88/license|389eef0fb81f8b3bc7aca2025da43cf007acfb390a70f49439d269e51de0b8dc")
    get_target_property(_vcs_game_dir ps2EntryRunner RUNTIME_OUTPUT_DIRECTORY)
    if(NOT _vcs_game_dir)
        set(_vcs_game_dir "${CMAKE_BINARY_DIR}/third_party/PS2Recomp/ps2xRuntime")
    endif()
    foreach(_vcs_data IN LISTS VCS_DATA_FILES)
        string(REPLACE "|" ";" _vcs_data "${_vcs_data}")
        list(GET _vcs_data 0 _vcs_data_path)
        list(GET _vcs_data 1 _vcs_data_url)
        list(GET _vcs_data 2 _vcs_data_hash)
        set(_vcs_data_file "${CMAKE_BINARY_DIR}/vcs_data/${_vcs_data_path}")
        set(_vcs_data_have "")
        if(EXISTS "${_vcs_data_file}")
            file(SHA256 "${_vcs_data_file}" _vcs_data_have)
        endif()
        if(NOT _vcs_data_have STREQUAL _vcs_data_hash)
            if(EXISTS "${CMAKE_SOURCE_DIR}/deps/${_vcs_data_path}")
                file(SHA256 "${CMAKE_SOURCE_DIR}/deps/${_vcs_data_path}" _vcs_data_have)
                if(_vcs_data_have STREQUAL _vcs_data_hash)
                    configure_file("${CMAKE_SOURCE_DIR}/deps/${_vcs_data_path}" "${_vcs_data_file}" COPYONLY)
                endif()
            endif()
        endif()
        # (also with FETCHCONTENT_FULLY_DISCONNECTED: a deps/ fetched before these files existed has none)
        if(NOT _vcs_data_have STREQUAL _vcs_data_hash)
            file(DOWNLOAD "${_vcs_data_url}" "${_vcs_data_file}" EXPECTED_HASH SHA256=${_vcs_data_hash} STATUS _vcs_data_status
                 TIMEOUT 20)
            list(GET _vcs_data_status 0 _vcs_data_code)
            if(_vcs_data_code EQUAL 0)
                set(_vcs_data_have "${_vcs_data_hash}")
            else()
                file(REMOVE "${_vcs_data_file}")
                message(WARNING "${_vcs_data_path}: download failed (${_vcs_data_status}); the option that reads it stays off")
            endif()
        endif()
        if(_vcs_data_have STREQUAL _vcs_data_hash)
            get_filename_component(_vcs_data_dir "${_vcs_data_path}" DIRECTORY)
            list(APPEND _vcs_data_copies
                 COMMAND "${CMAKE_COMMAND}" -E make_directory "${_vcs_game_dir}/${_vcs_data_dir}"
                 COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${_vcs_data_file}" "${_vcs_game_dir}/${_vcs_data_path}")
        endif()
    endforeach()
    if(_vcs_data_copies) # built with the game (a target of this folder: ps2EntryRunner is made in third_party/PS2Recomp)
        add_custom_target(vcs_data ${_vcs_data_copies} VERBATIM)
        add_dependencies(ps2EntryRunner vcs_data)
    endif()
    # Our own sources aren't generated code; keep them out of the unity batches.
    set_source_files_properties(${VCS_SOURCES} "${CMAKE_SOURCE_DIR}/src/main.cpp"
        TARGET_DIRECTORY ps2EntryRunner
        PROPERTIES SKIP_UNITY_BUILD_INCLUSION TRUE SKIP_PRECOMPILE_HEADERS TRUE)
else()
    message(STATUS "No generated code in ${VCS_GENERATED_DIR}: building tools only. Run python tools/build.py --recompile.")
endif()

# Debug symbols for the runtime and our code (not the 240 MB of generated code),
# so the sampling profiler and debuggers can name host functions.
option(VCS_RUNTIME_DEBUG_INFO "Emit CodeView debug info for runtime + launcher" ON)
if(VCS_RUNTIME_DEBUG_INFO AND MSVC)
    foreach(t ps2_runtime ps2_iop)
        if(TARGET ${t})
            target_compile_options(${t} PRIVATE /Z7)
        endif()
    endforeach()
    if(VCS_HAVE_GENERATED)
        set_source_files_properties(${VCS_SOURCES} "${CMAKE_SOURCE_DIR}/src/main.cpp"
            TARGET_DIRECTORY ps2EntryRunner PROPERTIES COMPILE_OPTIONS "/Z7")
        target_link_options(ps2EntryRunner PRIVATE /DEBUG)
        target_link_libraries(ps2EntryRunner PRIVATE dbghelp)
    endif()
elseif(VCS_RUNTIME_DEBUG_INFO AND MINGW)
    # llvm-mingw: CodeView in a PDB, as clang-cl's /Z7 /DEBUG - dbghelp (src/debug: crash handler, profiler) and
    # tools/minidbg.py read it the same way
    foreach(t ps2_runtime ps2_iop)
        if(TARGET ${t})
            target_compile_options(${t} PRIVATE -g -gcodeview)
        endif()
    endforeach()
    if(VCS_HAVE_GENERATED)
        set_source_files_properties(${VCS_SOURCES} "${CMAKE_SOURCE_DIR}/src/main.cpp"
            TARGET_DIRECTORY ps2EntryRunner PROPERTIES COMPILE_OPTIONS "-g;-gcodeview")
        target_link_options(ps2EntryRunner PRIVATE -g -Wl,--pdb=)
        target_link_libraries(ps2EntryRunner PRIVATE dbghelp)
    endif()
elseif(VCS_RUNTIME_DEBUG_INFO)
    # Linux: line tables for the crash handler / profiler (src/debug, symbolized with llvm-symbolizer); frame pointers
    # for their backtraces
    foreach(t ps2_runtime ps2_iop)
        if(TARGET ${t})
            target_compile_options(${t} PRIVATE -gline-tables-only -fno-omit-frame-pointer)
        endif()
    endforeach()
    if(VCS_HAVE_GENERATED)
        set_source_files_properties(${VCS_SOURCES} "${CMAKE_SOURCE_DIR}/src/main.cpp"
            TARGET_DIRECTORY ps2EntryRunner PROPERTIES COMPILE_OPTIONS "-gline-tables-only;-fno-omit-frame-pointer")
        target_link_options(ps2EntryRunner PRIVATE -rdynamic)
    endif()
endif()
# llvm-mingw: the profiler's __try / __except (src/debug/profiler.cpp) need the Microsoft extensions clang-cl has on
# (here, after the debug info's COMPILE_OPTIONS above, which would replace them)
if(MINGW AND VCS_HAVE_GENERATED)
    set_property(SOURCE "${CMAKE_SOURCE_DIR}/src/debug/profiler.cpp" TARGET_DIRECTORY ps2EntryRunner APPEND PROPERTY
                 COMPILE_OPTIONS -fms-extensions)
endif()
