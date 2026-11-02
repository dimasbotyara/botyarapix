# botyarapix 🎨

Кроссплатформенная консольная утилита на **C++17**, которая генерирует абстрактное монохромное
пиксельное искусство и сохраняет его в PNG с апскейлом (nearest-neighbour).

```
botyarapix  monochrome pixel art generator
──────────────────────────────────────────
Preview 32x32 - Symmetry - seed 0000000c
   ##  ##    ##    ##  ##
 ######    ####    ######
   ##  ##    ##    ##  ##
```

---

## Быстрый старт

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/botyarapix                 # интерактивное меню
```

Зависимости уже лежат в `third_party/`, интернет не нужен:

| Библиотека | Где | Как подключена |
|---|---|---|
| `nlohmann/json` (v3.11.3, single header) | `third_party/nlohmann/json.hpp` | `find_package` → иначе bundled INTERFACE-таргет |
| `stb_image_write.h` | `third_party/stb/stb_image_write.h` | header-only, реализация внутри `ImageExporter.cpp` |

Если в системе есть `nlohmann-json3-dev` (Linux) или vcpkg-пакет — CMake подхватит его автоматически.
Ветка сборки под Windows: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` + `cmake --build build --config Release`.

Требования: CMake ≥ 3.16, компилятор с поддержкой C++17.

---

## Управление

| Клавиши | Действие |
|---|---|
| `↑` / `↓` или `K` / `J` | перемещение по меню |
| `1` … `9` | мгновенный выбор пункта |
| `Enter` | подтвердить |
| `Q` / `Esc` | отмена (выход из текущего экрана) |

Если `stdin` не терминал (пайп, CI, `|`), программа сама переключается на текстовый режим:
печатает нумерованный список и читает номер строки. Это удобно для автотестов.

---

## CLI-режим (без меню)

```bash
./build/botyarapix --list                        # список алгоритмов
./build/botyarapix --gen symmetry --size 32 --seed 42 --scale 16 --out ./art
./build/botyarapix --gen random --size 64x64 --out ./art --no-preview
./build/botyarapix --selftest                    # встроенные проверки
```

| Флаг | Значение |
|---|---|
| `-g, --gen <id>` | id алгоритма (`random`, `symmetry`, `cellular`, …) |
| `-s, --size <N\|WxH>` | размер холста, 4…512 |
| `--width`, `--height` | размеры по отдельности |
| `--scale <N>` | апскейл 1…64 |
| `--seed <N>` | фиксированный сид (десятичный или `0x…`) — для повторяемости |
| `--detail <0..100>` | «ручка сложности» для алгоритмов, которым она нужна |
| `--out <dir>` | папка сохранения (конфиг при этом не меняется) |
| `--lang <ru\|en>` | язык интерфейса |
| `--config <path>` | альтернативный путь конфига |
| `--no-preview` | не печатать ASCII-предпросмотр |
| `-l, --list`, `--selftest`, `-h, --help`, `-v, --version` | служебные |

Сид всегда печатается (`Seed: 0x…`), так что любую картинку можно воспроизвести байт-в-байт.

---

## Конфиг

Путь выбирается через `std::filesystem` и переменные окружения ОС:

| ОС | Файл |
|---|---|
| Linux | `$XDG_CONFIG_HOME/botyarapix/config.json` (иначе `~/.config/botyarapix/config.json`) |
| macOS | `~/Library/Application Support/botyarapix/config.json` |
| Windows | `%APPDATA%\botyarapix\config.json` |

Пример:

```json
{
    "$schema": "botyarapix/config/v1",
    "language": "ru",
    "outputDirectory": "/home/user/Pictures/botyarapix",
    "lastWidth": 32,
    "lastHeight": 32,
    "lastGenerator": "symmetry",
    "lastScale": 8
}
```

Особенности реализации:

* **Первый запуск**: файла нет → экран выбора языка (RU / EN) → конфиг сохраняется.
* Запись **атомарная** (`config.json.tmp` + `rename`), битый JSON не роняет программу — берутся
  значения по умолчанию.
* Значения после загрузки проходят `sanitize()`: размеры зажимаются в 4…512, неизвестный язык → `en`.
* Папка сохранения по умолчанию: `~/Pictures/botyarapix` (Windows: `%USERPROFILE%\Pictures\botyarapix`),
  а если `Pictures` нет — `./botyarapix-output`.

---

## Структура проекта

```
botyarapix/
├── CMakeLists.txt
├── README.md
├── docs/
│   └── generator-template.md          # как добавить новый алгоритм (шаблон .hpp/.cpp)
├── third_party/
│   ├── nlohmann/json.hpp
│   └── stb/stb_image_write.h
├── examples/                          # примеры, сгенерированные утилитой
└── src/
    ├── main.cpp                       # точка входа, разбор флагов, сборка зависимостей
    ├── app/
    │   ├── Application.hpp/.cpp        # все экраны: first-run, меню, генерация, сохранение
    ├── core/
    │   ├── Canvas.hpp                  # холст 8-битного серого (0…255)
    │   ├── ConfigManager.hpp/.cpp      # чтение/запись JSON-конфига
    │   ├── Translator.hpp/.cpp         # словари RU/EN + плейсхолдеры {0}
    │   ├── ImageExporter.hpp/.cpp      # апскейл nearest-neighbour + PNG (stb)
    │   ├── PlatformPaths.hpp/.cpp      # пути конфига/картинок для Linux/macOS/Windows
    │   ├── Terminal.hpp/.cpp           # TUI: меню со стрелками, raw-mode (RAII), fallback
    │   ├── Rng.hpp                     # mt19937_64 + удобные range/unit/chance
    │   └── Utils.hpp/.cpp              # trim, парсинг, hex, таймстамп, humanSize
    └── generators/
        ├── IGenerator.hpp              # интерфейс (Strategy)
        ├── GeneratorRegistry.hpp/.cpp  # реестр: единственное место регистрации
        ├── RandomGenerator.hpp/.cpp
        ├── SymmetryGenerator.hpp/.cpp
        └── CellularGenerator.hpp/.cpp
```

---

## Архитектура

### Поток экранов

```
main
 └─ Application
     ├─ первый запуск?  → выбор языка → config.json
     └─ Главное меню
         ├─ Создать изображение → размер → алгоритм → [предпросмотр]
         │     └─ Что дальше? → Регенерировать / Апскейл и сохранить / Назад
         ├─ Язык: RU / EN       (сразу сохраняется в конфиг)
         ├─ Папка сохранения
         └─ Выход
```

### Strategy Pattern для генераторов

```cpp
struct GenParams { int width, height; std::uint64_t seed; int detail; };

class IGenerator {
public:
    virtual std::string_view id() const noexcept = 0;          // "random"
    virtual std::string_view nameKey() const noexcept = 0;     // "gen.random.name"
    virtual std::string_view descriptionKey() const noexcept = 0;
    virtual void generate(Canvas& canvas, const GenParams& params) const = 0;
};
```

* Каждый алгоритм — отдельный класс в своих `.hpp`/`.cpp`, **stateless**: всё состояние
  (включая ГПСЧ) создаётся внутри `generate()` из `params.seed`.
* `GeneratorRegistry` владеет `std::vector<std::unique_ptr<IGenerator>>` и умеет `find(id)`.
  Меню, CLI и `--list` работают через реестр — новый алгоритм автоматически появляется везде.
* Сид детерминирован: `--seed` даёт идентичную картинку на любой платформе (`mt19937_64`).

### Холст и экспорт

* `Canvas` — `std::vector<std::uint8_t>` (0 = чёрный, 255 = белый). Ч/б алгоритмы пишут только
  0/255, но формат готов к градациям серого (Perlin, фракталы, reaction-diffusion).
* `ImageExporter::upscaleNearest` копирует каждый пиксель в блок `factor × factor` — никакого
  сглаживания, пиксели остаются квадратными.
* PNG пишется в 8-битном grayscale (`stbi_write_png_to_func`). Используется именно коллбэк-версия,
  чтобы писать через `std::ofstream(fs::path)` — так корректно работают юникод-пути на Windows.
* Имя файла: `<generator>_<W>x<H>_s<seed x8>_x<scale>_<YYYYMMDD-HHMMSS>.png`.

### Прочее

* Терминал: `Terminal` включает raw-mode через RAII-guard (`termios` / `conio.h`) и гарантированно
  возвращает нормальный режим при любом выходе; ANSI-цвета отключаются для не-TTY и при `NO_COLOR`.
* Ввод: стрелки, `j/k`, цифры, `Enter`, `Q`. Не-TTY → текстовый fallback (номера + `getline`).
* Пакетный CLI-режим **не изменяет** конфиг пользователя (папка берётся из `--out`).

---

## Готово / план

| # | Алгоритм | Статус | Заметки по реализации |
|---|---|---|---|
| 1 | Random | ✅ | белый шум 50/50 |
| 2 | Symmetry | ✅ | ось (вертикаль/горизонталь/обе) выбирается сидом |
| 3 | Cellular | ✅ | Game of Life B3/S23, тор, 6…20 поколений от `--detail` |
| 4 | Maze | ⬜ | рекурсивный DFS (итеративный стек), стены/проходы 1 px |
| 5 | PerlinNoise | ⬜ | классический Перлин + порог/постеризация в ч/б |
| 6 | Fractal | ⬜ | Мандельброт/Джулия, число итераций → яркость или порог |
| 7 | Spiral | ⬜ | архимедова/логарифмическая спираль, полярная деформация |
| 8 | Voronoi | ⬜ | точки + границы (расстояние до 2-х ближайших) |
| 9 | Geometric | ⬜ | случайные линии (Брезенхем) и рамки |
| 10 | Sierpinski | ⬜ | треугольник (хаос-игра) / ковёр |
| 11 | Gosper | ⬜ | L-system или замена гексов, порядок 2–4 |
| 12 | ReactionDiffusion | ⬜ | Gray-Scott, итерации от `--detail`, порог в ч/б |

Плюс идеи: дизеринг (Bayer/Floyd–Steinberg) как пост-обработка, «gallery»-режим (пачка картинок
за раз), экспорт в PBM, конфиг-пресеты, `--detail` в интерактивном меню.

---

## Как добавить новый генератор

Короткая версия (подробный шаблон — `docs/generator-template.md`):

1. `src/generators/MazeGenerator.hpp` — класс `MazeGenerator final : public IGenerator`
   с `id() == "maze"`, `nameKey() == "gen.maze.name"`.
2. `src/generators/MazeGenerator.cpp` — реализация `generate()` (ГПСЧ только из `params.seed`).
3. `src/core/Translator.cpp` — две строки в словарь: `gen.maze.name` и `gen.maze.desc`.
4. `GeneratorRegistry.cpp` — `#include` + `add(std::make_unique<MazeGenerator>());`.
5. `CMakeLists.txt` — дописать новый `.cpp` в `add_executable(botyarapix …)`.
6. Проверка: `./build/botyarapix --gen maze --size 32 --scale 8 --out ./art`.

Меню, `--list`, конфиг и сохранение подхватят алгоритм сами.

---

## Самопроверка

```bash
./build/botyarapix --selftest     # 3 генератора × 4 размера × 3 сида + запись PNG
ctest --test-dir build            # то же через CTest
```

В песочнице проверено: сборка без предупреждений (`-Wall -Wextra -Wpedantic`), PNG — валидный
8-битный grayscale, блоки апскейла строго однородные, 4-кратная симметрия подтверждена по пикселям.
