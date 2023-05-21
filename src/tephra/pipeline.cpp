#include "common_impl.hpp"
#include "pipeline_builder.hpp"
#include "device/device_container.hpp"
#include <tephra/pipeline.hpp>
#include <deque>

namespace tp {

std::size_t PipelineCache::getDataSize() const {
    auto deviceImpl = static_cast<const DeviceContainer*>(device);
    ArrayView<std::byte> nullArray;
    return deviceImpl->getLogicalDevice()->getPipelineCacheData(vkGetPipelineCacheHandle(), nullArray);
}

void PipelineCache::getData(ArrayView<std::byte> data) {
    auto deviceImpl = static_cast<const DeviceContainer*>(device);
    deviceImpl->getLogicalDevice()->getPipelineCacheData(vkGetPipelineCacheHandle(), data);
}

ComputePipelineSetup::ComputePipelineSetup(
    const PipelineLayout* pipelineLayout,
    ShaderStageSetup computeStageSetup,
    const char* debugName)
    : pipelineLayout(pipelineLayout),
      computeStageSetup(std::move(computeStageSetup)),
      debugName(debugName ? debugName : std::string()) {}

ComputePipelineSetup& ComputePipelineSetup::setComputeStage(ShaderStageSetup computeStageSetup) {
    this->computeStageSetup = std::move(computeStageSetup);
    return *this;
}

ComputePipelineSetup& ComputePipelineSetup::addFlags(PipelineFlagMask flags) {
    this->flags |= flags;
    return *this;
}

ComputePipelineSetup& ComputePipelineSetup::clearFlags() {
    this->flags = PipelineFlagMask::None();
    return *this;
}

ComputePipelineSetup& ComputePipelineSetup::setDebugName(const char* debugName) {
    this->debugName = debugName ? debugName : std::string();
    return *this;
}

ComputePipelineSetup& ComputePipelineSetup::vkSetCreateInfoExtPtr(void* pNext) {
    this->pNext = pNext;
    return *this;
}

GraphicsPipelineSetup::GraphicsPipelineSetup(
    const PipelineLayout* pipelineLayout,
    const RenderPassLayout* renderPassLayout,
    uint32_t subpassIndex,
    ShaderStageSetup vertexStageSetup,
    ShaderStageSetup fragmentStageSetup,
    const char* debugName)
    : pipelineLayout(pipelineLayout),
      renderPassLayout(renderPassLayout),
      subpassIndex(subpassIndex),
      vertexStageSetup(std::move(vertexStageSetup)),
      fragmentStageSetup(std::move(fragmentStageSetup)),
      debugName(debugName ? debugName : std::string()) {
    TEPHRA_ASSERT(renderPassLayout != nullptr);
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setRenderPassLayout(
    const RenderPassLayout* renderPassLayout,
    uint32_t subpassIndex) {
    TEPHRA_ASSERT(renderPassLayout != nullptr);
    this->renderPassLayout = renderPassLayout;
    this->subpassIndex = subpassIndex;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setVertexInputBindings(
    ArrayParameter<const VertexInputBinding> vertexInputBindings) {
    this->vertexInputBindings.clear();
    this->vertexInputBindings.insert(
        this->vertexInputBindings.begin(), vertexInputBindings.begin(), vertexInputBindings.end());
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setVertexStage(ShaderStageSetup vertexStageSetup) {
    this->vertexStageSetup = std::move(vertexStageSetup);
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setFragmentStage(ShaderStageSetup fragmentStageSetup) {
    this->fragmentStageSetup = std::move(fragmentStageSetup);
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setGeometryStage(ShaderStageSetup geometryStageSetup) {
    this->geometryStageSetup = std::move(geometryStageSetup);
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setTessellationStages(
    ShaderStageSetup tessellationControlStageSetup,
    ShaderStageSetup tessellationEvaluationStageSetup,
    uint32_t patchControlPoints) {
    this->tessellationControlStageSetup = std::move(tessellationControlStageSetup);
    this->tessellationEvaluationStageSetup = std::move(tessellationEvaluationStageSetup);
    this->patchControlPoints = patchControlPoints;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setTopology(PrimitiveTopology topology, bool primitiveRestartEnable) {
    this->topology = topology;
    this->primitiveRestartEnable = primitiveRestartEnable;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setViewportCount(uint32_t viewportCount) {
    this->viewportCount = viewportCount;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setRasterizationMode(RasterizationMode mode) {
    this->rasterizationMode = mode;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setFrontFace(bool frontFaceIsClockwise) {
    this->frontFaceIsClockwise = frontFaceIsClockwise;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setCullMode(CullModeFlagMask cullMode) {
    this->cullMode = cullMode;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setDepthBias(
    bool enable,
    float constantFactor,
    float slopeFactor,
    float biasClamp) {
    this->depthBiasEnable = enable;
    this->depthBiasConstantFactor = constantFactor;
    this->depthBiasSlopeFactor = slopeFactor;
    this->depthBiasClamp = biasClamp;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setMultisampling(
    MultisampleLevel level,
    uint64_t sampleMask,
    bool sampleShadingEnable,
    float minSampleShading) {
    this->multisampleLevel = level;
    this->sampleMask = sampleMask;
    this->sampleShadingEnable = sampleShadingEnable;
    this->minSampleShading = minSampleShading;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setAlphaToCoverage(bool enable, bool alphaToOneEnable) {
    this->alphaToCoverageEnable = enable;
    this->alphaToOneEnable = alphaToOneEnable;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setDepthTest(bool enable, CompareOp compareOp, bool enableWrite) {
    this->depthTestEnable = enable;
    this->depthTestCompareOp = compareOp;
    this->depthWriteEnable = enableWrite;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setDepthBoundsTest(
    bool enable,
    float minDepthBounds,
    float maxDepthBounds) {
    this->depthBoundsTestEnable = enable;
    this->minDepthBounds = minDepthBounds;
    this->maxDepthBounds = maxDepthBounds;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setDepthClamp(bool enable) {
    this->depthClampEnable = enable;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setStencilTest(bool enable, StencilState stencilState) {
    this->stencilTestEnable = enable;
    this->backFaceStencilState = stencilState;
    this->frontFaceStencilState = stencilState;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setStencilTest(
    bool enable,
    StencilState frontFaceStencilState,
    StencilState backFaceStencilState) {
    this->stencilTestEnable = enable;
    this->backFaceStencilState = frontFaceStencilState;
    this->frontFaceStencilState = backFaceStencilState;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setLogicBlendOp(bool enable, LogicOp logicOp) {
    this->logicBlendEnable = enable;
    this->logicBlendOp = logicOp;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setLineWidth(float width) {
    this->lineWidth = width;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setBlending(bool enable, AttachmentBlendState blendState) {
    this->blendEnable = enable;
    this->independentBlendEnable = false;
    this->blendStates.clear();

    if (enable) {
        this->blendStates.push_back(blendState);
    }
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setIndependentBlending(
    bool enable,
    ArrayParameter<const AttachmentBlendState> blendStates) {
    this->blendEnable = enable;
    this->independentBlendEnable = enable;
    this->blendStates.clear();

    if (enable) {
        this->blendStates.insert(this->blendStates.begin(), blendStates.begin(), blendStates.end());
    }
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setBlendConstants(float blendConstants[4]) {
    for (int i = 0; i < 4; i++) {
        this->blendConstants[i] = blendConstants[i];
    }
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::addDynamicState(DynamicState dynamicState) {
    this->dynamicStates.push_back(dynamicState);
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::clearDynamicState() {
    this->dynamicStates.clear();
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::addFlags(PipelineFlagMask flags) {
    this->flags |= flags;
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::clearFlags() {
    this->flags = PipelineFlagMask::None();
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::setDebugName(const char* debugName) {
    this->debugName = debugName ? debugName : std::string();
    return *this;
}

GraphicsPipelineSetup& GraphicsPipelineSetup::vkSetCreateInfoExtPtr(void* pNext) {
    this->pNext = pNext;
    return *this;
}

}
