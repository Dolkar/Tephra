#include "../common_impl.hpp"
#include <tephra/utils/growable_ring_buffer.hpp>
#include <algorithm>

namespace tp {
namespace utils {

    tp::BufferView GrowableRingBuffer::push(uint64_t allocationSize) {
        if (regions.empty())
            return {};
        TEPHRA_ASSERT(allocationSize > 0);
        TEPHRA_ASSERT(headRegionIndex < regions.size());

        uint64_t regionIndex = headRegionIndex;
        do {
            RegionInfo& region = regions[regionIndex];

            uint64_t alignment = region.minAlignment;
            uint64_t candidateOffset = roundUpToPoTMultiple(region.headOffset, alignment);
            uint64_t candidateEnd = candidateOffset + allocationSize;

            bool doAllocate = false;
            if (region.tailOffset < region.headOffset) {
                // [----TXXXXXXH--]
                if (candidateEnd <= region.buffer->getSize()) {
                    doAllocate = true;
                } else {
                    // Does not fit by the end of the array, try wrap around the beginning
                    candidateOffset = 0;
                    candidateEnd = allocationSize;
                    if (candidateEnd <= region.tailOffset) {
                        doAllocate = true;
                        // Mark the now empty space at the end as truncated so we know it can be recovered later
                        region.truncatedSize = region.headOffset;
                    }
                }
            } else {
                // [XXH-----TXXX--]
                if (candidateEnd <= region.tailOffset)
                    doAllocate = true;
            }

            if (doAllocate) {
                TEPHRA_ASSERT(region.buffer != nullptr);
                AllocationInfo allocInfo;
                allocInfo.bufferView = region.buffer->getView(candidateOffset, allocationSize);
                allocInfo.regionIndex = regionIndex;
                allocInfo.allocationOffset = candidateOffset;
                allocations.push_back(allocInfo);

                region.headOffset = candidateEnd;
                region.allocationCount++;
                totalAllocationSize += allocationSize;

                // Next time start searching in this region
                headRegionIndex = regionIndex;

                return allocInfo.bufferView;
            }

            regionIndex++;
            if (regionIndex >= regions.size())
                regionIndex = 0;
        } while (regionIndex != headRegionIndex);

        // No free regions
        return {};
    }

    tp::BufferView GrowableRingBuffer::pushNoSuballocate(uint64_t allocationSize) {
        if (regions.empty())
            return {};
        TEPHRA_ASSERT(headRegionIndex < regions.size());

        uint64_t regionIndex = headRegionIndex;
        do {
            RegionInfo& region = regions[regionIndex];

            // If buffer exists, is big enough and empty, allocate
            if (region.buffer != nullptr && region.buffer->getSize() >= allocationSize && region.headOffset == 0 &&
                region.tailOffset == region.buffer->getSize()) {
                AllocationInfo allocInfo;
                allocInfo.bufferView = region.buffer->getDefaultView();
                allocInfo.regionIndex = regionIndex;
                allocInfo.allocationOffset = 0;
                allocations.push_back(allocInfo);

                region.headOffset = region.tailOffset;
                region.allocationCount++;
                totalAllocationSize += region.buffer->getSize();

                // Next time start searching in the next region
                headRegionIndex = regionIndex + 1;
                if (headRegionIndex >= regions.size())
                    headRegionIndex = 0;

                return allocInfo.bufferView;
            }

            regionIndex++;
            if (regionIndex >= regions.size())
                regionIndex = 0;
        } while (regionIndex != headRegionIndex);

        // No free regions
        return {};
    }

    void GrowableRingBuffer::pop() {
        TEPHRA_ASSERT(!allocations.empty());

        AllocationInfo poppedAllocInfo = allocations.front();
        allocations.pop_front();

        RegionInfo& region = regions[poppedAllocInfo.regionIndex];
        TEPHRA_ASSERT(region.allocationCount > 0);
        region.allocationCount--;
        region.tailOffset = poppedAllocInfo.allocationOffset + poppedAllocInfo.bufferView.getSize();

        uint64_t regionSize = region.buffer->getSize();
        if (region.tailOffset == region.headOffset) {
            // Freed the whole buffer, reset it to the beginning
            TEPHRA_ASSERT(region.allocationCount == 0);
            region.headOffset = 0;
            region.tailOffset = regionSize;
        }

        if (region.tailOffset >= region.truncatedSize) {
            // There is more unused space at the end of the array, recover it
            region.tailOffset = regionSize;
            region.truncatedSize = regionSize;
        }

        totalAllocationSize -= poppedAllocInfo.bufferView.getSize();
    }

    tp::BufferView GrowableRingBuffer::peek() {
        if (allocations.empty()) {
            return tp::BufferView();
        } else {
            return allocations.front().bufferView;
        }
    }

    void GrowableRingBuffer::grow(tp::Buffer* newRegionBuffer) {
        TEPHRA_ASSERT(newRegionBuffer != nullptr);

        uint64_t newRegionSize = newRegionBuffer->getSize();
        totalRegionSize += newRegionSize;

        RegionInfo newRegion;
        newRegion.buffer = newRegionBuffer;
        newRegion.minAlignment = newRegion.buffer->getRequiredViewAlignment();
        newRegion.headOffset = 0;
        newRegion.tailOffset = newRegionSize;
        newRegion.truncatedSize = newRegionSize;
        newRegion.allocationCount = 0;

        if (!regions.empty()) {
            // Try to assign the buffer to a previously freed region
            uint64_t regionIndex = headRegionIndex;
            do {
                RegionInfo& region = regions[regionIndex];
                if (region.buffer == nullptr) {
                    regions[regionIndex] = newRegion;
                    return;
                }

                regionIndex++;
                if (regionIndex >= regions.size())
                    regionIndex = 0;
            } while (regionIndex != headRegionIndex);
        }

        // Otherwise add it at the end
        regions.push_back(newRegion);
    }

    tp::Buffer* GrowableRingBuffer::shrink() {
        if (regions.empty())
            return nullptr;
        TEPHRA_ASSERT(headRegionIndex < regions.size());

        uint64_t regionIndex = headRegionIndex;
        do {
            RegionInfo& region = regions[regionIndex];

            if (region.allocationCount == 0 && region.buffer != nullptr) {
                // Unused region, free it
                tp::Buffer* bufferHandle = region.buffer;
                region.buffer = nullptr;
                region.tailOffset = 0;

                TEPHRA_ASSERT(totalRegionSize >= bufferHandle->getSize());
                totalRegionSize -= bufferHandle->getSize();

                return bufferHandle;
            }

            regionIndex++;
            if (regionIndex >= regions.size())
                regionIndex = 0;
        } while (regionIndex != headRegionIndex);

        // No free regions
        return nullptr;
    }

    AutoRingBuffer::AutoRingBuffer(
        tp::Device* device,
        tp::BufferUsageMask usage,
        tp::MemoryPreference memoryPreference,
        tp::OverallocationBehavior overallocationBehavior,
        const char* debugName)
        : device(device),
          usage(usage),
          memoryPreference(memoryPreference),
          overallocationBehavior(overallocationBehavior),
          debugName(debugName ? debugName : std::string()) {}

    tp::BufferView AutoRingBuffer::push(uint64_t allocationSize, uint64_t timestamp) {
        TEPHRA_ASSERT(allocationTimestamps.empty() || allocationTimestamps.back() <= timestamp);

        tp::BufferView newBuffer = growableBuffer.push(allocationSize);
        if (newBuffer.isNull()) {
            // TODO: Handle out of memory exception, fallback to allocating a smaller buffer
            uint64_t sizeToAlloc = overallocationBehavior.apply(allocationSize, growableBuffer.getTotalSize());

            std::string regionDebugName;
            if (!debugName.empty()) {
                regionDebugName = debugName + std::to_string(growableBuffer.getRegionCount());
            }
            regionBuffers.push_back(device->allocateBuffer(
                tp::BufferSetup(sizeToAlloc, usage),
                memoryPreference,
                regionDebugName.empty() ? nullptr : regionDebugName.c_str()));

            growableBuffer.grow(getOwnedPtr(regionBuffers.back()));

            newBuffer = growableBuffer.push(allocationSize);
            TEPHRA_ASSERT(!newBuffer.isNull());
        }

        allocationTimestamps.push_back(timestamp);
        return newBuffer;
    }

    void AutoRingBuffer::pop(uint64_t upToTimestamp) {
        while (!allocationTimestamps.empty()) {
            if (allocationTimestamps.front() <= upToTimestamp) {
                growableBuffer.pop();
                allocationTimestamps.pop_front();
            } else {
                break;
            }
        }
    }

    uint64_t AutoRingBuffer::trim() {
        uint64_t startSize = getTotalSize();

        while (tp::Buffer* freedBuffer = growableBuffer.shrink()) {
            auto freedBufferIt = std::find_if(
                regionBuffers.begin(), regionBuffers.end(), [freedBuffer](const tp::OwningPtr<Buffer>& el) {
                    return getOwnedPtr(el) == freedBuffer;
                });
            TEPHRA_ASSERT(freedBufferIt != regionBuffers.end());

            // Similar code in PreinitializedBufferAllocator releases the handle immediatelly rather than letting
            // it go through the deferred destruction. We can't do that here, because we don't know if the timestamps
            // actually correspond to device timeline semaphores.
            regionBuffers.erase(freedBufferIt);
        }

        TEPHRA_ASSERT(getTotalSize() <= startSize);
        return startSize - getTotalSize();
    }
}
}
