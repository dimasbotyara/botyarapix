"""Минимальный VT-эмулятор для проверки меню botyarapix.

Зачем свой, а не pyte: pyte при resize() переверстывает строки (reflow),
а реальные терминалы (xterm, Termux) — нет. На этом расхождении проверка
«поворот экрана» давала ложные срабатывания.

Реализовано ровно то, что использует программа: CR, LF, CUU/CUD/CUF/CUB,
EL (K), ED (J), CUP (H), SGR (m), показ/скрытие курсора (?25h/l) и resize
без reflow.
"""


class Cell:
    __slots__ = ("ch", "reverse", "bold", "dim")

    def __init__(self, ch=" "):
        self.ch = ch
        self.reverse = False
        self.bold = False
        self.dim = False

    def copy(self):
        cell = Cell(self.ch)
        cell.reverse = self.reverse
        cell.bold = self.bold
        cell.dim = self.dim
        return cell


class VT:
    def __init__(self, cols=80, rows=30):
        self.cols = cols
        self.rows = rows
        self.grid = [[Cell() for _ in range(cols)] for _ in range(rows)]
        self.margin = 0  # первая строка, на которую можно вернуться через CUU
        self.y = 0
        self.x = 0
        self.reverse = False
        self.bold = False
        self.dim = False
        self.cursor_visible = True
        self._pending = ""

    # ---------------------------------------------------------------- вывод
    def feed(self, data: bytes):
        text = self._pending + data.decode("utf-8", "replace")
        self._pending = ""
        i = 0
        while i < len(text):
            ch = text[i]
            if ch == "\x1b":
                match = self._parse_escape(text, i)
                if match is None:  # последовательность ещё не дописана
                    self._pending = text[i:]
                    return
                i = match
                continue
            if ch == "\r":
                self.x = 0
            elif ch == "\n":
                self._line_feed()
            elif ch == "\b":
                self.x = max(0, self.x - 1)
            elif ch >= " ":
                self._put(ch)
            i += 1

    def _put(self, ch):
        if self.x >= self.cols:  # автоперенос
            self.x = 0
            self._line_feed()
        cell = self.grid[self.y][self.x]
        cell.ch = ch
        cell.reverse = self.reverse
        cell.bold = self.bold
        cell.dim = self.dim
        self.x += 1

    def _line_feed(self):
        if self.y == self.rows - 1:
            self.grid.pop(0)
            self.grid.append([Cell() for _ in range(self.cols)])
            self.margin = max(0, self.margin - 1)
        else:
            self.y += 1
        if self.y >= self.rows:
            self.y = self.rows - 1

    def _parse_escape(self, text, i):
        start = i
        i += 1
        if i >= len(text):
            return None
        if text[i] == "[":
            i += 1
            params = ""
            while i < len(text) and not ("@" <= text[i] <= "~"):
                params += text[i]
                i += 1
            if i >= len(text):
                return None
            self._csi(params, text[i])
            return i + 1
        if text[i] in "()":  # charset selection
            return i + 2 if i + 1 < len(text) else None
        return i + 1  # одиночная ESC-последовательность
        # noqa: F841  (start оставлен для читаемости)

    def _csi(self, params, final):
        private = params.startswith("?")
        body = params[1:] if private else params
        numbers = [int(p) if p.isdigit() else 0 for p in body.split(";")] if body else []

        if final == "A":  # CUU — вверх
            count = numbers[0] if numbers and numbers[0] else 1
            self.y = max(self.margin, self.y - count)
        elif final == "B":  # CUD
            count = numbers[0] if numbers and numbers[0] else 1
            self.y = min(self.rows - 1, self.y + count)
        elif final == "C":
            count = numbers[0] if numbers and numbers[0] else 1
            self.x = min(self.cols - 1, self.x + count)
        elif final == "D":
            count = numbers[0] if numbers and numbers[0] else 1
            self.x = max(0, self.x - count)
        elif final == "G":
            self.x = max(0, (numbers[0] if numbers else 1) - 1)
        elif final == "d":
            self.y = max(0, (numbers[0] if numbers else 1) - 1)
        elif final == "K":  # EL
            mode = numbers[0] if numbers else 0
            if mode == 0:
                for x in range(self.x, self.cols):
                    self.grid[self.y][x] = Cell()
            elif mode == 2:
                self.grid[self.y] = [Cell() for _ in range(self.cols)]
        elif final == "J":  # ED
            mode = numbers[0] if numbers else 0
            if mode == 0:
                for x in range(self.x, self.cols):
                    self.grid[self.y][x] = Cell()
                for y in range(self.y + 1, self.rows):
                    self.grid[y] = [Cell() for _ in range(self.cols)]
            elif mode == 2:
                self.grid = [[Cell() for _ in range(self.cols)] for _ in range(self.rows)]
                self.y = 0
                self.x = 0
                self.margin = 0
        elif final == "H" or final == "f":  # CUP
            row = numbers[0] if numbers and numbers[0] else 1
            col = numbers[1] if len(numbers) > 1 and numbers[1] else 1
            self.y = min(self.rows - 1, max(0, row - 1))
            self.x = min(self.cols - 1, max(0, col - 1))
        elif final == "m":  # SGR
            codes = numbers or [0]
            for code in codes:
                if code == 0:
                    self.reverse = self.bold = self.dim = False
                elif code == 1:
                    self.bold = True
                elif code == 2:
                    self.dim = True
                elif code == 7:
                    self.reverse = True
                elif code == 22:
                    self.bold = self.dim = False
                elif code == 27:
                    self.reverse = False
        elif final in "hl" and private:  # DECSET / DECRST
            if 25 in numbers:
                self.cursor_visible = final == "h"

    # ---------------------------------------------------------------- запросы
    def lines(self):
        return ["".join(cell.ch for cell in row).rstrip() for row in self.grid]

    def text(self):
        return "\n".join(self.lines())

    def selected(self):
        """Строка, подсвеченная реверсом (курсор меню)."""
        for row in self.grid:
            if row and row[1].reverse or (row and row[0].reverse):
                return "".join(cell.ch for cell in row).rstrip()
        return ""

    def highlighted_lines(self):
        out = []
        for row in self.grid:
            if any(cell.reverse for cell in row):
                out.append("".join(cell.ch for cell in row).rstrip())
        return out

    def resize(self, cols, rows):
        """Как xterm: меняем размер, содержимое не переверстываем."""
        grid = [[Cell() for _ in range(cols)] for _ in range(rows)]
        for y in range(min(rows, self.rows)):
            for x in range(min(cols, self.cols)):
                grid[y][x] = self.grid[y][x].copy()
        self.grid = grid
        self.cols = cols
        self.rows = rows
        self.y = min(self.y, rows - 1)
        self.x = min(self.x, cols - 1)
