
#include "acceleration_structure_impl.hpp"
#include "device/device_container.hpp"
#include "job/local_acceleration_structures.hpp"
#include "job/local_buffers.hpp"

namespace tp {

AccelerationStructureBuilder::AccelerationStructureBuilder(
    DeviceContainer* deviceImpl,
    const AccelerationStructureSetup& setup) {
    reset(setup);

    // Query acceleration structure sizes
    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo = makeVkBuildInfo();
    vkBuildSizes = deviceImpl->getLogicalDevice()->getAccelerationStructureBuildSizes(
        vkBuildInfo, maxPrimitiveCounts.data());
}

std::pair<VkAccelerationStructureBuildGeometryInfoKHR, const VkAccelerationStructureBuildRangeInfoKHR*>
AccelerationStructureBuilder::prepareBuild(
    StoredAccelerationStructureBuildInfo& buildInfo,
    StoredBufferView& scratchBuffer) {
    auto getCheckedDeviceAddress = [](StoredBufferView& buffer) {
        DeviceAddress address = { buffer.getDeviceAddress() };
        TEPHRA_ASSERT(address != 0 || buffer.isNull());
        return address;
    };

    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo = makeVkBuildInfo(buildInfo.mode);

    if (!buildInfo.srcView.isNull())
        vkBuildInfo.srcAccelerationStructure = buildInfo.srcView.vkGetAccelerationStructureHandle();
    vkBuildInfo.dstAccelerationStructure = buildInfo.dstView.vkGetAccelerationStructureHandle();
    vkBuildInfo.scratchData = { getCheckedDeviceAddress(scratchBuffer) };

    if (type == AccelerationStructureType::TopLevel) {
        // Instance geometry
        TEPHRA_ASSERT(vkGeometries.size() == 1);
        TEPHRA_ASSERT(vkGeometries[0].geometryType == VK_GEOMETRY_TYPE_INSTANCES_KHR);
        auto& instanceGeom = vkGeometries[0].geometry.instances;

        instanceGeom.arrayOfPointers = buildInfo.instanceGeometry.arrayOfPointers;
        StoredBufferView& instanceData = buildInfo.instanceGeometry.instanceBuffer;
        std::size_t instanceSize = buildInfo.instanceGeometry.arrayOfPointers ?
            sizeof(DeviceAddress) :
            sizeof(VkAccelerationStructureInstanceKHR);

        instanceGeom.data = { getCheckedDeviceAddress(instanceData) };
        std::size_t instanceCount = instanceData.getSize() / instanceSize;

        // We never set the offsets in build ranges to anything other than 0
        vkBuildRanges[0].primitiveCount = static_cast<uint32_t>(instanceCount);
    } else {
        std::size_t geometryCount = buildInfo.triangleGeometries.size() + buildInfo.aabbGeometries.size();
        TEPHRA_ASSERT(vkGeometries.size() == geometryCount);

        std::size_t geomIndex = 0;
        for (StoredTriangleGeometryBuildInfo& triInfo : buildInfo.triangleGeometries) {
            TEPHRA_ASSERT(vkGeometries[geomIndex].geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR);
            auto& triangleGeom = vkGeometries[geomIndex].geometry.triangles;

            triangleGeom.vertexData = { getCheckedDeviceAddress(triInfo.vertexBuffer) };
            triangleGeom.vertexStride = triInfo.vertexStride;
            if (triangleGeom.vertexStride == 0) {
                Format vertexFormat = vkCastConvertibleEnum(triangleGeom.vertexFormat);
                triangleGeom.vertexStride = getFormatClassProperties(vertexFormat).texelBlockBytes;
            }

            // Optional buffer views, but getDeviceAddress on a null view returns 0
            triangleGeom.indexData = { getCheckedDeviceAddress(triInfo.indexBuffer) };
            triangleGeom.transformData = { getCheckedDeviceAddress(triInfo.transformBuffer) };

            std::size_t triangleCount;
            if (!triInfo.indexBuffer.isNull()) {
                TEPHRA_ASSERT(triangleGeom.indexType != VK_INDEX_TYPE_NONE_KHR);
                uint32_t indexSize = triangleGeom.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
                triangleCount = triInfo.indexBuffer.getSize() / (3 * indexSize);
            } else {
                triangleCount = (triInfo.vertexBuffer.getSize() - triInfo.firstVertex) /
                    (3 * triangleGeom.vertexStride);
            }

            vkBuildRanges[geomIndex].primitiveCount = static_cast<uint32_t>(triangleCount);
            vkBuildRanges[geomIndex].firstVertex = triInfo.firstVertex;
            geomIndex++;
        }

        for (StoredAABBGeometryBuildInfo& aabbInfo : buildInfo.aabbGeometries) {
            TEPHRA_ASSERT(vkGeometries[geomIndex].geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
            auto& aabbGeom = vkGeometries[geomIndex].geometry.aabbs;

            aabbGeom.data = { getCheckedDeviceAddress(aabbInfo.aabbBuffer) };
            aabbGeom.stride = aabbInfo.stride;

            std::size_t aabbCount = aabbInfo.aabbBuffer.getSize() / aabbInfo.stride;
            vkBuildRanges[geomIndex].primitiveCount = static_cast<uint32_t>(aabbCount);
            geomIndex++;
        }
    }

    return { vkBuildInfo, vkBuildRanges.data() };
}

void AccelerationStructureBuilder::reset(const AccelerationStructureSetup& setup) {
    type = setup.type;
    buildFlags = setup.buildFlags;

    // Initialize the geometries and maxPrimitiveCounts array with null resources according to the setup
    vkGeometries.clear();
    maxPrimitiveCounts.clear();

    // Prepare geometry template
    VkAccelerationStructureGeometryKHR geomTemplate;
    geomTemplate.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR;
    geomTemplate.pNext = nullptr;

    if (setup.type == AccelerationStructureType::TopLevel) {
        VkAccelerationStructureGeometryKHR& geom = vkGeometries.emplace_back(geomTemplate);
        geom.geometryType = VK_GEOMETRY_TYPE_INSTANCES_KHR;
        geom.flags = vkCastConvertibleEnumMask(setup.instanceGeometry.flags);
        geom.geometry.instances.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR;
        geom.geometry.instances.pNext = nullptr;

        // Not set yet:
        geom.geometry.instances.arrayOfPointers = false;
        geom.geometry.instances.data = { VkDeviceAddress(0) };

        maxPrimitiveCounts.emplace_back(setup.instanceGeometry.maxInstanceCount);
    } else {
        TEPHRA_ASSERT(setup.type == AccelerationStructureType::BottomLevel);

        std::size_t geometryCount = setup.triangleGeometries.size() + setup.aabbGeometries.size();
        vkGeometries.reserve(geometryCount);
        maxPrimitiveCounts.reserve(geometryCount);

        // Triangles, then AABBs
        for (const TriangleGeometrySetup& triSetup : setup.triangleGeometries) {
            VkAccelerationStructureGeometryKHR& geom = vkGeometries.emplace_back(geomTemplate);
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
                geom.geometry.triangles.transformData = { VkDeviceAddress(0) };

            // Not set yet:
            geom.geometry.triangles.vertexData = { VkDeviceAddress(0) };
            geom.geometry.triangles.vertexStride = 0;
            geom.geometry.triangles.indexData = { VkDeviceAddress(0) };

            maxPrimitiveCounts.emplace_back(triSetup.maxTriangleCount);
        }

        for (const AABBGeometrySetup& aabbSetup : setup.aabbGeometries) {
            VkAccelerationStructureGeometryKHR& geom = vkGeometries.emplace_back(geomTemplate);
            geom.geometryType = VK_GEOMETRY_TYPE_AABBS_NV;
            geom.flags = vkCastConvertibleEnumMask(aabbSetup.flags);
            geom.geometry.aabbs.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_AABBS_DATA_KHR;
            geom.geometry.aabbs.pNext = nullptr;

            // Not set yet:
            geom.geometry.aabbs.data = { VkDeviceAddress(0) };
            geom.geometry.aabbs.stride = 0;

            maxPrimitiveCounts.emplace_back(aabbSetup.maxAABBCount);
        }
    }

    VkAccelerationStructureBuildRangeInfoKHR buildRangeTemplate;
    buildRangeTemplate.primitiveCount = 0;
    buildRangeTemplate.primitiveOffset = 0;
    buildRangeTemplate.firstVertex = 0;
    buildRangeTemplate.transformOffset = 0;
    vkBuildRanges.resize(vkGeometries.size(), buildRangeTemplate);
}

VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuilder::makeVkBuildInfo(
    AccelerationStructureBuildMode buildMode) const {
    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo;
    vkBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    vkBuildInfo.pNext = nullptr;
    vkBuildInfo.type = vkCastConvertibleEnum(type);
    vkBuildInfo.flags = vkCastConvertibleEnumMask(buildFlags);
    vkBuildInfo.mode = vkCastConvertibleEnum(buildMode);
    vkBuildInfo.geometryCount = static_cast<uint32_t>(vkGeometries.size());
    vkBuildInfo.pGeometries = vkGeometries.data();
    vkBuildInfo.ppGeometries = nullptr;

    // Not set yet:
    vkBuildInfo.srcAccelerationStructure = VK_NULL_HANDLE;
    vkBuildInfo.dstAccelerationStructure = VK_NULL_HANDLE;
    vkBuildInfo.scratchData = { VkDeviceAddress(0) };

    return vkBuildInfo;
}

AccelerationStructureBaseImpl::AccelerationStructureBaseImpl(
    DeviceContainer* deviceImpl,
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle)
    : deviceImpl(deviceImpl) {
    assignHandle(std::move(accelerationStructureHandle));
}

void AccelerationStructureBaseImpl::assignHandle(
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle) {
    if (!accelerationStructureHandle.isNull()) {
        this->accelerationStructureHandle = std::move(accelerationStructureHandle);
        this->deviceAddress = deviceImpl->getLogicalDevice()->getAccelerationStructureDeviceAddress(
            this->accelerationStructureHandle.vkGetHandle());
    }
}

AccelerationStructureImpl::AccelerationStructureImpl(
    DeviceContainer* deviceImpl,
    std::shared_ptr<AccelerationStructureBuilder> builder,
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle,
    OwningPtr<Buffer> backingBuffer,
    DebugTarget debugTarget)
    : AccelerationStructureBaseImpl(deviceImpl, std::move(accelerationStructureHandle)),
      debugTarget(std::move(debugTarget)),
      backingBuffer(std::move(backingBuffer)),
      builder(std::move(builder)) {}

AccelerationStructureImpl& AccelerationStructureImpl::getAccelerationStructureImpl(
    const AccelerationStructureView& asView) {
    TEPHRA_ASSERT(!asView.isNull());
    TEPHRA_ASSERT(!asView.viewsJobLocalAccelerationStructure());
    return *std::get<AccelerationStructureImpl*>(asView.accelerationStructure);
}
}
