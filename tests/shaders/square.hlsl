
[[vk::binding(0)]]
Buffer<uint> inputBuffer;

[[vk::binding(1)]]
RWBuffer<uint> outputBuffer;

[numthreads(128, 1, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
    uint value = inputBuffer[threadID.x];
    outputBuffer[threadID.x] = value * value;
}
