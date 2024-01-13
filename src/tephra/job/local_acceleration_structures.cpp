#include "local_acceleration_structures.hpp"

namespace tp {

JobLocalAccelerationStructureImpl::JobLocalAccelerationStructureImpl(
    DeviceContainer* deviceImpl,
    AccelerationStructureBuilder builder,
    BufferView backingBufferView,
    DebugTarget debugTarget)
    : AccelerationStructureBaseImpl(deviceImpl, std::move(builder), {}), debugTarget(std::move(debugTarget)) {}

JobLocalAccelerationStructureImpl& JobLocalAccelerationStructureImpl::getAccelerationStructureImpl(
    const AccelerationStructureView& asView) {
    TEPHRA_ASSERT(asView.viewsJobLocalAccelerationStructure());
    return *std::get<JobLocalAccelerationStructureImpl*>(asView.accelerationStructure);
}

}
