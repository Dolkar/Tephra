#pragma once

#include <tephra/vulkan/header.hpp>

namespace tp {

class VulkanLoader {
public:
    VulkanLoader();
    PFN_vkVoidFunction loadExportedProcedure(const char* procName) const;
    ~VulkanLoader();

private:
    void* libHandle = nullptr;
};

}
