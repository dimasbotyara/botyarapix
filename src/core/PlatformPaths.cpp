// botyarapix — core/PlatformPaths.cpp
#include "core/PlatformPaths.hpp"

#include <cstdlib>

namespace bp::platform {
namespace {

fs::path envPath(const char* name) {
    const char* value = std::getenv(name);
    if (value == nullptr || *value == '\0') {
        return {};
    }
    return fs::path(value);
}

}  // namespace

fs::path homeDirectory() {
#if defined(_WIN32)
    fs::path home = envPath("USERPROFILE");
    if (home.empty()) {
        home = envPath("HOMEDRIVE");
        const fs::path tail = envPath("HOMEPATH");
        if (!home.empty() && !tail.empty()) {
            home /= tail;
        }
    }
    return home;
#else
    return envPath("HOME");
#endif
}

fs::path configDirectory() {
#if defined(_WIN32)
    fs::path base = envPath("APPDATA");
    if (base.empty()) {
        base = homeDirectory() / ".config";
    }
#elif defined(__APPLE__)
    fs::path base = homeDirectory() / "Library" / "Application Support";
#else
    fs::path base = envPath("XDG_CONFIG_HOME");
    if (base.empty()) {
        base = homeDirectory() / ".config";
    }
#endif
    if (base.empty()) {
        base = fs::current_path();
    }
    return base / "botyarapix";
}

fs::path configFile() { return configDirectory() / "config.json"; }

fs::path defaultOutputDirectory() {
    const fs::path home = homeDirectory();
    if (!home.empty()) {
        const fs::path pictures = home / "Pictures";
        std::error_code ec;
        if (fs::exists(pictures, ec)) {
            return pictures / "botyarapix";
        }
        return home / "Pictures" / "botyarapix";
    }
    return fs::current_path() / "botyarapix-output";
}

fs::path expandUser(const fs::path& path) {
    const std::string text = path.string();
    if (text == "~") {
        return homeDirectory();
    }
    if (text.rfind("~/", 0) == 0 || text.rfind("~\\", 0) == 0) {
        return homeDirectory() / text.substr(2);
    }
    return path;
}

std::string toUtf8(const fs::path& path) {
    // В C++17 предпочтительно u8string(); на Windows это UTF-8, на POSIX — байты.
#if defined(_WIN32)
    const std::u8string utf8 = path.u8string();
    return std::string(utf8.begin(), utf8.end());
#else
    return path.string();
#endif
}

}  // namespace bp::platform
