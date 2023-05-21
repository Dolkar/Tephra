#pragma once

namespace tp {

class VulkanLoader {
public:
    VulkanLoader();
    void* loadExportedProcedure(const char* procName) const;
    ~VulkanLoader();

private:
    void* libHandle = nullptr;
};

}
