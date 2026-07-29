cmake_minimum_required(VERSION 4.3.1)

add_library(ufbx STATIC ufbx.c ufbx.h)

target_include_directories(ufbx 
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
)
