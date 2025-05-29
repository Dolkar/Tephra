#include "cube.hpp"
#include "cube_data.hpp"
#include "linmath.h"

tp::Format depthFormat = tp::Format::DEPTH16_D16_UNORM;

CubeExample::CubeExample(std::ostream& debugStream, bool debugMode)
    : debugHandler(debugStream, debugSeverity), mainQueue(tp::QueueType::Graphics) {
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
        tp::ApplicationIdentifier("Cube Demo"),
        tp::VulkanValidationSetup(debugMode),
        &debugHandler,
        tp::view(appExtensions),
        tp::view(appLayers));

    application = tp::Application::createApplication(appSetup);

    // Choose and initialize the rendering device
    std::vector<const char*> deviceExtensions = { tp::DeviceExtension::KHR_Swapchain };

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

    prepareTexture();
    preparePipeline();

    cubeRotation = 0.0f;
}

void CubeExample::update() {
    const float spinAngle = 2.0f;

    cubeRotation = fmod(cubeRotation + spinAngle, 360.0f);
}

void CubeExample::drawFrame() {
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
    // SurfaceLostError is handled by the window manager by recreating the surface
    // If successful but suboptimal, we still present this frame and recreate at the start of the next one

    // Create a job that renders this frame
    tp::Job renderJob = jobResourcePool->createJob();

    // Allocate local resources that will be used only during the lifetime of the job
    auto depthImageSetup = tp::ImageSetup(
        tp::ImageType::Image2D, tp::ImageUsage::DepthStencilAttachment, depthFormat, acquiredImage.image->getExtent());
    tp::ImageView depthImage = renderJob.allocateLocalImage(depthImageSetup);

    auto uniformBufferSetup = tp::BufferSetup(
        sizeof(vktexcube_vs_uniform), tp::BufferUsage::HostMapped | tp::BufferUsage::UniformBuffer);
    tp::BufferView uniformBuffer = renderJob.allocatePreinitializedBuffer(
        uniformBufferSetup, tp::MemoryPreference::UploadStream);

    tp::DescriptorSetView descriptorSet = renderJob.allocateLocalDescriptorSet(
        &descriptorSetLayout, { uniformBuffer, tp::FutureDescriptor(*cubeTexture, &sampler) });

    // Set up the render pass
    auto clearColor = tp::ClearValue::ColorFloat(0.3f, 0.075f, 0.075f, 0.0f);
    auto clearDepth = tp::ClearValue::DepthStencil(1.0f, 0);
    auto depthAttachment = tp::DepthStencilAttachment(
        depthImage, /*readOnly*/ false, tp::AttachmentLoadOp::Clear, tp::AttachmentStoreOp::DontCare, clearDepth);
    auto colorAttachment = tp::ColorAttachment(
        *acquiredImage.image, tp::AttachmentLoadOp::Clear, tp::AttachmentStoreOp::Store, clearColor);

    // Tephra needs to be told how resources will be used inside a render pass - either through cmdExportResource or
    // explicitly like this:
    auto uniformBufferAccess = tp::BufferRenderAccess(
        uniformBuffer, tp::RenderAccess::VertexShaderUniformRead | tp::RenderAccess::FragmentShaderUniformRead);

    auto renderPassSetup = tp::RenderPassSetup(
        depthAttachment, tp::viewOne(colorAttachment), tp::viewOne(uniformBufferAccess), {});

    // Record the commands now

    // We don't need the swapchain image's old contents. It's good practice to discard.
    renderJob.cmdDiscardContents(*acquiredImage.image);

    // Record the pass commands inline
    renderJob.cmdExecuteRenderPass(
        renderPassSetup, { [&](tp::RenderList& renderList) {
            renderList.cmdBindGraphicsPipeline(pipeline);
            renderList.cmdBindDescriptorSets(pipelineLayout, { descriptorSet });

            // Viewport calculation from the Vulkan cube.cpp
            tp::Extent3D renderExtent = acquiredImage.image->getExtent();
            tp::Viewport viewport;
            if (renderExtent.width < renderExtent.height) {
                viewport.width = (float)renderExtent.width;
                viewport.y = (renderExtent.height - renderExtent.width) / 2.0f;
            } else {
                viewport.width = (float)renderExtent.height;
                viewport.x = (renderExtent.width - renderExtent.height) / 2.0f;
            }
            viewport.height = viewport.width;
            renderList.cmdSetViewport({ viewport });
            tp::Rect2D scissor = tp::Rect2D({ 0, 0 }, { renderExtent.width, renderExtent.height });
            renderList.cmdSetScissor({ scissor });

            renderList.cmdDraw(12 * 3);
        } });

    // Finally export for present
    renderJob.cmdExportResource(*acquiredImage.image, tp::ReadAccess::ImagePresentKHR);

    // Enqueue the job, synchronizing it with the presentation engine's semaphores
    tp::JobSemaphore jobSemaphore = device->enqueueJob(
        mainQueue, std::move(renderJob), {}, { acquiredImage.acquireSemaphore }, { acquiredImage.presentSemaphore });

    // Keep the semaphore so we can wait on it later. It will be signalled when the job is finished.
    frameSemaphores.push_back(jobSemaphore);

    // We can fill the uniform buffer now
    {
        tp::HostMappedMemory memory = uniformBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
        fillUniformBufferData(memory.getPtr<vktexcube_vs_uniform>());
    }

    // Submit the queued jobs. At this point the commands in the job get compiled and the RenderList
    // above executed.
    device->submitQueuedJobs(mainQueue);

    try {
        device->submitPresentImagesKHR(mainQueue, { swapchain.get() }, { acquiredImage.imageIndex });
    } catch (const tp::OutOfDateError&) {
        // Let the swapchain be recreated next frame
    }
}

void CubeExample::resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) {
    Example::resize(surface, width, height);

    // Recreate the swapchain
    prepareSwapchain(physicalDevice, device.get(), mainQueue, swapchainFormat);

    // Also trim the job resource pool to free temporary resources used for the previous resolution
    jobResourcePool->trim();
}

void CubeExample::releaseSurface() {
    device->waitForIdle();
    swapchain.reset();
}

void CubeExample::prepareTexture() {
    const tp::Format texFormat = tp::Format::COL32_R8G8B8A8_UNORM;

    // Check format support
    const auto requiredSupport = tp::FormatCapabilities(
        tp::FormatUsage::SampledImage, tp::FormatFeature::LinearFiltering);

    if (!requiredSupport.isSubsetOf(physicalDevice->queryFormatCapabilities(texFormat))) {
        showErrorAndExit("Demo initialization failed", "No support for R8G8B8A8_UNORM as texture image format.");
    }

    // Get size of image
    tp::Extent3D extent = tp::Extent3D(0, 0, 1);
    if (!loadLunarGTexture(nullptr, 0, &extent.width, &extent.height)) {
        showErrorAndExit("Demo initialization failed", "Failed to load LunarG texture.");
    }

    // Create an image for the cube texture
    auto imageSetup = tp::ImageSetup(
        tp::ImageType::Image2D, tp::ImageUsage::SampledImage | tp::ImageUsage::TransferDst, texFormat, extent);
    cubeTexture = device->allocateImage(imageSetup, "Cube Texture");

    // Now create a job from a temporary resource pool to upload the data
    auto tempJobPool = device->createJobResourcePool(tp::JobResourcePoolSetup(mainQueue));
    tp::Job uploadJob = tempJobPool->createJob({}, "Texture Upload Job");

    // Allocate a temporary preinitialized buffer for uploading the texture data
    uint32_t rowPitch = extent.width * tp::getFormatClassProperties(texFormat).texelBlockBytes;
    auto stagingBufferSetup = tp::BufferSetup(
        rowPitch * extent.height, tp::BufferUsage::HostMapped | tp::BufferUsage::ImageTransfer);
    tp::BufferView stagingBuffer = uploadJob.allocatePreinitializedBuffer(
        stagingBufferSetup, tp::MemoryPreference::Host);

    // Upload data to it
    {
        tp::HostMappedMemory memory = stagingBuffer.mapForHostAccess(tp::MemoryAccess::WriteOnly);
        if (!loadLunarGTexture(memory.getPtr<uint8_t>(), rowPitch, &extent.width, &extent.height)) {
            showErrorAndExit("Demo initialization failed", "Failed to load LunarG texture.");
        }
    }

    // Record the copy command
    auto copyInfo = tp::BufferImageCopyRegion(
        0, cubeTexture->getWholeRange().pickMipLevel(0), tp::Offset3D(0, 0, 0), cubeTexture->getExtent());
    uploadJob.cmdCopyBufferToImage(stagingBuffer, *cubeTexture, { copyInfo });

    // Synchronize against future access
    // Note that this is not strictly necessary. To be able to sample the texture in a shader, we either
    // export it for that future access now, or advertise it as being used as such in the render pass.
    // Doing it at this stage can be more performant and simplifies the later code.
    uploadJob.cmdExportResource(*cubeTexture, tp::ReadAccess::FragmentShaderSampled);

    // Enqueue the job, giving up ownership
    tp::JobSemaphore uploadJobSemaphore = device->enqueueJob(mainQueue, std::move(uploadJob));

    // Finally submit the job for execution and wait until its done
    device->submitQueuedJobs(mainQueue);
    device->waitForJobSemaphores({ uploadJobSemaphore });

    // Also create a sampler for it
    auto samplerSetup = tp::SamplerSetup(
        { tp::Filter::Nearest, tp::Filter::Nearest }, tp::SamplerAddressMode::ClampToEdge);
    sampler = device->createSampler(samplerSetup, "Cube Sampler");
}

void CubeExample::preparePipeline() {
    // Prepare the pipeline layout with a single descriptor set
    descriptorSetLayout = device->createDescriptorSetLayout(
        { tp::DescriptorBinding(0, tp::DescriptorType::UniformBuffer, tp::ShaderStage::Vertex),
          tp::DescriptorBinding(1, tp::DescriptorType::CombinedImageSampler, tp::ShaderStage::Fragment) });
    pipelineLayout = device->createPipelineLayout({ &descriptorSetLayout });

    // Now setup stuff for the actual pipeline
    tp::ShaderModule vertShader = device->createShaderModule(
        tp::view(vertShaderCode, sizeof(vertShaderCode) / sizeof(vertShaderCode[0])));
    tp::ShaderModule fragShader = device->createShaderModule(
        tp::view(fragShaderCode, sizeof(fragShaderCode) / sizeof(fragShaderCode[0])));

    auto vertShaderSetup = tp::ShaderStageSetup(&vertShader, "main");
    auto fragShaderSetup = tp::ShaderStageSetup(&fragShader, "main");

    auto pipelineSetup = tp::GraphicsPipelineSetup(&pipelineLayout, vertShaderSetup, fragShaderSetup, "Cube Pipeline");

    // Describe what it will render into
    pipelineSetup.setDepthStencilAttachment(depthFormat);
    pipelineSetup.setColorAttachments({ swapchainFormat });

    // Back face culling with depth test & write
    pipelineSetup.setCullMode(tp::CullModeFlag::BackFace);
    pipelineSetup.setDepthTest(true, tp::CompareOp::LessOrEqual, true);

    device->compileGraphicsPipelines({ &pipelineSetup }, nullptr, { &pipeline });
}

void CubeExample::fillUniformBufferData(vktexcube_vs_uniform* data) {
    // Set up matrices
    vec3 eye = { 0.0f, 3.0f, 5.0f };
    vec3 origin = { 0, 0, 0 };
    vec3 up = { 0.0f, 1.0f, 0.0 };

    mat4x4 projectionMatrix;
    mat4x4_perspective(projectionMatrix, (float)degreesToRadians(45.0f), 1.0f, 0.1f, 100.0f);
    projectionMatrix[1][1] *= -1; // Flip projection matrix from GL to Vulkan orientation.

    mat4x4 viewMatrix;
    mat4x4_look_at(viewMatrix, eye, origin, up);

    mat4x4 identityMatrix;
    mat4x4_identity(identityMatrix);
    mat4x4 modelMatrix;
    mat4x4_rotate(modelMatrix, identityMatrix, 0.0f, 1.0f, 0.0f, (float)degreesToRadians(cubeRotation));

    // Form MVP matrix and fill out the uniform data
    mat4x4 VP;
    mat4x4_mul(VP, projectionMatrix, viewMatrix);

    mat4x4 MVP;
    mat4x4_mul(MVP, VP, modelMatrix);

    memcpy(data->mvp, MVP, sizeof(MVP));

    for (int32_t i = 0; i < 12 * 3; i++) {
        data->position[i][0] = g_vertex_buffer_data[i * 3];
        data->position[i][1] = g_vertex_buffer_data[i * 3 + 1];
        data->position[i][2] = g_vertex_buffer_data[i * 3 + 2];
        data->position[i][3] = 1.0f;
        data->attr[i][0] = g_uv_buffer_data[2 * i];
        data->attr[i][1] = g_uv_buffer_data[2 * i + 1];
        data->attr[i][2] = 0;
        data->attr[i][3] = 0;
    }
}
