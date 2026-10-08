# Ctrl-X O: cycle focus
# Arrow/PageUp/PageDown: scroll window
# watch-tui foo: add watch
#unwatch-tui EXPRESSION List currently configured watches.
# clear-watches-tui

python
import gdb
import textwrap

watches = []
class ScrollableWindow:
    def __init__(self, win):
        self.win = win
        self.lines = []
        self.scroll = 0

    def close(self):
        pass

    def render(self):
        self.refresh()

    def visible_height(self):
        try:
            return max(1, self.win.height)
        except Exception:
            return 1

    def visible_width(self):
        try:
            return max(1, self.win.width)
        except Exception:
            return 1

    def display_lines(self):
        width = self.visible_width()
        rows = []
        for line in self.lines:
            if line == "":
                rows.append("")
                continue
            wrapped = textwrap.wrap(
                line,
                width=width,
                replace_whitespace=False,
                drop_whitespace=False,
                break_long_words=True,
                break_on_hyphens=False
            )

            if wrapped:
                rows.extend(wrapped)
            else:
                rows.append("")

        return rows

    def max_scroll(self):
        rows = self.display_lines()
        return max(0, len(rows) - self.visible_height())

    def clamp_scroll(self):
        self.scroll = max(0, min(self.scroll, self.max_scroll()))

    def draw(self):
        rows = self.display_lines()
        self.clamp_scroll()
        height = self.visible_height()
        visible = rows[self.scroll: self.scroll + height]
        self.win.write("\n".join(visible), True)

    def vscroll(self, num):
        self.scroll += num
        self.clamp_scroll()
        self.draw()

    def hscroll(self, num):
        pass

class LocalsWindow(ScrollableWindow):
    instance = None

    def __init__(self, win):
        super().__init__(win)
        self.win.title = "Locals"
        LocalsWindow.instance = self

    def close(self):
        LocalsWindow.instance = None

    def refresh(self):
        lines = []
        try:
            frame = gdb.selected_frame()
            block = frame.block()
            seen = set()

            while block is not None:
                for sym in block:
                    if not (sym.is_argument or sym.is_variable):
                        continue
                    name = sym.name

                    if not name:
                        continue
                    if name in seen:
                        continue
                    seen.add(name)
                    try:
                        value = sym.value(frame)
                        lines.append("{} = {}".format(name, value))
                    except (gdb.error, RuntimeError):
                        lines.append("{} = <unavailable>".format(name))
                if block.function is not None:
                    break
                block = block.superblock
        except (gdb.error, RuntimeError):
            lines = ["<no frame>"]
        if not lines:
            lines = ["<no locals>"]

        self.lines = lines
        self.draw()

class WatchesWindow(ScrollableWindow):
    instance = None

    def __init__(self, win):
        super().__init__(win)
        self.win.title = "Watches"
        WatchesWindow.instance = self

    def close(self):
        WatchesWindow.instance = None

    def refresh(self):
        lines = []
        for expr in watches:
            try:
                value = gdb.parse_and_eval(expr)
                lines.append("{} = {}".format(expr, value))
            except (gdb.error, RuntimeError):
                lines.append("{} = <error>".format(expr))

        if not lines:
            lines = ["<no watches>"]

        self.lines = lines
        self.draw()

def refresh_tui(event=None):

    if LocalsWindow.instance is not None:
        try:
            LocalsWindow.instance.refresh()
        except Exception:
            pass

    if WatchesWindow.instance is not None:
        try:
            WatchesWindow.instance.refresh()
        except Exception:
            pass

class WatchTuiCommand(gdb.Command):
    def __init__(self):
        super().__init__("watch-tui", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        expr = arg.strip()
        if not expr:
            print("usage: watch-tui EXPRESSION")
            return

        if expr in watches:
            print("Already watching: {}".format(expr))
            return

        watches.append(expr)
        refresh_tui()


class UnwatchTuiCommand(gdb.Command):
    def __init__(self):
        super().__init__("unwatch-tui", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        expr = arg.strip()
        if not expr:
            print("usage: unwatch-tui EXPRESSION")
            return

        try:
            watches.remove(expr)

        except ValueError:
            print("No such watch: {}".format(expr))
            return

        refresh_tui()


class ListWatchesTuiCommand(gdb.Command):
    def __init__(self):
        super().__init__("watches-tui", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if not watches:
            print("No TUI watches.")
            return

        for i, expr in enumerate(watches, start=1):
            print("{}: {}".format( i, expr))

class ClearWatchesTuiCommand(gdb.Command):
    def __init__(self):
        super().__init__("clear-watches-tui", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        watches.clear()
        refresh_tui()

class LocalsDownCommand(gdb.Command):
    def __init__(self):
        super().__init__("locals-down", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if LocalsWindow.instance is not None:
            LocalsWindow.instance.vscroll(1)

class LocalsUpCommand(gdb.Command):
    def __init__(self):
        super().__init__("locals-up", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if LocalsWindow.instance is not None:
            LocalsWindow.instance.vscroll(-1)


class WatchesDownCommand(gdb.Command):
    def __init__(self):
        super().__init__("watches-down", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):
        if WatchesWindow.instance is not None:
            WatchesWindow.instance.vscroll(1)

class WatchesUpCommand(gdb.Command):
    def __init__(self):
        super().__init__("watches-up", gdb.COMMAND_DATA)

    def invoke(self, arg, from_tty):

        if WatchesWindow.instance is not None:
            WatchesWindow.instance.vscroll(-1)


gdb.register_window_type("locals", LocalsWindow)
gdb.register_window_type("watches", WatchesWindow)


WatchTuiCommand()
UnwatchTuiCommand()
ListWatchesTuiCommand()
ClearWatchesTuiCommand()

LocalsDownCommand()
LocalsUpCommand()
WatchesDownCommand()
WatchesUpCommand()

gdb.events.stop.connect(refresh_tui)

end


# For more watch space: {locals 1 watches 2}
tui new-layout src-vars \
    {-horizontal src 2 {locals 1 watches 1} 1} 4 \
    status 0 \
    cmd 1

layout src-vars

