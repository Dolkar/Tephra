#include "local_acceleration_structures.hpp"

namespace tp {

JobLocalAccelerationStructureImpl::JobLocalAccelerationStructureImpl(
    DeviceContainer* deviceImpl,
    AccelerationStructureBuilder* builder,
    BufferView backingBufferView,
    DebugTarget debugTarget)
    : AccelerationStructureBaseImpl(deviceImpl, {}),
      debugTarget(std::move(debugTarget)),
      backingBufferView(backingBufferView),
      builder(builder) {}

JobLocalAccelerationStructureImpl& JobLocalAccelerationStructureImpl::getAccelerationStructureImpl(
    const AccelerationStructureView& asView) {
    TEPHRA_ASSERT(asView.viewsJobLocalAccelerationStructure());
    return *std::get<JobLocalAccelerationStructureImpl*>(asView.accelerationStructure);
}

AccelerationStructureView JobLocalAccelerationStructures::acquireNew(
    AccelerationStructureBuilder* builder,
    BufferView backingBufferView,
    DebugTarget debugTarget) {
    accelerationStructures.emplace_back(deviceImpl, builder, backingBufferView, std::move(debugTarget));
    return accelerationStructures.back().getView();
}

void JobLocalAccelerationStructures::clear() {
    accelerationStructures.clear();
}

}
