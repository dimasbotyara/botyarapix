// botyarapix — core/Terminal.cpp
#include "core/Terminal.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>

#if defined(_WIN32)
#include <conio.h>
#include <io.h>
#include <windows.h>
#else
#include <sys/select.h>
#include <termios.h>
#include <unistd.h>
#endif

#include "core/Utils.hpp"

namespace bp {
namespace {

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

bool colorsEnabled() {
    static const bool enabled = [] {
        const char* noColor = std::getenv("NO_COLOR");
        if (noColor != nullptr && *noColor != '\0') {
            return false;
        }
        return stdoutIsTty();
    }();
    return enabled;
}

#if !defined(_WIN32)
/// RAII для termios: гарантированно вернёт терминал в нормальное состояние.
class RawModeGuard {
public:
    RawModeGuard() {
        if (::tcgetattr(STDIN_FILENO, &saved_) != 0) {
            return;
        }
        termios raw = saved_;
        raw.c_lflag &= static_cast<tcflag_t>(~(ICANON | ECHO));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        active_ = ::tcsetattr(STDIN_FILENO, TCSANOW, &raw) == 0;
    }

    ~RawModeGuard() {
        if (active_) {
            ::tcsetattr(STDIN_FILENO, TCSANOW, &saved_);
        }
    }

    RawModeGuard(const RawModeGuard&) = delete;
    RawModeGuard& operator=(const RawModeGuard&) = delete;

private:
    termios saved_{};
    bool active_ = false;
};
#endif

#if !defined(_WIN32)
/// Есть ли байт на stdin в течение timeoutMs.
bool byteAvailable(int timeoutMs) {
    fd_set set;
    FD_ZERO(&set);
    FD_SET(STDIN_FILENO, &set);
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = timeoutMs * 1000;
    const int result = ::select(STDIN_FILENO + 1, &set, nullptr, nullptr, &tv);
    return result > 0;
}
#endif

}  // namespace

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

}  // namespace style

Terminal::Terminal() : interactive_(stdinIsTty()) {
#if defined(_WIN32)
    // Включаем ANSI escape-последовательности в консоли Windows 10+.
    HANDLE handle = GetStdHandle(STD_OUTPUT_HANDLE);
    if (handle != INVALID_HANDLE_VALUE) {
        DWORD mode = 0;
        if (GetConsoleMode(handle, &mode) != 0) {
            SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
        }
    }
#endif
}

Terminal::~Terminal() = default;

void Terminal::clearScreen() const {
    if (interactive_) {
        std::cout << "\033[2J\033[H" << std::flush;
    }
}

std::string Terminal::readLine(const std::string& prompt) const {
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
    std::cout << style::dim(message) << std::flush;
    std::string ignored;
    std::getline(std::cin, ignored);
}

// ---------------------------------------------------------------- ввод клавиши

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
            default: key.type = KeyPress::Type::Unknown; break;
        }
        return key;
    }
#else
    unsigned char buffer = 0;
    if (::read(STDIN_FILENO, &buffer, 1) != 1) {
        key.type = KeyPress::Type::Cancel;
        return key;
    }
    int ch = buffer;
    if (ch == 0x1B) {  // ESC: возможно, начало escape-последовательности
        if (!byteAvailable(60)) {
            key.type = KeyPress::Type::Cancel;
            return key;
        }
        unsigned char next = 0;
        if (::read(STDIN_FILENO, &next, 1) != 1) {
            key.type = KeyPress::Type::Cancel;
            return key;
        }
        if (next == '[' || next == 'O') {
            unsigned char code = 0;
            if (::read(STDIN_FILENO, &code, 1) != 1) {
                key.type = KeyPress::Type::Cancel;
                return key;
            }
            switch (code) {
                case 'A': key.type = KeyPress::Type::Up; return key;
                case 'B': key.type = KeyPress::Type::Down; return key;
                case 'C': key.type = KeyPress::Type::Right; return key;
                case 'D': key.type = KeyPress::Type::Left; return key;
                default: key.type = KeyPress::Type::Cancel; return key;
            }
        }
        key.type = KeyPress::Type::Cancel;
        return key;
    }
#endif
    if (ch == '\r' || ch == '\n') {
        key.type = KeyPress::Type::Enter;
    } else if (ch >= '1' && ch <= '9') {
        key.type = KeyPress::Type::Digit;
        key.digit = ch - '0';
    } else if (ch == 'q' || ch == 'Q') {
        key.type = KeyPress::Type::Cancel;
    } else if (ch == 'k' || ch == 'K') {  // vim-стиль, приятный бонус
        key.type = KeyPress::Type::Up;
    } else if (ch == 'j' || ch == 'J') {
        key.type = KeyPress::Type::Down;
    } else if (ch >= 32 && ch < 127) {
        key.type = KeyPress::Type::Char;
        key.character = static_cast<char>(ch);
    } else {
        key.type = KeyPress::Type::Unknown;
    }
    return key;
}

// ----------------------------------------------------------------------- меню

int Terminal::menuFallback(const std::string& title, const std::string& hint,
                           const std::vector<std::string>& items, int initialIndex) {
    std::cout << '\n' << title << '\n';
    if (!hint.empty()) {
        std::cout << hint << '\n';
    }
    for (std::size_t i = 0; i < items.size(); ++i) {
        std::cout << "  " << (i + 1) << ". " << items[i] << '\n';
    }
    std::cout << std::flush;

    while (true) {
        std::cout << "> " << std::flush;
        std::string line;
        if (!std::getline(std::cin, line)) {
            return -1;  // поток ввода закончился
        }
        line = utils::trim(line);
        if (line.empty()) {
            const int clamped = initialIndex < 0 ? 0
                                : (initialIndex >= static_cast<int>(items.size())
                                       ? static_cast<int>(items.size()) - 1
                                       : initialIndex);
            return clamped;
        }
        if (line == "q" || line == "Q") {
            return -1;
        }
        int choice = 0;
        if (utils::parseInt(line, choice) && choice >= 1 &&
            choice <= static_cast<int>(items.size())) {
            std::cout << '\n';  // введённый номер не эхом не виден — отделяем экраны
            return choice - 1;
        }
        std::cout << "  ?\n";
    }
}

int Terminal::menu(const std::string& title, const std::string& hint,
                   const std::vector<std::string>& items, int initialIndex) {
    if (items.empty()) {
        return -1;
    }
    if (!interactive_) {
        return menuFallback(title, hint, items, initialIndex);
    }

    const int count = static_cast<int>(items.size());
    int cursor = initialIndex;
    if (cursor < 0 || cursor >= count) {
        cursor = 0;
    }

    std::cout << '\n' << style::bold(title) << '\n';
    if (!hint.empty()) {
        std::cout << style::dim(hint) << '\n';
    }
    std::cout << std::flush;

    bool firstDraw = true;
    while (true) {
        if (!firstDraw) {
            std::cout << "\033[" << count << "A";
        }
        firstDraw = false;
        for (int i = 0; i < count; ++i) {
            const bool selected = (i == cursor);
            std::cout << "\033[K";
            if (selected) {
                std::cout << style::bold("> " + std::to_string(i + 1) + ". " + items[i]) << '\n';
            } else {
                std::cout << "  " << style::dim(std::to_string(i + 1) + ".") << ' ' << items[i]
                          << '\n';
            }
        }
        std::cout << std::flush;

        const KeyPress key = readKey();
        switch (key.type) {
            case KeyPress::Type::Up:
                cursor = (cursor == 0) ? count - 1 : cursor - 1;
                break;
            case KeyPress::Type::Down:
                cursor = (cursor + 1) % count;
                break;
            case KeyPress::Type::Digit:
                if (key.digit >= 1 && key.digit <= count) {
                    std::cout << '\n';
                    return key.digit - 1;
                }
                break;
            case KeyPress::Type::Enter:
                std::cout << '\n';
                return cursor;
            case KeyPress::Type::Cancel:
                std::cout << '\n';
                return -1;
            default:
                break;
        }
    }
}

}  // namespace bp
