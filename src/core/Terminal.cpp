// botyarapix — core/Terminal.cpp
#include "core/Terminal.hpp"

#include <algorithm>
#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>

#include "core/Utils.hpp"

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace bp {
namespace {

// ------------------------------------------------------------------ окружение

bool stdoutIsTty() {
#if defined(_WIN32)
    return _isatty(_fileno(stdout)) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

bool stdinIsTty() {
#if defined(_WIN32)
    return _isatty(_fileno(stdin)) != 0;
#else
    return ::isatty(STDIN_FILENO) != 0;
#endif
}

/// Терминал, который точно не понимает escape-коды (dumb, пустой TERM в cron/CI).
bool dumbTerminal() {
    const char* term = std::getenv("TERM");
    if (term == nullptr) {
        return false;  // TERM не задан — не повод отключать всё, решаем по TTY
    }
    const std::string value(term);
    return value.empty() || value == "dumb";
}

bool colorsEnabled() {
    static const bool enabled = [] {
        const char* noColor = std::getenv("NO_COLOR");
        if (noColor != nullptr && *noColor != '\0') {
            return false;
        }
        if (dumbTerminal()) {
            return false;
        }
        return stdoutIsTty();
    }();
    return enabled;
}

/// Ширина терминала в символах (0 — узнать не удалось).
int terminalWidth() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) != 0) {
        const int width = static_cast<int>(info.srWindow.Right - info.srWindow.Left) + 1;
        if (width > 0) {
            return width;
        }
    }
    return 0;
#else
    struct winsize size {};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return static_cast<int>(size.ws_col);
    }
    const char* columns = std::getenv("COLUMNS");
    if (columns != nullptr) {
        int parsed = 0;
        if (utils::parseInt(columns, parsed) && parsed > 0) {
            return parsed;
        }
    }
    return 0;
#endif
}

/// Высота терминала в строках (0 — узнать не удалось).
int terminalHeight() {
#if defined(_WIN32)
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &info) != 0) {
        const int height = static_cast<int>(info.srWindow.Bottom - info.srWindow.Top) + 1;
        if (height > 0) {
            return height;
        }
    }
    return 0;
#else
    struct winsize size {};
    if (::ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_row > 0) {
        return static_cast<int>(size.ws_row);
    }
    const char* lines = std::getenv("LINES");
    if (lines != nullptr) {
        int parsed = 0;
        if (utils::parseInt(lines, parsed) && parsed > 0) {
            return parsed;
        }
    }
    return 0;
#endif
}

#if !defined(_WIN32)

// ------------------------------------------------------- аварийное восстановление

/// Если сигнал убьёт процесс прямо в raw-mode, терминал остался бы «сломанным»
/// (без эха и перевода строки). Держим сохранённые настройки, чтобы обработчик
/// успел их вернуть. Только атомарные операции — безопасно из сигнала.
termios g_savedTermios{};
std::atomic<bool> g_rawActive{false};

void restoreTerminalFromSignal(int sig) {
    // Ctrl+C в меню сюда не попадает (ISIG выключен), но SIGTERM/SIGHUP приходят
    // именно так: без восстановления терминал остался бы без эха и CRLF.
    if (g_rawActive.exchange(false, std::memory_order_relaxed)) {
        ::tcsetattr(STDIN_FILENO, TCSANOW, &g_savedTermios);
    }
    // Завершаемся по-человечески: код 128+signal, без «Killed» и дампа.
    std::signal(sig, SIG_DFL);
    std::_Exit(128 + sig);
}

/// RAII для termios: гарантированно вернёт терминал в нормальное состояние.
class RawModeGuard {
public:
    RawModeGuard() {
        if (::tcgetattr(STDIN_FILENO, &saved_) != 0) {
            return;
        }
        g_savedTermios = saved_;
        g_rawActive.store(true, std::memory_order_relaxed);
        installSignalHandlers();

        termios raw = saved_;
        // ICANON+ECHO — главное: без этого терминал эхом печатает «^[[B»
        // и не отдаёт клавишу, пока не нажат Enter.
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO | ISIG));
        // IXON: иначе Ctrl+S «замораживает» вывод, Ctrl+Q размораживает.
        // ICRNL: Enter превращался в '\n' — проще обрабатывать '\r'.
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        // ISIG: Ctrl+C/Ctrl+Z/Ctrl+\ приходят обычным байтом. Ctrl+C не должен
        // убивать программу посреди меню — он отменяет текущий экран (readKey).
        // OPOST+ONLCR: «\n» печатается как CR LF, иначе строки съезжают вбок.
        raw.c_oflag |= static_cast<tcflag_t>(OPOST | ONLCR);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        active_ = ::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
        if (!active_) {
            g_rawActive.store(false, std::memory_order_relaxed);
        }
    }

    ~RawModeGuard() {
        if (active_) {
            ::tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
            g_rawActive.store(false, std::memory_order_relaxed);
        }
    }

    /// Удалось ли включить raw-mode.
    bool active() const noexcept { return active_; }

    RawModeGuard(const RawModeGuard&) = delete;
    RawModeGuard& operator=(const RawModeGuard&) = delete;

private:
    static void installSignalHandlers() {
        static bool installed = false;
        if (installed) {
            return;
        }
        installed = true;
        std::signal(SIGINT, restoreTerminalFromSignal);
        std::signal(SIGTERM, restoreTerminalFromSignal);
        std::signal(SIGHUP, restoreTerminalFromSignal);
        std::signal(SIGQUIT, restoreTerminalFromSignal);
    }

    termios saved_{};
    bool active_ = false;
};

/// Есть ли байт на stdin в течение timeoutMs (timeoutMs < 0 — ждём сколько нужно).
bool stdinReadable(int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeval tv{};
    if (timeoutMs >= 0) {
        tv.tv_sec = timeoutMs / 1000;
        tv.tv_usec = (timeoutMs % 1000) * 1000;
    }
    return ::select(STDIN_FILENO + 1, &set, nullptr, nullptr, timeoutMs >= 0 ? &tv : nullptr) > 0;
}

#endif  // !_WIN32

// ------------------------------------------------------------------- ширина/юникод

/// Длина строки без ANSI-последовательностей (UTF-8 символ = 1 колонка).
std::size_t visibleWidth(std::string_view text) {
    std::size_t width = 0;
    for (std::size_t i = 0; i < text.size();) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);
        if (byte == '\033') {
            ++i;
            if (i < text.size() && text[i] == '[') {
                ++i;
                while (i < text.size() && !(text[i] >= '@' && text[i] <= '~')) {
                    ++i;
                }
                ++i;  // финальный байт CSI
            } else if (i < text.size()) {
                ++i;  // двухбайтная ESC-последовательность
            }
            continue;
        }
        if ((byte & 0xC0) != 0x80) {  // считаем только стартовые байты UTF-8
            ++width;
        }
        ++i;
    }
    return width;
}

/// Обрезает уже оформленную строку по видимой ширине, сохраняя ANSI-разметку.
/// maxWidth == 0 — не обрезаем. Длинный пункт не должен переноситься на вторую
/// физическую строку, иначе перерисовка через \033[nA «съезжает».
std::string ellipsize(const std::string& text, std::size_t maxWidth) {
    if (maxWidth == 0 || visibleWidth(text) <= maxWidth) {
        return text;
    }
    if (maxWidth <= 1) {
        return std::string("\u2026");
    }
    const std::size_t budget = maxWidth - 1;  // один столбец под «…»
    std::string out;
    std::size_t width = 0;
    bool truncated = false;

    for (std::size_t i = 0; i < text.size() && !truncated;) {
        const unsigned char byte = static_cast<unsigned char>(text[i]);

        // ANSI-последовательность целиком переносим как есть (не считается в ширину).
        if (byte == '\033') {
            const std::size_t begin = i;
            ++i;
            if (i < text.size() && text[i] == '[') {
                ++i;
                while (i < text.size() && !(text[i] >= '@' && text[i] <= '~')) {
                    ++i;
                }
                if (i < text.size()) {
                    ++i;
                }
            } else if (i < text.size()) {
                ++i;
            }
            out.append(text, begin, i - begin);
            continue;
        }

        std::size_t length = 1;
        if ((byte & 0x80) != 0) {
            while (i + length < text.size() &&
                   (static_cast<unsigned char>(text[i + length]) & 0xC0) == 0x80) {
                ++length;
            }
        }
        if (width + 1 > budget) {
            truncated = true;
            break;
        }
        out.append(text, i, length);
        i += length;
        ++width;
    }

    // Если обрезка случилась внутри подсветки — вернём оформление в норму.
    if (out.find("\033[") != std::string::npos) {
        out += "\033[0m";
    }
    out += "\u2026";
    return out;
}

/// Горизонтальная черта шириной ровно в терминал.
std::string rule(std::size_t width) {
    std::string line;
    const std::string glyph = "\u2500";  // «─»
    line.reserve(glyph.size() * std::max<std::size_t>(width, 1));
    for (std::size_t i = 0; i < width; ++i) {
        line += glyph;
    }
    return line;
}

}  // namespace

// ---------------------------------------------------------------------- оформление

namespace style {

std::string bold(std::string_view text) {
    if (!colorsEnabled()) {
        return std::string(text);
    }
    return "\033[1m" + std::string(text) + "\033[0m";
}

std::string dim(std::string_view text) {
    if (!colorsEnabled()) {
        return std::string(text);
    }
    return "\033[2m" + std::string(text) + "\033[0m";
}

std::string highlight(std::string_view text) {
    if (!colorsEnabled()) {
        return std::string(text);
    }
    // Инверсия видео + жирный: работает в 8-цветном терминале, Termux и SSH.
    return "\033[7m\033[1m" + std::string(text) + "\033[0m";
}

bool enabled() { return colorsEnabled(); }

}  // namespace style

// ------------------------------------------------------------------- MenuItem

Terminal::MenuItem Terminal::MenuItem::separator() {
    MenuItem item;
    item.kind = Kind::Separator;
    return item;
}

Terminal::MenuItem Terminal::MenuItem::header(std::string text) {
    MenuItem item;
    item.kind = Kind::Title;
    item.label = std::move(text);
    return item;
}

Terminal::MenuItem Terminal::MenuItem::described(std::string text, std::string hint) {
    MenuItem item(std::move(text));
    item.hint = std::move(hint);
    return item;
}

Terminal::MenuItem Terminal::MenuItem::disabled(std::string text) {
    MenuItem item(std::move(text));
    item.enabled = false;
    return item;
}

Terminal::MenuItem& Terminal::MenuItem::describe(std::string text) {
    hint = std::move(text);
    return *this;
}

Terminal::MenuItem& Terminal::MenuItem::disable() {
    enabled = false;
    return *this;
}

// ---------------------------------------------------------------- жизненный цикл

Terminal::Terminal() : interactive_(stdinIsTty() && stdoutIsTty()) {
#if defined(_WIN32)
    // Включаем ANSI escape-последовательности в консоли Windows 10+.
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode) != 0) {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#else
    // Терминал не понимает escape-коды — уходим в текстовый режим.
    if (dumbTerminal()) {
        interactive_ = false;
    }
#endif
}

Terminal::~Terminal() = default;

int Terminal::width() const { return terminalWidth(); }

void Terminal::printLine(const std::string& text) const {
    const int detected = terminalWidth();
    const std::size_t limit =
        detected > 2 ? static_cast<std::size_t>(detected - 1) : std::size_t{0};
    std::cout << ellipsize(text, limit) << '\n' << std::flush;
}

void Terminal::clearScreen() const {
    if (interactive_) {
        std::cout << "\033[2J\033[H" << std::flush;
    }
}

std::string Terminal::readLine(const std::string& prompt) const {
    // Клавиши, нажатые «в запас» на предыдущем экране, не должны попасть в ввод.
    clearTypeAhead();
    std::cout << prompt << std::flush;
    std::string line;
    if (!std::getline(std::cin, line)) {
        return {};
    }
    return utils::trim(line);
}

void Terminal::waitForEnter(const std::string& message) const {
    if (!interactive_) {
        return;
    }
    // В этот момент терминал в каноническом режиме: лишний Enter, нажатый
    // заранее, уже лежит во входном буфере и «съест» паузу. Убираем его.
    clearTypeAhead();
    std::cout << style::dim(message) << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

// ------------------------------------------------------------------ чтение клавиш

#if !defined(_WIN32)

bool Terminal::fillPending(int timeoutMs) {
    if (!stdinReadable(timeoutMs)) {
        return false;
    }
    unsigned char buffer[256];
    const ssize_t count = ::read(STDIN_FILENO, buffer, sizeof(buffer));
    if (count <= 0) {
        return false;
    }
    pending_.insert(pending_.end(), buffer, buffer + count);
    return true;
}

bool Terminal::nextByte(unsigned char& out, int timeoutMs) {
    if (pending_.empty() && !fillPending(timeoutMs)) {
        return false;
    }
    out = pending_.front();
    pending_.erase(pending_.begin());
    return true;
}

void Terminal::skipBracketedPaste() {
    // Вставка из буфера обмена: ESC [ 2 0 0 ~ <текст> ESC [ 2 0 1 ~.
    // Съедаем её целиком, чтобы цифры/Enter из текста не «нажали» пункт меню.
    static const std::string kEnd = "\033[201~";
    std::string tail;
    for (int i = 0; i < 8192; ++i) {  // предохранитель
        unsigned char byte = 0;
        if (!nextByte(byte, 50)) {
            return;
        }
        tail.push_back(static_cast<char>(byte));
        if (tail.size() > kEnd.size()) {
            tail.erase(0, tail.size() - kEnd.size());
        }
        if (tail == kEnd) {
            return;
        }
    }
}

Terminal::KeyPress Terminal::parseCsi(const std::string& params, char finalByte) {
    KeyPress key;
    switch (finalByte) {
        case 'A': key.type = KeyPress::Type::Up; break;
        case 'B': key.type = KeyPress::Type::Down; break;
        case 'C': key.type = KeyPress::Type::Right; break;
        case 'D': key.type = KeyPress::Type::Left; break;
        case 'H': key.type = KeyPress::Type::Home; break;
        case 'F': key.type = KeyPress::Type::End; break;
        case 'Z': key.type = KeyPress::Type::Unknown; break;  // Shift+Tab
        case '~': {
            int code = 0;
            if (!utils::parseInt(params.substr(0, params.find(';')), code)) {
                return key;  // «ESC [ ~» без цифр — не наш случай
            }
            switch (code) {
                case 1:
                case 7: key.type = KeyPress::Type::Home; break;
                case 4:
                case 8: key.type = KeyPress::Type::End; break;
                case 5: key.type = KeyPress::Type::PageUp; break;
                case 6: key.type = KeyPress::Type::PageDown; break;
                case 200:  // начало bracketed paste — выкидываем содержимое
                    if (params.find(';') == std::string::npos) {
                        skipBracketedPaste();
                    }
                    break;
                default: break;  // Delete, F1-F12 и прочее — игнорируем
            }
            break;
        }
        default: break;
    }
    return key;
}

Terminal::KeyPress Terminal::parseEscape() {
    // ESC уже прочитан. Хвост ждём недолго: одиночный Esc должен срабатывать
    // сразу, а не «залипать» на каждой стрелке.
    unsigned char next = 0;
    if (!nextByte(next, 30)) {
        KeyPress key;
        key.type = KeyPress::Type::Cancel;  // одиночный Esc
        return key;
    }

    if (next == '[') {  // CSI: ESC [ параметры финальный-байт
        std::string params;
        for (int i = 0; i < 32; ++i) {
            unsigned char byte = 0;
            if (!nextByte(byte, 30)) {
                break;
            }
            if (byte >= 0x40 && byte <= 0x7E) {  // финальный байт
                return parseCsi(params, static_cast<char>(byte));
            }
            params.push_back(static_cast<char>(byte));
        }
        return KeyPress{};
    }

    if (next == 'O') {  // SS3: ESC O A/B/C/D/H/F — старые терминалы и Termux
        unsigned char byte = 0;
        if (!nextByte(byte, 30)) {
            return KeyPress{};
        }
        return parseCsi(std::string(), static_cast<char>(byte));
    }

    return KeyPress{};  // Alt+<символ> и прочее для меню бесполезно
}

#endif  // !_WIN32

Terminal::KeyPress Terminal::readKey() {
    KeyPress key;
#if defined(_WIN32)
    int ch = _getch();
    if (ch == 0 || ch == 224) {  // префикс спецклавиш
        switch (_getch()) {
            case 72: key.type = KeyPress::Type::Up; break;
            case 80: key.type = KeyPress::Type::Down; break;
            case 75: key.type = KeyPress::Type::Left; break;
            case 77: key.type = KeyPress::Type::Right; break;
            case 71: key.type = KeyPress::Type::Home; break;
            case 79: key.type = KeyPress::Type::End; break;
            case 73: key.type = KeyPress::Type::PageUp; break;
            case 81: key.type = KeyPress::Type::PageDown; break;
            default: key.type = KeyPress::Type::Unknown; break;
        }
        return key;
    }
#else
    unsigned char buffer = 0;
    if (!nextByte(buffer, -1)) {
        key.type = KeyPress::Type::Cancel;  // конец ввода / терминал закрылся
        return key;
    }
    int ch = buffer;
    if (ch == 0x1B) {
        return parseEscape();
    }
#endif
    if (ch == '\r' || ch == '\n') {
        key.type = KeyPress::Type::Enter;
    } else if (ch == 0x03 || ch == 0x04) {  // Ctrl+C / Ctrl+D
        key.type = KeyPress::Type::Cancel;
    } else if (ch == 'q' || ch == 'Q') {
        key.type = KeyPress::Type::Cancel;
    } else if (ch == 'k' || ch == 'K') {  // vim-стиль, приятный бонус
        key.type = KeyPress::Type::Up;
    } else if (ch == 'j' || ch == 'J') {
        key.type = KeyPress::Type::Down;
    } else if (ch >= '1' && ch <= '9') {
        key.type = KeyPress::Type::Digit;
        key.digit = ch - '0';
    } else if (ch == 0x7F || ch == 0x08) {  // Backspace (Termux и Windows шлют DEL)
        key.type = KeyPress::Type::Backspace;
    } else if (ch >= 32 && ch < 127) {
        key.type = KeyPress::Type::Char;
        key.character = static_cast<char>(ch);
    } else {
        key.type = KeyPress::Type::Unknown;
    }
    return key;
}

void Terminal::clearTypeAhead() const {
#if !defined(_WIN32)
    pending_.clear();
    // Отбрасываем то, что пользователь нажал «в запас», пока смотрел на меню:
    // иначе эти клавиши доедут до следующего readLine и испортят ввод.
    while (stdinReadable(0)) {
        unsigned char buffer[256];
        if (::read(STDIN_FILENO, buffer, sizeof(buffer)) <= 0) {
            break;
        }
    }
#endif
}

// --------------------------------------------------------------------------- меню

namespace {

/// Собирает индекс выбираемого пункта, ближайшего к желаемому (вперёд, потом назад).
int nearestSelectable(const std::vector<int>& selectable, int wanted, int count) {
    if (selectable.empty()) {
        return 0;
    }
    for (int offset = 0; offset < count; ++offset) {
        const int candidate = ((wanted + offset) % count + count) % count;
        if (std::find(selectable.begin(), selectable.end(), candidate) != selectable.end()) {
            return candidate;
        }
    }
    return selectable.front();
}

}  // namespace

int Terminal::menuFallback(const std::string& title, const std::string& hint,
                           const std::vector<MenuItem>& items,
                           const std::vector<int>& selectable, int initialIndex) {
    std::cout << '\n' << title << '\n';
    if (!hint.empty()) {
        std::cout << hint << '\n';
    }

    int number = 0;
    for (const MenuItem& item : items) {
        if (item.kind == MenuItem::Kind::Separator) {
            std::cout << "  " << std::string(30, '-') << '\n';
            continue;
        }
        if (item.kind == MenuItem::Kind::Title) {
            std::cout << "  " << item.label << '\n';
            continue;
        }
        std::cout << "  ";
        if (item.enabled) {
            std::cout << (number + 1) << ". ";
        } else {
            std::cout << "-. ";
        }
        std::cout << item.label;
        if (!item.hint.empty()) {
            std::cout << "  (" << item.hint << ')';
        }
        if (!item.enabled) {
            std::cout << "  [---]";
        }
        std::cout << '\n';
        ++number;
    }
    std::cout << std::flush;

    const int fallbackChoice =
        initialIndex >= 0 && initialIndex < static_cast<int>(items.size())
            ? nearestSelectable(selectable, initialIndex, static_cast<int>(items.size()))
            : selectable.front();

    while (true) {
        std::cout << "> " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return -1;  // поток ввода закончился
        }
        line = utils::trim(line);
        if (line.empty()) {
            return fallbackChoice;  // Enter = пункт по умолчанию
        }
        if (line == "q" || line == "Q") {
            return -1;
        }
        int choice = 0;
        if (utils::parseInt(line, choice) && choice >= 1 &&
            choice <= static_cast<int>(selectable.size())) {
            const int index = selectable[static_cast<std::size_t>(choice - 1)];
            if (items[static_cast<std::size_t>(index)].enabled) {
                std::cout << '\n';
                return index;
            }
            std::cout << "  ?\n";
            continue;
        }
        std::cout << "  ?\n";
    }
}

int Terminal::menu(const std::string& title, const std::string& hint,
                   const std::vector<MenuItem>& items, int initialIndex) {
    // Индексы пунктов, которые реально можно выбрать.
    std::vector<int> selectable;
    selectable.reserve(items.size());
    for (std::size_t i = 0; i < items.size(); ++i) {
        if (items[i].kind == MenuItem::Kind::Item && items[i].enabled) {
            selectable.push_back(static_cast<int>(i));
        }
    }
    if (selectable.empty()) {
        return -1;
    }
    if (!interactive_) {
        return menuFallback(title, hint, items, selectable, initialIndex);
    }

    const int count = static_cast<int>(items.size());
    const int totalSelectable = static_cast<int>(selectable.size());
    int cursor = nearestSelectable(selectable, initialIndex, count);

    // Ширина для обрезки: строка не должна переноситься, иначе \033[nA врёт.
    // Пересчитываем на каждом кадре — в Termux окно меняется при повороте экрана.
    std::size_t maxWidth = 0;
    std::size_t ruleWidth = 40;
    int terminalRows = 0;
    auto refreshGeometry = [&] {
        const int detected = terminalWidth();
        if (detected > 2) {
            maxWidth = static_cast<std::size_t>(detected - 1);
            ruleWidth = static_cast<std::size_t>(detected - 1);
        }
        terminalRows = terminalHeight();
    };
    refreshGeometry();

    RawModeGuard rawMode;
    if (!rawMode.active()) {
        // stdin выглядит как терминал, но raw-mode не включился (экзотическая
        // обёртка): стрелки/цифры в каноническом режиме придут только после Enter,
        // и весь экран засорится эхом. Честно уходим в текстовый список.
        return menuFallback(title, hint, items, selectable, initialIndex);
    }
    // Клавиши, нажатые «в запас» на прошлом экране, не должны двигать курсор.
    clearTypeAhead();
    // '\n' печатаем всегда: RawModeGuard принудительно включает OPOST/ONLCR,
    // поэтому терминал сам превратит его в CR LF (один раз, без двойного CR).
    const char* newline = "\n";

    std::cout << '\n' << style::bold(ellipsize(title, maxWidth)) << newline;
    if (!hint.empty()) {
        std::cout << style::dim(ellipsize(hint, maxWidth)) << newline;
    }
    std::cout << std::flush;

    // Курсор номера для пунктов: считаем только выбираемые подряд (1..N).
    auto numberAt = [&](int index) {
        const auto it = std::find(selectable.begin(), selectable.end(), index);
        return it == selectable.end() ? 0
                                     : static_cast<int>(it - selectable.begin()) + 1;
    };

    auto renderItem = [&](int index, int current, int chosen) {
        const MenuItem& item = items[static_cast<std::size_t>(index)];
        std::string line;

        if (item.kind == MenuItem::Kind::Separator) {
            line = style::dim(rule(ruleWidth));
        } else if (item.kind == MenuItem::Kind::Title) {
            line = style::dim(item.label);
        } else if (!item.enabled) {
            line = "    " + style::dim(item.label);
            if (!item.hint.empty()) {
                line += "  " + style::dim(item.hint);
            }
        } else {
            const std::string number = std::to_string(numberAt(index));
            const std::string label =
                (number.size() < 2 ? " " + number : number) + ". " + item.label;
            const std::string hintText = item.hint.empty() ? "" : "  " + item.hint;

            if (index == chosen) {  // итог: что выбрали
                line = "  " + style::bold(label);
            } else if (index == current) {  // курсор
                line = style::highlight("> " + label + hintText);
            } else {
                line = "  " + style::dim(" " + number + ".") + " " + item.label;
            }
            if (index != chosen && index != current && !item.hint.empty()) {
                line += "  " + style::dim(item.hint);
            }
        }
        return ellipsize(line, maxWidth);
    };

    int choice = -1;
    bool finished = false;
    bool firstDraw = true;
    bool resetDrawn = false;    // последний кадр нарисован «с чистого места»
    bool cursorRowValid = true; // весь кадр ещё на экране (без прокрутки)
    int rowsUsed = count;       // сколько строк занимает кадр

    while (!finished) {
        const std::size_t previousWidth = maxWidth;
        refreshGeometry();  // окно могло измениться, пока ждали клавишу

        if (!firstDraw) {
            if (maxWidth != 0 && previousWidth != 0 && maxWidth != previousWidth) {
                // Ширина терминала поменялась (поворот экрана в Termux): строки
                // могли перенестись, и возврат «на N строк вверх» уже врёт.
                if (cursorRowValid) {
                    std::cout << "\033[?25l";  // прячем курсор на время перерисовки
                    std::cout << "\033[" << rowsUsed << "A\033[J";  // стираем старый кадр
                    std::cout << "\033[?25h";
                    resetDrawn = false;
                } else {
                    // Список длиннее экрана — прокрутка неизбежна, зато не будет мусора.
                    std::cout << "\n";
                    resetDrawn = true;
                }
            } else {
                resetDrawn = false;
                std::cout << "\033[" << count << "A";  // на первую строку списка
            }
        }
        firstDraw = false;

        for (int i = 0; i < count; ++i) {
            // \033[K — стереть хвост от предыдущего кадра.
            std::cout << "\033[K" << renderItem(i, cursor, -1) << newline;
        }
        std::cout << std::flush;
        // Сколько строк занял кадр (для прокрутки) и влез ли он целиком.
        rowsUsed = count;
        cursorRowValid = (terminalRows <= 0) || (count < terminalRows);

        const KeyPress key = readKey();
        switch (key.type) {
            case KeyPress::Type::Up:
                cursor = (cursor > 0) ? cursor - 1 : count - 1;
                break;
            case KeyPress::Type::Down:
                cursor = (cursor + 1) % count;
                break;
            case KeyPress::Type::Home:
                cursor = 0;
                break;
            case KeyPress::Type::End:
                cursor = count - 1;
                break;
            case KeyPress::Type::PageUp:
                cursor = std::max(0, cursor - 5);
                break;
            case KeyPress::Type::PageDown:
                cursor = std::min(count - 1, cursor + 5);
                break;
            case KeyPress::Type::Digit:
                if (key.digit >= 1 && key.digit <= totalSelectable) {
                    choice = selectable[static_cast<std::size_t>(key.digit - 1)];
                    finished = true;
                }
                break;
            case KeyPress::Type::Enter:
                if (items[static_cast<std::size_t>(cursor)].kind == MenuItem::Kind::Item &&
                    items[static_cast<std::size_t>(cursor)].enabled) {
                    choice = cursor;
                    finished = true;
                }
                break;
            case KeyPress::Type::Cancel:
                choice = -1;
                finished = true;
                break;
            default:
                break;
        }

        // Курсор всегда стоит на выбираемом пункте, иначе Enter «не работает».
        if (!finished &&
            (items[static_cast<std::size_t>(cursor)].kind != MenuItem::Kind::Item ||
             !items[static_cast<std::size_t>(cursor)].enabled)) {
            cursor = nearestSelectable(selectable, cursor, count);
        }
    }

    // Финальный кадр: подсветка снята, выбранный пункт отмечен.
    // После сброса (смена ширины) список уже нарисован заново — не смещаемся.
    if (!resetDrawn) {
        std::cout << "\033[" << count << "A";
    }
    for (int i = 0; i < count; ++i) {
        std::cout << "\033[K" << renderItem(i, -1, choice) << newline;
    }
    std::cout << newline << std::flush;  // отделяем список от того, что ниже
    return choice;
}

}  // namespace bp
