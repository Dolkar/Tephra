#pragma once

#include <tephra/vulkan/structures.hpp>
#include <tephra/macros.hpp>
#include <type_traits>
#include <unordered_map>
#include <memory>
#include <cstdint>
#include <cstddef>

namespace tp {

/// Trait wrapper for tp::getVkFeatureStructureType
struct VkFeatureStructureMapTrait {
    template <typename T>
    static constexpr VkStructureType getVkStructureType() {
        return getVkFeatureStructureType<T>();
    }
};

/// Trait wrapper for tp::getVkPropertyStructureType
struct VkPropertyStructureMapTrait {
    template <typename T>
    static constexpr VkStructureType getVkStructureType() {
        return getVkPropertyStructureType<T>();
    }
};

/// A heterogenous container of unique Vulkan structure types. The structures get zero initialized with `sType` filled
/// out appropriately and `pNext` used to chain them in the order they were added.
/// @remarks
///     Not all Vulkan structure types are allowed to be used in this map. They must either be included in
///     `tephra/vulkan/structures.hpp` or as one of the special cases in this class.
template <typename TStructureTypeTrait>
class VkStructureMap {
public:
    VkStructureMap() = default;

    /// Returns the stub of the first added structure in the chain.
    const VkStructureStub& front() const {
        return *frontPtr;
    }

    /// Returns the stub of the first added structure in the chain.
    VkStructureStub& front() {
        return *frontPtr;
    }

    /// Returns the stub of the last added structure in the chain.
    const VkStructureStub& back() const {
        return *backPtr;
    }

    /// Returns the stub of the last added structure in the chain.
    VkStructureStub& back() {
        return *backPtr;
    }

    /// Returns `true` if the map is empty.
    bool empty() const {
        return backPtr == nullptr;
    }

    /// Returns `true` if the map contains the given type.
    template <typename T>
    bool contains() const {
        // Handle deprecated and combined structs
        if constexpr (std::is_same_v<T, VkPhysicalDeviceFeatures>) {
            return contains<VkPhysicalDeviceFeatures2>();
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceProperties>) {
            return contains<VkPhysicalDeviceProperties2>();
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceLimits>) {
            return contains<VkPhysicalDeviceProperties2>();
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceSparseProperties>) {
            return contains<VkPhysicalDeviceProperties2>();
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceMemoryProperties>) {
            return contains<VkPhysicalDeviceMemoryProperties2>();
        } else {
            VkStructureType typeValue = TStructureTypeTrait::template getVkStructureType<T>();
            auto hit = map.find(typeValue);
            return hit != map.end();
        }
    }

    /// Returns an instance of the given type from the map. If it doesn't yet exist in the map, it is created with
    /// correct `sType` and `pNext` values. The rest of the structure gets zero initialized.
    template <typename T>
    auto& get() {
        // Handle deprecated and combined structs
        if constexpr (std::is_same_v<T, VkPhysicalDeviceFeatures>) {
            VkPhysicalDeviceFeatures2* features2Ptr = getOrMakeNew<VkPhysicalDeviceFeatures2>();
            return features2Ptr->features;
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceProperties>) {
            VkPhysicalDeviceProperties2* properties2Ptr = getOrMakeNew<VkPhysicalDeviceProperties2>();
            return properties2Ptr->properties;
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceLimits>) {
            VkPhysicalDeviceProperties2* properties2Ptr = getOrMakeNew<VkPhysicalDeviceProperties2>();
            return properties2Ptr->properties.limits;
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceSparseProperties>) {
            VkPhysicalDeviceProperties2* properties2Ptr = getOrMakeNew<VkPhysicalDeviceProperties2>();
            return properties2Ptr->properties.sparseProperties;
        } else if constexpr (std::is_same_v<T, VkPhysicalDeviceMemoryProperties>) {
            VkPhysicalDeviceMemoryProperties2* memProperties2Ptr = getOrMakeNew<VkPhysicalDeviceMemoryProperties2>();
            return memProperties2Ptr->memoryProperties;
        } else {
            return *getOrMakeNew<T>();
        }
    }

    /// Removes all elements from the map.
    void clear() {
        map.clear();
    }

    VkStructureMap(const VkStructureMap& other) noexcept : map(other.map) {
        // Fix pointers
        VkStructureStub* otherPtr = other.frontPtr;
        while (otherPtr != nullptr) {
            VkStructureStub* thisPtr = map[otherPtr->sType].template get<VkStructureStub>();

            if (backPtr != nullptr) {
                backPtr->pNext = thisPtr;
            } else {
                frontPtr = thisPtr;
            }
            backPtr = thisPtr;

            otherPtr = static_cast<VkStructureStub*>(otherPtr->pNext);
        }
    }

    VkStructureMap& operator=(VkStructureMap other) noexcept {
        other.swap(*this);
        return *this;
    }

    TEPHRA_MAKE_MOVABLE_DEFAULT(VkStructureMap);

    ~VkStructureMap() = default;

private:
    // Provides a way to store a POD-style structure without storing its type
    class StructureStorage {
    public:
        StructureStorage() : structureSize(0), data(nullptr) {}

        template <typename T>
        T* get() {
            return reinterpret_cast<T*>(data.get());
        }

        template <typename T>
        T* reset() {
            return static_cast<T*>(reset(sizeof(T)));
        }

        void* reset(std::size_t size) {
            structureSize = size;
            // Use zero-initializing new operator. Char should be properly aligned for all primitive types
            data.reset(new char[structureSize]());
            return data.get();
        }

        void swap(StructureStorage& other) {
            std::swap(structureSize, other.structureSize);
            std::swap(data, other.data);
        }

        StructureStorage(const StructureStorage& other) noexcept {
            reset(other.structureSize);
            std::copy(&other.data[0], &other.data[0] + structureSize, &data[0]);
        }

        StructureStorage& operator=(StructureStorage other) noexcept {
            other.swap(*this);
            return *this;
        }

        TEPHRA_MAKE_MOVABLE_DEFAULT(StructureStorage);

        ~StructureStorage() = default;

    private:
        std::size_t structureSize;
        std::unique_ptr<char[]> data;
    };

    std::unordered_map<VkStructureType, StructureStorage> map;

    VkStructureStub* frontPtr = nullptr;
    VkStructureStub* backPtr = nullptr;

    template <typename T>
    T* getOrMakeNew() {
        VkStructureType typeValue = TStructureTypeTrait::template getVkStructureType<T>();
        StructureStorage& structStorage = map[typeValue];

        T* structPtr = structStorage.template get<T>();
        if (structPtr == nullptr) {
            structStorage.template reset<T>();
            structPtr = structStorage.template get<T>();

            // Setup sType and pNext
            structPtr->sType = typeValue;
            if (backPtr != nullptr) {
                backPtr->pNext = structPtr;
            } else {
                frontPtr = reinterpret_cast<VkStructureStub*>(structPtr);
            }
            backPtr = reinterpret_cast<VkStructureStub*>(structPtr);
        }

        return structPtr;
    }

    void swap(VkStructureMap& other) {
        std::swap(map, other.map);
        std::swap(frontPtr, other.frontPtr);
        std::swap(backPtr, other.backPtr);
    }
};

/// tp::VkStructureMap specialization that accepts Vulkan feature structures.
using VkFeatureMap = VkStructureMap<VkFeatureStructureMapTrait>;

/// tp::VkStructureMap specialization that accepts Vulkan property structures.
using VkPropertyMap = VkStructureMap<VkPropertyStructureMapTrait>;

}
