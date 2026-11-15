#!/usr/bin/env python3
"""Сценарии проверки TUI botyarapix в настоящем псевдо-терминале.

Зачем: меню работает с raw-mode, escape-кодами и перерисовкой — обычный
`ctest` этого не видит. Скрипт запускает бинарь в pty, эмулирует терминал
и проверяет то, что реально увидит пользователь (в том числе в Termux).

Требования: Python 3.8+, Linux/macOS. Никаких зависимостей — VT-эмулятор
лежит рядом (`vt_emulator.py`).

Запуск:
    python3 tests/tui_scenarios.py --binary ./build/botyarapix
    python3 tests/tui_scenarios.py --binary ./build/botyarapix --verbose

Скрипт ничего не меняет в твоём конфиге: используется временный
XDG_CONFIG_HOME во временном каталоге.
"""

from __future__ import annotations

import argparse
import fcntl
import os
import pty
import select
import shutil
import struct
import sys
import tempfile
import termios
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from vt_emulator import VT  # noqa: E402

COLS, ROWS = 80, 30

_failures: list[str] = []
_checks = 0
_verbose = False
_config_root = ""
_binary = ""


def check(name: str, condition: bool, detail: str = "") -> bool:
    global _checks
    _checks += 1
    mark = "OK  " if condition else "FAIL"
    print(f"  [{mark}] {name}" + (f"  -> {detail}" if detail and not condition else ""))
    if not condition:
        _failures.append(name)
    return condition


class Session:
    """Один запуск программы в pty + эмулятор экрана."""

    def __init__(self, args=None, cols=COLS, rows=ROWS, env_extra=None, config=True):
        self.vt = VT(cols, rows)
        self.raw = b""
        env = dict(os.environ)
        env["XDG_CONFIG_HOME"] = _config_root
        env["COLUMNS"] = str(cols)
        env["LINES"] = str(rows)
        env.pop("NO_COLOR", None)
        if env_extra:
            env.update(env_extra)
        argv = [_binary]
        if config:
            argv += ["--config", os.path.join(_config_root, "botyarapix", "config.json")]
        argv += args or []

        self.pid, self.fd = pty.fork()
        if self.pid == 0:  # дочерний процесс
            try:
                os.execvpe(_binary, argv, env)
            finally:
                os._exit(127)
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.pause(0.6)

    def drain(self, budget=0.05):
        while self.fd >= 0:
            ready, _, _ = select.select([self.fd], [], [], budget)
            if not ready:
                return
            try:
                data = os.read(self.fd, 65536)
            except OSError:
                return
            if not data:
                return
            self.raw += data
            self.vt.feed(data)

    def pause(self, seconds=0.35):
        end = time.time() + seconds
        while time.time() < end:
            self.drain()
            time.sleep(0.01)

    def send(self, data: bytes, pause=0.35):
        if self.fd < 0:
            return
        os.write(self.fd, data)
        self.pause(pause)

    def text(self) -> str:
        return self.vt.text()

    def selected(self) -> str:
        lines = self.vt.highlighted_lines()
        if lines:
            return lines[0]
        for line in self.vt.lines():
            if line.startswith("> "):
                return line
        return ""

    def resize(self, cols, rows):
        fcntl.ioctl(self.fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, cols, 0, 0))
        self.vt.resize(cols, rows)
        self.pause(0.45)

    def close(self) -> int:
        for _ in range(2):
            try:
                os.write(self.fd, b"q")
                self.pause(0.25)
            except OSError:
                break
        try:
            os.close(self.fd)
        except OSError:
            pass
        self.fd = -1
        try:
            _, status = os.waitpid(self.pid, 0)
        except ChildProcessError:
            status = 0
        return status


def write_config(language="en", size=32, generator="symmetry", scale=8):
    directory = os.path.join(_config_root, "botyarapix")
    shutil.rmtree(_config_root, ignore_errors=True)
    os.makedirs(directory, exist_ok=True)
    with open(os.path.join(directory, "config.json"), "w", encoding="utf-8") as handle:
        handle.write(
            '{"$schema":"botyarapix/config/v1","language":"%s","lastWidth":%d,'
            '"lastHeight":%d,"lastGenerator":"%s","lastScale":%d}'
            % (language, size, size, generator, scale)
        )


# --------------------------------------------------------------------- сценарии


def scenario_arrow_echo():
    """Исходный баг: в каноническом режиме терминал эхом печатал ^[[B."""
    print("\n1) Стрелки не эхом печатают ^[[B (raw-mode включён)")
    write_config("ru")
    session = Session()
    for key in (b"\x1b[A", b"\x1b[B", b"\x1b[C", b"\x1b[D", b"\x1b[H", b"\x1b[F",
                b"\x1b[5~", b"\x1b[6~", b"\x1bOA", b"\x1bOB", b"j", b"k"):
        session.send(key, pause=0.22)
    check("нет эха ^[[B", b"^[[B" not in session.raw, repr(session.raw[:160]))
    check("нет эха ^[[A", b"^[[A" not in session.raw)
    check("на экране нет мусора ^[", "^[" not in session.text())
    check("меню отрисовано", "Главное меню" in session.text())
    session.close()


def scenario_navigation():
    print("\n2) Навигация и границы списка")
    write_config("en")
    session = Session()
    session.send(b"\x1b[B")
    check("↓ подсвечивает 2-й пункт", session.selected().startswith(">  2. Language"),
          session.selected())
    for _ in range(2):
        session.send(b"\x1b[B", pause=0.25)
    check("↓ доходит до последнего пункта", session.selected().startswith(">  4. Exit"),
          session.selected())
    session.send(b"\x1b[B")
    check("↓ заворачивается на первый", session.selected().startswith(">  1. New image"),
          session.selected())
    session.send(b"\x1b[A")
    check("↑ заворачивается на последний", session.selected().startswith(">  4. Exit"),
          session.selected())
    session.send(b"\x1b[H")
    check("Home ведёт в начало", session.selected().startswith(">  1. New image"),
          session.selected())
    session.send(b"\x1b[F")
    check("End ведёт в конец", session.selected().startswith(">  4. Exit"), session.selected())
    session.send(b"2")
    session.pause(0.4)
    check("цифра 2 открывает экран языка", "Choose language" in session.text())
    session.send(b"\x03")  # Ctrl+C = отмена экрана, не выход из программы
    session.pause(0.4)
    check("Ctrl+C возвращает в меню", "Main menu" in session.text())
    session.send(b"\x1b[B")
    check("после Ctrl+C меню продолжает работать",
          session.selected().startswith(">  2. Language"), session.selected())
    status = session.close()
    check("обычный выход с кодом 0", status == 0, str(status))


def scenario_groups():
    print("\n3) Разделители и заголовки групп")
    write_config("en")
    session = Session()
    check("разделитель нарисован", "─────" in session.text())
    check("разделитель не пронумерован", "4. ─" not in session.text())
    check("нумерация после разделителя не сбилась", "4. Exit" in session.text())
    session.close()


def scenario_generator_flow():
    print("\n4) Полный цикл: размер → алгоритм → сохранение")
    write_config("en")
    session = Session()
    session.send(b"1")
    check("экран размера", "Choose canvas size" in session.text())
    session.send(b"3")
    check("экран алгоритма", "Choose algorithm" in session.text())
    check("у алгоритмов есть описание", "white noise" in session.text().lower()
          or "life" in session.text().lower())
    session.send(b"2")
    session.pause(0.5)
    check("предпросмотр нарисован", any(ch in session.text() for ch in "#@%*+="))
    check("меню «что дальше»", "What's next?" in session.text())
    session.send(b"2")
    check("экран апскейла", "Choose upscale factor" in session.text())
    session.send(b"2")
    session.pause(0.6)
    check("файл сохранён", "Saved:" in session.text(), session.text()[-200:])
    session.send(b"\r")
    check("возврат к предпросмотру", "What's next?" in session.text())
    session.close()


def scenario_custom_size():
    print("\n5) Свой размер и обработка ошибок ввода")
    write_config("en", generator="random")
    session = Session()
    session.send(b"1")
    session.send(b"6")  # Custom size...
    check("спросили размер", "Size (e.g." in session.text())
    session.send(b"abc\r")
    check("неверный ввод объяснён", "Invalid size" in session.text())
    session.send(b"24x48\r")
    session.pause(0.5)
    check("после валидного размера идём к алгоритму", "Choose algorithm" in session.text())
    session.close()


def scenario_typeahead():
    print("\n6) Лишние нажатия не протекают в следующий ввод")
    write_config("en")
    session = Session()
    session.send(b"2")
    session.send(b"\r")
    check("смена языка подтверждена", "Language switched" in session.text())
    session.send(b"\r", pause=0.5)
    check("один Enter закрывает паузу", "Main menu" in session.text())
    session.close()


def scenario_narrow():
    print("\n7) Узкий терминал (телефон, Termux)")
    write_config("ru")
    session = Session(cols=40, rows=24)
    session.send(b"\x1b[B")
    session.send(b"\x1b[B")
    check("строки не переносятся", max(len(l) for l in session.vt.lines()) <= 40,
          str(max(len(l) for l in session.vt.lines())))
    check("длинное обрезано многоточием", "…" in session.text())
    session.resize(24, 40)  # экран «повернули»
    check("после ресайза курсор остался на 3-м пункте",
          session.selected().startswith(">  3."), session.selected())
    session.send(b"\x1b[A")
    check("после ресайза навигация работает",
          session.selected().startswith(">  2."), session.selected())
    check("после ресайза нет мусора", session.text().count("Папка сохранения") == 1)
    session.close()

    session = Session(cols=20, rows=10)
    session.send(b"\x1b[B")
    check("20x10: без переносов", max(len(l) for l in session.vt.lines()) <= 20)
    session.close()


def scenario_paste():
    print("\n8) Вставка из буфера обмена не «нажимает» пункты")
    write_config("en")
    session = Session()
    session.send(b"\x1b[200~2\n\x1b[201~")
    check("после вставки курсор на месте", session.selected().startswith(">  1. New image"),
          session.selected())
    check("меню не открылось само", "Choose language" not in session.text())
    session.close()


def scenario_fallbacks():
    print("\n9) Резервные режимы: NO_COLOR, TERM=dumb, пайп")
    write_config("en")
    session = Session(env_extra={"NO_COLOR": "1"})
    session.send(b"\x1b[B")
    check("NO_COLOR: меню работает", session.selected().startswith(">  2. Language"),
          session.selected())
    check("NO_COLOR: без SGR-кодов", b"\x1b[1m" not in session.raw
          and b"\x1b[7m" not in session.raw)
    session.close()

    write_config("en")
    session = Session(env_extra={"TERM": "dumb"}, cols=80, rows=40)
    check("TERM=dumb: нет очистки экрана", b"\x1b[2J" not in session.raw)
    check("TERM=dumb: текстовый список", "1. New image" in session.text())
    session.send(b"1\r")
    check("TERM=dumb: ввод цифры работает", "Choose canvas size" in session.text())
    session.close()

    write_config("en")
    pipe = os.popen(
        "printf '1\\n3\\n2\\n2\\n2\\n\\n\\n' | %s --config %s 2>&1"
        % (_binary, os.path.join(_config_root, "botyarapix", "config.json"))
    )
    output = pipe.read()
    pipe.close()
    check("пайп: без ANSI-кодов", "\x1b[" not in output)
    check("пайп: работает нумерация", "Choose canvas size" in output)


def scenario_signals():
    print("\n10) Терминал восстанавливается при завершении")
    write_config("en")
    session = Session()
    session.send(b"\x1b[B")
    session.close()
    check("после выхода нет эха escape-кодов", b"^[[B" not in session.raw)

    session = Session()
    os.kill(session.pid, 15)  # SIGTERM прямо в raw-mode
    session.pause(0.5)
    try:
        _, status = os.waitpid(session.pid, 0)
    except ChildProcessError:
        status = 0
    check("SIGTERM завершает процесс", os.WIFEXITED(status) or os.WIFSIGNALED(status))
    session.pause(0.2)


# ------------------------------------------------------------------------- main


def main() -> int:
    global _verbose, _config_root, _binary

    parser = argparse.ArgumentParser(description="TUI-сценарии botyarapix в pty")
    parser.add_argument("--binary", default="./build/botyarapix", help="путь к собранному бинарю")
    parser.add_argument("--verbose", action="store_true", help="печатать экраны при ошибках")
    parser.add_argument("--keep", action="store_true", help="не удалять временный конфиг")
    args = parser.parse_args()

    _verbose = args.verbose
    _binary = os.path.abspath(args.binary)
    if not os.path.exists(_binary):
        print(f"не найден бинарь: {_binary}\nсначала собери: cmake --build build -j", file=sys.stderr)
        return 2
    if not sys.platform.startswith(("linux", "darwin")):
        print("этот скрипт использует pty и работает только на Linux/macOS", file=sys.stderr)
        return 2

    _config_root = tempfile.mkdtemp(prefix="botyarapix-tui-")
    print(f"бинарь: {_binary}")
    print(f"временный конфиг: {_config_root}")
    try:
        scenario_arrow_echo()
        scenario_navigation()
        scenario_groups()
        scenario_generator_flow()
        scenario_custom_size()
        scenario_typeahead()
        scenario_narrow()
        scenario_paste()
        scenario_fallbacks()
        scenario_signals()
    finally:
        if not args.keep:
            shutil.rmtree(_config_root, ignore_errors=True)

    print("\n" + "=" * 70)
    print(f"Проверок: {_checks}, провалов: {len(_failures)}")
    for name in _failures:
        print("  -", name)
    print("=" * 70)
    return 0 if not _failures else 1


if __name__ == "__main__":
    sys.exit(main())
