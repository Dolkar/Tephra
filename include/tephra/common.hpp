#pragma once

#include "../interface_glue.hpp"
#include <tephra/tools/array.hpp>
#include <tephra/vulkan/structures.hpp>
#include <tephra/vulkan/types.hpp>
#include <tephra/vulkan/enums.hpp>
#include <tephra/vulkan/handles.hpp>
#include <tephra/vulkan/header.hpp>
#include <tephra/macros.hpp>
#include <string>
#include <memory>
#include <cstdint>
#include <cstddef>

namespace tp {

class Application;
struct DeviceQueue;
class Device;
class PhysicalDevice;
class ShaderModule;
class Pipeline;

class DebugTarget;

// Internal class for storing DebugTarget without a full definition
class DebugTargetPtr {
public:
    explicit DebugTargetPtr(DebugTarget debugTarget);

    DebugTarget* get();
    const DebugTarget* get() const;

    DebugTarget* operator->();
    const DebugTarget* operator->() const;

    TEPHRA_MAKE_MOVABLE(DebugTargetPtr);
    TEPHRA_MAKE_NONCOPYABLE(DebugTargetPtr);
    ~DebugTargetPtr();

private:
    std::unique_ptr<DebugTarget> ptr;
};

}
