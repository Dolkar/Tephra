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
    BufferView instanceBuffer;
    bool arrayOfPointers;
    ArrayView<const AccelerationStructureView> accessedViews;

    /// @param instanceBuffer
    ///     A buffer containing a tightly packed array of instance data, either as the
    ///     @vksymbol{VkAccelerationStructureInstanceKHR} structure if `arrayOfPointers` is `false`, or as device
    ///     addresses pointing to the structure if `true`.
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

/// Information for building a triangle geometry of a bottom-level acceleration structure.
struct TriangleGeometryBuildInfo {
    BufferView vertexBuffer;
    uint64_t vertexStride;
    BufferView indexBuffer;
    uint32_t firstVertex;
    BufferView transformBuffer;

    /// @param vertexBuffer
    ///     A buffer containing vertex data used for the triangle geometry.
    /// @param vertexStride
    ///     The stride in bytes between each vertex in `vertexBuffer`. If 0, the size of the vertex format as defined
    ///     in the associated tp::TriangleGeometrySetup structure will be used as the stride.
    /// @param indexBuffer
    ///     An optional buffer containing index data for this geometry. The type of the inde xdata must match what was
    ///     provided in the associated tp::TriangleGeometrySetup.
    /// @param firstVertex
    ///     If index data is provided, this value is added to all indices. Otherwise, the value determines the index
    ///     of the first vertex used for the geometry.
    /// @param transformBuffer
    ///     A buffer containing the transformation matrix from geometry space to the space of the acceleration
    ///     structure in the form of a @vksymbol{VkTransformMatrixKHR} structure.
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

/// Information for building an AABB geometry of a bottom-level acceleration structure.
struct AABBGeometryBuildInfo {
    BufferView aabbBuffer;
    uint64_t stride;

    /// @param aabbBuffer
    ///     A buffer containing axis-aligned bounding box data in the form of a @vksymbol{VkAabbPositionsKHR} structure.
    /// @param stride
    ///     The stride in bytes between each element in `aabbBuffer`.
    AABBGeometryBuildInfo(BufferView aabbBuffer, uint64_t stride = 24) : aabbBuffer(aabbBuffer), stride(stride) {}
};

/// Information for building or updating an acceleration structure.
struct AccelerationStructureBuildInfo {
    AccelerationStructureBuildMode mode;
    AccelerationStructureView dstView;
    InstanceGeometryBuildInfo instanceGeometry;
    ArrayView<const TriangleGeometryBuildInfo> triangleGeometries;
    ArrayView<const AABBGeometryBuildInfo> aabbGeometries;
    AccelerationStructureView srcView;

    /// Creates build information for a top-level acceleration structure.
    /// @param mode
    ///     The build operation to perform on this acceleration structure.
    /// @param dstView
    ///     The output acceleration structure.
    /// @param instanceGeometry
    ///     Build information for the sole instance geometry of the acceleration structure.
    /// @param srcView
    ///     For updates, optionally provides the source acceleration structure for the update. It can be omitted for an
    ///     in-place update.
    /// @remarks
    ///     If `srcView` is not null, then the source and destination acceleration structures must have been created
    ///     with identical setup structures.
    static AccelerationStructureBuildInfo TopLevel(
        AccelerationStructureBuildMode mode,
        AccelerationStructureView dstView,
        InstanceGeometryBuildInfo instanceGeometry,
        AccelerationStructureView srcView = {});

    /// Creates build information for a bottom-level acceleration structure.
    /// @param mode
    ///     The build operation to perform on this acceleration structure.
    /// @param dstView
    ///     The output acceleration structure.
    /// @param triangleGeometries
    ///     Build information for the triangle geometries of the acceleration structure.
    /// @param aabbGeometries
    ///     Build information for the AABB geometries fo the acceleration structure.
    /// @param srcView
    ///     For updates, optionally provides the source acceleration structure for the update. It can be omitted for an
    ///     in-place update.
    /// @remarks
    ///     If `srcView` is not null, then the source and destination acceleration structures must have been created
    ///     with identical setup structures.
    static AccelerationStructureBuildInfo BottomLevel(
        AccelerationStructureBuildMode mode,
        AccelerationStructureView dstView,
        ArrayView<const TriangleGeometryBuildInfo> triangleGeometries,
        ArrayView<const AABBGeometryBuildInfo> aabbGeometries,
        AccelerationStructureView srcView = {});
};

/// Additional information for an indirect build or update of an acceleration structure.
struct AccelerationStructureBuildIndirectInfo {
    BufferView buildRangeBuffer;
    uint32_t buildRangeStride;

    /// @param buildRangeBuffer
    ///     Buffer containing build range information for each geometry in the form of
    ///     @vksymbol{VkAccelerationStructureBuildRangeInfoKHR} structures.
    /// @param buildRangeStride
    ///     The stride in bytes between each element in `buildRangeBuffer`.
    /// @remarks
    ///     For a top-level acceleration structure, only one element is expected in the `buildRangeBuffer` data.
    ///     For a bottom-level acceleration structure, the buffer should contain elements for each of the triangle
    ///     geometries, followed by each of the AABB geometries, as defined in the associated
    ///     tp::AccelerationStructureSetup when the acceleration structure was created.
    AccelerationStructureBuildIndirectInfo(BufferView buildRangeBuffer, uint32_t buildRangeStride = 16)
        : buildRangeBuffer(buildRangeBuffer), buildRangeStride(buildRangeStride) {}
};

}
