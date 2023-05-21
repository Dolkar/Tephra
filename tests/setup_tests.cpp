#include "tests_common.hpp"

namespace TephraIntegrationTests {

// Tests to verify the functionality of setting up a basic Tephra environment. That involves creating an Application,
// choosing a physical device through a Profile, creating a logical device from it and creating JobResourcePools.
TEST_CLASS(SetupTests) {
public:
    TEST_METHOD(MinimalApplication) {
        TestReportHandler debugHandler;

        tp::ApplicationSetup appSetup;
        appSetup.debugReportHandler = &debugHandler;
        tp::OwningPtr<tp::Application> app = tp::Application::createApplication(appSetup);

        Assert::IsFalse(app->vkGetInstanceHandle().isNull());
    }

    TEST_METHOD(ApplicationWithExtensions) {
        TestReportHandler debugHandler;

        std::vector<const char*> appExtensions = { tp::ApplicationExtension::EXT_DebugUtils,
                                                   tp::ApplicationExtension::KHR_Surface };

        tp::ApplicationSetup appSetup;
        appSetup.debugReportHandler = &debugHandler;
        appSetup.extensions = tp::view(appExtensions);
        tp::OwningPtr<tp::Application> app = tp::Application::createApplication(appSetup);

        Assert::IsFalse(app->vkGetInstanceHandle().isNull());
    }

    TEST_METHOD(MinimalDevice) {
        TestReportHandler debugHandler;

        tp::ApplicationSetup appSetup;
        appSetup.debugReportHandler = &debugHandler;
        appSetup.extensions = tp::viewOne(tp::ApplicationExtension::EXT_DebugUtils);
        tp::OwningPtr<tp::Application> app = tp::Application::createApplication(appSetup);

        tp::ArrayView<const tp::PhysicalDevice> physicalDevices = app->getPhysicalDevices();
        Assert::AreNotEqual(static_cast<std::size_t>(0), physicalDevices.size());

        tp::DeviceQueue queues[] = { tp::QueueType::Compute };

        for (const tp::PhysicalDevice& physicalDevice : physicalDevices) {
            auto deviceSetup = tp::DeviceSetup(&physicalDevice, tp::view(queues));
            tp::OwningPtr<tp::Device> device = app->createDevice(deviceSetup);
            Assert::IsFalse(device->vkGetDeviceHandle().isNull());
        }
    }

    TEST_METHOD(DeviceWithQueues) {
        TestReportHandler debugHandler;

        tp::ApplicationSetup appSetup;
        appSetup.debugReportHandler = &debugHandler;
        tp::OwningPtr<tp::Application> app = tp::Application::createApplication(appSetup);

        tp::ArrayView<const tp::PhysicalDevice> physicalDevices = app->getPhysicalDevices();
        Assert::AreNotEqual(static_cast<std::size_t>(0), physicalDevices.size());

        tp::DeviceQueue queues[] = {
            tp::DeviceQueue(tp::QueueType::Graphics),    tp::DeviceQueue(tp::QueueType::Compute, 0),
            tp::DeviceQueue(tp::QueueType::Compute, 1),  tp::DeviceQueue(tp::QueueType::Transfer, 0),
            tp::DeviceQueue(tp::QueueType::Transfer, 1), tp::DeviceQueue(tp::QueueType::Transfer, 2),
            tp::DeviceQueue(tp::QueueType::Transfer, 3)
        };

        auto deviceSetup = tp::DeviceSetup(&physicalDevices[0], view(queues));
        tp::OwningPtr<tp::Device> device = app->createDevice(deviceSetup);
        Assert::IsFalse(device->vkGetDeviceHandle().isNull());

        for (const tp::DeviceQueue& queue : queues) {
            Assert::IsFalse(device->vkGetQueueHandle(queue).isNull());
        }
    }

    TEST_METHOD(JobResourcePool) {
        TestReportHandler debugHandler;

        tp::ApplicationSetup appSetup;
        appSetup.debugReportHandler = &debugHandler;
        tp::OwningPtr<tp::Application> app = tp::Application::createApplication(appSetup);

        tp::ArrayView<const tp::PhysicalDevice> physicalDevices = app->getPhysicalDevices();
        Assert::AreNotEqual(static_cast<std::size_t>(0), physicalDevices.size());

        tp::DeviceQueue queues[] = { tp::DeviceQueue(tp::QueueType::Graphics),
                                     tp::DeviceQueue(tp::QueueType::Compute, 0),
                                     tp::DeviceQueue(tp::QueueType::Transfer, 0),
                                     tp::DeviceQueue(tp::QueueType::Transfer, 1) };

        auto deviceSetup = tp::DeviceSetup(&physicalDevices[0], view(queues));
        tp::OwningPtr<tp::Device> device = app->createDevice(deviceSetup);
        Assert::IsFalse(device->vkGetDeviceHandle().isNull());

        for (const tp::DeviceQueue& queue : queues) {
            auto setup = tp::JobResourcePoolSetup(queue);
            device->createJobResourcePool(setup)->createJob().createCommandPool();
        }
    }
};

}
