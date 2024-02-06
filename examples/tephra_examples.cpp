#include "1_cube/cube.hpp"
#include "window.hpp"

#include <fstream>

std::unique_ptr<Example> createExample(std::ofstream& logFile) {
    /* if (!logFile.is_open()) {
        logFile.open("examples_log.txt");
    }

    bool debug = true;
    return std::make_unique<CubeExample>(logFile, debug);
    */

    // Replaced with sync validation repro.
    // Init:
    auto application = tp::Application::createApplication({});
    tp::DeviceQueue mainQueue = tp::QueueType::Graphics;
    auto deviceSetup = tp::DeviceSetup(&application->getPhysicalDevices()[0], tp::viewOne(mainQueue));
    auto device = application->createDevice(deviceSetup);
    auto jobResourcePool = device->createJobResourcePool(tp::JobResourcePoolSetup(mainQueue));

    for (int i = 0; i < 100; i++) {
        // Buffer memory allocated through VMA, then released back to it along with the VkBuffer handle once the
        // timeline semaphore gets signalled by the job after the end of this scope
        auto bufferSetup = tp::BufferSetup(0x1000, tp::BufferUsageMask::None());
        std::unique_ptr<tp::Buffer> buffer = device->allocateBuffer(bufferSetup, tp::MemoryPreference::Device);

        tp::Job job = jobResourcePool->createJob();

        job.cmdFillBuffer(*buffer, 0x01234567);

        device->enqueueJob(mainQueue, std::move(job));
        device->submitQueuedJobs(mainQueue);

        // Not necessary for repro, but makes it appear sooner:
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        // Meanwhile this fixes the issue:
        // device->waitForIdle();
    }

    exit(0);
    return nullptr;
}

#if _WIN32
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {
    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile);

    WindowManager windowMgr;
    windowMgr.init(hInstance);
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#elif __linux__
int main(int argc, char** argv) {
    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile);

    WindowManager windowMgr;
    windowMgr.init();
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#elif defined(VK_USE_PLATFORM_METAL_EXT)
// Global function invoked from NS or UI views and controllers to create demo
static void demo_main(struct Demo& demo, void* caMetalLayer, int argc, const char* argv[]) {
    std::ofstream logFile;
    std::unique_ptr<Example> example = createExample(logFile);

    WindowManager windowMgr;
    windowMgr.init(caMetalLayer);
    windowMgr.run(example.get());
    windowMgr.cleanup();

    return 0;
}
#endif
