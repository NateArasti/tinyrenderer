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
        rendering
)

find_package(fmt REQUIRED)
target_link_libraries(loader PRIVATE fmt::fmt)

find_package(Stb REQUIRED)
target_include_directories(loader PRIVATE ${Stb_INCLUDE_DIR})

find_package(rapidobj CONFIG REQUIRED)
target_link_libraries(loader PRIVATE rapidobj::rapidobj)

find_package(fastgltf CONFIG REQUIRED)
target_link_libraries(loader PRIVATE fastgltf::fastgltf)

target_link_libraries(loader PRIVATE ufbx)
