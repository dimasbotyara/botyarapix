// botyarapix — core/Terminal.hpp
// Небольшой TUI-слой: меню со стрелками, цифрами, ввод строки.
// Если stdin не терминал (пайп/CI) — автоматически переходит на текстовый ввод.
#pragma once

#include <string>
#include <string_view>
#include <vector>

namespace bp {

namespace style {
/// ANSI-оформление; автоматически отключается, если вывод не в терминал или задан NO_COLOR.
std::string bold(std::string_view text);
std::string dim(std::string_view text);
}  // namespace style

class Terminal {
public:
    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    /// true — stdin это настоящий терминал (можно стрелки/raw-mode).
    bool isInteractive() const noexcept { return interactive_; }

    /// Меню с навигацией. Возвращает индекс выбранного пункта или -1 (отмена: Q/Esc).
    /// items — подписи без нумерации, номера добавляет меню.
    int menu(const std::string& title, const std::string& hint,
             const std::vector<std::string>& items, int initialIndex = 0);

    /// Чтение строки (в неинтерактивном режиме — обычный getline).
    std::string readLine(const std::string& prompt) const;

    /// Пауза «нажмите Enter» (в неинтерактивном режиме ничего не делает).
    void waitForEnter(const std::string& message) const;

    /// Очистка экрана (ANSI); в неинтерактивном режиме — no-op.
    void clearScreen() const;

private:
    struct KeyPress {
        enum class Type { Unknown, Up, Down, Left, Right, Enter, Cancel, Digit, Char } type = Type::Unknown;
        int digit = 0;
        char character = '\0';
    };

    KeyPress readKey();
    int menuFallback(const std::string& title, const std::string& hint,
                     const std::vector<std::string>& items, int initialIndex);

    bool interactive_;
};

}  // namespace bp
