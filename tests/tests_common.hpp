#pragma once

#define NOMINMAX
#include "CppUnitTest.h"
#include <tephra/tephra.hpp>
#include <tephra/utils/standard_report_handler.hpp>
#include <iostream>
#include <fstream>
#include <vector>
#include <random>

using namespace Microsoft::VisualStudio::CppUnitTestFramework;

template <typename T>
constexpr T roundUpToMultiple(T v, T m) {
    return ((v + m - 1) / m) * m;
}

class TestReportHandler : public tp::DebugReportHandler {
public:
    TestReportHandler() {}
    virtual void callbackMessage(const tp::DebugMessage& message) noexcept override {
        std::string formattedMessage = tp::utils::StandardReportHandler::formatDebugMessage(message);
        Logger::WriteMessage(formattedMessage.c_str());
        Logger::WriteMessage("\n");

        if (message.severity == tp::DebugMessageSeverity::Error) {
            tp::utils::StandardReportHandler::triggerDebugTrap();
        }
    }

    virtual void callbackRuntimeError(const tp::DebugMessageContext& context, const tp::RuntimeError& error) noexcept
        override {
        std::string formattedError = tp::utils::StandardReportHandler::formatRuntimeError(context, error);
        Logger::WriteMessage(formattedError.c_str());
        tp::utils::StandardReportHandler::triggerDebugTrap();
    }

    virtual void callbackStatisticEvent(const tp::StatisticEventInfo& eventInfo) override {
        lastCounterValues[static_cast<int>(eventInfo.type)] = eventInfo.counter;
    }

    virtual tp::DebugMessageSeverityMask getSeverityMask() const noexcept override {
        return tp::DebugMessageSeverity::Warning | tp::DebugMessageSeverity::Error;
    }

    virtual tp::DebugMessageTypeMask getTypeMask() const noexcept override {
        return tp::DebugMessageType::Performance | tp::DebugMessageType::Validation | tp::DebugMessageType::General;
    }

    uint64_t getLastStatistic(tp::StatisticEventType eventType) {
        return lastCounterValues[static_cast<int>(eventType)];
    }

protected:
    std::array<uint64_t, tp::StatisticEventTypeEnumView::size()> lastCounterValues{};
};

inline tp::ShaderModule loadShader(tp::Device* device, std::string path) {
    std::ifstream fileStream{ path, std::ios::binary | std::ios::in | std::ios::ate };

    if (!fileStream.is_open()) {
        throw std::runtime_error("Shader '" + path + "' not found.");
    }

    auto byteSize = fileStream.tellg();
    if (byteSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Shader '" + path + "' has incorrect size.");
    }
    std::vector<uint32_t> shaderCode;
    shaderCode.resize(byteSize / sizeof(uint32_t));

    fileStream.seekg(0);
    fileStream.read(reinterpret_cast<char*>(shaderCode.data()), byteSize);

    return device->createShaderModule(tp::view(shaderCode));
}

// Context for shared setup and handles for the purposes of integration tests
struct TephraContext {
    struct QueueContext {
        tp::DeviceQueue queue;
        tp::OwningPtr<tp::JobResourcePool> jobResourcePool;

        QueueContext(tp::DeviceQueue queue) : queue(queue) {}
    };

    void initialize(bool requirePresentCapabilities) {
        // Create application
        std::vector<const char*> appExtensions = { tp::ApplicationExtension::EXT_DebugUtils };
        if (requirePresentCapabilities) {
            appExtensions.push_back(tp::ApplicationExtension::KHR_Surface);
        }

        auto validationSetup = tp::VulkanValidationSetup(
            true,
            tp::ValidationFeatureEnable::BestPractices | tp::ValidationFeatureEnable::Synchronization |
                tp::ValidationFeatureEnable::GPUAssisted | tp::ValidationFeatureEnable::GPUAssistedReserveBindingSlot);

        auto appSetup = tp::ApplicationSetup(
            { "TephraIntegrationTests", tp::Version(0, 1, 0) },
            validationSetup,
            &testReportHandler,
            tp::view(appExtensions));
        application = tp::Application::createApplication(appSetup);

        // Pick the first physical device that supports what we need
        std::vector<const char*> deviceExtensions;
        if (requirePresentCapabilities) {
            deviceExtensions.push_back(tp::DeviceExtension::KHR_Swapchain);
        }

        physicalDevice = nullptr;
        for (auto& device : application->getPhysicalDevices()) {
            bool isDeviceSupported = true;
            for (const char* ext : deviceExtensions) {
                if (!device.isExtensionAvailable(ext))
                    isDeviceSupported = false;
            }

            if (isDeviceSupported) {
                physicalDevice = &device;
                break;
            }
        }
        Assert::IsNotNull(physicalDevice);

        tp::DeviceQueue queues[] = { graphicsQueueCtx.queue, asyncCompute0Ctx.queue, asyncCompute1Ctx.queue };

        // Create a device with the requested queues
        auto deviceSetup = tp::DeviceSetup(physicalDevice, tp::view(queues), tp::view(deviceExtensions));
        device = application->createDevice(deviceSetup, "TestDevice");

        // Create a job pool for each queue
        graphicsQueueCtx.jobResourcePool = device->createJobResourcePool({ graphicsQueueCtx.queue });
        asyncCompute0Ctx.jobResourcePool = device->createJobResourcePool({ asyncCompute0Ctx.queue });
        asyncCompute1Ctx.jobResourcePool = device->createJobResourcePool({ asyncCompute1Ctx.queue });
        tp::OverallocationBehavior noOverallocation = tp::OverallocationBehavior::Exact();
        noOverallocateCtx.jobResourcePool = device->createJobResourcePool(
            { noOverallocateCtx.queue, {}, noOverallocation, noOverallocation, noOverallocation });
    }

    void resetJobResourcePools() {
        // Resetting the resource pools is necessary when testing barriers between job-local buffers to make sure
        // buffers from the previous tests aren't aliased, and to test allocation in an isolated setting
        device->waitForIdle();
        graphicsQueueCtx.jobResourcePool->trim();
        asyncCompute0Ctx.jobResourcePool->trim();
        asyncCompute1Ctx.jobResourcePool->trim();
        noOverallocateCtx.jobResourcePool->trim();
    }

    void cleanup() {
        // Have to do it here instead of destructor, because this class lives in a static variable and
        // it can be destroyed before other static variables in Tephra
        asyncCompute1Ctx.jobResourcePool.reset();
        asyncCompute0Ctx.jobResourcePool.reset();
        graphicsQueueCtx.jobResourcePool.reset();
        noOverallocateCtx.jobResourcePool.reset();

        device.reset();
        application.reset();
    }

    uint64_t getLastStatistic(tp::StatisticEventType eventType) {
        return testReportHandler.getLastStatistic(eventType);
    }

    TestReportHandler testReportHandler;
    tp::OwningPtr<tp::Application> application;
    const tp::PhysicalDevice* physicalDevice = nullptr;
    tp::OwningPtr<tp::Device> device;
    std::mt19937 rand32;

    QueueContext graphicsQueueCtx = QueueContext(tp::QueueType::Graphics);
    QueueContext asyncCompute0Ctx = QueueContext(tp::DeviceQueue(tp::QueueType::Compute, 0));
    QueueContext asyncCompute1Ctx = QueueContext(tp::DeviceQueue(tp::QueueType::Compute, 1));
    QueueContext noOverallocateCtx = QueueContext(tp::QueueType::Graphics);
};
