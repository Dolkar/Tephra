
#include "debug_reporter.hpp"
#include "vulkan/interface.hpp"

namespace tp {

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT vkMessageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT vkMessageType,
    const VkDebugUtilsMessengerCallbackDataEXT* vkCallbackData,
    void* pUserData) {
    DebugMessageSeverity messageSeverity = vkCastConvertibleEnum(vkMessageSeverity);

    // Convert message type bitmask to just a single type
    DebugMessageType messageType = DebugMessageType::General;
    if (containsAllBits(vkMessageType, VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT))
        messageType = DebugMessageType::Performance;
    if (containsAllBits(vkMessageType, VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT))
        messageType = DebugMessageType::Validation;

    const char* message = vkCallbackData->pMessage;

    auto context = DebugContext::getCurrentContext();
    if (context != nullptr) {
        reportDebugMessageRaw(messageSeverity, messageType, message, vkCallbackData);
    } else {
        // Handle case where tephra debug reporting is disabled, but Vulkan one enabled
        auto reporter = static_cast<DebugReporter*>(pUserData);
        DebugMessage debugMessage;
        debugMessage.severity = messageSeverity;
        debugMessage.type = messageType;
        debugMessage.context = DebugMessageContext{ 0 };
        debugMessage.message = message;
        debugMessage.vkCallbackData = vkCallbackData;
        reporter->reportMessage(debugMessage);
    }

    // Debug messenger callback should always return false
    return false;
}

DebugReporter::DebugReporter(DebugReportHandler* debugHandler)
    : debugHandler(debugHandler),
      messageSeverityMask(debugHandler ? debugHandler->getSeverityMask() : DebugMessageSeverityMask::None()),
      messageTypeMask(debugHandler ? debugHandler->getTypeMask() : DebugMessageTypeMask::None()) {}

void DebugReporter::bindInstance(VkInstanceHandle vkInstanceHandle) {
    unbindInstance();
    this->vkInstanceHandle = vkInstanceHandle;

    const VulkanGlobals* vulkanGlobals = VulkanGlobals::get();

    vkiDebugUtils = vulkanGlobals->loadInstanceInterface<VulkanDebugUtilsMessengerInterfaceEXT>(vkInstanceHandle);

    if (debugHandler != nullptr && vkiDebugUtils.isLoaded()) { // Ignore if the relevant extension wasn't loaded
        VkDebugUtilsMessengerCreateInfoEXT messengerCreateInfo;
        messengerCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        messengerCreateInfo.pNext = nullptr;
        messengerCreateInfo.flags = 0;
        messengerCreateInfo.messageSeverity = vkCastConvertibleEnumMask(messageSeverityMask);
        messengerCreateInfo.messageType = vkCastConvertibleEnumMask(messageTypeMask);
        messengerCreateInfo.pfnUserCallback = debugMessengerCallback;
        messengerCreateInfo.pUserData = static_cast<void*>(this);

        throwRetcodeErrors(vkiDebugUtils.createDebugUtilsMessengerEXT(
            vkInstanceHandle, &messengerCreateInfo, nullptr, &vkDebugMessenger));
    }
}

void DebugReporter::unbindInstance() {
    if (!vkInstanceHandle.isNull()) {
        if (vkiDebugUtils.isLoaded()) {
            vkiDebugUtils.destroyDebugUtilsMessengerEXT(vkInstanceHandle, vkDebugMessenger, nullptr);
            vkDebugMessenger = VK_NULL_HANDLE;
        }
        vkInstanceHandle = VkInstanceHandle();
    }
}

void DebugReporter::reportMessage(const DebugMessage& message) const noexcept {
    if (debugHandler != nullptr && messageSeverityMask.contains(message.severity) &&
        messageTypeMask.contains(message.type)) {
        debugHandler->callbackMessage(message);
    }
}

void DebugReporter::reportRuntimeError(const DebugMessageContext& context, const RuntimeError& error) const noexcept {
    if (debugHandler != nullptr) {
        debugHandler->callbackRuntimeError(context, error);
    }
}

void DebugReporter::reportStatisticEvent(const StatisticEventInfo& eventInfo) const {
    if (debugHandler != nullptr) {
        debugHandler->callbackStatisticEvent(eventInfo);
    }
}

}
