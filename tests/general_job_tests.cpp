#include "tests_common.hpp"

namespace TephraIntegrationTests {

// Tests to verify the basic functionality of jobs, like creation, submission and synchronization
TEST_CLASS(GeneralJobTests) {
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

    TEST_METHOD(EmptyJobs) {
        // Keep last semaphores in each queue
        tp::JobSemaphore semaphores[4];
        for (int i = 0; i < 10; i++) {
            semaphores[0] = ctx.device->enqueueJob(
                ctx.graphicsQueueCtx.queue, ctx.graphicsQueueCtx.jobResourcePool->createJob());
            semaphores[1] = ctx.device->enqueueJob(
                ctx.asyncCompute0Ctx.queue, ctx.asyncCompute0Ctx.jobResourcePool->createJob());
            semaphores[2] = ctx.device->enqueueJob(
                ctx.asyncCompute1Ctx.queue, ctx.asyncCompute1Ctx.jobResourcePool->createJob());
            semaphores[3] = ctx.device->enqueueJob(
                ctx.noOverallocateCtx.queue, ctx.noOverallocateCtx.jobResourcePool->createJob());
        }

        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        ctx.device->submitQueuedJobs(ctx.asyncCompute0Ctx.queue);
        ctx.device->submitQueuedJobs(ctx.asyncCompute1Ctx.queue);
        ctx.device->submitQueuedJobs(ctx.noOverallocateCtx.queue);

        ctx.device->waitForJobSemaphores(tp::view(semaphores));

        for (tp::JobSemaphore semaphore : semaphores) {
            Assert::IsTrue(ctx.device->isJobSemaphoreSignalled(semaphore));
        }
    }

    // TODO: free used test, free unused test, pool trim test

    TEST_METHOD(CleanupCallbacks) {
        static const int jobCount = 10;
        static const uint64_t copyBufferSize = 1 << 25;
        static const int copyCount = 10;

        bool finished[jobCount];

        tp::OwningPtr<tp::Buffer> srcBuffer = ctx.device->allocateBuffer(
            { copyBufferSize, tp::BufferUsageMask::None() }, tp::MemoryPreference::Device, "SrcBuffer");
        tp::OwningPtr<tp::Buffer> dstBuffer = ctx.device->allocateBuffer(
            { copyBufferSize, tp::BufferUsageMask::None() }, tp::MemoryPreference::Device, "DstBuffer");

        for (int i = 0; i < jobCount; i++) {
            tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();

            // Add some workload to make sure jobs run for a while
            for (int j = 0; j < copyCount; j++) {
                job.cmdCopyBuffer(*srcBuffer, *dstBuffer, { tp::BufferCopyRegion(0, 0, copyBufferSize) });
            }

            ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
            finished[i] = false;

            ctx.device->addCleanupCallback([&finished, i] {
                for (int j = 0; j < i; j++) {
                    Assert::IsTrue(finished[j], L"Cleanup callbacks must be issued in order");
                }
                finished[i] = true;
            });
        }

        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        bool allFinished = true;
        for (int i = 0; i < jobCount; i++) {
            allFinished &= finished[i];
        }
        if (allFinished) {
            Logger::WriteMessage("All submitted jobs have already finished without waiting.");
            return;
        }

        ctx.device->waitForIdle();
        allFinished = true;
        for (int i = 0; i < jobCount; i++) {
            allFinished &= finished[i];
        }
        Assert::IsTrue(allFinished);
    }

    TEST_METHOD(BarriersDependentOps) {
        static const uint64_t bufferSize = 1 << 20;

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();

        auto bufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsageMask::None());
        tp::BufferView bufferA = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferB = job.allocateLocalBuffer(bufferSetup);

        job.cmdFillBuffer(bufferA, 123456);
        // Test read after write memory dependency (Barrier #1, Mem barrier #1)
        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion{ 0, 0, bufferSize } });

        // Then write after read execution dependency (Barrier #2)
        job.cmdFillBuffer(bufferA, 654321);

        // And write after write + read after write memory dependencies (Barrier #3, Mem barrier #2,3)
        job.cmdCopyBuffer(bufferB, bufferA, { tp::BufferCopyRegion{ 0, 0, bufferSize } });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(
            static_cast<uint64_t>(3), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(3), ctx.getLastStatistic(tp::StatisticEventType::JobBufferMemoryBarriersInserted));
    }

    TEST_METHOD(BarriersIndependentOps) {
        static const uint64_t bufferSize = 1 << 20;

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();

        auto bufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsageMask::None());
        tp::BufferView bufferA = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferB = job.allocateLocalBuffer(bufferSetup);
        tp::BufferView bufferC = job.allocateLocalBuffer(bufferSetup);

        job.cmdFillBuffer(bufferA, 123456);
        // Test that only one read after write pipeline barrier gets inserted for multiple reads (Barrier #1)
        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion{ 0, 0, bufferSize / 2 } });
        job.cmdCopyBuffer(bufferA, bufferB, { tp::BufferCopyRegion{ bufferSize / 2, bufferSize / 2, bufferSize / 2 } });

        // Test read after read non-dependency
        job.cmdCopyBuffer(bufferA, bufferC, { tp::BufferCopyRegion{ bufferSize / 4, 0, bufferSize / 2 } });

        // Test non-overlapping regions non-dependency
        job.cmdFillBuffer(bufferC.getView(bufferSize / 2, bufferSize / 4), 654321);
        job.cmdFillBuffer(bufferC.getView(3 * bufferSize / 4, bufferSize / 4), 456789);

        // Do put write after read execution barrier here (Barrier #2)
        job.cmdFillBuffer(bufferA.getView(0, bufferSize / 2), 654321);

        // Augment the previous barrier with another write after read execution barrier without inserting a new one
        job.cmdCopyBuffer(
            bufferA, bufferA, { tp::BufferCopyRegion{ bufferSize / 2, 3 * bufferSize / 4, bufferSize / 4 } });

        // Also augment the previous barrier to synchronize a copy between two unrelated buffers
        // This will need 5 memory barriers (2 for bufferB, 3 for bufferC) since there is no mem barrier range combining
        job.cmdCopyBuffer(bufferB, bufferC, { tp::BufferCopyRegion{ 0, 0, bufferSize } });

        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        Assert::AreEqual(
            static_cast<uint64_t>(2), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(6), ctx.getLastStatistic(tp::StatisticEventType::JobBufferMemoryBarriersInserted));
    }

    TEST_METHOD(BarriersExport) {
        static const uint64_t bufferSize = 1 << 20;

        auto bufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::HostMapped);
        tp::OwningPtr<tp::Buffer> bufferA = ctx.device->allocateBuffer(
            bufferSetup, tp::MemoryPreference::Host, "TestBuffer");
        tp::OwningPtr<tp::Buffer> bufferB = ctx.device->allocateBuffer(
            bufferSetup, tp::MemoryPreference::Host, "TestBuffer");

        // Job 1 writes to the fresh buffer, no barrier expected
        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        job.cmdFillBuffer(*bufferA, 123456);
        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        Assert::AreEqual(
            static_cast<uint64_t>(0), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));

        // Job 2 does the same thing and also exports it for transfer reads. 2 Barriers expected
        job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        job.cmdFillBuffer(*bufferA, 654321);
        job.cmdExportResource(*bufferA, tp::ReadAccess::Transfer);
        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        Assert::AreEqual(
            static_cast<uint64_t>(2), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));

        // Job 3 reads from the exported resource, no barriers expected.
        job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        job.cmdCopyBuffer(*bufferA, *bufferB, { tp::BufferCopyRegion{ 0, 0, bufferSize / 2 } });
        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        Assert::AreEqual(
            static_cast<uint64_t>(0), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));

        // Job 4 writes to the exported resource, 1 barrier expected
        job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        job.cmdCopyBuffer(*bufferA, *bufferA, { tp::BufferCopyRegion{ 0, bufferSize / 2, bufferSize / 2 } });
        ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);
        Assert::AreEqual(
            static_cast<uint64_t>(1), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
    }

private:
    static TephraContext ctx;
};

TephraContext GeneralJobTests::ctx;

}
