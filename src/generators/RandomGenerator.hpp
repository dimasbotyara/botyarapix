// botyarapix — generators/RandomGenerator.hpp
#pragma once

#include "generators/IGenerator.hpp"

namespace bp {

/// Чистый белый шум: каждый пиксель независимо чёрный или белый.
class RandomGenerator final : public IGenerator {
public:
    std::string_view id() const noexcept override { return "random"; }
    std::string_view nameKey() const noexcept override { return "gen.random.name"; }
    std::string_view descriptionKey() const noexcept override { return "gen.random.desc"; }

    void generate(Canvas& canvas, const GenParams& params) const override;
};

}  // namespace bp
