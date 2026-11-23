cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "resources/*.h"
    "resources/*.cpp"
)

add_library(rendering_vulkan STATIC ${SOURCES})

target_include_directories(rendering_vulkan PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/resources
)

target_link_libraries(rendering_vulkan
    PRIVATE
        app
        rendering
        data
        embedded_shaders
)

find_package(imgui CONFIG REQUIRED)
target_link_libraries(rendering_vulkan
    PRIVATE
        ui
        imgui::imgui
)

find_package(Vulkan REQUIRED)
target_link_libraries(rendering_vulkan
    PUBLIC
        Vulkan::Vulkan
)
find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(rendering_vulkan
    PRIVATE
        glfw
)
