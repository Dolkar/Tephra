
#if defined(__cplusplus)
    #include <cstdint>
#endif

#if defined(__cplusplus)

struct Vector {
    float x;
    float y;
    float z;
};

using Point = Vector;

struct Color {
    float r;
    float g;
    float b;
};

#elif defined(__HLSL_VERSION)

using uint32_t = uint;
using Vector = float3;
using Point = float3;
using Color = float3;

#endif

struct PlaneMaterialData {
    Vector normal;
    Color reflectance;
    Color emission;
};

struct PushConstantData {
    Vector cameraPosition;
    float cameraFovTan;
    uint32_t samplesPerPixel;
    uint32_t frameIndex;
    uint32_t imageWidth;
    uint32_t imageHeight;
};

static const uint32_t WorkgroupSizeDim = 8;
static const uint32_t MaxPlanesPerInstance = 8;
