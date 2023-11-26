#pragma once

#include <tephra/common.hpp>

namespace tp {

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

}
