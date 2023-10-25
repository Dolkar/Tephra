#include "vulkan/interface.hpp"
#include "device/device_container.hpp"
#include "common_impl.hpp"
#include <tephra/command_list.hpp>

namespace tp {

void CommandList::cmdBindDescriptorSets(
    const PipelineLayout& pipelineLayout,
    ArrayParameter<const DescriptorSetView> descriptorSets,
    uint32_t firstSet,
    ArrayParameter<const uint32_t> dynamicOffsets) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBindDescriptorSets", nullptr);

    ScratchVector<VkDescriptorSetHandle> vkDescriptorSetHandles(descriptorSets.size());
    for (uint32_t i = 0; i < descriptorSets.size(); i++) {
        vkDescriptorSetHandles[i] = descriptorSets[i].vkResolveDescriptorSetHandle();
    }

    vkiCommands->cmdBindDescriptorSets(
        vkCommandBufferHandle,
        vkPipelineBindPoint,
        pipelineLayout.vkGetPipelineLayoutHandle(),
        firstSet,
        static_cast<uint32_t>(vkDescriptorSetHandles.size()),
        vkCastTypedHandlePtr(vkDescriptorSetHandles.data()),
        static_cast<uint32_t>(dynamicOffsets.size()),
        dynamicOffsets.data());
}

void CommandList::cmdPushConstants(
    const PipelineLayout& pipelineLayout,
    ShaderStageMask stageMask,
    const void* data,
    uint32_t sizeBytes,
    uint32_t offsetBytes) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdPushConstants", nullptr);
    vkiCommands->cmdPushConstants(
        vkCommandBufferHandle,
        pipelineLayout.vkGetPipelineLayoutHandle(),
        vkCastConvertibleEnum(static_cast<ShaderStage>(stageMask)),
        offsetBytes,
        sizeBytes,
        data);
}

VkDebugUtilsLabelEXT makeDebugLabel(const char* name, ArrayParameter<const float> color) {
    VkDebugUtilsLabelEXT label;
    label.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
    label.pNext = nullptr;
    label.pLabelName = name;
    if (color.size() >= 4)
        memcpy(label.color, color.data(), sizeof(float) * 4);
    else
        memset(label.color, 0, sizeof(float) * 4);
    return label;
}

void CommandList::cmdBeginDebugLabel(const char* name, ArrayParameter<const float> color) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBeginDebugLabel", nullptr);
    if (vkiCommands->cmdBeginDebugUtilsLabelEXT != nullptr) {
        VkDebugUtilsLabelEXT label = makeDebugLabel(name, color);
        vkiCommands->cmdBeginDebugUtilsLabelEXT(vkCommandBufferHandle, &label);
    }
}

void CommandList::cmdInsertDebugLabel(const char* name, ArrayParameter<const float> color) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdInsertDebugLabel", nullptr);
    if (vkiCommands->cmdBeginDebugUtilsLabelEXT != nullptr) {
        VkDebugUtilsLabelEXT label = makeDebugLabel(name, color);
        vkiCommands->cmdInsertDebugUtilsLabelEXT(vkCommandBufferHandle, &label);
    }
}

void CommandList::cmdEndDebugLabel() {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdEndDebugLabel", nullptr);
    if (vkiCommands->cmdEndDebugUtilsLabelEXT != nullptr) {
        vkiCommands->cmdEndDebugUtilsLabelEXT(vkCommandBufferHandle);
    }
}

CommandList::CommandList(CommandList&&) noexcept = default;

CommandList& CommandList::operator=(CommandList&&) noexcept = default;

CommandList::~CommandList() = default;

CommandList::CommandList()
    : debugTarget(DebugTarget::makeSilent()),
      vkiCommands(nullptr),
      vkCommandBufferHandle(),
      vkFutureCommandBuffer(nullptr),
      vkPipelineBindPoint(VK_PIPELINE_BIND_POINT_COMPUTE) {}

CommandList::CommandList(
    const VulkanCommandInterface* vkiCommands,
    VkPipelineBindPoint vkPipelineBindPoint,
    VkCommandBufferHandle vkInlineCommandBuffer,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      vkiCommands(vkiCommands),
      vkCommandBufferHandle(vkInlineCommandBuffer),
      vkFutureCommandBuffer(nullptr),
      vkPipelineBindPoint(vkPipelineBindPoint) {}

CommandList::CommandList(
    const VulkanCommandInterface* vkiCommands,
    VkPipelineBindPoint vkPipelineBindPoint,
    VkCommandBufferHandle* vkFutureCommandBuffer,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      vkiCommands(vkiCommands),
      vkCommandBufferHandle(),
      vkFutureCommandBuffer(vkFutureCommandBuffer),
      vkPipelineBindPoint(vkPipelineBindPoint) {}

}
