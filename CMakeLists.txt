cmake_minimum_required(VERSION 4.2)
project(Ancile LANGUAGES CXX C)

# Force modern C++ standard required for SDL3 and ImGui bindings
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Set up clean path constants pointing to your shared project root folder
set(LOCAL_SDL3_PATH "${CMAKE_SOURCE_DIR}/third_party/sdl3-src")
set(LOCAL_IMGUI_PATH "${CMAKE_SOURCE_DIR}/third_party/imgui-src")

# ==============================================================================
# 1. Populate SDL3 (Check Local First, Otherwise Download)
# ==============================================================================
if(EXISTS "${LOCAL_SDL3_PATH}/CMakeLists.txt")
    message(STATUS "Ancile: Found existing local SDL3 source directory.")
    FetchContent_Declare(
            SDL3
            SOURCE_DIR "${LOCAL_SDL3_PATH}"
            BINARY_DIR "${CMAKE_BINARY_DIR}/third_party/sdl3-build"
    )
else()
    message(STATUS "Ancile: SDL3 missing locally. Downloading from GitHub...")
    FetchContent_Declare(
            SDL3
            GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
            GIT_TAG        main
            SOURCE_DIR     "${LOCAL_SDL3_PATH}"
            BINARY_DIR     "${CMAKE_BINARY_DIR}/third_party/sdl3-build"
    )
endif()

# Disable SDL3 tests, documentation, and examples to keep compile times ultra-fast
set(SDL_TESTS OFF CACHE BOOL "" FORCE)
set(SDL_DOCS OFF CACHE BOOL "" FORCE)
set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(SDL3)

# ==============================================================================
# 2. Find Local Vulkan SDK
# ==============================================================================
find_package(Vulkan REQUIRED)

# ==============================================================================
# 3. Populate Dear ImGui (Check Local First, Otherwise Download)
# ==============================================================================
if(EXISTS "${LOCAL_IMGUI_PATH}/imgui.cpp")
    message(STATUS "Ancile: Found existing local Dear ImGui source directory.")
    FetchContent_Declare(
            imgui_src
            SOURCE_DIR "${LOCAL_IMGUI_PATH}"
            BINARY_DIR "${CMAKE_BINARY_DIR}/third_party/imgui-build"
    )
else()
    message(STATUS "Ancile: Dear ImGui missing locally. Downloading from GitHub...")
    FetchContent_Declare(
            imgui_src
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG        docking
            SOURCE_DIR     "${LOCAL_IMGUI_PATH}"
            BINARY_DIR     "${CMAKE_BINARY_DIR}/third_party/imgui-build"
    )
endif()
FetchContent_MakeAvailable(imgui_src)

# Define ImGui as a static target library within CMake
add_library(imgui STATIC)

# Source paths for fundamental ImGui source files + platform & renderer backends
target_sources(imgui PRIVATE
        ${imgui_src_SOURCE_DIR}/imgui.cpp
        ${imgui_src_SOURCE_DIR}/imgui_demo.cpp
        ${imgui_src_SOURCE_DIR}/imgui_draw.cpp
        ${imgui_src_SOURCE_DIR}/imgui_tables.cpp
        ${imgui_src_SOURCE_DIR}/imgui_widgets.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
        ${imgui_src_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp
)

# Expose internal headers publicly to our executable down the pipeline
target_include_directories(imgui PUBLIC
        ${imgui_src_SOURCE_DIR}
        ${imgui_src_SOURCE_DIR}/backends
)

# Safe mapping target linker for compiled-from-source structures
if(TARGET SDL3::SDL3-shared)
    target_link_libraries(imgui PUBLIC SDL3::SDL3-shared Vulkan::Vulkan)
else()
    target_link_libraries(imgui PUBLIC SDL3::SDL3-static Vulkan::Vulkan)
endif()

# Define your local path variable for GLM (adjust the path to match your folder structure)
set(LOCAL_GLM_PATH "${CMAKE_SOURCE_DIR}/third_party/glm")

if(EXISTS "${LOCAL_GLM_PATH}/CMakeLists.txt")
    message(STATUS "Ancile: Found existing local GLM source directory.")
    FetchContent_Declare(
            glm
            SOURCE_DIR "${LOCAL_GLM_PATH}"
            BINARY_DIR "${CMAKE_BINARY_DIR}/third_party/glm-build"
    )
else()
    message(STATUS "Ancile: GLM missing locally. Downloading from GitHub...")
    FetchContent_Declare(
            glm
            GIT_REPOSITORY https://github.com/g-truc/glm.git
            GIT_TAG        1.0.1 # Using a stable release tag is recommended over 'master'
            SOURCE_DIR     "${LOCAL_GLM_PATH}"
            BINARY_DIR     "${CMAKE_BINARY_DIR}/third_party/glm-build"
    )
endif()

# Turn off GLM test compilation to keep configure and build times instantaneous
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(glm)

# ==============================================================================
# AncileNet Pre-built Library Import
# ==============================================================================
set(ANCILE_NET_ROOT "${CMAKE_SOURCE_DIR}/third_party/AncileNet")

# Create a shared library target for the pre-built binaries
add_library(AncileNet SHARED IMPORTED)

# Set the public include directory so headers are accessible project-wide
set_target_properties(AncileNet PROPERTIES
        INTERFACE_INCLUDE_DIRECTORIES "${ANCILE_NET_ROOT}/include"
)

if(WIN32)
    set_target_properties(AncileNet PROPERTIES
            # Path to the runtime .dll
            IMPORTED_LOCATION_DEBUG "${ANCILE_NET_ROOT}/bin/win64/debug/AncileNet.dll"
            IMPORTED_IMPLIB_DEBUG   "${ANCILE_NET_ROOT}/bin/win64/debug/AncileNetStatic.lib"
            IMPORTED_LOCATION_RELEASE "${ANCILE_NET_ROOT}/bin/win64/release/AncileNet.dll"
            IMPORTED_IMPLIB_RELEASE   "${ANCILE_NET_ROOT}/bin/win64/release/AncileNetStatic.lib"
            IMPORTED_IMPLIB   "${ANCILE_NET_ROOT}/bin/win64/release/AncileNetStatic.lib"
    )
elseif(UNIX AND NOT APPLE)
    set_target_properties(AncileNet PROPERTIES
            # Path to the shared library .so
            IMPORTED_LOCATION_DEBUG "${ANCILE_NET_ROOT}/bin/linux/debug/libAncileNet.so"
            IMPORTED_IMPLIB_DEBUG "${ANCILE_NET_ROOT}/bin/linux/debug/libAncileNetStatic.a"
            IMPORTED_LOCATION_RELEASE "${ANCILE_NET_ROOT}/bin/linux/release/libAncileNet.so"
            IMPORTED_IMPLIB_RELEASE "${ANCILE_NET_ROOT}/bin/linux/release/libAncileNetStatic.a"
    )
endif()

# ==============================================================================
# 4. Define and Link Application Executable
# ==============================================================================

add_executable(Ancile)
add_subdirectory(src)
target_link_libraries(Ancile PRIVATE imgui)
target_link_libraries(Ancile PRIVATE glm::glm)
target_link_libraries(Ancile PRIVATE AncileNet)
target_include_directories(Ancile PUBLIC include)

set_target_properties(Ancile PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# ==============================================================================
# 5. Automatically Copy DLLs to Output Directory (Windows)
# ==============================================================================
if(WIN32)
    # Automatically copy runtime dependencies (like SDL3.dll) next to Ancile.exe after build
    add_custom_command(
            TARGET Ancile POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:SDL3::SDL3-shared>
            $<TARGET_FILE_DIR:Ancile>
            VERBATIM
    )
endif()

# ==============================================================================
# Link and Copy setup for executable
# ==============================================================================
# Copy binaries to runtime folder post-build
add_custom_command(
        TARGET Ancile POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:AncileNet>
        $<TARGET_FILE_DIR:Ancile>
        VERBATIM
)
