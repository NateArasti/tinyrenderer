cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "passes/*.h"
    "passes/*.cpp"
)

add_library(rendering STATIC ${SOURCES})

target_include_directories(rendering PUBLIC  
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/passes
)

target_link_libraries(rendering
    PRIVATE
        app
        data
        ui
)

add_subdirectory(vulkan)

find_package(fmt REQUIRED)
target_link_libraries(rendering PRIVATE embedded_shaders fmt::fmt)
