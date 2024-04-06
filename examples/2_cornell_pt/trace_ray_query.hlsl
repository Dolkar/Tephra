#include "trace_shared.h"

[[vk::binding(0)]] RaytracingAccelerationStructure accelerationStructure;

[[vk::binding(1)]] StructuredBuffer<PlaneMaterialData> planeData;

[[vk::binding(2)]] RWTexture2D<float4> outputImage;

[[vk::push_constant]] PushConstantData pushConstants;

[numthreads(WorkgroupSizeDim, WorkgroupSizeDim, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    if (threadID.x < pushConstants.imageWidth && threadID.y < pushConstants.imageHeight) {
        float2 uv = (threadID.xy + 0.5f) / float2(pushConstants.imageWidth, pushConstants.imageHeight);

        RayDesc ray;
        ray.Origin = pushConstants.cameraPosition;
        ray.Direction = normalize(float3((0.5f - uv) * pushConstants.cameraFovTan, 1.0f));
        ray.TMin = 0.0f;
        ray.TMax = 10000.0f;

        RayQuery<
            RAY_FLAG_FORCE_OPAQUE |
            RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;

        query.TraceRayInline(accelerationStructure, 0, 0xff, ray);
        query.Proceed();

        int index = -1;
        float t = ray.TMax;
        float2 bary = 0.0f;

        if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
            index = query.CommittedInstanceIndex();
            bary = query.CommittedTriangleBarycentrics();
            t = query.CommittedRayT();
        }

        float4 color;
        if (index == -1)
            color = float4(1.0f, 0.0f, 1.0f, 1.0f);
        else
            color = float4(bary, 0.0f, 1.0f);
            //color = float4(index / 8.0f, t / 2000.0f, 0.0f, 1.0f);

        outputImage[threadID.xy] = color;
    }
}
