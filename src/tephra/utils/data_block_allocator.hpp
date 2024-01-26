#pragma once

#include "math.hpp"
#include <tephra/tools/array.hpp>
#include <deque>
#include <memory>

namespace tp {

template <std::size_t BlockSize = 4096, std::size_t AlignSize = alignof(max_align_t)>
class DataBlockAllocator {
public:
    // Allocates memory for count number of T sized objects. The objects are not constructed and the memory is not
    // initialized.
    template <typename T = std::byte>
    ArrayView<T> allocate(std::size_t count) {
        std::size_t requiredSize = count * sizeof(T);

        if (requiredSize > BlockSize) {
            // Use a dynamically sized block
            if (tailDynamicBlock == dynamicBlocks.size()) {
                dynamicBlocks.emplace_back();
            }
            DynamicBlockType& dynamicBlock = dynamicBlocks[tailDynamicBlock++];
            return ArrayView<T>(dynamicBlock.template reallocate<T>(count), count);
        }

        // Suballocate from the static sized blocks
        std::size_t padding = tailOffset % AlignSize == 0 ? 0 : AlignSize - (tailOffset % AlignSize);

        // Switch to the next block
        if (tailOffset + padding + requiredSize > BlockSize) {
            tailBlock++;
            tailOffset = 0;
            padding = 0;
        }

        if (tailBlock == blocks.size()) {
            blocks.push_back(std::make_unique<BlockType>());
        }

        // Allocate the memory, padded to alignment
        tailOffset += padding;
        T* ptr = reinterpret_cast<T*>(reinterpret_cast<std::byte*>(blocks[tailBlock].get()) + tailOffset);
        tailOffset += requiredSize;

        return ArrayView<T>(ptr, count);
    }

    // Helper that copies the given array to the allocator and returns a view to it
    template <typename T, typename TSrc>
    ArrayView<T> allocate(ArrayParameter<const TSrc> data) {
        if (data.empty())
            return {};

        ArrayView<T> copyView = allocate<T>(data.size());

        T* ptr = copyView.data();
        for (auto& element : data) {
            new (ptr++) T{ element };
        }

        return copyView;
    }

    template <typename T, typename TSrc>
    ArrayView<T> allocate(ArrayView<TSrc> data) {
        return allocate<T>(ArrayParameter<const TSrc>(data));
    }

    // Makes the allocator start anew, overwriting the previously allocated memory
    void clear() {
        tailBlock = 0;
        tailOffset = 0;
        tailDynamicBlock = 0;
    }

private:
    // Static sized block storage type
    using BlockType = typename std::aligned_storage<BlockSize, AlignSize>::type;

    // Dynamic sized block storage type
    class DynamicBlockType {
    public:
        // Allocate and return contiguous memory for count number of T sized objects
        template <typename T = std::byte>
        T* reallocate(std::size_t count) {
            std::size_t unitCount = roundUpToMultiple(count * sizeof(T), sizeof(UnitType)) / sizeof(UnitType);
            if (data.size() < unitCount) {
                data = std::vector<UnitType>(unitCount);
            }
            return reinterpret_cast<T*>(data.data());
        }

        template <typename T = std::byte>
        T* get() {
            return reinterpret_cast<T*>(data.data());
        }

    private:
        using UnitType = typename std::aligned_storage<AlignSize, AlignSize>::type;

        std::vector<UnitType> data;
    };

    // Static sized block storage for most small data
    std::deque<std::unique_ptr<BlockType>> blocks;

    // Dynamic sized block storage for data that needs to be contiguous but won't fit in a single block
    std::deque<DynamicBlockType> dynamicBlocks;

    // The index of the tail block
    std::size_t tailBlock = 0;

    // The offset after the last allocation in the tail block
    std::size_t tailOffset = 0;

    // The index of the tail dynamic block
    std::size_t tailDynamicBlock = 0;
};

}
