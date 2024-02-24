#include "trace_shared.h"

[[vk::binding(0)]] StructuredBuffer<PlaneMaterialData> planeData;

[[vk::binding(1)]] RWBuffer<float4> outputImage;

[[vk::push_constant]] PushConstantData pushConstants;

[numthreads(WorkgroupSizeDim, WorkgroupSizeDim, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    
}
