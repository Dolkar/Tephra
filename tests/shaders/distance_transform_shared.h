
#ifdef __cplusplus
    #include <cstdint>
#endif

static const uint32_t WorkgroupSize = 256;
static const uint32_t MaxInputSize = 1 << 30;
// The factor by which jump size should decrease each pass
static const int32_t JumpShrinkFactor = 4;

typedef int32_t DistanceValueType;
static const DistanceValueType MaxDistanceValue = MaxInputSize;

struct PushConstantData {
    int32_t inputSize;
    int32_t jumpSize;
    int32_t passNumber;
};
