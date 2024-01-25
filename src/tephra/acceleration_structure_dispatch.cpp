#include "common_impl.hpp"
#include <tephra/acceleration_structure.hpp>

#include "acceleration_structure_impl.hpp"
#include "job/local_acceleration_structures.hpp"

namespace tp {

AccelerationStructureView::AccelerationStructureView(AccelerationStructureImpl* persistentAccelerationStructure)
    : accelerationStructure(persistentAccelerationStructure) {}

AccelerationStructureView::AccelerationStructureView(JobLocalAccelerationStructureImpl* jobLocalAccelerationStructure)
    : accelerationStructure(jobLocalAccelerationStructure) {}

DeviceAddress AccelerationStructureView::getDeviceAddress() const {
    if (viewsJobLocalAccelerationStructure()) {
        return std::get<JobLocalAccelerationStructureImpl*>(accelerationStructure)->getDeviceAddress_();
    } else if (!isNull()) {
        return std::get<AccelerationStructureImpl*>(accelerationStructure)->getDeviceAddress_();
    } else {
        return {};
    }
}

BufferView AccelerationStructureView::getBackingBufferView() const {
    if (viewsJobLocalAccelerationStructure()) {
        return std::get<JobLocalAccelerationStructureImpl*>(accelerationStructure)->getBackingBufferView();
    } else if (!isNull()) {
        return std::get<AccelerationStructureImpl*>(accelerationStructure)->getBackingBufferView_();
    } else {
        return {};
    }
}

VkAccelerationStructureHandleKHR AccelerationStructureView::vkGetAccelerationStructureHandle() const {
    if (viewsJobLocalAccelerationStructure()) {
        return std::get<JobLocalAccelerationStructureImpl*>(accelerationStructure)->vkGetAccelerationStructureHandle_();
    } else if (!isNull()) {
        return std::get<AccelerationStructureImpl*>(accelerationStructure)->vkGetAccelerationStructureHandle_();
    } else {
        return {};
    }
}

bool tp::operator==(const AccelerationStructureView& lhs, const AccelerationStructureView& rhs) {
    return lhs.accelerationStructure == rhs.accelerationStructure;
}

const AccelerationStructureView AccelerationStructure::getView() const {
    auto asImpl = static_cast<const AccelerationStructureImpl*>(this);
    return asImpl->getView_();
}

DeviceAddress AccelerationStructure::getDeviceAddress() const {
    auto asImpl = static_cast<const AccelerationStructureImpl*>(this);
    return asImpl->getDeviceAddress_();
}

const BufferView AccelerationStructure::getBackingBufferView() const {
    auto asImpl = static_cast<const AccelerationStructureImpl*>(this);
    return asImpl->getBackingBufferView_();
}

VkAccelerationStructureHandleKHR AccelerationStructure::vkGetAccelerationStructureHandle() const {
    auto asImpl = static_cast<const AccelerationStructureImpl*>(this);
    return asImpl->vkGetAccelerationStructureHandle_();
}

AccelerationStructureSetup AccelerationStructureSetup::TopLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    InstanceGeometrySetup instanceGeometry) {
    return { AccelerationStructureType::TopLevel, buildFlags, instanceGeometry, {}, {} };
}

AccelerationStructureSetup AccelerationStructureSetup::BottomLevel(
    AccelerationStructureBuildFlagMask buildFlags,
    ArrayView<const TriangleGeometrySetup> triangleGeometries,
    ArrayView<const AABBGeometrySetup> aabbGeometries) {
    return {
        AccelerationStructureType::BottomLevel, buildFlags, InstanceGeometrySetup(0), triangleGeometries, aabbGeometries
    };
}

AccelerationStructureBuildInfo AccelerationStructureBuildInfo::TopLevel(
    AccelerationStructureBuildMode mode,
    AccelerationStructureView dstView,
    InstanceGeometryBuildInfo instanceGeometry,
    AccelerationStructureView srcView) {
    return { mode, dstView, instanceGeometry, {}, {}, srcView };
}

AccelerationStructureBuildInfo AccelerationStructureBuildInfo::BottomLevel(
    AccelerationStructureBuildMode mode,
    AccelerationStructureView dstView,
    ArrayView<const TriangleGeometryBuildInfo> triangleGeometries,
    ArrayView<const AABBGeometryBuildInfo> aabbGeometries,
    AccelerationStructureView srcView) {
    return { mode, dstView, InstanceGeometryBuildInfo({}), triangleGeometries, aabbGeometries, srcView };
}

}
