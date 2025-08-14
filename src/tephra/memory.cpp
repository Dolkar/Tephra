
#include <tephra/memory.hpp>
#include <tephra/buffer.hpp>
#include "buffer_impl.hpp"

namespace tp {

constexpr const char* HostMappedMemoryTypeName = "HostMappedMemoryBase";

MemoryPreference::MemoryPreference(
    std::initializer_list<MemoryLocation> locationProgression,
    bool createPersistentlyMapped)
    : hash(0) {
    TEPHRA_ASSERT(locationProgression.size() > 0);
    TEPHRA_ASSERT(locationProgression.size() <= MemoryLocationEnumView::size());

    this->createPersistentlyMapped = createPersistentlyMapped;
    std::copy(locationProgression.begin(), locationProgression.end(), this->locationProgression);
}

const MemoryPreference MemoryPreference::Device = MemoryPreference(
    {
        MemoryLocation::DeviceLocal,
        MemoryLocation::DeviceLocalHostVisible,
        MemoryLocation::DeviceLocalHostCached,
    },
    false);

const MemoryPreference MemoryPreference::Host = MemoryPreference(
    {
        MemoryLocation::HostCached,
        MemoryLocation::HostVisible,
        MemoryLocation::DeviceLocalHostCached,
        MemoryLocation::DeviceLocalHostVisible,
    },
    true);

const MemoryPreference MemoryPreference::UploadStream = MemoryPreference(
    {
        MemoryLocation::DeviceLocalHostVisible,
        MemoryLocation::DeviceLocalHostCached,
        MemoryLocation::HostVisible,
        MemoryLocation::HostCached,
    },
    true);

const MemoryPreference MemoryPreference::ReadbackStream = MemoryPreference(
    {
        MemoryLocation::DeviceLocalHostCached,
        MemoryLocation::HostCached,
        MemoryLocation::DeviceLocalHostVisible,
        MemoryLocation::HostVisible,
    },
    true);

HostMappedMemoryBase::HostMappedMemoryBase()
    : dataPtr(nullptr),
      mappedBuffer(nullptr),
      mappingOffset(0),
      mappingSize(0),
      hasReadAccess(false),
      hasWriteAccess(false) {}

HostMappedMemoryBase::HostMappedMemoryBase(
    BufferImpl* mappedBuffer,
    uint64_t mappingOffset,
    uint64_t mappingSize,
    bool hasReadAccess,
    bool hasWriteAccess)
    : dataPtr(nullptr),
      mappedBuffer(mappedBuffer),
      mappingOffset(mappingOffset),
      mappingSize(mappingSize),
      hasReadAccess(hasReadAccess),
      hasWriteAccess(hasWriteAccess) {
    TEPHRA_ASSERT(mappedBuffer != nullptr);
    TEPHRA_DEBUG_SET_CONTEXT_TEMP(mappedBuffer->getDebugTarget(), HostMappedMemoryTypeName, "constructor", nullptr);
    dataPtr = mappedBuffer->beginHostAccess(mappingOffset, mappingSize, hasReadAccess);
}

HostMappedMemoryBase::~HostMappedMemoryBase() {
    if (mappedBuffer != nullptr) {
        TEPHRA_DEBUG_SET_CONTEXT_TEMP(mappedBuffer->getDebugTarget(), HostMappedMemoryTypeName, "destructor", nullptr);
        mappedBuffer->endHostAccess(mappingOffset, mappingSize, hasWriteAccess);
    }
}

template <typename T>
ArrayView<const T> HostReadableMemory::getArrayView(uint64_t byteOffset, uint64_t count) const {
    TEPHRA_ASSERT(byteOffset + count * sizeof(T) <= getSize());
    return ArrayView<const T>(getPtr<T>(byteOffset), count);
}

void HostWritableMemory::write(uint64_t byteOffset, uint8_t value, uint64_t byteCount) {
    TEPHRA_ASSERT(byteOffset + byteCount <= getSize());
    std::memset(static_cast<std::byte*>(dataPtr) + byteOffset, static_cast<int>(value), byteCount);
}

void HostWritableMemory::writeTypeless(uint64_t byteOffset, const void* srcPtr, uint64_t srcSize) {
    TEPHRA_ASSERT(byteOffset + srcSize <= getSize());
    std::memcpy(static_cast<std::byte*>(dataPtr) + byteOffset, srcPtr, srcSize);
}

template <typename T>
ArrayView<T> HostAccessibleMemory::getArrayView(uint64_t byteOffset, uint64_t count) {
    TEPHRA_ASSERT(byteOffset + count * sizeof(T) <= getSize());
    return ArrayView<T>(getPtr<T>(byteOffset), count);
}

template <typename T>
ArrayView<const T> HostAccessibleMemory::getArrayView(uint64_t byteOffset, uint64_t count) const {
    TEPHRA_ASSERT(byteOffset + count * sizeof(T) <= getSize());
    return ArrayView<const T>(getPtr<T>(byteOffset), count);
}

}
