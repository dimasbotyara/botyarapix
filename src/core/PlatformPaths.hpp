// botyarapix — core/PlatformPaths.hpp
// Кроссплатформенные пути: конфиг и папка сохранения по умолчанию.
#pragma once

#include <filesystem>
#include <string>

namespace bp::platform {

namespace fs = std::filesystem;

/// Домашний каталог пользователя (HOME / USERPROFILE). Может быть пустым.
fs::path homeDirectory();

/// Каталог для конфигов:
///   Linux:   $XDG_CONFIG_HOME/botyarapix  или  ~/.config/botyarapix
///   macOS:   ~/Library/Application Support/botyarapix
///   Windows: %APPDATA%\botyarapix
fs::path configDirectory();

/// Полный путь к config.json.
fs::path configFile();

/// Папка сохранения картинок по умолчанию:
///   ~/Pictures/botyarapix (Linux/macOS), %USERPROFILE%\Pictures\botyarapix (Windows),
///   а если Pictures нет — <текущая папка>/botyarapix-output.
fs::path defaultOutputDirectory();

/// Разворачивает ведущий "~" в домашний каталог и нормализует путь.
fs::path expandUser(const fs::path& path);

/// Строка пути в UTF-8 (для логов и JSON на Windows).
std::string toUtf8(const fs::path& path);

}  // namespace bp::platform
