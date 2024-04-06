#include "cornell.hpp"
#include "cornell_data.hpp"

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
                                                  tp::DeviceExtension::KHR_AccelerationStructure,
                                                  "VK_EXT_scalar_block_layout" };

    if (method == RenderingMethod::RayQuery)
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayQuery);
    else if (method == RenderingMethod::RayTracingPipeline)
        deviceExtensions.push_back(tp::DeviceExtension::KHR_RayTracingPipeline);

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
        tp::view(deviceExtensions));

    device = application->createDevice(deviceSetup);

    // Also create a Job Resource Pool that temporary resources will be allocated from
    jobResourcePool = device->createJobResourcePool(tp::JobResourcePoolSetup(mainQueue));

    preparePipelineLayout();

    switch (method) {
    case RenderingMethod::RayQuery:
        prepareRayQueryPipeline();
        break;
    case RenderingMethod::RayTracingPipeline:
        prepareRayTracingPipeline();
        break;
    }

    prepareBLAS();

    auto bufferSetup = tp::BufferSetup(16, tp::BufferUsage::StorageBuffer);
    planeMaterialBuffer = device->allocateBuffer(bufferSetup, tp::MemoryPreference::Device, "Plane Material Data");

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
        prepareSwapchain(physicalDevice, device.get(), mainQueue);
    }

    // Acquire a swapchain image to draw the frame to
    tp::AcquiredImageInfo acquiredImage;
    try {
        acquiredImage = swapchain->acquireNextImage().value();
    } catch (const tp::OutOfDateError&) {
        return;
    }

    tp::Job renderJob = jobResourcePool->createJob();

    if (accumImage == nullptr || accumImage->getExtent() != acquiredImage.image->getExtent()) {
        // Create an image to accumulate our renders to
        auto imageSetup = tp::ImageSetup(
            tp::ImageType::Image2D,
            tp::ImageUsage::StorageImage | tp::ImageUsage::TransferSrc | tp::ImageUsage::TransferDst,
            tp::Format::COL64_R16G16B16A16_SFLOAT,
            acquiredImage.image->getExtent());
        accumImage = device->allocateImage(imageSetup);

        // Also clear it for the first pass
        renderJob.cmdClearImage(*accumImage, tp::ClearValue::ColorFloat(0.0f, 0.0f, 0.0f, 0.0f));
    }

    tp::AccelerationStructureView tlasView = prepareTLAS(renderJob);
    tp::DescriptorSetView descriptorSet = renderJob.allocateLocalDescriptorSet(
        &descSetLayout, { tlasView, planeMaterialBuffer->getDefaultView(), accumImage->getDefaultView() });

    // Run a single inline compute pass
    auto imageAccess = tp::ImageComputeAccess(
        *accumImage, tp::ComputeAccess::ComputeShaderStorageRead | tp::ComputeAccess::ComputeShaderStorageWrite);
    renderJob.cmdExecuteComputePass(
        tp::ComputePassSetup({}, tp::viewOne(imageAccess)), [&](tp::ComputeList& inlineList) {
            inlineList.cmdBindComputePipeline(pipeline);
            inlineList.cmdBindDescriptorSets(pipelineLayout, { descriptorSet });

            Vector cameraPosition = { 278.0f, 273.0f, -800.0f };
            float cameraFovTan = 0.025f / 0.035f;
            PushConstantData data = {
                cameraPosition, cameraFovTan, accumImage->getExtent().width, accumImage->getExtent().height
            };
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
}

void CornellExample::resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    Example::resize(surface, width, height);

    // Recreate the swapchain
    prepareSwapchain(physicalDevice, device.get(), mainQueue);

    // Also trim the job resource pool to free temporary resources used for the previous resolution
    jobResourcePool->trim();
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

    for (int i = 0; i < geometry.size(); i++) {
        const std::vector<Point>& vertices = geometry[i];
        auto triSetup = tp::TriangleGeometrySetup(
            static_cast<uint32_t>(vertices.size() / 3),
            tp::Format::COL96_R32G32B32_SFLOAT,
            static_cast<uint32_t>(vertices.size()),
            tp::IndexType::NoneKHR,
            false,
            tp::GeometryFlag::Opaque);

        auto blasSetup = tp::AccelerationStructureSetup::BottomLevel(
            tp::AccelerationStructureBuildFlag::PreferFastTrace, tp::viewOne(triSetup), {});

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

        // And store it for use later
        blasList.push_back(std::move(blas));
    }

    // Build and submit
    buildJob.cmdBuildAccelerationStructuresKHR(tp::view(buildInfos));

    device->enqueueJob(mainQueue, std::move(buildJob));
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

void CornellExample::prepareRayTracingPipeline() {
    showErrorAndExit("Ray tracing pipeline demo not implemented yet", "");
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
        tp::AccelerationStructureBuildFlag::PreferFastTrace, instanceSetup);
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
