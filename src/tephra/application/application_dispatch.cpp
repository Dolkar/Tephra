
#include "application_container.hpp"
#include "../common_impl.hpp"
#include <tephra/application.hpp>
#include <tephra/physical_device.hpp>

namespace tp {

constexpr const char* ApplicationTypeName = "Application";

VulkanValidationSetup::VulkanValidationSetup(bool enable, ValidationFeatureMask features)
    : enable(enable), features(features) {}

ApplicationIdentifier::ApplicationIdentifier(
    const char* applicationName,
    Version applicationVersion,
    const char* engineName,
    Version engineVersion)
    : applicationName(applicationName),
      applicationVersion(applicationVersion),
      engineName(engineName),
      engineVersion(engineVersion) {}

ApplicationSetup::ApplicationSetup(
    ApplicationIdentifier applicationIdentifier,
    VulkanValidationSetup vulkanValidation,
    DebugReportHandler* debugReportHandler,
    ArrayView<const char* const> extensions,
    ArrayView<const char* const> instanceLayers,
    Version apiVersion,
    void* vkCreateInfoExtPtr)
    : applicationIdentifier(std::move(applicationIdentifier)),
      vulkanValidation(std::move(vulkanValidation)),
      debugReportHandler(debugReportHandler),
      extensions(extensions),
      instanceLayers(instanceLayers),
      apiVersion(apiVersion),
      vkCreateInfoExtPtr(vkCreateInfoExtPtr) {}

bool Application::isExtensionAvailable(const char* extension) {
    const VulkanGlobals* vulkanGlobals = VulkanGlobals::get();
    return vulkanGlobals->isInstanceExtensionAvailable(extension);
}

bool Application::isLayerAvailable(const char* layer) {
    const VulkanGlobals* vulkanGlobals = VulkanGlobals::get();
    return vulkanGlobals->isInstanceLayerAvailable(layer);
}

OwningPtr<Application> Application::createApplication(const ApplicationSetup& appSetup) {
    auto debugReporter = std::make_unique<DebugReporter>(appSetup.debugReportHandler);
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(debugReporter.get(), nullptr, "createApplication", nullptr);

    auto debugTarget = DebugTarget(debugReporter.get(), ApplicationTypeName, nullptr);
    return OwningPtr<Application>(new ApplicationContainer(appSetup, std::move(debugReporter), std::move(debugTarget)));
}

OwningPtr<Application> Application::createApplication(
    VkInstanceHandle vkInstanceHandle,
    DebugReportHandler* debugReportHandler) {
    auto debugReporter = std::make_unique<DebugReporter>(debugReportHandler);
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(debugReporter.get(), nullptr, "createApplication", nullptr);

    if constexpr (TephraValidationEnabled) {
        if (vkInstanceHandle.isNull()) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "'vkInstanceHandle' is a null handle.");
        }
    }

    auto debugTarget = DebugTarget(debugReporter.get(), ApplicationTypeName, nullptr);
    return OwningPtr<Application>(
        new ApplicationContainer(vkInstanceHandle, std::move(debugReporter), std::move(debugTarget)));
}

ArrayView<const PhysicalDevice> Application::getPhysicalDevices() const {
    auto appImpl = static_cast<const ApplicationContainer*>(this);
    return appImpl->getInstance()->getPhysicalDevices();
}

VkInstanceHandle Application::vkGetInstanceHandle() const {
    return static_cast<const ApplicationContainer*>(this)->getInstance()->vkGetInstanceHandle();
}

PFN_vkVoidFunction Application::vkLoadInstanceProcedure(const char* procedureName) const {
    const VulkanGlobals* vulkanGlobals = VulkanGlobals::get();
    return vulkanGlobals->loadInstanceProcedure(vkGetInstanceHandle(), procedureName);
}

Version Application::getApiVersion() {
    return Version();
}

ApplicationContainer::~ApplicationContainer() {
    TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(getDebugTarget());
    if (debugReporter != nullptr) {
        debugReporter->unbindInstance();
    }
}

}
