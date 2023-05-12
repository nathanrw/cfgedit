cmake_minimum_required(VERSION 3.20 FATAL_ERROR)

project(cfgedit)

add_subdirectory(glfw)

set(sources
src/main.cpp
imgui/imgui.cpp
imgui/imgui_draw.cpp
imgui/imgui_tables.cpp
imgui/imgui_widgets.cpp
imgui/backends/imgui_impl_glfw.cpp
imgui/backends/imgui_impl_opengl3.cpp
)
set(headers
imgui/imgui.h
imgui/imconfig.h
imgui/imgui_internal.h
imgui/imstb_rectpack.h
imgui/imstb_textedit.h
imgui/imstb_truetype.h
imgui/backends/imgui_impl_glfw.h
imgui/backends/imgui_impl_opengl3.h
)

add_executable(cfgedit "${sources}" "${headers}")
target_include_directories(cfgedit PUBLIC imgui imgui/backends rapidjson/include)
target_link_libraries(cfgedit PUBLIC glfw)
if (WIN32)
  target_link_libraries(cfgedit PUBLIC opengl32.lib)
  target_link_options(cfgedit PRIVATE /SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup)
endif()