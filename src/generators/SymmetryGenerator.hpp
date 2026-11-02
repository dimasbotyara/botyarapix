// botyarapix — generators/SymmetryGenerator.hpp
#pragma once

#include "core/Rng.hpp"
#include "generators/IGenerator.hpp"

namespace bp {

/// Случайная половина (или квадрант) + её зеркальное отражение.
/// Ось симметрии выбирается сидом: вертикальная, горизонтальная или обе.
class SymmetryGenerator final : public IGenerator {
public:
    enum class Axis { Vertical, Horizontal, Both };

    std::string_view id() const noexcept override { return "symmetry"; }
    std::string_view nameKey() const noexcept override { return "gen.symmetry.name"; }
    std::string_view descriptionKey() const noexcept override { return "gen.symmetry.desc"; }

    void generate(Canvas& canvas, const GenParams& params) const override;

private:
    /// Заполняет область случайным шумом с «комками» — так отражение выглядит живее
    /// чистого шума, но остаётся абстрактным ч/б паттерном.
    static void fillRegion(Canvas& canvas, Rng& rng, int x0, int y0, int x1, int y1, double density);
};

}  // namespace bp
