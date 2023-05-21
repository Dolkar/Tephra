#pragma once

#include "../common_impl.hpp"

namespace tp {

// Captures the indices of the first and last command a resource is used in. Both ends are inclusive.
struct ResourceUsageRange {
    uint64_t firstUsage = ~0;
    uint64_t lastUsage = ~0;

    bool isEmpty() const {
        return firstUsage == ~0;
    }

    bool isOverlapping(const ResourceUsageRange& other) const {
        return firstUsage <= other.lastUsage && lastUsage >= other.firstUsage;
    }

    void update(uint64_t usage);
};

// Defines a suballocation algorithm that greedily aliases resources whose usage range doesn't overlap. It will
// progressively use up space in the backing allocations of the given sizes.
// Takes O(N) space and O(N^2) time for N allocations.
class AliasingSuballocator {
public:
    // Assigns a new set of backing allocations. After the ones provided, an unbounded backing allocation is assumed
    // to exist to which leftover resources that don't fit will be assigned.
    AliasingSuballocator(ArrayParameter<const uint64_t> backingSizes);

    // Returns the index of the backing allocation and an offset into it.
    std::pair<uint32_t, uint64_t> allocate(
        uint64_t requiredSize,
        ResourceUsageRange usageRange,
        uint64_t requiredAlignment);

    // Calculates the size of the total used space, including fragmentation between backing allocs
    uint64_t getUsedSize() const {
        return usedSize;
    }

private:
    struct Allocation : ResourceUsageRange {
        uint64_t offset;
        uint64_t size;

        Allocation(const ResourceUsageRange& usageRange, uint64_t offset, uint64_t size)
            : ResourceUsageRange(usageRange), offset(offset), size(size) {}

        bool isBackingAllocBoundary() const {
            return size == 0;
        }
    };

    // List of current allocations sorted by offset
    ScratchVector<Allocation> allocations;
    // Total used size
    uint64_t usedSize = 0;
};

}
