// botyarapix — generators/CellularGenerator.cpp
#include "generators/CellularGenerator.hpp"

#include <algorithm>
#include <cstddef>
#include <vector>

#include "core/Rng.hpp"

namespace bp {
namespace {

/// Соседи по тору: холст «заворачивается», поэтому рисунок не липнет к краям.
int countNeighbours(const std::vector<std::uint8_t>& grid, int width, int height, int x, int y) {
    int count = 0;
    for (int dy = -1; dy <= 1; ++dy) {
        for (int dx = -1; dx <= 1; ++dx) {
            if (dx == 0 && dy == 0) {
                continue;
            }
            const int nx = (x + dx + width) % width;
            const int ny = (y + dy + height) % height;
            count += grid[static_cast<std::size_t>(ny) * width + nx] != 0 ? 1 : 0;
        }
    }
    return count;
}

}  // namespace

void CellularGenerator::generate(Canvas& canvas, const GenParams& params) const {
    const int width = canvas.width();
    const int height = canvas.height();
    Rng rng(params.seed);

    std::vector<std::uint8_t> current(static_cast<std::size_t>(width) * height, 0);
    std::vector<std::uint8_t> next(current.size(), 0);

    const double density = 0.38 + 0.14 * rng.unit();
    for (auto& cell : current) {
        cell = rng.chance(density) ? 1 : 0;
    }

    const int steps = 6 + std::clamp(params.detail, 0, 100) / 7;  // 6..20 поколений
    for (int step = 0; step < steps; ++step) {
        std::size_t alive = 0;
        for (int y = 0; y < height; ++y) {
            for (int x = 0; x < width; ++x) {
                const int neighbours = countNeighbours(current, width, height, x, y);
                const bool wasAlive = current[static_cast<std::size_t>(y) * width + x] != 0;
                // B3/S23 — классические правила Конвея.
                const bool aliveNow = wasAlive ? (neighbours == 2 || neighbours == 3)
                                               : (neighbours == 3);
                next[static_cast<std::size_t>(y) * width + x] = aliveNow ? 1 : 0;
                alive += aliveNow ? 1 : 0;
            }
        }
        current.swap(next);

        const double fraction = static_cast<double>(alive) / static_cast<double>(current.size());
        if (fraction < 0.01) {
            break;  // колония вымерла — дальше смотреть нечего
        }
    }

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            canvas.at(x, y) =
                current[static_cast<std::size_t>(y) * width + x] != 0 ? kWhite : kBlack;
        }
    }
}

}  // namespace bp
