// botyarapix — core/ConfigManager.cpp
#include "core/ConfigManager.hpp"

#include <algorithm>
#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include "core/PlatformPaths.hpp"

namespace bp {
namespace {

using nlohmann::json;

constexpr int kDefaultScale = 8;

int clampSize(int value) {
    return std::clamp(value, AppConfig::kMinSize, AppConfig::kMaxSize);
}

}  // namespace

void AppConfig::sanitize() {
    if (language != "ru" && language != "en") {
        language = "en";
    }
    lastWidth = clampSize(lastWidth);
    lastHeight = clampSize(lastHeight);
    if (lastScale < 1 || lastScale > 64) {
        lastScale = kDefaultScale;
    }
    if (lastGenerator.empty()) {
        lastGenerator = "random";
    }
}

ConfigManager::ConfigManager(std::filesystem::path path)
    : path_(path.empty() ? platform::configFile() : std::move(path)),
      config_() {
    config_.outputDirectory = platform::defaultOutputDirectory();
}

bool ConfigManager::exists() const {
    std::error_code ec;
    return fs::is_regular_file(path_, ec);
}

void ConfigManager::reset() {
    config_ = AppConfig{};
    config_.outputDirectory = platform::defaultOutputDirectory();
}

bool ConfigManager::load() {
    reset();
    if (!exists()) {
        return false;
    }

    std::ifstream in(path_);
    if (!in) {
        return false;
    }

    try {
        json root;
        in >> root;
        if (!root.is_object()) {
            return false;
        }

        config_.language = root.value("language", config_.language);
        config_.lastGenerator = root.value("lastGenerator", config_.lastGenerator);
        config_.lastWidth = root.value("lastWidth", config_.lastWidth);
        config_.lastHeight = root.value("lastHeight", config_.lastHeight);
        config_.lastScale = root.value("lastScale", config_.lastScale);

        const std::string outputDir = root.value("outputDirectory", std::string{});
        if (!outputDir.empty()) {
            config_.outputDirectory = platform::expandUser(fs::path(outputDir));
        }
    } catch (const std::exception&) {
        reset();
        return false;
    }

    config_.sanitize();
    return true;
}

bool ConfigManager::save() {
    config_.sanitize();

    std::error_code ec;
    const fs::path parent = path_.parent_path();
    if (!parent.empty()) {
        fs::create_directories(parent, ec);
        if (ec) {
            return false;
        }
    }

    json root;
    root["$schema"] = "botyarapix/config/v1";
    root["language"] = config_.language;
    root["outputDirectory"] = platform::toUtf8(config_.outputDirectory);
    root["lastWidth"] = config_.lastWidth;
    root["lastHeight"] = config_.lastHeight;
    root["lastGenerator"] = config_.lastGenerator;
    root["lastScale"] = config_.lastScale;

    // Пишем во временный файл и переименовываем: конфиг не «побьётся» при сбое.
    const fs::path tempPath = path_.string() + ".tmp";
    {
        std::ofstream out(tempPath, std::ios::trunc);
        if (!out) {
            return false;
        }
        out << root.dump(4) << '\n';
        if (!out) {
            return false;
        }
    }

    fs::remove(path_, ec);
    ec.clear();
    fs::rename(tempPath, path_, ec);
    if (ec) {
        fs::remove(tempPath, ec);
        return false;
    }
    return true;
}

}  // namespace bp
