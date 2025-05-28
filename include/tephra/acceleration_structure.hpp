#pragma once

#include <tephra/buffer.hpp>
#include <tephra/common.hpp>
#include <variant>

namespace tp {

class AccelerationStructureImpl;
class JobLocalAccelerationStructureImpl;

/// Represents a non-owning view of a tp::AccelerationStructure.
/// @see tp::AccelerationStructure::getView
/// @see tp::Job::allocateLocalAccelerationStructure
class AccelerationStructureView {
public:
    /// Creates a null acceleration structure view.
    AccelerationStructureView() = default;

    /// Returns `true` if the viewed acceleration structure is null and not valid for use.
    bool isNull() const {
        return std::holds_alternative<std::monostate>(accelerationStructure);
    }

    /// Returns `true` if the instance views a job-local acceleration structure.
    /// Returns `false` if it views a persistent one.
    bool viewsJobLocalAccelerationStructure() const {
        return std::holds_alternative<JobLocalAccelerationStructureImpl*>(accelerationStructure);
    }

    /// Returns the device address of the acceleration structure that can then be used for ray tracing and
    /// acceleration structure build operations.
    /// @see @vksymbol{vkGetAccelerationStructureDeviceAddressKHR}
    DeviceAddress getDeviceAddress() const;

    /// Returns a view of the backing buffer that is used as storage for the acceleration structure data.
    BufferView getBackingBufferView() const;

    /// Returns the associated @vksymbol{VkAccelerationStructureKHR} handle.
    VkAccelerationStructureHandleKHR vkGetAccelerationStructureHandle() const;

private:
    friend class AccelerationStructureImpl;
    friend class JobLocalAccelerationStructureImpl;
    friend bool operator==(const AccelerationStructureView&, const AccelerationStructureView&);

    AccelerationStructureView(AccelerationStructureImpl* persistentAccelerationStructure);
    AccelerationStructureView(JobLocalAccelerationStructureImpl* jobLocalAccelerationStructure);

    std::variant<std::monostate, AccelerationStructureImpl*, JobLocalAccelerationStructureImpl*> accelerationStructure;
};

/// Equality operator for tp::AccelerationStructureView.
bool operator==(const AccelerationStructureView& lhs, const AccelerationStructureView& rhs);
/// Inequality operator for tp::AccelerationStructureView.
inline bool operator!=(const AccelerationStructureView& lhs, const AccelerationStructureView& rhs) {
    return !(lhs == rhs);
}

/// Top-level acceleration structure geometry containing references to bottom-level acceleration structures.
struct InstanceGeometrySetup {
    uint32_t maxInstanceCount;
    GeometryFlagMask flags;

    /// @param maxInstanceCount
    ///     The maximum number of BLAS instances this geometry can hold.
    /// @param flags
    ///     Additional geometry flags.
    InstanceGeometrySetup(uint32_t maxInstanceCount, GeometryFlagMask flags = {})
        : maxInstanceCount(maxInstanceCount), flags(flags) {}
};

/// Bottom-level acceleration structure geometry containing triangles.
struct TriangleGeometrySetup {
    uint32_t maxTriangleCount;
    Format vertexFormat;
    uint32_t maxVertexIndex;
    IndexType indexType;
    bool useTransform;
    GeometryFlagMask flags;

    /// @param maxTriangleCount
    ///     The maximum number of triangles this geometry can hold.
    /// @param vertexFormat
    ///     The format of each vertex element.
    /// @param maxVertexIndex
    ///     The largest vertex index present in the index data.
    /// @indexType
    ///     The type of each index element.
    /// @hasTransform
    ///     If true, the geometry will include a transformation matrix from geometry space to the space of the
    ///     acceleration structure.
    /// @flags
    ///     Additional geometry flags.
    TriangleGeometrySetup(
        uint32_t maxTriangleCount,
        Format vertexFormat,
        uint32_t maxVertexIndex,
        IndexType indexType,
        bool hasTransform,
        GeometryFlagMask flags = {})
        : maxTriangleCount(maxTriangleCount),
          vertexFormat(vertexFormat),
          maxVertexIndex(maxVertexIndex),
          indexType(indexType),
          useTransform(hasTransform),
          flags(flags) {}
};

/// Top-level acceleration structure geometry containing axis-aligned bounding boxes.
struct AABBGeometrySetup {
    uint32_t maxAABBCount;
    GeometryFlagMask flags;

    /// @param maxAABBCount
    ///     The maximum number of AABBs this geometry can hold.
    /// @param flags
    ///     Additional geometry flags.
    AABBGeometrySetup(uint32_t maxAABBCount, GeometryFlagMask flags = {}) : maxAABBCount(maxAABBCount), flags(flags) {}
};

/// Used as configuration for creating a new tp::AccelerationStructure object.
/// @see tp::Device::allocateAccelerationStructureKHR
/// @see @vksymbol{VkAccelerationStructureBuildGeometryInfoKHR}
struct AccelerationStructureSetup {
    AccelerationStructureType type;
    AccelerationStructureFlagMask flags;
    InstanceGeometrySetup instanceGeometry;
    ArrayView<const TriangleGeometrySetup> triangleGeometries;
    ArrayView<const AABBGeometrySetup> aabbGeometries;

    /// Creates a setup for a top-level acceleration structure.
    /// @param flags
    ///     Flags that determine how the acceleration structure will get built.
    /// @param instanceGeometry
    ///     Setup for the instance geometry containing bottom-level acceleration structures.
    static AccelerationStructureSetup TopLevel(
        AccelerationStructureFlagMask flags,
        InstanceGeometrySetup instanceGeometry);

    /// Creates a setup for a bottom-level acceleration structure.
    /// @param flags
    ///     Flags that determine how the acceleration structure will get built.
    /// @param triangleGeometries
    ///     An array of setups for geometries containing triangle data.
    /// @param aabbGeometries
    ///     An array of setups for geometries containing AABB data.
    static AccelerationStructureSetup BottomLevel(
        AccelerationStructureFlagMask flags,
        ArrayView<const TriangleGeometrySetup> triangleGeometries,
        ArrayView<const AABBGeometrySetup> aabbGeometries);
};

/// Represents an opaque acceleration structure used for hardware-accelerated ray tracing.
/// @see @vksymbol{VkAccelerationStructureKHR}
class AccelerationStructure : public Ownable {
public:
    /// Returns a view of this acceleration structure.
    const AccelerationStructureView getView() const;

    /// Returns the device address of the acceleration structure that can then be used for ray tracing and
    /// acceleration structure build operations.
    /// @see @vksymbol{vkGetAccelerationStructureDeviceAddressKHR}
    DeviceAddress getDeviceAddress() const;

    /// Returns a view of the backing buffer that is used as storage for the acceleration structure data.
    const BufferView getBackingBufferView() const;

    /// Returns the associated @vksymbol{VkAccelerationStructureKHR} handle.
    VkAccelerationStructureHandleKHR vkGetAccelerationStructureHandle() const;

    /// Casting operator returning the associated tp::AccelerationStructureView object.
    /// @see tp::AccelerationStructure::getView
    operator const AccelerationStructureView() const {
        return getView();
    }

    TEPHRA_MAKE_INTERFACE(AccelerationStructure);

protected:
    AccelerationStructure() {}
};

/// Information for building an instance geometry of a top-level acceleration structure.
struct InstanceGeometryBuildInfo {
    BufferView instanceBuffer; // VkAccelerationStructureInstanceKHR
    bool arrayOfPointers;
    ArrayView<const AccelerationStructureView> accessedViews;

    /// @param instanceBuffer
    ///     A buffer containing a tightly packed array of instance data, either in the form of the
    ///     @vksymbol{VkAccelerationStructureInstanceKHR} structure if `arrayOfPointers` is `false`, or of pointers
    ///     to it if `true`.
    /// @param arrayOfPointers
    ///     Determines if the `instanceBuffer` contains structures or device addresses pointing to structures.
    /// @param accessedViews
    ///     A list of all potentially referenced bottom-level acceleration structures for synchronization purposes.
    InstanceGeometryBuildInfo(
        BufferView instanceBuffer,
        bool arrayOfPointers = false,
        ArrayView<const AccelerationStructureView> accessedViews = {})
        : instanceBuffer(instanceBuffer), arrayOfPointers(arrayOfPointers), accessedViews(accessedViews) {}
};

struct TriangleGeometryBuildInfo {
    BufferView vertexBuffer;
    uint64_t vertexStride;
    BufferView indexBuffer;
    uint32_t firstVertex;
    BufferView transformBuffer;

    TriangleGeometryBuildInfo(
        BufferView vertexBuffer,
        uint64_t vertexStride = 0,
        BufferView indexBuffer = {},
        uint32_t firstVertex = 0,
        BufferView transformBuffer = {})
        : vertexBuffer(vertexBuffer),
          vertexStride(vertexStride),
          indexBuffer(indexBuffer),
          firstVertex(firstVertex),
          transformBuffer(transformBuffer) {}
};

struct AABBGeometryBuildInfo {
    BufferView aabbBuffer; // VkAabbPositionsKHR
    uint64_t stride;

    AABBGeometryBuildInfo(BufferView aabbBuffer, uint64_t stride = 24) : aabbBuffer(aabbBuffer), stride(stride) {}
};

struct AccelerationStructureBuildInfo {
    AccelerationStructureBuildMode mode;
    AccelerationStructureView dstView;
    InstanceGeometryBuildInfo instanceGeometry;
    ArrayView<const TriangleGeometryBuildInfo> triangleGeometries;
    ArrayView<const AABBGeometryBuildInfo> aabbGeometries;
    AccelerationStructureView srcView;

    static AccelerationStructureBuildInfo TopLevel(
        AccelerationStructureBuildMode mode,
        AccelerationStructureView dstView,
        InstanceGeometryBuildInfo instanceGeometry,
        AccelerationStructureView srcView = {});

    static AccelerationStructureBuildInfo BottomLevel(
        AccelerationStructureBuildMode mode,
        AccelerationStructureView dstView,
        ArrayView<const TriangleGeometryBuildInfo> triangleGeometries,
        ArrayView<const AABBGeometryBuildInfo> aabbGeometries,
        AccelerationStructureView srcView = {});
};

struct AccelerationStructureBuildIndirectInfo {
    ArrayView<const uint32_t> maxPrimitiveCounts;
    BufferView buildRangeBuffer; // VkAccelerationStructureBuildRangeInfoKHR
    uint32_t buildRangeStride;

    AccelerationStructureBuildIndirectInfo(
        ArrayView<const uint32_t> maxPrimitiveCounts,
        BufferView buildRangeBuffer,
        uint32_t buildRangeStride = 16)
        : maxPrimitiveCounts(maxPrimitiveCounts),
          buildRangeBuffer(buildRangeBuffer),
          buildRangeStride(buildRangeStride) {}
};

}
