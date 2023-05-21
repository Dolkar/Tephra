#include "tests_common.hpp"

namespace TephraIntegrationTests {

// Tests to verify the creation, memory allocation and use of buffers and buffer views,
// both as persistent and job-local resources.
TEST_CLASS(BufferTests) {
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

    TEST_METHOD(BufferCreation) {
        tp::MemoryLocation usedLocation;
        for (uint64_t size = 1 << 0; size <= 1 << 30; size *= 2) {
            auto setup = tp::BufferSetup(size, tp::BufferUsageMask::None());
            std::string bufferName = "TestBuffer[" + std::to_string(size) + "]";
            tp::OwningPtr<tp::Buffer> buffer = ctx.device->allocateBuffer(
                setup, tp::MemoryPreference::Device, bufferName.c_str());
            Assert::IsFalse(buffer->vkGetBufferHandle().isNull());
            Assert::AreEqual(size, buffer->getSize());
            Assert::AreEqual(size, buffer->getDefaultView().getSize());

            // Check if the actual allocation size is approximately the same as well, barring various alignment
            // constraints
            VmaAllocationInfo allocInfo;
            vmaGetAllocationInfo(
                ctx.device->vmaGetAllocatorHandle(), buffer->vmaGetMemoryAllocationHandle(), &allocInfo);
            Assert::AreEqual(roundUpToMultiple(size, 256ull), roundUpToMultiple(allocInfo.size, 256ull));
            usedLocation = buffer->getMemoryLocation();
        }

        // Check for a leak
        uint32_t usedHeapIndex = ctx.physicalDevice->getMemoryLocationInfo(usedLocation).memoryHeapIndex;
        Assert::AreEqual(0ull, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);
    }

    TEST_METHOD(BufferMapping) {
        auto setup = tp::BufferSetup(1 << 20, tp::BufferUsage::HostMapped);
        tp::OwningPtr<tp::Buffer> buffer = ctx.device->allocateBuffer(setup, tp::MemoryPreference::Host, "TestBuffer");

        {
            tp::HostMappedMemory writeAccess = buffer->mapForHostAccess(tp::MemoryAccess::WriteOnly);
            Assert::IsFalse(writeAccess.isNull());
            uint8_t* byteWritePtr = writeAccess.getPtr<uint8_t*>();
            memset(byteWritePtr, 0x33, buffer->getSize());
        }

        {
            tp::HostMappedMemory readAccess = buffer->mapForHostAccess(tp::MemoryAccess::ReadOnly);
            Assert::IsFalse(readAccess.isNull());
            const uint8_t* byteReadPtr = readAccess.getPtr<const uint8_t*>();
            const uint8_t* endPtr = byteReadPtr + buffer->getSize();

            uint64_t sum = 0;
            for (; byteReadPtr < endPtr; byteReadPtr++) {
                sum += *byteReadPtr;
            }

            Assert::AreEqual(static_cast<uint64_t>(0x33 << 20), sum);
        }
    }

    TEST_METHOD(BufferViews) {
        static const unsigned seed = 123456;
        static const uint64_t arraySize = 1 << 20;
        static const int randomIters = 100;

        auto setup = tp::BufferSetup(arraySize, tp::BufferUsage::HostMapped);
        tp::OwningPtr<tp::Buffer> buffer = ctx.device->allocateBuffer(setup, tp::MemoryPreference::Host, "TestBuffer");
        // Limit view offsets to allowed alignments
        uint64_t offsetMask = ~(buffer->getRequiredViewAlignment() - 1);

        // First create a reference array
        std::vector<uint8_t> reference;
        reference.resize(arraySize);
        uint8_t* refPtr = reference.data();

        memset(refPtr, 0x00, reference.size());

        std::srand(seed);
        for (int i = 0; i < randomIters; i++) {
            uint64_t viewSize = 1 + std::rand() % arraySize;
            uint64_t viewOffset = (std::rand() % (arraySize - viewSize)) & offsetMask;
            uint8_t writeValue = std::rand() & 0xff;
            memset(refPtr + viewOffset, writeValue, viewSize);
        }

        // Then recreate it through Tephra buffer views

        tp::HostMappedMemory bufferMemory = buffer->mapForHostAccess(tp::MemoryAccess::ReadWrite);
        uint8_t* byteBufferPtr = bufferMemory.getPtr<uint8_t*>();
        memset(byteBufferPtr, 0x00, buffer->getSize());

        std::srand(seed);
        for (int i = 0; i < randomIters; i++) {
            uint64_t viewSize = 1 + std::rand() % arraySize;
            uint64_t viewOffset = (std::rand() % (arraySize - viewSize)) & offsetMask;
            uint8_t writeValue = std::rand() & 0xff;

            tp::BufferView bufView = buffer->getView(viewOffset, viewSize);
            Assert::AreEqual(viewSize, bufView.getSize());

            tp::HostMappedMemory writeAccess = bufView.mapForHostAccess(tp::MemoryAccess::WriteOnly);
            uint8_t* byteWritePtr = writeAccess.getPtr<uint8_t*>();
            memset(byteWritePtr, writeValue, bufView.getSize());
        }

        // Check equivalence
        bool isEqual = std::equal(refPtr, refPtr + arraySize, byteBufferPtr, byteBufferPtr + arraySize);
        Assert::IsTrue(isEqual);
    }

    // TODO: Test formatted buffer views with compute shaders

    TEST_METHOD(JobLocalNoOverlap) {
        static const uint64_t blockSize = 1 << 20;

        // Test whether 4 buffers with the same setups and non-overlapping execution can be allocated
        // to use the space of only 2
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto bufferSetup = tp::BufferSetup(blockSize, tp::BufferUsageMask::None());
        tp::BufferView bufferA = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferB = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferC = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferD = job.allocateLocalBuffer(bufferSetup);

        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion(0, 0, blockSize) });
        job.cmdCopyBuffer(bufferB, bufferC, { tp::BufferCopyRegion(0, 0, blockSize) });
        job.cmdCopyBuffer(bufferC, bufferD, { tp::BufferCopyRegion(0, 0, blockSize) });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(blockSize * 4, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferRequestedBytes));
        Assert::AreEqual(blockSize * 2, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferCommittedBytes));
    }

    TEST_METHOD(JobLocalWithOverlap) {
        static const uint64_t blockSize = 1 << 20;

        // Test whether 3 buffers with the same setups and overlapping execution are not being aliased
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto bufferSetup = tp::BufferSetup(blockSize, tp::BufferUsageMask::None());
        tp::BufferView bufferA = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferB = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferC = job.allocateLocalBuffer(bufferSetup);

        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion(0, 0, blockSize) });
        job.cmdCopyBuffer(bufferB, bufferC, { tp::BufferCopyRegion(0, 0, blockSize) });
        job.cmdCopyBuffer(bufferC, bufferA, { tp::BufferCopyRegion(0, 0, blockSize) });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(blockSize * 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferRequestedBytes));
        Assert::AreEqual(blockSize * 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferCommittedBytes));
    }

    TEST_METHOD(JobLocalUnused) {
        static const uint64_t blockSize = 1 << 20;

        // Test whether an unused buffer doesn't throw an error or get allocated
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        auto bufferSetup = tp::BufferSetup(blockSize, tp::BufferUsageMask::None());
        tp::BufferView bufferA = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferB = job.allocateLocalBuffer(bufferSetup);
        job.allocateLocalBuffer(bufferSetup);

        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion(0, 0, blockSize) });

        tp::JobSemaphore semaphore = ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        ctx.device->waitForJobSemaphores({ semaphore });

        Assert::AreEqual(blockSize * 3, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferRequestedBytes));
        Assert::AreEqual(blockSize * 2, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferCommittedBytes));
    }

    TEST_METHOD(JobLocalOptimalAllocation) {
        static const uint64_t blockSize = 1 << 20;

        // Test whether 4 buffers with different sizes, but same setups and non-overlapping execution
        // can be aliased optimally, rather than what a naive algorithm would do
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        tp::BufferUsageMask usageMask = tp::BufferUsageMask::None();

        tp::BufferView bufferA = job.allocateLocalBuffer({ blockSize * 1, usageMask });
        tp::BufferView bufferB = job.allocateLocalBuffer({ blockSize * 4, usageMask });
        tp::BufferView bufferC = job.allocateLocalBuffer({ blockSize * 2, usageMask });
        tp::BufferView bufferD = job.allocateLocalBuffer({ blockSize * 2, usageMask });

        // To make sure this buffer appears first to defeat a naive algorithm
        job.cmdFillBuffer(bufferA, 123456);
        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion(0, 0, blockSize) });
        // The largest concurrent use happens here, from the 4x buffer to 2x
        job.cmdCopyBuffer(bufferB, bufferC, { tp::BufferCopyRegion(0, 0, blockSize) });
        job.cmdCopyBuffer(bufferC, bufferD, { tp::BufferCopyRegion(0, 0, blockSize) });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(blockSize * 9, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferRequestedBytes));
        Assert::AreEqual(blockSize * 6, ctx.getLastStatistic(tp::StatisticEventType::JobLocalBufferCommittedBytes));
    }

    TEST_METHOD(PreinitializedWithWait) {
        static const uint64_t bufferSize = 1 << 20;
        tp::MemoryLocation usedLocation;
        tp::JobSemaphore waitSemaphore;

        // Test whether two jobs with two preinitialized buffers each with a wait in between alias properly
        auto bufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::HostMapped);
        {
            tp::Job job = ctx.noOverallocateCtx.jobResourcePool->createJob();
            tp::BufferView buffer = job.allocatePreinitializedBuffer(bufferSetup, tp::MemoryPreference::Host);

            job.cmdFillBuffer(buffer, 123456);

            waitSemaphore = ctx.device->enqueueJob(ctx.noOverallocateCtx.queue, std::move(job));
            Assert::IsFalse(buffer.mapForHostAccess(tp::MemoryAccess::ReadOnly).isNull());
            ctx.device->submitQueuedJobs(ctx.noOverallocateCtx.queue);

            usedLocation = buffer.getMemoryLocation();
        }

        Assert::AreEqual(bufferSize, ctx.getLastStatistic(tp::StatisticEventType::JobPreinitBufferRequestedBytes));
        uint32_t usedHeapIndex = ctx.physicalDevice->getMemoryLocationInfo(usedLocation).memoryHeapIndex;
        Assert::AreEqual(bufferSize, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);

        ctx.device->waitForJobSemaphores({ waitSemaphore });
        {
            tp::Job job = ctx.noOverallocateCtx.jobResourcePool->createJob();
            tp::BufferView buffer = job.allocatePreinitializedBuffer(bufferSetup, tp::MemoryPreference::Host);

            job.cmdFillBuffer(buffer, 123456);

            waitSemaphore = ctx.device->enqueueJob(ctx.noOverallocateCtx.queue, std::move(job));
            Assert::IsFalse(buffer.mapForHostAccess(tp::MemoryAccess::ReadOnly).isNull());
            ctx.device->submitQueuedJobs(ctx.noOverallocateCtx.queue);
        }

        Assert::AreEqual(bufferSize, ctx.getLastStatistic(tp::StatisticEventType::JobPreinitBufferRequestedBytes));
        Assert::AreEqual(bufferSize, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);

        // Wait and trim should free both
        ctx.device->waitForJobSemaphores({ waitSemaphore });
        uint64_t trimmedSize = ctx.noOverallocateCtx.jobResourcePool->trim();
        Assert::AreEqual(bufferSize, trimmedSize);
        Assert::AreEqual(0ull, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);
    }

    TEST_METHOD(PreinitializedNoWait) {
        static const uint64_t bufferSize = 1 << 20;
        tp::MemoryLocation usedLocation;

        // Test whether two jobs with two preinitialized buffers each without a wait in between (submitted at once)
        // do not alias and are both available.
        auto bufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::HostMapped);

        tp::Job job1 = ctx.noOverallocateCtx.jobResourcePool->createJob();
        tp::BufferView bufferA = job1.allocatePreinitializedBuffer(bufferSetup, tp::MemoryPreference::Host);
        usedLocation = bufferA.getMemoryLocation();

        job1.cmdFillBuffer(bufferA, 123456);

        ctx.device->enqueueJob(ctx.noOverallocateCtx.queue, std::move(job1));
        Assert::AreEqual(bufferSize, ctx.getLastStatistic(tp::StatisticEventType::JobPreinitBufferRequestedBytes));
        uint32_t usedHeapIndex = ctx.physicalDevice->getMemoryLocationInfo(usedLocation).memoryHeapIndex;
        Assert::AreEqual(bufferSize, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);

        tp::Job job2 = ctx.noOverallocateCtx.jobResourcePool->createJob();
        tp::BufferView bufferB = job2.allocatePreinitializedBuffer(bufferSetup, tp::MemoryPreference::Host);
        Assert::IsTrue(usedLocation == bufferB.getMemoryLocation());

        job2.cmdFillBuffer(bufferB, 123456);

        ctx.device->enqueueJob(ctx.noOverallocateCtx.queue, std::move(job2));
        Assert::AreEqual(bufferSize, ctx.getLastStatistic(tp::StatisticEventType::JobPreinitBufferRequestedBytes));
        Assert::AreEqual(bufferSize * 2, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);

        ctx.device->submitQueuedJobs(ctx.noOverallocateCtx.queue);

        tp::HostMappedMemory memA = bufferA.mapForHostAccess(tp::MemoryAccess::ReadOnly);
        tp::HostMappedMemory memB = bufferB.mapForHostAccess(tp::MemoryAccess::ReadOnly);

        Assert::IsFalse(memA.isNull());
        Assert::IsFalse(memB.isNull());
        Assert::AreNotEqual(memA.getPtr<uint8_t*>(), memB.getPtr<uint8_t*>());

        // No trim expected
        uint64_t trimmedSize = ctx.noOverallocateCtx.jobResourcePool->trim();
        Assert::AreEqual(0ull, trimmedSize);
        Assert::AreEqual(bufferSize * 2, ctx.device->getMemoryHeapStatistics(usedHeapIndex).allocationBytes);
    }

private:
    static TephraContext ctx;
};

TephraContext BufferTests::ctx;

}
