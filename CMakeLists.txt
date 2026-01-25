cmake_minimum_required(VERSION 3.20)
project(PixelMotion VERSION 1.0.0 LANGUAGES CXX)

# C++20 standard required
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# vcpkg integration
if(DEFINED ENV{VCPKG_ROOT} AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
    set(CMAKE_TOOLCHAIN_FILE "$ENV{VCPKG_ROOT}/scripts/buildsystems/vcpkg.cmake"
        CACHE STRING "")
endif()

# Include custom CMake modules
list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

# Find FFmpeg
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFMPEG REQUIRED IMPORTED_TARGET
    libavcodec
    libavformat
    libavutil
    libswscale
    libswresample
)

# Find ImGui
find_package(imgui CONFIG REQUIRED)

# DirectX 11 libraries (Windows SDK)
set(DX11_LIBRARIES
    d3d11.lib
    dxgi.lib
    d3dcompiler.lib
    dxguid.lib
)

# Windows libraries
set(WIN_LIBRARIES
    winmm.lib
    comctl32.lib
    shell32.lib
)

# Source files
set(CORE_SOURCES
    src/main.cpp
    src/Application.cpp
    src/core/Configuration.cpp
    src/core/Logger.cpp
)

set(DESKTOP_SOURCES
    src/desktop/DesktopManager.cpp
    src/desktop/MonitorManager.cpp
    src/desktop/MonitorInfo.cpp
    src/desktop/WallpaperWindow.cpp
)

set(RENDERING_SOURCES
    src/rendering/DX11Device.cpp
    src/rendering/RendererContext.cpp
    src/rendering/TextureManager.cpp
)

set(VIDEO_SOURCES
    src/video/VideoDecoder.cpp
    src/video/FrameQueue.cpp
    src/video/AudioPlayer.cpp
)

set(RESOURCE_SOURCES
    src/resources/ResourceManager.cpp
    src/resources/GameModeDetector.cpp
    src/resources/BatteryMonitor.cpp
)

set(UI_SOURCES
    src/ui/TrayIcon.cpp
    src/ui/SettingsWindow.cpp
    src/ui/PixelMotion.rc
)

# Shader files
set(SHADER_SOURCES
    src/rendering/shaders/FullscreenQuad.hlsl
    src/rendering/shaders/NV12ToRGBA.hlsl
)

# Main executable
add_executable(PixelMotion WIN32
    ${CORE_SOURCES}
    ${DESKTOP_SOURCES}
    ${RENDERING_SOURCES}
    ${VIDEO_SOURCES}
    ${RESOURCE_SOURCES}
    ${UI_SOURCES}
)

# Include directories
target_include_directories(PixelMotion PRIVATE
    ${CMAKE_SOURCE_DIR}/src
    ${FFMPEG_INCLUDE_DIRS}
)

# Link libraries
target_link_libraries(PixelMotion PRIVATE
    PkgConfig::FFMPEG
    imgui::imgui
    ${DX11_LIBRARIES}
    ${WIN_LIBRARIES}
)

# Compiler flags
if(MSVC)
    target_compile_options(PixelMotion PRIVATE
        /W4                 # Warning level 4
        /WX-                # Warnings not as errors (for now)
        /permissive-        # Standards conformance
        /MP                 # Multi-processor compilation
        /EHsc               # Exception handling
        /utf-8              # UTF-8 source and execution
    )
    
    # Debug configuration
    target_compile_definitions(PixelMotion PRIVATE
        $<$<CONFIG:Debug>:_DEBUG>
        $<$<CONFIG:Debug>:DEBUG_BUILD>
    )
    
    # Release optimizations
    target_compile_options(PixelMotion PRIVATE
        $<$<CONFIG:Release>:/O2>
        $<$<CONFIG:Release>:/GL>
    )
    target_link_options(PixelMotion PRIVATE
        $<$<CONFIG:Release>:/LTCG>
    )
endif()

# Windows-specific definitions
target_compile_definitions(PixelMotion PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00  # Windows 10/11
)

# Compile HLSL shaders
function(compile_shader SHADER_FILE SHADER_TYPE ENTRY_POINT)
    get_filename_component(SHADER_NAME ${SHADER_FILE} NAME_WE)
    set(COMPILED_SHADER "${CMAKE_BINARY_DIR}/shaders/${SHADER_NAME}.cso")
    
    add_custom_command(
        OUTPUT ${COMPILED_SHADER}
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/shaders"
        COMMAND fxc /T ${SHADER_TYPE}_5_0 /E ${ENTRY_POINT} /Fo ${COMPILED_SHADER} ${SHADER_FILE}
        DEPENDS ${SHADER_FILE}
        COMMENT "Compiling shader: ${SHADER_NAME}"
        VERBATIM
    )
    
    target_sources(PixelMotion PRIVATE ${COMPILED_SHADER})
endfunction()

# Compile vertex shader
compile_shader(
    "${CMAKE_SOURCE_DIR}/src/rendering/shaders/FullscreenQuad.hlsl"
    vs
    VSMain
)

# Compile pixel shader
compile_shader(
    "${CMAKE_SOURCE_DIR}/src/rendering/shaders/NV12ToRGBA.hlsl"
    ps
    PSMain
)

# Copy compiled shaders to output directory
add_custom_command(TARGET PixelMotion POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_BINARY_DIR}/shaders"
        "$<TARGET_FILE_DIR:PixelMotion>/shaders"
    COMMENT "Copying shaders to output directory"
)

# Installation
install(TARGETS PixelMotion
    RUNTIME DESTINATION bin
)

install(DIRECTORY "${CMAKE_BINARY_DIR}/shaders"
    DESTINATION bin
)
