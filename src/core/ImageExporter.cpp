// botyarapix — core/ImageExporter.cpp
#include "core/ImageExporter.hpp"

#include <fstream>
#include <system_error>
#include <vector>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb/stb_image_write.h"

#include "core/PlatformPaths.hpp"
#include "core/Utils.hpp"

namespace bp {
namespace {

/// stb пишет через коллбэк — так мы не зависим от fopen и корректно
/// работаем с юникод-путями на Windows (std::ofstream(fs::path)).
void writeToStream(void* context, void* data, int size) {
    auto* out = static_cast<std::ofstream*>(context);
    if (out != nullptr && data != nullptr && size > 0) {
        out->write(static_cast<const char*>(data), size);
    }
}

std::string sanitizeForFileName(std::string_view text) {
    std::string out;
    out.reserve(text.size());
    for (char c : text) {
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '-' || c == '_';
        out.push_back(ok ? c : '_');
    }
    return out;
}

}  // namespace

Canvas ImageExporter::upscaleNearest(const Canvas& source, int factor) {
    if (factor <= 1) {
        return source;
    }

    const int newWidth = source.width() * factor;
    const int newHeight = source.height() * factor;
    Canvas out(newWidth, newHeight);

    for (int y = 0; y < source.height(); ++y) {
        const std::uint8_t* row = source.data() + static_cast<std::size_t>(y) * source.width();
        for (int fy = 0; fy < factor; ++fy) {
            std::uint8_t* dst = out.data() + static_cast<std::size_t>(y * factor + fy) * newWidth;
            for (int x = 0; x < source.width(); ++x) {
                const std::uint8_t value = row[x];
                for (int fx = 0; fx < factor; ++fx) {
                    dst[x * factor + fx] = value;
                }
            }
        }
    }
    return out;
}

bool ImageExporter::ensureDirectory(const fs::path& directory, std::string* error) {
    if (directory.empty()) {
        if (error != nullptr) {
            *error = "empty output directory";
        }
        return false;
    }
    std::error_code ec;
    if (fs::exists(directory, ec)) {
        if (fs::is_directory(directory, ec)) {
            return true;
        }
        if (error != nullptr) {
            *error = platform::toUtf8(directory) + " is not a directory";
        }
        return false;
    }
    fs::create_directories(directory, ec);
    if (ec) {
        if (error != nullptr) {
            *error = ec.message();
        }
        return false;
    }
    return true;
}

ImageExporter::Result ImageExporter::savePng(const Canvas& canvas, int scale, const fs::path& file) {
    Result result;

    if (scale < 1 || scale > 64) {
        result.error = "upscale factor must be within 1..64";
        return result;
    }

    std::string dirError;
    if (!ensureDirectory(file.parent_path(), &dirError)) {
        result.error = dirError;
        return result;
    }

    const Canvas upscaled = upscaleNearest(canvas, scale);

    std::ofstream out(file, std::ios::binary | std::ios::trunc);
    if (!out) {
        result.error = "cannot open file for writing";
        return result;
    }

    const int written = stbi_write_png_to_func(writeToStream, &out, upscaled.width(),
                                               upscaled.height(), 1, upscaled.data(),
                                               upscaled.width());
    out.flush();
    const bool streamOk = static_cast<bool>(out);
    out.close();

    if (written == 0 || !streamOk) {
        result.error = written == 0 ? "stb_image_write failed" : "write error";
        std::error_code ec;
        fs::remove(file, ec);
        return result;
    }

    std::error_code ec;
    result.bytes = fs::file_size(file, ec);
    if (ec) {
        result.bytes = 0;
    }
    result.ok = true;
    return result;
}

fs::path ImageExporter::buildFileName(const fs::path& directory, std::string_view generatorId,
                                      int width, int height, std::uint64_t seed, int scale) {
    std::string name = sanitizeForFileName(generatorId);
    name += '_';
    name += std::to_string(width);
    name += 'x';
    name += std::to_string(height);
    name += "_s";
    name += utils::toHex(seed, 8);
    name += "_x";
    name += std::to_string(scale);
    name += '_';
    name += utils::timestampNow();
    name += ".png";
    return directory / name;
}

}  // namespace bp
