// botyarapix — generators/CellularGenerator.hpp
#pragma once

#include "generators/IGenerator.hpp"

namespace bp {

/// Игра «Жизнь» (B3/S23) на торе, остановленная на N-м шаге.
/// N зависит от params.detail: 6..20 поколений.
class CellularGenerator final : public IGenerator {
public:
    std::string_view id() const noexcept override { return "cellular"; }
    std::string_view nameKey() const noexcept override { return "gen.cellular.name"; }
    std::string_view descriptionKey() const noexcept override { return "gen.cellular.desc"; }

    void generate(Canvas& canvas, const GenParams& params) const override;
};

}  // namespace bp
