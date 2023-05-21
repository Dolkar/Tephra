#pragma once
/// @file
/// The main file that includes the core library.

#ifdef DOXYGEN
    /// Enables all Tephra-specific debug functionality. Equivalent to defining all of
    /// #TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION, #TEPHRA_ENABLE_DEBUG_ASSERTS, #TEPHRA_ENABLE_DEBUG_CONTEXTS,
    /// #TEPHRA_ENABLE_DEBUG_NAMES and #TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
    /// @remarks
    ///     It is recommended to enable this macro during development. Enabling it may bring a performance loss.
    #define TEPHRA_ENABLE_DEBUG

    /// Enables validating correct usage of the library.
    /// @remarks
    ///     To receive these messages, tp::ApplicationSetup::debugReportHandler must be provided.
    /// @remarks
    ///     The validation is currently incomplete. A lack of validation messages doesn't mean the library is being used
    ///     correctly. Enabling asserts (#TEPHRA_ENABLE_DEBUG_ASSERTS) may catch more cases.
    /// @remarks
    ///     This is independent of Vulkan validation, which can be enabled at runtime through tp::VulkanValidationSetup.
    #define TEPHRA_ENABLE_DEBUG_TEPHRA_VALIDATION

    /// Enables internal debug asserts.
    /// @remarks
    ///     To receive these messages, tp::ApplicationSetup::debugReportHandler must be provided.
    /// @remarks
    ///     A debug assert may currently be caused either by incorrect usage that isn't checked by the validation,
    ///     or by a bug in the library.
    #define TEPHRA_ENABLE_DEBUG_ASSERTS

    /// Enables the management and reporting of contexts in debug messages. These include information about the Tephra
    /// interface function call during which the message was generated.
    /// @remarks
    ///     Also generates verbose severity messages on each entered and left context.
    #define TEPHRA_ENABLE_DEBUG_CONTEXTS

    /// Enables storage of debug names of Tephra objects and using them for internal Vulkan handles when
    /// tp::ApplicationExtension::EXT_DebugUtils is enabled.
    /// @remarks
    ///     This functionality is fairly lightweight and shouldn't impact performance beyond string allocations.
    #define TEPHRA_ENABLE_DEBUG_NAMES

    /// Enables the reporting of certain statistics gathered during various stages of execution to a
    /// tp::DebugReportHandler.
    #define TEPHRA_ENABLE_DEBUG_STATISTIC_EVENTS
#endif

#include <tephra/application.hpp>
#include <tephra/device.hpp>
#include <tephra/physical_device.hpp>
#include <tephra/job.hpp>
#include <tephra/swapchain.hpp>
#include <tephra/pipeline.hpp>
#include <tephra/descriptor.hpp>
#include <tephra/compute.hpp>
#include <tephra/render.hpp>
#include <tephra/buffer.hpp>
#include <tephra/image.hpp>
#include <tephra/sampler.hpp>
#include <tephra/debug_handler.hpp>
#include <tephra/format_compatibility.hpp>
#include <tephra/format.hpp>
#include <tephra/version.hpp>
