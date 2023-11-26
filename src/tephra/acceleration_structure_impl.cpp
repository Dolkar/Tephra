
#include "acceleration_structure_impl.hpp"
#include "device/device_container.hpp"

namespace tp {

AccelerationStructureBuildInfo AccelerationStructureImpl::prepareBuildInfoForSizeQuery(
    const AccelerationStructureSetup& setup) {
    // Prepare geometry template
    VkAccelerationStructureGeometryKHR geomTemplate;
    geomTemplate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geomTemplate.pNext = nullptr;

    AccelerationStructureBuildInfo info;
    info.geomInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    info.geomInfo.pNext = nullptr;
    info.geomInfo.type = vkCastConvertibleEnum(setup.type);
    info.geomInfo.flags = vkCastConvertibleEnumMask(setup.buildFlags);
    info.geomInfo.mode = VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR;
    info.geomInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    info.geomInfo.dstAccelerationStructure = VK_NULL_HANDLE;

    if (setup.type == AccelerationStructureType::TopLevel) {
        info.geomInfo.geometryCount = 1;

        VkAccelerationStructureGeometryKHR& geom = info.geoms.emplace_back(geomTemplate);
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.flags = vkCastConvertibleEnumMask(setup.instanceGeometry.flags);
        geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geom.geometry.instances.pNext = nullptr;
        geom.geometry.instances.arrayOfPointers = false;

        // Not required:
        geom.geometry.instances.data = MakeConstDeviceAddress();

        info.maxPrimitiveCounts.emplace_back(setup.instanceGeometry.maxInstanceCount);
    } else {
        TEPHRA_ASSERT(setup.type == AccelerationStructureType::BottomLevel);

        info.geomInfo.geometryCount = static_cast<uint32_t>(setup.triangleGeometries.size()) +
            static_cast<uint32_t>(setup.aabbGeometries.size());
        info.geoms.reserve(info.geomInfo.geometryCount);
        info.maxPrimitiveCounts.reserve(info.geomInfo.geometryCount);

        // Triangles, then AABBs
        for (const TriangleGeometrySetup& triSetup : setup.triangleGeometries) {
            VkAccelerationStructureGeometryKHR& geom = info.geoms.emplace_back(geomTemplate);
            geom.geometryType = VK_GEOMETRY_TYPE_TRIANGLES_KHR;
            geom.flags = vkCastConvertibleEnumMask(triSetup.flags);
            geom.geometry.triangles.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR;
            geom.geometry.triangles.pNext = nullptr;
            geom.geometry.triangles.vertexFormat = vkCastConvertibleEnum(triSetup.vertexFormat);
            geom.geometry.triangles.maxVertex = triSetup.maxVertexIndex;
            geom.geometry.triangles.indexType = vkCastConvertibleEnum(triSetup.indexType);

            // If we want to use transform data later, the host address here must be a non-null address:
            if (triSetup.useTransform)
                geom.geometry.triangles.transformData.hostAddress = reinterpret_cast<void*>(~0ull);
            else
                geom.geometry.triangles.transformData = MakeConstDeviceAddress();

            // Not required:
            geom.geometry.triangles.vertexData = MakeConstDeviceAddress();
            geom.geometry.triangles.vertexStride = 0;
            geom.geometry.triangles.indexData = MakeConstDeviceAddress();

            info.maxPrimitiveCounts.emplace_back(triSetup.maxTriangleCount);
        }

        for (const AABBGeometrySetup& aabbSetup : setup.aabbGeometries) {
            VkAccelerationStructureGeometryKHR& geom = info.geoms.emplace_back(geomTemplate);
            geom.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
            geom.flags = vkCastConvertibleEnumMask(aabbSetup.flags);
            geom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            geom.geometry.aabbs.pNext = nullptr;

            // Not required:
            geom.geometry.aabbs.stride = 0;
            geom.geometry.aabbs.data = MakeConstDeviceAddress();

            info.maxPrimitiveCounts.emplace_back(aabbSetup.maxAABBCount);
        }
    }

    info.geomInfo.pGeometries = info.geoms.data();
    info.geomInfo.ppGeometries = nullptr;
    info.geomInfo.scratchData = MakeDeviceAddress();

    return info;
}

}
