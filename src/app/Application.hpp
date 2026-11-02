// botyarapix — app/Application.hpp
// Вся логика экранов: первый запуск, меню, генерация, апскейл, сохранение.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>

#include "core/Canvas.hpp"
#include "core/ConfigManager.hpp"
#include "core/Translator.hpp"
#include "generators/GeneratorRegistry.hpp"
#include "generators/IGenerator.hpp"

namespace bp {

class Terminal;

/// Параметры неинтерактивного запуска (CLI-флаги).
struct CliOptions {
    bool listGenerators = false;
    bool selfTest = false;
    bool showPreview = true;
    bool interactive = true;

    std::string generatorId;  ///< не пусто => пакетная генерация без меню
    int width = 0;            ///< 0 => взять из конфига
    int height = 0;
    int scale = 0;            ///< 0 => взять из конфига
    int detail = 50;
    std::uint64_t seed = 0;
    bool seedProvided = false;
    std::filesystem::path outputDirectory;
};

class Application {
public:
    Application(ConfigManager& config, GeneratorRegistry& registry, Terminal& terminal,
                Translator& translator);

    /// Интерактивный режим: первый запуск, меню, генерация.
    int runInteractive();

    /// Неинтерактивный режим: --gen/--list/--selftest.
    int runCli(const CliOptions& options);

    /// Проверка всех генераторов: холст не пустой, PNG пишется.
    int selfTest();

private:
    enum class MainMenuResult { Continue, Exit };

    // --- экраны ---
    void firstRun();
    MainMenuResult mainMenu();
    void createImage();
    void showLanguageMenu();
    void showOutputMenu();

    bool chooseSize(int& width, int& height);
    const IGenerator* chooseGenerator();
    int chooseScale(int currentWidth, int currentHeight);

    // --- действия ---
    void printPreview(const Canvas& canvas, const IGenerator& generator, std::uint64_t seed) const;
    bool generateAndSave(const IGenerator& generator, int width, int height, std::uint64_t seed,
                         int scale, bool showPreview);
    bool saveImage(const Canvas& canvas, const IGenerator& generator, std::uint64_t seed,
                   int scale);
    void persistConfig();

    // --- утилиты ---
    std::filesystem::path outputDirectory() const;
    std::string generatorName(const IGenerator& generator) const;
    static std::uint64_t newSeed();

    ConfigManager& config_;
    GeneratorRegistry& registry_;
    Terminal& terminal_;
    Translator& translator_;

    /// Пакетный режим: не сохранять конфиг, папка берётся из --out.
    bool persist_ = true;
    std::filesystem::path outputOverride_;
};

}  // namespace bp
