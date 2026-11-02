// botyarapix — generators/GeneratorRegistry.cpp
#include "generators/GeneratorRegistry.hpp"

#include "generators/CellularGenerator.hpp"
#include "generators/RandomGenerator.hpp"
#include "generators/SymmetryGenerator.hpp"

namespace bp {

GeneratorRegistry::GeneratorRegistry() {
    // --- Регистрация алгоритмов. Новый режим = новый #include + одна строка. ---
    add(std::make_unique<RandomGenerator>());
    add(std::make_unique<SymmetryGenerator>());
    add(std::make_unique<CellularGenerator>());
    // Дальше по плану: Maze, PerlinNoise, Fractal, Spiral, Voronoi,
    // Geometric, Sierpinski, Gosper, ReactionDiffusion.
}

void GeneratorRegistry::add(std::unique_ptr<IGenerator> generator) {
    if (generator != nullptr) {
        generators_.push_back(std::move(generator));
    }
}

const IGenerator* GeneratorRegistry::find(std::string_view id) const {
    for (const auto& generator : generators_) {
        if (generator->id() == id) {
            return generator.get();
        }
    }
    return nullptr;
}

}  // namespace bp
