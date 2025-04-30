#pragma once

#include "accesses.hpp"
#include "local_acceleration_structures.hpp"
#include "local_buffers.hpp"
#include "local_images.hpp"
#include "local_descriptor_sets.hpp"
#include "compute_pass.hpp"
#include "render_pass.hpp"
#include "../device/query_manager.hpp"
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
    EndDebugLabel,
    WriteTimestamp,
    BuildAccelerationStructures,
    BuildAccelerationStructuresIndirect,
    CopyAccelerationStructure,
    WriteAccelerationStructureSizes,
};

struct JobResourceStorage {
    JobLocalBuffers localBuffers;
    JobLocalImages localImages;
    JobLocalAccelerationStructures localAccelerationStructures;
    JobLocalDescriptorSets localDescriptorSets;
    std::vector<CommandPool*> commandPools;
    // TODO: We need to extend the lifetime of AS builders used in this job, but DataBlockAllocator currently
    // doesn't call destructors of the type-erased blocks
    std::vector<std::shared_ptr<AccelerationStructureBuilder>> usedASBuilders;

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
        ResourceAccess access;
        uint32_t dstQueueFamilyIndex;

        ExportBufferData(const BufferView& buffer, ResourceAccess access, uint32_t dstQueueFamilyIndex)
            : buffer(buffer), access(access), dstQueueFamilyIndex(dstQueueFamilyIndex) {}
    };

    struct ExportImageData {
        StoredImageView image;
        ImageAccessRange range;
        ResourceAccess access;
        VkImageLayout vkImageLayout;
        uint32_t dstQueueFamilyIndex;

        ExportImageData(
            const ImageView& image,
            ImageAccessRange range,
            ResourceAccess access,
            VkImageLayout vkImageLayout,
            uint32_t dstQueueFamilyIndex)
            : image(image),
              range(range),
              access(access),
              vkImageLayout(vkImageLayout),
              dstQueueFamilyIndex(dstQueueFamilyIndex) {}
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
        ArrayView<const std::byte> data;

        UpdateBufferData(const BufferView& dstBuffer, ArrayView<const std::byte> data)
            : dstBuffer(dstBuffer), data(data) {}
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
        ResourceAccess access;
        VkImageLayout vkImageLayout;

        ImportExternalImageData(
            const ImageView& image,
            ImageAccessRange range,
            ResourceAccess access,
            VkImageLayout vkImageLayout)
            : image(image), range(range), access(access), vkImageLayout(vkImageLayout) {}
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

    struct WriteTimestampData {
        QueryHandle query;
        PipelineStage stage;

        WriteTimestampData(const QueryHandle& query, PipelineStage stage) : query(query), stage(stage) {}
    };

    // Shared for regular and indirect build commands
    struct BuildAccelerationStructuresData {
        struct SingleBuild {
            AccelerationStructureBuilder* builder;
            StoredAccelerationStructureBuildInfo buildInfo;
            StoredAccelerationStructureBuildIndirectInfo indirectInfo;
            StoredBufferView scratchBuffer;

            SingleBuild(
                AccelerationStructureBuilder* builder,
                StoredAccelerationStructureBuildInfo buildInfo,
                StoredAccelerationStructureBuildIndirectInfo indirectInfo,
                StoredBufferView scratchBuffer)
                : builder(builder), buildInfo(buildInfo), indirectInfo(indirectInfo), scratchBuffer(scratchBuffer) {}
        };

        ArrayView<SingleBuild> builds;

        BuildAccelerationStructuresData(ArrayView<SingleBuild> builds) : builds(builds) {}
    };

    struct CopyAccelerationStructureData {
        StoredAccelerationStructureView srcView;
        StoredAccelerationStructureView dstView;
        AccelerationStructureCopyMode mode;

        CopyAccelerationStructureData(
            const AccelerationStructureView& srcView,
            const AccelerationStructureView& dstView,
            AccelerationStructureCopyMode mode)
            : srcView(srcView), dstView(dstView), mode(mode) {}
    };

    struct WriteAccelerationStructureSizesData {
        ArrayView<QueryHandle> queries;
        ArrayView<StoredAccelerationStructureView> views;

        WriteAccelerationStructureSizesData(
            ArrayView<QueryHandle> queries,
            ArrayView<StoredAccelerationStructureView> views)
            : queries(queries), views(views) {}
    };

    void addCommand(JobRecordStorage::CommandMetadata* commandPtr);
    void addDelayedCommand(JobRecordStorage::CommandMetadata* commandPtr);

    void clear();

    uint64_t nextCommandIndex = 0;
    DataBlockAllocator<> cmdBuffer;
    CommandMetadata* firstCommandPtr = nullptr;
    CommandMetadata* lastCommandPtr = nullptr;
    CommandMetadata* firstDelayedCommandPtr = nullptr;
    CommandMetadata* lastDelayedCommandPtr = nullptr;

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
