cmake_minimum_required(VERSION 3.28)
project(ScientificSymbolPanel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Backend selection
# ============================================================================
# SSP_BACKEND: "GLFW" (cross-platform, default) or "Win32" (Windows Direct2D)
set(SSP_BACKEND "GLFW" CACHE STRING "Rendering backend: GLFW or Win32")
set_property(CACHE SSP_BACKEND PROPERTY STRINGS "GLFW" "Win32")

if(NOT SSP_BACKEND STREQUAL "Win32")
    set(SSP_BACKEND "GLFW")
endif()

message(STATUS "SSP backend: ${SSP_BACKEND}")

# ============================================================================
# Source files
# ============================================================================

file(GLOB_RECURSE SSP_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

# Exclude the other backend's sources
if(SSP_BACKEND STREQUAL "Win32")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/main_glfw\\.cpp$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/Panel\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/TextEdit\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/stb_textedit\\.h$")
else()
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/main_win32\\.cpp$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/App/.*")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/Renderer\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/Layout\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/Animation\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/Themes\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/TextEdit\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/stb_textedit\\.h$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/Search/TextEdit\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/Search/stb_textedit\\.h$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/Platform/Win32\\.(cpp|h)$")
    list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/resource\\.h$")
endif()

# ============================================================================
# Executable
# ============================================================================

if(SSP_BACKEND STREQUAL "Win32")
    add_executable(SSP WIN32
        ${SSP_SOURCES}
        assets/icon.rc
    )
elseif(WIN32)
    add_executable(SSP WIN32 ${SSP_SOURCES})
else()
    add_executable(SSP ${SSP_SOURCES})
endif()

# ============================================================================
# Backend-specific dependencies
# ============================================================================

if(SSP_BACKEND STREQUAL "Win32")
    # Direct2D / DirectWrite (Windows only, no external deps)
    target_link_libraries(SSP PRIVATE
        d2d1
        dwrite
        windowscodecs
        dwmapi
        user32
        gdi32
        comctl32
        shell32
        shlwapi
        ole32
    )
else()
    # GLFW + ImGui + OpenGL
    if(DEFINED VCPKG_TOOLCHAIN_FILE OR DEFINED CMAKE_TOOLCHAIN_FILE)
        find_package(glfw3 CONFIG REQUIRED)
        find_package(imgui CONFIG REQUIRED)
        find_package(OpenGL REQUIRED)
        target_link_libraries(SSP PRIVATE
            glfw
            imgui::imgui
            OpenGL::GL
        )
    else()
        include(FetchContent)

        FetchContent_Declare(glfw
            GIT_REPOSITORY https://github.com/glfw/glfw.git
            GIT_TAG 3.4
            GIT_SHALLOW TRUE
        )
        set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        FetchContent_MakeAvailable(glfw)

        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.91.9
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(imgui)

        add_library(imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
            ${imgui_SOURCE_DIR}/imgui_demo.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
            ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
        )
        target_include_directories(imgui PUBLIC
            ${imgui_SOURCE_DIR}
            ${imgui_SOURCE_DIR}/backends
        )
        target_link_libraries(imgui PUBLIC glfw)
        find_package(OpenGL REQUIRED)
        target_link_libraries(SSP PRIVATE imgui OpenGL::GL)
    endif()

    if(WIN32)
        target_link_libraries(SSP PRIVATE dwmapi)
    elseif(APPLE)
        target_link_libraries(SSP PRIVATE
            "-framework Cocoa"
            "-framework IOKit"
            "-framework CoreFoundation"
        )
    endif()
endif()

# ============================================================================
# Include dirs
# ============================================================================

target_include_directories(SSP PRIVATE src)

# ============================================================================
# Compiler options
# ============================================================================

if(MSVC)
    target_compile_options(SSP PRIVATE /W4 /EHsc /std:c++latest /FS /MP /Zc:__cplusplus /Zc:preprocessor)

    # Win32 backend: Windows Unicode
    if(SSP_BACKEND STREQUAL "Win32")
        target_compile_definitions(SSP PRIVATE UNICODE _UNICODE)
        target_compile_definitions(SSP PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    endif()
else()
    target_compile_options(SSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Config-specific optimizations
target_compile_options(SSP PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2;/Gw>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O2>
)
target_link_options(SSP PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:REF;/OPT:ICF>
)

# Release: static CRT for Win32 backend, debug info stripping
if(SSP_BACKEND STREQUAL "Win32" AND MSVC)
    target_compile_options(SSP PRIVATE $<$<CONFIG:Release>:/MT>)
    target_link_options(SSP PRIVATE $<$<CONFIG:Release>:/DEBUG:NONE>)
endif()

# Debug defines
target_compile_definitions(SSP PRIVATE
    $<$<CONFIG:Debug>:SSP_DEBUG>
)

# ============================================================================
# Copy data files to output directory
# ============================================================================

add_custom_command(TARGET SSP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:SSP>/data"
    COMMENT "Copying data files..."
)

# ============================================================================
# Installation
# ============================================================================

install(TARGETS SSP RUNTIME DESTINATION .)
