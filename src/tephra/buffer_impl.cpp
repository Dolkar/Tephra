
#include "buffer_impl.hpp"
#include "device/device_container.hpp"
#include <tephra/format_compatibility.hpp>

namespace tp {

BufferImpl::BufferImpl(
    DeviceContainer* deviceImpl,
    const BufferSetup& bufferSetup,
    Lifeguard<VkBufferHandle>&& bufferHandle,
    Lifeguard<VmaAllocationHandle>&& memoryAllocationHandle,
    DebugTarget debugTarget)
    : debugTarget(std::move(debugTarget)),
      deviceImpl(deviceImpl),
      memoryAllocationHandle(std::move(memoryAllocationHandle)),
      bufferHandle(std::move(bufferHandle)),
      bufferSetup(bufferSetup) {
    if (!this->memoryAllocationHandle.isNull() &&
        deviceImpl->getMemoryAllocator()->isAllocationFullyHostCoherent(this->memoryAllocationHandle.vkGetHandle())) {
        coherentlyMappedMemoryPtr =
            deviceImpl->getMemoryAllocator()->getAllocationInfo(this->memoryAllocationHandle.vkGetHandle()).pMappedData;
        TEPHRA_ASSERT(coherentlyMappedMemoryPtr != nullptr);
    } else {
        coherentlyMappedMemoryPtr = nullptr;
    }
}

MemoryLocation BufferImpl::getMemoryLocation_() const {
    return deviceImpl->getMemoryAllocator()->getAllocationLocation(memoryAllocationHandle.vkGetHandle());
}

void* BufferImpl::beginHostAccess(uint64_t offset, uint64_t size, MemoryAccess accessType) {
    if (coherentlyMappedMemoryPtr != nullptr)
        return static_cast<void*>(static_cast<std::byte*>(coherentlyMappedMemoryPtr) + offset);

    // otherwise do a thread safe mapping & sync
    std::lock_guard<Mutex> mutexLock(memoryMappingMutex);
    void* mappedMemoryPtr = deviceImpl->getMemoryAllocator()->mapMemory(memoryAllocationHandle.vkGetHandle());

    if (accessType == MemoryAccess::ReadOnly || accessType == MemoryAccess::ReadWrite) {
        deviceImpl->getMemoryAllocator()->invalidateAllocationMemory(
            memoryAllocationHandle.vkGetHandle(), offset, size);
    }

    return static_cast<void*>(static_cast<std::byte*>(mappedMemoryPtr) + offset);
}

void BufferImpl::endHostAccess(uint64_t offset, uint64_t size, MemoryAccess accessType) {
    if (coherentlyMappedMemoryPtr == nullptr) {
        std::lock_guard<Mutex> mutexLock(memoryMappingMutex);
        deviceImpl->getMemoryAllocator()->unmapMemory(memoryAllocationHandle.vkGetHandle());

        if (accessType == MemoryAccess::WriteOnly || accessType == MemoryAccess::ReadWrite) {
            deviceImpl->getMemoryAllocator()->flushAllocationMemory(memoryAllocationHandle.vkGetHandle(), offset, size);
        }
    }
}

BufferView BufferImpl::createTexelView_(uint64_t offset, uint64_t size, Format format) {
    TexelViewSetup setup = { offset, size, format };

    VkBufferViewHandle& vkBufferViewHandle = texelViewHandleMap[setup];
    if (vkBufferViewHandle.isNull()) {
        vkBufferViewHandle = deviceImpl->getLogicalDevice()->createBufferView(
            bufferHandle.vkGetHandle(), offset, size, format);
    }

    return BufferView(this, offset, size, format);
}

VkDeviceAddress BufferImpl::getDeviceAddress_() const {
    return deviceImpl->getLogicalDevice()->getBufferDeviceAddress(bufferHandle.vkGetHandle());
}

void BufferImpl::destroyHandles(bool immediately) {
    if (bufferHandle.isNull())
        return;

    // Free all the texel buffer views
    for (const std::pair<TexelViewSetup, VkBufferViewHandle>& bufferViewPair : texelViewHandleMap) {
        VkBufferViewHandle vkBufferViewHandle = bufferViewPair.second;
        // Create a temporary lifeguard here to avoid the unnecessary overhead of storing them in texelViewHandleMap
        Lifeguard<VkBufferViewHandle> lifeguard = deviceImpl->vkMakeHandleLifeguard(vkBufferViewHandle);
        lifeguard.destroyHandle(immediately);
    }
    texelViewHandleMap.clear();

    bufferHandle.destroyHandle(immediately);
    memoryAllocationHandle.destroyHandle(immediately);
}

VkBufferViewHandle BufferImpl::vkGetBufferViewHandle(const BufferView& bufferView) {
    if (bufferView.format == Format::Undefined) {
        // No Vulkan buffer view used
        return {};
    }

    TexelViewSetup setup = { bufferView.offset, bufferView.size, bufferView.format };
    VkBufferViewHandle vkBufferViewHandle = getBufferImpl(bufferView).texelViewHandleMap[setup];
    TEPHRA_ASSERTD(!vkBufferViewHandle.isNull(), "BufferView with format should have a Vulkan handle created");
    return vkBufferViewHandle;
}

HostMappedMemory BufferImpl::mapViewForHostAccess(const BufferView& bufferView, MemoryAccess accessType) {
    return HostMappedMemory(&getBufferImpl(bufferView), bufferView.offset, bufferView.size, accessType);
}

BufferImpl& BufferImpl::getBufferImpl(const BufferView& bufferView) {
    TEPHRA_ASSERT(!bufferView.isNull());
    TEPHRA_ASSERT(!bufferView.viewsJobLocalBuffer());
    return *std::get<BufferImpl*>(bufferView.buffer);
}

uint64_t BufferImpl::getRequiredViewAlignment_(const DeviceContainer* deviceImpl, BufferUsageMask usage) {
    const VkPhysicalDeviceLimits& deviceLimits = deviceImpl->getPhysicalDevice()
                                                     ->vkQueryProperties<VkPhysicalDeviceLimits>();

    uint64_t alignment = 4; // General minimum alignment
    if (usage.contains(BufferUsage::ImageTransfer)) {
        // Buffer-Image copies require alignment to match texel block size. As there is no way for us to know
        // what sort of copies will be done with the buffer, so let's be conservative
        uint64_t maxImageCopyAlignment = getFormatClassProperties(FormatCompatibilityClass::COL256).texelBlockBytes;
        alignment = tp::max(alignment, maxImageCopyAlignment);
        // Technically not required but nice to have
        alignment = tp::max(alignment, deviceLimits.optimalBufferCopyOffsetAlignment);
    }
    if (usage.contains(BufferUsage::HostMapped) && !deviceImpl->getMemoryAllocator()->isAllMemoryHostCoherent()) {
        alignment = tp::max(alignment, deviceLimits.nonCoherentAtomSize);
    }
    if (usage.contains(BufferUsage::TexelBuffer)) {
        alignment = tp::max(alignment, deviceLimits.minTexelBufferOffsetAlignment);
    }
    if (usage.contains(BufferUsage::UniformBuffer)) {
        alignment = tp::max(alignment, deviceLimits.minUniformBufferOffsetAlignment);
    }
    if (usage.contains(BufferUsage::StorageBuffer)) {
        alignment = tp::max(alignment, deviceLimits.minStorageBufferOffsetAlignment);
    }
    if (usage.contains(BufferUsage::VertexBuffer)) {
        uint64_t maxVertexAlignment = 8ull; // Conservative assumption of using 64-bit components
        alignment = tp::max(alignment, maxVertexAlignment);
    }

    return alignment;
}

}
