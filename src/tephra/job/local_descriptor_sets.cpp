#include "local_descriptor_sets.hpp"
#include "../device/device_container.hpp"

namespace tp {

DescriptorSetView JobLocalDescriptorSets::prepareNewDescriptorSet(
    const DescriptorSetLayout* descriptorSetLayout,
    ArrayParameter<const FutureDescriptor> descriptors,
    const char* debugName) {
    // Store setup information for when the descriptors can be resolved
    SetToAllocate setInfo;
    setInfo.descriptorLayout = descriptorSetLayout;
    setInfo.descriptorCount = descriptors.size();
    if (debugName != nullptr)
        setInfo.debugName = debugName;

    setsToAllocate.push_back(setInfo);

    localDescriptors.reserve(localDescriptors.size() + descriptors.size());
    for (const FutureDescriptor& descriptor : descriptors) {
        localDescriptors.push_back(descriptor);
    }

    // Create a null handle that will be overwritten when the actual set is created
    // and return a job-local DescriptorSet pointing to it
    allocatedSetHandles.push_back({ VkDescriptorSetHandle(), nullptr });
    return DescriptorSetView(&allocatedSetHandles.back().vkDescriptorSetHandle);
}

void JobLocalDescriptorSets::allocatePreparedDescriptorSets() {
    TEPHRA_ASSERT(setsToAllocate.size() == allocatedSetHandles.size());
    if (setsToAllocate.empty())
        return;

    // Resolve descriptors to actual resources
    ScratchVector<Descriptor> resolvedDescriptors;
    resolvedDescriptors.reserve(localDescriptors.size());

    for (std::size_t i = 0; i < localDescriptors.size(); i++) {
        resolvedDescriptors.push_back(localDescriptors[i].resolve());
    }
    localDescriptors.clear();

    // Fill descriptor set setups and allocate the sets, grouped by layout
    ScratchVector<DescriptorSetSetup> descriptorSetSetups;
    ScratchVector<VkDescriptorSetHandle> vkAllocatedDescriptorSets;

    std::size_t descriptorIndex = 0;
    for (std::size_t i = 0; i < setsToAllocate.size(); i++) {
        const SetToAllocate& setInfo = setsToAllocate[i];
        descriptorSetSetups.emplace_back(viewRange(resolvedDescriptors, descriptorIndex, setInfo.descriptorCount));
        descriptorIndex += setInfo.descriptorCount;

        bool isLastSet = i == setsToAllocate.size() - 1;
        if (isLastSet || setInfo.descriptorLayout != setsToAllocate[i + 1].descriptorLayout) {
            // Allocate descriptor sets from the pool and set future handles
            vkAllocatedDescriptorSets.resize(descriptorSetSetups.size());

            tp::DescriptorPoolEntry* poolEntry = descriptorPoolImpl->allocateDescriptorSets_(
                setInfo.descriptorLayout, view(descriptorSetSetups), view(vkAllocatedDescriptorSets));

            for (std::size_t j = 0; j < setsToAllocate.size(); j++) {
                allocatedSetHandles[j].vkDescriptorSetHandle = vkAllocatedDescriptorSets[j];
                allocatedSetHandles[j].poolEntry = poolEntry;
                descriptorPoolImpl->getParentDeviceImpl()->getLogicalDevice()->setObjectDebugName(
                    vkAllocatedDescriptorSets[j], setsToAllocate[j].debugName.c_str());
            }

            descriptorSetSetups.clear();
        }
    }

    TEPHRA_ASSERT(descriptorSetSetups.empty());
    setsToAllocate.clear();
}

void JobLocalDescriptorSets::freeAllocatedDescriptorSets() {
    // This method is called when the job using these has already finished, so free them immediately
    for (std::size_t i = 0; i < allocatedSetHandles.size(); i++) {
        descriptorPoolImpl->queueFreeDescriptorSet(
            allocatedSetHandles[i].vkDescriptorSetHandle, allocatedSetHandles[i].poolEntry, 0);
    }
    allocatedSetHandles.clear();
}

void JobLocalDescriptorSets::clear() {
    setsToAllocate.clear();
    localDescriptors.clear();
    allocatedSetHandles.clear();
}

}
