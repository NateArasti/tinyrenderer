cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp" 
)

add_library(app STATIC ${SOURCES})

target_include_directories(app PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(glfw3 CONFIG REQUIRED)
target_link_libraries(app PRIVATE glfw)

find_package(glm CONFIG REQUIRED)
target_link_libraries(app INTERFACE glm::glm)
