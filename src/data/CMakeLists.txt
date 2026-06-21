cmake_minimum_required(VERSION 4.3.1)

file(GLOB SOURCES
    "*.h"
)

add_library(data INTERFACE ${SOURCES})

target_include_directories(data 
    INTERFACE ${CMAKE_CURRENT_SOURCE_DIR}
)

set_target_properties(data PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(data INTERFACE resources)

find_package(glm CONFIG REQUIRED)
target_link_libraries(data INTERFACE glm::glm)
