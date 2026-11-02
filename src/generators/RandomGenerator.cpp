// botyarapix — generators/RandomGenerator.cpp
#include "generators/RandomGenerator.hpp"

#include "core/Rng.hpp"

namespace bp {

void RandomGenerator::generate(Canvas& canvas, const GenParams& params) const {
    Rng rng(params.seed);

    for (int y = 0; y < canvas.height(); ++y) {
        for (int x = 0; x < canvas.width(); ++x) {
            canvas.at(x, y) = rng.coin() ? kWhite : kBlack;
        }
    }
}

}  // namespace bp
