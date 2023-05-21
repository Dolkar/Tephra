#pragma once

#include "math.hpp"
#include <memory>
#include <vector>
#include <deque>

namespace tp {

// Scratch allocator for short-lived allocations handling API calls. Uses thread_local state, freeing everything only
// after all prior allocations get deallocated.
class ScratchAllocatorState {
public:
    template <typename T>
    T* allocate(std::size_t count) {
        // Round up everything to base alignment
        static_assert(alignof(T) <= UnitSize);
        std::size_t unitCount = roundUpToMultiple(count * sizeof(T), sizeof(UnitType)) / sizeof(UnitType);

        // Handle vector re-allocations ourselves, keep the old vectors alive until deallocation
        if (memory.size() - memoryUsed < unitCount) {
            std::size_t newSize = memory.size() + tp::max(memory.size(), unitCount);
            graveyard.push_back(std::move(memory));
            memory = std::vector<UnitType>(newSize);
            memoryUsed = 0;

            if constexpr (TephraValidationEnabled) {
                if (getAllocatedMemoryBytes() > MemorySizeWarnThreshold)
                    reportDebugMessage(
                        DebugMessageSeverity::Warning,
                        DebugMessageType::Performance,
                        "Internal scratch memory has reached an unexpectedly high size (",
                        getAllocatedMemoryBytes(),
                        "), please check your API usage or report an issue.");
            }
        }

        T* ptr = reinterpret_cast<T*>(memory.data() + memoryUsed);
        memoryUsed += unitCount;
        allocCounter++;
        return ptr;
    }

    void deallocate() {
        TEPHRA_ASSERT(allocCounter > 0);
        allocCounter--;
        if (allocCounter == 0) {
            // Cleanup
            graveyard.clear();
            memoryUsed = 0;
        }
    }

    void trim() {
        TEPHRA_ASSERT(isEmpty());
        memory.clear();
    }

    bool isEmpty() const {
        return memoryUsed == 0;
    }

    std::size_t getAllocatedMemoryBytes() const {
        return memory.size() * UnitSize;
    }

    static ScratchAllocatorState* get() {
        thread_local ScratchAllocatorState state;
        return &state;
    }

private:
    static constexpr std::size_t UnitSize = alignof(max_align_t);
    static constexpr std::size_t MemorySizeWarnThreshold = 64 * 1024 * 1024;
    using UnitType = typename std::aligned_storage<UnitSize, UnitSize>::type;

    // Current memory buffer
    std::vector<UnitType> memory;
    // Old memory buffers kept alive until the next cleanup
    std::vector<std::vector<UnitType>> graveyard;
    // Memory used from the current memory buffer
    std::size_t memoryUsed = 0;
    // Number of currently active allocations
    uint64_t allocCounter = 0;
};

// Stateless allocator adapter that can be passed to std containers
template <typename T>
class ScratchAllocator {
public:
    using value_type = T;
    using pointer = value_type*;
    using propagate_on_container_move_assignment = std::true_type;
    using is_always_equal = std::true_type;

    ScratchAllocator() noexcept = default;

    template <typename U>
    ScratchAllocator(const ScratchAllocator<U>& other) noexcept {};

    pointer allocate(std::size_t count) {
        return ScratchAllocatorState::get()->allocate<value_type>(count);
    }

    void deallocate(pointer ptr, std::size_t count) {
        ScratchAllocatorState::get()->deallocate();
    }
};

template <typename T1, typename T2>
bool operator==(const ScratchAllocator<T1>& lhs, const ScratchAllocator<T2>& rhs) noexcept {
    return true;
}

template <typename T1, typename T2>
bool operator!=(const ScratchAllocator<T1>& lhs, const ScratchAllocator<T2>& rhs) noexcept {
    return false;
}

template <typename T>
using ScratchVector = std::vector<T, ScratchAllocator<T>>;

template <typename T>
using ScratchDeque = std::deque<T, ScratchAllocator<T>>;

}
