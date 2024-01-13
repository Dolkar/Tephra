#pragma once

#include "job_data.hpp"
#include "accesses.hpp"
#include "../common_impl.hpp"
#include "../vulkan/interface.hpp"

namespace tp {

// Handles recording to primary command buffers, creating them and chaining them on demand
class PrimaryBufferRecorder {
public:
    PrimaryBufferRecorder(
        CommandPool* commandPool,
        const VulkanCommandInterface* vkiCommands,
        const char* debugName,
        ScratchVector<VkCommandBufferHandle>* vkCommandBuffers);

    const VulkanCommandInterface& getVkiCommands() const {
        return *vkiCommands;
    }

    std::size_t getCommandBufferCount() const {
        return vkCommandBuffers->size();
    }

    // Request the last primary command buffer to record a command to it
    VkCommandBufferHandle requestBuffer();

    // Append an existing primary command buffer
    void appendBuffer(VkCommandBufferHandle vkNewBuffer);

    // End recording, closing the current buffer if needed
    void endRecording();

private:
    CommandPool* commandPool;
    const VulkanCommandInterface* vkiCommands;
    const char* debugName;
    ScratchVector<VkCommandBufferHandle>* vkCommandBuffers;
    // Current buffer being recorded to or null
    VkCommandBufferHandle vkCurrentBuffer;
};

template <typename T>
T* getCommandData(JobRecordStorage::CommandMetadata* command) {
    return reinterpret_cast<T*>(command + 1);
}

template <typename T>
const T* getCommandData(const JobRecordStorage::CommandMetadata* command) {
    return reinterpret_cast<const T*>(command + 1);
}

void identifyCommandResourceAccesses(
    JobRecordStorage::CommandMetadata* command,
    ScratchVector<NewBufferAccess>& bufferAccesses,
    ScratchVector<NewImageAccess>& imageAccesses);

// Records a Tephra command to primary Vulkan command buffers. The command data may be consumed by this operation
void recordCommand(PrimaryBufferRecorder& recorder, JobRecordStorage::CommandMetadata* command);

}
