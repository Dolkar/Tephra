#pragma once

#include "../common_impl.hpp"
#include "../acceleration_structure_impl.hpp"
#include <tephra/acceleration_structure.hpp>

namespace tp {

class JobLocalAccelerationStructureImpl : public AccelerationStructureBaseImpl {
public:
    JobLocalAccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        AccelerationStructureBuilder builder,
        BufferView backingBufferView,
        DebugTarget debugTarget);

    BufferView getBackingBufferView_() const {
        return backingBufferView;
    }

    bool hasHandle() const {
        return !accelerationStructureHandle.isNull();
    }

    static JobLocalAccelerationStructureImpl& getAccelerationStructureImpl(const AccelerationStructureView& asView);

private:
    DebugTarget debugTarget;
    BufferView backingBufferView; // Expected to be a job-local buffer
};

}
