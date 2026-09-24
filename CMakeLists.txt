cmake_minimum_required(VERSION 4.3.1)

project(
    tinyrenderer 
    VERSION 1.0 
    LANGUAGES C CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(tinyrenderer
    src/main.cpp
    src/tiny_renderer.cpp
    src/tiny_renderer.h
)
if(WIN32)
    set_target_properties(tinyrenderer PROPERTIES
        WIN32_EXECUTABLE "$<$<CONFIG:Release>:TRUE>"
    )
endif()
if(MSVC)
    target_link_options(tinyrenderer PRIVATE "$<$<CONFIG:Release>:/ENTRY:mainCRTStartup>")
endif()

add_subdirectory(third-party/ufbx)

add_subdirectory(src/app)
add_subdirectory(src/resources)
add_subdirectory(src/data)
add_subdirectory(src/embedded_shaders)
add_subdirectory(src/rendering)
add_subdirectory(src/controllers)
add_subdirectory(src/ui)
add_subdirectory(src/loader)

target_link_libraries(tinyrenderer 
    PRIVATE 
        app
        resources
        data
        embedded_shaders
        rendering
        controllers
        ui
        loader
)

target_link_libraries(tinyrenderer PRIVATE rendering_vulkan)

find_package(fmt REQUIRED)
target_link_libraries(tinyrenderer PRIVATE fmt::fmt)

