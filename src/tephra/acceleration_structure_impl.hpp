#pragma once

#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>
#include <tephra/buffer.hpp>
#include <memory.h>

namespace tp {

struct StoredAccelerationStructureBuildInfo;
class StoredBufferView;

// Stores information needed to build or update an acceleration structure
class AccelerationStructureBuilder {
public:
    AccelerationStructureBuilder() = default;
    AccelerationStructureBuilder(DeviceContainer* deviceImpl, const AccelerationStructureSetup& setup);

    TEPHRA_MAKE_NONCOPYABLE(AccelerationStructureBuilder);
    TEPHRA_MAKE_MOVABLE_DEFAULT(AccelerationStructureBuilder);

    AccelerationStructureType getType() const {
        return type;
    }

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
        StoredAccelerationStructureBuildInfo& buildInfo,
        StoredBufferView& scratchBuffer);

    void reset(DeviceContainer* deviceImpl, const AccelerationStructureSetup& setup);

private:
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
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle);

    DeviceAddress getDeviceAddress_() const {
        return deviceAddress;
    }

    VkAccelerationStructureHandleKHR vkGetAccelerationStructureHandle_() const {
        return accelerationStructureHandle.vkGetHandle();
    }

    void assignHandle(Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle);

protected:
    DeviceContainer* deviceImpl;
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle;
    DeviceAddress deviceAddress = 0;
};

// Acceleration structure that manages its own backing buffer
class AccelerationStructureImpl : public AccelerationStructure, public AccelerationStructureBaseImpl {
public:
    AccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        std::shared_ptr<AccelerationStructureBuilder> builder,
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle,
        OwningPtr<Buffer> backingBuffer,
        DebugTarget debugTarget);

    AccelerationStructureView getView_() {
        return AccelerationStructureView(this);
    }

    const AccelerationStructureView getView_() const {
        return const_cast<AccelerationStructureImpl*>(this)->getView_();
    }

    const BufferView getBackingBufferView_() const {
        return backingBuffer->getDefaultView();
    }

    std::shared_ptr<AccelerationStructureBuilder>& getBuilder() {
        return builder;
    }

    static AccelerationStructureImpl& getAccelerationStructureImpl(const AccelerationStructureView& asView);

private:
    DebugTarget debugTarget;
    OwningPtr<Buffer> backingBuffer;
    // Shared ptr because jobs need temporary ownership of the builder
    std::shared_ptr<AccelerationStructureBuilder> builder;
};

}
