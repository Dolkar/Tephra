#pragma once

#include "accesses.hpp"
#include "local_buffers.hpp"
#include "local_images.hpp"
#include "local_descriptor_sets.hpp"
#include "compute_pass.hpp"
#include "render_pass.hpp"
#include "../utils/data_block_allocator.hpp"
#include "../common_impl.hpp"
#include <tephra/job.hpp>

namespace tp {

enum class JobCommandTypes {
    ExportBuffer,
    ExportImage,
    DiscardImageContents,
    FillBuffer,
    UpdateBuffer,
    CopyBuffer,
    CopyBufferToImage,
    CopyImageToBuffer,
    CopyImage,
    BlitImage,
    ClearImage,
    ResolveImage,
    ExecuteComputePass,
    ExecuteRenderPass,
    ImportExternalBuffer,
    ImportExternalImage,
    BeginDebugLabel,
    InsertDebugLabel,
    EndDebugLabel
};

struct JobResourceStorage {
    JobLocalBuffers localBuffers;
    JobLocalImages localImages;
    JobLocalDescriptorSets localDescriptorSets;
    std::vector<CommandPool*> commandPools;

    explicit JobResourceStorage(JobResourcePoolContainer* resourcePoolImpl);

    void clear();
};

struct JobRecordStorage {
    struct CommandMetadata {
        JobCommandTypes commandType;
        CommandMetadata* nextCommand;
    };

    struct ExportBufferData {
        BufferView buffer;
        ReadAccessMask readAccessMask;
        uint32_t dstQueueFamilyIndex;

        ExportBufferData(BufferView buffer, ReadAccessMask readAccessMask, uint32_t dstQueueFamilyIndex)
            : buffer(std::move(buffer)), readAccessMask(readAccessMask), dstQueueFamilyIndex(dstQueueFamilyIndex) {}
    };

    struct ExportImageData {
        ImageView image;
        ImageAccessRange range;
        ReadAccessMask readAccessMask;
        uint32_t dstQueueFamilyIndex;

        ExportImageData(
            ImageView image,
            ImageAccessRange range,
            ReadAccessMask readAccessMask,
            uint32_t dstQueueFamilyIndex)
            : image(std::move(image)),
              range(range),
              readAccessMask(readAccessMask),
              dstQueueFamilyIndex(dstQueueFamilyIndex) {}
    };

    struct DiscardImageContentsData {
        ImageView image;
        ImageAccessRange range;

        explicit DiscardImageContentsData(ImageView image, ImageAccessRange range)
            : image(std::move(image)), range(range) {}
    };

    struct FillBufferData {
        BufferView dstBuffer;
        uint32_t value;

        FillBufferData(BufferView dstBuffer, uint32_t value) : dstBuffer(std::move(dstBuffer)), value(value) {}
    };

    struct UpdateBufferData {
        BufferView dstBuffer;
        const void* data;

        UpdateBufferData(BufferView dstBuffer, const void* data) : dstBuffer(std::move(dstBuffer)), data(data) {}
    };

    struct CopyBufferData {
        BufferView srcBuffer;
        BufferView dstBuffer;
        ArrayView<BufferCopyRegion> copyRegions;

        CopyBufferData(BufferView srcBuffer, BufferView dstBuffer, ArrayView<BufferCopyRegion> copyRegions)
            : srcBuffer(std::move(srcBuffer)), dstBuffer(std::move(dstBuffer)), copyRegions(copyRegions) {}
    };

    struct CopyImageData {
        ImageView srcImage;
        ImageView dstImage;
        ArrayView<ImageCopyRegion> copyRegions;

        CopyImageData(ImageView srcImage, ImageView dstImage, ArrayView<ImageCopyRegion> copyRegions)
            : srcImage(std::move(srcImage)), dstImage(std::move(dstImage)), copyRegions(copyRegions) {}
    };

    struct CopyBufferImageData {
        BufferView buffer;
        ImageView image;
        ArrayView<BufferImageCopyRegion> copyRegions;

        CopyBufferImageData(BufferView buffer, ImageView image, ArrayView<BufferImageCopyRegion> copyRegions)
            : buffer(std::move(buffer)), image(std::move(image)), copyRegions(copyRegions) {}
    };

    struct BlitImageData {
        ImageView srcImage;
        ImageView dstImage;
        ArrayView<ImageBlitRegion> blitRegions;
        Filter filter;

        BlitImageData(ImageView srcImage, ImageView dstImage, ArrayView<ImageBlitRegion> blitRegions, Filter filter)
            : srcImage(std::move(srcImage)), dstImage(std::move(dstImage)), blitRegions(blitRegions), filter(filter) {}
    };

    struct ClearImageData {
        ImageView dstImage;
        ClearValue value;
        ArrayView<ImageSubresourceRange> ranges;

        ClearImageData(ImageView dstImage, ClearValue value, ArrayView<ImageSubresourceRange> ranges)
            : dstImage(std::move(dstImage)), value(std::move(value)), ranges(ranges) {}
    };

    struct ExecuteComputePassData {
        ComputePass* pass;

        explicit ExecuteComputePassData(ComputePass* pass) : pass(pass) {}
    };

    struct ExecuteRenderPassData {
        RenderPass* pass;

        explicit ExecuteRenderPassData(RenderPass* pass) : pass(pass) {}
    };

    struct ImportExternalBufferData {
        BufferView buffer;
        ResourceAccess access;

        ImportExternalBufferData(BufferView buffer, ResourceAccess access)
            : buffer(std::move(buffer)), access(access) {}
    };

    struct ImportExternalImageData {
        ImageView image;
        ImageAccessRange range;
        VkImageLayout vkImageLayout;
        ResourceAccess access;

        ImportExternalImageData(
            ImageView image,
            ImageAccessRange range,
            VkImageLayout vkImageLayout,
            ResourceAccess access)
            : image(std::move(image)), range(range), vkImageLayout(vkImageLayout), access(access) {}
    };

    struct DebugLabelData {
        std::string name;
        float color[4];

        DebugLabelData(const char* labelName, ArrayParameter<const float> labelColor = {})
            : name(labelName ? labelName : std::string()) {
            if (labelColor.size() >= 4)
                memcpy(color, labelColor.data(), sizeof(float) * 4);
            else
                memset(color, 0, sizeof(float) * 4);
        }
    };

    void clear();

    uint64_t commandCount = 0;
    DataBlockAllocator<> cmdBuffer;
    CommandMetadata* firstCommandPtr = nullptr;
    CommandMetadata* lastCommandPtr = nullptr;

    std::size_t computePassCount = 0;
    std::deque<ComputePass> computePassStorage;
    std::size_t renderPassCount = 0;
    std::deque<RenderPass> renderPassStorage;
};

struct JobSemaphoreStorage {
    std::vector<JobSemaphore> jobWaits;
    JobSemaphore jobSignal;
    std::vector<ExternalSemaphore> externalWaits;
    std::vector<ExternalSemaphore> externalSignals;

    void insertWaits(
        ArrayParameter<const JobSemaphore> newJobWaits,
        ArrayParameter<const ExternalSemaphore> newExternalWaits) {
        jobWaits.insert(jobWaits.end(), newJobWaits.begin(), newJobWaits.end());
        externalWaits.insert(externalWaits.end(), newExternalWaits.begin(), newExternalWaits.end());
    }

    void clear();
};

class JobResourcePoolContainer;

struct JobData {
    JobData(JobResourcePoolContainer* resourcePoolImpl);

    void clear();

    JobResourcePoolContainer* resourcePoolImpl;

    uint64_t jobIdInPool;
    JobFlagMask flags;
    JobRecordStorage record;
    JobResourceStorage resources;
    JobSemaphoreStorage semaphores;
};

}
