cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
)

add_library(controllers STATIC ${SOURCES})

target_include_directories(controllers PUBLIC  
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(controllers PRIVATE app data)

find_package(glm CONFIG REQUIRED)
target_link_libraries(controllers PRIVATE glm::glm)
