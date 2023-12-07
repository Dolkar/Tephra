#pragma once

#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>
#include <tephra/buffer.hpp>

namespace tp {

struct AccelerationStructureInfo {
    VkAccelerationStructureBuildGeometryInfoKHR geomInfo;
    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<uint32_t> maxPrimitiveCounts;

    AccelerationStructureInfo() = default;
    TEPHRA_MAKE_NONCOPYABLE(AccelerationStructureInfo);
    TEPHRA_MAKE_MOVABLE_DEFAULT(AccelerationStructureInfo);
};

class AccelerationStructureImpl : public AccelerationStructure {
public:
    AccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        const AccelerationStructureSetup& setup,
        AccelerationStructureInfo info,
        const VkAccelerationStructureBuildSizesInfoKHR& vkBuildSizes,
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle,
        BufferView backingBufferView,
        OwningPtr<Buffer> backingBufferOwningPtr,
        DebugTarget debugTarget);

    uint64_t getScratchBufferSize(AccelerationStructureBuildMode buildMode);

    // Prepares build info with only enough information needed to query the required size
    static AccelerationStructureInfo prepareInfoForSizeQuery(const AccelerationStructureSetup& setup);

    static AccelerationStructureImpl& getAccelerationStructureImpl(const AccelerationStructureView& asView);

private:
    static VkDeviceOrHostAddressKHR MakeDeviceAddress(VkDeviceAddress address = 0) {
        return { address };
    }

    static VkDeviceOrHostAddressConstKHR MakeConstDeviceAddress(VkDeviceAddress address = 0) {
        return { address };
    }

    DebugTarget debugTarget;
    DeviceContainer* deviceImpl;
    Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle;
    BufferView backingBufferView;
    // Optional owning reference to the backing pointer. The AS may but does not have to own its backing buffer.
    OwningPtr<Buffer> backingBufferOwningPtr;
};

}
