#pragma once

#include <tephra/format.hpp>
#include <tephra/memory.hpp>
#include <tephra/common.hpp>

namespace tp {

class Buffer;
class BufferImpl;
class JobLocalBufferImpl;

/// Represents the view of a contiguous range of tp::Buffer memory.
///
/// Optionally, it can be made to interpret the data in a particular format for binding
/// to a tp::Descriptor of the tp::DescriptorType::TexelBuffer or tp::DescriptorType::StorageTexelBuffer types.
/// Such buffer views are called texel buffer views.
///
/// @see tp::Buffer::createView
/// @see tp::BufferView::createView
/// @see tp::Job::allocateLocalBuffer
/// @see tp::Job::allocatePreinitializedBuffer
/// @see @vksymbol{VkBufferView}
class BufferView {
public:
    /// Constructs a null tp::BufferView.
    BufferView();

    /// Returns `true` if the buffer view is null and does not view any resource.
    bool isNull() const {
        return persistentBuffer == nullptr;
    }

    /// Returns the size of the buffer view in bytes.
    uint64_t getSize() const {
        return size;
    }

    /// Returns the format of the buffer view. Texel buffer views have a format other than tp::Format::Undefined.
    Format getFormat() const {
        return format;
    }

    /// Returns the tp::MemoryLocation of the underlying buffer, or tp::MemoryLocation::Undefined if it doesn't exist
    /// yet.
    MemoryLocation getMemoryLocation() const;

    /// Returns another view of the viewed buffer relative to this view.
    /// @param offset
    ///     The additional offset to the viewed buffer in bytes. Must be a multiple of
    ///     tp::BufferView::getRequiredViewAlignment.
    /// @param size
    ///     The size of the buffer view in bytes.
    /// @remarks
    ///     The range of the new view must be fully contained inside this view, meaning the `offset + size` must be less
    ///     or equal the size of the current view.
    /// @remarks
    ///     This method will always return regular buffer views (format set to tp::Format::Undefined) even if this view
    ///     is a texel buffer view.
    BufferView getView(uint64_t offset, uint64_t size) const;

    /// Returns the minimum required alignment in bytes for the views of this buffer.  It is always a power of two
    /// not larger than 256 bytes.
    uint64_t getRequiredViewAlignment() const;

    /// Returns `true` if the instance views a job-local buffer.
    /// Returns `false` if it views a persistent or a job preinitialized buffer and therefore
    /// can be mapped for host access.
    bool viewsJobLocalBuffer() const {
        return viewsJobLocalBuffer_;
    }

    /// Maps the viewed range of the buffer to visible memory, allowing it to be written
    /// or read out by the application.
    /// @param accessType
    ///     Specifies the allowed types of access.
    /// @returns
    ///     Returns tp::HostMappedMemory object that can be used to access the memory range.
    /// @remarks
    ///     The viewed buffer must be either a persistent buffer or a job preinitialized buffer
    ///     allocated from a tp::Job. The viewed buffer must be allocated from host-visible memory.
    HostMappedMemory mapForHostAccess(MemoryAccess accessType) const;

    /// Creates a texel buffer view of the viewed buffer relative to this view.
    /// @param offset
    ///     The additional offset to the viewed buffer in bytes. Must be a multiple of
    ///     tp::Buffer::getRequiredViewAlignment.
    /// @param size
    ///     The size of the buffer view in bytes.
    /// @param format
    ///     The format that the viewed data will be interpreted as when bound as a texel buffer descriptor.
    ///     Must not be tp::Format::Undefined.
    /// @remarks
    ///     The range of the new view must be fully contained inside this view, meaning the `offset + size` must be less
    ///     or equal the size of the current view.
    /// @remarks
    ///     The viewed buffer must have been created with tp::BufferUsage::TexelBuffer.
    /// @remarks
    ///     This is effectively a shorthand for tp::Buffer::createTexelView. This means that it isn't thread safe with
    ///     respect to the underlying buffer. For job-local buffer views, this extends to the parent
    ///     tp::JobResourcePool.
    BufferView createTexelView(uint64_t offset, uint64_t size, Format format);

    /// Returns the device address of the buffer view that can then be used for accessing it in shaders.
    /// @remarks
    ///     The viewed buffer must have been created with tp::BufferUsage::DeviceAddress buffer usage.
    /// @remarks
    ///     If the viewed buffer is a job-local buffer, a valid address will only be returned after the tp::Job has
    ///     been enqueued.
    DeviceAddress getDeviceAddress() const;

    /// Returns the associated Vulkan @vksymbol{VkBufferView} handle of a texel buffer view, `VK_NULL_HANDLE` otherwise.
    /// @remarks
    ///     If the viewed buffer is a job-local buffer, the @vksymbol{VkBufferView} handle will exist only after
    ///     the tp::Job has been enqueued.
    VkBufferViewHandle vkGetBufferViewHandle() const;

    /// Resolves and returns the underlying @vksymbol{VkBuffer} handle of this view or `VK_NULL_HANDLE` if it doesn't
    /// exist.
    /// @param offset
    ///     An output parameter to which the offset of the view in bytes will be set if the underlying buffer exists.
    /// @remarks
    ///     If the viewed buffer is a job-local buffer, the underlying @vksymbol{VkBuffer} handle will exist only after
    ///     the tp::Job has been enqueued.
    VkBufferHandle vkResolveBufferHandle(uint64_t* offset) const;

private:
    friend class BufferImpl;
    friend class JobLocalBufferImpl;
    friend bool operator==(const BufferView&, const BufferView&);

    BufferView(BufferImpl* persistentBuffer, uint64_t offset, uint64_t size, Format format);

    BufferView(JobLocalBufferImpl* jobLocalBuffer, uint64_t offset, uint64_t size, Format format);

    const DebugTarget* getDebugTarget() const;

    union {
        BufferImpl* persistentBuffer;
        JobLocalBufferImpl* jobLocalBuffer;
    };
    uint64_t offset;
    uint64_t size;
    Format format;
    bool viewsJobLocalBuffer_;
};

/// Equality operator for tp::BufferView.
bool operator==(const BufferView& lhs, const BufferView& rhs);

/// Inequality operator for tp::BufferView.
inline bool operator!=(const BufferView& lhs, const BufferView& rhs) {
    return !(lhs == rhs);
}

/// The allowed usage of a tp::Buffer.
/// @remarks
///     Copy commands that don't involve an image, as well as tp::Job::cmdFillBuffer and tp::Job::cmdUpdateBuffer are
///     implicitly allowed and don't require a usage flag.
/// @see @vksymbol{VkBufferUsageFlagBits}
enum class BufferUsage : uint32_t {
    /// Allows the buffer to be used as the source or destination buffer of copy commands that copy to or from an image.
    ImageTransfer = 1 << 0,
    /// Allows the buffer to have its memory mapped for host access using tp::Buffer::mapForHostAccess.
    HostMapped = 1 << 1,
    /// Allows the buffer to be used in a tp::DescriptorType::TexelBuffer or a tp::DescriptorType::StorageTexelBuffer
    /// descriptor.
    TexelBuffer = 1 << 2,
    /// Allows the buffer to be used in a tp::DescriptorType::UniformBuffer descriptor.
    UniformBuffer = 1 << 3,
    /// Allows the buffer to be used in a tp::DescriptorType::StorageBuffer descriptor.
    StorageBuffer = 1 << 4,
    /// Allows the buffer to be used as an index buffer through tp::RenderList::cmdBindIndexBuffer.
    IndexBuffer = 1 << 5,
    /// Allows the buffer to be used as a vertex buffer through tp::RenderList::cmdBindVertexBuffers.
    VertexBuffer = 1 << 6,
    /// Allows the buffer to be used as an indirect parameter buffer for tp::RenderList::cmdDrawIndirect,
    /// tp::ComputeList::cmdDispatchIndirect and similar.
    IndirectBuffer = 1 << 7,
    /// Allows the use of tp::Buffer::getDeviceAddress and tp::BufferView::getDeviceAddress to access the buffer
    /// from shaders through its address.
    /// @remarks
    ///     Requires the @vksymbol{VkPhysicalDeviceVulkan12Features}::`bufferDeviceAddress` feature to be enabled.
    DeviceAddress = 1 << 8,
    /// Allows the use of the buffer as a read-only input to an acceleration structure build operation
    /// TODO!
    /// @remarks
    ///     Requires the tp::DeviceExtension::KHR_AccelerationStructure device extension to be enabled.
    AccelerationStructureInputKHR = 1 << 9,
};
TEPHRA_MAKE_ENUM_BIT_MASK(BufferUsageMask, BufferUsage);

/// Used as configuration for creating a new tp::Buffer object.
/// @see tp::Device::allocateBuffer
struct BufferSetup {
    uint64_t size;
    BufferUsageMask usage;
    VkBufferUsageFlags vkAdditionalUsage;

    /// @param size
    ///     The size of the new buffer in bytes.
    /// @param usage
    ///     A mask of tp::BufferUsage specifying the permitted set of usages of the new buffer.
    /// @param vkAdditionalUsage
    ///     A mask of additional Vulkan usage flags that will be passed to @vksymbol{VkBufferCreateInfo}.
    BufferSetup(uint64_t size, BufferUsageMask usage, VkBufferUsageFlags vkAdditionalUsage = 0)
        : size(size), usage(usage), vkAdditionalUsage(vkAdditionalUsage) {}
};

/// Represents a linear array of data visible to the device.
///
/// They are not used directly, but instead are passed to commands or descriptors through tp::BufferView
/// objects that view a contiguous range of its data.
///
/// @remarks
///     The intention is that a single tp::Buffer object may contain more than one set of data, with each
///     set accessible through user created views.
/// @see tp::Device::allocateBuffer
/// @see @vksymbol{VkBuffer}
class Buffer : public Ownable {
public:
    /// Returns the size of the buffer in bytes.
    uint64_t getSize() const;

    /// Returns the tp::MemoryLocation that the buffer has been allocated from.
    MemoryLocation getMemoryLocation() const;

    /// Returns the default tp::BufferView object that views the entire buffer range.
    const BufferView getDefaultView() const;

    /// Returns a view of a specified contiguous range of the buffer data.
    /// @param offset
    ///     The offset to the viewed buffer in bytes. Must be a multiple of tp::Buffer::getRequiredViewAlignment.
    /// @param size
    ///     The size of the buffer view in bytes.
    /// @remarks
    ///     The range of the new view must be fully contained inside the buffer, meaning the `offset + size` must be
    ///     less or equal the size of the buffer.
    BufferView getView(uint64_t offset, uint64_t size) const;

    /// Returns the minimum required alignment in bytes for the views of this buffer. It is always a power of two
    /// not larger than 256 bytes.
    uint64_t getRequiredViewAlignment() const;

    /// Maps the buffer to visible memory, allowing it to be written or read out by the application.
    /// @param accessType
    ///     Specifies the allowed types of access.
    /// @returns
    ///     Returns tp::HostMappedMemory object that can be used to access the memory.
    /// @remarks
    ///     The buffer must be allocated from host-visible memory.
    HostMappedMemory mapForHostAccess(MemoryAccess accessType) const;

    /// Creates a texel buffer view of the buffer data.
    /// @param offset
    ///     The offset to the viewed buffer in bytes. Must be a multiple of tp::Buffer::getRequiredViewAlignment.
    /// @param size
    ///     The size of the buffer view in bytes.
    /// @param format
    ///     The format that the viewed data will be interpreted as when bound as a texel buffer descriptor.
    ///     Must not be tp::Format::Undefined.
    /// @remarks
    ///     The buffer must have been created with tp::BufferUsage::TexelBuffer.
    /// @remarks
    ///     The range of the new view must be fully contained inside the buffer, meaning the `offset + size` must be
    ///     less or equal the size of the buffer.
    BufferView createTexelView(uint64_t offset, uint64_t size, Format format);

    /// Returns the device address of the buffer that can then be used for accessing it in shaders.
    /// @remarks
    ///     The buffer must have been created with tp::BufferUsage::DeviceAddress buffer usage.
    /// @see @vksymbol{vkGetBufferDeviceAddress}
    DeviceAddress getDeviceAddress() const;

    /// Returns the associated @vmasymbol{VmaAllocation,struct_vma_allocation} handle.
    VmaAllocationHandle vmaGetMemoryAllocationHandle() const;

    /// Returns the associated @vksymbol{VkBuffer} handle.
    VkBufferHandle vkGetBufferHandle() const;

    /// Casting operator returning the default tp::BufferView object
    /// @see tp::Buffer::getDefaultView
    operator const BufferView() const {
        return getDefaultView();
    }

    /// Returns the minimum required alignment in bytes for the views of any buffer with the given usage flags. It is
    /// always a power of two not larger than 256 bytes.
    /// @param device
    ///     The device that the parent buffer will be allocated from.
    /// @param usage
    ///     The usage of the buffer.
    static uint64_t getRequiredViewAlignment(const Device* device, BufferUsageMask usage);

    TEPHRA_MAKE_INTERFACE(Buffer)

protected:
    Buffer() {}
};

/// Equality operator for tp::Buffer.
inline bool operator==(const Buffer& lhs, const Buffer& rhs) {
    return lhs.vkGetBufferHandle() == rhs.vkGetBufferHandle();
}

/// Inequality operator for tp::Buffer.
inline bool operator!=(const Buffer& lhs, const Buffer& rhs) {
    return !(lhs == rhs);
}

}
