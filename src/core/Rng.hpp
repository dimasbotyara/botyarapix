// botyarapix — core/Rng.hpp
// Детерминированный ГПСЧ: один и тот же сид => один и тот же рисунок.
#pragma once

#include <cstdint>
#include <random>

namespace bp {

class Rng {
public:
    explicit Rng(std::uint64_t seed) : engine_(seed) {}

    std::uint32_t u32() { return static_cast<std::uint32_t>(engine_()); }
    std::uint64_t u64() { return engine_(); }

    /// Случайное целое в [lo, hi] включительно.
    int range(int lo, int hi) {
        if (hi <= lo) {
            return lo;
        }
        std::uniform_int_distribution<int> dist(lo, hi);
        return dist(engine_);
    }

    /// Случайное вещественное в [0, 1).
    double unit() {
        return std::uniform_real_distribution<double>(0.0, 1.0)(engine_);
    }

    /// true с вероятностью p.
    bool chance(double p) { return unit() < p; }

    bool coin() { return (engine_() & 1u) != 0u; }

    std::mt19937_64& engine() noexcept { return engine_; }

private:
    std::mt19937_64 engine_;
};

}  // namespace bp
