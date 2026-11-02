# Как добавить новый генератор

Пошаговый шаблон для `botyarapix`. Пример — новый алгоритм «Maze» (лабиринт).

---

## 1. Заголовочный файл `src/generators/MazeGenerator.hpp`

```cpp
// botyarapix — generators/MazeGenerator.hpp
#pragma once

#include "generators/IGenerator.hpp"

namespace bp {

/// Лабиринт: рекурсивный поиск в глубину (итеративный стек), 1 px стены.
class MazeGenerator final : public IGenerator {
public:
    std::string_view id() const noexcept override { return "maze"; }
    std::string_view nameKey() const noexcept override { return "gen.maze.name"; }
    std::string_view descriptionKey() const noexcept override { return "gen.maze.desc"; }

    void generate(Canvas& canvas, const GenParams& params) const override;
};

}  // namespace bp
```

Правила для генератора:

* наследование `final`, метод `generate()` — `const` (генераторы без состояния);
* `id()` — нижний регистр, без пробелов: он попадает в `--gen`, конфиг и имя файла;
* вся случайность — только из `bp::Rng rng(params.seed)`; системное время/`random_device` нельзя,
  иначе ломается воспроизводимость по `--seed`;
* если алгоритму нужна «сложность» — читайте `params.detail` (0…100), не хардкодьте магию;
* рисуйте через `canvas.set(x, y, v)` (с проверкой границ) или `canvas.at(x, y)` (быстро, без проверки);
  значения — `kBlack` / `kWhite`, для полутонов допустимы 1…254.

## 2. Реализация `src/generators/MazeGenerator.cpp`

```cpp
// botyarapix — generators/MazeGenerator.cpp
#include "generators/MazeGenerator.hpp"

#include <algorithm>
#include <vector>

#include "core/Rng.hpp"

namespace bp {
namespace {

struct Cell { int x; int y; };

}  // namespace

void MazeGenerator::generate(Canvas& canvas, const GenParams& params) const {
    Rng rng(params.seed);
    canvas.fill(kWhite);  // стены — чёрные, проходы — белые

    const int step = 2;  // 2 px на клетку: 1 проход + 1 стена
    const int cols = std::max(1, (canvas.width() - 1) / step);
    const int rows = std::max(1, (canvas.height() - 1) / step);

    std::vector<std::uint8_t> visited(static_cast<std::size_t>(cols) * rows, 0);
    std::vector<Cell> stack;
    stack.push_back({0, 0});
    visited[0] = 1;
    canvas.set(0, 0, kBlack);

    while (!stack.empty()) {
        const Cell current = stack.back();
        // ... здесь: собрать непосещённых соседей, выбрать случайного,
        // пробить стену между клетками (canvas.set(..., kBlack)) и положить в стек;
        // если соседей нет — stack.pop_back().
        // Ключевое: используем rng.range(...) и покрываем весь холст.
        stack.pop_back();
    }
    (void)rng;
}

}  // namespace bp
```

Каркас намеренно короткий: важно соблюсти контракт, а тело алгоритма можно писать как удобно.

## 3. Переводы `src/core/Translator.cpp`

Добавьте две строки в словарь `dictionary()`:

```cpp
{"gen.maze.name", {"Maze", "Лабиринт (Maze)"}},
{"gen.maze.desc", {"Depth-first search maze", "Лабиринт (поиск в глубину)"}},
```

Ключи `nameKey()`/`descriptionKey()` должны совпадать с этими именами.

## 4. Регистрация `src/generators/GeneratorRegistry.cpp`

```cpp
#include "generators/MazeGenerator.hpp"
// ...
add(std::make_unique<MazeGenerator>());
```

## 5. Сборка `CMakeLists.txt`

В список источников `add_executable(botyarapix …)`:

```cmake
    src/generators/MazeGenerator.cpp
```

## 6. Проверка

```bash
cmake --build build -j
./build/botyarapix --list                              # алгоритм появился в списке
./build/botyarapix --gen maze --size 32 --scale 8 --out ./art --seed 1
./build/botyarapix --selftest                          # общий самотест
```

Меню, `--list`, конфиг (`lastGenerator`) и сохранение PNG подхватят режим автоматически —
больше нигде править ничего не нужно.

---

## Чек-лист качества

- [ ] сборка без предупреждений (`-Wall -Wextra -Wpedantic`);
- [ ] работает на 8x8, 16x16, 64x64, 128x128 (нет деления на ноль / выхода за границы);
- [ ] при одинаковом `--seed` результат идентичен;
- [ ] рисунок не «пустой» (не вся заливка) и не забит на 100 %;
- [ ] холст не-квадратного размера (например `--size 48x24`) не ломает алгоритм;
- [ ] `--selftest` проходит без `FAIL`.
