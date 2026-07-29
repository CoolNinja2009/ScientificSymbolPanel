cmake_minimum_required(VERSION 3.28)
project(SSP_CrossPlatform VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ============================================================================
# Source files
# ============================================================================

file(GLOB_RECURSE SSP_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)

# Windows: GUI app (no console). Other platforms: console.
if(WIN32)
    add_executable(SSP WIN32 ${SSP_SOURCES})
else()
    add_executable(SSP ${SSP_SOURCES})
endif()

# ============================================================================
# Dependencies (vcpkg or FetchContent)
# ============================================================================

if(DEFINED VCPKG_TOOLCHAIN_FILE OR DEFINED CMAKE_TOOLCHAIN_FILE)
    # vcpkg mode
    find_package(glfw3 CONFIG REQUIRED)
    find_package(imgui CONFIG REQUIRED)
    find_package(OpenGL REQUIRED)
    target_link_libraries(SSP PRIVATE
        glfw
        imgui::imgui
        OpenGL::GL
    )
else()
    # FetchContent fallback
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

# Platform-specific
if(WIN32)
    target_link_libraries(SSP PRIVATE dwmapi)
elseif(APPLE)
    target_link_libraries(SSP PRIVATE
        "-framework Cocoa"
        "-framework IOKit"
        "-framework CoreFoundation"
    )
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
else()
    target_compile_options(SSP PRIVATE -Wall -Wextra -Wpedantic)
endif()

# Config-specific optimizations (genex avoids Debug/Release flag conflicts)
target_compile_options(SSP PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/O2;/Gw>
    $<$<AND:$<NOT:$<CXX_COMPILER_ID:MSVC>>,$<CONFIG:Release>>:-O2>
)
target_link_options(SSP PRIVATE
    $<$<AND:$<CXX_COMPILER_ID:MSVC>,$<CONFIG:Release>>:/OPT:REF;/OPT:ICF>
)

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
