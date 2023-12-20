#pragma once

#include "aliasing_suballocator.hpp"
#include "../buffer_impl.hpp"
#include "../common_impl.hpp"
#include <tephra/job.hpp>
#include <deque>

namespace tp {

class JobLocalBufferImpl {
public:
    JobLocalBufferImpl(
        DebugTarget debugTarget,
        DeviceContainer* deviceImpl,
        BufferSetup setup,
        uint64_t localBufferIndex,
        std::deque<BufferView>* jobPendingBufferViews);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    const BufferSetup& getBufferSetup() const {
        return bufferSetup;
    }

    void assignUnderlyingBuffer(Buffer* buffer, uint64_t offset) {
        underlyingBuffer = buffer;
        underlyingBufferOffset = offset;
    }

    bool hasUnderlyingBuffer() const {
        return underlyingBuffer != nullptr;
    }

    const Buffer* getUnderlyingBuffer() const {
        return underlyingBuffer;
    }

    Buffer* getUnderlyingBuffer() {
        return underlyingBuffer;
    }

    uint64_t getLocalIndex() const {
        return localBufferIndex;
    }

    BufferView getDefaultView() {
        return BufferView(this, 0, bufferSetup.size, Format::Undefined);
    }

    BufferView createTexelView(uint64_t offset, uint64_t size, Format format);

    DeviceAddress getDeviceAddress() const;

    uint64_t getRequiredViewAlignment() const;

    static void createPendingBufferViews(std::deque<BufferView>& jobPendingBufferViews);

    // Translates view of the local buffer to a view of the underlying resource.
    static BufferView getViewToUnderlyingBuffer(const BufferView& bufferView);

    static JobLocalBufferImpl& getBufferImpl(const BufferView& bufferView);

private:
    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    uint64_t localBufferIndex;

    BufferSetup bufferSetup;

    Buffer* underlyingBuffer = nullptr;
    uint64_t underlyingBufferOffset = 0;
    std::deque<BufferView>* jobPendingBufferViews;
};

class JobResourcePoolContainer;

class JobLocalBuffers {
public:
    explicit JobLocalBuffers(JobResourcePoolContainer* resourcePoolImpl) : resourcePoolImpl(resourcePoolImpl) {}

    BufferView acquireNewBuffer(BufferSetup setup, DebugTarget debugTarget);

    void createPendingBufferViews();

    void markBufferUsage(const BufferView& bufferView, uint64_t usageNumber);

    const ResourceUsageRange& getBufferUsage(const BufferView& bufferView) const;

    void clear();

private:
    friend class JobLocalBufferAllocator;

    uint64_t getLocalBufferIndex(const BufferView& bufferView) const;

    JobResourcePoolContainer* resourcePoolImpl;
    std::deque<JobLocalBufferImpl> buffers; // The local buffers implementing access through views
    std::deque<BufferView> pendingBufferViews; // Buffer views that need vkBufferViews assigned
    std::deque<ResourceUsageRange> usageRanges; // The usages of the local buffers within the job
};

}
