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

class BufferImpl;

/// Shared base implementation of mapped tp::Buffer memory accessor.
class HostMappedMemoryBase {
public:
    HostMappedMemoryBase();
    HostMappedMemoryBase(
        BufferImpl* mappedBuffer,
        uint64_t mappingOffset,
        uint64_t mappingSize,
        bool hasReadAccess,
        bool hasWriteAccess);

    /// Returns `true` if no memory is mapped.
    bool isNull() const {
        return dataPtr == nullptr;
    }

    /// Returns the size of the mapped memory in bytes
    uint64_t getSize() const {
        return mappingSize;
    }

    TEPHRA_MAKE_NONCOPYABLE(HostMappedMemoryBase);
    TEPHRA_MAKE_MOVABLE_DEFAULT(HostMappedMemoryBase);

    ~HostMappedMemoryBase();

protected:
    void* dataPtr;

private:
    BufferImpl* mappedBuffer;
    uint64_t mappingOffset;
    uint64_t mappingSize;
    bool hasReadAccess;
    bool hasWriteAccess;
};

/// Represents tp::Buffer memory mapped for host read-only operations. Pointers are valid for the duration of the
/// lifetime of this object and that of the mapped buffer.
/// @see tp::BufferView::mapForHostRead
class HostReadableMemory : public HostMappedMemoryBase {
public:
    /// Constructs a null tp::HostReadableMemory
    HostReadableMemory() : HostMappedMemoryBase() {}

    HostReadableMemory(BufferImpl* mappedBuffer, uint64_t mappingOffset, uint64_t mappingSize)
        : HostMappedMemoryBase(mappedBuffer, mappingOffset, mappingSize, true, false) {}

    /// Returns a pointer to the mapped memory interpreted as the given type.
    template <typename T = void>
    const T* getPtr() const {
        return static_cast<const T*>(dataPtr);
    }

    /// Returns a pointer to the mapped memory with a byte offset interpreted as the given type.
    template <typename T = void>
    const T* getPtr(uint64_t byteOffset) const {
        return static_cast<const T*>(static_cast<const void*>(static_cast<const std::byte*>(dataPtr) + byteOffset));
    }

    /// Returns an array view of the mapped memory interpreted as the given type.
    template <typename T>
    ArrayView<const T> getArrayView() const {
        return ArrayView<const T>(getPtr<T>(), getSize() / sizeof(T));
    }

    /// Returns an array view of the mapped memory with a byte offset interpreted as `count` elements of the given type.
    template <typename T>
    ArrayView<const T> getArrayView(uint64_t byteOffset, uint64_t count) const;
};

/// Represents tp::Buffer memory mapped for host write-only operations. Intended for uploading data to the device.
/// @see tp::BufferView::mapForHostWrite
class HostWritableMemory : public HostMappedMemoryBase {
public:
    /// Constructs a null tp::HostWritableMemory
    HostWritableMemory() : HostMappedMemoryBase() {}

    HostWritableMemory(BufferImpl* mappedBuffer, uint64_t mappingOffset, uint64_t mappingSize)
        : HostMappedMemoryBase(mappedBuffer, mappingOffset, mappingSize, false, true) {}

    /// Returns a pointer to the mapped memory interpreted as the given type.
    /// @remarks
    ///     Reads from this pointer, even unintended or caused by compiler optimizations, may return undefined values or
    ///     cause performance degradation when accessing uncached memory.
    /// @remarks
    ///     The pointer is valid for the duration of the lifetime of this object and that of the mapped buffer.
    template <typename T = void>
    T* getPtrUnsafe() {
        return static_cast<T*>(dataPtr);
    }

    /// Writes a number of bytes with the given value into the mapped memory at the given byte offset.
    void write(uint64_t byteOffset, uint8_t value, uint64_t byteCount);

    /// Copies the given data range into the mapped memory at the given byte offset.
    template <typename T = std::byte>
    void write(uint64_t byteOffset, const T* srcPtr, uint64_t count) {
        writeTypeless(byteOffset, static_cast<const void*>(srcPtr), count * sizeof(T));
    }

    /// Copies the given data view into the mapped memory at the given byte offset.
    template <typename T>
    void write(uint64_t byteOffset, ArrayParameter<const T> srcView) {
        writeTypeless(byteOffset, static_cast<const void*>(srcView.data()), srcView.size() * sizeof(T));
    }

private:
    void writeTypeless(uint64_t byteOffset, const void* srcPtr, uint64_t srcSize);
};

/// Represents tp::Buffer memory mapped for generic host access. Pointers are valid for the duration of the
/// lifetime of this object and that of the mapped buffer.
/// @see tp::BufferView::mapForHostAccess
class HostAccessibleMemory : public HostMappedMemoryBase {
public:
    /// Constructs a null tp::HostAccessibleMemory
    HostAccessibleMemory() : HostMappedMemoryBase() {}

    HostAccessibleMemory(
        BufferImpl* mappedBuffer,
        uint64_t mappingOffset,
        uint64_t mappingSize,
        bool hasReadAccess = true,
        bool hasWriteAccess = true)
        : HostMappedMemoryBase(mappedBuffer, mappingOffset, mappingSize, hasReadAccess, hasWriteAccess) {}

    /// Returns a pointer to the mapped memory interpreted as the given type.
    template <typename T = void>
    T* getPtr() {
        return static_cast<T*>(dataPtr);
    }

    /// Returns a pointer to the mapped memory interpreted as the given type.
    template <typename T = void>
    const T* getPtr() const {
        return static_cast<const T*>(dataPtr);
    }

    /// Returns a pointer to the mapped memory with a byte offset interpreted as the given type.
    template <typename T = void>
    T* getPtr(uint64_t byteOffset) {
        return static_cast<T*>(static_cast<void*>(static_cast<std::byte*>(dataPtr) + byteOffset));
    }

    /// Returns a pointer to the mapped memory with a byte offset interpreted as the given type.
    template <typename T = void>
    const T* getPtr(uint64_t byteOffset) const {
        return static_cast<const T*>(static_cast<const void*>(static_cast<const std::byte*>(dataPtr) + byteOffset));
    }

    /// Returns an array view of the mapped memory interpreted as the given type.
    template <typename T>
    ArrayView<T> getArrayView() {
        return ArrayView<T>(getPtr<T>(), getSize() / sizeof(T));
    }

    /// Returns an array view of the mapped memory interpreted as the given type.
    template <typename T>
    ArrayView<const T> getArrayView() const {
        return ArrayView<const T>(getPtr<T>(), getSize() / sizeof(T));
    }

    /// Returns an array view of the mapped memory with a byte offset interpreted as `count` elements of the given type.
    template <typename T>
    ArrayView<T> getArrayView(uint64_t byteOffset, uint64_t count);

    /// Returns an array view of the mapped memory with a byte offset interpreted as `count` elements of the given type.
    template <typename T>
    ArrayView<const T> getArrayView(uint64_t byteOffset, uint64_t count) const;
};

}
