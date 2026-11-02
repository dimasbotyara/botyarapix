// botyarapix — generators/SymmetryGenerator.cpp
#include "generators/SymmetryGenerator.hpp"

#include <algorithm>

namespace bp {

void SymmetryGenerator::fillRegion(Canvas& canvas, Rng& rng, int x0, int y0, int x1, int y1,
                                   double density) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            canvas.at(x, y) = rng.chance(density) ? kWhite : kBlack;
        }
    }

    // Немного «штрихов», чтобы отражение не было однородной кашей.
    const int regionWidth = x1 - x0;
    const int regionHeight = y1 - y0;
    const int strokes = std::max(1, (regionWidth * regionHeight) / 48);
    for (int i = 0; i < strokes; ++i) {
        const bool horizontal = rng.coin();
        const int limit = horizontal ? regionWidth : regionHeight;
        const int length = rng.range(2, std::max(2, limit / 2));
        const int startX = rng.range(x0, x1 - 1);
        const int startY = rng.range(y0, y1 - 1);
        const std::uint8_t value = rng.chance(0.7) ? kWhite : kBlack;

        for (int step = 0; step < length; ++step) {
            const int px = horizontal ? startX + step : startX;
            const int py = horizontal ? startY : startY + step;
            if (px >= x1 || py >= y1) {
                break;
            }
            canvas.at(px, py) = value;
        }
    }
}

void SymmetryGenerator::generate(Canvas& canvas, const GenParams& params) const {
    Rng rng(params.seed);

    const int width = canvas.width();
    const int height = canvas.height();
    const int halfWidth = (width + 1) / 2;
    const int halfHeight = (height + 1) / 2;
    const double density = 0.30 + 0.35 * rng.unit();
    const Axis axis = static_cast<Axis>(rng.range(0, 2));

    canvas.fill(kBlack);

    switch (axis) {
        case Axis::Vertical:  // левая половина -> правая
            fillRegion(canvas, rng, 0, 0, halfWidth, height, density);
            for (int y = 0; y < height; ++y) {
                for (int x = 0; x < halfWidth; ++x) {
                    canvas.at(width - 1 - x, y) = canvas.at(x, y);
                }
            }
            break;

        case Axis::Horizontal:  // верхняя половина -> нижняя
            fillRegion(canvas, rng, 0, 0, width, halfHeight, density);
            for (int y = 0; y < halfHeight; ++y) {
                for (int x = 0; x < width; ++x) {
                    canvas.at(x, height - 1 - y) = canvas.at(x, y);
                }
            }
            break;

        case Axis::Both:  // квадрант -> все четыре
            fillRegion(canvas, rng, 0, 0, halfWidth, halfHeight, density);
            for (int y = 0; y < halfHeight; ++y) {
                for (int x = 0; x < halfWidth; ++x) {
                    const std::uint8_t value = canvas.at(x, y);
                    canvas.at(width - 1 - x, y) = value;
                    canvas.at(x, height - 1 - y) = value;
                    canvas.at(width - 1 - x, height - 1 - y) = value;
                }
            }
            break;
    }
}

}  // namespace bp
