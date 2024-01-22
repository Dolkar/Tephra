#pragma once

#include <tephra/vulkan/header.hpp>
#include <tephra/macros.hpp>
#include <functional>

namespace tp {

class DeviceContainer;

/// A strongly typed Vulkan object handle wrapper.
template <typename T, VkObjectType Id>
struct VkObjectHandle {
    using VkHandleType = T;
    static constexpr VkObjectType VkHandleId = Id;

    VkHandleType vkRawHandle = VK_NULL_HANDLE;

    VkObjectHandle() = default;
    explicit VkObjectHandle(VkHandleType vkRawHandle) : vkRawHandle(vkRawHandle) {}

    operator VkHandleType() const {
        return vkRawHandle;
    }

    /// Returns `true` if the handle is a null handle.
    bool isNull() const {
        return vkRawHandle == VK_NULL_HANDLE;
    }
};

/// Equality operator for tp::VkObjectHandle.
template <typename T, VkObjectType Id>
inline bool operator==(const VkObjectHandle<T, Id>& lhs, const VkObjectHandle<T, Id>& rhs) {
    return lhs.vkRawHandle == rhs.vkRawHandle;
}

/// Inequality operator for tp::VkObjectHandle.
template <typename T, VkObjectType Id>
inline bool operator!=(const VkObjectHandle<T, Id>& lhs, const VkObjectHandle<T, Id>& rhs) {
    return !(lhs == rhs);
}

template <typename TypedHandle>
constexpr typename TypedHandle::VkHandleType* vkCastTypedHandlePtr(TypedHandle* ptr) noexcept {
    static_assert(sizeof(TypedHandle) == sizeof(typename TypedHandle::VkHandleType));
    return reinterpret_cast<typename TypedHandle::VkHandleType*>(ptr);
}

template <typename TypedHandle>
constexpr const typename TypedHandle::VkHandleType* vkCastTypedHandlePtr(const TypedHandle* ptr) noexcept {
    static_assert(sizeof(TypedHandle) == sizeof(typename TypedHandle::VkHandleType));
    return reinterpret_cast<const typename TypedHandle::VkHandleType*>(ptr);
}

using VkBufferHandle = VkObjectHandle<VkBuffer, VK_OBJECT_TYPE_BUFFER>;
using VkBufferViewHandle = VkObjectHandle<VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW>;
using VkCommandBufferHandle = VkObjectHandle<VkCommandBuffer, VK_OBJECT_TYPE_COMMAND_BUFFER>;
using VkCommandPoolHandle = VkObjectHandle<VkCommandPool, VK_OBJECT_TYPE_COMMAND_POOL>;
using VkDescriptorPoolHandle = VkObjectHandle<VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL>;
using VkDescriptorSetHandle = VkObjectHandle<VkDescriptorSet, VK_OBJECT_TYPE_DESCRIPTOR_SET>;
using VkDescriptorSetLayoutHandle = VkObjectHandle<VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>;
using VkDescriptorUpdateTemplateHandle =
    VkObjectHandle<VkDescriptorUpdateTemplate, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>;
using VkDeviceHandle = VkObjectHandle<VkDevice, VK_OBJECT_TYPE_DEVICE>;
using VkImageHandle = VkObjectHandle<VkImage, VK_OBJECT_TYPE_IMAGE>;
using VkImageViewHandle = VkObjectHandle<VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW>;
using VkAccelerationStructureHandleKHR =
    VkObjectHandle<VkAccelerationStructureKHR, VK_OBJECT_TYPE_ACCELERATION_STRUCTURE_KHR>;
using VkInstanceHandle = VkObjectHandle<VkInstance, VK_OBJECT_TYPE_INSTANCE>;
using VkPhysicalDeviceHandle = VkObjectHandle<VkPhysicalDevice, VK_OBJECT_TYPE_PHYSICAL_DEVICE>;
using VkPipelineCacheHandle = VkObjectHandle<VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE>;
using VkPipelineHandle = VkObjectHandle<VkPipeline, VK_OBJECT_TYPE_PIPELINE>;
using VkPipelineLayoutHandle = VkObjectHandle<VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT>;
using VkQueueHandle = VkObjectHandle<VkQueue, VK_OBJECT_TYPE_QUEUE>;
using VkSamplerHandle = VkObjectHandle<VkSampler, VK_OBJECT_TYPE_SAMPLER>;
using VkSemaphoreHandle = VkObjectHandle<VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE>;
using VkShaderModuleHandle = VkObjectHandle<VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE>;
using VkSurfaceHandleKHR = VkObjectHandle<VkSurfaceKHR, VK_OBJECT_TYPE_SURFACE_KHR>;
using VkSwapchainHandleKHR = VkObjectHandle<VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR>;
using VmaAllocationHandle = VkObjectHandle<VmaAllocation, VK_OBJECT_TYPE_UNKNOWN>;
using VmaAllocatorHandle = VkObjectHandle<VmaAllocator, VK_OBJECT_TYPE_UNKNOWN>;

/// A lifeguard for a Vulkan handle implementing RAII by invoking specialized deleters according to the type of the
/// handle when the lifeguard is destroyed. Can be created from an existing Vulkan handle with
/// tp::Device::vkMakeHandleLifeguard.
/// @remarks
///     The handle can be constructed as non-owning, in which case it won't delete the underlying object.
///     This can be useful for passing Vulkan objects to functions that otherwise expect an owning handle.
///     A non-owning handle lifeguard can be created with tp::Lifeguard::NonOwning.
template <typename T>
class Lifeguard final {
public:
    using TypedHandle = T;
    using VkHandleType = typename TypedHandle::VkHandleType;
    static constexpr VkObjectType VkHandleId = TypedHandle::VkHandleId;

    Lifeguard() {}

    Lifeguard(DeviceContainer* deviceImpl, TypedHandle vkHandle);

    /// Returns the underlying Vulkan object handle.
    TypedHandle vkGetHandle() const {
        return vkHandle;
    }

    /// Returns `true` if the object handle is `VK_NULL_HANDLE`.
    bool isNull() const noexcept {
        return vkHandle.isNull();
    }

    /// `Returns `true` if the handle lifeguard does not own its handle.
    bool isNonOwning() const noexcept {
        return deviceImpl == nullptr;
    }

    /// Destroys the underlying handle.
    void destroyHandle(bool immediately = false) noexcept;

    TEPHRA_MAKE_NONCOPYABLE(Lifeguard);

    Lifeguard(Lifeguard&& other) noexcept;

    Lifeguard& operator=(Lifeguard&& other) noexcept;

    ~Lifeguard();

    /// Creates a non-owning tp::Lifeguard instance out of a Vulkan object handle. A non-owning handle lifeguard
    /// will not delete the Vulkan object when it is destroyed. It can be useful for passing Vulkan objects to functions
    /// that expect an owning handle.
    static Lifeguard NonOwning(TypedHandle vkHandle);

private:
    friend class HandleLifetimeManager;

    DeviceContainer* deviceImpl = nullptr;
    TypedHandle vkHandle;

    void releaseHandle() noexcept {
        deviceImpl = nullptr;
        vkHandle = TypedHandle();
    }
};

}

namespace std {
/// Hashing object for tp::VkObjectHandle.
template <typename T, VkObjectType Id>
struct hash<tp::VkObjectHandle<T, Id>> {
    std::size_t operator()(const tp::VkObjectHandle<T, Id>& handle) const noexcept {
        return std::hash<T>{}(handle.vkRawHandle);
    }
};
}
