#pragma once

#include "../examples_common.hpp"
#include <tephra/utils/standard_report_handler.hpp>
#include <deque>

struct vktexcube_vs_uniform;

// Reimplementation of https://github.com/KhronosGroup/Vulkan-LoaderAndValidationLayers/blob/master/demos/cube.cpp
class CubeExample : public Example {
public:
    CubeExample(std::ostream& debugStream, bool debugMode);

    virtual const tp::Application* getApplication() const override;

    virtual void update() override;

    virtual void drawFrame() override;

    virtual void resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) override;

    virtual void releaseSurface() override;

private:
    tp::utils::StandardReportHandler debugHandler;
    tp::DeviceQueue mainQueue;

    // tp::OwningPtr is by default just an alias for std::unique_ptr
    tp::OwningPtr<tp::Application> application;
    const tp::PhysicalDevice* physicalDevice;
    tp::OwningPtr<tp::Device> device;
    tp::OwningPtr<tp::JobResourcePool> jobResourcePool;

    VkSurfaceKHR surface;
    tp::OwningPtr<tp::Swapchain> swapchain;

    tp::OwningPtr<tp::Image> cubeTexture;
    tp::Sampler sampler;

    tp::DescriptorSetLayout descriptorSetLayout;
    tp::PipelineLayout pipelineLayout;
    tp::Pipeline pipeline;

    std::deque<tp::JobSemaphore> frameSemaphores;

    uint32_t width;
    uint32_t height;

    float cubeRotation;

    void prepareTexture();
    void preparePipeline();
    void prepareSwapchain();

    void fillUniformBufferData(vktexcube_vs_uniform* data);
};
