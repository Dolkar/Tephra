
set(TEPHRA_SOURCES
    ${SOURCE_PATH}/tephra/application/application_dispatch.cpp
    ${SOURCE_PATH}/tephra/application/instance.cpp
    ${SOURCE_PATH}/tephra/application/vulkan_globals.cpp

    ${SOURCE_PATH}/tephra/device/command_pool.cpp
    ${SOURCE_PATH}/tephra/device/cross_queue_sync.cpp
    ${SOURCE_PATH}/tephra/device/device_dispatch.cpp
    ${SOURCE_PATH}/tephra/device/handle_lifeguard.cpp
    ${SOURCE_PATH}/tephra/device/logical_device.cpp
    ${SOURCE_PATH}/tephra/device/memory_allocator.cpp
    ${SOURCE_PATH}/tephra/device/queue_map.cpp
    ${SOURCE_PATH}/tephra/device/queue_state.cpp
    ${SOURCE_PATH}/tephra/device/timeline_manager.cpp

    ${SOURCE_PATH}/tephra/job/accesses.cpp
    ${SOURCE_PATH}/tephra/job/aliasing_suballocator.cpp
    ${SOURCE_PATH}/tephra/job/barriers.cpp
    ${SOURCE_PATH}/tephra/job/command_recording.cpp
    ${SOURCE_PATH}/tephra/job/compute_pass.cpp
    ${SOURCE_PATH}/tephra/job/job.cpp
    ${SOURCE_PATH}/tephra/job/job_compile.cpp
    ${SOURCE_PATH}/tephra/job/job_data.cpp
    ${SOURCE_PATH}/tephra/job/local_buffers.cpp
    ${SOURCE_PATH}/tephra/job/local_buffer_allocator.cpp
    ${SOURCE_PATH}/tephra/job/local_descriptor_sets.cpp
    ${SOURCE_PATH}/tephra/job/local_images.cpp
    ${SOURCE_PATH}/tephra/job/local_image_allocator.cpp
    ${SOURCE_PATH}/tephra/job/preinit_buffer_allocator.cpp
    ${SOURCE_PATH}/tephra/job/render_pass.cpp
    ${SOURCE_PATH}/tephra/job/resource_pool_dispatch.cpp

    ${SOURCE_PATH}/tephra/utils/growable_ring_buffer.cpp
    ${SOURCE_PATH}/tephra/utils/mutable_descriptor_set.cpp
    ${SOURCE_PATH}/tephra/utils/standard_report_handler.cpp

    ${SOURCE_PATH}/tephra/vulkan/interface.cpp
    ${SOURCE_PATH}/tephra/vulkan/loader.cpp

    ${SOURCE_PATH}/tephra/buffer_dispatch.cpp
    ${SOURCE_PATH}/tephra/buffer_impl.cpp
    ${SOURCE_PATH}/tephra/command_list.cpp
    ${SOURCE_PATH}/tephra/compute.cpp
    ${SOURCE_PATH}/tephra/debugging.cpp
    ${SOURCE_PATH}/tephra/debug_reporter.cpp
    ${SOURCE_PATH}/tephra/descriptor.cpp
    ${SOURCE_PATH}/tephra/descriptor_pool_impl.cpp
    ${SOURCE_PATH}/tephra/errors.cpp
    ${SOURCE_PATH}/tephra/image_dispatch.cpp
    ${SOURCE_PATH}/tephra/image_impl.cpp
    ${SOURCE_PATH}/tephra/memory.cpp
    ${SOURCE_PATH}/tephra/physical_device.cpp
    ${SOURCE_PATH}/tephra/pipeline.cpp
    ${SOURCE_PATH}/tephra/pipeline_builder.cpp
    ${SOURCE_PATH}/tephra/render.cpp
    ${SOURCE_PATH}/tephra/swapchain_impl.cpp
)

set(VMA_SOURCES
    ${SOURCE_PATH}/vma/vk_mem_alloc.cpp
)

set(TEPHRA_LIB_SOURCES
    ${TEPHRA_SOURCES}
    ${VMA_SOURCES}
)

if(MSVC)
    list(APPEND TEPHRA_LIB_SOURCES ${CMAKE_SOURCE_DIR}/build/NatvisFile.natvis)
endif()

add_library(Tephra ${TEPHRA_LIB_SOURCES})

target_include_directories(Tephra
    PUBLIC
    ${HEADER_PATH}
    Vulkan::Vulkan
)

target_link_libraries(Tephra PUBLIC Vulkan::Vulkan)

target_compile_definitions(Tephra
    PUBLIC
    $<$<CONFIG:Debug>:TEPHRA_ENABLE_DEBUG>
)

target_compile_features(Tephra PUBLIC cxx_std_17)
