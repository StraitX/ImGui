cmake_minimum_required(VERSION 3.10)

project(StraitXImGui)

set(CMAKE_CXX_STANDARD 14)

set(SX_IMGUI_SOURCES_DIR ${PROJECT_SOURCE_DIR}/sources)
set(SX_IMGUI_THIRDPARTY_DIR ${PROJECT_SOURCE_DIR}/thirdparty)

set(SX_IMGUI_SOURCES

)

add_library(StraitXImGui STATIC ${SX_IMGUI_SOURCES})
target_link_libraries(StraitXImGui PUBLIC StraitXBase)