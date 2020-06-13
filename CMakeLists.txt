cmake_minimum_required(VERSION 3.15)
project(aryibi LANGUAGES C CXX)

if (${CMAKE_BUILD_TYPE} STREQUAL "Debug" OR ${CMAKE_BUILD_TYPE} STREQUAL "RelWithDebInfo")
    option(ARYIBI_DETECT_LEAKS "Detect most of the leaks that happen because of forgetting to unload handles" ON)
else ()
    option(ARYIBI_DETECT_LEAKS "Detect most of the leaks that happen because of forgetting to unload handles" OFF)
endif ()

if (${ARYIBI_DETECT_LEAKS})
    message(STATUS "[aryibi] Leak detection is ON")
else ()
    message(STATUS "[aryibi] Leak detection is OFF")
endif ()

set(ARYIBI_BACKEND "glfw-vulkan" CACHE STRING "The backend to use. Can be: 'glfw-opengl', 'glfw-vulkan', 'none'. Default: 'glfw-vulkan'")

set(CMAKE_CXX_STANDARD 17)

add_library(aryibi STATIC src/sprites.cpp)

if (CMAKE_BUILD_TYPE MATCHES Debug)
    target_compile_definitions(aryibi PRIVATE ARYIBI_DEBUG)
endif()

target_include_directories(aryibi PUBLIC include)
target_include_directories(aryibi PRIVATE src)

# ARYIBI_REQUIRED_LIBS are the required library targets that must be supplied externally.
if (ARYIBI_BACKEND STREQUAL "glfw-opengl")
    message(STATUS "[aryibi] Using GLFW + OpenGL backend")
    message(STATUS "[aryibi] For the stb required library target, create an interface library with an interface
    target_include_directories containing the stb directory.")
    message(STATUS "[aryibi] For the imgui required library target, create an static library that includes the imgui
    directory publicly and has the following sources: imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/imgui_widgets.cpp
    imgui/imgui.cpp imgui/examples/imgui_impl_glfw.cpp imgui/examples/imgui_impl_opengl3.cpp")
    target_sources(aryibi PRIVATE src/renderer/opengl/renderer.cpp src/renderer/opengl/renderer_types.cpp
            src/windowing/glfw/windowing.cpp)
    set(ARYIBI_REQUIRED_LIBS glad glfw imgui stb)
elseif(ARYIBI_BACKEND STREQUAL "glfw-vulkan")
    message(STATUS "[aryibi] Using GLFW + Vulkan backend")
    message(STATUS "[aryibi] For the stb required library target, create an interface library with an interface
    target_include_directories containing the stb directory.")
    message(STATUS "[aryibi] For the imgui required library target, create an static library that includes the imgui
    directory publicly and has the following sources: imgui/imgui_draw.cpp imgui/imgui_demo.cpp imgui/imgui_widgets.cpp
    imgui/imgui.cpp imgui/examples/imgui_impl_glfw.cpp imgui/examples/imgui_impl_opengl3.cpp")
    target_sources(aryibi PRIVATE
        src/renderer/vulkan/detail/buffer.hpp
        src/renderer/vulkan/detail/buffer.cpp
        src/renderer/vulkan/detail/command_buffer.hpp
        src/renderer/vulkan/detail/command_buffer.cpp
        src/renderer/vulkan/detail/constants.hpp
        src/renderer/vulkan/detail/context.hpp
        src/renderer/vulkan/detail/context.cpp
        src/renderer/vulkan/detail/descriptor_set.hpp
        src/renderer/vulkan/detail/descriptor_set.cpp
        src/renderer/vulkan/detail/forwards.hpp
        src/renderer/vulkan/detail/image.hpp
        src/renderer/vulkan/detail/image.cpp
        src/renderer/vulkan/detail/mesh.hpp
        src/renderer/vulkan/detail/mesh.cpp
        src/renderer/vulkan/detail/vma.cpp
        src/renderer/vulkan/detail/pipeline.hpp
        src/renderer/vulkan/detail/pipeline.cpp
        src/renderer/vulkan/detail/raw_buffer.hpp
        src/renderer/vulkan/detail/raw_buffer.cpp
        src/renderer/vulkan/detail/render_pass.hpp
        src/renderer/vulkan/detail/render_pass.cpp
        src/renderer/vulkan/detail/swapchain.hpp
        src/renderer/vulkan/detail/swapchain.cpp
        src/renderer/vulkan/detail/texture.hpp
        src/renderer/vulkan/detail/texture.cpp
        src/renderer/vulkan/detail/sampler.hpp
        src/renderer/vulkan/detail/sampler.cpp
        src/renderer/vulkan/detail/dyn_mesh.hpp
        src/renderer/vulkan/detail/dyn_mesh.cpp
        src/renderer/vulkan/renderer_types.cpp
        src/renderer/vulkan/impl_types.hpp
        src/renderer/vulkan/renderer.cpp
        src/windowing/glfw/impl_types.hpp
        src/windowing/glfw/windowing.cpp)
    set(ARYIBI_REQUIRED_LIBS vma glfw imgui stb)
elseif (ARYIBI_BACKEND STREQUAL "none")
else ()
    message(FATAL_ERROR "Please select a valid backend for ARYIBI_BACKEND.")
endif ()

foreach (REQUIRED_LIB ${ARYIBI_REQUIRED_LIBS})
    if (NOT TARGET ${REQUIRED_LIB})
        message(FATAL_ERROR "You must supply the '${REQUIRED_LIB}' library target before including aryibi in order to\
        meet the requirements of the ${ARYIBI_BACKEND} aryibi backend.")
    endif ()
    target_link_libraries(aryibi PRIVATE ${REQUIRED_LIB})
endforeach ()

add_subdirectory(lib)