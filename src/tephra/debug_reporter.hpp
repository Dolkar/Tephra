#pragma once

#include "application/vulkan_globals.hpp"
#include <tephra/debug_handler.hpp>

namespace tp {

class DebugReporter {
public:
    explicit DebugReporter(DebugReportHandler* debugHandler);

    void bindInstance(VkInstanceHandle vkInstanceHandle);
    void unbindInstance();

    void reportMessage(const DebugMessage& message) const noexcept;
    void reportRuntimeError(const DebugMessageContext& context, const RuntimeError& error) const noexcept;
    void reportStatisticEvent(const StatisticEventInfo& eventInfo) const;

    TEPHRA_MAKE_NONCOPYABLE(DebugReporter);
    TEPHRA_MAKE_NONMOVABLE(DebugReporter);

    ~DebugReporter() {
        unbindInstance();
    }

private:
    DebugReportHandler* debugHandler;
    DebugMessageSeverityMask messageSeverityMask;
    DebugMessageTypeMask messageTypeMask;
    VkInstanceHandle vkInstanceHandle;
    VulkanDebugUtilsMessengerInterfaceEXT vkiDebugUtils;
    VkDebugUtilsMessengerEXT vkDebugMessenger = VK_NULL_HANDLE;
};

}
