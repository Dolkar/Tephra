#pragma once

#include "../common_impl.hpp"
#include "../descriptor_pool_impl.hpp"
#include <tephra/descriptor.hpp>
#include <string>

namespace tp {

class JobLocalDescriptorSets {
public:
    explicit JobLocalDescriptorSets(DescriptorPool* descriptorPool)
        : descriptorPoolImpl(static_cast<DescriptorPoolImpl*>(descriptorPool)) {}

    DescriptorSetView prepareNewDescriptorSet(
        const DescriptorSetLayout* descriptorSetLayout,
        ArrayParameter<const FutureDescriptor> descriptors,
        const char* debugName);

    void allocatePreparedDescriptorSets();

    void freeAllocatedDescriptorSets();

    void clear();

    TEPHRA_MAKE_NONCOPYABLE(JobLocalDescriptorSets);
    TEPHRA_MAKE_NONMOVABLE(JobLocalDescriptorSets);
    ~JobLocalDescriptorSets() = default;

private:
    struct SetToAllocate {
        const DescriptorSetLayout* descriptorLayout;
        std::size_t descriptorCount;
        std::string debugName;
    };

    struct AllocatedSet {
        VkDescriptorSetHandle vkDescriptorSetHandle;
        DescriptorPoolEntry* poolEntry;
    };

    DescriptorPoolImpl* descriptorPoolImpl;
    std::vector<SetToAllocate> setsToAllocate;
    // Shared descriptor array for all the sets
    std::vector<FutureDescriptor> localDescriptors;
    // Stationary handles for each set that will be allocated
    std::deque<AllocatedSet> allocatedSetHandles;
};

}
