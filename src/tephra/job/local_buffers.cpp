#include "local_buffers.hpp"
#include "resource_pool_container.hpp"
#include "../device/device_container.hpp"

namespace tp {

JobLocalBufferImpl::JobLocalBufferImpl(
    DeviceContainer* deviceImpl,
    BufferSetup setup,
    uint64_t localBufferIndex,
    std::deque<BufferView>* jobPendingBufferViews,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      deviceImpl(deviceImpl),
      localBufferIndex(localBufferIndex),
      bufferSetup(std::move(setup)),
      jobPendingBufferViews(jobPendingBufferViews) {}

BufferView JobLocalBufferImpl::createTexelView(uint64_t offset, uint64_t size, Format format) {
    if (hasUnderlyingBuffer()) {
        // Just create a view of the underlying buffer
        offset += underlyingBufferOffset;
        return static_cast<BufferImpl*>(underlyingBuffer)->createTexelView_(offset, size, format);
    } else {
        // No buffer assigned yet, so make it a view of this local buffer and add it to the collective pending list in
        // JobLocalBuffers for when the underlying buffer gets assigned
        BufferView view = BufferView(this, offset, size, format);
        jobPendingBufferViews->push_back(view);
        return view;
    }
}

VkDeviceAddress JobLocalBufferImpl::getDeviceAddress() const {
    if (hasUnderlyingBuffer()) {
        return underlyingBuffer->getDeviceAddress() + underlyingBufferOffset;
    } else {
        return 0;
    }
}

uint64_t JobLocalBufferImpl::getRequiredViewAlignment() const {
    return BufferImpl::getRequiredViewAlignment_(deviceImpl, bufferSetup.usage);
}

void JobLocalBufferImpl::createPendingBufferViews(std::deque<BufferView>& jobPendingBufferViews) {
    for (BufferView& bufferView : jobPendingBufferViews) {
        JobLocalBufferImpl& localBuffer = getBufferImpl(bufferView);
        // It may not have been assigned an underlying buffer if it's never used
        if (!localBuffer.hasUnderlyingBuffer())
            continue;

        uint64_t finalOffset = localBuffer.underlyingBufferOffset + bufferView.offset;
        static_cast<BufferImpl*>(localBuffer.underlyingBuffer)
            ->createTexelView_(finalOffset, bufferView.size, bufferView.format);
    }
}

BufferView JobLocalBufferImpl::getViewToUnderlyingBuffer(const BufferView& bufferView) {
    TEPHRA_ASSERT(bufferView.viewsJobLocalBuffer());
    JobLocalBufferImpl& localBuffer = getBufferImpl(bufferView);
    TEPHRA_ASSERT(localBuffer.hasUnderlyingBuffer());

    BufferImpl* underlyingBuffer = static_cast<BufferImpl*>(localBuffer.underlyingBuffer);
    uint64_t finalOffset = localBuffer.underlyingBufferOffset + bufferView.offset;
    return BufferView(underlyingBuffer, finalOffset, bufferView.size, bufferView.format);
}

JobLocalBufferImpl& JobLocalBufferImpl::getBufferImpl(const BufferView& bufferView) {
    TEPHRA_ASSERT(bufferView.viewsJobLocalBuffer());
    return *std::get<JobLocalBufferImpl*>(bufferView.buffer);
}

BufferView JobLocalBuffers::acquireNewBuffer(BufferSetup setup, const char* debugName) {
    DeviceContainer* deviceImpl = resourcePoolImpl->getParentDeviceImpl();
    DebugTarget debugTarget = DebugTarget(resourcePoolImpl->getDebugTarget(), "JobLocalBuffer", debugName);
    buffers.emplace_back(deviceImpl, setup, buffers.size(), &pendingBufferViews, std::move(debugTarget));
    usageRanges.emplace_back();
    return buffers.back().getDefaultView();
}

void JobLocalBuffers::createPendingBufferViews() {
    JobLocalBufferImpl::createPendingBufferViews(pendingBufferViews);
    pendingBufferViews.clear();
}

void JobLocalBuffers::markBufferUsage(const BufferView& bufferView, uint64_t usageNumber) {
    uint64_t bufferIndex = getLocalBufferIndex(bufferView);
    usageRanges[bufferIndex].update(usageNumber);
}

uint64_t JobLocalBuffers::getLocalBufferIndex(const BufferView& bufferView) const {
    return JobLocalBufferImpl::getBufferImpl(bufferView).getLocalIndex();
}

const ResourceUsageRange& JobLocalBuffers::getBufferUsage(const BufferView& bufferView) const {
    uint64_t bufferIndex = getLocalBufferIndex(bufferView);
    return usageRanges[bufferIndex];
}

void JobLocalBuffers::clear() {
    buffers.clear();
    pendingBufferViews.clear();
    usageRanges.clear();
}

}
