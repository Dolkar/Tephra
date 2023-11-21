#include "tests_common.hpp"
#include "shaders/distance_transform_shared.h"

#include <numeric>
#include <tephra/utils/growable_ring_buffer.hpp>

namespace TephraIntegrationTests {

// Helper functions to generate and verify data for the distance transform shader
namespace DistanceTransformTestUtils {
    constexpr uint32_t MarkedValueCount = 3;

    uint32_t getMarkedCellIndex(uint64_t cellCount, uint32_t seed, uint32_t i) {
        // Get some pseudorandom cells to mark
        const uint32_t fibMul = 2654435769u; // 2^32 / phi
        uint32_t v = (seed * fibMul ^ i) * fibMul;
        return v % cellCount;
    }

    void prepareInputData(tp::ArrayView<DistanceValueType> inputBuffer, uint32_t seed) {
        // The input buffer data consists of 0s as "empty cells" and 1s as "marked cells" that we measure distances to.
        // For testing, make it mostly empty cells except for a few randomly distributed marked cells.
        memset(inputBuffer.data(), 0, inputBuffer.size() * sizeof(DistanceValueType));

        for (uint32_t i = 0; i < MarkedValueCount; i++) {
            inputBuffer[getMarkedCellIndex(inputBuffer.size(), seed, i)] = 1;
        }
    }

    void validateOutputData(tp::ArrayView < const DistanceValueType> outputBuffer, uint32_t seed) {
        uint32_t markedCells[MarkedValueCount];
        for (uint32_t i = 0; i < MarkedValueCount; i++) {
            markedCells[i] = getMarkedCellIndex(outputBuffer.size(), seed, i);
        }

        for (uint32_t i = 0; i < outputBuffer.size(); i++) {
            // Calculate the min distance to all known marked cells
            DistanceValueType closestDistance = MaxDistanceValue;
            for (uint32_t j = 0; j < MarkedValueCount; j++) {
                DistanceValueType markDistance = markedCells[j] - i;

                // In case of a tie, positive distance wins over negative
                if (abs(markDistance) < abs(closestDistance) ||
                    (abs(markDistance) == abs(closestDistance) && markDistance > closestDistance))
                    closestDistance = markDistance;
            }

            DistanceValueType outputDistance = outputBuffer[i];
            Assert::AreEqual(closestDistance, outputDistance);
        }
    }
};

// Performs tests of a workload that involves continuously and asynchronously streaming data to the gpu,
// processing it and streaming it back. The chosen workload is a simple distance transform, see above and the
// associated shader. This class also validates that the output is correct and checks that the allocation sizes
// of automatic pools meet expectations.
class StreamingComputeTester {
public:
    explicit StreamingComputeTester(TephraContext* ctx) : ctx(ctx) {
        // In this specialized case, we want to customize the overallocation behavior of our allocators. Since
        // the size of each buffer will vary greatly, we won't bloat the request size (requestFactor = 1.0).
        // Instead, we will double the size of the pool when needed (growFactor = 2.0) and set no min allocation
        // size for easier allocation testing.
        allocBehavior = tp::OverallocationBehavior(1.0, 2.0, 0);

        // Use job preinitialized buffers for the input buffers and tp::utils::AutoRingBuffer for the output
        // (readback) buffers. We could use the ring buffers for both, but this is preferred and also maximizes
        // coverage for the test.
        outputRingBuffer = std::make_unique<tp::utils::AutoRingBuffer>(
            ctx->device.get(),
            tp::BufferUsage::StorageBuffer | tp::BufferUsage::HostMapped,
            // This memory preference will use the device-local host-visible memory heap if available, but falls
            // back to host heap if needed. This can be better on systems with resizable BAR, but it's also a
            // useful test for the allocation fallbacks on systems without it.
            tp::MemoryPreference::ReadbackStream,
            allocBehavior);

        // Also create our own job resource pool instead of using the shared test ones so we can apply our
        // allocation behavior. This pool will service the streamed input buffers and the ping-pong buffers.
        auto jobPoolSetup = tp::JobResourcePoolSetup(ctx->graphicsQueueCtx.queue);
        jobPoolSetup.preinitBufferOverallocationBehavior = allocBehavior;
        jobResourcePool = ctx->device->createJobResourcePool(jobPoolSetup);

        // Pipeline will need just one input buffer, one output buffer and some push constants
        descriptorSetLayout = ctx->device->createDescriptorSetLayout(
            { tp::DescriptorBinding(0, tp::DescriptorType::StorageBuffer, tp::ShaderStage::Compute),
              tp::DescriptorBinding(1, tp::DescriptorType::StorageBuffer, tp::ShaderStage::Compute) });

        auto pushConstants = tp::PushConstantRange(tp::ShaderStage::Compute, 0, sizeof(PushConstantData));
        pipelineLayout = ctx->device->createPipelineLayout({ &descriptorSetLayout }, { pushConstants });

        // Load the distance transform shader and create the pipeline
        tp::ShaderModule shaderModule = loadShader(ctx->device.get(), "distance_transform.spv");
        auto pipelineSetup = tp::ComputePipelineSetup(&pipelineLayout, { &shaderModule, "main" });

        tp::Pipeline* compiledPipelines[1] = { &distanceTransformPipeline };
        ctx->device->compileComputePipelines({ &pipelineSetup }, nullptr, tp::view(compiledPipelines));
    }

    void run(uint32_t jobCount, uint32_t maxConcurrency, uint32_t maxBufferSize, uint32_t runSeed, bool validateOutput) {
        // Initialize a tester that checks if we're allocating the expected amount of memory
        auto allocTester = AllocTester(this, maxConcurrency);

        // Future result of a submitted job
        struct JobResult {
            uint32_t id;
            uint32_t seed;
            tp::JobSemaphore semaphore; // Signalled when completed
            tp::BufferView outputBuffer;
        };

        auto processResult = [this, validateOutput](const JobResult& result) {
            // Wait for the job to finish
            ctx->device->waitForJobSemaphores({ result.semaphore });

            // We can now read and validate the output buffer
            if (validateOutput) {
                tp::HostMappedMemory readAccess = result.outputBuffer.mapForHostAccess(tp::MemoryAccess::ReadOnly);
                auto dataView = readAccess.getArrayView<const DistanceValueType>();
                DistanceTransformTestUtils::validateOutputData(dataView, result.seed);
            }

            // Free anything we allocated up to that id from the auto ring buffer, which in this case is only
            // the output buffer
            outputRingBuffer->pop(result.id);
        };

        std::deque<JobResult> jobResults;
        ctx->rand32.seed(runSeed);

        for (uint32_t jobId = 0; jobId < jobCount; jobId++) {
            // Limit max concurrency by waiting on old results
            if (jobResults.size() == maxConcurrency) {
                processResult(jobResults.front());
                jobResults.pop_front();

                // Release unused memory to minimize memory usage
                outputRingBuffer->trim();
                jobResourcePool->trim();
            }

            // Choose buffer size, same for both input and output
            uint32_t elementCount = std::max(ctx->rand32() % maxBufferSize, 1u);
            uint32_t bufferSize = elementCount * sizeof(DistanceValueType);
            // And a separate seed that we will use to make and verify the workload
            uint32_t jobSeed = ctx->rand32();

            // Allocate an output buffer, annotating it with incrementing id
            tp::BufferView outputBuffer = outputRingBuffer->push(bufferSize, jobId);

            // Submit a test job writing results for this seed to the given output buffer
            auto jobSemaphore = submitTestJob(outputBuffer, elementCount, jobSeed);

            // Keep the future result of the job, so we can validate it and limit concurrency
            JobResult result = { jobId, jobSeed, jobSemaphore, outputBuffer };
            jobResults.push_back(result);

            // Test we didn't allocate more than expected
            allocTester.checkBuffer(bufferSize);
        }

        // Process the rest
        while (!jobResults.empty()) {
            processResult(jobResults.front());
            jobResults.pop_front();
        }

        // Assuming all work has completed, this should release all memory
        outputRingBuffer->trim();
        jobResourcePool->trim();
        Assert::AreEqual(0ull, outputRingBuffer->getAllocatedSize());
        Assert::AreEqual(0ull, jobResourcePool->getStatistics().getTotalAllocationBytes());
    }

private:
    // Records and submits the job for one test run of the distance transform algorithm
    tp::JobSemaphore submitTestJob(const tp::BufferView& outputBuffer, uint32_t elementCount, uint32_t seed) {
        tp::Job job = jobResourcePool->createJob();

        // Allocate an input buffer of the same size as the output buffer, but use UploadStream memory pref,
        // for similar reasons as why the output buffer uses ReadbackStream
        uint32_t bufferSize = static_cast<uint32_t>(outputBuffer.getSize());
        auto inputBufferSetup = tp::BufferSetup(
            bufferSize, tp::BufferUsage::StorageBuffer | tp::BufferUsage::HostMapped);
        auto inputBuffer = job.allocatePreinitializedBuffer(inputBufferSetup, tp::MemoryPreference::UploadStream);

        // Write our test data to it
        {
            tp::HostMappedMemory writeAccess = inputBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
            DistanceTransformTestUtils::prepareInputData(writeAccess.getArrayView<DistanceValueType>(), seed);
        }

        // We will also need in the general case two device-local buffers to ping-pong between. It's not too
        // tricky to re-use the input/output buffers if they end up in device-local memory, so let's do it:
        auto pingPongBufferSetup = tp::BufferSetup(bufferSize, tp::BufferUsage::StorageBuffer);
        tp::BufferView pingBuffer;
        if (inputBuffer.getMemoryLocation() == tp::MemoryLocation::DeviceLocalHostVisible ||
            inputBuffer.getMemoryLocation() == tp::MemoryLocation::DeviceLocalHostCached)
            pingBuffer = inputBuffer;
        else
            pingBuffer = job.allocateLocalBuffer(pingPongBufferSetup);

        tp::BufferView pongBuffer;
        if (outputBuffer.getMemoryLocation() == tp::MemoryLocation::DeviceLocalHostVisible ||
            outputBuffer.getMemoryLocation() == tp::MemoryLocation::DeviceLocalHostCached)
            pongBuffer = outputBuffer;
        else
            pongBuffer = job.allocateLocalBuffer(pingPongBufferSetup);

        // Ensure ping buffer has the input data
        if (pingBuffer != inputBuffer) {
            job.cmdCopyBuffer(inputBuffer, pingBuffer, { { 0, 0, bufferSize } });
        }

        // Pre-allocate the needed descriptor sets
        tp::DescriptorSetView pingSet = job.allocateLocalDescriptorSet(
            &descriptorSetLayout, { pingBuffer, pongBuffer });
        tp::DescriptorSetView pongSet = job.allocateLocalDescriptorSet(
            &descriptorSetLayout, { pongBuffer, pingBuffer });

        // Prepare matching accesses which will be used to automatically insert barriers
        std::array<tp::BufferComputeAccess, 2> pingAccesses = {
            { { pingBuffer, tp::ComputeAccess::ComputeShaderStorageRead },
              { pongBuffer, tp::ComputeAccess::ComputeShaderStorageWrite } }
        };
        std::array<tp::BufferComputeAccess, 2> pongAccesses = {
            { { pingBuffer, tp::ComputeAccess::ComputeShaderStorageWrite },
              { pongBuffer, tp::ComputeAccess::ComputeShaderStorageRead } }
        };

        // The actual JFA algorithm - running ping-pong dispatches with gradually decreasing step size
        int32_t stepSize = static_cast<int32_t>(elementCount);
        int passNumber = 0;
        while (stepSize > 1) {
            // Decrease step size by the shrink factor, rounded up, on every pass
            stepSize = roundUpToMultiple(stepSize, JumpShrinkFactor) / JumpShrinkFactor;
            PushConstantData pushConstants = { static_cast<int32_t>(elementCount), stepSize, passNumber++ };

            // Use inline command recording for simplicity
            job.cmdExecuteComputePass(
                { tp::view(pingAccesses), {} },
                [this, elementCount, pingSet, pushConstants](tp::ComputeList& inlineList) {
                    inlineList.cmdBindComputePipeline(distanceTransformPipeline);

                    inlineList.cmdBindDescriptorSets(pipelineLayout, { pingSet });
                    inlineList.cmdPushConstants(pipelineLayout, tp::ShaderStage::Compute, pushConstants);

                    // One thread per element
                    uint32_t groupCount = roundUpToMultiple(elementCount, WorkgroupSize) / WorkgroupSize;
                    inlineList.cmdDispatch(groupCount, 1, 1);
                });

            // Instead of switching between pingSet/pongSet, etc based on pass number, just swap them:
            std::swap(pingSet, pongSet);
            std::swap(pingBuffer, pongBuffer);
            std::swap(pingAccesses, pongAccesses);
        }

        // We may need to copy the result to the output buffer, depending on the number of passes and whether
        // the output buffer itself was used to ping-pong:
        if (pingBuffer != outputBuffer) {
            job.cmdCopyBuffer(pingBuffer, outputBuffer, { { 0, 0, bufferSize } });
        }

        // Finally, export the output buffer to be read by the host
        job.cmdExportResource(outputBuffer, tp::ReadAccess::Host);

        // Immediately enqueue and submit the job
        auto jobSemaphore = ctx->device->enqueueJob(ctx->graphicsQueueCtx.queue, std::move(job));
        // This is when our inline lambda gets executed
        ctx->device->submitQueuedJobs(ctx->graphicsQueueCtx.queue);
        return jobSemaphore;
    }

    // Helper to test allocator behavior
    struct AllocTester {
        AllocTester(StreamingComputeTester* parent, uint32_t maxConcurrency)
            : parent(parent), maxConcurrency(maxConcurrency) {}

        void checkBuffer(uint32_t bufferSize) {
            // Test how well we allocated memory for the given run when compared to a simulation of an
            // ideal allocator. Consider only one buffer per job, it should be symmetrical to both input and
            // output buffers.

            if (bufferSizes.size() == maxConcurrency) {
                minFreeSize += bufferSizes.front();
                bufferSizes.pop_front();
            }

            // Simulate allocation
            if (bufferSize >= minFreeSize)
                minPoolSize += bufferSize;
            else
                minFreeSize -= bufferSize;
            bufferSizes.push_back(bufferSize);

            // Compare to actual results of output ring buffer and, where available, input buffers as preinit
            // job buffers from the job pool
            Assert::AreEqual(bufferSizes.size(), parent->outputRingBuffer->getAllocationCount());
            uint64_t bufferSizeSum = std::accumulate(bufferSizes.begin(), bufferSizes.end(), 0ull);
            Assert::AreEqual(bufferSizeSum, parent->outputRingBuffer->getAllocatedSize());

            // For total pool size, include some leeway due to overallocation, fragmentation and aliasing
            float leeway = parent->allocBehavior.growFactor * 1.1f;
            uint64_t maxPoolSize = static_cast<uint64_t>(minPoolSize * leeway);
            Assert::IsTrue(maxPoolSize >= parent->outputRingBuffer->getTotalSize());

            auto jobPoolStats = parent->jobResourcePool->getStatistics();
            Assert::IsTrue(maxPoolSize >= jobPoolStats.preinitBufferAllocationBytes);
        }

        StreamingComputeTester* parent;
        uint32_t maxConcurrency;
        // The sizes of currently allocated buffers
        std::deque<uint64_t> bufferSizes;
        // The minimum expected pool size
        uint64_t minPoolSize = 0;
        // The simulated minimum free size of the pool
        uint64_t minFreeSize = 0;
    };

    TephraContext* ctx;
    tp::OverallocationBehavior allocBehavior = tp::OverallocationBehavior::Exact();

    // Output buffer allocator
    std::unique_ptr<tp::utils::AutoRingBuffer> outputRingBuffer;
    // Customized job resource pool allocating input and ping-pong buffers
    std::unique_ptr<tp::JobResourcePool> jobResourcePool;

    tp::DescriptorSetLayout descriptorSetLayout;
    tp::PipelineLayout pipelineLayout;
    tp::Pipeline distanceTransformPipeline;
};

TEST_CLASS(StreamingComputeTests) {
public:
    TEST_CLASS_INITIALIZE(Initialize) {
        ctx.initialize(false);
        // Don't make the buffers that big to not slow down the tests too much
        tester = std::make_unique<StreamingComputeTester>(&ctx);
    }

    TEST_CLASS_CLEANUP(Cleanup) {
        tester.reset();
        ctx.cleanup();
    }

    TEST_METHOD_CLEANUP(TestCleanup) {
        ctx.resetJobResourcePools();
    }

    TEST_METHOD(SingleJobsValidated) {
        for (int i = 0; i < 10; i++) {
            // Don't make the buffers too big to not slow down the tests due to cpu validation
            tester->run(1, 1, 1 << 12, 123456 * (i + 1), true);
        }
    }

    TEST_METHOD(MultipleJobs) {
        // We can run much larger batches when we don't need validate the outputs
        tester->run(100, 100, 1 << 20, 123456, false);
    }

    TEST_METHOD(MultipleJobsThrottled) {
        // Test reducing the concurrency, should sustain lower memory usage
        tester->run(100, 16, 1 << 20, 123456, false);
    }

    TEST_METHOD(MultipleJobsThrottledValidated) {
        // Just to check this didn't somehow screw up the output data
        tester->run(10, 4, 1 << 12, 123456, true);
    }

    TEST_METHOD(LotsOfJobsThrottled) {
        // Try to increase the number of jobs with a medium sized workload
        tester->run(1000, 64, 1 << 16, 123456, false);
    }

    TEST_METHOD(BigJobs) {
        // Finally, try the limits of what we can do to test allocation fallbacks
        // Warning: This may consume several GB of VRAM and/or RAM
        tester->run(10, 4, 1 << 26, 123456, false);
    }

private:
    static TephraContext ctx;
    static std::unique_ptr<StreamingComputeTester> tester;
};

TephraContext StreamingComputeTests::ctx;
std::unique_ptr<StreamingComputeTester> StreamingComputeTests::tester;

}
