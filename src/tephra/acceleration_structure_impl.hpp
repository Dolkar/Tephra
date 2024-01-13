#pragma once

#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>
#include <tephra/buffer.hpp>

namespace tp {

class AccelerationStructureBuilder {
public:
    AccelerationStructureBuilder() = default;
    AccelerationStructureBuilder(DeviceContainer* deviceImpl, const AccelerationStructureSetup& setup);

    TEPHRA_MAKE_NONCOPYABLE(AccelerationStructureBuilder);
    TEPHRA_MAKE_MOVABLE_DEFAULT(AccelerationStructureBuilder);

    uint64_t getStorageSize() const {
        return vkBuildSizes.accelerationStructureSize;
    }

    uint64_t getScratchBufferSize(AccelerationStructureBuildMode buildMode) const {
        if (buildMode == AccelerationStructureBuildMode::Build)
            return vkBuildSizes.buildScratchSize;
        else
            return vkBuildSizes.updateScratchSize;
    }

    std::pair<VkAccelerationStructureBuildGeometryInfoKHR, const VkAccelerationStructureBuildRangeInfoKHR*> prepareBuild(
        const AccelerationStructureBuildInfo& buildInfo,
        const BufferView& scratchBuffer);

private:
    // Initializes the geometries and maxPrimitiveCounts array with null resources from setup structure
    void initGeometries(const AccelerationStructureSetup& setup);
    // Makes the Vulkan build info structure with null resources from setup structure
    VkAccelerationStructureBuildGeometryInfoKHR makeVkBuildInfo(
        AccelerationStructureBuildMode buildMode = AccelerationStructureBuildMode::Build) const;

    AccelerationStructureType type;
    AccelerationStructureBuildFlagMask buildFlags;
    std::vector<VkAccelerationStructureGeometryKHR> vkGeometries;
    std::vector<uint32_t> maxPrimitiveCounts;
    std::vector<VkAccelerationStructureBuildRangeInfoKHR> vkBuildRanges;
    VkAccelerationStructureBuildSizesInfoKHR vkBuildSizes;
};

// Helper base class containing a common part of the implementations
class AccelerationStructureBaseImpl {
public:
    AccelerationStructureBaseImpl(
        DeviceContainer* deviceImpl,
        AccelerationStructureBuilder builder,
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle);

    AccelerationStructureBuilder& getBuilder() {
        return builder;
    }

    DeviceAddress getDeviceAddress_() const {
        return deviceAddress;
    }

    VkAccelerationStructureHandleKHR vkGetAccelerationStructureHandle_() const {
        return accelerationStructureHandle.vkGetHandle();
    }

    void assignHandle(Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle);

protected:
    DeviceContainer* deviceImpl;
    AccelerationStructureBuilder builder;
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle;
    DeviceAddress deviceAddress = 0;
};

// Acceleration structure that manages its own backing buffer
class AccelerationStructureImpl : public AccelerationStructure, public AccelerationStructureBaseImpl {
public:
    AccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        AccelerationStructureBuilder builder,
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle,
        OwningPtr<Buffer> backingBuffer,
        DebugTarget debugTarget);

    BufferView getBackingBufferView_() const {
        return backingBuffer->getDefaultView();
    }

    static AccelerationStructureImpl& getAccelerationStructureImpl(const AccelerationStructureView& asView);

private:
    DebugTarget debugTarget;
    OwningPtr<Buffer> backingBuffer;
};

}
