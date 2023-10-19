#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>

namespace tp {

AccelerationStructureSetup AccelerationStructureSetup::TopLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    uint32_t maxInstanceCount) {
    AccelerationStructureSetup setup;
    setup.type = AccelerationStructureType::TopLevel;
    setup.buildFlags = buildFlags;
    setup.maxInstanceCount = maxInstanceCount;
    setup.triangleGeometries = {};
    setup.aabbGeometries = {};
    return setup;
}

AccelerationStructureSetup AccelerationStructureSetup::BottomLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    ArrayView<const TriangleGeometrySetup> triangleGeometries,
    ArrayView<const AABBGeometrySetup> aabbGeometries) {
    AccelerationStructureSetup setup;
    setup.type = AccelerationStructureType::BottomLevel;
    setup.buildFlags = buildFlags;
    setup.maxInstanceCount = 0;
    setup.triangleGeometries = triangleGeometries;
    setup.aabbGeometries = aabbGeometries;
    return setup;
}

}
