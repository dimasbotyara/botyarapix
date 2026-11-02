// botyarapix — core/ConfigManager.hpp
// Чтение/запись ~/.config/botyarapix/config.json (и аналогов для других ОС).
#pragma once

#include <filesystem>
#include <string>

namespace bp {

namespace fs = std::filesystem;

/// Все настройки приложения. Значения по умолчанию — рабочие «из коробки».
struct AppConfig {
    std::string language = "en";                 ///< "en" | "ru"
    std::filesystem::path outputDirectory;       ///< пусто => значение по умолчанию
    int lastWidth = 32;                          ///< последний размер холста
    int lastHeight = 32;
    std::string lastGenerator = "random";        ///< последний алгоритм
    int lastScale = 8;                           ///< последний апскейл

    static constexpr int kMinSize = 4;
    static constexpr int kMaxSize = 512;

    /// Приводит значения к допустимым (вызывается после загрузки JSON).
    void sanitize();
};

/// Управляет конфигом: загрузка, сохранение, значения по умолчанию.
class ConfigManager {
public:
    /// path по умолчанию — platform::configFile().
    explicit ConfigManager(std::filesystem::path path = {});

    bool exists() const;
    bool load();                       ///< false — файла нет или он битый (конфиг остаётся дефолтным)
    bool save();                       ///< атомарная запись (tmp + rename)
    void reset();

    const AppConfig& config() const noexcept { return config_; }
    AppConfig& config() noexcept { return config_; }

    const std::filesystem::path& filePath() const noexcept { return path_; }

private:
    std::filesystem::path path_;
    AppConfig config_;
};

}  // namespace bp
