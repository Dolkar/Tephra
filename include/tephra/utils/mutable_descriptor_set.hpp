#pragma once

#include <tephra/tephra.hpp>
#include <vector>

namespace tp {
namespace utils {

    /// A mutable variant of tp::DescriptorSet. Updating a Vulkan descriptor set after binding it is not allowed by
    /// default. This class maintains a state of all its descriptors so they can be set one at a time. By calling
    /// tp::utils::MutableDescriporSet::commit, a new descriptor set will be created based on the current state.
    /// Any following `set` calls won't disturb the already created descriptor set.
    class MutableDescriptorSet {
    public:
        /// @param device
        ///     The Tephra device used.
        /// @param layout
        ///     The layout of this descriptor set.
        /// @param debugName
        ///     The debug name of this descriptor set.
        MutableDescriptorSet(tp::Device* device, const tp::DescriptorSetLayout& layout, const char* debugName = nullptr);

        /// Returns the currently set (uncommitted) descriptor that was previously. Can be used
        /// to avoid spurious calls to tp::utils::MutableDescriptorSet::set or
        /// tp::utils::MutableDescriptorSet::setImmediate.
        /// @param descriptorIndex
        ///     The index of the descriptor in the descriptor set. This is the index it would have in the
        ///     `descriptors` array of tp::DescriptorSetSetup.
        /// @remarks
        ///     Any previously set tp::FutureDescriptor will only be resolved after
        ///     tp::utils::MutableDescriptorSet::commit gets called.
        const tp::Descriptor& get(uint32_t descriptorIndex) const;

        /// Sets a resource descriptor to the given index. The change will have no effect except for
        /// tp::utils::MutableDescriptorSet::get calls until tp::utils::MutableDescriptorSet::commit gets called.
        /// @param descriptorIndex
        ///     The index of the descriptor in the descriptor set. This is the index it would have in the
        ///     `descriptors` array of tp::DescriptorSetSetup.
        /// @param descriptor
        ///     The descriptor to be set to this index.
        void set(uint32_t descriptorIndex, tp::Descriptor descriptor);

        /// Sets a resource descriptor to the given index. The change will have no effect until
        /// tp::utils::MutableDescriptorSet::commit gets called. This overload is used for binding job-local resources
        /// before the tp::Job gets enqueued.
        /// @param descriptorIndex
        ///     The index of the descriptor in the descriptor set. This is the index it would have in the `descriptors`
        ///     array of tp::DescriptorSetSetup.
        /// @param descriptor
        ///     The future descriptor to be set to this index.
        void set(uint32_t descriptorIndex, tp::FutureDescriptor descriptor);

        /// Sets resource descriptors starting at the given index, immediately and retroactively updating the last
        /// committed descriptor set. This function is intended to be used with a "bindless" descriptor setup.
        /// @param firstDescriptorIndex
        ///     The index of the first descriptor to update. This is the index it would have in the `descriptors`
        ///     array of tp::DescriptorSetSetup.
        /// @param descriptors
        ///     The array of descriptors to be set.
        /// @remarks
        ///     Note that this will retroactively change the contents of the last committed descriptor set. If it has
        ///     already been bound, the tp::DescriptorBindingFlag::UpdateAfterBind flag must be set to the relevant
        ///     bindings. If the affected range was used by a tp::Job that has already been submitted, then that job
        ///     must have already finished executing on the device.
        /// @remarks
        ///     The provided descriptors must *not* be null and they must all be set to the same binding as defined by
        ///     this set's tp::DescriptorSetLayout.
        void setImmediate(uint32_t firstDescriptorIndex, tp::ArrayParameter<const tp::Descriptor> descriptors);

        /// Transfers over the given mutable descriptor set's current state of descriptors. Does not commit.
        /// @param other
        ///     The mutable descriptor set to transfer the descriptor state from. This set must use the same layout.
        void copyDescriptors(const MutableDescriptorSet& other);

        /// Processes the changes to the descriptors, resolving any tp::FutureDescriptor. Returns a view to a
        /// tp::DescriptorSet with the current state of the descriptors, allocating a new one from the given pool if
        /// needed.
        /// @param pool
        ///     The descriptor pool to allocate from.
        /// @remarks
        ///     All resources referenced by any of the previously set tp::FutureDescriptor must be ready.
        ///     For job-local resources it means this can only be called after the tp::Job has been enqueued.
        /// @remarks
        ///     If both a tp::Descriptor and a tp::FutureDescriptor were set to the same descriptorIndex since the last
        ///     commit, the tp::FutureDescriptor will be prioritized.
        tp::DescriptorSetView commit(tp::DescriptorPool& pool);

        /// Returns the last allocated descriptor set from the last tp::utils::MutableDescriptorSet::commit call.
        tp::DescriptorSetView getLastCommittedView() const {
            return allocatedSets.empty() ? tp::DescriptorSetView() : allocatedSets.back().getView();
        }

        /// Returns `true` if any descriptors have been set since the last tp::utils::MutableDescriptorSet::commit call.
        bool hasPendingChanges() const {
            return changesPending;
        }

        /// Resets the state of the descriptors without releasing any previously allocated descriptor sets.
        void reset();

        /// Releases all descriptor sets and resets the state of the descriptors.
        void releaseAndReset();

    private:
        tp::DebugTargetPtr debugTarget;
        tp::Device* device;
        PFN_vkUpdateDescriptorSets vkUpdateDescriptorSets;
        tp::DescriptorSetLayout layout;
        std::vector<uint32_t> bindingDescriptorOffsets;
        bool changesPending;
        bool needsResolve;
        std::vector<tp::DescriptorSet> allocatedSets;
        std::vector<tp::Descriptor> currentDescriptors;
        std::vector<tp::FutureDescriptor> futureDescriptors;

        void doResolve();
        // returns the binding and descriptor offset into that binding
        std::pair<const DescriptorBinding*, uint32_t> findDescriptorBinding(std::size_t descriptorIndex) const;
    };

}
}
