#pragma once

#include "../common_impl.hpp"
#include "../acceleration_structure_impl.hpp"
#include "local_buffers.hpp"
#include <tephra/acceleration_structure.hpp>

namespace tp {

class JobLocalAccelerationStructureImpl : public AccelerationStructureBaseImpl {
public:
    JobLocalAccelerationStructureImpl(
        DeviceContainer* deviceImpl,
        AccelerationStructureBuilder* builder,
        BufferView backingBufferView,
        DebugTarget debugTarget);

    AccelerationStructureView getView() {
        return AccelerationStructureView(this);
    }

    const BufferView getBackingBufferView() const {
        return backingBufferView;
    }

    bool hasHandle() const {
        return !accelerationStructureHandle.isNull();
    }

    void assignHandle(VkAccelerationStructureHandleKHR vkAccelerationStructureHandle) {
        accelerationStructureHandle = Lifeguard<VkAccelerationStructureHandleKHR>::NonOwning(
            vkAccelerationStructureHandle);
    }

    AccelerationStructureBuilder* getBuilder() {
        return builder;
    }

    static JobLocalAccelerationStructureImpl& getAccelerationStructureImpl(const AccelerationStructureView& asView);

private:
    DebugTarget debugTarget;
    BufferView backingBufferView; // Always a job-local buffer
    AccelerationStructureBuilder* builder;
};

// Once stored, it is not guaranteed that the persistent parent objects (AccelerationStructureImpl) of views will be
// kept alive, so they need to be resolved immediately. But job-local resources need to be resolved later after they
// actually get created. This class handles resolving both at the right time.
class StoredAccelerationStructureView {
public:
    StoredAccelerationStructureView(const AccelerationStructureView& view)
        : storedView(store(view)), storedBackingBufferView(view.getBackingBufferView()) {}

    bool isNull() const {
        return storedBackingBufferView.isNull();
    }

    DeviceAddress getDeviceAddress() {
        resolve();
        return std::get<ResolvedView>(storedView).deviceAddress;
    }

    StoredBufferView& getBackingBufferView() {
        return storedBackingBufferView;
    }

    VkAccelerationStructureHandleKHR vkGetAccelerationStructureHandle() {
        resolve();
        return std::get<ResolvedView>(storedView).vkAccelerationStructureHandle;
    }

private:
    struct ResolvedView {
        DeviceAddress deviceAddress;
        VkAccelerationStructureHandleKHR vkAccelerationStructureHandle;

        explicit ResolvedView(const AccelerationStructureView& view) {
            deviceAddress = view.getDeviceAddress();
            vkAccelerationStructureHandle = view.vkGetAccelerationStructureHandle();
        }
    };

    static std::variant<ResolvedView, AccelerationStructureView> store(const AccelerationStructureView& view) {
        if (!view.viewsJobLocalAccelerationStructure()) {
            return ResolvedView(view);
        } else {
            return view;
        }
    }

    void resolve() {
        if (std::holds_alternative<AccelerationStructureView>(storedView)) {
            storedView = ResolvedView(std::get<AccelerationStructureView>(storedView));
            TEPHRA_ASSERTD(
                !std::get<ResolvedView>(storedView).vkAccelerationStructureHandle.isNull(),
                "Job-local acceleration structures must be resolvable at this point");
        }
    }

    StoredBufferView storedBackingBufferView;
    std::variant<ResolvedView, AccelerationStructureView> storedView;
};

// We need a version of AccelerationStructureBuildInfo and friends with stored buffer views
struct StoredInstanceGeometryBuildInfo {
    StoredBufferView instanceBuffer;
    bool arrayOfPointers;
    ArrayView<StoredAccelerationStructureView> accessedViews;

    StoredInstanceGeometryBuildInfo(
        const InstanceGeometryBuildInfo& info,
        ArrayView<StoredAccelerationStructureView> accessedViewData)
        : instanceBuffer(info.instanceBuffer), arrayOfPointers(info.arrayOfPointers), accessedViews(accessedViewData) {}
};

struct StoredTriangleGeometryBuildInfo {
    StoredBufferView vertexBuffer;
    uint64_t vertexStride;
    StoredBufferView indexBuffer;
    uint32_t firstVertex;
    StoredBufferView transformBuffer;

    StoredTriangleGeometryBuildInfo(const TriangleGeometryBuildInfo& info)
        : vertexBuffer(info.vertexBuffer),
          vertexStride(info.vertexStride),
          indexBuffer(info.indexBuffer),
          firstVertex(info.firstVertex),
          transformBuffer(info.transformBuffer) {}
};

struct StoredAABBGeometryBuildInfo {
    StoredBufferView aabbBuffer;
    uint64_t stride;

    StoredAABBGeometryBuildInfo(const AABBGeometryBuildInfo& info) : aabbBuffer(info.aabbBuffer), stride(info.stride) {}
};

struct StoredAccelerationStructureBuildInfo {
    AccelerationStructureBuildMode mode;
    StoredAccelerationStructureView dstView;
    StoredInstanceGeometryBuildInfo instanceGeometry;
    ArrayView<StoredTriangleGeometryBuildInfo> triangleGeometries;
    ArrayView<StoredAABBGeometryBuildInfo> aabbGeometries;
    StoredAccelerationStructureView srcView;

    StoredAccelerationStructureBuildInfo(
        const AccelerationStructureBuildInfo& info,
        ArrayView<StoredAccelerationStructureView> accessedViewData,
        ArrayView<StoredTriangleGeometryBuildInfo> triangleGeometriesData,
        ArrayView<StoredAABBGeometryBuildInfo> aabbGeometriesData)
        : mode(info.mode),
          dstView(info.dstView),
          instanceGeometry(info.instanceGeometry, accessedViewData),
          triangleGeometries(triangleGeometriesData),
          aabbGeometries(aabbGeometriesData),
          srcView(info.srcView) {}
};

struct StoredAccelerationStructureBuildIndirectInfo {
    ArrayView<uint32_t> maxPrimitiveCounts;
    StoredBufferView buildRangeBuffer;
    uint32_t buildRangeStride;

    StoredAccelerationStructureBuildIndirectInfo(
        const AccelerationStructureBuildIndirectInfo& info,
        ArrayView<uint32_t> maxPrimitiveCounts)
        : maxPrimitiveCounts(maxPrimitiveCounts),
          buildRangeBuffer(info.buildRangeBuffer),
          buildRangeStride(info.buildRangeStride) {}
};

// Caching of AccelerationStructure handles - they depend on buffer, offset, size and type
// Caching of AccelerationStructureBuilders - can be reused arbitrarily after submit

class JobLocalAccelerationStructures {
public:
    explicit JobLocalAccelerationStructures(DeviceContainer* deviceImpl) : deviceImpl(deviceImpl) {}

    AccelerationStructureView acquireNew(
        AccelerationStructureBuilder* builder,
        BufferView backingBufferView,
        DebugTarget debugTarget);

    void clear();

private:
    friend class JobLocalAccelerationStructureAllocator;

    DeviceContainer* deviceImpl;
    std::deque<JobLocalAccelerationStructureImpl> accelerationStructures;
};

}
