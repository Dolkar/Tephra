#pragma once

#include <tephra/common.hpp>

namespace tp {

/// Defines the memory location types according to its accessibility and intended use.
/// @see @vksymbol{VkMemoryPropertyFlagBits}
enum class MemoryLocation : uint8_t {
    /// An undefined, invalid memory location.
    Undefined,
    /// Memory that is efficient for device access, but isn't accessible by the host.
    DeviceLocal,
    /// Memory that is efficient for device access and is accessible by the host without caching.
    DeviceLocalHostVisible,
    /// Memory that is efficient for device access and is accessible by the host with caching.
    DeviceLocalHostCached,
    /// Memory that is accessible by device less efficiently and by the host without caching.
    HostVisible,
    /// Memory that is accessible by device less efficiently and by the host with caching.
    HostCached,
};
TEPHRA_MAKE_CONTIGUOUS_ENUM_VIEW(MemoryLocationEnumView, MemoryLocation, HostCached);

/// Specifies the type of memory access.
enum class MemoryAccess {
    /// The access is read only.
    ReadOnly,
    /// The access is write only.
    WriteOnly,
    /// The access is both read and write.
    ReadWrite,
};

/// Represents the progression of tp::MemoryLocation that the device will attempt to allocate resources from,
/// in the given order of preference.
struct MemoryPreference {
    MemoryPreference() : hash(0) {}

    /// @param locationProgression
    ///     The progression of memory locations in the order of preference.
    /// @param createPersistentlyMapped
    ///     If set to `true`, the memory allocated from this location will be mapped for host access upon creation
    ///     and unmapped only when freed.
    MemoryPreference(std::initializer_list<MemoryLocation> locationProgression, bool createPersistentlyMapped);

    union {
        struct {
            MemoryLocation locationProgression[MemoryLocationEnumView::size()];
            bool createPersistentlyMapped;
        };
        uint64_t hash;
    };

    /// Only device-local memory will be allocated, otherwise memory allocation error is thrown. This preference
    /// should be used when the resource does not need to be directly accessible by the host, but fast
    /// access by the device is needed. Images are always allocated with this memory preference.
    ///
    /// Progression: `DeviceLocal` -> `DeviceLocalHostVisible` -> `DeviceLocalHostCached`
    const static MemoryPreference Device;
    /// Used for resources that should live in host memory. Meant for large data that is being read by the device
    /// infrequently and shouldn't be wasting the potentially limited device local, host visible memory. This is the
    /// best progression for staging buffers used to copy data to device local memory.
    ///
    /// Progression: `HostCached` -> `HostVisible` -> `DeviceLocalHostCached` -> `DeviceLocalHostVisible`
    const static MemoryPreference Host;
    /// Used for priority resources that are written to by the host and need to be read by the device with low latency.
    /// If device locality is required, the resulting memory location of the allocation should be checked for a
    /// potential fallback to be used as a staging buffer.
    ///
    /// Progression: `DeviceLocalHostVisible` -> `DeviceLocalHostCached` -> `HostVisible` -> `HostCached`
    const static MemoryPreference UploadStream;
    /// Used for priority resources that are written to by the device and need to be read by the host with low latency.
    ///
    /// Progression: `DeviceLocalHostCached` -> `HostCached` -> `DeviceLocalHostVisible` -> `HostVisible`
    const static MemoryPreference ReadbackStream;
};
static_assert(sizeof(MemoryPreference) == sizeof(uint64_t), "Size of MemoryPreference is not suitable for hashing.");

class Buffer;

/// Represents tp::Buffer memory mapped for host access by the application. Automatically handles cache
/// management for incoherent memory types and unmaps the memory when destroyed, if the memory is not mapped
/// persistently.
/// @see tp::BufferView::mapForHostAccess
class HostMappedMemory {
public:
    HostMappedMemory();

    /// Maps a range of tp::Buffer memory. See tp::BufferView::mapForHostAccess for more convenient mapping of
    /// tp::BufferView objects.
    /// @param mappedBuffer
    ///     The tp::Buffer to map.
    /// @param mappingOffset
    ///     The base offset to the memory range to be mapped.
    /// @param mappingSize
    ///     The size of the memory range to be mapped.
    /// @param accessType
    ///     The type of access.
    HostMappedMemory(Buffer* mappedBuffer, uint64_t mappingOffset, uint64_t mappingSize, MemoryAccess accessType);

    /// Returns a pointer to the mapped memory interpreted as the given type.
    template <typename T = void*>
    T getPtr() {
        return static_cast<T>(dataPtr);
    }

    /// Returns a pointer to the mapped memory interpreted as the given type.
    template <typename T = void*>
    const T getPtr() const {
        return static_cast<const T>(dataPtr);
    }

    /// Returns a pointer to the mapped memory with a byte offset interpreted as the given type.
    template <typename T = void*>
    T getPtr(uint64_t byteOffset) {
        return static_cast<T>(static_cast<void*>(static_cast<std::byte*>(dataPtr) + byteOffset));
    }

    /// Returns a pointer to the mapped memory with a byte offset interpreted as the given type.
    template <typename T = void*>
    const T getPtr(uint64_t byteOffset) const {
        return static_cast<const T>(static_cast<const void*>(static_cast<const std::byte*>(dataPtr) + byteOffset));
    }

    /// Returns `true` if no memory is mapped.
    bool isNull() const {
        return dataPtr == nullptr;
    }

    TEPHRA_MAKE_NONCOPYABLE(HostMappedMemory);
    TEPHRA_MAKE_MOVABLE_DEFAULT(HostMappedMemory);

    ~HostMappedMemory();

private:
    void* dataPtr;
    Buffer* mappedBuffer;
    uint64_t mappingOffset;
    uint64_t mappingSize;
    MemoryAccess accessType;
};

}
