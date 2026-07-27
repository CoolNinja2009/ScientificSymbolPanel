cmake_minimum_required(VERSION 3.28)
project(ScientificSymbolPanel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Windows Unicode
if(WIN32)
    add_compile_definitions(UNICODE _UNICODE)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

# Source files
file(GLOB_RECURSE SSP_SOURCES CONFIGURE_DEPENDS
    src/*.cpp
    src/*.h
)
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/InputHandler\\.(cpp|h)$")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/TextEdit\\.(cpp|h)$")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/stb_textedit\\.h$")

# Executable
add_executable(SSP WIN32
    ${SSP_SOURCES}
    assets/icon.rc
)

# Copy data files to output directory
add_custom_command(TARGET SSP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data"
        "$<TARGET_FILE_DIR:SSP>/data"
    COMMENT "Copying data files..."
)

# Link libraries
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

# Include directories
target_include_directories(SSP PRIVATE src)

# Compiler options
target_compile_options(SSP PRIVATE
    /W4
    /Zi
    /EHsc
    /std:c++latest
    /FS
    /MP
    /Zc:__cplusplus
    /Zc:preprocessor
)

target_link_options(SSP PRIVATE /DEBUG:FULL)

# Release optimizations
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_options(SSP PRIVATE
        /O2
        /GL
        /Gw
        /MT
    )
    target_link_options(SSP PRIVATE
        /LTCG
        /OPT:REF
        /OPT:ICF
    )
    # Strip debug info from release
    target_link_options(SSP PRIVATE /DEBUG:NONE)
endif()

# Debug defines
target_compile_definitions(SSP PRIVATE
    $<$<CONFIG:Debug>:SSP_DEBUG>
)

# Installation
install(TARGETS SSP RUNTIME DESTINATION .)
