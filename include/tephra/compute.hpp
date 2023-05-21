#pragma once

#include <tephra/command_list.hpp>
#include <tephra/common.hpp>
#include <functional>

namespace tp {

/// Describes the kind of resource access from the compute pipeline.
/// @see tp::DescriptorType for classification of descriptors into Storage, Sampled and Uniform.
/// @see tp::BufferComputeAccess
/// @see tp::BufferImageAccess
enum class ComputeAccess : uint64_t {
    /// Compute shader read access through storage descriptors.
    ComputeShaderStorageRead = 1 << 0,
    /// Compute shader write access through storage descriptors.
    ComputeShaderStorageWrite = 1 << 1,
    /// Compute shader atomic write access through storage descriptors.
    ComputeShaderStorageAtomic = 1 << 2,
    /// Compute shader read access through sampled descriptors.
    ComputeShaderSampledRead = 1 << 3,
    /// Compute shader read access through uniform buffer descriptors.
    ComputeShaderUniformRead = 1 << 4
};
TEPHRA_MAKE_ENUM_BIT_MASK(ComputeAccessMask, ComputeAccess);

/// Provides an interface to directly record compute commands into a Vulkan @vksymbol{VkCommandBuffer}
/// inside a compute pass.
///
/// The behavior and expected usage differs depending on the variant of the `commandRecording` parameter
/// passed to tp::Job::cmdExecuteComputePass.
///
/// If the list was provided through the tp::ArrayView<tp::ComputeList> variant, then
/// tp::ComputeList::beginRecording must be called before the first and tp::ComputeList::endRecording after
/// the last recorded command.
///
/// If the list was provided as a parameter to tp::ComputeInlineCallback using the function callback
/// variant, tp::ComputeList::beginRecording and tp::ComputeList::endRecording must not be called.
/// Any changed state (cmdBind..., cmdSet...) persists between all inline lists within the same tp::Job.
///
/// @see tp::Job::cmdExecuteComputePass
/// @see @vksymbol{VkCommandBuffer}
class ComputeList : public CommandList {
public:
    /// Constructs a null tp::ComputeList.
    ComputeList() : CommandList(){};

    /// Begins recording commands to the list, using the given command pool.
    /// @param commandPool
    ///     The command pool to use for memory allocations for command recording. Cannot be nullptr.
    /// @remarks
    ///     The parent tp::Job must be in an enqueued state.
    /// @remarks
    ///     The tp::ComputeList must not have been received as a parameter to tp::ComputeInlineCallback.
    /// @remarks
    ///     The tp::CommandPool is not thread safe. Only one thread may be recording commands
    ///     using the same pool at a time.
    /// @see @vksymbol{vkBeginCommandBuffer}
    void beginRecording(CommandPool* commandPool);

    /// Ends recording commands to the list. No other methods can be called after this point.
    /// @remarks
    ///     The parent tp::Job must be in an enqueued state.
    /// @remarks
    ///     The tp::ComputeList must not have been received as a parameter to tp::ComputeInlineCallback.
    /// @see @vksymbol{vkEndCommandBuffer}
    void endRecording();

    /// Binds a compute tp::Pipeline for use in the subsequent dispatch commands.
    /// @param pipeline
    ///     The pipeline object to bind.
    /// @remarks
    ///     If the pipeline was created with a tp::PipelineLayout whose descriptor set
    ///     layouts are compatible with the pipeline layout of sets previously bound with
    ///     tp::ComputeList::cmdBindDescriptorSets, then the descriptor sets are not disturbed
    ///     and may still be accessed, up to the first incompatible set number.
    /// @see @vksymbol{vkCmdBindPipeline}
    void cmdBindComputePipeline(const Pipeline& pipeline);

    /// Records a dispatch of `groupCountX * groupCountY * groupCountZ` compute workgroups.
    /// @remarks
    ///     The parameters describe the number of workgroups, *NOT* the number of invocations. The total number of
    ///     invocations in each dimension will be the number of workgroups multiplied with the workgroup side as defined
    ///     by the compute shader in the currently bound pipeline.
    /// @see @vksymbol{vkCmdDispatch}
    void cmdDispatch(uint32_t groupCountX, uint32_t groupCountY = 1, uint32_t groupCountZ = 1);

    /// Records an indirect dispatch with the parameters sourced from a buffer.
    /// @param dispatchParamBuffer
    ///     The buffer containing dispatch parameters in the form of @vksymbol{VkDispatchIndirectCommand}.
    /// @see @vksymbol{vkCmdDispatchIndirect}
    void cmdDispatchIndirect(const BufferView& dispatchParamBuffer);

    /// Inserts a pipeline barrier that synchronizes the given dependencies of future commands on past commands.
    /// @param dependencies
    ///     The global execution and memory dependencies to synchronize.
    /// @see @vksymbol{vkCmdPipelineBarrier}
    void cmdPipelineBarrier(ArrayParameter<const std::pair<ComputeAccessMask, ComputeAccessMask>> dependencies);

    TEPHRA_MAKE_NONCOPYABLE(ComputeList);
    TEPHRA_MAKE_MOVABLE(ComputeList);
    virtual ~ComputeList();

private:
    friend class Job;
    friend class ComputePass;

    ComputeList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle vkInlineCommandBuffer,
        DebugTarget debugTarget);

    ComputeList(
        const VulkanCommandInterface* vkiCommands,
        VkCommandBufferHandle* vkFutureCommandBuffer,
        DebugTarget debugTarget);
};

/// Represents an access to a range of tp::BufferView from a compute pipeline.
/// @see tp::ComputePassSetup
struct BufferComputeAccess {
    BufferView buffer;
    ComputeAccessMask accessMask;

    /// @param buffer
    ///     The buffer view being accessed.
    /// @param accessMask
    ///     The type of accesses that are made.
    BufferComputeAccess(BufferView buffer, ComputeAccessMask accessMask)
        : buffer(std::move(buffer)), accessMask(accessMask) {}
};

/// Represents an access to a range of tp::ImageView from a compute pipeline.
/// @see tp::ComputePassSetup
struct ImageComputeAccess {
    ImageView image;
    ImageSubresourceRange range;
    ComputeAccessMask accessMask;

    /// @param image
    ///     The image view being accessed.
    /// @param accessMask
    ///     The type of accesses that are made.
    ImageComputeAccess(ImageView image, ComputeAccessMask accessMask)
        : ImageComputeAccess(std::move(image), image.getWholeRange(), accessMask) {}

    /// @param image
    ///     The image view being accessed.
    /// @param range
    ///     The accessed range of the image view.
    /// @param accessMask
    ///     The type of accesses that are made.
    ImageComputeAccess(ImageView image, ImageSubresourceRange range, ComputeAccessMask accessMask)
        : image(std::move(image)), range(range), accessMask(accessMask) {}
};

/// Used as configuration for executing a compute pass.
/// @see tp::Job::cmdExecuteComputePass
struct ComputePassSetup {
    ArrayView<const BufferComputeAccess> bufferAccesses;
    ArrayView<const ImageComputeAccess> imageAccesses;

    /// @param bufferAccesses
    ///     The buffer accesses that will be made within the compute pass.
    /// @param imageAccesses
    ///     The image accesses that will be made within the compute pass.
    ComputePassSetup(
        ArrayView<const BufferComputeAccess> bufferAccesses,
        ArrayView<const ImageComputeAccess> imageAccesses)
        : bufferAccesses(bufferAccesses), imageAccesses(imageAccesses) {}
};

/// The type of the user-provided function callback for recording commands to a compute pass inline.
/// @see tp::Job::cmdExecuteComputePass
using ComputeInlineCallback = std::function<void(ComputeList&)>;

}
