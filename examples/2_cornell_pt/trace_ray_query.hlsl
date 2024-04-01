#include "trace_shared.h"

[[vk::binding(0)]] StructuredBuffer<PlaneMaterialData> planeData;

[[vk::binding(1)]] RWTexture2D<float4> outputImage;

[[vk::push_constant]] PushConstantData pushConstants;

[numthreads(WorkgroupSizeDim, WorkgroupSizeDim, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    if (threadID.x < pushConstants.imageWidth && threadID.y < pushConstants.imageHeight) {
        float2 v = float2(threadID.xy % 64) / 64.0;
        outputImage[threadID.xy] = float4(v.x, v.y, 0.0, 1.0);
    }
}
