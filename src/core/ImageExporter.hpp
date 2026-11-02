// botyarapix — core/ImageExporter.hpp
// Апскейл nearest-neighbour + сохранение PNG через stb_image_write.
#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <string_view>

#include "core/Canvas.hpp"

namespace bp {

namespace fs = std::filesystem;

class ImageExporter {
public:
    struct Result {
        bool ok = false;
        std::uintmax_t bytes = 0;
        std::string error;

        explicit operator bool() const noexcept { return ok; }
    };

    /// Увеличение без сглаживания: каждый пиксель становится блоком factor x factor.
    /// При factor <= 1 возвращает копию холста.
    static Canvas upscaleNearest(const Canvas& source, int factor);

    /// Апскейл + запись 8-битного grayscale PNG. Папка создаётся при необходимости.
    static Result savePng(const Canvas& canvas, int scale, const fs::path& file);

    /// Имя файла вида random_32x32_s-a1b2c3d4_x8_20261102-204501.png
    static fs::path buildFileName(const fs::path& directory, std::string_view generatorId,
                                  int width, int height, std::uint64_t seed, int scale);

    /// Гарантирует существование папки; false + error при неудаче.
    static bool ensureDirectory(const fs::path& directory, std::string* error);
};

}  // namespace bp
