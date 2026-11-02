// botyarapix — core/Translator.hpp
// Словари RU/EN + подстановка плейсхолдеров {0}, {1}, ...
#pragma once

#include <initializer_list>
#include <string>
#include <string_view>

namespace bp {

enum class Language { English, Russian };

class Translator {
public:
    explicit Translator(Language language = Language::English);

    void setLanguage(Language language) noexcept { language_ = language; }
    Language language() const noexcept { return language_; }

    /// "ru" -> Russian, "en"/пусто/что угодно -> English.
    static Language fromCode(std::string_view code);
    static std::string toCode(Language language);

    bool has(std::string_view key) const;

    /// Перевод по ключу. Если ключа нет — вернёт сам ключ (видно, что забыли).
    std::string tr(std::string_view key) const;

    /// Перевод с подстановкой {0}, {1}, ... (лишние аргументы игнорируются).
    std::string tr(std::string_view key, std::initializer_list<std::string> args) const;

private:
    Language language_;
};

}  // namespace bp
