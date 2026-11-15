// botyarapix — core/Terminal.hpp
// Небольшой TUI-слой: меню со стрелками, цифрами, ввод строки.
// Если stdin/stdout не терминал (пайп/CI) — автоматически переходит на текстовый ввод.
//
// Клавиши в интерактивном меню:
//   ↑/↓, k/j, Home/End, PgUp/PgDn  — навигация
//   1…9                            — быстрый выбор
//   Enter                          — подтвердить
//   Esc, Q, Ctrl+C, Ctrl+D         — отмена
#pragma once

#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace bp {

namespace style {
/// ANSI-оформление; автоматически отключается, если вывод не в терминал или задан NO_COLOR.
std::string bold(std::string_view text);
std::string dim(std::string_view text);
/// Выбранная строка: инверсия видео + жирный (без цвета — обычный текст).
std::string highlight(std::string_view text);

/// Включено ли оформление (для тех мест, где нужен запасной маркер выбора).
bool enabled();
}  // namespace style

class Terminal {
public:
    /// Пункт меню: обычная строка, заголовок группы или разделитель.
    struct MenuItem {
        enum class Kind { Item, Separator, Title };

        std::string label;
        std::string hint;  ///< серое пояснение справа от подписи
        bool enabled = true;
        Kind kind = Kind::Item;

        MenuItem() = default;
        // Не explicit: позволяют писать вектор меню просто строками: {"A", "B"}.
        MenuItem(std::string text) : label(std::move(text)) {}  // NOLINT(google-explicit-constructor)
        MenuItem(const char* text) : label(text) {}             // NOLINT(google-explicit-constructor)

        /// Горизонтальная черта между группами (не нумеруется, выбрать нельзя).
        static MenuItem separator();
        /// Заголовок группы (не нумеруется, выбрать нельзя).
        static MenuItem header(std::string text);
        /// Пункт с пояснением справа — удобно в списках.
        static MenuItem described(std::string text, std::string hint);
        /// Видимый, но недоступный пункт.
        static MenuItem disabled(std::string text);

        /// Пояснение справа от подписи. Вызывать у уже сохранённого объекта.
        MenuItem& describe(std::string text);
        /// Пометить пункт недоступным.
        MenuItem& disable();
    };

    Terminal();
    ~Terminal();

    Terminal(const Terminal&) = delete;
    Terminal& operator=(const Terminal&) = delete;

    /// true — можно использовать стрелки/raw-mode (stdin и stdout — настоящие терминалы).
    bool isInteractive() const noexcept { return interactive_; }

    /// Меню с навигацией. Возвращает индекс выбранного пункта **в векторе items**
    /// (разделители и заголовки пропускаются) или -1 при отмене (Q/Esc/Ctrl+C).
    /// initialIndex — желаемая стартовая позиция; если она недоступна,
    /// курсор встанет на ближайший следующий выбираемый пункт.
    int menu(const std::string& title, const std::string& hint,
             const std::vector<MenuItem>& items, int initialIndex = 0);

    /// Ширина терминала в символах (0 — узнать не удалось).
    int width() const;

    /// Печатает строку целиком, обрезая её по ширине терминала.
    /// Без этого длинный путь/заголовок переносится и ломает перерисовку меню.
    void printLine(const std::string& text) const;

    /// Чтение строки (в неинтерактивном режиме — обычный getline).
    std::string readLine(const std::string& prompt) const;

    /// Пауза «нажмите Enter» (в неинтерактивном режиме ничего не делает).
    void waitForEnter(const std::string& message) const;

    /// Очистка экрана (ANSI); в неинтерактивном режиме — no-op.
    void clearScreen() const;

private:
    struct KeyPress {
        enum class Type {
            Unknown,
            Up,
            Down,
            Left,
            Right,
            Home,
            End,
            PageUp,
            PageDown,
            Enter,
            Cancel,
            Backspace,
            Digit,
            Char
        } type = Type::Unknown;
        int digit = 0;
        char character = '\0';
    };

    KeyPress readKey();

#if !defined(_WIN32)
    /// Разбор ESC-последовательности (стрелки, Home/End, PgUp/PgDn, Alt+…).
    KeyPress parseEscape();
    /// Разбор CSI-последовательности: параметры («1;5») + финальный байт ('A', '~', …).
    KeyPress parseCsi(const std::string& params, char finalByte);
    /// Читает доступные байты в pending_. timeoutMs < 0 — ждать блокирующе.
    bool fillPending(int timeoutMs);
    /// Забирает один байт (сначала из буфера, потом с timeoutMs ожидания).
    bool nextByte(unsigned char& out, int timeoutMs);
    /// Съедает вставку из буфера обмена (bracketed paste), чтобы её не приняли за клавиши.
    void skipBracketedPaste();
#endif

    /// Выбрасывает набранные «вперёд» клавиши, чтобы они не попали в getline.
    /// Меняет только внутренний буфер чтения, поэтому логически константна.
    void clearTypeAhead() const;

    int menuFallback(const std::string& title, const std::string& hint,
                     const std::vector<MenuItem>& items, const std::vector<int>& selectable,
                     int initialIndex);

    bool interactive_;

    // Буфер прочитанных, но ещё не обработанных байт (mutable: чистка не меняет
    // наблюдаемое состояние терминала).
    mutable std::vector<unsigned char> pending_;
};

}  // namespace bp
