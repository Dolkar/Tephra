#include "vulkan/interface.hpp"
#include "job/accesses.hpp"
#include "job/compute_pass.hpp"
#include "job/resource_pool_container.hpp"
#include "device/device_container.hpp"
#include "common_impl.hpp"
#include <tephra/compute.hpp>

namespace tp {

void ComputeList::beginRecording(CommandPool* commandPool) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "beginRecording", nullptr);

    TEPHRA_ASSERT(vkCommandBufferHandle.isNull());
    TEPHRA_ASSERTD(vkFutureCommandBuffer != nullptr, "beginRecording() of inline ComputeList");

    vkCommandBufferHandle = commandPool->acquirePrimaryCommandBuffer(debugTarget->getObjectName());

    // Record to a fresh primary command buffer
    VkCommandBufferBeginInfo beginInfo;
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.pNext = nullptr;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    beginInfo.pInheritanceInfo = nullptr;

    throwRetcodeErrors(vkiCommands->beginCommandBuffer(vkCommandBufferHandle, &beginInfo));
}

void ComputeList::endRecording() {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "endRecording", nullptr);
    throwRetcodeErrors(vkiCommands->endCommandBuffer(vkCommandBufferHandle));

    // The command buffer is ready to be used now
    *vkFutureCommandBuffer = vkCommandBufferHandle;
}

void ComputeList::cmdBindComputePipeline(const Pipeline& pipeline) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdBindComputePipeline", nullptr);
    vkiCommands->cmdBindPipeline(vkCommandBufferHandle, VK_PIPELINE_BIND_POINT_COMPUTE, pipeline.vkGetPipelineHandle());
}

void ComputeList::cmdDispatch(uint32_t groupCountX, uint32_t groupCountY, uint32_t groupCountZ) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDispatch", nullptr);
    vkiCommands->cmdDispatch(vkCommandBufferHandle, groupCountX, groupCountY, groupCountZ);
}

void ComputeList::cmdDispatchIndirect(const BufferView& dispatchParamBuffer) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdDispatchIndirect", nullptr);
    uint64_t viewOffset;
    VkBufferHandle vkBufferHandle = dispatchParamBuffer.vkResolveBufferHandle(&viewOffset);
    vkiCommands->cmdDispatchIndirect(vkCommandBufferHandle, vkBufferHandle, viewOffset);
}

void ComputeList::cmdPipelineBarrier(
    ArrayParameter<const std::pair<ComputeAccessMask, ComputeAccessMask>> dependencies) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdPipelineBarrier", nullptr);

    // Convert to Vulkan execution and memory barriers
    VkPipelineStageFlags srcExecStageMask = 0;
    VkPipelineStageFlags dstExecStageMask = 0;
    ScratchVector<VkMemoryBarrier> memoryBarriers;
    memoryBarriers.reserve(dependencies.size());

    for (const std::pair<ComputeAccessMask, ComputeAccessMask> dependency : dependencies) {
        VkPipelineStageFlags srcStageMask, dstStageMask;
        VkAccessFlags srcAccessMask, dstAccessMask;
        bool srcIsAtomic, dstIsAtomic;
        convertComputeAccessToVkAccess(dependency.first, &srcStageMask, &srcAccessMask, &srcIsAtomic);
        convertComputeAccessToVkAccess(dependency.second, &dstStageMask, &dstAccessMask, &dstIsAtomic);
        bool srcIsReadOnly = (srcAccessMask & WriteAccessBits) == 0;
        bool dstIsReadOnly = (dstAccessMask & WriteAccessBits) == 0;

        if ((srcIsReadOnly && dstIsReadOnly) || (srcIsAtomic && dstIsAtomic))
            continue;

        // Add execution dependency
        srcExecStageMask |= srcStageMask;
        dstExecStageMask |= dstStageMask;

        if (!srcIsReadOnly) {
            // Write -> Read and Write -> Write may need a memory barrier
            memoryBarriers.emplace_back();
            VkMemoryBarrier& memoryBarrier = memoryBarriers.back();
            memoryBarrier.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            memoryBarrier.pNext = nullptr;
            memoryBarrier.srcAccessMask = srcAccessMask;
            memoryBarrier.dstAccessMask = dstAccessMask;
        }
    }

    vkiCommands->cmdPipelineBarrier(
        vkCommandBufferHandle,
        srcExecStageMask,
        dstExecStageMask,
        0,
        static_cast<uint32_t>(memoryBarriers.size()),
        memoryBarriers.data(),
        0,
        nullptr,
        0,
        nullptr);
}

void ComputeList::cmdWriteTimestamp(const TimestampQuery& query, PipelineStage stage) {
    TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "cmdWriteTimestamp", nullptr);
    jobData->resourcePoolImpl->getParentDeviceImpl()->getQueryManager()->sampleTimestampQuery(
        vkCommandBufferHandle, QueryManager::getQueryHandle(query), stage, 1, jobData->semaphores.jobSignal);
}

ComputeList::ComputeList(ComputeList&&) noexcept = default;

ComputeList& ComputeList::operator=(ComputeList&&) noexcept = default;

ComputeList::~ComputeList() = default;

ComputeList::ComputeList(
    const VulkanCommandInterface* vkiCommands,
    const JobData* jobData,
    VkCommandBufferHandle vkInlineCommandBuffer,
    DebugTarget debugTarget)
    : CommandList(vkiCommands, jobData, VK_PIPELINE_BIND_POINT_COMPUTE, vkInlineCommandBuffer, std::move(debugTarget)) {
}

ComputeList::ComputeList(
    const VulkanCommandInterface* vkiCommands,
    const JobData* jobData,
    VkCommandBufferHandle* vkFutureCommandBuffer,
    DebugTarget debugTarget)
    : CommandList(vkiCommands, jobData, VK_PIPELINE_BIND_POINT_COMPUTE, vkFutureCommandBuffer, std::move(debugTarget)) {
}

}
