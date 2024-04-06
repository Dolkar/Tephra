#pragma once

#include <tephra/job.hpp>
#include "../common_impl.hpp"
#include "local_buffers.hpp"
#include "local_images.hpp"
#include <map>

namespace tp {

static constexpr VkAccessFlags WriteAccessBits = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
    VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT | VK_ACCESS_TRANSFER_WRITE_BIT | VK_ACCESS_HOST_WRITE_BIT |
    VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_ACCELERATION_STRUCTURE_WRITE_BIT_KHR |
    VK_ACCESS_COMMAND_PREPROCESS_WRITE_BIT_NV;

// Describes the Vulkan properties of a resource access
struct ResourceAccess {
    VkPipelineStageFlags stageMask;
    VkAccessFlags accessMask;

    ResourceAccess() : ResourceAccess(0, 0) {}

    ResourceAccess(VkPipelineStageFlags stageMask, VkAccessFlags accessMask)
        : stageMask(stageMask), accessMask(accessMask) {}

    bool isReadOnly() const {
        return (accessMask & WriteAccessBits) == 0;
    }

    bool isNull() const {
        return stageMask == 0;
    }

    ResourceAccess& operator|=(const ResourceAccess& other);
};

ResourceAccess operator|(const ResourceAccess& a, const ResourceAccess& b);

// Returns the optimal Vulkan image layout for the given descriptor type. For non-image descriptor types, returns
// VK_IMAGE_LAYOUT_UNDEFINED
VkImageLayout vkGetImageLayoutForDescriptor(DescriptorType descriptorType, bool aliasStorageImage);

// Converts the Tephra read access mask provided by the user into useable vulkan synchronization flags
ResourceAccess convertReadAccessToVkAccess(ReadAccessMask readMask);

// Returns the optimal Vulkan image layout for a read access mask. Assumes the mask contains only accesses with the same
// optimal image layout.
VkImageLayout vkGetImageLayoutFromReadAccess(ReadAccessMask readMask);

// Converts the Tephra compute access mask provided by the user into useable vulkan synchronization flags
void convertComputeAccessToVkAccess(
    ComputeAccessMask computeMask,
    VkPipelineStageFlags* stageMask,
    VkAccessFlags* accessMask,
    bool* isAtomic);

// Returns the optimal Vulkan image layout for a compute access mask. Assumes the mask contains only accesses with the
// same optimal image layout.
VkImageLayout vkGetImageLayoutFromComputeAccess(ComputeAccessMask computeMask);

// Converts the Tephra render access mask provided by the user into useable vulkan synchronization flags
void convertRenderAccessToVkAccess(
    RenderAccessMask renderMask,
    VkPipelineStageFlags* stageMask,
    VkAccessFlags* accessMask,
    bool* isAtomic);

// Returns the optimal Vulkan image layout for a render access mask. Assumes the mask contains only accesses with the
// same optimal image layout.
VkImageLayout vkGetImageLayoutFromRenderAccess(RenderAccessMask renderMask);

// Structure representing the extent of an access to a buffer resource
// Defines < and > operators in such a way that neither of them returns true if two ranges overlap
struct BufferAccessRange {
    uint64_t offset;
    uint64_t size;

    BufferAccessRange() : BufferAccessRange(0, 0) {}

    BufferAccessRange(uint64_t offset, uint64_t size) : offset(offset), size(size) {}

    uint64_t getStartPoint() const {
        return offset;
    }

    uint64_t getEndPoint() const {
        return offset + size;
    }

    bool isNull() const {
        return size == 0;
    }

    bool operator<(const BufferAccessRange& other) const {
        return getEndPoint() <= other.getStartPoint();
    }

    bool operator>(const BufferAccessRange& other) const {
        return getStartPoint() >= other.getEndPoint();
    }
};

VkBufferHandle resolveBufferAccess(StoredBufferView& bufferView, BufferAccessRange* range);
std::pair<VkBufferHandle, BufferAccessRange> resolveBufferAccess(StoredBufferView& bufferView);

// Structure representing the extent of an access to an image resource
// For reduced storage requirements and complexity, mip levels accessed is stored as a mask rather than a range
// Array layers are used as start and end points for purposes of comparison for the range map
struct ImageAccessRange {
    ImageAspectMask aspectMask;
    uint32_t baseArrayLayer;
    uint32_t arrayLayerCount;
    uint32_t mipLevelMask;

    ImageAccessRange() : ImageAccessRange(ImageAspectMask::None(), 0, 0, 0) {}

    ImageAccessRange(const ImageSubresourceRange& range);

    ImageAccessRange(
        ImageAspectMask aspectMask,
        uint32_t baseArrayLayer,
        uint32_t arrayLayerCount,
        uint32_t mipLevelMask)
        : aspectMask(aspectMask),
          baseArrayLayer(baseArrayLayer),
          arrayLayerCount(arrayLayerCount),
          mipLevelMask(mipLevelMask) {}

    uint32_t getStartPoint() const {
        return baseArrayLayer;
    }

    uint32_t getEndPoint() const {
        return baseArrayLayer + arrayLayerCount;
    }

    bool isNull() const {
        return arrayLayerCount == 0 || mipLevelMask == 0 || aspectMask == ImageAspectMask::None();
    }
};

VkImageHandle resolveImageAccess(StoredImageView& imageView, ImageAccessRange* range);

// Returns true when any part of the access ranges is overlapping
inline bool areAccessRangesOverlapping(const BufferAccessRange& a, const BufferAccessRange& b) {
    return (a.getEndPoint() > b.getStartPoint()) && (a.getStartPoint() < b.getEndPoint());
}

inline bool areAccessRangesOverlapping(const ImageAccessRange& a, const ImageAccessRange& b) {
    bool layersOverlapping = (a.getEndPoint() > b.getStartPoint()) && (a.getStartPoint() < b.getEndPoint());
    return layersOverlapping && (a.mipLevelMask & b.mipLevelMask) != 0 && a.aspectMask.containsAny(b.aspectMask);
}

// Returns true when access range a fully contains range b
inline bool doesAccessRangeContainAnother(const BufferAccessRange& a, const BufferAccessRange& b) {
    return a.getStartPoint() <= b.getStartPoint() && a.getEndPoint() >= b.getEndPoint();
}

inline bool doesAccessRangeContainAnother(const ImageAccessRange& a, const ImageAccessRange& b) {
    return a.getStartPoint() <= b.getStartPoint() && a.getEndPoint() >= b.getEndPoint() &&
        a.aspectMask.containsAll(b.aspectMask) && containsAllBits(a.mipLevelMask, b.mipLevelMask);
}

// Structure for a new, identified buffer access
struct NewBufferAccess : ResourceAccess {
    VkBufferHandle vkResourceHandle;
    BufferAccessRange range;

    NewBufferAccess(VkBufferHandle vkBufferHandle, BufferAccessRange range, ResourceAccess access)
        : ResourceAccess(std::move(access)), vkResourceHandle(vkBufferHandle), range(std::move(range)) {}
};

// Structure for a new, identified image access
struct NewImageAccess : ResourceAccess {
    VkImageHandle vkResourceHandle;
    ImageAccessRange range;
    // The layout the image range needs to be in for this access
    VkImageLayout layout;

    NewImageAccess(VkImageHandle vkImageHandle, ImageAccessRange range, ResourceAccess access, VkImageLayout layout)
        : ResourceAccess(std::move(access)), vkResourceHandle(vkImageHandle), range(std::move(range)), layout(layout) {}
};

// Specifies a nullable reference to a particular pipeline and memory dependency within a BarrierList
struct BarrierReference {
    uint32_t pipelineBarrierIndex;
    uint32_t memoryBarrierIndex;

    BarrierReference() : BarrierReference(~0, ~0) {}

    BarrierReference(uint32_t pipelineBarrierIndex, uint32_t memoryBarrierIndex)
        : pipelineBarrierIndex(pipelineBarrierIndex), memoryBarrierIndex(memoryBarrierIndex) {}

    bool isNull() {
        return pipelineBarrierIndex == ~0;
    }

    bool hasMemoryBarrier() {
        return memoryBarrierIndex != ~0;
    }
};

class BarrierList;

// Maintains a map of past accesses of a single buffer resource, using them to synchronize against new accesses
class BufferAccessMap {
public:
    explicit BufferAccessMap(VkBufferHandle vkBufferHandle);

    // Returns the number of currently tracked access ranges
    uint64_t getAccessCount() const;

    // Synchronizes the new access with the previous ones through the provided barrier list
    // Does not modify the access map in a way that would affect any future accesses
    void synchronizeNewAccess(const NewBufferAccess& newAccess, uint32_t commandIndex, BarrierList& barriers);

    // Updates the access map by inserting the new access, to be synchronized against others in the future
    void insertNewAccess(
        const NewBufferAccess& newAccess,
        uint32_t nextBarrierIndex,
        bool forceOverwrite = false,
        bool isExport = false);

    // Clears all previous accesses and barriers
    void clear();

private:
    // Describes how a particular range of the buffer has been previously accessed and synchronized
    struct BufferRangeEntry {
        // Records the last write access to this range
        ResourceAccess lastWriteAccess;

        // Captures when the write access happened and the barriers that can be reused
        uint32_t barrierIndexAfterWriteAccess;

        // Records all read accesses that happened since the last write access
        ResourceAccess lastReadAccesses;

        // Captures when the read accesses happened and the barriers that can be reused
        uint32_t barrierIndexAfterReadAccesses;

        // Tracks if one of the last accesses was an export operation
        bool wasExported;

        // The barrier that was used to synchronize read accesses with the preceding write access
        BarrierReference barrierAfterWriteAccess;

        // Constructs a new entry, initialized to some access - it is treated like write access
        BufferRangeEntry(ResourceAccess access, uint32_t barrierIndexAfterAccess, bool isExport)
            : lastWriteAccess(std::move(access)),
              barrierIndexAfterWriteAccess(barrierIndexAfterAccess),
              lastReadAccesses(0, 0),
              barrierIndexAfterReadAccesses(0),
              wasExported(isExport),
              barrierAfterWriteAccess() {}
    };

    using AccessMapType = std::map<BufferAccessRange, BufferRangeEntry>;

    // The buffer being tracked
    VkBufferHandle vkBufferHandle;
    // The id of the last job. Used for invalidating barriers
    uint64_t lastJobId;
    // Contains a unique entry for each access range
    AccessMapType accessMap;

    // Resets the recorded barrier information of the past accesses.
    void resetBarriers();

    // Helper function to remove the parts of the given entry from the access map that overlap with the range
    AccessMapType::iterator removeOverlappingRange(
        const AccessMapType::iterator& entryIt,
        const BufferAccessRange& overlappingRange);
};

// Maintains a map of past accesses of a single image resource, using them to synchronize against new accesses
class ImageAccessMap {
public:
    explicit ImageAccessMap(VkImageHandle vkImageHandle);

    // Returns the number of currently tracked access ranges
    uint64_t getAccessCount() const;

    // Synchronizes the new access with the previous ones through the provided barrier list
    // Does not modify the access map in a way that would affect any future accesses
    void synchronizeNewAccess(const NewImageAccess& newAccess, uint32_t commandIndex, BarrierList& barriers);

    // Updates the access map by inserting the new access, to be synchronized against others in the future
    void insertNewAccess(
        const NewImageAccess& newAccess,
        uint32_t nextBarrierIndex,
        bool forceOverwrite = false,
        bool isExport = false);

    // Marks the range as not needing to preserve contents for future accesses
    void discardContents(const ImageAccessRange& range);

    // Clears all previous accesses and barriers
    void clear();

private:
    // Describes how a particular range of the image has been previously accessed and synchronized
    struct ImageRangeEntry {
        // Records the last write access to this range
        ResourceAccess lastWriteAccess;

        // Captures when the write access happened and the barriers that can be reused
        uint32_t barrierIndexAfterWriteAccess;

        // Records all read accesses that happened since the last write access
        ResourceAccess lastReadAccesses;

        // Captures when the read accesses happened and the barriers that can be reused
        uint32_t barrierIndexAfterReadAccesses;

        // Tracks if one of the last accesses was an export operation
        bool wasExported;

        // The barrier that was used to synchronize read accesses with the preceding write access
        BarrierReference barrierAfterWriteAccess;

        // The current layout the image subresource range is in
        VkImageLayout layout;

        // Constructs a new entry, initialized to some access - it is treated like write access
        ImageRangeEntry(ResourceAccess access, uint32_t barrierIndexAfterAccess, VkImageLayout layout, bool isExport)
            : lastWriteAccess(std::move(access)),
              barrierIndexAfterWriteAccess(barrierIndexAfterAccess),
              lastReadAccesses(0, 0),
              barrierIndexAfterReadAccesses(0),
              wasExported(isExport),
              barrierAfterWriteAccess(),
              layout(layout) {}
    };
    // We cannot use map here because there is no way to order overlapping image ranges
    // Instead, we use a vector where null ranges represent deleted elements
    using AccessMapType = std::vector<std::pair<ImageAccessRange, ImageRangeEntry>>;

    // The image being tracked
    VkImageHandle vkImageHandle;
    // The id of the last job. Used for invalidating barriers
    uint64_t lastJobId;
    // Contains a unique entry for each access range
    AccessMapType accessMap;

    // Compacts the access map and resets the recorded barrier information of the past accesses.
    void compactAndResetBarriers();

    // Helper function to split the given entry, such that its range will be reduced to an intersection with the given
    // overlapping range and the rest gets appended to access map.
    void splitOverlappingRange(std::size_t entryIndex, const ImageAccessRange& overlappingRange);
};

}
