#pragma once

#include "../examples_common.hpp"
#include <tephra/utils/standard_report_handler.hpp>
#include <deque>

enum class RenderingMethod {
    RayQuery,
    RayTracingPipeline,
};

class CornellExample : public Example {
public:
    CornellExample(std::ostream& debugStream, RenderingMethod method, bool debugMode);

    virtual const tp::Application* getApplication() const override {
        return application.get();
    }

    virtual void update() override;

    virtual void drawFrame() override;

    virtual void resize(VkSurfaceKHR surface, uint32_t width, uint32_t height) override;

    virtual void releaseSurface() override;

private:
    static constexpr tp::Format swapchainFormat = tp::Format::COL32_B8G8R8A8_SRGB;

    tp::utils::StandardReportHandler debugHandler;
    RenderingMethod method;
    tp::DeviceQueue mainQueue;

    tp::OwningPtr<tp::Application> application;
    const tp::PhysicalDevice* physicalDevice;
    tp::OwningPtr<tp::Device> device;
    tp::OwningPtr<tp::JobResourcePool> jobResourcePool;

    tp::DescriptorSetLayout descSetLayout;
    tp::PipelineLayout pipelineLayout;
    tp::Pipeline pipeline;

    tp::OwningPtr<tp::Buffer> planeBuffer;
    tp::OwningPtr<tp::Image> accumImage;
    std::vector<tp::OwningPtr<tp::AccelerationStructure>> blasList;

    std::deque<tp::JobSemaphore> frameSemaphores;
    uint32_t frameIndex = 0;

    void prepareBLAS();
    void preparePlaneBuffer();
    void preparePipelineLayout();
    void prepareRayQueryPipeline();
    tp::AccelerationStructureView prepareTLAS(tp::Job& renderJob);
};
