#pragma once

#include "bcad/core/Document.h"
#include <string>
#include <vector>

namespace bcad::plugin {

// Version information for API compatibility checks
struct PluginVersion {
    int major = 0;
    int minor = 0;
    int patch = 0;

    PluginVersion() = default;
    PluginVersion(int maj, int min, int pat) : major(maj), minor(min), patch(pat) {}

    // Compare versions (returns true if this version is >= other)
    bool operator>=(const PluginVersion& other) const {
        if (major != other.major) return major > other.major;
        if (minor != other.minor) return minor > other.minor;
        return patch >= other.patch;
    }

    bool operator<=(const PluginVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch <= other.patch;
    }

    bool operator==(const PluginVersion& other) const {
        return major == other.major && minor == other.minor && patch == other.patch;
    }

    bool operator!=(const PluginVersion& other) const {
        return !(*this == other);
    }

    bool operator<(const PluginVersion& other) const {
        if (major != other.major) return major < other.major;
        if (minor != other.minor) return minor < other.minor;
        return patch < other.patch;
    }

    bool operator>(const PluginVersion& other) const {
        return other < *this;
    }

    std::string toString() const {
        return std::to_string(major) + "." + std::to_string(minor) + "." + std::to_string(patch);
    }
};

// Sandbox level for plugin API restrictions
enum class SandboxLevel {
    // Full access to all BCAD APIs
    Full = 0,
    // Restricted: no file system access, no network, no process execution
    Restricted = 1,
    // Minimal: only core geometry/document APIs, no I/O
    Minimal = 2
};

// Capabilities that a plugin can request
struct PluginCapabilities {
    bool needsFileSystem = false;
    bool needsNetwork = false;
    bool needsProcessExecution = false;
    bool needsGPU = false;
    bool needsAudio = false;
    SandboxLevel requestedSandbox = SandboxLevel::Full;
};

// Interface that every BCAD plugin must implement.
// The host calls bcad_plugin_init() which returns an instance of this interface.
class IPlugin {
public:
    virtual ~IPlugin() = default;

    // Unique identifier for the plugin (e.g., "com.example.myplugin").
    virtual std::string id() const = 0;

    // Human-readable name for UI display.
    virtual std::string name() const = 0;

    // Plugin version (semantic versioning recommended).
    virtual std::string version() const = 0;

    // BCAD API version that this plugin was built against.
    // Used for compatibility checking.
    virtual PluginVersion requiredApiVersion() const { return PluginVersion(1, 0, 0); }

    // Capabilities that this plugin requires.
    // Host can deny loading if capabilities are not granted.
    virtual PluginCapabilities capabilities() const { return {}; }

    // Called once after the plugin is loaded.
    // Return true on success, false to indicate load failure.
    virtual bool initialize() = 0;

    // Called before the plugin is unloaded.
    virtual void shutdown() = 0;

    // Register custom entity types with the registry.
    // Called after initialize() if the plugin provides new entity types.
    virtual void registerEntityTypes() {}

    // Register custom serializers for entity types.
    virtual void registerSerializers() {}

    // Register custom commands with the command system.
    virtual void registerCommands() {}

    // Return a list of file extensions this plugin can read/write.
    // Used by the file dialog filters.
    virtual std::vector<std::string> supportedFileExtensions() const { return {}; }

    // Called when a file with a supported extension is opened.
    // Return true if the plugin handled the file.
    virtual bool loadFile(const std::string& path, core::Document& doc) { return false; }

    // Called when a file with a supported extension is saved.
    // Return true if the plugin handled the file.
    virtual bool saveFile(const std::string& path, const core::Document& doc) { return false; }

    // Called by host to check if plugin is compatible with current BCAD version.
    // Return true if compatible, false to prevent loading.
    virtual bool isCompatibleWithHost(const PluginVersion& hostVersion) const {
        return requiredApiVersion() >= PluginVersion(1, 0, 0) && 
               requiredApiVersion() <= hostVersion;
    }
};

// C-compatible entry point that the host calls via dlopen/dlsym.
// Must return a pointer to an IPlugin instance (owned by the host).
// The host will call delete on the pointer when unloading.
extern "C" IPlugin* bcad_plugin_init();

} // namespace bcad::plugin