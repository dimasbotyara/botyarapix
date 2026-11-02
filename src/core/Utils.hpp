// botyarapix — core/Utils.hpp
// Мелкие строковые/временные утилиты без внешних зависимостей.
#pragma once

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace bp::utils {

/// Убирает пробелы/табы по краям.
std::string trim(std::string_view text);

/// Нижний регистр (ASCII).
std::string toLower(std::string_view text);

/// Разбивает строку по разделителю.
std::vector<std::string> split(std::string_view text, char separator);

/// Шестнадцатеричное представление значения, дополненное нулями до width.
std::string toHex(std::uint64_t value, int width);

/// Пытается распарсить целое; false — если строка не число или есть мусор.
bool parseInt(std::string_view text, int& out);

/// Разбирает размер холста: "48" -> 48x48, "32x64" -> 32x64 (допустимы x, X, *).
/// Границы minSize..maxSize проверяются для обеих сторон.
bool parseCanvasSize(std::string_view text, int minSize, int maxSize, int& width, int& height);

/// Локальная метка времени вида 20261102-204501 (для имён файлов).
std::string timestampNow();

/// Человекочитаемый размер файла: "412 KB".
std::string humanSize(std::uintmax_t bytes);

}  // namespace bp::utils
