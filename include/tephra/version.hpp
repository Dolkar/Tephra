#pragma once

#include <tephra/common.hpp>

namespace tp {

/// Represents and stores a version number.
struct Version {
    /// The major version number.
    uint32_t major;
    /// The minor version number.
    uint32_t minor;
    /// The patch version number.
    uint32_t patch;

    constexpr Version() : Version(0, 0, 0) {}

    /// @param major
    ///     The major version number.
    /// @param minor
    ///     The minor version number.
    /// @param patch
    ///     The patch version number.
    constexpr Version(uint32_t major, uint32_t minor, uint32_t patch) : major(major), minor(minor), patch(patch) {}

    /// Constructs a new tp::Version object out of a packed version number used by Vulkan.
    constexpr explicit Version(uint32_t packedVersion)
        : Version(VK_VERSION_MAJOR(packedVersion), VK_VERSION_MINOR(packedVersion), VK_VERSION_PATCH(packedVersion)) {}

    /// Packs the version number into a format used by Vulkan.
    constexpr uint32_t pack() const {
        return VK_MAKE_VERSION(major, minor, patch);
    }

    /// Returns a formatted string for the version number.
    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }

    /// Returns the lowest version of the Vulkan instance-level API that Tephra supports.
    static constexpr Version getMinSupportedVulkanInstanceVersion() {
        return Version(1, 1, 0);
    }

    /// Returns the lowest version of the Vulkan device-level API that Tephra supports.
    static constexpr Version getMinSupportedVulkanDeviceVersion() {
        return Version(1, 2, 0);
    }

    /// Returns the highest Vulkan API version that Tephra will make use of.
    static constexpr Version getMaxUsedVulkanAPIVersion() {
        return Version(1, 2, 0);
    }
};

constexpr bool operator==(const Version& lhs, const Version& rhs) {
    return lhs.major == rhs.major && lhs.minor == rhs.minor && lhs.patch == rhs.patch;
}

constexpr bool operator!=(const Version& lhs, const Version& rhs) {
    return !(lhs == rhs);
}

constexpr bool operator<(const Version& lhs, const Version& rhs) {
    if (lhs.major != rhs.major) {
        return lhs.major < rhs.major;
    }
    if (lhs.minor != rhs.minor) {
        return lhs.minor < rhs.minor;
    }
    return lhs.patch < rhs.patch;
}

constexpr bool operator<=(const Version& lhs, const Version& rhs) {
    return !(rhs < lhs);
}

constexpr bool operator>(const Version& lhs, const Version& rhs) {
    return rhs < lhs;
}

constexpr bool operator>=(const Version& lhs, const Version& rhs) {
    return !(lhs < rhs);
}

}
