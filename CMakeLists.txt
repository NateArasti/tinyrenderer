cmake_minimum_required(VERSION 4.3.1)

project(
    tinyrenderer 
    VERSION 1.0 
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tinyrenderer src/main.cpp)

add_subdirectory(src/app)
add_subdirectory(src/resources)
add_subdirectory(src/data)
add_subdirectory(src/rendering)
add_subdirectory(src/controllers)

target_link_libraries(tinyrenderer 
    PRIVATE 
        app
        resources
        data
        rendering
        controllers
)

target_link_libraries(tinyrenderer PRIVATE rendering_vulkan)

find_package(Stb REQUIRED)
target_include_directories(tinyrenderer PRIVATE ${Stb_INCLUDE_DIR})

find_package(fmt REQUIRED)
target_link_libraries(tinyrenderer PRIVATE fmt::fmt)

