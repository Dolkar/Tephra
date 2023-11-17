#include "tests_common.hpp"

std::vector<uint8_t> generateExampleImageData(uint32_t size, uint8_t blueValue = 0, uint8_t alphaValue = 255) {
    // Generate "UV map" RGBA8 image with colors
    // (0, 0, B, A) ... (1, 0, B, A)
    //    ...           ...
    // (0, 1, B, A) ... (1, 1, B, A)
    // where the rest of the pixels are interpolated between
    std::vector<uint8_t> image;
    image.resize(size * size * 4);
    if (size <= 1)
        return image;

    float d = 255.0f / static_cast<float>(size - 1);

    for (uint32_t i = 0; i < size; i++) {
        for (uint32_t j = 0; j < size; j++) {
            uint32_t index = (i * size + j) * 4;
            image[index + 0] = static_cast<uint32_t>(round(j * d));
            image[index + 1] = static_cast<uint32_t>(round(i * d));
            image[index + 2] = blueValue;
            image[index + 3] = alphaValue;
        }
    }

    return image;
}

// Compares the expected size of a 2D image with no mipmaps to the actual size, barring various alignment constraints
void testExpected2DImageSize(tp::Format format, uint32_t dimSize, uint32_t count, uint64_t actualSize) {
    uint64_t expectedSize = dimSize * dimSize * count * tp::getFormatClassProperties(format).texelBlockBytes;
    Assert::AreEqual(roundUpToMultiple(expectedSize, 512ull), roundUpToMultiple(actualSize, 512ull));
}

namespace TephraIntegrationTests {

// Tests to verify the creation, memory allocation and use of images and image views,
// both as persistent and job-local resources.
TEST_CLASS(ImageTests) {
public:
    TEST_CLASS_INITIALIZE(Initialize) {
        ctx.initialize(false);
    }

    TEST_CLASS_CLEANUP(Cleanup) {
        ctx.cleanup();
    }

    TEST_METHOD_CLEANUP(TestCleanup) {
        ctx.resetJobResourcePools();
    }

    TEST_METHOD(ImageCreation) {
        tp::Format format = tp::Format::COL32_R8G8B8A8_SRGB;
        tp::MemoryLocation usedLocation;
        for (uint32_t dimSize = 1 << 0; dimSize <= 1 << 12; dimSize *= 2) {
            auto setup = tp::ImageSetup(
                tp::ImageType::Image2D, tp::ImageUsage::SampledImage, format, { dimSize, dimSize, 1 });

            std::string imageName = "TestImage[" + std::to_string(dimSize) + "]";
            tp::OwningPtr<tp::Image> image = ctx.device->allocateImage(setup, imageName.c_str());
            Assert::IsFalse(image->vkGetImageHandle().isNull());

            Assert::AreEqual(dimSize, image->getExtent().width);
            Assert::AreEqual(dimSize, image->getExtent().height);

            // Check if the actual allocation size is approximately the same as well, barring various alignment
            // constraints
            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(
                ctx.device->vmaGetAllocatorHandle(), image->vmaGetMemoryAllocationHandle(), &allocInfo);
            testExpected2DImageSize(format, dimSize, 1, allocInfo.size);
            usedLocation = image->getMemoryLocation();
        }

        // Check for a leak
        uint32_t usedHeapIndex = ctx.physicalDevice->getMemoryLocationInfo(usedLocation).memoryHeapIndex;
        Assert::AreEqual(0ull, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);
    }

    TEST_METHOD(ImageViews) {
        static const unsigned seed = 123456;
        static const uint32_t arrayCount = 1 << 10;
        static const uint32_t viewArrayCount = 1 << 4;
        static const uint32_t arrayLevelSize = 1 << 10;
        static const int randomIters = 100;

        // First create a reference array
        std::vector<uint8_t> reference;
        reference.resize(arrayCount * arrayLevelSize);
        uint8_t* refPtr = reference.data();

        memset(refPtr, 0x00, reference.size());

        ctx.rand32.seed(seed);
        for (int i = 0; i < randomIters; i++) {
            uint32_t viewOffset = ctx.rand32() % (arrayCount - viewArrayCount);
            uint8_t writeValue = ctx.rand32() & 0xff;
            memset(refPtr + viewOffset * arrayLevelSize, writeValue, viewArrayCount * arrayLevelSize);
        }

        // Then recreate it through Tephra image views
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image1D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            tp::Format::COL8_R8_UINT,
            { arrayLevelSize, 1, 1 },
            1,
            arrayCount);

        tp::OwningPtr<tp::Image> image = ctx.device->allocateImage(imageSetup, "TestImage");

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        job.cmdClearImage(*image, tp::ClearValue::ColorUInt(0, 0, 0, 0));

        auto stagingSetup = tp::BufferSetup(
            viewArrayCount * arrayLevelSize * randomIters,
            tp::BufferUsage::HostMapped | tp::BufferUsage::ImageTransfer);
        tp::BufferView stagingBuffer = job.allocatePreinitializedBuffer(stagingSetup, tp::MemoryPreference::Host);

        ctx.rand32.seed(seed);
        for (int i = 0; i < randomIters; i++) {
            uint32_t viewOffset = ctx.rand32() % (arrayCount - viewArrayCount);
            uint8_t writeValue = ctx.rand32() & 0xff;

            // Data in staging buffer is laid out linearly, copy it randomly into the image layers
            auto viewSetup = tp::ImageViewSetup(
                tp::ImageViewType::View1D, { tp::ImageAspect::Color, 0u, ~0u, viewOffset, viewArrayCount });
            tp::ImageView imageView = image->createView(viewSetup);

            auto copyRegion = tp::BufferImageCopyRegion(
                i * viewArrayCount * arrayLevelSize,
                imageView.getWholeRange().pickMipLevel(0),
                {},
                imageView.getExtent());
            job.cmdCopyBufferToImage(stagingBuffer, imageView, { copyRegion });
        }

        auto readbackSetup = tp::BufferSetup(
            arrayCount * arrayLevelSize, tp::BufferUsage::HostMapped | tp::BufferUsage::ImageTransfer);
        tp::OwningPtr<tp::Buffer> readbackBuffer = ctx.device->allocateBuffer(
            readbackSetup, tp::MemoryPreference::ReadbackStream);

        auto copyRegion = tp::BufferImageCopyRegion(0, image->getWholeRange().pickMipLevel(0), {}, image->getExtent());
        job.cmdCopyImageToBuffer(*image, *readbackBuffer, { copyRegion });
        job.cmdExportResource(*readbackBuffer, tp::ReadAccess::Host);

        tp::JobSemaphore semaphore = ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));

        // Write data to the staging buffer
        tp::HostMappedMemory stagingMemory = stagingBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
        uint8_t* byteBufferPtr = stagingMemory.getPtr<uint8_t>();

        ctx.rand32.seed(seed);
        for (int i = 0; i < randomIters; i++) {
            uint32_t viewOffset = ctx.rand32() % (arrayCount - viewArrayCount);
            uint8_t writeValue = ctx.rand32() & 0xff;

            memset(byteBufferPtr + i * viewArrayCount * arrayLevelSize, writeValue, viewArrayCount * arrayLevelSize);
        }

        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        ctx.device->waitForJobSemaphores({ semaphore });

        // Check equivalence
        tp::HostMappedMemory readbackMemory = readbackBuffer->mapForHostAccess(tp::MemoryAccess::ReadOnly);
        byteBufferPtr = readbackMemory.getPtr<uint8_t>();

        bool isEqual = std::equal(
            refPtr, refPtr + arrayCount * arrayLevelSize, byteBufferPtr, byteBufferPtr + arrayCount * arrayLevelSize);
        Assert::IsTrue(isEqual);
    }

    TEST_METHOD(JobLocalNoOverlap) {
        tp::Format format = tp::Format::COL32_R8G8B8A8_SRGB;
        uint32_t dimSize = 1024;

        // Test whether 4 images with the same setups and non-overlapping execution can be allocated
        // to use the space of only 2
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            format,
            { dimSize, dimSize, 1 });
        tp::ImageView imageA = job.allocateLocalImage(imageSetup);
        tp::ImageView imageB = job.allocateLocalImage(imageSetup);
        tp::ImageView imageC = job.allocateLocalImage(imageSetup);
        tp::ImageView imageD = job.allocateLocalImage(imageSetup);

        auto copyRegion = tp::ImageCopyRegion(
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getExtent());
        job.cmdCopyImage(imageA, imageB, { copyRegion });
        job.cmdCopyImage(imageB, imageC, { copyRegion });
        job.cmdCopyImage(imageC, imageD, { copyRegion });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        testExpected2DImageSize(
            format, dimSize, 4, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageRequestedBytes));
        testExpected2DImageSize(
            format, dimSize, 2, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageCommittedBytes));
    }

    TEST_METHOD(JobLocalWithOverlap) {
        tp::Format format = tp::Format::COL32_R8G8B8A8_SRGB;
        uint32_t dimSize = 1024;

        // Test whether 3 images with the same setups and overlapping execution are not being aliased
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            format,
            { dimSize, dimSize, 1 });
        tp::ImageView imageA = job.allocateLocalImage(imageSetup);
        tp::ImageView imageB = job.allocateLocalImage(imageSetup);
        tp::ImageView imageC = job.allocateLocalImage(imageSetup);

        auto copyRegion = tp::ImageCopyRegion(
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getExtent());
        job.cmdCopyImage(imageA, imageB, { copyRegion });
        job.cmdCopyImage(imageB, imageC, { copyRegion });
        job.cmdCopyImage(imageC, imageA, { copyRegion });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        testExpected2DImageSize(
            format, dimSize, 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageRequestedBytes));
        testExpected2DImageSize(
            format, dimSize, 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageCommittedBytes));
    }

    TEST_METHOD(JobLocalUnused) {
        tp::Format format = tp::Format::COL32_R8G8B8A8_SRGB;
        uint32_t dimSize = 1024;

        // Test whether 3 images with the same setups and overlapping execution are not being aliased
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            format,
            { dimSize, dimSize, 1 });
        tp::ImageView imageA = job.allocateLocalImage(imageSetup);
        tp::ImageView imageB = job.allocateLocalImage(imageSetup);
        job.allocateLocalImage(imageSetup);

        auto copyRegion = tp::ImageCopyRegion(
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getWholeRange().pickMipLevel(0),
            { 0, 0, 0 },
            imageA.getExtent());
        job.cmdCopyImage(imageA, imageB, { copyRegion });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        testExpected2DImageSize(
            format, dimSize, 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageRequestedBytes));
        testExpected2DImageSize(
            format, dimSize, 2, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageCommittedBytes));
    }

    TEST_METHOD(JobLocalOptimalAllocation) {
        tp::Format format = tp::Format::COL32_R8G8B8A8_SRGB;
        uint32_t dimSize = 1024;

        // Test whether 4 images with different array counts, but same setups and non-overlapping execution
        // can be aliased optimally, rather than what a naive algorithm would do
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            format,
            { dimSize, dimSize, 1 });

        imageSetup.arrayLayerCount = 1;
        tp::ImageView imageA = job.allocateLocalImage(imageSetup);
        imageSetup.arrayLayerCount = 4;
        tp::ImageView imageB = job.allocateLocalImage(imageSetup);
        imageSetup.arrayLayerCount = 2;
        tp::ImageView imageC = job.allocateLocalImage(imageSetup);
        imageSetup.arrayLayerCount = 2;
        tp::ImageView imageD = job.allocateLocalImage(imageSetup);

        // To make sure this image appears first to defeat a naive algorithm
        job.cmdClearImage(imageA, tp::ClearValue::ColorFloat(1.0f, 0.0f, 1.0f, 0.0f), { imageA.getWholeRange() });

        auto firstLayerCopyRegion = tp::ImageCopyRegion(
            imageA.getWholeRange().pickMipLevel(0).pickLayer(0),
            { 0, 0, 0 },
            imageA.getWholeRange().pickMipLevel(0).pickLayer(0),
            { 0, 0, 0 },
            imageA.getExtent());
        job.cmdCopyImage(imageA, imageB, { firstLayerCopyRegion });
        // The largest concurrent use happens here, from the 4x image to 2x
        job.cmdCopyImage(imageB, imageC, { firstLayerCopyRegion });
        job.cmdCopyImage(imageC, imageD, { firstLayerCopyRegion });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        testExpected2DImageSize(
            format, dimSize, 9, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageRequestedBytes));
        testExpected2DImageSize(
            format, dimSize, 6, ctx.getLastStatistic(tp::StatisticEventType::JobLocalImageCommittedBytes));
    }

    TEST_METHOD(ImageLayouts) {
        // Test that barriers with image layout transitions are inserted where expected
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            tp::Format::COL32_R8G8B8A8_SRGB,
            { 1024, 1024, 1 },
            1,
            4 // arrayCount
        );

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();

        tp::ImageView image = job.allocateLocalImage(imageSetup);
        tp::ImageView layer0 = image.createView(
            tp::ImageViewSetup(tp::ImageViewType::View2D, { tp::ImageAspect::Color, 0, ~0u, 0, 1 }));
        tp::ImageView layer1 = image.createView(
            tp::ImageViewSetup(tp::ImageViewType::View2D, { tp::ImageAspect::Color, 0, ~0u, 1, 1 }));
        tp::ImageView layer2 = image.createView(
            tp::ImageViewSetup(tp::ImageViewType::View2D, { tp::ImageAspect::Color, 0, ~0u, 2, 1 }));
        tp::ImageView layer3 = image.createView(
            tp::ImageViewSetup(tp::ImageViewType::View2D, { tp::ImageAspect::Color, 0, ~0u, 3, 1 }));

        // Barrier 1 expected here, transition from UNDEFINED to TRANSFER_DST_OPTIMAL
        job.cmdClearImage(image, tp::ClearValue::ColorFloat(1.0f, 0.0f, 0.0f, 0.0f));

        auto copyRegion = tp::ImageCopyRegion(
            layer0.getWholeRange().pickMipLevel(0), {}, layer0.getWholeRange().pickMipLevel(0), {}, layer0.getExtent());

        // Barrier 2 expected here, memory barrier + transition to TRANSFER_SRC_OPTIMAL
        job.cmdCopyImage(layer0, layer1, { copyRegion });

        // No barrier here for layer 0, already in correct layout, layer2 memory barrier can extend previous barriers
        job.cmdCopyImage(layer0, layer2, { copyRegion });

        // Discard sets image layout to undefined, no barrier expected for this
        job.cmdDiscardContents(layer0);

        // Barrier 3
        job.cmdCopyImage(layer3, layer0, { copyRegion });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(
            static_cast<uint64_t>(3), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(6), ctx.getLastStatistic(tp::StatisticEventType::JobImageMemoryBarriersInserted));
    }

    TEST_METHOD(ImageMipmapCreation) {
        static const uint32_t imageMipLevelCount = 9;
        static const uint32_t imageSize = 1 << (imageMipLevelCount - 1);
        static const tp::Format format = tp::Format::COL32_R8G8B8A8_UNORM;

        // Allocate resources
        // We will read back the last mip level
        auto readbackBufferSetup = tp::BufferSetup(
            1 * 1 * 4, tp::BufferUsage::HostMapped | tp::BufferUsage::ImageTransfer);
        tp::OwningPtr<tp::Buffer> readbackBuffer = ctx.device->allocateBuffer(
            readbackBufferSetup, tp::MemoryPreference::ReadbackStream);

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto uploadBufferSetup = tp::BufferSetup(
            imageSize * imageSize * 4, tp::BufferUsage::HostMapped | tp::BufferUsage::ImageTransfer);
        tp::BufferView uploadBuffer = job.allocatePreinitializedBuffer(
            uploadBufferSetup, tp::MemoryPreference::UploadStream);

        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            format,
            { imageSize, imageSize, 1 },
            imageMipLevelCount);
        tp::ImageView image = job.allocateLocalImage(imageSetup);
        tp::ImageSubresourceRange imageRange = image.getWholeRange();

        // Upload the first mip level data
        auto copyRegion = tp::BufferImageCopyRegion(0, imageRange.pickMipLevel(0), { 0, 0, 0 }, image.getExtent());
        job.cmdCopyBufferToImage(uploadBuffer, image, { copyRegion });

        // Create the mip level chain
        for (uint32_t targetMip = 1; targetMip < imageMipLevelCount; targetMip++) {
            uint32_t sourceMip = targetMip - 1;
            auto blitRegion = tp::ImageBlitRegion(
                imageRange.pickMipLevel(sourceMip),
                { 0, 0, 0 },
                image.getExtent(sourceMip),
                imageRange.pickMipLevel(targetMip),
                { 0, 0, 0 },
                image.getExtent(targetMip));
            job.cmdBlitImage(image, image, { blitRegion });
        }

        Assert::AreEqual(image.getExtent(imageMipLevelCount - 1).width, static_cast<uint32_t>(1));

        // Readback the last mip level data
        copyRegion = tp::BufferImageCopyRegion(
            0, imageRange.pickMipLevel(imageMipLevelCount - 1), { 0, 0, 0 }, image.getExtent(imageMipLevelCount - 1));

        job.cmdCopyImageToBuffer(image, *readbackBuffer, { copyRegion });

        job.cmdExportResource(*readbackBuffer, tp::ReadAccess::Host);

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));

        // Create and upload the first mip data
        std::vector<uint8_t> imageData = generateExampleImageData(imageSize);

        tp::HostMappedMemory uploadMemory = uploadBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
        std::copy(imageData.begin(), imageData.end(), uploadMemory.getPtr<uint8_t>());

        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        // Wait, read back the last mip and compare to the expected result
        ctx.device->waitForIdle();

        tp::HostMappedMemory readbackMemory = readbackBuffer->mapForHostAccess(tp::MemoryAccess::ReadOnly);
        const uint8_t* readbackData = readbackMemory.getPtr<uint8_t>();

        // The average can be 127 or 128, permitting rounding errors
        Assert::IsTrue(readbackData[0] == 127 || readbackData[0] == 128);
        Assert::IsTrue(readbackData[1] == 127 || readbackData[1] == 128);
        Assert::AreEqual(readbackData[2], static_cast<uint8_t>(0));
        Assert::AreEqual(readbackData[3], static_cast<uint8_t>(255));

        Assert::AreEqual(
            static_cast<uint64_t>(imageMipLevelCount + 2),
            ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(imageMipLevelCount * 2),
            ctx.getLastStatistic(tp::StatisticEventType::JobImageMemoryBarriersInserted));
    }

private:
    static TephraContext ctx;
};

TephraContext ImageTests::ctx;

}
