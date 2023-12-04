#pragma once

#include <tephra/buffer.hpp>
#include <tephra/common.hpp>

namespace tp {

class AccelerationStructureImpl;

class AccelerationStructureView {
public:
    AccelerationStructureView();

    bool isNull() const;

    bool viewsJobLocalAccelerationStructure() const;

    DeviceAddress getDeviceAddress() const;

    BufferView getBackingBufferView() const;

    VkAccelerationStructureHandleKHR vkResolveAccelerationStructureHandle() const;

private:
    friend bool operator==(const AccelerationStructureView&, const AccelerationStructureView&);
    friend class AccelerationStructureImpl;

    AccelerationStructureImpl* accelerationStructure;
    BufferView backingBufferView;
};

bool operator==(const AccelerationStructureView& lhs, const AccelerationStructureView& rhs);
inline bool operator!=(const AccelerationStructureView& lhs, const AccelerationStructureView& rhs) {
    return !(lhs == rhs);
}

struct InstanceGeometrySetup {
    uint32_t maxInstanceCount;
    GeometryFlagMask flags;

    InstanceGeometrySetup(uint32_t maxInstanceCount, GeometryFlagMask flags = {})
        : maxInstanceCount(maxInstanceCount), flags(flags) {}
};

struct TriangleGeometrySetup {
    uint32_t maxTriangleCount;
    Format vertexFormat;
    uint32_t maxVertexIndex;
    IndexType indexType;
    bool useTransform;
    GeometryFlagMask flags;

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

struct AABBGeometrySetup {
    uint32_t maxAABBCount;
    GeometryFlagMask flags;

    AABBGeometrySetup(uint32_t maxAABBCount, GeometryFlagMask flags = {}) : maxAABBCount(maxAABBCount), flags(flags) {}
};

struct AccelerationStructureSetup {
    AccelerationStructureType type;
    AccelerationStructureBuildFlagMask buildFlags;
    InstanceGeometrySetup instanceGeometry;
    ArrayView<const TriangleGeometrySetup> triangleGeometries;
    ArrayView<const AABBGeometrySetup> aabbGeometries;

    static AccelerationStructureSetup TopLevel(
        AccelerationStructureBuildFlagMask buildFlags,
        InstanceGeometrySetup instanceGeometry);

    static AccelerationStructureSetup BottomLevel(
        AccelerationStructureBuildFlagMask buildFlags,
        ArrayView<const TriangleGeometrySetup> triangleGeometries,
        ArrayView<const AABBGeometrySetup> aabbGeometries);
};

class AccelerationStructure : public Ownable {
public:
    TEPHRA_MAKE_INTERFACE(AccelerationStructure);

protected:
    AccelerationStructure() {}
};

struct InstanceGeometryBuildInfo {
    BufferView instanceBuffer; // VkAccelerationStructureInstanceKHR
    bool arrayOfPointers;

    InstanceGeometryBuildInfo(BufferView instanceBuffer, bool arrayOfPointers = false)
        : instanceBuffer(instanceBuffer), arrayOfPointers(arrayOfPointers) {}
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

    AABBGeometryBuildInfo(BufferView aabbBuffer, uint64_t stride = 8) : aabbBuffer(aabbBuffer), stride(stride) {}
};

struct AccelerationStructureBuildInfo {
    AccelerationStructureBuildMode mode;
    AccelerationStructureView dstView;
    InstanceGeometryBuildInfo instanceGeometry;
    ArrayView<const TriangleGeometryBuildInfo> triangleGeometries;
    ArrayView<const AABBGeometrySetup> aabbGeometries;
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
        ArrayView<const AABBGeometrySetup> aabbGeometries,
        AccelerationStructureView srcView = {});
};

}
