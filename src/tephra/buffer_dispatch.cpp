
#include "buffer_impl.hpp"
#include "job/local_buffers.hpp"
#include "device/device_container.hpp"
#include "..\..\include\tephra\buffer.hpp"

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

BufferView::BufferView() : buffer(), offset(0), size(0), format(tp::Format::Undefined) {}

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
        return std::get<JobLocalBufferImpl*>(buffer)->getRequiredViewAlignment();
    } else if (!isNull()) {
        return std::get<BufferImpl*>(buffer)->getRequiredViewAlignment_();
    } else {
        return 0;
    }
}

MemoryLocation BufferView::getMemoryLocation() const {
    if (viewsJobLocalBuffer()) {
        if (std::get<JobLocalBufferImpl*>(buffer)->hasUnderlyingBuffer()) {
            return std::get<JobLocalBufferImpl*>(buffer)->getUnderlyingBuffer()->getMemoryLocation();
        } else {
            return MemoryLocation::Undefined;
        }
    } else if (!isNull()) {
        return std::get<BufferImpl*>(buffer)->getMemoryLocation_();
    } else {
        return MemoryLocation::Undefined;
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
        } else if (isNull()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "Attempt to map a null buffer for host access.");
        } else {
            if (!std::get<BufferImpl*>(buffer)->bufferSetup.usage.contains(BufferUsage::HostMapped)) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "Buffer wasn't created with BufferUsage::HostMapped.");
            }
            MemoryLocation location = std::get<BufferImpl*>(buffer)->getMemoryLocation_();
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

    if (isNull() || viewsJobLocalBuffer()) {
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
        return std::get<JobLocalBufferImpl*>(buffer)->createTexelView(offset + viewOffset, viewSize, viewFormat);
    } else if (!isNull()) {
        return std::get<BufferImpl*>(buffer)->createTexelView_(offset + viewOffset, viewSize, viewFormat);
    } else {
        return {};
    }
}

VkDeviceAddress BufferView::getDeviceAddress() const {
    VkDeviceAddress parentAddress = 0;
    if (viewsJobLocalBuffer()) {
        parentAddress = std::get<JobLocalBufferImpl*>(buffer)->getDeviceAddress();
    } else if (!isNull()) {
        parentAddress = std::get<BufferImpl*>(buffer)->getDeviceAddress_();
    }

    if (parentAddress != 0) {
        parentAddress += offset;
    }

    return parentAddress;
}

VkBufferViewHandle BufferView::vkGetBufferViewHandle() const {
    if (viewsJobLocalBuffer()) {
        if (std::get<JobLocalBufferImpl*>(buffer)->hasUnderlyingBuffer()) {
            return BufferImpl::vkGetBufferViewHandle(JobLocalBufferImpl::getViewToUnderlyingBuffer(*this));
        } else {
            return {};
        }
    } else if (!isNull()) {
        return BufferImpl::vkGetBufferViewHandle(*this);
    } else {
        return {};
    }
}

VkBufferHandle BufferView::vkResolveBufferHandle(uint64_t* viewOffset) const {
    if (viewsJobLocalBuffer()) {
        if (std::get<JobLocalBufferImpl*>(buffer)->hasUnderlyingBuffer()) {
            BufferView underlyingView = JobLocalBufferImpl::getViewToUnderlyingBuffer(*this);
            TEPHRA_ASSERT(!underlyingView.viewsJobLocalBuffer());
            return underlyingView.vkResolveBufferHandle(viewOffset);
        } else {
            return {};
        }
    } else if (!isNull()) {
        *viewOffset = offset;
        return std::get<BufferImpl*>(buffer)->vkGetBufferHandle();
    } else {
        return {};
    }
}

BufferView::BufferView(BufferImpl* persistentBuffer, uint64_t offset, uint64_t size, Format format)
    : buffer(persistentBuffer), offset(offset), size(size), format(format) {}

BufferView::BufferView(JobLocalBufferImpl* jobLocalBuffer, uint64_t offset, uint64_t size, Format format)
    : buffer(jobLocalBuffer), offset(offset), size(size), format(format) {}

const DebugTarget* BufferView::getDebugTarget() const {
    if (viewsJobLocalBuffer())
        return std::get<JobLocalBufferImpl*>(buffer)->getDebugTarget();
    else if (!isNull())
        return std::get<BufferImpl*>(buffer)->getDebugTarget();
    else
        return nullptr;
}

bool operator==(const BufferView& lhs, const BufferView& rhs) {
    if (lhs.size != rhs.size || lhs.offset != rhs.offset || lhs.format != rhs.format) {
        return false;
    }
    return lhs.buffer == rhs.buffer;
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

VkDeviceAddress Buffer::getDeviceAddress() const {
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
