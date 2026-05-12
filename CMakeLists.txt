cmake_minimum_required(VERSION 4.3.1)

project(
    TinyRenderer 
    VERSION 1.0 
    LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tiny-renderer main.cpp)


find_package(Vulkan REQUIRED)
target_link_libraries(tiny-renderer PRIVATE Vulkan::Vulkan)

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(tiny-renderer PRIVATE glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(tiny-renderer PRIVATE glm::glm)

find_package(Stb REQUIRED)
target_include_directories(tiny-renderer PRIVATE ${Stb_INCLUDE_DIR})
