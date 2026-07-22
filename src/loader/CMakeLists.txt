cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
)

add_library(loader STATIC ${SOURCES})

target_include_directories(loader PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(loader
    PUBLIC
        data
        resources
)

find_package(Stb REQUIRED)
target_include_directories(tinyrenderer PRIVATE ${Stb_INCLUDE_DIR})
