#pragma once

#include <tephra/descriptor.hpp>
#include <tephra/pipeline.hpp>
#include <tephra/common.hpp>

namespace tp {

class CommandPool;
class VulkanCommandInterface;
struct JobData;

/// An abstract class that implements common functionality for recording commands inside either a compute or render
/// pass. See tp::ComputeList and tp::RenderList for further details.
///
/// @see @vksymbol{VkCommandBuffer}
class CommandList {
public:
    /// Returns `true` if the command list is null and not valid for use.
    bool isNull() const {
        return vkiCommands == nullptr;
    }

    /// Binds descriptor sets for use in the subsequent commands.
    ///
    /// The provided descriptor sets are bound consecutively, meaning the first descriptor set gets bound to set number
    /// `firstSet`, the second one to `firstSet + 1` and so on.
    ///
    /// @param pipelineLayout
    ///     The tp::PipelineLayout used to program the bindings.
    /// @param descriptorSets
    ///     An array of pointers to tp::DescriptorSet to bind.
    /// @param firstSet
    ///     The set number in the pipeline layout that the first provided descriptor set will be bound to.
    /// @param dynamicOffsets
    ///     The dynamic offsets to be applied to the provided descriptors of types
    ///     tp::DescriptorType::UniformBufferDynamic or tp::DescriptorType::StorageBufferDynamic in the order they
    ///     appear in the descriptor sets. The size of the array must match the number of dynamic buffer descriptors
    ///     to be bound. Each offset moves the entire range of its corresponding buffer view within its parent buffer.
    ///     E.g. a buffer view offset by 64 bytes and a size of 32 bytes will expose bytes 128-159 of the viewed buffer
    ///     when bound with a dynamic offset value of 64.
    /// @remarks
    ///     If the descriptor sets previously bound to numbers up to `firstSet - 1` were bound using a pipeline layout
    ///     compatible up to the set number `firstSet - 1`, then the lower numbered bindings are not disturbed and may
    ///     still be accessed.
    /// @see @vksymbol{vkCmdBindDescriptorSets}
    void cmdBindDescriptorSets(
        const PipelineLayout& pipelineLayout,
        ArrayParameter<const DescriptorSetView> descriptorSets,
        uint32_t firstSet = 0,
        ArrayParameter<const uint32_t> dynamicOffsets = {});

    /// Updates the push constant values using the given pipeline layout.
    /// @param pipelineLayout
    ///     The tp::PipelineLayout used to program the push constants.
    /// @param stageMask
    ///     The stages that will use the push constants in the updated range.
    /// @param value
    ///     The value that the push constants will be updated to. The size of the type determines the size of the
    ///     updated range.
    /// @param offsetBytes
    ///     The offset to the start of the updated range in bytes.
    /// @remarks
    ///     Values outside of the updated range are not disturbed.
    /// @see @vksymbol{vkCmdPushConstants}
    template <typename T>
    void cmdPushConstants(
        const PipelineLayout& pipelineLayout,
        ShaderStageMask stageMask,
        const T& value,
        uint32_t offsetBytes = 0) {
        cmdPushConstants(pipelineLayout, stageMask, &value, sizeof(T), offsetBytes);
    }

    /// Updates the push constant values using the given pipeline layout.
    /// @param pipelineLayout
    ///     The tp::PipelineLayout used to program the push constants.
    /// @param stageMask
    ///     The stages that will use the push constants in the updated range.
    /// @param data
    ///     Pointer to the data that the push constants will be updated to.
    /// @param sizeBytes
    ///     The size of the updated range in bytes.
    /// @param offsetBytes
    ///     The offset to the start of the updated range in bytes.
    /// @remarks
    ///     Values outside of the updated range are not disturbed.
    /// @see @vksymbol{vkCmdPushConstants}
    void cmdPushConstants(
        const PipelineLayout& pipelineLayout,
        ShaderStageMask stageMask,
        const void* data,
        uint32_t sizeBytes,
        uint32_t offsetBytes = 0);

    /// Begins a debug label, marking the following commands until the next tp::CommandList::cmdEndDebugLabel with the
    /// given name and optional color for display in validation and debugging tools.
    /// @param name
    ///     The name of the label.
    /// @param color
    ///     The color of the label. Only used by external tools.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdBeginDebugLabel(const char* name, ArrayParameter<const float> color = {});

    /// Inserts a debug label, marking the following commands with the given name and optional color for display in
    /// validation and debugging tools.
    /// @param name
    ///     The name of the label.
    /// @param color
    ///     The color of the label. Only used by external tools.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdInsertDebugLabel(const char* name, ArrayParameter<const float> color = {});

    /// Ends the last debug label. Must be preceded by tp::CommandList::cmdBeginDebugLabel.
    /// @remarks
    ///     The call will have no effect when tp::ApplicationExtension::EXT_DebugUtils is not enabled.
    void cmdEndDebugLabel();

    /// Returns the associated @vksymbol{VkCommandBuffer} handle if the command list is being recorded, VK_NULL_HANDLE
    /// otherwise.
    VkCommandBufferHandle vkGetCommandBufferHandle() const {
        return vkCommandBufferHandle;
    }

    TEPHRA_MAKE_NONCOPYABLE(CommandList);
    TEPHRA_MAKE_MOVABLE(CommandList);

protected:
    DebugTargetPtr debugTarget;
    const VulkanCommandInterface* vkiCommands;
    const JobData* jobData;
    VkCommandBufferHandle vkCommandBufferHandle;
    VkCommandBufferHandle* vkFutureCommandBuffer;
    VkPipelineBindPoint vkPipelineBindPoint;

    CommandList();

    CommandList(
        const VulkanCommandInterface* vkiCommands,
        const JobData* jobData,
        VkPipelineBindPoint vkPipelineBindPoint,
        VkCommandBufferHandle vkInlineCommandBuffer,
        DebugTarget debugTarget);

    CommandList(
        const VulkanCommandInterface* vkiCommands,
        const JobData* jobData,
        VkPipelineBindPoint vkPipelineBindPoint,
        VkCommandBufferHandle* vkFutureCommandBuffer,
        DebugTarget debugTarget);

    ~CommandList();
};

}
