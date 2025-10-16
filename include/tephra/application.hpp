#pragma once

#include <tephra/version.hpp>
#include <tephra/device.hpp>
#include <tephra/debug_handler.hpp>
#include <tephra/tools/structure_map.hpp>
#include <tephra/common.hpp>

/// Common Tephra namespace
namespace tp {

/// Collection of application extensions that are either specific to Tephra, or are Vulkan instance extensions with
/// built-in support in Tephra. Vulkan extensions outside of the ones defined here may be used, but their support may
/// be limited.
/// @see tp::ApplicationSetup
namespace ApplicationExtension {
    /// Allows the creation of presentable surfaces and the use of the tp::DeviceExtension::KHR_Swapchain extension
    /// on devices.
    ///
    /// When this extension is requested, any available platform specific surface extension is also enabled.
    ///
    /// @remarks
    ///     This extension does not expose any additional functionality in Tephra. The creation of @vksymbol{VkSurface}
    ///     objects is left up to the user as it is heavily platform dependent and there are already third party
    ///     libraries for handling this.
    /// @see @vksymbol{VK_KHR_surface}
    const char* const KHR_Surface = VK_KHR_SURFACE_EXTENSION_NAME;

    /// Allows using various Vulkan debug features.
    ///
    /// This extension allows Tephra to report messages from Vulkan layers to the user through the
    /// tp::DebugReportHandler interface. It also allows passing the user provided debug names and labels to the various
    /// Vulkan layers, where they may be visible in validation messages or in debuggers like RenderDoc.
    ///
    /// @see @vksymbol{VK_EXT_debug_utils}
    const char* const EXT_DebugUtils = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    /// Provides a mechanism to programmatically configure the behavior of enabled layers.
    ///
    /// @remarks
    ///     This is an extension implemented by Vulkan layers. To check if it is available, the name of the layer must
    ///     be passed to tp::Application::isExtensionAvailable, for example "VK_LAYER_KHRONOS_validation"
    /// @see @vksymbol{VK_EXT_layer_settings}
    const char* const EXT_LayerSettings = VK_EXT_LAYER_SETTINGS_EXTENSION_NAME;
}

/// Describes the application and engine, allowing the Vulkan driver to use this information for identification
/// purposes.
/// @see tp::ApplicationSetup
struct ApplicationIdentifier {
    const char* applicationName;
    Version applicationVersion;
    const char* engineName;
    Version engineVersion;

    /// @param applicationName
    ///     The name of the application.
    /// @param applicationVersion
    ///     The version of the application.
    /// @param engineName
    ///     The name of the application's engine that interfaces with Tephra.
    /// @param engineVersion
    ///     The version of the engine.
    /// @see @vksymbol{VkApplicationInfo}
    ApplicationIdentifier(
        const char* applicationName = nullptr,
        Version applicationVersion = {},
        const char* engineName = nullptr,
        Version engineVersion = {});
};

/// Used as configuration for creating a new tp::Application object.
/// @see tp::Application::createApplication
/// @see @vksymbol{VkInstanceCreateInfo}
struct ApplicationSetup {
    ApplicationIdentifier applicationIdentifier;
    DebugReportHandler* debugReportHandler;
    ArrayView<const char* const> extensions;
    ArrayView<const char* const> layers;
    Version apiVersion;
    ArrayView<VkLayerSettingEXT> layerSettingsEXT;
    void* vkCreateInfoExtPtr;

    /// @param applicationIdentifier
    ///     The optional identification of your application.
    /// @param debugReportHandler
    ///     If not `nullptr`, points to the object responsible for reporting debug and validation information.
    ///     If `nullptr`, reporting of debug information will be disabled.
    /// @param extensions
    ///     The set of application extensions or Vulkan instance extensions to enable. The extensions must be available,
    ///     as can be checked with tp::Application::isExtensionAvailable. See tp::ApplicationExtension.
    /// @param layers
    ///     The set of additional Vulkan instance layers to enable. The layers must be available, as can be checked with
    ///     tp::Application::isLayerAvailable.
    /// @param apiVersion
    ///     The highest Vulkan API version that this application will use. Only the major and minor versions are
    ///     considered. Should be equal or higher than tp::Version::getMaxUsedVulkanAPIVersion.
    /// @param layerSettingsEXT
    ///     Layer-specific settings that can be used to configure enabled layers.
    ///     Requires the tp::ApplicationExtension::EXT_LayerSettings extension to be among the list of extensions to
    ///     enable.
    /// @param vkCreateInfoExtPtr
    ///     A pointer to additional Vulkan structures to be passed in `pNext` of @vksymbol{VkInstanceCreateInfo}.
    ApplicationSetup(
        ApplicationIdentifier applicationIdentifier = {},
        DebugReportHandler* debugReportHandler = nullptr,
        ArrayView<const char* const> extensions = {},
        ArrayView<const char* const> layers = {},
        Version apiVersion = Version::getMaxUsedVulkanAPIVersion(),
        ArrayView<VkLayerSettingEXT> layerSettingsEXT = {},
        void* vkCreateInfoExtPtr = nullptr);
};

/// Represents the application using this library, storing all per-application state.
///
/// It can be used to gather information about instance-level capabilities and all available physical devices, out of
/// which Device objects can be created.
///
/// @remarks
///     Access to the tp::Application object is internally synchronized, meaning it is safe to operate on it from
///     multiple threads at the same time.
/// @see tp::Application::createApplication
class Application : public Ownable {
public:
    /// Returns the list of available physical devices on this platform.
    ArrayView<const PhysicalDevice> getPhysicalDevices() const;

    /// Creates a tp::Device object according to the given setup structure.
    /// @param deviceSetup
    ///     The setup structure describing the object.
    /// @param debugName
    ///     The debug name identifier for the object.
    OwningPtr<Device> createDevice(const DeviceSetup& deviceSetup, const char* debugName = nullptr);

    /// Returns the Vulkan @vksymbol{VkInstance} handle.
    VkInstanceHandle vkGetInstanceHandle() const;

    /// Loads a Vulkan instance procedure with the given name and returns a pointer to it, or `nullptr` if not
    /// successful.
    PFN_vkVoidFunction vkLoadInstanceProcedure(const char* procedureName) const;

    /// Returns the API version of the Vulkan runtime library (loader).
    static Version getApiVersion();

    /// Returns `true` when the provided tp::ApplicationExtension or Vulkan instance extension is available.
    /// @param extension
    ///     The name of the extension, see tp::ApplicationExtension or list of Vulkan instance extensions
    /// @param sourceLayer
    ///     If `nullptr`, checks if the extension is provided by the base Vulkan implementation or any implicitly
    ///     enabled layers. Otherwise, checks if the extensions is provided by the specified layer.
    static bool isExtensionAvailable(const char* extension, const char* sourceLayer = nullptr);

    /// Returns `true` when the provided Vulkan instance layer is available.
    static bool isLayerAvailable(const char* layer);

    /// Creates a tp::Application object according to the given setup structure.
    /// @param appSetup
    ///     The setup structure describing the object.
    static OwningPtr<Application> createApplication(const ApplicationSetup& appSetup);

    /// Creates a tp::Application object out of an existing Vulkan handle.
    /// @param vkInstanceHandle
    ///     The Vulkan @vksymbol{VkInstance} handle.
    /// @param debugReportHandler
    ///     If not `nullptr`, points to the object responsible for reporting debug and validation information.
    /// @remarks
    ///     The ownership of the instance handle remains with the user and it must not be destroyed during the lifetime
    ///     of the application.
    static OwningPtr<Application> createApplication(
        VkInstanceHandle vkInstanceHandle,
        DebugReportHandler* debugReportHandler);

    TEPHRA_MAKE_INTERFACE(Application);

protected:
    Application() {}
};

}
