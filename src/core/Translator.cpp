// botyarapix — core/Translator.cpp
#include "core/Translator.hpp"

#include <unordered_map>
#include <vector>

namespace bp {
namespace {

struct Entry {
    const char* en;
    const char* ru;
};

/// Один общий словарь: добавить строку = добавить одну запись.
/// Ключи с точкой: группа.подключ.
const std::unordered_map<std::string, Entry>& dictionary() {
    static const std::unordered_map<std::string, Entry> kDict = {
        // --- общее ---
        {"app.name", {"botyarapix", "botyarapix"}},
        {"app.subtitle", {"monochrome pixel art generator", "генератор монохромного пиксель-арта"}},
        {"nav.hint",
         {"Up/Down move - Enter select - 1-9 quick pick - Q cancel",
          "вверх/вниз выбор - Enter подтвердить - 1-9 быстрый выбор - Q отмена"}},
        {"common.cancel", {"< Cancel", "< Отмена"}},
        {"common.back", {"< Back", "< Назад"}},
        {"common.invalid", {"Invalid choice, try again.", "Неверный выбор, попробуйте снова."}},
        {"common.press_enter", {"Press Enter to continue...", "Нажмите Enter, чтобы продолжить..."}},

        // --- первый запуск ---
        {"firstrun.welcome", {"Welcome to botyarapix!", "Добро пожаловать в botyarapix!"}},
        {"firstrun.intro",
         {"This is your first run, so let's pick a language first.",
          "Похоже, это первый запуск, поэтому сначала выберем язык."}},
        {"firstrun.title", {"Choose language / Выберите язык", "Выберите язык / Choose language"}},
        {"firstrun.saved", {"Config saved to: {0}", "Конфиг сохранён: {0}"}},
        {"firstrun.save_failed",
         {"Could not write config {0} - settings will not persist.",
          "Не удалось записать конфиг {0} - настройки не сохранятся."}},

        // --- главное меню ---
        {"menu.title", {"Main menu", "Главное меню"}},
        {"menu.new", {"New image", "Создать изображение"}},
        {"menu.language", {"Language: {0}", "Язык: {0}"}},
        {"menu.output", {"Save folder: {0}", "Папка сохранения: {0}"}},
        {"menu.exit", {"Exit", "Выход"}},
        {"menu.state", {"Canvas {0}x{1} - algorithm {2} - upscale x{3}",
                        "Холст {0}x{1} - алгоритм {2} - апскейл x{3}"}},

        // --- размер холста ---
        {"size.title", {"Choose canvas size", "Выбор размера холста"}},
        {"size.item", {"{0}x{1}", "{0}x{1}"}},
        {"size.custom", {"Custom size...", "Свой размер..."}},
        {"size.prompt",
         {"Size (e.g. 48 or 32x64, range 4-512): ", "Размер (например 48 или 32x64, 4-512): "}},
        {"size.invalid", {"Invalid size. Use 4-512, e.g. 32 or 48x24.",
                          "Неверный размер. Нужно 4-512, например 32 или 48x24."}},
        {"size.last", {"{0}  (last used)", "{0}  (последний)"}},

        // --- алгоритмы/апскейл ---
        {"gen.title", {"Choose algorithm", "Выбор алгоритма"}},
        {"gen.unknown", {"Unknown algorithm: {0}", "Неизвестный алгоритм: {0}"}},
        {"scale.title", {"Choose upscale factor (nearest neighbour)", "Выбор апскейла (nearest neighbour)"}},
        {"scale.original", {"1x  (original {0}x{1})", "1x  (оригинал {0}x{1})"}},
        {"scale.item", {"{0}x  ->  {1}x{2} px", "{0}x  ->  {1}x{2} px"}},

        // --- генерация ---
        {"preview.title", {"Preview {0}x{1} - {2} - seed {3}", "Предпросмотр {0}x{1} - {2} - сид {3}"}},
        {"after.title", {"What's next?", "Что дальше?"}},
        {"after.regen", {"Regenerate (new seed)", "Регенерировать (новый сид)"}},
        {"after.save", {"Upscale & save as PNG", "Апскейл и сохранить в PNG"}},
        {"after.back", {"Back to main menu", "Назад в главное меню"}},
        {"after.push", {"Press Enter to go back to the preview.", "Нажмите Enter - вернёмся к предпросмотру."}},

        // --- сохранение ---
        {"save.ok", {"Saved: {0}", "Сохранено: {0}"}},
        {"save.info", {"{0}x{1} px - upscale x{2} - {3}", "{0}x{1} px - апскейл x{2} - {3}"}},
        {"save.error", {"Failed to save: {0}", "Не удалось сохранить: {0}"}},
        {"cli.seed", {"Seed: 0x{0}  (repeat with --seed 0x{0})", "Сид: 0x{0}  (повторить: --seed 0x{0})"}},

        // --- язык ---
        {"lang.title", {"Choose language", "Выберите язык"}},
        {"lang.en", {"English", "English"}},
        {"lang.ru", {"Russian", "Русский"}},
        {"lang.changed", {"Language switched to {0}", "Язык переключён на {0}"}},

        // --- папка сохранения ---
        {"out.title", {"Save folder", "Папка сохранения"}},
        {"out.current", {"Current: {0}", "Текущая: {0}"}},
        {"out.change", {"Change folder...", "Изменить папку..."}},
        {"out.prompt", {"New path (empty = default): ", "Новый путь (пусто = по умолчанию): "}},
        {"out.default", {"default: {0}", "по умолчанию: {0}"}},
        {"out.changed", {"Save folder: {0}", "Папка сохранения: {0}"}},
        {"out.error", {"Cannot create folder: {0}", "Не удалось создать папку: {0}"}},

        // --- выход ---
        {"exit.bye", {"Done. Images are in: {0}", "Готово. Картинки лежат здесь: {0}"}},
        {"exit.config", {"Config: {0}", "Конфиг: {0}"}},

        // --- генераторы ---
        {"gen.random.name", {"Random", "Случайный шум (Random)"}},
        {"gen.random.desc", {"Pure white noise", "Чистый белый шум"}},
        {"gen.symmetry.name", {"Symmetry", "Симметрия (Symmetry)"}},
        {"gen.symmetry.desc", {"Random half mirrored vertically, horizontally or both",
                               "Случайная половина и её зеркальное отражение"}},
        {"gen.cellular.name", {"Cellular", "Клеточный автомат (Cellular)"}},
        {"gen.cellular.desc", {"Game of Life frozen at step N", "Игра «Жизнь», остановленная на N-шаге"}},
    };
    return kDict;
}

}  // namespace

Translator::Translator(Language language) : language_(language) {}

Language Translator::fromCode(std::string_view code) {
    return (code == "ru" || code == "RU" || code == "russian") ? Language::Russian
                                                              : Language::English;
}

std::string Translator::toCode(Language language) {
    return language == Language::Russian ? "ru" : "en";
}

bool Translator::has(std::string_view key) const {
    return dictionary().find(std::string(key)) != dictionary().end();
}

std::string Translator::tr(std::string_view key) const {
    const auto it = dictionary().find(std::string(key));
    if (it == dictionary().end()) {
        return std::string(key);
    }
    return language_ == Language::Russian ? it->second.ru : it->second.en;
}

std::string Translator::tr(std::string_view key, std::initializer_list<std::string> args) const {
    std::string text = tr(key);
    std::size_t index = 0;
    for (const std::string& arg : args) {
        const std::string token = "{" + std::to_string(index) + "}";
        std::size_t pos = 0;
        while ((pos = text.find(token, pos)) != std::string::npos) {
            text.replace(pos, token.size(), arg);
            pos += arg.size();
        }
        ++index;
    }
    return text;
}

}  // namespace bp
