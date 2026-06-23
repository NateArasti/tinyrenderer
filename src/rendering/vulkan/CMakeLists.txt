cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
)

add_library(rendering_vulkan STATIC ${SOURCES})

target_include_directories(rendering_vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(rendering_vulkan
    PUBLIC
        app
        rendering
        Vulkan::Vulkan
)

find_package(Vulkan REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(rendering_vulkan
    PRIVATE
        glfw
)
