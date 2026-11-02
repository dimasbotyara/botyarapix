// botyarapix — core/Utils.cpp
#include "core/Utils.hpp"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <ctime>
#include <sstream>
#include <vector>

namespace bp::utils {
namespace {

bool isSpace(unsigned char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; }

}  // namespace

std::string trim(std::string_view text) {
    std::size_t begin = 0;
    std::size_t end = text.size();
    while (begin < end && isSpace(static_cast<unsigned char>(text[begin]))) {
        ++begin;
    }
    while (end > begin && isSpace(static_cast<unsigned char>(text[end - 1]))) {
        --end;
    }
    return std::string(text.substr(begin, end - begin));
}

std::string toLower(std::string_view text) {
    std::string out(text);
    std::transform(out.begin(), out.end(), out.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return out;
}

std::vector<std::string> split(std::string_view text, char separator) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (true) {
        const std::size_t pos = text.find(separator, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(text.substr(start));
            return parts;
        }
        parts.emplace_back(text.substr(start, pos - start));
        start = pos + 1;
    }
}

std::string toHex(std::uint64_t value, int width) {
    static const char* digits = "0123456789abcdef";
    std::string out;
    do {
        out.push_back(digits[value & 0xFu]);
        value >>= 4;
    } while (value != 0);
    while (static_cast<int>(out.size()) < width) {
        out.push_back('0');
    }
    std::reverse(out.begin(), out.end());
    return out;
}

bool parseInt(std::string_view text, int& out) {
    const std::string value = trim(text);
    if (value.empty()) {
        return false;
    }
    std::size_t consumed = 0;
    int parsed = 0;
    try {
        parsed = std::stoi(value, &consumed);
    } catch (...) {
        return false;
    }
    if (consumed != value.size()) {
        return false;
    }
    out = parsed;
    return true;
}

bool parseCanvasSize(std::string_view text, int minSize, int maxSize, int& width, int& height) {
    std::string normalized = toLower(trim(text));
    if (normalized.empty()) {
        return false;
    }
    std::replace(normalized.begin(), normalized.end(), '*', 'x');
    const std::vector<std::string> parts = split(normalized, 'x');

    int w = 0;
    int h = 0;
    if (parts.size() == 1) {
        if (!parseInt(parts[0], w)) {
            return false;
        }
        h = w;
    } else if (parts.size() == 2) {
        if (!parseInt(parts[0], w) || !parseInt(parts[1], h)) {
            return false;
        }
    } else {
        return false;
    }

    if (w < minSize || w > maxSize || h < minSize || h > maxSize) {
        return false;
    }
    width = w;
    height = h;
    return true;
}

std::string timestampNow() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t time = std::chrono::system_clock::to_time_t(now);
    std::tm tm{};
#if defined(_WIN32)
    localtime_s(&tm, &time);
#else
    localtime_r(&time, &tm);
#endif
    char buffer[32] = {};
    std::strftime(buffer, sizeof(buffer), "%Y%m%d-%H%M%S", &tm);
    return std::string(buffer);
}

std::string humanSize(std::uintmax_t bytes) {
    std::ostringstream out;
    if (bytes >= 1024 * 1024) {
        out.setf(std::ios::fixed);
        out.precision(1);
        out << (static_cast<double>(bytes) / (1024.0 * 1024.0)) << " MB";
    } else if (bytes >= 1024) {
        out << (bytes / 1024) << " KB";
    } else {
        out << bytes << " B";
    }
    return out.str();
}

}  // namespace bp::utils
