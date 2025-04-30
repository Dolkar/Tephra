
#include "acceleration_structure_impl.hpp"
#include "device/device_container.hpp"
#include "job/local_acceleration_structures.hpp"
#include "job/local_buffers.hpp"

namespace tp {

AccelerationStructureBuilder::AccelerationStructureBuilder(
    DeviceContainer* deviceImpl,
    const AccelerationStructureSetup& setup) {
    reset(deviceImpl, setup);
}

VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuilder::prepareBuild(
    StoredAccelerationStructureBuildInfo& buildInfo,
    StoredBufferView& scratchBuffer,
    ArrayView<VkAccelerationStructureBuildRangeInfoKHR> vkBuildRanges) {
    TEPHRA_ASSERT(vkBuildRanges.size() == vkGeometries.size());

    // Offsets remain zero, the rest gets filled depending on the geometry
    VkAccelerationStructureBuildRangeInfoKHR buildRangeTemplate;
    buildRangeTemplate.primitiveCount = 0;
    buildRangeTemplate.primitiveOffset = 0;
    buildRangeTemplate.firstVertex = 0;
    buildRangeTemplate.transformOffset = 0;
    for (std::size_t i = 0; i < vkBuildRanges.size(); i++) {
        vkBuildRanges[i] = buildRangeTemplate;
    }

    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo = prepareBuildInfo(buildInfo, scratchBuffer);

    // Fill out build ranges
    if (type == AccelerationStructureType::TopLevel) {
        // Instance geometry
        StoredBufferView& instanceData = buildInfo.instanceGeometry.instanceBuffer;
        std::size_t instanceSize = buildInfo.instanceGeometry.arrayOfPointers ?
            sizeof(DeviceAddress) :
            sizeof(VkAccelerationStructureInstanceKHR);

        std::size_t instanceCount = instanceData.getSize() / instanceSize;

        vkBuildRanges[0].primitiveCount = static_cast<uint32_t>(instanceCount);
    } else {
        std::size_t geomIndex = 0;
        for (StoredTriangleGeometryBuildInfo& triInfo : buildInfo.triangleGeometries) {
            // Calculate triangle count
            const auto& triangleGeom = vkGeometries[geomIndex].geometry.triangles;

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
            std::size_t aabbCount = aabbInfo.aabbBuffer.getSize() / aabbInfo.stride;
            vkBuildRanges[geomIndex].primitiveCount = static_cast<uint32_t>(aabbCount);
            geomIndex++;
        }
    }

    return vkBuildInfo;
}

VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuilder::prepareBuildIndirect(
    StoredAccelerationStructureBuildInfo& buildInfo,
    StoredBufferView& scratchBuffer) {
    // Filling out build ranges indirectly is the app's responsibility
    return prepareBuildInfo(buildInfo, scratchBuffer);
}

void AccelerationStructureBuilder::reset(DeviceContainer* deviceImpl, const AccelerationStructureSetup& setup) {
    type = setup.type;
    flags = setup.flags;

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
            geom.geometry.triangles.indexData = { VkDeviceAddress(0) };
            geom.geometry.triangles.vertexStride = 0;

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

    // Query acceleration structure sizes
    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo = initVkBuildInfo();
    vkBuildSizes = deviceImpl->getLogicalDevice()->getAccelerationStructureBuildSizes(
        vkBuildInfo, maxPrimitiveCounts.data());
}

AccelerationStructureBuilder& AccelerationStructureBuilder::getBuilderFromView(const AccelerationStructureView& asView) {
    TEPHRA_ASSERT(!asView.isNull());
    if (asView.viewsJobLocalAccelerationStructure()) {
        return *JobLocalAccelerationStructureImpl::getAccelerationStructureImpl(asView).getBuilder();
    } else {
        return *AccelerationStructureImpl::getAccelerationStructureImpl(asView).getBuilder();
    }
}

VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuilder::prepareBuildInfo(
    StoredAccelerationStructureBuildInfo& buildInfo,
    StoredBufferView& scratchBuffer) {
    auto getCheckedDeviceAddress = [](StoredBufferView& buffer) {
        DeviceAddress address = { buffer.getDeviceAddress() };
        TEPHRA_ASSERT(address != 0 || buffer.isNull());
        return address;
    };

    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo = initVkBuildInfo(buildInfo.mode);

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
        instanceGeom.data = { getCheckedDeviceAddress(instanceData) };
    } else {
        std::size_t geometryCount = buildInfo.triangleGeometries.size() + buildInfo.aabbGeometries.size();
        TEPHRA_ASSERT(vkGeometries.size() == geometryCount);

        std::size_t geomIndex = 0;
        for (StoredTriangleGeometryBuildInfo& triInfo : buildInfo.triangleGeometries) {
            TEPHRA_ASSERT(vkGeometries[geomIndex].geometryType == VK_GEOMETRY_TYPE_TRIANGLES_KHR);
            auto& triGeom = vkGeometries[geomIndex].geometry.triangles;

            triGeom.vertexData = { getCheckedDeviceAddress(triInfo.vertexBuffer) };
            if (triInfo.vertexStride != 0)
                triGeom.vertexStride = triInfo.vertexStride;
            else
                triGeom.vertexStride = getFormatClassProperties(vkCastConvertibleEnum(triGeom.vertexFormat))
                                           .texelBlockBytes;

            // Optional buffer views, but getCheckedDeviceAddress on a null view returns 0
            triGeom.indexData = { getCheckedDeviceAddress(triInfo.indexBuffer) };
            triGeom.transformData = { getCheckedDeviceAddress(triInfo.transformBuffer) };
            geomIndex++;
        }

        for (StoredAABBGeometryBuildInfo& aabbInfo : buildInfo.aabbGeometries) {
            TEPHRA_ASSERT(vkGeometries[geomIndex].geometryType == VK_GEOMETRY_TYPE_AABBS_KHR);
            auto& aabbGeom = vkGeometries[geomIndex].geometry.aabbs;

            aabbGeom.data = { getCheckedDeviceAddress(aabbInfo.aabbBuffer) };
            aabbGeom.stride = aabbInfo.stride;
            geomIndex++;
        }
    }

    return vkBuildInfo;
}

VkAccelerationStructureBuildGeometryInfoKHR AccelerationStructureBuilder::initVkBuildInfo(
    AccelerationStructureBuildMode buildMode) const {
    VkAccelerationStructureBuildGeometryInfoKHR vkBuildInfo;
    vkBuildInfo.sType = VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR;
    vkBuildInfo.pNext = nullptr;
    vkBuildInfo.type = vkCastConvertibleEnum(type);
    vkBuildInfo.flags = vkCastConvertibleEnumMask(flags);
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

AccelerationStructureQueryKHR& AccelerationStructureImpl::getOrCreateCompactedSizeQuery() {
    TEPHRA_ASSERT(builder->getFlags().contains(tp::AccelerationStructureFlag::AllowCompaction));
    if (compactedSizeQuery.isNull()) {
        deviceImpl->getQueryManager()->createAccelerationStructureQueriesKHR({ &compactedSizeQuery });
    }
    return compactedSizeQuery;
}

AccelerationStructureImpl& AccelerationStructureImpl::getAccelerationStructureImpl(
    const AccelerationStructureView& asView) {
    TEPHRA_ASSERT(!asView.isNull());
    TEPHRA_ASSERT(!asView.viewsJobLocalAccelerationStructure());
    return *std::get<AccelerationStructureImpl*>(asView.accelerationStructure);
}

// --- Validation ---

void AccelerationStructureBuilder::validateBuildInfo(const AccelerationStructureBuildInfo& buildInfo, int buildIndex)
    const {
    TEPHRA_ASSERT(TephraValidationEnabled);

    // Check for proper buffer size alignments
    if (type == AccelerationStructureType::TopLevel) {
        // Should be caught by Vulkan validation, but would crash our validation
        if (buildInfo.instanceGeometry.instanceBuffer.isNull())
            return;

        std::size_t bufferSize = buildInfo.instanceGeometry.instanceBuffer.getSize();
        std::size_t instanceSize = buildInfo.instanceGeometry.arrayOfPointers ?
            sizeof(DeviceAddress) :
            sizeof(VkAccelerationStructureInstanceKHR);

        if ((bufferSize % instanceSize) != 0)
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The size of `buildInfos[",
                buildIndex,
                "].instanceGeometry.instanceBuffer' (",
                bufferSize,
                ") is not a multiple of the expected instance value size (",
                instanceSize,
                ").");
    } else {
        std::size_t geomCount = buildInfo.triangleGeometries.size() + buildInfo.aabbGeometries.size();
        if (geomCount != getGeometryCount()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The total number of triangle and / or AABB geometries in `buildInfos[",
                buildIndex,
                "]` (",
                geomCount,
                ") is different than expected (",
                getGeometryCount(),
                ").");
            return;
        }

        for (std::size_t triGeomIndex = 0; triGeomIndex < buildInfo.triangleGeometries.size(); triGeomIndex++) {
            const TriangleGeometryBuildInfo& triInfo = buildInfo.triangleGeometries[triGeomIndex];
            const auto& triGeom = vkGeometries[triGeomIndex].geometry.triangles;

            if (!triInfo.indexBuffer.isNull()) {
                TEPHRA_ASSERT(triGeom.indexType != VK_INDEX_TYPE_NONE_KHR);
                uint32_t indexSize = triGeom.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
                std::size_t triangleSize = 3 * indexSize;
                std::size_t bufferSize = triInfo.indexBuffer.getSize();

                if ((bufferSize % triangleSize) != 0)
                    reportDebugMessage(
                        DebugMessageSeverity::Error,
                        DebugMessageType::Validation,
                        "The size of `buildInfos[",
                        buildIndex,
                        "].triangleGeometries[",
                        triGeomIndex,
                        "].indexBuffer' (",
                        bufferSize,
                        ") is not a multiple of the expected triangle size (",
                        triangleSize,
                        ").");
            }

            if (!triInfo.vertexBuffer.isNull()) {
                std::size_t bufferSize = triInfo.vertexBuffer.getSize();
                std::size_t vertexStride = triInfo.vertexStride;
                // Default to vertex stride derived from format
                if (vertexStride == 0)
                    vertexStride = getFormatClassProperties(vkCastConvertibleEnum(triGeom.vertexFormat)).texelBlockBytes;

                if ((bufferSize % vertexStride) != 0)
                    reportDebugMessage(
                        DebugMessageSeverity::Error,
                        DebugMessageType::Validation,
                        "The size of `buildInfos[",
                        buildIndex,
                        "].triangleGeometries[",
                        triGeomIndex,
                        "].vertexBuffer' (",
                        bufferSize,
                        ") is not a multiple of the expected stride (",
                        vertexStride,
                        ").");
            }
        }

        for (std::size_t aabbGeomIndex = 0; aabbGeomIndex < buildInfo.aabbGeometries.size(); aabbGeomIndex++) {
            const AABBGeometryBuildInfo& aabbInfo = buildInfo.aabbGeometries[aabbGeomIndex];
            if (!aabbInfo.aabbBuffer.isNull()) {
                std::size_t bufferSize = aabbInfo.aabbBuffer.getSize();
                if ((bufferSize % aabbInfo.stride) != 0)
                    reportDebugMessage(
                        DebugMessageSeverity::Error,
                        DebugMessageType::Validation,
                        "The size of `buildInfos[",
                        buildIndex,
                        "].aabbGeometries[",
                        aabbGeomIndex,
                        "].aabbBuffer' (",
                        bufferSize,
                        ") is not a multiple of the expected stride (",
                        aabbInfo.stride,
                        ").");
            }
        }
    }
}

void AccelerationStructureBuilder::validateBuildIndirectInfo(
    const AccelerationStructureBuildInfo& buildInfo,
    const AccelerationStructureBuildIndirectInfo& indirectInfo,
    int buildIndex) const {
    TEPHRA_ASSERT(TephraValidationEnabled);

    // Should be caught by Vulkan validation, but would crash our validation
    if (indirectInfo.buildRangeBuffer.isNull())
        return;

    // Check indirectInfo array and buffer sizes based on the geometry count
    std::size_t geomCount;
    if (type == AccelerationStructureType::TopLevel) {
        geomCount = 1;
    } else {
        geomCount = buildInfo.triangleGeometries.size() + buildInfo.aabbGeometries.size();
        if (geomCount != getGeometryCount()) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "The total number of triangle and / or AABB geometries in `buildInfos[",
                buildIndex,
                "]` (",
                geomCount,
                ") is different than expected (",
                getGeometryCount(),
                ").");
            return;
        }
    }

    if (indirectInfo.maxPrimitiveCounts.size() != geomCount) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "The size of `indirectInfos[",
            buildIndex,
            "].maxPrimitiveCounts' (",
            indirectInfo.maxPrimitiveCounts.size(),
            ") is different from the expected size (",
            geomCount,
            ").");
        return;
    }

    std::size_t indirectBufferSize = indirectInfo.buildRangeBuffer.getSize();
    std::size_t indirectBufferSizeExpected = geomCount * indirectInfo.buildRangeStride;
    if (indirectBufferSize != indirectBufferSizeExpected)
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "The size of `indirectInfos[",
            buildIndex,
            "].buildRangeBuffer' (",
            indirectBufferSize,
            ") is different from the expected size (",
            indirectBufferSizeExpected,
            ").");

    // Now, for each geometry, check that the buffers are big enough to hold the given maxPrimitiveCounts
    // Fill these arrays and validate them all at once
    ScratchVector<std::size_t> primitiveBufferSizes;
    ScratchVector<std::size_t> primitiveCapacities;
    primitiveBufferSizes.resize(geomCount);
    primitiveCapacities.resize(geomCount);

    if (type == AccelerationStructureType::TopLevel) {
        if (buildInfo.instanceGeometry.instanceBuffer.isNull())
            return;

        std::size_t bufferSize = buildInfo.instanceGeometry.instanceBuffer.getSize();
        std::size_t instanceSize = buildInfo.instanceGeometry.arrayOfPointers ?
            sizeof(DeviceAddress) :
            sizeof(VkAccelerationStructureInstanceKHR);

        std::size_t maxInstanceCount = bufferSize / instanceSize;
        TEPHRA_ASSERT(geomCount == 1);
        primitiveBufferSizes[0] = bufferSize;
        primitiveCapacities[0] = maxInstanceCount;
    } else {
        std::size_t geomIndex = 0;
        for (std::size_t triGeomIndex = 0; triGeomIndex < buildInfo.triangleGeometries.size(); triGeomIndex++) {
            const TriangleGeometryBuildInfo& triInfo = buildInfo.triangleGeometries[triGeomIndex];
            const auto& triGeom = vkGeometries[triGeomIndex].geometry.triangles;

            std::size_t bufferSize = 0;
            std::size_t maxTriangleCount = 0;
            if (!triInfo.indexBuffer.isNull()) {
                bufferSize = triInfo.indexBuffer.getSize();
                TEPHRA_ASSERT(triGeom.indexType != VK_INDEX_TYPE_NONE_KHR);
                uint32_t indexSize = triGeom.indexType == VK_INDEX_TYPE_UINT16 ? 2 : 4;
                maxTriangleCount = bufferSize / (3 * indexSize);
            } else if (!triInfo.vertexBuffer.isNull()) {
                bufferSize = triInfo.vertexBuffer.getSize();
                uint64_t vertexStride = triInfo.vertexStride;
                // Default to vertex stride derived from format
                if (vertexStride == 0)
                    vertexStride = getFormatClassProperties(vkCastConvertibleEnum(triGeom.vertexFormat)).texelBlockBytes;
                maxTriangleCount = bufferSize / (3 * vertexStride);
            }

            primitiveBufferSizes[geomIndex] = bufferSize;
            primitiveCapacities[geomIndex] = maxTriangleCount;
            geomIndex++;
        }

        for (std::size_t aabbGeomIndex = 0; aabbGeomIndex < buildInfo.aabbGeometries.size(); aabbGeomIndex++) {
            const AABBGeometryBuildInfo& aabbInfo = buildInfo.aabbGeometries[aabbGeomIndex];
            if (!aabbInfo.aabbBuffer.isNull()) {
                primitiveBufferSizes[geomIndex] = aabbInfo.aabbBuffer.getSize();
                primitiveCapacities[geomIndex] = aabbInfo.aabbBuffer.getSize() / aabbInfo.stride;
            }
            geomIndex++;
        }
    }

    for (std::size_t geomIndex = 0; geomIndex < geomCount; geomIndex++) {
        std::size_t bufferSize = primitiveBufferSizes[geomIndex];
        std::size_t maxBufferPrimitives = primitiveCapacities[geomIndex];

        if (bufferSize != 0 && indirectInfo.maxPrimitiveCounts[geomIndex] > maxBufferPrimitives) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "`indirectInfos[",
                buildIndex,
                "].maxPrimitiveCount[",
                geomIndex,
                "]' (",
                indirectInfo.maxPrimitiveCounts[geomIndex],
                ") is bigger than the maximum number of primitives the corresponding geometry buffer can hold (",
                maxBufferPrimitives,
                ", buffer size is ",
                bufferSize,
                ").");
        }
    }
}

}
