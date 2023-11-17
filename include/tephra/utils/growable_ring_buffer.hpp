#pragma once

#include <tephra/tephra.hpp>
#include <deque>

namespace tp {

/// Convenience utilities that build upon the base Tephra interface.
namespace utils {

    /// A ring buffer implementation that supports resizing. Useful for suballocating and reusing buffer memory.
    ///
    /// To increase capacity, user allocated tp::Buffer objects must be provided. Individual tp::BufferView objects
    /// can then be requested and will be allocated from one of these backing buffers. The allocations can then be freed
    /// in the order they were allocated from.
    ///
    /// Implemented as a list of regular fixed size ring buffers. The ring buffers are considered for allocation in
    /// sequence, starting with the last used one.
    class GrowableRingBuffer {
    public:
        /// Tries to allocate a tp::BufferView with the given size.
        ///
        /// If the allocation fails due to a lack of space, the returned view will be null. Use
        /// tp::utils::GrowableRingBuffer::grow to add more space for future allocations.
        ///
        /// @param allocationSize
        ///     The size of the allocation to be made.
        tp::BufferView push(uint64_t allocationSize);

        /// Tries to allocate a tp::BufferView with the given size. Every buffer will only serve a single allocation.
        /// This function is meant for debugging purposes and isn't ideal for memory consumption.
        ///
        /// If the allocation fails due to a lack of space, the returned view will be null. Use
        /// tp::utils::GrowableRingBuffer::grow to add more space for future allocations.
        ///
        /// @param allocationSize
        ///     The size of the allocation to be made.
        tp::BufferView pushNoSuballocate(uint64_t allocationSize);

        /// Frees the least recently allocated tp::BufferView from this ring buffer, allowing its memory region to be
        /// reused.
        void pop();

        /// Returns the least recently allocated tp::BufferView from this ring buffer.
        tp::BufferView peek();

        /// Adds a new buffer region to be used for suballocating views from.
        /// @param newRegionBuffer
        ///     Pointer to a buffer to use for suballocating views from.
        /// @remarks
        ///     The buffer must not be destroyed before the tp::utils::GrowableRingBuffer.
        /// @remarks
        ///     Given that it's not defined which buffer will be serving a view allocation, it is expected that all of
        ///     the provided buffers will have the same usage flags and memory preference.
        void grow(tp::Buffer* newRegionBuffer);

        /// Releases a previously added buffer if one is available and unused, otherwise returns nullptr.
        tp::Buffer* shrink();

        /// Returns the number of regions used so far.
        uint64_t getRegionCount() const {
            return regions.size();
        }

        /// Returns the number of active allocations.
        uint64_t getAllocationCount() const {
            return allocations.size();
        }

        /// Returns the total size of all regions in bytes.
        uint64_t getTotalSize() const {
            return totalRegionSize;
        }

        /// Returns the total size of all allocations in bytes.
        uint64_t getAllocationSize() const {
            return totalAllocationSize;
        }

    private:
        struct AllocationInfo {
            tp::BufferView bufferView;
            uint64_t regionIndex;
            uint64_t allocationOffset;
        };

        struct RegionInfo {
            tp::Buffer* buffer;
            uint64_t minAlignment;
            uint64_t headOffset;
            uint64_t tailOffset;
            uint64_t truncatedSize;
            uint32_t allocationCount;
        };

        std::deque<AllocationInfo> allocations;
        std::vector<RegionInfo> regions;
        uint64_t headRegionIndex = 0;
        uint64_t totalRegionSize = 0;
        uint64_t totalAllocationSize = 0;
    };

    /// A ring buffer implementation with automatic allocation of memory and timestamp-based garbage collection.
    ///
    /// An abstraction around the more involved tp::utils::GrowableRingBuffer. This one automatically allocates more
    /// buffers when running out of space.
    ///
    /// @see tp::utils::GrowableRingBuffer
    class AutoRingBuffer {
    public:
        /// @param device
        ///     The Tephra device that new buffers should be allocated from.
        /// @param usage
        ///     The expected usage of the views allocated from this ring buffer.
        /// @param memoryPreference
        ///     The memory preference of the underlying memory.
        /// @param overallocationBehavior
        ///     The overallocation behavior to be applied when allocating new space.
        /// @param debugName
        ///     The debug name to use as a basis for the backing buffers.
        explicit AutoRingBuffer(
            tp::Device* device,
            tp::BufferUsageMask usage,
            tp::MemoryPreference memoryPreference,
            tp::OverallocationBehavior overallocationBehavior = { 3.0f, 1.5f, 65536 },
            const char* debugName = nullptr);

        /// Allocate a tp::BufferView with the given size.
        ///
        /// @param allocationSize
        ///     The size of the allocation to be made.
        /// @param timestamp
        ///     The timestamp determining the allocation lifetime. See tp::utils::AutoRingBuffer::pop.
        /// @remarks
        ///     `timestamp` must be greater or equal to the last timestamp passed to this function. In other words,
        ///     the timestamp values must be monotonically increasing.
        tp::BufferView push(uint64_t allocationSize, uint64_t timestamp);

        /// Frees all of the allocations with a timestamp value less or equal to `upToTimestamp`, allowing their memory
        /// regions to be reused.
        void pop(uint64_t upToTimestamp);

        /// Attempts to free up unused memory regions. Returns the number of bytes freed.
        uint64_t trim();

        /// Returns the number of regions used so far.
        uint64_t getRegionCount() const {
            return growableBuffer.getRegionCount();
        }

        /// Returns the number of active allocations.
        uint64_t getAllocationCount() const {
            return growableBuffer.getAllocationCount();
        }

        /// Returns the total size of all regions in bytes.
        uint64_t getTotalSize() const {
            return growableBuffer.getTotalSize();
        }

        /// Returns the total size of all allocations in bytes.
        uint64_t getAllocatedSize() const {
            return growableBuffer.getAllocationSize();
        }

    private:
        tp::Device* device;
        tp::BufferUsageMask usage;
        tp::MemoryPreference memoryPreference;
        tp::OverallocationBehavior overallocationBehavior;
        std::string debugName;

        GrowableRingBuffer growableBuffer;
        std::vector<tp::OwningPtr<tp::Buffer>> regionBuffers;
        std::deque<uint64_t> allocationTimestamps;
    };

}
}
