#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>

namespace tp {

AccelerationStructureSetup AccelerationStructureSetup::TopLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    InstanceGeometrySetup instanceGeometry) {
    return { AccelerationStructureType::TopLevel, buildFlags, instanceGeometry, {}, {} };
}

AccelerationStructureSetup AccelerationStructureSetup::BottomLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    ArrayView<const TriangleGeometrySetup> triangleGeometries,
    ArrayView<const AABBGeometrySetup> aabbGeometries) {
    return {
        AccelerationStructureType::BottomLevel, buildFlags, InstanceGeometrySetup(0), triangleGeometries, aabbGeometries
    };
}

AccelerationStructureBuildInfo AccelerationStructureBuildInfo::TopLevel(
    AccelerationStructureBuildMode mode,
    AccelerationStructureView dstView,
    InstanceGeometryBuildInfo instanceGeometry,
    AccelerationStructureView srcView) {
    return { mode, dstView, instanceGeometry, {}, {}, srcView };
}

AccelerationStructureBuildInfo AccelerationStructureBuildInfo::BottomLevel(
    AccelerationStructureBuildMode mode,
    AccelerationStructureView dstView,
    ArrayView<const TriangleGeometryBuildInfo> triangleGeometries,
    ArrayView<const AABBGeometryBuildInfo> aabbGeometries,
    AccelerationStructureView srcView) {
    return { mode, dstView, InstanceGeometryBuildInfo({}), triangleGeometries, aabbGeometries, srcView };
}

}
