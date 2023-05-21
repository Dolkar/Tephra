#pragma once

#include "command_recording.hpp"
#include "../device/cross_queue_sync.hpp"
#include "../device/device_container.hpp"
#include "../common_impl.hpp"

namespace tp {

struct JobCompilationContext {
    DeviceContainer* deviceImpl;
    QueueSyncState* queueSyncState;
    PrimaryBufferRecorder* recorder;
};

void compileJob(
    JobCompilationContext& context,
    const Job& job,
    ArrayParameter<const CrossQueueSync::ExportEntry> incomingExports);

}
