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
        DeviceContainer* deviceImpl,
        BufferSetup setup,
        uint64_t localBufferIndex,
        std::deque<BufferView>* jobPendingBufferViews,
        DebugTarget debugTarget);

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

    VkDeviceAddress getDeviceAddress() const;

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

// Once stored, it is not guaranteed that the persistent parent objects (BufferImpl) of views will be kept alive,
// so they need to be resolved immediately. But job-local resources need to be resolved later after they actually get
// created. This class handles resolving both at the right time.
class StoredBufferView {
public:
    StoredBufferView(const BufferView& view) : storedView(store(view)) {}

    bool isNull() const {
        if (std::holds_alternative<BufferView>(storedView)) {
            return std::get<BufferView>(storedView).isNull();
        } else {
            return std::get<ResolvedView>(storedView).vkBufferHandle.isNull();
        }
    }

    uint64_t getSize() {
        resolve();
        return std::get<ResolvedView>(storedView).size;
    }

    VkBufferHandle vkResolveBufferHandle(uint64_t* offset) {
        resolve();
        *offset = std::get<ResolvedView>(storedView).offset;
        return std::get<ResolvedView>(storedView).vkBufferHandle;
    }

private:
    struct ResolvedView {
        uint64_t size;
        uint64_t offset;
        VkBufferHandle vkBufferHandle;

        explicit ResolvedView(const BufferView& view) {
            size = view.getSize();
            offset = 0;
            vkBufferHandle = view.vkResolveBufferHandle(&offset);
        }
    };

    static std::variant<ResolvedView, BufferView> store(const BufferView& view) {
        if (!view.viewsJobLocalBuffer()) {
            return ResolvedView(view);
        } else {
            return view;
        }
    }

    void resolve() {
        if (std::holds_alternative<BufferView>(storedView)) {
            storedView = ResolvedView(std::get<BufferView>(storedView));
            TEPHRA_ASSERTD(
                !std::get<ResolvedView>(storedView).vkBufferHandle.isNull(),
                "Job-local buffers must be resolvable at this point");
        }
    }

    std::variant<ResolvedView, BufferView> storedView;
};

class JobResourcePoolContainer;

class JobLocalBuffers {
public:
    explicit JobLocalBuffers(JobResourcePoolContainer* resourcePoolImpl) : resourcePoolImpl(resourcePoolImpl) {}

    BufferView acquireNewBuffer(BufferSetup setup, const char* debugName);

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
