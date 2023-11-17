// Shader used in StreamingComputeTest

#include "distance_transform_shared.h"

[[vk::binding(0)]]
StructuredBuffer<DistanceValueType> inputBuffer;

[[vk::binding(1)]]
RWStructuredBuffer<DistanceValueType> outputBuffer;

[[vk::push_constant]]
PushConstantData pushConstants;

// 1D Distance transform:
// Given an input buffer of 0 or 1, fill an output buffer such that:
// - If the corresponding value in the input buffer is 1, the output value is 0
// - Otherwise, the output value is the signed distance to the closest element with value 1
// For example:
//  0   0   1   0   0   1   0
// Produces:
//  2   1   0  -1   1   0  -1
// 
// The implementation uses the simple jump-flooding algorithm
// The number of passes needed depends on the number of samples taken per pass:
// 3 samples  -> log2(n) passes
// 7 samples  -> log4(n) passes
// 15 samples -> log8(n) passes
// We'll choose the middle option

[numthreads(WorkgroupSize, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID) {
    int threadIndex = threadID.x;

    if (threadIndex >= pushConstants.inputSize)
        return;

    DistanceValueType closestDistance = MaxDistanceValue;
    [unroll]
    for (int i = -3; i <= 3; i++) {
        int offset = i * pushConstants.jumpSize;
        int sampleIndex = threadIndex + offset;
        if (sampleIndex < 0 || sampleIndex >= pushConstants.inputSize)
            continue;

        DistanceValueType sampleValue = inputBuffer[sampleIndex];
        if (pushConstants.passNumber == 0) {
            // Convert from 0 and 1 to distances
            sampleValue = sampleValue > 0 ? 0 : MaxDistanceValue;
        }

        DistanceValueType sampleDistance = sampleValue + offset;
        // In case of a tie, positive distance wins over negative
        if (abs(sampleDistance) < abs(closestDistance) ||
            (abs(sampleDistance) == abs(closestDistance) && sampleDistance > closestDistance))
            closestDistance = sampleDistance;
    }

    outputBuffer[threadIndex] = closestDistance;
}
