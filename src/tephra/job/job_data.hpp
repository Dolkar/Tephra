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
        StoredBufferView buffer;
        ReadAccessMask readAccessMask;
        uint32_t dstQueueFamilyIndex;

        ExportBufferData(const BufferView& buffer, ReadAccessMask readAccessMask, uint32_t dstQueueFamilyIndex)
            : buffer(buffer), readAccessMask(readAccessMask), dstQueueFamilyIndex(dstQueueFamilyIndex) {}
    };

    struct ExportImageData {
        StoredImageView image;
        ImageAccessRange range;
        ReadAccessMask readAccessMask;
        uint32_t dstQueueFamilyIndex;

        ExportImageData(
            const ImageView& image,
            ImageAccessRange range,
            ReadAccessMask readAccessMask,
            uint32_t dstQueueFamilyIndex)
            : image(image), range(range), readAccessMask(readAccessMask), dstQueueFamilyIndex(dstQueueFamilyIndex) {}
    };

    struct DiscardImageContentsData {
        StoredImageView image;
        ImageAccessRange range;

        explicit DiscardImageContentsData(const ImageView& image, ImageAccessRange range)
            : image(image), range(range) {}
    };

    struct FillBufferData {
        StoredBufferView dstBuffer;
        uint32_t value;

        FillBufferData(const BufferView& dstBuffer, uint32_t value) : dstBuffer(dstBuffer), value(value) {}
    };

    struct UpdateBufferData {
        StoredBufferView dstBuffer;
        const void* data;

        UpdateBufferData(const BufferView& dstBuffer, const void* data) : dstBuffer(dstBuffer), data(data) {}
    };

    struct CopyBufferData {
        StoredBufferView srcBuffer;
        StoredBufferView dstBuffer;
        ArrayView<BufferCopyRegion> copyRegions;

        CopyBufferData(const BufferView& srcBuffer, const BufferView& dstBuffer, ArrayView<BufferCopyRegion> copyRegions)
            : srcBuffer(srcBuffer), dstBuffer(dstBuffer), copyRegions(copyRegions) {}
    };

    struct CopyImageData {
        StoredImageView srcImage;
        StoredImageView dstImage;
        ArrayView<ImageCopyRegion> copyRegions;

        CopyImageData(const ImageView& srcImage, const ImageView& dstImage, ArrayView<ImageCopyRegion> copyRegions)
            : srcImage(srcImage), dstImage(dstImage), copyRegions(copyRegions) {}
    };

    struct CopyBufferImageData {
        StoredBufferView buffer;
        StoredImageView image;
        ArrayView<BufferImageCopyRegion> copyRegions;

        CopyBufferImageData(
            const BufferView& buffer,
            const ImageView& image,
            ArrayView<BufferImageCopyRegion> copyRegions)
            : buffer(buffer), image(image), copyRegions(copyRegions) {}
    };

    struct BlitImageData {
        StoredImageView srcImage;
        StoredImageView dstImage;
        ArrayView<ImageBlitRegion> blitRegions;
        Filter filter;

        BlitImageData(
            const ImageView& srcImage,
            const ImageView& dstImage,
            ArrayView<ImageBlitRegion> blitRegions,
            Filter filter)
            : srcImage(srcImage), dstImage(dstImage), blitRegions(blitRegions), filter(filter) {}
    };

    struct ClearImageData {
        StoredImageView dstImage;
        ClearValue value;
        ArrayView<ImageSubresourceRange> ranges;

        ClearImageData(const ImageView& dstImage, ClearValue value, ArrayView<ImageSubresourceRange> ranges)
            : dstImage(dstImage), value(std::move(value)), ranges(ranges) {}
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
        StoredBufferView buffer;
        ResourceAccess access;

        ImportExternalBufferData(const BufferView& buffer, ResourceAccess access) : buffer(buffer), access(access) {}
    };

    struct ImportExternalImageData {
        StoredImageView image;
        ImageAccessRange range;
        VkImageLayout vkImageLayout;
        ResourceAccess access;

        ImportExternalImageData(
            const ImageView& image,
            ImageAccessRange range,
            VkImageLayout vkImageLayout,
            ResourceAccess access)
            : image(image), range(range), vkImageLayout(vkImageLayout), access(access) {}
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
