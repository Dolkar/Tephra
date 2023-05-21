#include "tests_common.hpp"

namespace TephraIntegrationTests {

// Tests for Compute passes, lists and, by extension, pipelines and descriptors
TEST_CLASS(ComputeTests) {
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

    TEST_METHOD(ComputePasses) {
        static const uint64_t bufferSize = 1 << 20;
        static const uint64_t groupSize = 128;

        auto hostSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::HostMapped | tp::BufferUsage::TexelBuffer);
        tp::OwningPtr<tp::Buffer> hostBuffer = ctx.device->allocateBuffer(
            hostSetup, tp::MemoryPreference::Host, "TestBuffer");
        tp::BufferView hostBufferView = hostBuffer->createTexelView(0, bufferSize, tp::Format::COL32_R32_UINT);

        {
            tp::HostMappedMemory writeAccess = hostBufferView.mapForHostAccess(tp::MemoryAccess::WriteOnly);
            Assert::IsFalse(writeAccess.isNull());
            uint32_t* writePtr = writeAccess.getPtr<uint32_t*>();
            for (std::size_t i = 0; i < bufferSize / sizeof(uint32_t); i++) {
                *(writePtr + i) = static_cast<uint32_t>(i);
            }
        }

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();

        auto tempSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::TexelBuffer);
        tp::BufferView tempBuffer = job.allocateLocalBuffer(tempSetup);
        // Create formatted buffer view for the temporary buffer as well
        tp::BufferView tempBufferView = tempBuffer.createTexelView(0, bufferSize, tp::Format::COL32_R32_UINT);

        // Execute the first pass from host to temp device buffer
        tp::DescriptorSetView firstDescSet = job.allocateLocalDescriptorSet(
            &ctx.ioComputeDescriptorSetLayout, { hostBufferView, tempBufferView });
        std::vector<tp::BufferComputeAccess> bufferAccesses = {
            // Don't have to include the not yet acquired host buffer here
            { tempBufferView, tp::ComputeAccess::ComputeShaderStorageWrite }
        };

        job.cmdExecuteComputePass(tp::ComputePassSetup(tp::view(bufferAccesses), {}), [&](tp::ComputeList& inlineList) {
            inlineList.cmdBindComputePipeline(ctx.squareComputePipeline);
            inlineList.cmdBindDescriptorSets(ctx.ioComputePipelineLayout, { firstDescSet });
            inlineList.cmdDispatch(bufferSize / (sizeof(uint32_t) * groupSize), 1, 1);
        });

        // Execute the second pass, writing the output back to the host buffer
        tp::DescriptorSetView secondDescSet = job.allocateLocalDescriptorSet(
            &ctx.ioComputeDescriptorSetLayout, { tempBufferView, hostBufferView });
        bufferAccesses = { { tempBufferView, tp::ComputeAccess::ComputeShaderStorageRead },
                           { hostBufferView, tp::ComputeAccess::ComputeShaderStorageWrite } };
        job.cmdExecuteComputePass(tp::ComputePassSetup(tp::view(bufferAccesses), {}), [&](tp::ComputeList& inlineList) {
            // Compute pipeline is still bound from the previous inline pass
            inlineList.cmdBindDescriptorSets(ctx.ioComputePipelineLayout, { secondDescSet });
            inlineList.cmdDispatch(bufferSize / (sizeof(uint32_t) * groupSize), 1, 1);
        });

        // Export the host buffer back to host so we can read from it to confirm the results
        job.cmdExportResource(hostBufferView, tp::ReadAccess::Host);

        tp::JobSemaphore semaphore = ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        ctx.device->waitForJobSemaphores({ semaphore });
        {
            tp::HostMappedMemory readAccess = hostBufferView.mapForHostAccess(tp::MemoryAccess::ReadOnly);
            Assert::IsFalse(readAccess.isNull());
            const uint32_t* readPtr = readAccess.getPtr<const uint32_t*>();

            uint64_t error = 0;
            for (std::size_t i = 0; i < bufferSize / sizeof(uint32_t); i++) {
                uint32_t input = static_cast<uint32_t>(i);
                uint32_t expected = (input * input) * (input * input);
                int64_t diff = static_cast<int64_t>(readPtr[i]) - static_cast<int64_t>(expected);
                error += diff * diff;
            }

            Assert::AreEqual(static_cast<uint64_t>(0), error);
        }

        // Inline compute passes should record in the same command buffer
        Assert::AreEqual(
            static_cast<uint64_t>(1), ctx.getLastStatistic(tp::StatisticEventType::JobPrimaryCommandBuffersUsed));
        // Two barriers expected, one between compute passes and one between last compute pass and host export
        Assert::AreEqual(
            static_cast<uint64_t>(2), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(2), ctx.getLastStatistic(tp::StatisticEventType::JobBufferMemoryBarriersInserted));
    }

    // Same test as above, but within one compute pass, using a manual pipeline barrier and deferred compute list
    TEST_METHOD(ComputeDeferredPass) {
        static const uint64_t bufferSize = 1 << 20;
        static const uint64_t groupSize = 128;

        auto hostSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::HostMapped | tp::BufferUsage::TexelBuffer);
        tp::OwningPtr<tp::Buffer> hostBuffer = ctx.device->allocateBuffer(
            hostSetup, tp::MemoryPreference::Host, "TestBuffer");
        tp::BufferView hostBufferView = hostBuffer->createTexelView(0, bufferSize, tp::Format::COL32_R32_UINT);

        {
            tp::HostMappedMemory writeAccess = hostBufferView.mapForHostAccess(tp::MemoryAccess::WriteOnly);
            Assert::IsFalse(writeAccess.isNull());
            uint32_t* writePtr = writeAccess.getPtr<uint32_t*>();
            for (std::size_t i = 0; i < bufferSize / sizeof(uint32_t); i++) {
                *(writePtr + i) = static_cast<uint32_t>(i);
            }
        }

        tp::Job job = ctx.graphicsQueueCtx.jobResourcePool->createJob();
        tp::CommandPool* commandPool = job.createCommandPool();

        auto tempSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::TexelBuffer);
        tp::BufferView tempBuffer = job.allocateLocalBuffer(tempSetup);
        // Create formatted buffer view for the temporary buffer as well
        tp::BufferView tempBufferView = tempBuffer.createTexelView(0, bufferSize, tp::Format::COL32_R32_UINT);

        // Allocate descriptor sets for job-local resources
        tp::DescriptorSetView firstPassDescriptor = job.allocateLocalDescriptorSet(
            &ctx.ioComputeDescriptorSetLayout, { hostBufferView, tempBufferView });
        tp::DescriptorSetView secondPassDescriptor = job.allocateLocalDescriptorSet(
            &ctx.ioComputeDescriptorSetLayout, { tempBufferView, hostBufferView });

        // Only define the compute pass without inline recording any commands
        tp::BufferComputeAccess bufferAccesses[2] = {
            // Have to include both accesses since it's all done in one pass
            { tempBufferView,
              tp::ComputeAccess::ComputeShaderStorageRead | tp::ComputeAccess::ComputeShaderStorageWrite },
            { hostBufferView,
              tp::ComputeAccess::ComputeShaderStorageRead | tp::ComputeAccess::ComputeShaderStorageWrite }
        };

        tp::ComputeList computeList;
        job.cmdExecuteComputePass(tp::ComputePassSetup(tp::view(bufferAccesses), {}), tp::viewOne(computeList));

        // Export the host buffer back to host so we can read from it to confirm the results
        job.cmdExportResource(hostBufferView, tp::ReadAccess::Host);

        tp::JobSemaphore semaphore = ctx.device->enqueueJob(ctx.graphicsQueueCtx.queue, std::move(job));

        // Record compute pass commands after job has been enqueued
        computeList.beginRecording(commandPool);
        computeList.cmdBindComputePipeline(ctx.squareComputePipeline);

        // First dispatch
        computeList.cmdBindDescriptorSets(ctx.ioComputePipelineLayout, { firstPassDescriptor });
        computeList.cmdDispatch(bufferSize / (sizeof(uint32_t) * groupSize), 1, 1);

        // Insert a barrier manually, synchronizing the writes of the previous dispatch to the reads of the following
        // one
        computeList.cmdPipelineBarrier(
            { { tp::ComputeAccess::ComputeShaderStorageWrite, tp::ComputeAccess::ComputeShaderStorageRead } });

        // Second dispatch
        computeList.cmdBindDescriptorSets(ctx.ioComputePipelineLayout, { secondPassDescriptor });
        computeList.cmdDispatch(bufferSize / (sizeof(uint32_t) * groupSize), 1, 1);

        computeList.endRecording();

        // Finally submit the job
        ctx.device->submitQueuedJobs(ctx.graphicsQueueCtx.queue);

        ctx.device->waitForJobSemaphores({ semaphore });
        {
            tp::HostMappedMemory readAccess = hostBufferView.mapForHostAccess(tp::MemoryAccess::ReadOnly);
            Assert::IsFalse(readAccess.isNull());
            const uint32_t* readPtr = readAccess.getPtr<const uint32_t*>();

            uint64_t error = 0;
            for (std::size_t i = 0; i < bufferSize / sizeof(uint32_t); i++) {
                uint32_t input = static_cast<uint32_t>(i);
                uint32_t expected = (input * input) * (input * input);
                int64_t diff = static_cast<int64_t>(readPtr[i]) - static_cast<int64_t>(expected);
                error += diff * diff;
            }

            Assert::AreEqual(static_cast<uint64_t>(0), error);
        }

        // Deferred recording splits the primary command buffers
        Assert::AreEqual(
            static_cast<uint64_t>(2), ctx.getLastStatistic(tp::StatisticEventType::JobPrimaryCommandBuffersUsed));
        // Just one automatic barrier expected to synchronize against host export
        Assert::AreEqual(
            static_cast<uint64_t>(1), ctx.getLastStatistic(tp::StatisticEventType::JobPipelineBarriersInserted));
        Assert::AreEqual(
            static_cast<uint64_t>(1), ctx.getLastStatistic(tp::StatisticEventType::JobBufferMemoryBarriersInserted));
    }

private:
    static TephraContext ctx;
};

TephraContext ComputeTests::ctx;

}
