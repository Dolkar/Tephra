#include "trace_shared.h"

static const float Pi = 3.14159265f;

[[vk::binding(0)]] RaytracingAccelerationStructure accelerationStructure;

[[vk::binding(1)]] StructuredBuffer<PlaneMaterialData> planeData;

[[vk::binding(2)]] RWTexture2D<float4> outputImage;

[[vk::push_constant]] PushConstantData pushConstants;

// 32-bit PCG variant from https://www.reedbeta.com/blog/hash-functions-for-gpu-rendering/
struct PCGRand {
    uint state;

    void init(uint4 seed) {
        state = seed.x;
        advance();
        state += seed.y;
        advance();
        state += seed.z;
        advance();
        state += seed.w;
    }

    void advance() {
        state = state * 747796405u + 2891336453u;
    }

    float next() {
        uint word = ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;
        word = (word >> 22u) ^ word;
        advance();

        // Convert to uniform float [0, 1)
        return (word >> 8u) * asfloat(0x33800000u);
    }
};

float3 sampleCosineHemisphere(float3 n, inout PCGRand rand) {
    // Random point on a sphere
    float z = 1.0f - 2.0f * rand.next();
    float r = sqrt(1.0f - z * z);
    float phi = 2.0f * Pi * rand.next();
    float3 randSphere = float3(r * cos(phi), r * sin(phi), z);

    return normalize(n + randSphere);
}

struct RayHit {
    int planeIndex;
    float t;

    bool HitScene() {
        return planeIndex >= 0;
    }

    PlaneMaterialData GetMaterialData() {
        return planeData[planeIndex];
    }
};

RayHit traceRay(float3 origin, float3 direction) {
    RayDesc ray;
    ray.Origin = origin;
    ray.Direction = direction;
    ray.TMin = 0.0f;
    ray.TMax = 10000.0f;

    RayQuery<RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_SKIP_PROCEDURAL_PRIMITIVES> query;

    query.TraceRayInline(accelerationStructure, 0, 0xff, ray);
    query.Proceed();

    RayHit rayHit;
    rayHit.planeIndex = -1;
    rayHit.t = ray.TMax;

    if (query.CommittedStatus() == COMMITTED_TRIANGLE_HIT) {
        int instanceIndex = query.CommittedInstanceIndex();
        int primIndex = query.CommittedPrimitiveIndex();
        rayHit.planeIndex = instanceIndex * MaxPlanesPerInstance + primIndex / 2;
        rayHit.t = query.CommittedRayT();
    }

    return rayHit;
}

float3 tracePath(float3 rayOrigin, float3 rayDirection, inout PCGRand rand) {
    static const int MaxDepth = 16;

    float3 radiance = 0.0f; // Accumulated radiance along the primary ray
    float3 throughput = 1.0f; // Current path throughput

    for (int depth = 0; depth < MaxDepth + 1; depth++) {
        RayHit hit = traceRay(rayOrigin, rayDirection);
        if (!hit.HitScene())
            break;

        PlaneMaterialData plane = hit.GetMaterialData();

        // Add emission and update throughput with diffuse BRDF (division by pi cancels out with PDF)
        radiance += plane.emission * throughput;
        float3 diffuse = plane.reflectance;
        throughput *= diffuse;

        if (depth < MaxDepth) {
            // Form a secondary ray to importance sample the diffuse BRDF in the next iteration
            float3 hitPosition = rayOrigin + rayDirection * hit.t;
            rayOrigin = hitPosition + plane.normal * 1.0e-3f;
            rayDirection = sampleCosineHemisphere(plane.normal, rand);
        }
    }

    return radiance;
}

[numthreads(WorkgroupSizeDim, WorkgroupSizeDim, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    if (threadID.x < pushConstants.imageWidth && threadID.y < pushConstants.imageHeight) {
        float3 accumColor = 0.0f;

        for (uint sample = 0; sample < pushConstants.samplesPerPixel; sample++) {
            PCGRand rand;
            rand.init(uint4(threadID.xy, pushConstants.frameIndex, sample));

            // Randomize sample position to integrate over the pixel's area
            float2 samplePos = threadID.xy + float2(rand.next(), rand.next());
            float2 sampleUV = samplePos / float2(pushConstants.imageWidth, pushConstants.imageHeight);

            // Setup primary ray
            float3 rayOrigin = pushConstants.cameraPosition;
            float3 rayDirection = normalize(float3((0.5f - sampleUV) * pushConstants.cameraFovTan, 1.0f));

            // Trace and accumulate
            float3 sampleColor = tracePath(rayOrigin, rayDirection, rand);
            accumColor += sampleColor / pushConstants.samplesPerPixel;
        }

        // Also accumulate over previous frames
        if (pushConstants.frameIndex > 0) {
            float3 prevColor = outputImage[threadID.xy].rgb;
            accumColor = (accumColor + prevColor * pushConstants.frameIndex) / (pushConstants.frameIndex + 1.0f);
        }

        outputImage[threadID.xy] = float4(accumColor, 1.0f);
    }
}
