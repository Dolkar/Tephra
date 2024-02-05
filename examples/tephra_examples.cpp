#include "1_cube/cube.hpp"
#include "window.hpp"

#include <fstream>

#if _WIN32
    #include <shellapi.h>
    #include <cwchar>
#elif __linux__
    #include <cstdlib>
#endif

std::unique_ptr<Example> createExample(std::ofstream& logFile, int exampleNumber) {
    if (!logFile.is_open()) {
        logFile.open("examples_log.txt");
    }

    bool debug = true;
    return std::make_unique<CubeExample>(logFile, debug);
}

#if _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    LPWSTR* argv;
    int argc;
    argv = CommandLineToArgvW(GetCommandLineW(), &argc);

    int exampleNumber = 0;
    if (argc > 1) {
        exampleNumber = std::abs(std::wcstol(argv[1], nullptr, 10));
    }

    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile, exampleNumber);

    WindowManager windowMgr;
    windowMgr.init(hInstance);
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#elif __linux__
int main(int argc, char** argv) {
    int exampleNumber = 0;
    if (argc > 1) {
        exampleNumber = std::abs(std::strtol(argv[1], nullptr, 10));
    }

    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile, exampleNumber);

    WindowManager windowMgr;
    windowMgr.init();
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#elif defined(VK_USE_PLATFORM_METAL_EXT)
// Global function invoked from NS or UI views and controllers to create demo
static void demo_main(struct Demo& demo, void* caMetalLayer, int argc, const char* argv[]) {
    int exampleNumber = 0;
    if (argc > 1) {
        exampleNumber = std::abs(std::strtol(argv[1], nullptr, 10));
    }

    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile, exampleNumber);

    WindowManager windowMgr;
    windowMgr.init(caMetalLayer);
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#endif
