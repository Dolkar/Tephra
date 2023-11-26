#pragma once

#include "common_impl.hpp"
#include <tephra/buffer.hpp>
#include <unordered_map>

namespace tp {

struct TexelViewSetup {
    uint64_t size;
    uint64_t offset;
    Format format;
};

inline bool operator==(const TexelViewSetup& lhs, const TexelViewSetup& rhs) {
    return lhs.offset == rhs.offset && lhs.size == rhs.size && lhs.format == rhs.format;
}

struct TexelViewSetupHash {
    constexpr std::size_t operator()(const TexelViewSetup& setup) const {
        const uint64_t fibMul = 11400714819323198485ull; // 2^64 / phi
        uint64_t hash = setup.size;
        hash = hash * fibMul ^ setup.offset;
        hash = hash * fibMul ^ static_cast<uint64_t>(setup.format);
        return hash;
    }
};

class BufferImpl : public Buffer {
public:
    BufferImpl(
        DeviceContainer* deviceImpl,
        const BufferSetup& bufferSetup,
        Lifeguard<VkBufferHandle>&& bufferHandle,
        Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
        DebugTarget debugTarget);

    const DebugTarget* getDebugTarget() const {
        return &debugTarget;
    }

    DebugTarget* getDebugTarget() {
        return &debugTarget;
    }

    const BufferSetup& getBufferSetup() const {
        return bufferSetup;
    }

    uint64_t getSize_() const {
        return bufferSetup.size;
    };

    MemoryLocation getMemoryLocation_() const;

    BufferView getDefaultView_() {
        return BufferView(this, 0, bufferSetup.size, tp::Format::Undefined);
    }

    const BufferView getDefaultView_() const {
        return const_cast<BufferImpl*>(this)->getDefaultView_();
    }

    uint64_t getRequiredViewAlignment_() const {
        return getRequiredViewAlignment_(deviceImpl, bufferSetup.usage);
    }

    void* beginHostAccess(uint64_t offset, uint64_t size, MemoryAccess accessType);

    void endHostAccess(uint64_t offset, uint64_t size, MemoryAccess accessType);

    BufferView createTexelView_(uint64_t offset, uint64_t size, Format format);

    DeviceAddress getDeviceAddress_() const {
        TEPHRA_ASSERTD(deviceAddress != 0, "Device address queried for a buffer without appropriate usage flag!");
        return deviceAddress;
    }

    VmaAllocationHandle vmaGetMemoryAllocationHandle_() const {
        return memoryAllocationHandle.vkGetHandle();
    }

    VkBufferHandle vkGetBufferHandle_() const {
        return bufferHandle.vkGetHandle();
    }

    void destroyHandles(bool immediately);

    static VkBufferViewHandle vkGetBufferViewHandle(const BufferView& bufferView);

    static HostMappedMemory mapViewForHostAccess(const BufferView& bufferView, MemoryAccess accessType);

    static BufferImpl* getBufferImpl(const BufferView& bufferView);

    static uint64_t getRequiredViewAlignment_(const DeviceContainer* deviceImpl, BufferUsageMask usage);

    TEPHRA_MAKE_NONCOPYABLE(BufferImpl);
    TEPHRA_MAKE_NONMOVABLE(BufferImpl);
    ~BufferImpl();

private:
    friend class BufferView;

    using TexelViewHandleMap = std::unordered_map<TexelViewSetup, VkBufferViewHandle, TexelViewSetupHash>;

    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    Lifeguard<VmaAllocationHandle> memoryAllocationHandle;
    Lifeguard<VkBufferHandle> bufferHandle;
    BufferSetup bufferSetup;
    DeviceAddress deviceAddress = 0;

    TexelViewHandleMap texelViewHandleMap;
    // For internal synchronization of memory mapping. In most cases the memory is coherent and the mutex won't be used
    void* coherentlyMappedMemoryPtr;
    Mutex memoryMappingMutex;
};

}
