#pragma once

#include <tephra/buffer.hpp>
#include <tephra/image.hpp>
#include <tephra/sampler.hpp>
#include <tephra/common.hpp>

namespace tp {

/// Describes a kind of read-only resource access by any part of the pipeline.
/// @see tp::DescriptorType for classification of descriptors into Storage, Sampled and Uniform.
/// @see tp::DescriptorBinding::getReadAccessMask
enum class ReadAccess : uint64_t {
    /// Read access of indirect command data through an indirect drawing command.
    DrawIndirect = 1 << 0,
    /// Read access of an index buffer through an indexed drawing command.
    DrawIndex = 1 << 1,
    /// Read access of a vertex buffer through a drawing command.
    DrawVertex = 1 << 2,
    /// Read transfer access of a resource through a transfer command.
    Transfer = 1 << 3,
    /// Read host access of a buffer through HostMappedMemory.
    Host = 1 << 4,
    /// Read access of an image as a depth stencil attachment of a render pass.
    DepthStencilAttachment = 1 << 5,

    /// Vertex shader read access through storage descriptors.
    VertexShaderStorage = 1 << 6,
    /// Vertex shader read access through sampled descriptors.
    VertexShaderSampled = 1 << 7,
    /// Vertex shader read access through uniform buffer descriptors.
    VertexShaderUniform = 1 << 8,

    /// Tessellation control shader read access through storage descriptors.
    TessellationControlShaderStorage = 1 << 9,
    /// Tessellation control shader read access through sampled descriptors.
    TessellationControlShaderSampled = 1 << 10,
    /// Tessellation control shader read access through uniform buffer descriptors.
    TessellationControlShaderUniform = 1 << 11,

    /// Tessellation evaluation shader read access through storage descriptors.
    TessellationEvaluationShaderStorage = 1 << 12,
    /// Tessellation evaluation shader read access through sampled descriptors.
    TessellationEvaluationShaderSampled = 1 << 13,
    /// Tessellation evaluation shader read access through uniform buffer descriptors.
    TessellationEvaluationShaderUniform = 1 << 14,

    /// Geometry shader read access through storage descriptors.
    GeometryShaderStorage = 1 << 15,
    /// Geometry shader read access through sampled descriptors.
    GeometryShaderSampled = 1 << 16,
    /// Geometry shader read access through uniform buffer descriptors.
    GeometryShaderUniform = 1 << 17,

    /// Fragment shader read access through storage descriptors.
    FragmentShaderStorage = 1 << 18,
    /// Fragment shader read access through sampled descriptors.
    FragmentShaderSampled = 1 << 19,
    /// Fragment shader read access through uniform buffer descriptors.
    FragmentShaderUniform = 1 << 20,

    /// Compute shader read access through storage descriptors.
    ComputeShaderStorage = 1 << 21,
    /// Compute shader read access through sampled descriptors.
    ComputeShaderSampled = 1 << 22,
    /// Compute shader read access through uniform buffer descriptors.
    ComputeShaderUniform = 1 << 23,

    /// Image present operation access through tp::Device::submitPresentImagesKHR.
    ImagePresentKHR = 1ull << 62,

    /// Represents an unknown or generic read access.
    /// @remarks
    ///     If you don't wish to specify any access, use tp::ReadAccessMask::None() instead.
    Unknown = 1ull << 63
};
TEPHRA_MAKE_ENUM_BIT_MASK(ReadAccessMask, ReadAccess);

struct DescriptorBinding;

/// Binds an existing resource or sampler inside a tp::DescriptorSet, according to the tp::DescriptorBinding
/// defined inside a tp::DescriptorSetLayout.
/// @remarks
///     This can only be used for existing resources. For use with job-local resources of a job that hasn't
///     been enqueued yet, see tp::FutureDescriptor.
/// @see tp::DescriptorSetSetup
/// @see tp::DescriptorBinding
class Descriptor {
public:
    enum class ResourceType;

    /// Creates a null descriptor.
    Descriptor();
    /// Creates a buffer descriptor.
    /// @param bufferView
    ///     The tp::BufferView to bind.
    Descriptor(const BufferView& bufferView);
    /// Creates an image descriptor.
    /// @param imageView
    ///     The tp::ImageView to bind.
    Descriptor(const ImageView& imageView);
    /// Creates a combined image sampler binding to tp::DescriptorType::CombinedImageSampler.
    /// @param imageView
    ///     The tp::ImageView to bind.
    /// @param sampler
    ///     The tp::Sampler to bind.
    Descriptor(const ImageView& imageView, const Sampler& sampler);
    /// Creates a sampler descriptor binding to tp::DescriptorType::Sampler.
    Descriptor(const Sampler& sampler);

    /// Returns `true` if the descriptor is null and does not refer to any resource.
    bool isNull() const;

    const VkDescriptorImageInfo* vkResolveDescriptorImageInfo() const;
    const VkDescriptorBufferInfo* vkResolveDescriptorBufferInfo() const;
    const VkBufferView* vkResolveDescriptorBufferViewHandle() const;
    void debugValidateAgainstBinding(
        const DescriptorBinding& binding,
        std::size_t descriptorIndex,
        bool ignoreNullDescriptors) const;

private:
    friend class DescriptorPoolImpl;
    friend bool operator==(const Descriptor&, const Descriptor&);

    union {
        mutable VkDescriptorImageInfo vkDescriptorImageInfo;
        VkDescriptorBufferInfo vkDescriptorBufferInfo;
        VkBufferViewHandle vkDescriptorBufferViewHandle;
    };

    ResourceType resourceType;
};

bool operator==(const Descriptor& lhs, const Descriptor& rhs);
inline bool operator!=(const Descriptor& lhs, const Descriptor& rhs) {
    return !(lhs == rhs);
}

/// Binds a resource or sampler inside a tp::DescriptorSet, according to the tp::DescriptorBinding defined
/// inside a tp::DescriptorSetLayout. This variant can be used with job-local resources of a job that hasn't
/// been enqueued yet.
/// @see tp::Job::allocateLocalDescriptorSet
/// @see tp::DescriptorBinding
class FutureDescriptor {
public:
    /// Creates a null descriptor.
    FutureDescriptor();
    /// Creates a buffer descriptor.
    /// @param bufferView
    ///     The tp::BufferView to bind.
    FutureDescriptor(BufferView bufferView);
    /// Creates an image descriptor.
    /// @param imageView
    ///     The tp::ImageView to bind.
    FutureDescriptor(ImageView imageView);
    /// Creates a combined image sampler binding to tp::DescriptorType::CombinedImageSampler.
    /// @param imageView
    ///     The tp::ImageView to bind.
    /// @param sampler
    ///     The tp::Sampler to bind.
    FutureDescriptor(ImageView imageView, const Sampler* sampler);
    /// Creates a sampler descriptor binding to tp::DescriptorType::Sampler.
    FutureDescriptor(const Sampler* sampler);

    /// Resolves the descriptor. The resource referenced by this descriptor must be ready. For job-local resources
    /// it means this can only be called after the tp::Job has been enqueued.
    Descriptor resolve() const;

    /// Returns `true` if the descriptor is null and does not refer to any resource.
    bool isNull() const;

private:
    friend bool operator==(const FutureDescriptor&, const FutureDescriptor&);

    // Store the views instead to resolve later
    union {
        BufferView descriptorBufferView;
        ImageView descriptorImageView;
    };

    const Sampler* descriptorSampler = nullptr;

    Descriptor::ResourceType resourceType;
};

bool operator==(const FutureDescriptor& lhs, const FutureDescriptor& rhs);
inline bool operator!=(const FutureDescriptor& lhs, const FutureDescriptor& rhs) {
    return !(lhs == rhs);
}

/// Specifies additional properties of a tp::DescriptorBinding
/// @see @vksymbol{VkDescriptorBindingFlagBits}
enum class DescriptorBindingFlag {
    /// Indicates that the descriptors in this binding that are not dynamically used don't need to contain
    /// valid descriptors at the time the descriptors are consumed. A descriptor is dynamically used if
    /// any shader invocation executes an instruction that performs any memory access using the descriptor.
    /// @remarks
    ///     The use of this flag requires the
    ///     @vksymbol{VkPhysicalDeviceVulkan12Features}::`descriptorBindingPartiallyBound` feature to be enabled.
    PartiallyBound = 1 << 0,
    /// Indicates that this descriptor binding has a variable size that will be specified when a descriptor
    /// set is allocated using this layout. The `arraySize` of tp::DescriptorBinding is then treated as an upper
    /// bound on the size of this binding.
    /// @remarks
    ///     This flag must only be used for the last binding in the array passed to
    ///     tp::Device::createDescriptorSetLayout, and it must also be the binding with the largest value of
    ///     `bindingNumber`.
    /// @remarks
    ///     The use of this flag requires the
    ///     @vksymbol{VkPhysicalDeviceVulkan12Features}::`descriptorBindingVariableDescriptorCount` feature to be
    ///     enabled.
    VariableDescriptorCount = 1 << 1,
    /// Allows binding the same image to a sampled image descriptor that uses this flag at the same time as to a storage
    /// image descriptor. Internally, the image will be transitioned to a general layout, just like for a storage image.
    /// @remarks
    ///     The read access performed through this descriptor will be the same as if its type was
    ///     tp::DescriptorType::StorageImage.
    /// @remarks
    ///     This flag may potentially reduce sampling performance of the image, especially if the image has previously
    ///     been bound as a render target.
    /// @remarks
    ///     This flag is only valid with descriptor types tp::DescriptorType::SampledImage,
    ///     tp::DescriptorType::CombinedImageSampler and tp::DescriptorType::InputAttachment.
    AliasStorageImage = 1 << 2,
    /// Allows this binding to be updated inside a descriptor set after it has already been bound and used. This only
    /// affects tp::utils::MutableDescriptorSet::setImmediate or other custom methods relying on
    /// `VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT`.
    /// @remarks
    ///     The use of this flag requires the various `descriptorBindingUpdateAfterBind` features in
    ///     @vksymbol{VkPhysicalDeviceVulkan12Features} depending on the type of the descriptor binding.
    UpdateAfterBind = 1 << 3,
};
TEPHRA_MAKE_ENUM_BIT_MASK(DescriptorBindingFlagMask, DescriptorBindingFlag)

/// Describes the type of tp::Descriptor or descriptor array to be bound to a particular binding number.
/// @see @vksymbol{VkDescriptorSetLayoutBinding}
struct DescriptorBinding {
    uint32_t bindingNumber;
    DescriptorType descriptorType;
    uint32_t arraySize;
    ShaderStageMask stageMask;
    ArrayView<const Sampler* const> immutableSamplers;
    DescriptorBindingFlagMask flags;

    /// Default constructor creating an empty binding with an `arraySize` of 0 that will be ignored.
    DescriptorBinding();

    /// @param bindingNumber
    ///     The binding number within the descriptor set used to access this descriptor from within shaders.
    /// @param descriptorType
    ///     The type of the descriptor.
    /// @param stageMask
    ///     The mask of shader stages to which the bound resource will be accessible.
    /// @param arraySize
    ///     The optional size of the array for descriptor arrays.
    /// @param flags
    ///     Additional flags for the descriptor binding.
    DescriptorBinding(
        uint32_t bindingNumber,
        DescriptorType descriptorType,
        ShaderStageMask stageMask,
        uint32_t arraySize = 1,
        DescriptorBindingFlagMask flags = DescriptorBindingFlagMask::None());

    /// Constructs a descriptor binding with immutable samplers.
    /// @param bindingNumber
    ///     The binding number within the descriptor set used to access this descriptor from within shaders.
    /// @param descriptorType
    ///     The type of the descriptor.
    /// @param stageMask
    ///     The mask of shader stages to which the bound resource will be accessible.
    /// @param immutableSamplers
    ///     An array of samplers to be statically bound to the binding's descriptors. The size of this
    ///     array also specifies the binding's `arraySize`.
    /// @param flags
    ///     Additional flags for the descriptor binding.
    DescriptorBinding(
        uint32_t bindingNumber,
        DescriptorType descriptorType,
        ShaderStageMask stageMask,
        ArrayView<const Sampler* const> immutableSamplers,
        DescriptorBindingFlagMask flags = DescriptorBindingFlagMask::None());

    /// Returns a read access mask that covers all the possible ways a resource can be accessed through this binding.
    /// @remarks
    ///     The returned access mask will always conform to the single image layout export rule
    ///     (see tp::Job::cmdExportResource for details).
    ReadAccessMask getReadAccessMask() const;

    /// Returns an empty descriptor binding of the given array size. The corresponding descriptors in
    /// tp::DescriptorSetSetup will be ignored.
    static DescriptorBinding Empty(uint32_t arraySize = 1);
};

/// Describes the layout of descriptor bindings that pipelines can use to access resources. Serves as
/// a template for creating tp::DescriptorSet objects out of resources to be bound.
/// @see tp::Device::createDescriptorSetLayout
/// @see @vksymbol{VkDescriptorSetLayout}
class DescriptorSetLayout {
public:
    /// Creates a null descriptor set layout.
    DescriptorSetLayout() {}

    DescriptorSetLayout(
        Lifeguard<VkDescriptorSetLayoutHandle>&& descriptorSetLayoutHandle,
        Lifeguard<VkDescriptorUpdateTemplateHandle>&& descriptorUpdateTemplateHandle,
        ArrayParameter<const DescriptorBinding> descriptorBindings);

    /// Returns `true` if the descriptor set layout is null and not valid for use.
    bool isNull() const {
        return descriptorSetLayoutHandle.isNull();
    }

    /// Returns the descriptor bindings that were used to create this layout.
    ArrayView<const DescriptorBinding> getBindings() const {
        return view(descriptorBindings);
    }

    /// Returns the number of descriptors in this layout, equal to the sum of `arraySize` of all the
    /// tp::DescriptorBinding objects used to define the layout.
    uint32_t getDescriptorCount() const {
        return descriptorCount;
    }

    /// Returns the associated @vksymbol{VkDescriptorSetLayout} handle.
    VkDescriptorSetLayoutHandle vkGetDescriptorSetLayoutHandle() const {
        return descriptorSetLayoutHandle.vkGetHandle();
    }

    /// Returns the associated @vksymbol{VkDescriptorUpdateTemplate} handle.
    VkDescriptorUpdateTemplateHandle vkGetDescriptorUpdateTemplateHandle() const {
        return descriptorUpdateTemplateHandle.vkGetHandle();
    }

    void debugValidateDescriptors(ArrayParameter<const Descriptor> descriptors, bool ignoreNullDescriptors) const;

private:
    friend class DescriptorPoolImpl;

    Lifeguard<VkDescriptorSetLayoutHandle> descriptorSetLayoutHandle;
    Lifeguard<VkDescriptorUpdateTemplateHandle> descriptorUpdateTemplateHandle;

    std::vector<DescriptorBinding> descriptorBindings;
    std::vector<VkDescriptorPoolSize> vkPoolSizes;
    uint32_t descriptorCount = 0;

    void fillVkPoolSizes();
};

/// Represents the non-owning view of a tp::DescriptorSet.
/// @see tp::DescriptorSet::getView
/// @see tp::Job::allocateLocalDescriptorSet
class DescriptorSetView {
public:
    /// Creates a null descriptor set view.
    DescriptorSetView();

    explicit DescriptorSetView(VkDescriptorSetHandle vkPersistentDescriptorSetHandle);
    explicit DescriptorSetView(VkDescriptorSetHandle* vkJobLocalDescriptorSetPtr);

    /// Returns `true` if the viewed descriptor set is null and not valid for use.
    bool isNull() const;

    /// Returns `true` if the instance views a job-local descriptor set.
    /// Returns `false` if it views a persistent one.
    bool viewsJobLocalSet() const {
        return vkPersistentDescriptorSetHandle.isNull();
    }

    /// Resolves and returns the underlying @vksymbol{VkDescriptorSet} handle of this view or `VK_NULL_HANDLE` if it
    /// doesn't exist.
    /// @remarks
    ///     If the viewed descriptor set is a job-local descriptor set, the underlying @vksymbol{VkDescriptorSet} handle
    ///     will exist only after the tp::Job has been enqueued.
    VkDescriptorSetHandle vkResolveDescriptorSetHandle() const;

private:
    friend bool operator==(const DescriptorSetView&, const DescriptorSetView&);

    VkDescriptorSetHandle vkPersistentDescriptorSetHandle;
    VkDescriptorSetHandle* vkJobLocalDescriptorSetPtr;
};

bool operator==(const DescriptorSetView& lhs, const DescriptorSetView& rhs);
inline bool operator!=(const DescriptorSetView& lhs, const DescriptorSetView& rhs) {
    return !(lhs == rhs);
}

struct DescriptorPoolEntry;

/// Describes the set of resources that can be bound at once to allow access to them from pipelines.
/// @see tp::DescriptorPool::allocateDescriptorSets
/// @see @vksymbol{VkDescriptorSet}
class DescriptorSet {
public:
    DescriptorSet();

    DescriptorSet(VkDescriptorSetHandle vkDescriptorSetHandle, DescriptorPoolEntry* parentDescriptorPoolEntry);

    /// Returns `true` if the descriptor set is null and not valid for use.
    bool isNull() const {
        return vkDescriptorSetHandle.isNull();
    }

    /// Returns a view of this descriptor set.
    DescriptorSetView getView() const;

    /// Returns the associated @vksymbol{VkDescriptorSet} handle.
    VkDescriptorSetHandle vkGetDescriptorSetHandle() const {
        return vkDescriptorSetHandle;
    }

    TEPHRA_MAKE_NONCOPYABLE(DescriptorSet);
    TEPHRA_MAKE_MOVABLE(DescriptorSet);

    ~DescriptorSet() noexcept;

private:
    VkDescriptorSetHandle vkDescriptorSetHandle;
    DescriptorPoolEntry* parentDescriptorPoolEntry;
};

inline bool operator==(const DescriptorSet& lhs, const DescriptorSet& rhs) {
    return lhs.vkGetDescriptorSetHandle() == rhs.vkGetDescriptorSetHandle();
}
inline bool operator!=(const DescriptorSet& lhs, const DescriptorSet& rhs) {
    return !(lhs == rhs);
}

/// Additional descriptor set creation options.
enum class DescriptorSetFlag {
    /// Specifying this flag allows you to provide null descriptors during descriptor set creation. Such descriptors
    /// will be ignored and their associated bindings will be left unbound. Such bindings must not be accessed from
    /// the shader side either statically or dynamically, depending on if tp::DescriptorBindingFlag::PartiallyBound was
    /// used.
    IgnoreNullDescriptors = 1 << 0,
};
TEPHRA_MAKE_ENUM_BIT_MASK(DescriptorSetFlagMask, DescriptorSetFlag);

/// Used as configuration for creating a new tp::DescriptorSet object.
/// @see tp::DescriptorPool::allocateDescriptorSets
struct DescriptorSetSetup {
    ArrayView<const Descriptor> descriptors;
    DescriptorSetFlagMask flags;
    const char* debugName;

    /// @param descriptors
    ///     The array of descriptors following the layout that will be passed to
    ///     tp::DescriptorPool::allocateDescriptorSets along with this setup structure. The descriptors must be in the
    ///     same order as the order of tp::DescriptorBinding values provided to tp::Device::createDescriptorSetLayout,
    ///     regardless of their `bindingNumber`. For each tp::DescriptorBinding, an `arraySize` number of valid
    ///     descriptors must be present, therefore the total number of descriptors in the array must match
    ///     tp::DescriptorSetLayout::getDescriptorCount.
    /// @param flags
    ///     Additional flags for the descriptor set.
    /// @param debugName
    ///     The debug name identifier for the object.
    /// @remarks
    ///     When tp::DescriptorSetFlag::IgnoreNullDescriptors is used, some of the descriptors provided may be null.
    /// @remarks
    ///     When tp::DescriptorBindingFlag::VariableDescriptorCount is used, the requirements on the number of
    ///     descriptors are relaxed for that binding - The `arraySize` of tp::DescriptorBinding then only becomes the
    ///     upper bound on the number of descriptors.
    DescriptorSetSetup(
        ArrayView<const Descriptor> descriptors,
        DescriptorSetFlagMask flags = DescriptorSetFlagMask::None(),
        const char* debugName = nullptr);
};

/// Specifies the overallocation behavior of a pool. This can be useful for reducing the frequency
/// of allocations at the cost of potentially higher memory usage.
struct OverallocationBehavior {
    float requestFactor;
    float growFactor;
    uint64_t minAllocationSize;

    /// Creates the specified overallocation behavior.
    ///
    /// Given a new allocation request, the size of the actual allocation to be made is calculated with
    /// the following formula:
    /// `max(floor(requestedSize * requestFactor), floor(poolSize * growFactor), minAllocationSize)`
    /// where `requestedSize` is the size of the requested allocation and `poolSize` is the sum of all
    /// allocations made by the pool.
    ///
    /// @param requestFactor
    ///     The factor applied to requested allocation sizes. Must be greater or equal to 1.
    /// @param growFactor
    ///     The factor applied to the total size of all allocations. Must be greater or equal to 0.
    /// @param minAllocationSize
    ///     The size of the smallest allocation allowed to be made. The units are dependent on the
    ///     specific pool.
    /// @remarks
    ///     The resulting size is just a hint and doesn't have to be fulfilled exactly.
    OverallocationBehavior(float requestFactor, float growFactor, uint64_t minAllocationSize);

    /// Applies the overallocation behavior to the requested size, returning the desired allocation size.
    /// @param requestedSize
    ///     The requested size of the allocation.
    /// @param poolSize
    ///     The size of all allocations made by the pool so far.
    uint64_t apply(uint64_t requestedSize, uint64_t poolSize) const;

    /// Creates a behavior of no overallocation that allocates exactly the requested amount.
    static OverallocationBehavior Exact();
};

/// Used as configuration for creating a new tp::DescriptorPool object.
/// @see tp::Device::createDescriptorPool
struct DescriptorPoolSetup {
    OverallocationBehavior overallocationBehavior;

    /// @param overallocationBehavior
    ///     Specifies the overallocation behavior of the descriptor pool. The units used represent
    ///     the number of descriptors.
    explicit DescriptorPoolSetup(OverallocationBehavior overallocationBehavior = { 1.0f, 0.5f, 256 });
};

/// Enables efficient creation, storage and reuse of tp::DescriptorSet objects.
/// @remarks
///     The allocated descriptor sets are reused for future allocations that use the same descriptor set layout.
/// @see tp::Device::createDescriptorPool
class DescriptorPool : public Ownable {
public:
    /// Allocates multiple descriptor sets with the same layout from the pool.
    /// @param descriptorSetLayout
    ///     The layout to be used for the descriptor sets.
    /// @param descriptorSetSetups
    ///     The setup structures used to create each descriptor set.
    /// @param allocatedDescriptorSets
    ///     An output array of pointers to the descriptor sets to be allocated. Must be of the same size
    ///     as `descriptorSetSetups`.
    /// @remarks
    ///     If the current capacity isn't sufficient, a new Vulkan @vksymbol{VkDescriptorPool} object will be
    ///     allocated, the size of which will be based on the number of remaining unallocated descriptor sets.
    void allocateDescriptorSets(
        const DescriptorSetLayout* descriptorSetLayout,
        ArrayParameter<const DescriptorSetSetup> descriptorSetSetups,
        ArrayParameter<DescriptorSet* const> allocatedDescriptorSets);

    /// Preallocates space for the given number of descriptor sets using the provided layout. If usage characteristics
    /// are known ahead of time, it may be more efficient to preallocate the required space at once.
    /// @param descriptorSetLayout
    ///     The descriptor set layout for which the memory will be reserved.
    /// @param descriptorSetCount
    ///     The number of descriptor sets using the provided layout for which the memory will be reserved.
    /// @remarks
    ///     The overallocation behavior specified during pool creation will not be applied to this allocation.
    /// @remarks
    ///     The actual allocation will be made upon the next call to allocateDescriptorSets.
    void reserve(const DescriptorSetLayout* descriptorSetLayout, uint32_t descriptorSetCount);

    TEPHRA_MAKE_INTERFACE(DescriptorPool);

protected:
    DescriptorPool() {}
};

}
