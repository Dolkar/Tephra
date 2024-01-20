
#include <tephra/memory.hpp>
#include <tephra/buffer.hpp>
#include "buffer_impl.hpp"

namespace tp {

constexpr const char* HostMappedMemoryTypeName = "HostMappedMemory";

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

HostMappedMemory::HostMappedMemory()
    : dataPtr(nullptr), mappedBuffer(nullptr), mappingOffset(0), mappingSize(0), accessType(MemoryAccess::ReadOnly) {}

HostMappedMemory::HostMappedMemory(
    Buffer* mappedBuffer,
    uint64_t mappingOffset,
    uint64_t mappingSize,
    MemoryAccess accessType)
    : dataPtr(nullptr),
      mappedBuffer(mappedBuffer),
      mappingOffset(mappingOffset),
      mappingSize(mappingSize),
      accessType(accessType) {
    TEPHRA_ASSERT(mappedBuffer != nullptr);
    BufferImpl* bufferImpl = static_cast<BufferImpl*>(mappedBuffer);

    TEPHRA_DEBUG_SET_CONTEXT_TEMP(bufferImpl->getDebugTarget(), HostMappedMemoryTypeName, "constructor", nullptr);

    dataPtr = bufferImpl->beginHostAccess(mappingOffset, mappingSize, accessType);
}

HostMappedMemory::~HostMappedMemory() {
    if (mappedBuffer != nullptr) {
        BufferImpl* bufferImpl = static_cast<BufferImpl*>(mappedBuffer);

        TEPHRA_DEBUG_SET_CONTEXT_TEMP(bufferImpl->getDebugTarget(), HostMappedMemoryTypeName, "destructor", nullptr);

        bufferImpl->endHostAccess(mappingOffset, mappingSize, accessType);
    }
}

template <typename T>
ArrayView<T> HostMappedMemory::getArrayView(uint64_t byteOffset, uint64_t count) {
    TEPHRA_ASSERT(byteOffset + count * sizeof(T) <= getSize());
    return ArrayView<T>(getPtr<T>(byteOffset), count);
}

template <typename T>
ArrayView<const T> HostMappedMemory::getArrayView(uint64_t byteOffset, uint64_t count) const {
    TEPHRA_ASSERT(byteOffset + count * sizeof(T) <= getSize());
    return ArrayView<const T>(getPtr<T>(byteOffset), count);
}

}
