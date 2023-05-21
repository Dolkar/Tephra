#include "aliasing_suballocator.hpp"

namespace tp {

void ResourceUsageRange::update(uint64_t usage) {
    TEPHRA_ASSERT(firstUsage <= lastUsage);
    if (isEmpty()) {
        firstUsage = usage;
        lastUsage = usage;
    } else if (usage > lastUsage) {
        lastUsage = usage;
    } else if (usage < firstUsage) {
        firstUsage = usage;
    }
}

AliasingSuballocator::AliasingSuballocator(ArrayParameter<const uint64_t> backingSizes) {
    // To extend the algorithm to consider multiple backing buffers, we just need to make sure that no allocation
    // spans the buffer boundaries if they were put right after each other in a virtual address space. To do that,
    // we add dummy allocations of size 0 at those boundaries.

    allocations.reserve(backingSizes.size());
    uint64_t offset = 0;
    for (uint64_t backingSize : backingSizes) {
        offset += backingSize;

        ResourceUsageRange fullRange = { 0ull, ~0ull };
        allocations.emplace_back(fullRange, offset, 0);
    }
}

std::pair<uint32_t, uint64_t> AliasingSuballocator::allocate(
    uint64_t requiredSize,
    ResourceUsageRange usageRange,
    uint64_t requiredAlignment) {
    TEPHRA_ASSERT(requiredSize > 0);
    uint32_t allocIndex = 0; // Index of the backing allocation used
    uint64_t allocOffset = 0; // Offset of the backing allocation used
    uint64_t offset = 0; // The virtual offset across the space of all backing allocations
    int sortedIndex = 0; // The index where the allocation should be inserted wrt its virtual offset

    // Walk over already assigned allocations for ones with overlapping usage in the order they are assigned
    // in memory and find an empty space for this one.
    for (int i = 0; i < allocations.size(); i++) {
        const Allocation& otherAllocation = allocations[i];

        if (!usageRange.isOverlapping(otherAllocation)) {
            // Non-overlapping usage
            continue;
        }

        if (offset + requiredSize <= otherAllocation.offset) {
            // Allocation fits to the left. Since the current allocations are sorted, we know all the space between
            // offset and otherAllocation.offset is free to use
            break;
        } else {
            // Have to allocate to the right of the allocation
            if (otherAllocation.isBackingAllocBoundary()) {
                // We have crossed to another backing allocation
                allocIndex++;
                allocOffset = otherAllocation.offset;
            }

            uint64_t unalignedOffset = otherAllocation.offset + otherAllocation.size;
            offset = allocOffset + roundUpToPoTMultiple(unalignedOffset - allocOffset, requiredAlignment);
            // This new allocation would go right after this one
            sortedIndex = i + 1;
        }
    }

    // Insert the new allocation to consider it for the next one
    allocations.emplace(allocations.begin() + sortedIndex, usageRange, offset, requiredSize);
    usedSize = tp::max(usedSize, offset + requiredSize);

    // Convert virtual offset to actual offset
    TEPHRA_ASSERT(offset >= allocOffset);
    return { allocIndex, offset - allocOffset };
}

}
