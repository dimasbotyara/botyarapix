// botyarapix — generators/GeneratorRegistry.hpp
// Реестр алгоритмов: единственное место, где нужно «прописать» новый генератор.
#pragma once

#include <memory>
#include <string_view>
#include <vector>

#include "generators/IGenerator.hpp"

namespace bp {

class GeneratorRegistry {
public:
    GeneratorRegistry();

    /// nullptr, если такого id нет.
    const IGenerator* find(std::string_view id) const;

    /// Все генераторы в порядке регистрации.
    const std::vector<std::unique_ptr<IGenerator>>& all() const noexcept { return generators_; }

    std::size_t size() const noexcept { return generators_.size(); }

private:
    void add(std::unique_ptr<IGenerator> generator);

    std::vector<std::unique_ptr<IGenerator>> generators_;
};

}  // namespace bp
