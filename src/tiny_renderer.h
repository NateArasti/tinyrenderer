#pragma once

#include <memory>

namespace tr {
    class TinyRenderer {
    private:
        struct Impl;
        std::unique_ptr<Impl> _impl;

    public:
        TinyRenderer();
        ~TinyRenderer();

        TinyRenderer(const TinyRenderer&) = delete;
        TinyRenderer& operator=(const TinyRenderer&) = delete;
        TinyRenderer(TinyRenderer&&) = delete;
        TinyRenderer& operator=(TinyRenderer&&) = delete;

        int run();
    };
}
