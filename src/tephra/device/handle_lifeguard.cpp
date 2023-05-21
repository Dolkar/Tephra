#include <tephra/vulkan/handles.hpp>
#include "device_container.hpp"
#include <type_traits>

namespace tp {

// Handle lifeguard declarations
template class Lifeguard<VkObjectHandle<VkShaderModule, VK_OBJECT_TYPE_SHADER_MODULE>>;
template class Lifeguard<VkObjectHandle<VkDescriptorSetLayout, VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT>>;
template class Lifeguard<VkObjectHandle<VkDescriptorUpdateTemplate, VK_OBJECT_TYPE_DESCRIPTOR_UPDATE_TEMPLATE>>;
template class Lifeguard<VkObjectHandle<VkPipelineLayout, VK_OBJECT_TYPE_PIPELINE_LAYOUT>>;
template class Lifeguard<VkObjectHandle<VkPipelineCache, VK_OBJECT_TYPE_PIPELINE_CACHE>>;
template class Lifeguard<VkObjectHandle<VmaAllocation, VK_OBJECT_TYPE_UNKNOWN>>;
template class Lifeguard<VkObjectHandle<VkBuffer, VK_OBJECT_TYPE_BUFFER>>;
template class Lifeguard<VkObjectHandle<VkBufferView, VK_OBJECT_TYPE_BUFFER_VIEW>>;
template class Lifeguard<VkObjectHandle<VkImage, VK_OBJECT_TYPE_IMAGE>>;
template class Lifeguard<VkObjectHandle<VkImageView, VK_OBJECT_TYPE_IMAGE_VIEW>>;
template class Lifeguard<VkObjectHandle<VkSampler, VK_OBJECT_TYPE_SAMPLER>>;
template class Lifeguard<VkObjectHandle<VkDescriptorPool, VK_OBJECT_TYPE_DESCRIPTOR_POOL>>;
template class Lifeguard<VkObjectHandle<VkPipeline, VK_OBJECT_TYPE_PIPELINE>>;
template class Lifeguard<VkObjectHandle<VkRenderPass, VK_OBJECT_TYPE_RENDER_PASS>>;
template class Lifeguard<VkObjectHandle<VkFramebuffer, VK_OBJECT_TYPE_FRAMEBUFFER>>;
template class Lifeguard<VkObjectHandle<VkSwapchainKHR, VK_OBJECT_TYPE_SWAPCHAIN_KHR>>;
template class Lifeguard<VkObjectHandle<VkSemaphore, VK_OBJECT_TYPE_SEMAPHORE>>;

// Helper class with value set to true if the first type is the same as at least one of the other types
template <typename T, typename... Ts>
constexpr bool isOneOf() {
    return (std::is_same_v<T, Ts> || ...);
};

template <typename T>
void Lifeguard<T>::destroyHandle(bool immediately) noexcept {
    if (!isNonOwning()) {
        TEPHRA_ASSERT(!isNull());
        // Check if the handle is a type that is safe to destroy right away
        if constexpr (isOneOf<
                          T,
                          VkShaderModuleHandle,
                          VkDescriptorSetLayoutHandle,
                          VkDescriptorUpdateTemplateHandle,
                          VkPipelineLayoutHandle,
                          VkPipelineCacheHandle>()) {
            deviceImpl->getDeferredDestructor()->destroyImmediately(vkHandle);
        } else {
            if (immediately) {
                deviceImpl->getDeferredDestructor()->destroyImmediately(vkHandle);
            } else {
                // Queue the handle to be deleted when all jobs potentially using the object have finished executing
                uint64_t timestamp = deviceImpl->getTimelineManager()->getLastTrackedTimestamp();
                deviceImpl->getDeferredDestructor()->queueForDestruction(vkHandle, timestamp);
            }
        }

        releaseHandle();
    }
}

template <typename T>
Lifeguard<T>::Lifeguard(Lifeguard&& other) noexcept : deviceImpl(other.deviceImpl), vkHandle(other.vkHandle) {
    other.releaseHandle();
}

template <typename T>
Lifeguard<T>& Lifeguard<T>::operator=(Lifeguard&& other) noexcept {
    destroyHandle();
    deviceImpl = other.deviceImpl;
    vkHandle = other.vkHandle;
    other.releaseHandle();
    return *this;
}

template <typename T>
Lifeguard<T>::~Lifeguard() {
    destroyHandle();
}

template <typename T>
Lifeguard<T> Lifeguard<T>::NonOwning(TypedHandle vkHandle) {
    return Lifeguard<T>(nullptr, vkHandle);
}

template <typename T>
Lifeguard<T>::Lifeguard(DeviceContainer* deviceImpl, TypedHandle vkHandle)
    : deviceImpl(deviceImpl), vkHandle(vkHandle) {}

}
