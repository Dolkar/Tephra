
#include "buffer_impl.hpp"
#include "job/local_buffers.hpp"
#include "device/device_container.hpp"
#include <tephra/buffer.hpp>

namespace tp {

constexpr const char* BufferViewTypeName = "BufferView";

inline void validateViewOffsetSize(uint64_t viewOffset, uint64_t viewSize, uint64_t parentSize, uint64_t alignment) {
    if (viewOffset + viewSize > parentSize) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "The provided `offset + size` (",
            viewOffset + viewSize,
            ") is greater than the size of the buffer or view it's being created from (",
            parentSize,
            ").");
    }
    if (viewOffset % alignment != 0) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "The provided offset (",
            viewOffset,
            ") is not a multiple of the required view alignment (",
            alignment,
            ").");
    }
}

BufferView::BufferView()
    : persistentBuffer(nullptr), offset(0), size(0), format(tp::Format::Undefined), viewsJobLocalBuffer_(false) {}

BufferView BufferView::getView(uint64_t viewOffset, uint64_t viewSize) const {
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(getDebugTarget(), BufferViewTypeName, "getView", nullptr);

    if constexpr (TephraValidationEnabled) {
        validateViewOffsetSize(viewOffset, viewSize, size, getRequiredViewAlignment());
    }

    BufferView subview = *this;
    subview.offset += viewOffset;
    subview.size = viewSize;
    subview.format = Format::Undefined;
    return subview;
}

uint64_t BufferView::getRequiredViewAlignment() const {
    if (viewsJobLocalBuffer()) {
        return jobLocalBuffer->getRequiredViewAlignment();
    } else {
        return persistentBuffer->getRequiredViewAlignment_();
    }
}

MemoryLocation BufferView::getMemoryLocation() const {
    if (viewsJobLocalBuffer()) {
        if (jobLocalBuffer->hasUnderlyingBuffer())
            return jobLocalBuffer->getUnderlyingBuffer()->getMemoryLocation();
        else
            return MemoryLocation::Undefined;
    } else {
        return persistentBuffer->getMemoryLocation_();
    }
}

HostMappedMemory BufferView::mapForHostAccess(MemoryAccess accessType) const {
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(getDebugTarget(), BufferViewTypeName, "mapForHostAccess", nullptr);
    if constexpr (TephraValidationEnabled) {
        if (viewsJobLocalBuffer()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "Attempt to map a job-local buffer for host access.");
        } else {
            if (!persistentBuffer->bufferSetup.usage.contains(BufferUsage::HostMapped)) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "Buffer wasn't created with BufferUsage::HostMapped.");
            }
            MemoryLocation location = persistentBuffer->getMemoryLocation_();
            if (location == MemoryLocation::DeviceLocal || location == MemoryLocation::Undefined) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "Buffer doesn't reside in host-visible memory.");
            } else if (
                (accessType == MemoryAccess::ReadOnly || accessType == MemoryAccess::ReadWrite) &&
                location != MemoryLocation::HostCached && location != MemoryLocation::DeviceLocalHostCached) {
                // TODO: Store and check the used progression rather than the resulting location
                reportDebugMessage(
                    DebugMessageSeverity::Warning,
                    DebugMessageType::Performance,
                    "Read access of buffers allocated from non-cached memory locations can be very slow.");
            }
        }
    }

    if (viewsJobLocalBuffer()) {
        return HostMappedMemory();
    } else {
        return BufferImpl::mapViewForHostAccess(*this, accessType);
    }
}

BufferView BufferView::createTexelView(uint64_t viewOffset, uint64_t viewSize, Format viewFormat) {
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(getDebugTarget(), BufferViewTypeName, "createTexelView", nullptr);
    if constexpr (TephraValidationEnabled) {
        validateViewOffsetSize(viewOffset, viewSize, size, getRequiredViewAlignment());
        if (viewFormat == Format::Undefined) {
            reportDebugMessage(
                DebugMessageSeverity::Error, DebugMessageType::Validation, "The provided format is Undefined.");
        }
    }

    if (viewsJobLocalBuffer()) {
        return jobLocalBuffer->createTexelView(offset + viewOffset, viewSize, viewFormat);
    } else {
        return persistentBuffer->createTexelView_(offset + viewOffset, viewSize, viewFormat);
    }
}

DeviceAddress BufferView::getDeviceAddress() const {
    DeviceAddress parentAddress;
    if (viewsJobLocalBuffer()) {
        parentAddress = jobLocalBuffer->getDeviceAddress();
    } else {
        parentAddress = persistentBuffer->getDeviceAddress_();
    }

    if (parentAddress != 0) {
        return parentAddress + offset;
    } else {
        return 0;
    }
}

VkBufferViewHandle BufferView::vkGetBufferViewHandle() const {
    if (viewsJobLocalBuffer()) {
        if (jobLocalBuffer->hasUnderlyingBuffer()) {
            return BufferImpl::vkGetBufferViewHandle(JobLocalBufferImpl::getViewToUnderlyingBuffer(*this));
        } else {
            return {};
        }
    } else {
        return BufferImpl::vkGetBufferViewHandle(*this);
    }
}

bool operator==(const BufferView& lhs, const BufferView& rhs) {
    if (lhs.size != rhs.size || lhs.offset != rhs.offset || lhs.format != rhs.format) {
        return false;
    }
    if (lhs.viewsJobLocalBuffer() != rhs.viewsJobLocalBuffer()) {
        return false;
    }
    if (lhs.viewsJobLocalBuffer()) {
        return lhs.jobLocalBuffer == rhs.jobLocalBuffer;
    } else {
        return lhs.persistentBuffer == rhs.persistentBuffer;
    }
}

VkBufferHandle BufferView::vkResolveBufferHandle(uint64_t* viewOffset) const {
    if (viewsJobLocalBuffer()) {
        if (jobLocalBuffer->hasUnderlyingBuffer()) {
            BufferView underlyingView = JobLocalBufferImpl::getViewToUnderlyingBuffer(*this);
            TEPHRA_ASSERT(!underlyingView.viewsJobLocalBuffer());
            return underlyingView.vkResolveBufferHandle(viewOffset);
        } else {
            return {};
        }
    } else {
        *viewOffset = offset;
        return persistentBuffer->vkGetBufferHandle();
    }
}

BufferView::BufferView(BufferImpl* persistentBuffer, uint64_t offset, uint64_t size, Format format)
    : persistentBuffer(persistentBuffer), offset(offset), size(size), format(format), viewsJobLocalBuffer_(false) {}

BufferView::BufferView(JobLocalBufferImpl* jobLocalBuffer, uint64_t offset, uint64_t size, Format format)
    : jobLocalBuffer(jobLocalBuffer), offset(offset), size(size), format(format), viewsJobLocalBuffer_(true) {}

const DebugTarget* BufferView::getDebugTarget() const {
    return viewsJobLocalBuffer() ? jobLocalBuffer->getDebugTarget() : persistentBuffer->getDebugTarget();
}

uint64_t Buffer::getSize() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->getSize_();
}

MemoryLocation Buffer::getMemoryLocation() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->getMemoryLocation_();
}

const BufferView Buffer::getDefaultView() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->getDefaultView_();
}

BufferView Buffer::getView(uint64_t viewOffset, uint64_t viewSize) const {
    return getDefaultView().getView(viewOffset, viewSize);
}

uint64_t Buffer::getRequiredViewAlignment() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->getRequiredViewAlignment_();
}

HostMappedMemory Buffer::mapForHostAccess(MemoryAccess accessType) const {
    return getDefaultView().mapForHostAccess(accessType);
}

BufferView Buffer::createTexelView(uint64_t offset, uint64_t size, Format format) {
    auto bufferImpl = static_cast<BufferImpl*>(this);
    return bufferImpl->getDefaultView_().createTexelView(offset, size, format);
}

DeviceAddress Buffer::getDeviceAddress() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->getDeviceAddress_();
}

VmaAllocationHandle Buffer::vmaGetMemoryAllocationHandle() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->vmaGetMemoryAllocationHandle_();
}

VkBufferHandle Buffer::vkGetBufferHandle() const {
    auto bufferImpl = static_cast<const BufferImpl*>(this);
    return bufferImpl->vkGetBufferHandle_();
}

uint64_t Buffer::getRequiredViewAlignment(const Device* device, BufferUsageMask usage) {
    auto deviceImpl = static_cast<const DeviceContainer*>(device);
    return BufferImpl::getRequiredViewAlignment_(deviceImpl, usage);
}

BufferImpl::~BufferImpl() {
    TEPHRA_DEBUG_SET_CONTEXT_DESTRUCTOR(getDebugTarget());
    destroyHandles(false);
}

}
