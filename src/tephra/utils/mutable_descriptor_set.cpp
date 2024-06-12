#include "../common_impl.hpp"
#include "../device/device_container.hpp"
#include <tephra/utils/mutable_descriptor_set.hpp>
#include <algorithm>
#include <string>

namespace tp {
namespace utils {

    MutableDescriptorSet::MutableDescriptorSet(
        tp::Device* device,
        const tp::DescriptorSetLayout& layout,
        const char* debugName)
        : device(device),
          debugTarget(tp::DebugTarget(
              static_cast<tp::DeviceContainer*>(device)->getDebugTarget(),
              "MutableDescriptorSet",
              debugName)),
          changesPending(true),
          needsResolve(false) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "constructor", nullptr);

        // Make a copy of the layout with non-owning handles
        this->layout = tp::DescriptorSetLayout(
            tp::Lifeguard<tp::VkDescriptorSetLayoutHandle>::NonOwning(layout.vkGetDescriptorSetLayoutHandle()),
            tp::Lifeguard<tp::VkDescriptorUpdateTemplateHandle>::NonOwning(
                layout.vkGetDescriptorUpdateTemplateHandle()),
            layout.getBindings());
        reset();

        // Precompute starting offsets of descriptor indices for each binding for validation and setImmediate
        bindingDescriptorOffsets.reserve(layout.getBindings().size());
        uint32_t descriptorIndex = 0;
        for (const tp::DescriptorBinding& binding : layout.getBindings()) {
            bindingDescriptorOffsets.push_back(descriptorIndex);
            descriptorIndex += binding.arraySize;
        }

        vkUpdateDescriptorSets = reinterpret_cast<PFN_vkUpdateDescriptorSets>(
            device->vkLoadDeviceProcedure("vkUpdateDescriptorSets"));
        TEPHRA_ASSERT(vkUpdateDescriptorSets != nullptr);
    }

    const tp::Descriptor& MutableDescriptorSet::get(uint32_t descriptorIndex) const {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "get", std::to_string(descriptorIndex).c_str());
        TEPHRA_ASSERT(descriptorIndex < currentDescriptors.size());

        return currentDescriptors[descriptorIndex];
    }

    void MutableDescriptorSet::set(uint32_t descriptorIndex, tp::Descriptor descriptor) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "set", std::to_string(descriptorIndex).c_str());

        if constexpr (TephraValidationEnabled) {
            if (descriptorIndex >= currentDescriptors.size()) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "'descriptorIndex' (",
                    descriptorIndex,
                    ") was not smaller than the number of descriptors in the set's layout (",
                    currentDescriptors.size(),
                    ").");
            } else {
                auto [descriptorBinding, firstDescriptorOffset] = findDescriptorBinding(descriptorIndex);
                descriptor.debugValidateAgainstBinding(*descriptorBinding, descriptorIndex, true);
            }
        }

        currentDescriptors[descriptorIndex] = std::move(descriptor);
        changesPending = true;
    }

    void MutableDescriptorSet::set(uint32_t descriptorIndex, tp::FutureDescriptor descriptor) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "set", std::to_string(descriptorIndex).c_str());
        TEPHRA_ASSERT(descriptorIndex < currentDescriptors.size());

        if constexpr (TephraValidationEnabled) {
            if (descriptorIndex >= currentDescriptors.size()) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "'descriptorIndex' (",
                    descriptorIndex,
                    ") was not smaller than the number of descriptors in the set's layout (",
                    currentDescriptors.size(),
                    ").");
            }
        }

        if (needsResolve == false) {
            futureDescriptors.resize(layout.getDescriptorCount());
            needsResolve = true;
        }

        currentDescriptors[descriptorIndex] = tp::Descriptor();
        futureDescriptors[descriptorIndex] = std::move(descriptor);
        changesPending = true;
    }

    void MutableDescriptorSet::setImmediate(
        uint32_t firstDescriptorIndex,
        tp::ArrayParameter<const tp::Descriptor> descriptors) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "setImmediate", std::to_string(firstDescriptorIndex).c_str());

        if constexpr (TephraValidationEnabled) {
            if (allocatedSets.empty()) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "No descriptor sets have been committed yet since the last reset.");
            }

            if (firstDescriptorIndex + descriptors.size() > currentDescriptors.size()) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "The range of descriptors being set (",
                    firstDescriptorIndex,
                    " - ",
                    firstDescriptorIndex + descriptors.size(),
                    ") is out of range of the number of descriptor in the set's layout (",
                    currentDescriptors.size(),
                    ").");
            }
        }

        for (uint32_t i = 0; i < descriptors.size(); i++) {
            currentDescriptors[firstDescriptorIndex + i] = descriptors[i];
        }

        auto [descriptorBinding, firstDescriptorOffset] = findDescriptorBinding(firstDescriptorIndex);

        if constexpr (TephraValidationEnabled) {
            if (firstDescriptorOffset + descriptors.size() > descriptorBinding->arraySize) {
                reportDebugMessage(
                    DebugMessageSeverity::Error,
                    DebugMessageType::Validation,
                    "The range of descriptors being set (",
                    firstDescriptorIndex,
                    " - ",
                    firstDescriptorIndex + descriptors.size(),
                    ") is out of range of the associated descriptor set binding's array size (",
                    descriptorBinding->arraySize,
                    ").");
            }

            for (uint32_t i = 0; i < descriptors.size(); i++) {
                descriptors[i].debugValidateAgainstBinding(*descriptorBinding, firstDescriptorIndex + i, true);
            }
        }

        VkWriteDescriptorSet descriptorWrite;
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.pNext = nullptr;
        descriptorWrite.dstSet = allocatedSets.back().vkGetDescriptorSetHandle();
        descriptorWrite.dstBinding = descriptorBinding->bindingNumber;
        descriptorWrite.dstArrayElement = firstDescriptorOffset;
        descriptorWrite.descriptorCount = static_cast<uint32_t>(descriptors.size());
        descriptorWrite.descriptorType = vkCastConvertibleEnum(descriptorBinding->descriptorType);

        // Resolve descriptor infos and handles
        // Only one of these vectors will get used
        ScratchVector<VkDescriptorImageInfo> vkImageInfos;
        ScratchVector<VkDescriptorBufferInfo> vkBufferInfos;
        ScratchVector<VkBufferView> vkBufferViews;

        if (descriptors[0].vkResolveDescriptorImageInfo() != nullptr) {
            // Deduce image layout
            VkImageLayout imageLayout = vkGetImageLayoutForDescriptor(
                descriptorBinding->descriptorType,
                descriptorBinding->flags.contains(DescriptorBindingFlag::AliasStorageImage));

            vkImageInfos.reserve(descriptors.size());
            for (const tp::Descriptor& descriptor : descriptors) {
                vkImageInfos.push_back(*descriptor.vkResolveDescriptorImageInfo());
                vkImageInfos.back().imageLayout = imageLayout;
            }
            descriptorWrite.pImageInfo = vkImageInfos.data();
        } else if (descriptors[0].vkResolveDescriptorBufferInfo() != nullptr) {
            vkBufferInfos.reserve(descriptors.size());
            for (const tp::Descriptor& descriptor : descriptors) {
                vkBufferInfos.push_back(*descriptor.vkResolveDescriptorBufferInfo());
            }
            descriptorWrite.pBufferInfo = vkBufferInfos.data();
        } else {
            TEPHRA_ASSERT(descriptors[0].vkResolveDescriptorBufferViewHandle() != nullptr);
            vkBufferViews.reserve(descriptors.size());
            for (const tp::Descriptor& descriptor : descriptors) {
                vkBufferViews.push_back(*descriptor.vkResolveDescriptorBufferViewHandle());
            }
            descriptorWrite.pTexelBufferView = vkBufferViews.data();
        }

        vkUpdateDescriptorSets(device->vkGetDeviceHandle(), 1, &descriptorWrite, 0, nullptr);
    }

    void MutableDescriptorSet::copyDescriptors(const MutableDescriptorSet& other) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "copyDescriptors", other.debugTarget->getObjectName());
        TEPHRA_ASSERT(layout.vkGetDescriptorSetLayoutHandle() == other.layout.vkGetDescriptorSetLayoutHandle());
        TEPHRA_ASSERT(currentDescriptors.size() == other.currentDescriptors.size());

        currentDescriptors = other.currentDescriptors;
        futureDescriptors = other.futureDescriptors;
        needsResolve = other.needsResolve;
        changesPending = true;
    }

    tp::DescriptorSetView MutableDescriptorSet::commit(tp::DescriptorPool& pool) {
        TEPHRA_DEBUG_SET_CONTEXT(debugTarget.get(), "commit", nullptr);

        if (!changesPending) {
            if (allocatedSets.empty())
                return tp::DescriptorSetView();
            else
                return allocatedSets.back().getView();
        }
        if (needsResolve)
            doResolve();

        auto setSetup = tp::DescriptorSetSetup(
            tp::view(currentDescriptors),
            // We want to support null descriptor sets here
            tp::DescriptorSetFlag::IgnoreNullDescriptors,
            debugTarget->getObjectName());
        allocatedSets.push_back(tp::DescriptorSet());
        pool.allocateDescriptorSets(&layout, { setSetup }, { &allocatedSets.back() });

        changesPending = false;
        return allocatedSets.back().getView();
    }

    void MutableDescriptorSet::reset() {
        currentDescriptors.clear();
        currentDescriptors.resize(layout.getDescriptorCount());
        futureDescriptors.clear();
        changesPending = true;
        needsResolve = false;
    }

    void MutableDescriptorSet::releaseAndReset() {
        allocatedSets.clear();
        reset();
    }

    void MutableDescriptorSet::doResolve() {
        TEPHRA_ASSERT(currentDescriptors.size() == futureDescriptors.size());
        for (std::size_t descriptorIndex = 0; descriptorIndex < currentDescriptors.size(); descriptorIndex++) {
            if (currentDescriptors[descriptorIndex].isNull() && !futureDescriptors[descriptorIndex].isNull()) {
                currentDescriptors[descriptorIndex] = futureDescriptors[descriptorIndex].resolve();
                futureDescriptors[descriptorIndex] = {};

                if constexpr (TephraValidationEnabled) {
                    auto [descriptorBinding, firstDescriptorOffset] = findDescriptorBinding(descriptorIndex);
                    currentDescriptors[descriptorIndex].debugValidateAgainstBinding(
                        *descriptorBinding, descriptorIndex, true);
                }
                TEPHRA_ASSERT(!currentDescriptors[descriptorIndex].isNull());
            }
        }
        needsResolve = false;
    }

    std::pair<const DescriptorBinding*, uint32_t> MutableDescriptorSet::findDescriptorBinding(
        std::size_t descriptorIndex) const {
        auto it = std::upper_bound(bindingDescriptorOffsets.begin(), bindingDescriptorOffsets.end(), descriptorIndex);
        int bindingIndex = static_cast<int>(it - bindingDescriptorOffsets.begin()) - 1;
        TEPHRA_ASSERT(bindingIndex >= 0 && bindingIndex < bindingDescriptorOffsets.size());

        return { &layout.getBindings()[bindingIndex], bindingDescriptorOffsets[bindingIndex] };
    }

}
}
