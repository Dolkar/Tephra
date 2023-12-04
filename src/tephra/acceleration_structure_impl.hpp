#pragma once

#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>
#include <tephra/buffer.hpp>

namespace tp {

struct AccelerationStructureBuildInfo {
    VkAccelerationStructureBuildGeometryInfoKHR geomInfo;
    std::vector<VkAccelerationStructureGeometryKHR> geoms;
    std::vector<uint32_t> maxPrimitiveCounts;

    AccelerationStructureBuildInfo() = default;
    TEPHRA_MAKE_NONCOPYABLE(AccelerationStructureBuildInfo);
    TEPHRA_MAKE_MOVABLE_DEFAULT(AccelerationStructureBuildInfo);
};

class AccelerationStructureImpl : public AccelerationStructure {
public:
    AccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        const AccelerationStructureSetup& setup,
        AccelerationStructureBuildInfo buildInfo,
        const VkAccelerationStructureBuildSizesInfoKHR& vkBuildSizes,
        Lifeguard<VkAccelerationStructureHandleKHR> accelerationStructureHandle,
        BufferView backingBufferView,
        OwningPtr<Buffer> backingBufferOwningPtr,
        DebugTarget debugTarget);

    // Prepares build info with only enough information needed to query the required size
    static AccelerationStructureBuildInfo prepareBuildInfoForSizeQuery(const AccelerationStructureSetup& setup);

    static AccelerationStructureImpl* getViewImpl(const AccelerationStructureView& asView);

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
