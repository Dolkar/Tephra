#pragma once

#include "vulkan_globals.hpp"
#include "instance.hpp"
#include "../debug_reporter.hpp"
#include "../common_impl.hpp"
#include <tephra/application.hpp>

namespace tp {

class ApplicationContainer : public Application {
public:
    ApplicationContainer(
        const ApplicationSetup& appSetup,
        std::unique_ptr<DebugReporter> debugReporter,
        DebugTarget debugTarget)
        : debugReporter(std::move(debugReporter)), debugTarget(std::move(debugTarget)), instance(appSetup) {
        bindInstanceToReporter();
    }

    ApplicationContainer(
        VkInstanceHandle vkInstanceHandle,
        std::unique_ptr<DebugReporter> debugReporter,
        DebugTarget debugTarget)
        : debugReporter(std::move(debugReporter)), debugTarget(std::move(debugTarget)), instance(vkInstanceHandle) {
        bindInstanceToReporter();
    }

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    const DebugReporter* getDebugReporter() const {
        return debugReporter.get();
    }

    DebugReporter* getDebugReporter() {
        return debugReporter.get();
    }

    const Instance* getInstance() const {
        return &instance;
    }

    Instance* getInstance() {
        return &instance;
    }

    TEPHRA_MAKE_NONCOPYABLE(ApplicationContainer);
    TEPHRA_MAKE_NONMOVABLE(ApplicationContainer);
    ~ApplicationContainer();

private:
    const std::unique_ptr<DebugReporter> debugReporter;
    DebugTarget debugTarget;
    Instance instance;

    void bindInstanceToReporter() {
        if (debugReporter != nullptr) {
            debugReporter->bindInstance(instance.vkGetInstanceHandle());
        }
    }
};

}
