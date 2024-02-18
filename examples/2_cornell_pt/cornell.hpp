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
    tp::utils::StandardReportHandler debugHandler;
    RenderingMethod method;
    tp::DeviceQueue mainQueue;

    tp::OwningPtr<tp::Application> application;
    const tp::PhysicalDevice* physicalDevice;
    tp::OwningPtr<tp::Device> device;
    tp::OwningPtr<tp::JobResourcePool> jobResourcePool;

    tp::PipelineLayout pipelineLayout;
    tp::Pipeline pipeline;

    std::vector<tp::OwningPtr<tp::AccelerationStructure>> blasList;

    std::deque<tp::JobSemaphore> frameSemaphores;

    void prepareBLAS();
    void prepareRayQueryPipeline();
    void prepareRayTracingPipeline();
};
