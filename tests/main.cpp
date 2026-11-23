#include <cstdlib>
#include <iostream>

#include <gtest/gtest.h>

namespace tr::Tests {
    void registerDemoSceneTests();
}

int main(int argc, char** argv) {
#ifdef _WIN32
    const int result = _putenv_s("VK_IMPLICIT_LAYER_PATH", TEST_IMPLICIT_LAYER_PATH);
#else
    const int result = setenv("VK_IMPLICIT_LAYER_PATH", TEST_IMPLICIT_LAYER_PATH, 1);
#endif
    if (result != 0) {
        std::cerr << "Couldn't configure the Vulkan test environment.\n";
        return EXIT_FAILURE;
    }

    testing::InitGoogleTest(&argc, argv);
    tr::Tests::registerDemoSceneTests();
    return RUN_ALL_TESTS();
}
