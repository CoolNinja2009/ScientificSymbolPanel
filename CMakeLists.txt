cmake_minimum_required(VERSION 3.28)
project(ScientificSymbolPanel VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Dependencies (vcpkg) ----
find_package(SDL2 CONFIG REQUIRED)
find_package(SDL2_ttf CONFIG REQUIRED)

# ---- Source files ----
file(GLOB_RECURSE SSP_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
# Exclude platform-specific stubs and nanoVG
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/Platform/Win32\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/nanovg\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/nanovg_gl\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/nanovg_impl\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/stb_truetype\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/gl_loader\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/glcorearb\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/KHR/")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/InputHandler\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/TextEdit\\.")
list(FILTER SSP_SOURCES EXCLUDE REGEX ".*/src/UI/stb_textedit\\.")

# ---- Executable ----
if(WIN32)
    add_executable(SSP ${SSP_SOURCES} assets/icon.rc)
else()
    add_executable(SSP ${SSP_SOURCES})
endif()

# ---- Copy data + font files ----
add_custom_command(TARGET SSP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/data" "$<TARGET_FILE_DIR:SSP>/data"
    COMMENT "Copying data..."
)
add_custom_command(TARGET SSP POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:SSP>/assets"
    COMMENT "Copying assets..."
)

# ---- Link ----
target_link_libraries(SSP PRIVATE
    SDL2::SDL2
    $<IF:$<TARGET_EXISTS:SDL2_ttf::SDL2_ttf>,SDL2_ttf::SDL2_ttf,SDL2_ttf::SDL2_ttf-static>
)

# ---- Include dirs ----
target_include_directories(SSP PRIVATE src)

# ---- Platform ----
if(WIN32)
    target_compile_definitions(SSP PRIVATE
        UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS
        _SILENCE_CXX17_CODECVT_HEADER_DEPRECATION_WARNING
    )
    target_link_libraries(SSP PRIVATE user32 gdi32 shell32 dwmapi ole32)
    target_compile_options(SSP PRIVATE /W3 /EHsc /std:c++latest /FS /MP /Zc:__cplusplus)
elseif(APPLE)
    target_compile_options(SSP PRIVATE -Wall -Wextra)
else()
    target_link_libraries(SSP PRIVATE dl pthread)
    target_compile_options(SSP PRIVATE -Wall -Wextra)
endif()

# ---- Debug ----
target_compile_definitions(SSP PRIVATE $<$<CONFIG:Debug>:SSP_DEBUG>)

# ---- Install ----
install(TARGETS SSP RUNTIME DESTINATION .)
