#define SDL_MAIN_HANDLED
#include "App/App.h"
#include "config.h"
#include <chrono>
#include <cstdio>

int main(int argc, char* argv[]) {
    (void)argc; (void)argv;
    auto start = std::chrono::steady_clock::now();

    ssp::App app;
    if (!app.Initialize()) {
        fprintf(stderr, "Failed to initialize Scientific Symbol Panel.\n");
        return 1;
    }

    auto ready = std::chrono::steady_clock::now();
    double initMs = std::chrono::duration<double, std::milli>(ready - start).count();
    printf("Scientific Symbol Panel started in %.1f ms\n", initMs);

    return app.Run();
}
