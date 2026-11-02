// botyarapix — app/Application.cpp
#include "app/Application.hpp"

#include <algorithm>
#include <iostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

#include "core/ImageExporter.hpp"
#include "core/PlatformPaths.hpp"
#include "core/Terminal.hpp"
#include "core/Utils.hpp"

namespace bp {
namespace {

/// Символы для предпросмотра: от чёрного к белому.
constexpr char kRamp[] = " .:-=+*#%@";
constexpr int kPreviewMaxColumns = 32;  ///< исходных пикселей по ширине
constexpr int kDefaultDetail = 50;

std::string sizeLabel(int width, int height) {
    return std::to_string(width) + "x" + std::to_string(height);
}

/// Путь для показа пользователю: домашний каталог заменяется на "~".
std::string displayPath(const fs::path& path) {
    std::string text = platform::toUtf8(path);
    const fs::path home = platform::homeDirectory();
    if (!home.empty()) {
        const std::string homeText = platform::toUtf8(home);
        if (text.rfind(homeText, 0) == 0) {
            text = "~" + text.substr(homeText.size());
        }
    }
    return text;
}

}  // namespace

Application::Application(ConfigManager& config, GeneratorRegistry& registry, Terminal& terminal,
                         Translator& translator)
    : config_(config), registry_(registry), terminal_(terminal), translator_(translator) {}

// ------------------------------------------------------------------ интерактив

int Application::runInteractive() {
    if (!config_.exists()) {
        firstRun();
    }

    while (true) {
        if (mainMenu() == MainMenuResult::Exit) {
            break;
        }
    }

    std::cout << '\n' << translator_.tr("exit.bye", {displayPath(outputDirectory())}) << '\n';
    std::cout << style::dim(translator_.tr("exit.config", {displayPath(config_.filePath())}))
              << '\n';
    return 0;
}

void Application::firstRun() {
    terminal_.clearScreen();
    std::cout << '\n'
              << style::bold(translator_.tr("firstrun.welcome")) << '\n'
              << translator_.tr("firstrun.intro") << "\n\n";

    const std::vector<std::string> items{translator_.tr("lang.en"), translator_.tr("lang.ru")};
    const int choice = terminal_.menu(translator_.tr("firstrun.title"), translator_.tr("nav.hint"),
                                      items, 0);

    const Language language = (choice == 1) ? Language::Russian : Language::English;
    translator_.setLanguage(language);
    config_.config().language = Translator::toCode(language);

    const std::string configPath = displayPath(config_.filePath());
    if (config_.save()) {
        std::cout << translator_.tr("firstrun.saved", {configPath}) << '\n';
    } else {
        std::cout << translator_.tr("firstrun.save_failed", {configPath}) << '\n';
    }
    terminal_.waitForEnter(translator_.tr("common.press_enter"));
}

Application::MainMenuResult Application::mainMenu() {
    terminal_.clearScreen();

    const AppConfig& cfg = config_.config();
    const IGenerator* current = registry_.find(cfg.lastGenerator);
    const std::string currentName = current != nullptr ? generatorName(*current) : "-";

    std::cout << '\n'
              << style::bold(translator_.tr("app.name")) << "  "
              << style::dim(translator_.tr("app.subtitle")) << '\n'
              << style::dim(translator_.tr("menu.state",
                                           {std::to_string(cfg.lastWidth),
                                            std::to_string(cfg.lastHeight), currentName,
                                            std::to_string(cfg.lastScale)}))
              << '\n';

    const std::string languageLabel =
        translator_.tr(translator_.language() == Language::Russian ? "lang.ru" : "lang.en");
    const std::vector<std::string> items{
        translator_.tr("menu.new"),
        translator_.tr("menu.language", {languageLabel}),
        translator_.tr("menu.output", {displayPath(outputDirectory())}),
        translator_.tr("menu.exit"),
    };

    switch (terminal_.menu(translator_.tr("menu.title"), translator_.tr("nav.hint"), items, 0)) {
        case 0:
            createImage();
            return MainMenuResult::Continue;
        case 1:
            showLanguageMenu();
            return MainMenuResult::Continue;
        case 2:
            showOutputMenu();
            return MainMenuResult::Continue;
        default:
            return MainMenuResult::Exit;
    }
}

void Application::createImage() {
    AppConfig& cfg = config_.config();

    int width = cfg.lastWidth;
    int height = cfg.lastHeight;
    if (!chooseSize(width, height)) {
        return;
    }

    const IGenerator* generator = chooseGenerator();
    if (generator == nullptr) {
        return;
    }

    cfg.lastWidth = width;
    cfg.lastHeight = height;
    cfg.lastGenerator = std::string(generator->id());
    persistConfig();

    std::uint64_t seed = newSeed();
    Canvas canvas(width, height);
    generator->generate(canvas, GenParams{width, height, seed, kDefaultDetail});

    while (true) {
        terminal_.clearScreen();
        printPreview(canvas, *generator, seed);

        const std::vector<std::string> items{translator_.tr("after.regen"),
                                             translator_.tr("after.save"),
                                             translator_.tr("after.back")};
        const int choice =
            terminal_.menu(translator_.tr("after.title"), translator_.tr("nav.hint"), items, 0);

        if (choice == 0) {
            seed = newSeed();
            canvas = Canvas(width, height);
            generator->generate(canvas, GenParams{width, height, seed, kDefaultDetail});
        } else if (choice == 1) {
            const int scale = chooseScale(width, height);
            if (scale > 0) {
                saveImage(canvas, *generator, seed, scale);
                terminal_.waitForEnter(translator_.tr("common.press_enter"));
            }
        } else {
            return;
        }
    }
}

bool Application::chooseSize(int& width, int& height) {
    const AppConfig& cfg = config_.config();
    std::vector<std::pair<int, int>> presets{{8, 8}, {16, 16}, {32, 32}, {64, 64}, {128, 128}};
    const std::pair<int, int> last{cfg.lastWidth, cfg.lastHeight};
    const bool lastIsPreset = std::find(presets.begin(), presets.end(), last) != presets.end();

    std::vector<std::string> items;
    int initialIndex = 0;
    for (std::size_t i = 0; i < presets.size(); ++i) {
        const std::string label = sizeLabel(presets[i].first, presets[i].second);
        if (presets[i] == last) {
            items.push_back(translator_.tr("size.last", {label}));
            initialIndex = static_cast<int>(i);
        } else {
            items.push_back(label);
        }
    }

    int lastCustomIndex = -1;
    if (!lastIsPreset) {
        lastCustomIndex = static_cast<int>(items.size());
        items.push_back(
            translator_.tr("size.last", {sizeLabel(cfg.lastWidth, cfg.lastHeight)}));
        initialIndex = lastCustomIndex;
    }

    const int customIndex = static_cast<int>(items.size());
    items.push_back(translator_.tr("size.custom"));
    items.push_back(translator_.tr("common.back"));

    const int choice =
        terminal_.menu(translator_.tr("size.title"), translator_.tr("nav.hint"), items, initialIndex);
    if (choice < 0 || choice == static_cast<int>(items.size()) - 1) {
        return false;
    }

    if (choice == customIndex) {
        while (true) {
            const std::string input = terminal_.readLine(translator_.tr("size.prompt"));
            int w = 0;
            int h = 0;
            if (utils::parseCanvasSize(input, AppConfig::kMinSize, AppConfig::kMaxSize, w, h)) {
                width = w;
                height = h;
                return true;
            }
            std::cout << translator_.tr("size.invalid") << '\n' << std::flush;
            if (!terminal_.isInteractive()) {
                return false;  // в режиме пайпа не зацикливаемся
            }
        }
    }

    if (choice == lastCustomIndex) {
        width = cfg.lastWidth;
        height = cfg.lastHeight;
        return true;
    }

    width = presets[static_cast<std::size_t>(choice)].first;
    height = presets[static_cast<std::size_t>(choice)].second;
    return true;
}

const IGenerator* Application::chooseGenerator() {
    const auto& generators = registry_.all();
    std::vector<std::string> items;
    int initialIndex = 0;
    for (std::size_t i = 0; i < generators.size(); ++i) {
        items.push_back(generatorName(*generators[i]));
        if (generators[i]->id() == config_.config().lastGenerator) {
            initialIndex = static_cast<int>(i);
        }
    }
    items.push_back(translator_.tr("common.back"));

    const int choice =
        terminal_.menu(translator_.tr("gen.title"), translator_.tr("nav.hint"), items, initialIndex);
    if (choice < 0 || choice >= static_cast<int>(generators.size())) {
        return nullptr;
    }
    return generators[static_cast<std::size_t>(choice)].get();
}

int Application::chooseScale(int currentWidth, int currentHeight) {
    const std::vector<int> factors{1, 8, 16, 32};
    std::vector<std::string> items;
    int initialIndex = 1;
    for (std::size_t i = 0; i < factors.size(); ++i) {
        const int factor = factors[i];
        if (factor == 1) {
            items.push_back(translator_.tr("scale.original",
                                           {sizeLabel(currentWidth, currentHeight)}));
        } else {
            items.push_back(translator_.tr("scale.item",
                                           {std::to_string(factor),
                                            std::to_string(currentWidth * factor),
                                            std::to_string(currentHeight * factor)}));
        }
        if (factor == config_.config().lastScale) {
            initialIndex = static_cast<int>(i);
        }
    }
    items.push_back(translator_.tr("common.back"));

    const int choice =
        terminal_.menu(translator_.tr("scale.title"), translator_.tr("nav.hint"), items, initialIndex);
    if (choice < 0 || choice >= static_cast<int>(factors.size())) {
        return -1;
    }
    return factors[static_cast<std::size_t>(choice)];
}

void Application::showLanguageMenu() {
    const std::vector<std::string> items{translator_.tr("lang.en"), translator_.tr("lang.ru")};
    const int initial = translator_.language() == Language::Russian ? 1 : 0;
    const int choice =
        terminal_.menu(translator_.tr("lang.title"), translator_.tr("nav.hint"), items, initial);
    if (choice < 0) {
        return;
    }

    const Language language = (choice == 1) ? Language::Russian : Language::English;
    translator_.setLanguage(language);
    config_.config().language = Translator::toCode(language);
    persistConfig();

    std::cout << '\n'
              << translator_.tr("lang.changed",
                                {translator_.tr(language == Language::Russian ? "lang.ru"
                                                                             : "lang.en")})
              << '\n';
    terminal_.waitForEnter(translator_.tr("common.press_enter"));
}

void Application::showOutputMenu() {
    while (true) {
        terminal_.clearScreen();
        std::cout << '\n'
                  << style::bold(translator_.tr("out.title")) << '\n'
                  << translator_.tr("out.current", {displayPath(outputDirectory())}) << '\n'
                  << style::dim(translator_.tr(
                         "out.default", {displayPath(platform::defaultOutputDirectory())}))
                  << '\n';

        const std::vector<std::string> items{translator_.tr("out.change"),
                                             translator_.tr("common.back")};
        const int choice =
            terminal_.menu(translator_.tr("out.title"), translator_.tr("nav.hint"), items, 0);
        if (choice != 0) {
            return;
        }

        const std::string input = terminal_.readLine(translator_.tr("out.prompt"));
        const fs::path target = input.empty()
                                    ? platform::defaultOutputDirectory()
                                    : platform::expandUser(fs::path(input));

        std::string error;
        if (ImageExporter::ensureDirectory(target, &error)) {
            config_.config().outputDirectory = target;
            persistConfig();
            std::cout << translator_.tr("out.changed", {displayPath(target)}) << '\n';
        } else {
            std::cout << translator_.tr("out.error", {error}) << '\n';
        }
        terminal_.waitForEnter(translator_.tr("common.press_enter"));
    }
}

// ------------------------------------------------------------------- вывод/файлы

void Application::printPreview(const Canvas& canvas, const IGenerator& generator,
                               std::uint64_t seed) const {
    std::cout << '\n'
              << style::bold(translator_.tr("preview.title",
                                            {std::to_string(canvas.width()),
                                             std::to_string(canvas.height()),
                                             generatorName(generator),
                                             utils::toHex(seed, 8)}))
              << '\n';

    const int step = std::max(1, (canvas.width() + kPreviewMaxColumns - 1) / kPreviewMaxColumns);
    const int rampSize = static_cast<int>(sizeof(kRamp) - 2);  // без '\0'
    for (int y = 0; y < canvas.height(); y += step) {
        std::string line;
        line.reserve(static_cast<std::size_t>(canvas.width() / step + 1) * 2);
        for (int x = 0; x < canvas.width(); x += step) {
            // Инверсия: белый пиксель = пробел, чёрный = плотный символ (как «тушь»).
            const int value = 255 - static_cast<int>(canvas.at(x, y));
            const char symbol = kRamp[(value * rampSize) / 255];
            line.push_back(symbol);
            line.push_back(symbol);  // терминальные символы выше, чем шире
        }
        std::cout << line << '\n';
    }
    std::cout << std::flush;
}

bool Application::saveImage(const Canvas& canvas, const IGenerator& generator, std::uint64_t seed,
                            int scale) {
    config_.config().lastScale = scale;
    persistConfig();

    const fs::path file = ImageExporter::buildFileName(outputDirectory(), generator.id(),
                                                       canvas.width(), canvas.height(), seed, scale);
    const ImageExporter::Result result = ImageExporter::savePng(canvas, scale, file);

    if (!result.ok) {
        std::cout << '\n' << translator_.tr("save.error", {result.error}) << '\n';
        return false;
    }

    std::cout << '\n'
              << style::bold(translator_.tr("save.ok", {platform::toUtf8(file)})) << '\n'
              << style::dim(translator_.tr("save.info",
                                           {std::to_string(canvas.width() * scale),
                                            std::to_string(canvas.height() * scale),
                                            std::to_string(scale),
                                            utils::humanSize(result.bytes)}))
              << '\n';
    return true;
}

void Application::persistConfig() {
    if (!persist_) {
        return;
    }
    if (!config_.save()) {
        std::cerr << "\n[warn] " << translator_.tr("firstrun.save_failed",
                                                   {displayPath(config_.filePath())})
                  << '\n';
    }
}

// ------------------------------------------------------------------- CLI-режимы

int Application::runCli(const CliOptions& options) {
    if (options.selfTest) {
        return selfTest();
    }

    if (options.listGenerators) {
        std::cout << '\n' << style::bold(translator_.tr("gen.title")) << '\n';
        for (const auto& generator : registry_.all()) {
            std::cout << "  " << style::bold(generatorName(*generator)) << "  "
                      << style::dim("[" + std::string(generator->id()) + "]") << '\n'
                      << "      " << translator_.tr(std::string(generator->descriptionKey()))
                      << '\n';
        }
        std::cout << std::flush;
        return 0;
    }

    const IGenerator* generator = registry_.find(options.generatorId);
    if (generator == nullptr) {
        std::cerr << translator_.tr("gen.unknown", {options.generatorId}) << '\n';
        return 2;
    }

    const AppConfig& cfg = config_.config();
    const int width = options.width > 0 ? options.width : cfg.lastWidth;
    const int height = options.height > 0 ? options.height : cfg.lastHeight;
    const int scale = options.scale > 0 ? options.scale : cfg.lastScale;
    const std::uint64_t seed = options.seedProvided ? options.seed : newSeed();

    persist_ = false;  // пакетный режим не трогает пользовательский конфиг
    outputOverride_ = options.outputDirectory;

    const bool ok = generateAndSave(*generator, width, height, seed, scale, options.showPreview);
    return ok ? 0 : 1;
}

bool Application::generateAndSave(const IGenerator& generator, int width, int height,
                                  std::uint64_t seed, int scale, bool showPreview) {
    Canvas canvas(width, height);
    generator.generate(canvas, GenParams{width, height, seed, kDefaultDetail});

    if (showPreview) {
        printPreview(canvas, generator, seed);
    }
    std::cout << translator_.tr("cli.seed", {utils::toHex(seed, 8)}) << '\n' << std::flush;

    return saveImage(canvas, generator, seed, scale);
}

int Application::selfTest() {
    std::cout << "botyarapix self-test\n";
    int failures = 0;
    int warnings = 0;

    std::error_code ec;
    const fs::path directory = fs::temp_directory_path(ec) / "botyarapix-selftest";
    fs::remove_all(directory, ec);

    const int sizes[] = {8, 16, 32, 64};
    const std::uint64_t seeds[] = {1, 7, 12345};

    for (const auto& generator : registry_.all()) {
        int white = 0;
        int total = 0;
        bool saveOk = true;
        std::string saveError;

        for (const int size : sizes) {
            for (const std::uint64_t seed : seeds) {
                Canvas canvas(size, size);
                generator->generate(canvas, GenParams{size, size, seed, 50});
                total += static_cast<int>(canvas.pixelCount());
                for (std::size_t i = 0; i < canvas.pixelCount(); ++i) {
                    white += canvas.data()[i] != 0 ? 1 : 0;
                }

                const fs::path file =
                    directory / (std::string(generator->id()) + "_" + std::to_string(size) + "_" +
                                 std::to_string(seed) + ".png");
                const ImageExporter::Result result = ImageExporter::savePng(canvas, 4, file);
                if (!result.ok) {
                    saveOk = false;
                    saveError = result.error;
                }
            }
        }

        const double ratio = total > 0 ? static_cast<double>(white) / total : 0.0;
        const bool suspicious = ratio < 0.05 || ratio > 0.95;
        if (!saveOk) {
            ++failures;
        }
        if (suspicious) {
            ++warnings;
        }

        std::cout << "  " << (saveOk ? "PASS" : "FAIL") << "  " << generator->id() << "  white "
                  << static_cast<int>(ratio * 100) << "%"
                  << (suspicious ? "  (warning: almost uniform)" : "")
                  << (saveOk ? "" : "  " + saveError) << '\n';
    }

    fs::remove_all(directory, ec);
    std::cout << "  failures: " << failures << ", warnings: " << warnings << '\n';
    return failures == 0 ? 0 : 1;
}

// --------------------------------------------------------------------- утилиты

std::filesystem::path Application::outputDirectory() const {
    if (!outputOverride_.empty()) {
        return outputOverride_;
    }
    fs::path directory = config_.config().outputDirectory;
    if (directory.empty()) {
        directory = platform::defaultOutputDirectory();
    }
    return directory;
}

std::string Application::generatorName(const IGenerator& generator) const {
    return translator_.tr(std::string(generator.nameKey()));
}

std::uint64_t Application::newSeed() {
    std::random_device device;
    std::uint64_t seed = (static_cast<std::uint64_t>(device()) << 32) ^ device();
    if (seed == 0) {
        seed = 0x9E3779B97F4A7C15ull;
    }
    return seed;
}

}  // namespace bp
