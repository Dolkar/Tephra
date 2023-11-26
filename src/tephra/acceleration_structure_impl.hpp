#pragma once

#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>

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
    // Prepares build info with only enough information needed to query the required size
    static AccelerationStructureBuildInfo prepareBuildInfoForSizeQuery(const AccelerationStructureSetup& setup);

private:
    static VkDeviceOrHostAddressKHR MakeDeviceAddress(VkDeviceAddress address = 0) {
        return { address };
    }

    static VkDeviceOrHostAddressConstKHR MakeConstDeviceAddress(VkDeviceAddress address = 0) {
        return { address };
    }
};

}
