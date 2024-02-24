
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
};

static const uint32_t WorkgroupSizeDim = 8;
static const Vector CameraPosition = { 278.0f, 273.0f, -800.0f };
