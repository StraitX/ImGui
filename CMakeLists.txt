cmake_minimum_required(VERSION 3.10)

project(StraitXImGui)

set(CMAKE_CXX_STANDARD 14)

add_subdirectory(thirdparty/imgui)

set(SX_IMGUI_SOURCES_DIR ${PROJECT_SOURCE_DIR}/sources)
set(SX_IMGUI_INCLUDE_DIR ${PROJECT_SOURCE_DIR}/include)
set(SX_IMGUI_THIRDPARTY_DIR ${PROJECT_SOURCE_DIR}/thirdparty)

set(SX_IMGUI_SOURCES
    ${SX_IMGUI_SOURCES_DIR}/backend.cpp
    ${SX_IMGUI_SOURCES_DIR}/widgets.cpp
)

add_library(StraitXImGui STATIC ${SX_IMGUI_SOURCES})
target_link_libraries(StraitXImGui 
    PUBLIC StraitXBase
    PUBLIC imgui
)
target_include_directories(StraitXImGui 
    PUBLIC  ${SX_IMGUI_INCLUDE_DIR}
    PUBLIC  ${SX_IMGUI_THIRDPARTY_DIR}/imgui
)