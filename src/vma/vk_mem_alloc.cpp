#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#define VMA_DYNAMIC_VULKAN_FUNCTIONS 0

#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>
#undef VK_NO_PROTOTYPES

// Vulkan SDK 1.2 backwards compatibility
#ifndef VK_API_VERSION_VARIANT
    #define VK_API_VERSION_VARIANT(version) ((uint32_t)(version) >> 29)
#endif
#ifndef VK_API_VERSION_MAJOR
    #define VK_API_VERSION_MAJOR(version) (((uint32_t)(version) >> 22) & 0x7FU)
#endif
#ifndef VK_API_VERSION_MINOR
    #define VK_API_VERSION_MINOR(version) (((uint32_t)(version) >> 12) & 0x3FFU)
#endif
#ifndef VK_API_VERSION_PATCH
    #define VK_API_VERSION_PATCH(version) (((uint32_t)(version)) & 0xFFFU)
#endif

#include <vma/vk_mem_alloc.h>
