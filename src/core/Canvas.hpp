// botyarapix — core/Canvas.hpp
// Монохромный холст: 8-битная градация серого (0 = чёрный, 255 = белый).
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace bp {

inline constexpr std::uint8_t kBlack = 0;
inline constexpr std::uint8_t kWhite = 255;

/// Прямоугольный холст пикселей.
/// Значение 0..255 — градация серого; чёрно-белые алгоритмы пишут только kBlack/kWhite.
class Canvas {
public:
    Canvas(int width, int height, std::uint8_t fill = kBlack)
        : width_(width > 0 ? width : 1),
          height_(height > 0 ? height : 1),
          pixels_(static_cast<std::size_t>(width_) * static_cast<std::size_t>(height_), fill) {}

    int width() const noexcept { return width_; }
    int height() const noexcept { return height_; }
    std::size_t pixelCount() const noexcept { return pixels_.size(); }

    std::uint8_t* data() noexcept { return pixels_.data(); }
    const std::uint8_t* data() const noexcept { return pixels_.data(); }

    bool inBounds(int x, int y) const noexcept {
        return x >= 0 && y >= 0 && x < width_ && y < height_;
    }

    /// Без проверки границ (быстро).
    std::uint8_t& at(int x, int y) noexcept { return pixels_[index(x, y)]; }
    std::uint8_t at(int x, int y) const noexcept { return pixels_[index(x, y)]; }

    /// С проверкой границ: за пределами холста — no-op.
    void set(int x, int y, std::uint8_t value) noexcept {
        if (inBounds(x, y)) {
            pixels_[index(x, y)] = value;
        }
    }

    /// Значение с проверкой границ (вне холста — kBlack).
    std::uint8_t get(int x, int y) const noexcept {
        return inBounds(x, y) ? pixels_[index(x, y)] : kBlack;
    }

    void fill(std::uint8_t value) noexcept {
        std::fill(pixels_.begin(), pixels_.end(), value);
    }

    void invert() noexcept {
        for (auto& p : pixels_) {
            p = static_cast<std::uint8_t>(255 - p);
        }
    }

private:
    std::size_t index(int x, int y) const noexcept {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(width_) +
               static_cast<std::size_t>(x);
    }

    int width_;
    int height_;
    std::vector<std::uint8_t> pixels_;
};

}  // namespace bp
