#include "cornell.hpp"
#include "cornell_data.hpp"

#include <cassert>
#include <iostream>
#include <fstream>

inline tp::ShaderModule loadShader(tp::Device* device, std::string path) {
    std::ifstream fileStream{ path, std::ios::binary | std::ios::in | std::ios::ate };

    if (!fileStream.is_open()) {
        throw std::runtime_error("Shader '" + path + "' not found.");
    }

    auto byteSize = fileStream.tellg();
    if (byteSize % sizeof(uint32_t) != 0) {
        throw std::runtime_error("Shader '" + path + "' has incorrect size.");
    }
    std::vector<uint32_t> shaderCode;
    shaderCode.resize(byteSize / sizeof(uint32_t));

    fileStream.seekg(0);
    fileStream.read(reinterpret_cast<char*>(shaderCode.data()), byteSize);

    return device->createShaderModule(tp::view(shaderCode));
}

CornellExample::CornellExample(std::ostream& debugStream, RenderingMethod method, bool debugMode)
    : debugHandler(debugStream, debugSeverity), method(method), mainQueue(tp::QueueType::Graphics) {
    // Initialize Tephra Application
    // Setup required and optional extensions
    std::vector<const char*> appExtensions = { tp::ApplicationExtension::KHR_Surface };

    if (debugMode && tp::Application::isExtensionAvailable(tp::ApplicationExtension::EXT_DebugUtils)) {
        appExtensions.push_back(tp::ApplicationExtension::EXT_DebugUtils);
    }

    // Also enable monitor layer if present. Validation layers are enabled through validation setup.
    std::vector<const char*> appLayers = {};
    if (tp::Application::isLayerAvailable(vkLayerLunargMonitorName)) {
        appLayers.push_back(vkLayerLunargMonitorName);
    }

    // Create the application
    auto appSetup = tp::ApplicationSetup(
        tp::ApplicationIdentifier("Cornell Path Tracing Demo"),
        tp::VulkanValidationSetup(debugMode),
        &debugHandler,
        tp::view(appExtensions),
        tp::view(appLayers));

    application = tp::Application::createApplication(appSetup);

    // Choose and initialize the rendering device
    std::vector<const char*> deviceExtensions = { tp::DeviceExtension::KHR_Swapchain,
                                                  tp::DeviceExtension::KHR_AccelerationStructure };

    // Enable scalar block layout to simplify passing of data to shaders
    tp::VkFeatureMap featureMap;
    featureMap.get<VkPhysicalDeviceVulkan12Features>().scalarBlockLayout = true;

    if (method == RenderingMethod::RayQuery)
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayQuery);

    for (const tp::PhysicalDevice& candidateDevice : application->getPhysicalDevices()) {
        for (const char* ext : deviceExtensions) {
            if (!candidateDevice.isExtensionAvailable(ext))
                continue;
        }

        physicalDevice = &candidateDevice;
        break;
    }

    if (physicalDevice == nullptr) {
        showErrorAndExit("Vulkan initialization failed", "No supported physical device has been found!");
    }

    auto deviceSetup = tp::DeviceSetup(
        physicalDevice,
        tp::viewOne(mainQueue), // Use one graphics queue for this example
        tp::view(deviceExtensions),
        &featureMap);

    device = application->createDevice(deviceSetup);

    // Also create a Job Resource Pool that temporary resources will be allocated from
    jobResourcePool = device->createJobResourcePool(tp::JobResourcePoolSetup(mainQueue));

    preparePipelineLayout();

    switch (method) {
    case RenderingMethod::RayQuery:
        prepareRayQueryPipeline();
        break;
    case RenderingMethod::RayTracingPipeline:
        showErrorAndExit("Ray tracing pipelines not implemented yet", "");
        break;
    }

    prepareBLAS();
    preparePlaneBuffer();

    windowWidth = 800;
    windowHeight = 800;
}

void CornellExample::update() {}

void CornellExample::drawFrame() {
    // Limit the number of outstanding frames being rendered
    if (frameSemaphores.size() >= 2) {
        device->waitForJobSemaphores({ frameSemaphores.front() });
        frameSemaphores.pop_front();
    }

    if (swapchain->getStatus() != tp::SwapchainStatus::Optimal) {
        // Recreate out of date or suboptimal swapchain
        prepareSwapchain(physicalDevice, device.get(), mainQueue, swapchainFormat);
    }

    // Acquire a swapchain image to draw the frame to
    tp::AcquiredImageInfo acquiredImage;
    try {
        acquiredImage = swapchain->acquireNextImage().value();
    } catch (const tp::OutOfDateError&) {
        return;
    }

    tp::Job renderJob = jobResourcePool->createJob({}, "Render Job");

    if (accumImage == nullptr || accumImage->getExtent() != acquiredImage.image->getExtent()) {
        // Create an image to accumulate our renders to
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::StorageImage | tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            tp::Format::COL128_R32G32B32A32_SFLOAT,
            acquiredImage.image->getExtent());
        accumImage = device->allocateImage(imageSetup);

        // Also clear it for the first pass
        renderJob.cmdClearImage(*accumImage, tp::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
    }

    tp::AccelerationStructureView tlasView = prepareTLAS(renderJob);
    tp::DescriptorSetView descriptorSet = renderJob.allocateLocalDescriptorSet(
        &descSetLayout, { tlasView, planeBuffer->getDefaultView(), accumImage->getDefaultView() });

    // Run a single inline compute pass
    auto imageAccess = tp::ImageComputeAccess(
        *accumImage, tp::ComputeAccess::ComputeShaderStorageRead | tp::ComputeAccess::ComputeShaderStorageWrite);
    renderJob.cmdExecuteComputePass(
        tp::ComputePassSetup({}, tp::viewOne(imageAccess)), [&](tp::ComputeList& inlineList) {
            inlineList.cmdBindComputePipeline(pipeline);
            inlineList.cmdBindDescriptorSets(pipelineLayout, { descriptorSet });

            Vector cameraPosition = { 278.0f, 273.0f, -800.0f };
            float cameraFovTan = 0.025f / 0.035f;
            uint32_t samplesPerPixel = 16;

            PushConstantData data = { cameraPosition,
                                      cameraFovTan,
                                      samplesPerPixel,
                                      frameIndex,
                                      accumImage->getExtent().width,
                                      accumImage->getExtent().height };
            inlineList.cmdPushConstants<PushConstantData>(pipelineLayout, tp::ShaderStage::Compute, data);

            inlineList.cmdDispatch(
                roundUpToMultiple(accumImage->getExtent().width, WorkgroupSizeDim) / WorkgroupSizeDim,
                roundUpToMultiple(accumImage->getExtent().height, WorkgroupSizeDim) / WorkgroupSizeDim);
        });

    // Blit the image to the swapchain one
    auto blitRegion = tp::ImageBlitRegion(
        accumImage->getWholeRange().pickMipLevel(0),
        { 0, 0, 0 },
        accumImage->getExtent(),
        acquiredImage.image->getWholeRange().pickMipLevel(0),
        { 0, 0, 0 },
        acquiredImage.image->getExtent());
    renderJob.cmdBlitImage(*accumImage, *acquiredImage.image, { blitRegion });
    renderJob.cmdExportResource(*acquiredImage.image, tp::ReadAccess::ImagePresentKHR);

    // Enqueue the job, synchronizing it with the presentation engine's semaphores
    tp::JobSemaphore jobSemaphore = device->enqueueJob(
        mainQueue, std::move(renderJob), {}, { acquiredImage.acquireSemaphore }, { acquiredImage.presentSemaphore });

    frameSemaphores.push_back(jobSemaphore);

    // Submit and present
    device->submitQueuedJobs(mainQueue);
    try {
        device->submitPresentImagesKHR(mainQueue, { swapchain.get() }, { acquiredImage.imageIndex });
    } catch (const tp::OutOfDateError&) {
        // Let the swapchain be recreated next frame
    }

    frameIndex++;
}

void CornellExample::resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    Example::resize(surface, width, height);

    // Recreate the swapchain
    prepareSwapchain(physicalDevice, device.get(), mainQueue, swapchainFormat);

    // Also trim the job resource pool to free temporary resources used for the previous resolution
    jobResourcePool->trim();

    // Reset frame index so we start accumulating the result from scratch
    frameIndex = 0;
}

void CornellExample::releaseSurface() {
    device->waitForIdle();
    swapchain.reset();
}

void CornellExample::prepareBLAS() {
    std::vector<std::vector<Point>> geometry;

    // Form a list of triangles for each object
    geometry.resize(static_cast<std::size_t>(CornellObject::NObjects));

    for (const Plane& plane : cornellBox) {
        std::vector<Point>& vertices = geometry[static_cast<std::size_t>(plane.objectId)];

        // Split each plane into two triangles
        vertices.push_back(plane.p0);
        vertices.push_back(plane.p1);
        vertices.push_back(plane.p2);

        vertices.push_back(plane.p0);
        vertices.push_back(plane.p2);
        vertices.push_back(plane.p3);
    }

    // Create and build a BLAS for each object
    tp::Job buildJob = jobResourcePool->createJob();

    std::vector<tp::TriangleGeometryBuildInfo> geomInfos;
    geomInfos.reserve(geometry.size());
    std::vector<tp::AccelerationStructureBuildInfo> buildInfos;
    std::vector<std::unique_ptr<tp::AccelerationStructure>> srcBlasList;

    for (int i = 0; i < geometry.size(); i++) {
        const std::vector<Point>& vertices = geometry[i];
        auto triSetup = tp::TriangleGeometrySetup(
            static_cast<uint32_t>(vertices.size() / 3),
            tp::Format::COL96_R32G32B32_SFLOAT,
            static_cast<uint32_t>(vertices.size()),
            tp::IndexType::NoneKHR,
            false,
            tp::GeometryFlag::Opaque);

        // Compacting after build will reduce the final size in memory
        auto blasSetup = tp::AccelerationStructureSetup::BottomLevel(
            tp::AccelerationStructureFlag::PreferFastTrace | tp::AccelerationStructureFlag::AllowCompaction,
            tp::viewOne(triSetup),
            {});

        std::string name = "geom" + std::to_string(i);
        auto blas = device->allocateAccelerationStructureKHR(blasSetup, name.c_str());

        // Upload vertex data. It's very small so we can use cmdUpdateBuffer into a device local buffer
        std::size_t verticesBytes = vertices.size() * sizeof(Point);
        auto vertexBuffer = buildJob.allocateLocalBuffer(
            tp::BufferSetup(verticesBytes, tp::BufferUsage::AccelerationStructureInputKHR));
        buildJob.cmdUpdateBuffer(
            vertexBuffer, tp::view(reinterpret_cast<const std::byte*>(vertices.data()), verticesBytes));

        // Setup building the BLAS for this object
        auto& geomInfo = geomInfos.emplace_back(tp::TriangleGeometryBuildInfo(vertexBuffer));
        buildInfos.push_back(tp::AccelerationStructureBuildInfo::BottomLevel(
            tp::AccelerationStructureBuildMode::Build, *blas, tp::viewOne(geomInfo), {}));

        srcBlasList.push_back(std::move(blas));
    }

    // Build and submit
    buildJob.cmdBuildAccelerationStructuresKHR(tp::view(buildInfos));

    tp::JobSemaphore jobSemaphore = device->enqueueJob(mainQueue, std::move(buildJob));
    device->submitQueuedJobs(mainQueue);

    // Wait for the build to complete so we can create compacted BLASes
    device->waitForJobSemaphores({ jobSemaphore });

    // Compact and store the results
    tp::Job compactionJob = jobResourcePool->createJob();

    for (auto& srcBlas : srcBlasList) {
        // Debug name gets inherited
        auto blas = device->allocateCompactedAccelerationStructureKHR(*srcBlas);

        compactionJob.cmdCopyAccelerationStructureKHR(*srcBlas, *blas, tp::AccelerationStructureCopyMode::Compact);

        // We will use it to build a TLAS and also trace through it in a compute shader
        compactionJob.cmdExportResource(
            *blas, tp::ReadAccess::AccelerationStructureBuildKHR | tp::ReadAccess::ComputeShaderUniform);

        blasList.push_back(std::move(blas));
    }

    device->enqueueJob(mainQueue, std::move(compactionJob));
    device->submitQueuedJobs(mainQueue);
}

void CornellExample::preparePlaneBuffer() {
    // Form a flat list of plane data, indexable by instanceIndex * MaxPlanesPerInstance + primitiveIndex / 2
    std::vector<PlaneMaterialData> planesData;
    planesData.resize(static_cast<std::size_t>(CornellObject::NObjects) * MaxPlanesPerInstance);

    std::vector<std::size_t> planeCounts;
    planeCounts.resize(static_cast<std::size_t>(CornellObject::NObjects));

    for (const Plane& plane : cornellBox) {
        // Calculate plane normal
        Vector v01 = { plane.p1.x - plane.p0.x, plane.p1.y - plane.p0.y, plane.p1.z - plane.p0.z };
        Vector v02 = { plane.p2.x - plane.p0.x, plane.p2.y - plane.p0.y, plane.p2.z - plane.p0.z };
        Vector cross = { v01.y * v02.z - v01.z * v02.y, v01.z * v02.x - v01.x * v02.z, v01.x * v02.y - v01.y * v02.x };
        float d = sqrt(cross.x * cross.x + cross.y * cross.y + cross.z * cross.z);
        Vector n = { cross.x / d, cross.y / d, cross.z / d };

        PlaneMaterialData planeData = { n, plane.reflectance, plane.emission };

        std::size_t instanceIndex = static_cast<std::size_t>(plane.objectId);
        std::size_t planeIndex = planeCounts[instanceIndex]++;
        assert(planeIndex < MaxPlanesPerInstance);

        planesData[instanceIndex * MaxPlanesPerInstance + planeIndex] = planeData;
    }

    // Create the buffer in device memory
    auto bufferSetup = tp::BufferSetup(planesData.size() * sizeof(PlaneMaterialData), tp::BufferUsage::StorageBuffer);
    planeBuffer = device->allocateBuffer(bufferSetup, tp::MemoryPreference::Device, "Plane Material Data");

    // Now create an upload job
    tp::Job uploadJob = jobResourcePool->createJob({}, "Plane Data Upload Job");

    // Allocate a temporary staging buffer in host memory
    auto stagingBufferSetup = tp::BufferSetup(bufferSetup.size, tp::BufferUsage::HostMapped);
    tp::BufferView stagingBuffer = uploadJob.allocatePreinitializedBuffer(
        stagingBufferSetup, tp::MemoryPreference::Host);

    // Upload data to it
    {
        tp::HostMappedMemory memory = stagingBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
        memcpy(memory.getPtr(), planesData.data(), bufferSetup.size);
    }

    // Record the copy and export
    uploadJob.cmdCopyBuffer(stagingBuffer, *planeBuffer, { tp::BufferCopyRegion(0, 0, bufferSetup.size) });
    uploadJob.cmdExportResource(*planeBuffer, tp::ReadAccess::ComputeShaderStorage);

    // Submit the work
    device->enqueueJob(mainQueue, std::move(uploadJob));
    device->submitQueuedJobs(mainQueue);
}

void CornellExample::preparePipelineLayout() {
    descSetLayout = device->createDescriptorSetLayout(
        { tp::DescriptorBinding(0, tp::DescriptorType::AccelerationStructureKHR, tp::ShaderStage::Compute),
          tp::DescriptorBinding(1, tp::DescriptorType::StorageBuffer, tp::ShaderStage::Compute),
          tp::DescriptorBinding(2, tp::DescriptorType::StorageImage, tp::ShaderStage::Compute) });

    pipelineLayout = device->createPipelineLayout(
        { &descSetLayout }, { tp::PushConstantRange(tp::ShaderStage::Compute, 0, sizeof(PushConstantData)) });
}

void CornellExample::prepareRayQueryPipeline() {
    auto shader = loadShader(device.get(), "trace_ray_query.spv");

    auto pipelineSetup = tp::ComputePipelineSetup(&pipelineLayout, tp::ShaderStageSetup(&shader, "main"));
    device->compileComputePipelines({ &pipelineSetup }, nullptr, { &pipeline });
}

tp::AccelerationStructureView CornellExample::prepareTLAS(tp::Job& renderJob) {
    // Form a list of instances. One instance = one BLAS
    int instanceCount = blasList.size();
    std::vector<VkAccelerationStructureInstanceKHR> vkInstances;

    for (int i = 0; i < instanceCount; i++) {
        // Identity matrix - the geometry is pre-transformed
        VkTransformMatrixKHR transform = {
            { { 1.0f, 0.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f, 0.0f } }
        };

        VkAccelerationStructureInstanceKHR vkInstance;
        vkInstance.transform = transform;
        vkInstance.instanceCustomIndex = i; // objectId
        vkInstance.mask = 0xff;
        vkInstance.instanceShaderBindingTableRecordOffset = 0;
        vkInstance.flags = 0;
        vkInstance.accelerationStructureReference = blasList[i]->getDeviceAddress();

        vkInstances.push_back(vkInstance);
    }

    auto instanceSetup = tp::InstanceGeometrySetup(instanceCount, tp::GeometryFlag::Opaque);
    auto tlasSetup = tp::AccelerationStructureSetup::TopLevel(
        tp::AccelerationStructureFlag::PreferFastTrace, instanceSetup);
    auto tlas = renderJob.allocateLocalAccelerationStructureKHR(tlasSetup, "tlas");

    // Upload instance data. Again, very small, so cmdUpdateBuffer suffices
    std::size_t instancesBytes = instanceCount * sizeof(VkAccelerationStructureInstanceKHR);
    auto instanceBuffer = renderJob.allocateLocalBuffer(
        tp::BufferSetup(instancesBytes, tp::BufferUsage::AccelerationStructureInputKHR));
    renderJob.cmdUpdateBuffer(
        instanceBuffer, tp::view(reinterpret_cast<const std::byte*>(vkInstances.data()), instancesBytes));

    auto buildInfo = tp::AccelerationStructureBuildInfo::TopLevel(
        tp::AccelerationStructureBuildMode::Build, tlas, tp::InstanceGeometryBuildInfo(instanceBuffer));
    renderJob.cmdBuildAccelerationStructuresKHR({ buildInfo });

    // Export for reading it from inside the compute shader - treated as uniform
    renderJob.cmdExportResource(tlas, tp::ReadAccess::ComputeShaderUniform);

    return tlas;
}
