#include "common_impl.hpp"
#include "descriptor_pool_impl.hpp"
#include "device/device_container.hpp"
#include <tephra/descriptor.hpp>
#include <tephra/buffer.hpp>

namespace tp {

enum class Descriptor::ResourceType {
    Invalid = -1,
    None = 0,
    Sampler = 1,
    Image = 2,
    CombinedImageSampler = 3,
    Buffer = 4,
    TexelBufferView = 5,
    AccelerationStructure = 6
};

DescriptorBinding::DescriptorBinding()
    : bindingNumber(0),
      descriptorType(IgnoredDescriptorType),
      stageMask(ShaderStageMask::None()),
      arraySize(0),
      flags(DescriptorBindingFlagMask::None()) {}

DescriptorBinding::DescriptorBinding(
    uint32_t bindingNumber,
    DescriptorType descriptorType,
    ShaderStageMask stageMask,
    uint32_t arraySize,
    DescriptorBindingFlagMask flags)
    : bindingNumber(bindingNumber),
      descriptorType(descriptorType),
      stageMask(stageMask),
      arraySize(arraySize),
      flags(flags) {}

DescriptorBinding::DescriptorBinding(
    uint32_t bindingNumber,
    DescriptorType descriptorType,
    ShaderStageMask stageMask,
    ArrayView<const Sampler* const> immutableSamplers,
    DescriptorBindingFlagMask flags)
    : bindingNumber(bindingNumber),
      descriptorType(descriptorType),
      stageMask(stageMask),
      arraySize(static_cast<uint32_t>(immutableSamplers.size())),
      immutableSamplers(immutableSamplers),
      flags(flags) {}

ReadAccessMask DescriptorBinding::getReadAccessMask() const {
    static constexpr int storageAccessIndex = 0;
    static constexpr int sampledAccessIndex = 1;
    static constexpr int uniformAccessIndex = 2;

    static constexpr ReadAccess vertexAccesses[] = {
        ReadAccess::VertexShaderStorage,
        ReadAccess::VertexShaderSampled,
        ReadAccess::VertexShaderUniform,
    };
    static constexpr ReadAccess tessellationControlAccesses[] = {
        ReadAccess::TessellationControlShaderStorage,
        ReadAccess::TessellationControlShaderSampled,
        ReadAccess::TessellationControlShaderUniform,
    };
    static constexpr ReadAccess tessellationEvaluationAccesses[] = {
        ReadAccess::TessellationEvaluationShaderStorage,
        ReadAccess::TessellationEvaluationShaderSampled,
        ReadAccess::TessellationEvaluationShaderUniform,
    };
    static constexpr ReadAccess geometryAccesses[] = {
        ReadAccess::GeometryShaderStorage,
        ReadAccess::GeometryShaderSampled,
        ReadAccess::GeometryShaderUniform,
    };
    static constexpr ReadAccess fragmentAccesses[] = {
        ReadAccess::FragmentShaderStorage,
        ReadAccess::FragmentShaderSampled,
        ReadAccess::FragmentShaderUniform,
    };
    static constexpr ReadAccess computeAccesses[] = {
        ReadAccess::ComputeShaderStorage,
        ReadAccess::ComputeShaderSampled,
        ReadAccess::ComputeShaderUniform,
    };

    bool aliasStorageImage = flags.contains(DescriptorBindingFlag::AliasStorageImage);
    int accessTypeIndex;
    switch (descriptorType) {
    case DescriptorType::StorageImage:
    case DescriptorType::StorageBuffer:
    case DescriptorType::StorageBufferDynamic:
    case DescriptorType::StorageTexelBuffer:
        accessTypeIndex = storageAccessIndex;
        break;
    case DescriptorType::CombinedImageSampler:
    case DescriptorType::TexelBuffer:
        accessTypeIndex = sampledAccessIndex;
        break;
    case DescriptorType::SampledImage:
        accessTypeIndex = aliasStorageImage ? storageAccessIndex : sampledAccessIndex;
        break;
    case DescriptorType::UniformBuffer:
    case DescriptorType::UniformBufferDynamic:
        accessTypeIndex = uniformAccessIndex;
        break;
    case DescriptorType::Sampler:
        return ReadAccessMask::None();
    default:
        TEPHRA_ASSERT(false);
        return ReadAccessMask::None();
    }

    ReadAccessMask accesses = ReadAccessMask::None();
    if (stageMask.contains(ShaderStage::Vertex)) {
        accesses |= vertexAccesses[accessTypeIndex];
    }
    if (stageMask.contains(ShaderStage::TessellationControl)) {
        accesses |= tessellationControlAccesses[accessTypeIndex];
    }
    if (stageMask.contains(ShaderStage::TessellationEvaluation)) {
        accesses |= tessellationEvaluationAccesses[accessTypeIndex];
    }
    if (stageMask.contains(ShaderStage::Geometry)) {
        accesses |= geometryAccesses[accessTypeIndex];
    }
    if (stageMask.contains(ShaderStage::Fragment)) {
        accesses |= fragmentAccesses[accessTypeIndex];
    }
    if (stageMask.contains(ShaderStage::Compute)) {
        accesses |= computeAccesses[accessTypeIndex];
    }

    return accesses;
}

DescriptorBinding DescriptorBinding::Empty(uint32_t arraySize) {
    return DescriptorBinding(0, IgnoredDescriptorType, ShaderStageMask::None(), arraySize);
}

// Initialize vkDescriptorBufferInfo as the largest member of the union
Descriptor::Descriptor() : resourceType(ResourceType::None), vkDescriptorBufferInfo() {}

Descriptor::Descriptor(const BufferView& bufferView) {
    VkBufferViewHandle vkBufferViewHandle = bufferView.vkGetBufferViewHandle();
    if (!vkBufferViewHandle.isNull()) {
        vkDescriptorBufferViewHandle = vkBufferViewHandle;
        resourceType = ResourceType::TexelBufferView;
    } else {
        VkBufferHandle vkBufferHandle = bufferView.vkResolveBufferHandle(&vkDescriptorBufferInfo.offset);
        if (!vkBufferHandle.isNull()) {
            vkDescriptorBufferInfo.range = bufferView.getSize();
            vkDescriptorBufferInfo.buffer = vkBufferHandle;
            resourceType = ResourceType::Buffer;
        } else {
            resourceType = bufferView.isNull() ? ResourceType::None : ResourceType::Invalid;
            vkDescriptorBufferInfo = {};
        }
    }
}

Descriptor::Descriptor(const ImageView& imageView) {
    VkImageViewHandle vkImageViewHandle = imageView.vkGetImageViewHandle();
    if (!vkImageViewHandle.isNull()) {
        vkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkDescriptorImageInfo.imageView = vkImageViewHandle;
        vkDescriptorImageInfo.sampler = VK_NULL_HANDLE;
        resourceType = ResourceType::Image;
    } else {
        resourceType = imageView.isNull() ? ResourceType::None : ResourceType::Invalid;
        vkDescriptorBufferInfo = {};
    }
}

Descriptor::Descriptor(const ImageView& imageView, const Sampler& sampler) {
    VkImageViewHandle vkImageViewHandle = imageView.vkGetImageViewHandle();
    if (!vkImageViewHandle.isNull()) {
        vkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkDescriptorImageInfo.imageView = vkImageViewHandle;
        vkDescriptorImageInfo.sampler = sampler.vkGetSamplerHandle();
        resourceType = ResourceType::CombinedImageSampler;
    } else {
        resourceType = imageView.isNull() ? ResourceType::None : ResourceType::Invalid;
        vkDescriptorBufferInfo = {};
    }
}

Descriptor::Descriptor(const Sampler& sampler) {
    if (!sampler.isNull()) {
        vkDescriptorImageInfo.imageLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        vkDescriptorImageInfo.imageView = VK_NULL_HANDLE;
        vkDescriptorImageInfo.sampler = sampler.vkGetSamplerHandle();
        resourceType = ResourceType::Sampler;
    } else {
        resourceType = ResourceType::Invalid;
        vkDescriptorBufferInfo = {};
    }
}

Descriptor::Descriptor(const AccelerationStructureView& accelerationStructureView) {
    VkAccelerationStructureHandleKHR vkHandle = accelerationStructureView.vkGetAccelerationStructureHandle();
    if (!vkHandle.isNull()) {
        vkDescriptorAccelerationStructureHandle = vkHandle;
        resourceType = ResourceType::AccelerationStructure;
    } else {
        resourceType = ResourceType::Invalid;
        vkDescriptorBufferInfo = {};
    }
}

bool Descriptor::isNull() const {
    return resourceType == ResourceType::None;
}

bool operator==(const Descriptor& lhs, const Descriptor& rhs) {
    if (lhs.resourceType != rhs.resourceType)
        return false;

    switch (lhs.resourceType) {
    case Descriptor::ResourceType::Sampler:
        return lhs.vkDescriptorImageInfo.sampler == rhs.vkDescriptorImageInfo.sampler;
    case Descriptor::ResourceType::Image:
        return lhs.vkDescriptorImageInfo.imageView == rhs.vkDescriptorImageInfo.imageView;
    case Descriptor::ResourceType::CombinedImageSampler:
        return lhs.vkDescriptorImageInfo.imageView == rhs.vkDescriptorImageInfo.imageView &&
            lhs.vkDescriptorImageInfo.sampler == rhs.vkDescriptorImageInfo.sampler;
    case Descriptor::ResourceType::Buffer:
        return lhs.vkDescriptorBufferInfo.buffer == rhs.vkDescriptorBufferInfo.buffer &&
            lhs.vkDescriptorBufferInfo.offset == rhs.vkDescriptorBufferInfo.offset &&
            lhs.vkDescriptorBufferInfo.range == rhs.vkDescriptorBufferInfo.range;
    case Descriptor::ResourceType::TexelBufferView:
        return lhs.vkDescriptorBufferViewHandle == rhs.vkDescriptorBufferViewHandle;
    case Descriptor::ResourceType::AccelerationStructure:
        return lhs.vkDescriptorAccelerationStructureHandle == rhs.vkDescriptorAccelerationStructureHandle;
    case Descriptor::ResourceType::None:
        return true;
    case Descriptor::ResourceType::Invalid:
    default:
        return false;
    }
}

// Initialize descriptorImageView as the largest member of the union
FutureDescriptor::FutureDescriptor() : resourceType(Descriptor::ResourceType::None), descriptorImageView() {}

FutureDescriptor::FutureDescriptor(BufferView bufferView) : descriptorBufferView(std::move(bufferView)) {
    if (descriptorBufferView.getFormat() != Format::Undefined)
        resourceType = Descriptor::ResourceType::TexelBufferView;
    else
        resourceType = Descriptor::ResourceType::Buffer;
}

FutureDescriptor::FutureDescriptor(ImageView imageView)
    : descriptorImageView(std::move(imageView)), resourceType(Descriptor::ResourceType::Image) {}

FutureDescriptor::FutureDescriptor(ImageView imageView, const Sampler* sampler)
    : descriptorImageView(std::move(imageView)),
      descriptorSampler(sampler),
      resourceType(Descriptor::ResourceType::CombinedImageSampler) {}

FutureDescriptor::FutureDescriptor(const Sampler* sampler)
    : descriptorSampler(sampler), descriptorImageView(), resourceType(Descriptor::ResourceType::Sampler) {}

FutureDescriptor::FutureDescriptor(AccelerationStructureView accelerationStructureView)
    : descriptorAccelerationStructureView(std::move(accelerationStructureView)),
      resourceType(Descriptor::ResourceType::AccelerationStructure) {}

Descriptor FutureDescriptor::resolve() const {
    static const Sampler nullSampler = {};
    switch (resourceType) {
    case Descriptor::ResourceType::Sampler:
        return Descriptor(descriptorSampler == nullptr ? nullSampler : *descriptorSampler);
    case Descriptor::ResourceType::Image:
        return Descriptor(descriptorImageView);
    case Descriptor::ResourceType::CombinedImageSampler:
        return Descriptor(descriptorImageView, descriptorSampler == nullptr ? nullSampler : *descriptorSampler);
    case Descriptor::ResourceType::Buffer:
        return Descriptor(descriptorBufferView);
    case Descriptor::ResourceType::TexelBufferView:
        return Descriptor(descriptorBufferView);
    case Descriptor::ResourceType::AccelerationStructure:
        return Descriptor(descriptorAccelerationStructureView);
    case Descriptor::ResourceType::Invalid:
    case Descriptor::ResourceType::None:
        return Descriptor();
    default:
        TEPHRA_ASSERT(false);
        return Descriptor();
    }
}

bool FutureDescriptor::isNull() const {
    return resourceType == Descriptor::ResourceType::None;
}

bool operator==(const FutureDescriptor& lhs, const FutureDescriptor& rhs) {
    if (lhs.resourceType != rhs.resourceType)
        return false;

    switch (lhs.resourceType) {
    case Descriptor::ResourceType::Sampler:
        return lhs.descriptorSampler == rhs.descriptorSampler;
    case Descriptor::ResourceType::Image:
        return lhs.descriptorImageView == rhs.descriptorImageView;
    case Descriptor::ResourceType::CombinedImageSampler:
        return lhs.descriptorImageView == rhs.descriptorImageView && lhs.descriptorSampler == rhs.descriptorImageView;
    case Descriptor::ResourceType::Buffer:
        return lhs.descriptorBufferView == rhs.descriptorBufferView;
    case Descriptor::ResourceType::TexelBufferView:
        return lhs.descriptorBufferView == rhs.descriptorBufferView;
    case Descriptor::ResourceType::AccelerationStructure:
        return lhs.descriptorAccelerationStructureView == rhs.descriptorAccelerationStructureView;
    case Descriptor::ResourceType::None:
        return true;
    case Descriptor::ResourceType::Invalid:
    default:
        return false;
    }
}

DescriptorSetLayout::DescriptorSetLayout(
    Lifeguard<VkDescriptorSetLayoutHandle> descriptorSetLayoutHandle,
    Lifeguard<VkDescriptorUpdateTemplateHandle> descriptorUpdateTemplateHandle,
    ArrayParameter<const DescriptorBinding> descriptorBindings_)
    : descriptorSetLayoutHandle(std::move(descriptorSetLayoutHandle)),
      descriptorUpdateTemplateHandle(std::move(descriptorUpdateTemplateHandle)) {
    descriptorBindings = std::vector<DescriptorBinding>(descriptorBindings_.begin(), descriptorBindings_.end());

    descriptorCount = 0;
    for (DescriptorBinding& binding : descriptorBindings) {
        descriptorCount += binding.arraySize;

        // Also remove immutable samplers, since the array might not be valid anymore
        binding.immutableSamplers = {};
    }

    fillVkPoolSizes();
}

void DescriptorSetLayout::fillVkPoolSizes() {
    for (const DescriptorBinding& binding : descriptorBindings) {
        if (binding.descriptorType == IgnoredDescriptorType)
            continue;

        VkDescriptorType vkType = vkCastConvertibleEnum(binding.descriptorType);
        auto it = std::find_if(vkPoolSizes.begin(), vkPoolSizes.end(), [&](VkDescriptorPoolSize& poolSize) {
            return poolSize.type == vkType;
        });

        if (it != vkPoolSizes.end()) {
            it->descriptorCount += binding.arraySize;
        } else {
            VkDescriptorPoolSize newPoolSize;
            newPoolSize.type = vkType;
            newPoolSize.descriptorCount = binding.arraySize;
            vkPoolSizes.push_back(newPoolSize);
        }
    }
}

DescriptorSetView::DescriptorSetView() : vkPersistentDescriptorSetHandle(), vkJobLocalDescriptorSetPtr(nullptr) {}

DescriptorSetView::DescriptorSetView(VkDescriptorSetHandle vkPersistentDescriptorSetHandle)
    : vkPersistentDescriptorSetHandle(vkPersistentDescriptorSetHandle), vkJobLocalDescriptorSetPtr(nullptr) {}

DescriptorSetView::DescriptorSetView(VkDescriptorSetHandle* vkJobLocalDescriptorSetPtr)
    : vkPersistentDescriptorSetHandle(), vkJobLocalDescriptorSetPtr(vkJobLocalDescriptorSetPtr) {}

bool DescriptorSetView::isNull() const {
    return viewsJobLocalSet() && vkJobLocalDescriptorSetPtr == nullptr;
}

VkDescriptorSetHandle DescriptorSetView::vkResolveDescriptorSetHandle() const {
    if (viewsJobLocalSet()) {
        return *vkJobLocalDescriptorSetPtr;
    } else {
        return vkPersistentDescriptorSetHandle;
    }
}

bool operator==(const DescriptorSetView& lhs, const DescriptorSetView& rhs) {
    if (lhs.viewsJobLocalSet() != rhs.viewsJobLocalSet()) {
        return false;
    }
    if (lhs.viewsJobLocalSet()) {
        return lhs.vkJobLocalDescriptorSetPtr == rhs.vkJobLocalDescriptorSetPtr;
    } else {
        return lhs.vkPersistentDescriptorSetHandle == rhs.vkPersistentDescriptorSetHandle;
    }
}

DescriptorSet::DescriptorSet() : DescriptorSet({}, nullptr) {}

DescriptorSet::DescriptorSet(VkDescriptorSetHandle vkDescriptorSetHandle, DescriptorPoolEntry* parentDescriptorPoolEntry)
    : vkDescriptorSetHandle(vkDescriptorSetHandle), parentDescriptorPoolEntry(parentDescriptorPoolEntry) {}

DescriptorSetView DescriptorSet::getView() const {
    return DescriptorSetView(vkDescriptorSetHandle);
}

DescriptorSet::DescriptorSet(DescriptorSet&& other) noexcept : DescriptorSet() {
    std::swap(vkDescriptorSetHandle, other.vkDescriptorSetHandle);
    std::swap(parentDescriptorPoolEntry, other.parentDescriptorPoolEntry);
}

DescriptorSet& DescriptorSet::operator=(DescriptorSet&& other) noexcept {
    std::swap(vkDescriptorSetHandle, other.vkDescriptorSetHandle);
    std::swap(parentDescriptorPoolEntry, other.parentDescriptorPoolEntry);
    return *this;
}

DescriptorSet::~DescriptorSet() noexcept {
    if (!isNull()) {
        TEPHRA_ASSERT_NOEXCEPT(parentDescriptorPoolEntry != nullptr);
        uint64_t timestampToWaitOn = parentDescriptorPoolEntry->timelineManager->getLastTrackedTimestamp();
        DescriptorPoolImpl::queueFreeDescriptorSet(vkDescriptorSetHandle, parentDescriptorPoolEntry, timestampToWaitOn);
    }
}

DescriptorSetSetup::DescriptorSetSetup(
    ArrayView<const Descriptor> descriptors,
    DescriptorSetFlagMask flags,
    const char* debugName)
    : descriptors(descriptors), flags(flags), debugName(debugName) {}

const VkDescriptorImageInfo* Descriptor::vkResolveDescriptorImageInfo() const {
    bool hasImageDescriptor = resourceType == ResourceType::Sampler || resourceType == ResourceType::Image ||
        resourceType == ResourceType::CombinedImageSampler;
    return hasImageDescriptor ? &vkDescriptorImageInfo : nullptr;
}

const VkDescriptorBufferInfo* Descriptor::vkResolveDescriptorBufferInfo() const {
    return resourceType == ResourceType::Buffer ? &vkDescriptorBufferInfo : nullptr;
}

const VkBufferView* Descriptor::vkResolveDescriptorBufferViewHandle() const {
    return resourceType == ResourceType::TexelBufferView ? &vkDescriptorBufferViewHandle.vkRawHandle : nullptr;
}

const VkAccelerationStructureKHR* Descriptor::vkResolveAccelerationStructureHandle() const {
    return resourceType == ResourceType::AccelerationStructure ? &vkDescriptorAccelerationStructureHandle.vkRawHandle :
                                                                 nullptr;
}

#ifdef TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION
    #include <string>
std::string resourceTypeToString(Descriptor::ResourceType resourceType) {
    switch (resourceType) {
    case Descriptor::ResourceType::Sampler:
        return "Sampler";
    case Descriptor::ResourceType::Image:
        return "Image";
    case Descriptor::ResourceType::CombinedImageSampler:
        return "CombinedImageSampler";
    case Descriptor::ResourceType::Buffer:
        return "Buffer";
    case Descriptor::ResourceType::TexelBufferView:
        return "TexelBufferView";
    case Descriptor::ResourceType::AccelerationStructure:
        return "AccelerationStructure";
    case Descriptor::ResourceType::None:
        return "None";
    default:
        return "Invalid";
    }
}

Descriptor::ResourceType getExpectedResourceType(DescriptorType descriptorType) {
    switch (descriptorType) {
    case DescriptorType::Sampler:
        return Descriptor::ResourceType::Sampler;
    case DescriptorType::CombinedImageSampler:
        return Descriptor::ResourceType::CombinedImageSampler;
    case DescriptorType::SampledImage:
        return Descriptor::ResourceType::Image;
    case DescriptorType::StorageImage:
        return Descriptor::ResourceType::Image;
    case DescriptorType::TexelBuffer:
        return Descriptor::ResourceType::TexelBufferView;
    case DescriptorType::StorageTexelBuffer:
        return Descriptor::ResourceType::TexelBufferView;
    case DescriptorType::UniformBuffer:
        return Descriptor::ResourceType::Buffer;
    case DescriptorType::StorageBuffer:
        return Descriptor::ResourceType::Buffer;
    case DescriptorType::UniformBufferDynamic:
        return Descriptor::ResourceType::Buffer;
    case DescriptorType::StorageBufferDynamic:
        return Descriptor::ResourceType::Buffer;
    case DescriptorType::AccelerationStructureKHR:
        return Descriptor::ResourceType::AccelerationStructure;
    default:
        TEPHRA_ASSERT(descriptorType == IgnoredDescriptorType);
        return Descriptor::ResourceType::None;
    }
}

void Descriptor::debugValidateAgainstBinding(
    const DescriptorBinding& binding,
    std::size_t descriptorIndex,
    bool ignoreNullDescriptors) const {
    if (resourceType == Descriptor::ResourceType::Invalid) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "Attempting to use a descriptor at index ",
            descriptorIndex,
            " with an invalid view or a job-local resource from a job that hasn't been enqueued yet.");
        return;
    }

    Descriptor::ResourceType expectedType = getExpectedResourceType(binding.descriptorType);
    if (resourceType != expectedType &&
        // Allow setting a combined image sampler with only the image
        !(resourceType == Descriptor::ResourceType::Image &&
          expectedType == Descriptor::ResourceType::CombinedImageSampler) &&
        // Allow null descriptors
        !(resourceType == Descriptor::ResourceType::None && ignoreNullDescriptors)) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "Descriptor at index ",
            descriptorIndex,
            " referencing a resource of type '",
            resourceTypeToString(resourceType),
            "' is being bound to a DescriptorBinding expecting a resource type '",
            resourceTypeToString(expectedType),
            "'.");
    }
}

void DescriptorSetLayout::debugValidateDescriptors(
    ArrayParameter<const Descriptor> descriptors,
    bool ignoreNullDescriptors) const {
    bool containsVariableDescriptorCount = !descriptorBindings.empty() &&
        descriptorBindings.back().flags.contains(DescriptorBindingFlag::VariableDescriptorCount);

    if (containsVariableDescriptorCount) {
        uint32_t maxDescriptorCount = descriptorCount;
        uint32_t minDescriptorCount = maxDescriptorCount - descriptorBindings.back().arraySize;
        if (descriptors.size() < minDescriptorCount || descriptors.size() > maxDescriptorCount) {
            reportDebugMessage(
                DebugMessageSeverity::Error,
                DebugMessageType::Validation,
                "Number of descriptors passed (",
                descriptors.size(),
                ") is not within the expected range for the DescriptorSetLayout with a variable descriptor count "
                "binding (",
                minDescriptorCount,
                ", ",
                maxDescriptorCount,
                ").");
        }
    } else if (descriptors.size() != descriptorCount) {
        reportDebugMessage(
            DebugMessageSeverity::Error,
            DebugMessageType::Validation,
            "Number of descriptors passed (",
            descriptors.size(),
            ") does not match number of descriptors in the DescriptorSetLayout (",
            descriptorCount,
            ").");
    }

    std::size_t descriptorIndex = 0;
    for (const DescriptorBinding& descriptorBinding : descriptorBindings) {
        for (std::size_t i = 0; i < descriptorBinding.arraySize; i++) {
            if (descriptorIndex >= descriptors.size()) {
                break;
            }
            descriptors[descriptorIndex++].debugValidateAgainstBinding(
                descriptorBinding, descriptorIndex, ignoreNullDescriptors);
        }
    }
}
#else
void Descriptor::debugValidateAgainstBinding(
    const DescriptorBinding& binding,
    std::size_t descriptorIndex,
    bool ignoreNullDescriptors) const {}
void DescriptorSetLayout::debugValidateDescriptors(
    ArrayParameter<const Descriptor> descriptors,
    bool ignoreNullDescriptors) const {}
#endif

}
