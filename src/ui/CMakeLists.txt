cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp" 
)

add_library(ui STATIC ${SOURCES})

target_include_directories(ui PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(ui PUBLIC app)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(ui PRIVATE imgui::imgui)
