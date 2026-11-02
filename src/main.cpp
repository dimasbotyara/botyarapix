// botyarapix — main.cpp
// Точка входа: разбор флагов, сборка зависимостей, запуск меню или пакетной генерации.
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <string>

#include "app/Application.hpp"
#include "core/ConfigManager.hpp"
#include "core/PlatformPaths.hpp"
#include "core/Terminal.hpp"
#include "core/Translator.hpp"
#include "core/Utils.hpp"
#include "generators/GeneratorRegistry.hpp"

#ifndef BOTYARAPIX_VERSION
#define BOTYARAPIX_VERSION "0.1.0"
#endif

namespace {

namespace fs = std::filesystem;

void printHelp() {
    std::cout << R"(botyarapix - monochrome pixel art generator / генератор монохромного пиксель-арта

Usage / Использование:
  botyarapix                             interactive menu / интерактивное меню
  botyarapix --gen random --size 32 --scale 16
  botyarapix --gen symmetry --size 64x64 --seed 42 --out ./art
  botyarapix --list                      show algorithms / список алгоритмов
  botyarapix --selftest                  built-in checks / встроенные проверки

Options / Флаги:
  -g, --gen <id>         generator id (see --list) / id алгоритма
  -s, --size <N|WxH>     canvas size, 4..512 / размер холста
      --width <N>        canvas width / ширина
      --height <N>       canvas height / высота
      --scale <N>        upscale factor, 1..64 / апскейл
      --seed <N>         fixed seed (decimal or 0x...) / фиксированный сид
      --detail <0..100>  complexity knob / ручка сложности
      --out <dir>        output folder / папка сохранения
      --lang <ru|en>     interface language / язык интерфейса
      --config <path>    alternative config file / другой путь конфига
      --no-preview       skip ASCII preview / без ASCII-предпросмотра
  -l, --list             list algorithms / список алгоритмов
      --selftest         run self-test / самотест
  -h, --help             this help / эта справка
  -v, --version          version / версия

Config / Конфиг:
  Linux:   $XDG_CONFIG_HOME/botyarapix/config.json  (обычно ~/.config/botyarapix)
  macOS:   ~/Library/Application Support/botyarapix/config.json
  Windows: %APPDATA%\botyarapix\config.json

Controls / Управление: Up/Down (or 1-9) + Enter, Q to cancel.
)";
}

bool parseSeed(const std::string& text, std::uint64_t& out) {
    try {
        std::size_t consumed = 0;
        const unsigned long long value = std::stoull(text, &consumed, 0);
        if (consumed != text.size()) {
            return false;
        }
        out = static_cast<std::uint64_t>(value);
        return true;
    } catch (...) {
        return false;
    }
}

struct ParsedArgs {
    bp::CliOptions options;
    std::string language;
    std::string configPath;
    bool wantHelp = false;
    bool wantVersion = false;
    std::string error;
};

ParsedArgs parseArgs(int argc, char** argv) {
    ParsedArgs parsed;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        std::string inlineValue;
        const std::size_t equals = arg.find('=');
        if (arg.rfind("--", 0) == 0 && equals != std::string::npos) {
            inlineValue = arg.substr(equals + 1);
            arg = arg.substr(0, equals);
        }

        // Берёт значение флага: либо из --flag=value, либо из следующего аргумента.
        auto takeValue = [&](std::string& out) {
            if (!inlineValue.empty()) {
                out = inlineValue;
                return true;
            }
            if (i + 1 >= argc) {
                parsed.error = "missing value for " + arg;
                return false;
            }
            out = argv[++i];
            return true;
        };

        if (arg == "-h" || arg == "--help") {
            parsed.wantHelp = true;
        } else if (arg == "-v" || arg == "--version") {
            parsed.wantVersion = true;
        } else if (arg == "-l" || arg == "--list") {
            parsed.options.listGenerators = true;
        } else if (arg == "--selftest") {
            parsed.options.selfTest = true;
        } else if (arg == "--no-preview") {
            parsed.options.showPreview = false;
        } else if (arg == "-g" || arg == "--gen") {
            if (!takeValue(parsed.options.generatorId)) {
                return parsed;
            }
        } else if (arg == "-s" || arg == "--size") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            int width = 0;
            int height = 0;
            if (!bp::utils::parseCanvasSize(value, bp::AppConfig::kMinSize, bp::AppConfig::kMaxSize,
                                            width, height)) {
                parsed.error = "invalid --size: " + value;
                return parsed;
            }
            parsed.options.width = width;
            parsed.options.height = height;
        } else if (arg == "--width" || arg == "--height") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            int number = 0;
            if (!bp::utils::parseInt(value, number) || number < bp::AppConfig::kMinSize ||
                number > bp::AppConfig::kMaxSize) {
                parsed.error = "invalid " + arg + ": " + value;
                return parsed;
            }
            if (arg == "--width") {
                parsed.options.width = number;
            } else {
                parsed.options.height = number;
            }
        } else if (arg == "--scale") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            int scale = 0;
            if (!bp::utils::parseInt(value, scale) || scale < 1 || scale > 64) {
                parsed.error = "invalid --scale: " + value;
                return parsed;
            }
            parsed.options.scale = scale;
        } else if (arg == "--seed") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            if (!parseSeed(value, parsed.options.seed)) {
                parsed.error = "invalid --seed: " + value;
                return parsed;
            }
            parsed.options.seedProvided = true;
        } else if (arg == "--detail") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            int detail = 0;
            if (!bp::utils::parseInt(value, detail)) {
                parsed.error = "invalid --detail: " + value;
                return parsed;
            }
            parsed.options.detail = detail < 0 ? 0 : (detail > 100 ? 100 : detail);
        } else if (arg == "--out") {
            std::string value;
            if (!takeValue(value)) {
                return parsed;
            }
            parsed.options.outputDirectory = bp::platform::expandUser(fs::path(value));
        } else if (arg == "--lang") {
            if (!takeValue(parsed.language)) {
                return parsed;
            }
        } else if (arg == "--config") {
            if (!takeValue(parsed.configPath)) {
                return parsed;
            }
        } else {
            parsed.error = "unknown option: " + arg;
            return parsed;
        }
    }

    return parsed;
}

}  // namespace

int main(int argc, char** argv) {
    const ParsedArgs parsed = parseArgs(argc, argv);
    if (!parsed.error.empty()) {
        std::cerr << "error: " << parsed.error << "\n\n";
        printHelp();
        return 2;
    }
    if (parsed.wantHelp) {
        printHelp();
        return 0;
    }
    if (parsed.wantVersion) {
        std::cout << "botyarapix " << BOTYARAPIX_VERSION << '\n';
        return 0;
    }

    // Умные указатели: каждая подсистема владеет своим состоянием.
    auto config = std::make_unique<bp::ConfigManager>(
        parsed.configPath.empty() ? fs::path{} : fs::path(parsed.configPath));
    (void)config->load();  // нет файла/битый JSON -> остаются значения по умолчанию

    auto translator =
        std::make_unique<bp::Translator>(bp::Translator::fromCode(config->config().language));
    if (!parsed.language.empty()) {
        const bp::Language language = bp::Translator::fromCode(parsed.language);
        translator->setLanguage(language);
        config->config().language = bp::Translator::toCode(language);
        (void)config->save();
    }

    auto registry = std::make_unique<bp::GeneratorRegistry>();
    auto terminal = std::make_unique<bp::Terminal>();
    auto app = std::make_unique<bp::Application>(*config, *registry, *terminal, *translator);

    const bool batchMode = !parsed.options.generatorId.empty() || parsed.options.listGenerators ||
                           parsed.options.selfTest;
    return batchMode ? app->runCli(parsed.options) : app->runInteractive();
}
