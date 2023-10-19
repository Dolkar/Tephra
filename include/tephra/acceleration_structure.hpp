#pragma once

#include <tephra/common.hpp>

namespace tp {

struct TriangleGeometrySetup {
    GeometryFlagMask flags;
    uint32_t maxTriangleCount;
    Format vertexFormat;
    uint32_t maxVertexIndex;
    IndexType indexType;
    bool hasTransform;
};

struct AABBGeometrySetup {
    GeometryFlagMask flags;
    uint32_t maxAABBCount;
};

struct AccelerationStructureSetup {
    AccelerationStructureType type;
    AccelerationStructureBuildFlagMask buildFlags;
    uint32_t maxInstanceCount;
    ArrayView<const TriangleGeometrySetup> triangleGeometries;
    ArrayView<const AABBGeometrySetup> aabbGeometries;

    static AccelerationStructureSetup TopLevel(AccelerationStructureBuildFlagMask buildFlags, uint32_t maxInstanceCount);
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
