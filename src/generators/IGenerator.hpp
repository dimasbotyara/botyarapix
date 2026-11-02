// botyarapix — generators/IGenerator.hpp
// Общий интерфейс всех алгоритмов (Strategy Pattern).
#pragma once

#include <cstdint>
#include <string_view>

#include "core/Canvas.hpp"

namespace bp {

/// Входные параметры генерации. Всё, что нужно алгоритму, приходит здесь —
/// сами генераторы не хранят состояние между вызовами.
struct GenParams {
    int width = 32;
    int height = 32;
    std::uint64_t seed = 0;
    /// Универсальная «ручка сложности» 0..100 (шаги автомата, плотность, количество фигур).
    int detail = 50;
};

class IGenerator {
public:
    virtual ~IGenerator() = default;

    /// Уникальный идентификатор: "random", "symmetry", ... (используется в CLI и конфиге).
    virtual std::string_view id() const noexcept = 0;

    /// Ключи переводов: "gen.<id>.name" / "gen.<id>.desc".
    virtual std::string_view nameKey() const noexcept = 0;
    virtual std::string_view descriptionKey() const noexcept = 0;

    /// Рисует в canvas. canvas уже нужного размера (width x height из params).
    virtual void generate(Canvas& canvas, const GenParams& params) const = 0;
};

}  // namespace bp
