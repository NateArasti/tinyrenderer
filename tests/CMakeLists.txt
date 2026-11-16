find_package(GTest CONFIG REQUIRED)

add_executable(tinyrenderer_tests
    main.cpp
    demo_smoke_tests.cpp
)

target_link_libraries(tinyrenderer_tests PRIVATE
    GTest::gtest
    app
    data
    embedded_shaders
    loader
    rendering
    rendering_vulkan
    ui
)

include(GoogleTest)
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/implicit_layers")
target_compile_definitions(tinyrenderer_tests PRIVATE
    TEST_IMPLICIT_LAYER_PATH="${CMAKE_CURRENT_BINARY_DIR}/implicit_layers"
)
gtest_discover_tests(tinyrenderer_tests
    DISCOVERY_MODE PRE_TEST
    NO_PRETTY_VALUES
    PROPERTIES
        LABELS demo_smoke
        RESOURCE_LOCK vulkan_gpu
        TIMEOUT 180
)
