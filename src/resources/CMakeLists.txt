cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
    "*.cpp" 
)

add_library(resources INTERFACE ${SOURCES})

target_include_directories(resources 
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(resources PROPERTIES LINKER_LANGUAGE CXX)
