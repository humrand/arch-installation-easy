import curses
import subprocess
import sys
import os
import re
import shutil
import shlex
from datetime import datetime
import termios
import tty
import threading
import time

VERSION = "V1.1.0"
LOG_FILE = "/mnt/install_log.txt"
ESC = 27
BACK = "back"

SPINNER = ["⠋","⠙","⠹","⠸","⠼","⠴","⠦","⠧","⠇","⠏"]

ARCH_LOGO = [
    "                 -`                  ",
    "                .o+`                 ",
    "               `ooo/                 ",
    "              `+oooo:                ",
    "             `+oooooo:               ",
    "             -+oooooo+:              ",
    "           `/:-:++oooo+:             ",
    "          `/++++/+++++++:            ",
    "         `/++++++++++++++:           ",
    "        `/+++ooooooooooooo/`         ",
    "       ./ooosssso++osssssso+`        ",
    "      .oossssso-````/ossssss+`       ",
    "     -osssssso.      :ssssssso.      ",
    "    :osssssss/        osssso+++.     ",
    "   /ossssssss/        +ssssooo/-     ",
    " `/ossssso+/:-        -:/+osssso+-   ",
    "`+sso+:-`                 `.-/+oso:  ",
    "`++:.                           `-/+/",
    ".`                                 `/",
]
_LOGO_W = max(len(l) for l in ARCH_LOGO)

_PAT_INSTALL = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)

_C = {}

def init_colors():
    curses.start_color()
    try:
        curses.use_default_colors()
    except Exception:
        pass
    curses.init_pair(1, curses.COLOR_WHITE,  curses.COLOR_BLUE)
    curses.init_pair(2, curses.COLOR_GREEN,  curses.COLOR_BLUE)
    curses.init_pair(3, curses.COLOR_RED,    curses.COLOR_BLUE)
    curses.init_pair(4, curses.COLOR_YELLOW, curses.COLOR_BLUE)
    curses.init_pair(5, curses.COLOR_BLACK,  curses.COLOR_CYAN)
    curses.init_pair(6, curses.COLOR_CYAN,   curses.COLOR_BLUE)
    curses.init_pair(7, curses.COLOR_WHITE,  curses.COLOR_RED)
    _C["n"] = curses.color_pair(1)
    _C["s"] = curses.color_pair(2)
    _C["e"] = curses.color_pair(3)
    _C["w"] = curses.color_pair(4)
    _C["l"] = curses.color_pair(5)
    _C["a"] = curses.color_pair(6)
    _C["d"] = curses.color_pair(7)

def nowtag():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except Exception:
        pass

append_buffer = []
append_lock   = threading.Lock()

def append_buffer_add(s):
    line = f"[{nowtag()}] {s}"
    write_log(line)
    with append_lock:
        append_buffer.append(line)
        if len(append_buffer) > 1000:
            del append_buffer[:500]

def run_stream(cmd, on_line=None, cwd=None, ignore_error=False):
    append_buffer_add(f"$ {cmd}")
    p = subprocess.Popen(
        cmd, shell=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, cwd=cwd, executable="/bin/bash"
    )
    buf = b""
    while True:
        chunk = p.stdout.read(512)
        if not chunk:
            if p.poll() is not None:
                break
            time.sleep(0.02)
            continue
        buf += chunk
        parts = re.split(b"[\r\n]+", buf)
        buf = parts[-1]
        for part in parts[:-1]:
            line = part.decode("utf-8", errors="replace").strip()
            if line:
                append_buffer_add(line)
                if on_line:
                    on_line(line)
    if buf.strip():
        line = buf.decode("utf-8", errors="replace").strip()
        append_buffer_add(line)
        if on_line:
            on_line(line)
    rc = p.wait()
    if rc != 0 and not ignore_error:
        append_buffer_add(f"ERROR (rc={rc}): {cmd}")
    return rc

def run_simple(cmd, ignore_error=False):
    append_buffer_add(f"$ {cmd}")
    r = subprocess.call(cmd, shell=True, executable="/bin/bash")
    if r != 0 and not ignore_error:
        append_buffer_add(f"ERROR (rc={r}): {cmd}")
    return r

def ensure_network():
    def ping():
        return subprocess.call(
            "ping -c1 -W2 8.8.8.8 >/dev/null 2>&1",
            shell=True, executable="/bin/bash"
        ) == 0
    if ping():
        return True
    for tool in ("dhcpcd", "dhclient"):
        if shutil.which(tool):
            run_simple(f"{tool} >/dev/null 2>&1", ignore_error=True)
            time.sleep(3)
            if ping():
                return True
    return False

def list_disks():
    try:
        out = subprocess.check_output(
            "lsblk -b -d -o NAME,SIZE,MODEL | tail -n +2",
            shell=True, text=True
        )
    except Exception:
        return []
    disks = []
    for line in out.splitlines():
        parts = line.split(None, 2)
        if len(parts) < 2:
            continue
        name = parts[0]
        try:
            size_gb = int(parts[1]) // (1024 ** 3)
        except Exception:
            size_gb = 0
        model = parts[2].strip() if len(parts) > 2 else ""
        disks.append((name, size_gb, model))
    return disks

def partition_paths_for(disk_path):
    if "nvme" in disk_path or "mmcblk" in disk_path:
        return f"{disk_path}p1", f"{disk_path}p2", f"{disk_path}p3"
    return f"{disk_path}1", f"{disk_path}2", f"{disk_path}3"

def validate_name(n):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", n or ""))

def validate_swap(s):
    return bool(re.match(r"^\d+$", s or "")) and 1 <= int(s) <= 128

state = {
    "lang":      "en",
    "hostname":  "",
    "username":  "",
    "root_pass": "",
    "user_pass": "",
    "swap":      "8",
    "disk":      None,
    "desktop":   "None",
    "gpu":       "None",
    "keymap":    "us",
    "timezone":  "UTC",
}

screens = [
    "welcome", "language", "identity", "passwords", "disk",
    "keymap", "timezone", "desktop", "gpu", "review", "install"
]

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def safe_addstr(win, y, x, s, attr=0):
    try:
        mh, mw = win.getmaxyx()
        if y < 0 or y >= mh or x < 0 or x >= mw:
            return
        avail = mw - x
        if avail <= 0:
            return
        win.addstr(y, x, s[:avail], attr)
    except curses.error:
        pass

def draw_box(win, y, x, h, w, attr=0):
    attr = attr or _C.get("n", 0)
    safe_addstr(win, y, x, "╔" + "═" * (w - 2) + "╗", attr)
    for i in range(1, h - 1):
        safe_addstr(win, y + i, x,         "║", attr)
        safe_addstr(win, y + i, x + w - 1, "║", attr)
    try:
        win.addstr(y + h - 1, x, "╚" + "═" * (w - 2) + "╝", attr)
    except curses.error:
        try:
            win.insstr(y + h - 1, x, "╚" + "═" * (w - 2) + "╝", attr)
        except curses.error:
            pass

def draw_hline(win, y, w, attr=0):
    attr = attr or _C.get("n", 0)
    safe_addstr(win, y, 0, "╠" + "═" * (w - 2) + "╣", attr)

def draw_hline_split(win, y, w, mid, cross_top=True, attr=0):
    attr  = attr or _C.get("n", 0)
    cross = "╦" if cross_top else "╩"
    left  = mid - 1
    right = w - mid - 2
    safe_addstr(win, y, 0,
        "╠" + "═" * left + cross + "═" * right + "╣", attr)

def draw_header(stdscr, title, step=None, total=None):
    stdscr.erase()
    stdscr.bkgd(" ", _C.get("n", 0))
    h, w = curses.LINES, curses.COLS
    draw_box(stdscr, 0, 0, h, w)
    stdscr.refresh()
    time.sleep(0.015)
    draw_hline(stdscr, 2, w)
    draw_hline(stdscr, h - 3, w)
    safe_addstr(stdscr, 1, 2, " ❱ Arch Linux Installer ",
                _C.get("a", 0) | curses.A_BOLD)
    if step is not None and total is not None:
        tag = f" Step {step}/{total} "
        safe_addstr(stdscr, 1, w - len(tag) - 1, tag,
                    _C.get("w", 0) | curses.A_BOLD)
    safe_addstr(stdscr, 3, 3, title, _C.get("n", 0) | curses.A_BOLD)
    stdscr.refresh()
    time.sleep(0.015)

def draw_footer(stdscr, msg):
    h, w = curses.LINES, curses.COLS
    safe_addstr(stdscr, h - 2, 3, msg, _C.get("a", 0))

def input_curses(stdscr, y, x, prompt, initial="", secret=False):
    curses.curs_set(1)
    curses.noecho()
    stdscr.keypad(True)
    s       = initial
    fx      = x + len(prompt) + 1
    field_w = max(2, curses.COLS - fx - 3)
    while True:
        display = "*" * len(s) if secret else s
        safe_addstr(stdscr, y, x,  prompt,           _C.get("n", 0) | curses.A_BOLD)
        safe_addstr(stdscr, y, fx, " " * field_w,    _C.get("n", 0))
        safe_addstr(stdscr, y, fx, display[:field_w], _C.get("w", 0))
        try:
            stdscr.move(y, min(fx + len(display), curses.COLS - 2))
        except curses.error:
            pass
        stdscr.refresh()
        try:
            ch = stdscr.get_wch()
        except Exception:
            continue
        if ch in (10, 13, "\n", "\r") or ch == curses.KEY_ENTER:
            break
        if (isinstance(ch, int) and ch == ESC) or ch == "\x1b":
            curses.curs_set(0)
            return None
        if ch in ("\x7f", "\b", 127) or ch == curses.KEY_BACKSPACE:
            s = s[:-1] if s else s
        elif isinstance(ch, str) and ch.isprintable():
            s += ch
    curses.curs_set(0)
    return s

def curses_picker(stdscr, options, title):
    if not options:
        return None
    per_page   = max(6, curses.LINES - 12)
    page_start = 0
    sel        = 0
    while True:
        stdscr.erase()
        stdscr.bkgd(" ", _C.get("n", 0))
        h, w = curses.LINES, curses.COLS
        draw_box(stdscr, 0, 0, h, w)
        draw_hline(stdscr, 2, w)
        draw_hline(stdscr, h - 3, w)
        safe_addstr(stdscr, 1, 2, " ❱ Arch Linux Installer ",
                    _C.get("a", 0) | curses.A_BOLD)
        safe_addstr(stdscr, 3, 3, title, _C.get("n", 0) | curses.A_BOLD)
        page  = options[page_start:page_start + per_page]
        opt_w = max(20, min(w - 10, 60))
        for i, opt in enumerate(page):
            attr = _C.get("l", 0) if i == sel else _C.get("n", 0)
            safe_addstr(stdscr, 5 + i, 4, f"  {opt:<{opt_w}}  ", attr)
        total_pages = max(1, (len(options) - 1) // per_page + 1)
        cur_page    = page_start // per_page + 1
        safe_addstr(stdscr, h - 4, 4, f"Page {cur_page}/{total_pages}",
                    _C.get("w", 0))
        safe_addstr(stdscr, h - 2, 3,
            L("↑↓ move · PgUp/PgDn page · Enter=select · Esc/q=back",
              "↑↓ mover · PgUp/PgDn página · Enter=seleccionar · Esc/q=volver"),
            _C.get("a", 0))
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = max(0, sel - 1)
        elif k == curses.KEY_DOWN:
            sel = min(len(page) - 1, sel + 1)
        elif k == curses.KEY_NPAGE:
            if page_start + per_page < len(options):
                page_start += per_page
                sel = 0
        elif k == curses.KEY_PPAGE:
            if page_start - per_page >= 0:
                page_start -= per_page
                sel = 0
        elif k in (10, 13):
            return page_start + sel
        elif k in (ESC, ord("q")):
            return None

def build_zone_tree(zones):
    tree = {}
    for z in zones:
        node = tree
        for p in z.split("/"):
            node = node.setdefault(p, {})
    return tree

def traverse_tree_picker(stdscr, tree):
    node    = tree
    path    = []
    history = []
    while True:
        keys    = sorted(node.keys())
        display = [k + ("/" if node[k] else "") for k in keys]
        loc     = "/".join(path) if path else L("region", "región")
        idx     = curses_picker(stdscr, display, f"Timezone › {loc}")
        if idx is None:
            if history:
                node, path = history.pop()
                continue
            return None
        chosen = keys[idx]
        if node[chosen]:
            history.append((node, path[:]))
            path.append(chosen)
            node = node[chosen]
        else:
            path.append(chosen)
            return "/".join(path)

def screen_welcome_c(stdscr):
    curses.curs_set(0)
    stdscr.erase()
    stdscr.bkgd(" ", _C.get("n", 0))
    h, w = curses.LINES, curses.COLS
    draw_box(stdscr, 0, 0, h, w)
    draw_hline(stdscr, 2, w)
    draw_hline(stdscr, h - 3, w)
    safe_addstr(stdscr, 1, 2, " ❱ Arch Linux Installer ",
                _C.get("a", 0) | curses.A_BOLD)

    logo_x = max(2, (w - _LOGO_W) // 2)
    logo_y = 4
    for i, line in enumerate(ARCH_LOGO):
        safe_addstr(stdscr, logo_y + i, logo_x, line,
                    _C.get("a", 0) | curses.A_BOLD)

    content_y = logo_y + len(ARCH_LOGO) + 1
    title_str = "Arch Linux Installer"
    safe_addstr(stdscr, content_y,
                max(2, (w - len(title_str)) // 2), title_str,
                _C.get("s", 0) | curses.A_BOLD)
    ver_line = f"{VERSION}  ·  {datetime.now().strftime('%Y-%m-%d')}"
    safe_addstr(stdscr, content_y + 1,
                max(2, (w - len(ver_line)) // 2), ver_line,
                _C.get("w", 0))
    warn = "⚠  This installer will ERASE the selected disk"
    safe_addstr(stdscr, content_y + 3,
                max(2, (w - len(warn)) // 2), warn,
                _C.get("e", 0) | curses.A_BOLD)
    prompt = L("Press any key to start…", "Presiona cualquier tecla para empezar…")
    safe_addstr(stdscr, h - 2,
                max(2, (w - len(prompt)) // 2), prompt,
                _C.get("a", 0))
    stdscr.refresh()

    frame = 0
    stdscr.timeout(200)
    while True:
        k = stdscr.getch()
        if k != -1:
            break
        safe_addstr(stdscr, h - 2, 2,
                    SPINNER[frame % len(SPINNER)], _C.get("s", 0))
        stdscr.refresh()
        frame += 1
    stdscr.timeout(-1)

def screen_language_c(stdscr):
    curses.curs_set(0)
    opts = ["English", "Español"]
    sel  = 0 if state["lang"] == "en" else 1
    while True:
        draw_header(stdscr, "Language / Idioma", 1, 9)
        for i, opt in enumerate(opts):
            attr = _C.get("l", 0) if i == sel else _C.get("n", 0)
            safe_addstr(stdscr, 6 + i, 6, f"  {opt:<24}  ", attr)
        draw_footer(stdscr, "↑↓ move · Enter=select · q=quit")
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10, 13):
            state["lang"] = "en" if sel == 0 else "es"
            return
        elif k == ord("q"):
            sys.exit(0)

def screen_identity_c(stdscr):
    error = ""
    while True:
        draw_header(stdscr, L("System Identity", "Identidad del sistema"), 2, 9)
        if error:
            safe_addstr(stdscr, 5, 4, error, _C.get("e", 0) | curses.A_BOLD)
        safe_addstr(stdscr, 6, 4,
            L("Esc=back  (letters, digits, - _ · max 32 chars)",
              "Esc=volver  (letras, dígitos, - _ · máx 32 chars)"),
            _C.get("w", 0))
        stdscr.refresh()
        hostname = input_curses(stdscr, 8, 4,
                                L("Hostname   :", "Nombre equipo:"),
                                state.get("hostname", ""))
        if hostname is None:
            return BACK
        username = input_curses(stdscr, 10, 4,
                                L("Username   :", "Usuario      :"),
                                state.get("username", ""))
        if username is None:
            return BACK
        if not validate_name(hostname):
            error = L("✗  Invalid hostname (a-z 0-9 - _ · 1-32 chars).",
                      "✗  Hostname inválido (a-z 0-9 - _ · 1-32 chars).")
            continue
        if not validate_name(username):
            error = L("✗  Invalid username (a-z 0-9 - _ · 1-32 chars).",
                      "✗  Usuario inválido (a-z 0-9 - _ · 1-32 chars).")
            continue
        state["hostname"] = hostname
        state["username"] = username
        return

def screen_passwords_c(stdscr):
    error = ""
    while True:
        draw_header(stdscr, L("Passwords", "Contraseñas"), 3, 9)
        if error:
            safe_addstr(stdscr, 5, 4, error, _C.get("e", 0) | curses.A_BOLD)
        safe_addstr(stdscr, 6, 4, L("Esc=back", "Esc=volver"), _C.get("w", 0))
        stdscr.refresh()
        prompts = [
            L("Root password       :", "Contraseña root     :"),
            L("Confirm root        :", "Confirmar root      :"),
            L("User password       :", "Contraseña usuario  :"),
            L("Confirm user        :", "Confirmar usuario   :"),
        ]
        vals      = []
        cancelled = False
        for i, prompt in enumerate(prompts):
            v = input_curses(stdscr, 8 + i * 2, 4, prompt, secret=True)
            if v is None:
                cancelled = True
                break
            vals.append(v)
        if cancelled:
            return BACK
        rp1, rp2, up1, up2 = vals
        if not rp1:
            error = L("✗  Root password is empty.",
                      "✗  Contraseña root vacía.")
            continue
        if rp1 != rp2:
            error = L("✗  Root passwords do not match.",
                      "✗  Contraseñas root no coinciden.")
            continue
        if not up1:
            error = L("✗  User password is empty.",
                      "✗  Contraseña usuario vacía.")
            continue
        if up1 != up2:
            error = L("✗  User passwords do not match.",
                      "✗  Contraseñas usuario no coinciden.")
            continue
        state["root_pass"] = rp1
        state["user_pass"] = up1
        return

def screen_disk_c(stdscr):
    curses.curs_set(0)
    disks = list_disks()
    if not disks:
        draw_header(stdscr, L("Disk Selection", "Selección de disco"))
        safe_addstr(stdscr, 6, 4,
            L("✗  No disks found. Aborting.",
              "✗  No se encontraron discos. Abortando."),
            _C.get("e", 0) | curses.A_BOLD)
        stdscr.refresh()
        stdscr.getch()
        sys.exit(1)

    sel = 0
    for i, (d, _, _) in enumerate(disks):
        if d == state.get("disk"):
            sel = i
            break

    while True:
        draw_header(stdscr, L("Disk & Swap", "Disco y Swap"), 4, 9)
        h, w = curses.LINES, curses.COLS

        safe_addstr(stdscr, 5, 4,
            L("⚠  ALL DATA ON THE SELECTED DISK WILL BE ERASED",
              "⚠  SE BORRARÁN TODOS LOS DATOS DEL DISCO SELECCIONADO"),
            _C.get("e", 0) | curses.A_BOLD)

        n_col  = 14
        s_col  = 10
        m_col  = max(10, w - 8 - n_col - s_col - 4)

        tbl_top = "╔" + "═"*n_col + "╦" + "═"*s_col + "╦" + "═"*m_col + "╗"
        tbl_mid = "╠" + "═"*n_col + "╬" + "═"*s_col + "╬" + "═"*m_col + "╣"
        tbl_bot = "╚" + "═"*n_col + "╩" + "═"*s_col + "╩" + "═"*m_col + "╝"

        safe_addstr(stdscr, 7,  4, tbl_top, _C.get("n", 0))

        hdr = (f"║ {'NAME':<{n_col-1}}"
               f"║ {'SIZE':>{s_col-1}}"
               f"║ {'MODEL':<{m_col-1}}║")
        safe_addstr(stdscr, 8,  4, hdr,     _C.get("a", 0) | curses.A_BOLD)
        safe_addstr(stdscr, 9,  4, tbl_mid, _C.get("n", 0))

        for i, (name, size_gb, model) in enumerate(disks):
            name_s  = f"/dev/{name}"
            size_s  = f"{size_gb} GB"
            model_s = (model or "")[:m_col - 1]
            row = (f"║ {name_s:<{n_col-1}}"
                   f"║ {size_s:>{s_col-1}}"
                   f"║ {model_s:<{m_col-1}}║")
            attr = _C.get("l", 0) if i == sel else _C.get("n", 0)
            safe_addstr(stdscr, 10 + i, 4, row, attr)

        bot_y = 10 + len(disks)
        safe_addstr(stdscr, bot_y, 4, tbl_bot, _C.get("n", 0))

        swap_attr = _C.get("w", 0) if state["swap"] == "8" else _C.get("s", 0)
        swap_line = L(f"Swap: {state['swap']} GB  ·  s=change",
                      f"Swap: {state['swap']} GB  ·  s=cambiar")
        safe_addstr(stdscr, bot_y + 2, 4, swap_line, swap_attr)

        draw_footer(stdscr,
            L("↑↓ disk · s=swap · Enter=confirm · Esc=back · q=quit",
              "↑↓ disco · s=swap · Enter=confirmar · Esc=volver · q=salir"))
        stdscr.refresh()

        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(disks)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(disks)
        elif k in (10, 13):
            state["disk"] = disks[sel][0]
            return
        elif k == ord("s"):
            safe_addstr(stdscr, bot_y + 2, 4, " " * 40, _C.get("n", 0))
            v = input_curses(stdscr, bot_y + 2, 4,
                             L("Swap GB (1-128):", "Swap GB (1-128):"))
            if v and validate_swap(v.strip()):
                state["swap"] = v.strip()
        elif k == ESC:
            return BACK
        elif k == ord("q"):
            sys.exit(0)

def screen_keymap_c(stdscr):
    try:
        out  = subprocess.check_output(
            "localectl list-keymaps 2>/dev/null || true",
            shell=True, text=True
        )
        maps = [l for l in out.splitlines() if l]
    except Exception:
        maps = []
    wanted  = ["us", "es", "fr", "de", "ru", "ara"]
    options = [m for m in wanted if m in maps] if maps else wanted
    idx = curses_picker(stdscr, options,
                        L("Choose keymap", "Seleccionar teclado"))
    if idx is None:
        return BACK
    state["keymap"] = options[idx]
    run_simple(f"loadkeys {shlex.quote(state['keymap'])}", ignore_error=True)

def screen_timezone_c(stdscr):
    try:
        out   = subprocess.check_output(
            "timedatectl list-timezones 2>/dev/null || true",
            shell=True, text=True
        )
        zones = [l for l in out.splitlines() if l]
    except Exception:
        zones = []
    if not zones:
        zones = ["UTC", "Europe/Madrid", "America/New_York",
                 "America/Los_Angeles", "Asia/Tokyo"]
    tree = build_zone_tree(zones)
    tz   = traverse_tree_picker(stdscr, tree)
    if tz is None:
        return BACK
    state["timezone"] = tz

def _choice_screen(stdscr, title, opts, key, step):
    curses.curs_set(0)
    try:
        sel = opts.index(state[key])
    except (ValueError, KeyError):
        sel = len(opts) - 1
    while True:
        draw_header(stdscr, title, step, 9)
        for i, opt in enumerate(opts):
            attr = _C.get("l", 0) if i == sel else _C.get("n", 0)
            safe_addstr(stdscr, 6 + i, 6, f"  {opt:<26}  ", attr)
        draw_footer(stdscr,
            L("↑↓ move · Enter=select · Esc=back · q=quit",
              "↑↓ mover · Enter=seleccionar · Esc=volver · q=salir"))
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10, 13):
            state[key] = opts[sel]
            return
        elif k == ESC:
            return BACK
        elif k == ord("q"):
            sys.exit(0)

def screen_desktop_c(stdscr):
    return _choice_screen(
        stdscr,
        L("Desktop Environment", "Entorno de escritorio"),
        ["KDE Plasma", "Cinnamon", "None"],
        "desktop", 7
    )

def screen_gpu_c(stdscr):
    if state["gpu"] not in ("NVIDIA", "AMD/Intel", "None"):
        state["gpu"] = "None"
    return _choice_screen(
        stdscr,
        L("GPU Drivers", "Drivers GPU"),
        ["NVIDIA", "AMD/Intel", "None"],
        "gpu", 8
    )

_DEFAULTS = {
    "swap":     "8",
    "desktop":  "None",
    "gpu":      "None",
    "keymap":   "us",
    "timezone": "UTC",
}

def screen_review_c(stdscr):
    curses.curs_set(0)
    LC = 20
    VC = 24
    while True:
        draw_header(stdscr, L("Review & Confirm", "Revisar y confirmar"), 9, 9)
        h, w = curses.LINES, curses.COLS

        disk_label = (f"/dev/{state['disk']}" if state["disk"]
                      else L("NOT SET", "SIN ASIGNAR"))
        rows = [
            (L("Language",  "Idioma"),  state["lang"],            "lang"),
            ("Hostname",               state["hostname"] or "—",  "hostname"),
            (L("Username", "Usuario"), state["username"] or "—",  "username"),
            ("Disk",                   disk_label,                "disk"),
            ("Swap",                   f"{state['swap']} GB",     "swap"),
            ("Keymap",                 state["keymap"],           "keymap"),
            ("Timezone",               state["timezone"],         "timezone"),
            ("Desktop",                state["desktop"],          "desktop"),
            ("GPU",                    state["gpu"],              "gpu"),
        ]

        y = 5
        safe_addstr(stdscr, y, 4,
            "╔" + "═"*LC + "╦" + "═"*VC + "╗", _C.get("n", 0))
        y += 1

        safe_addstr(stdscr, y, 4,
            "║" + f" {'Setting':<{LC-1}}" + "║", _C.get("a", 0) | curses.A_BOLD)
        safe_addstr(stdscr, y, 4 + LC + 1,
            "║" + f" {'Value':<{VC-1}}" + "║",   _C.get("a", 0) | curses.A_BOLD)
        y += 1

        safe_addstr(stdscr, y, 4,
            "╠" + "═"*LC + "╬" + "═"*VC + "╣", _C.get("n", 0))
        y += 1

        for idx_r, (label, val, key) in enumerate(rows):
            raw        = state.get(key)
            is_default = _DEFAULTS.get(key) == raw
            if not raw:
                val_attr = _C.get("e", 0)
            elif is_default:
                val_attr = _C.get("w", 0)
            else:
                val_attr = _C.get("s", 0)

            safe_addstr(stdscr, y, 4,
                "║" + f" {label:<{LC-1}}" + "║", _C.get("n", 0))
            safe_addstr(stdscr, y, 4 + LC + 1,
                "║" + f" {val[:VC-1]:<{VC-1}}" + "║", val_attr)
            y += 1

            if idx_r < len(rows) - 1:
                sep = "╠" + "═"*LC + "╬" + "═"*VC + "╣"
            else:
                sep = "╚" + "═"*LC + "╩" + "═"*VC + "╝"
            safe_addstr(stdscr, y, 4, sep, _C.get("n", 0))
            y += 1

        hint = L("Yellow=default  Green=custom  Red=missing/unset",
                 "Amarillo=default  Verde=personaliz.  Rojo=falta/vacío")
        safe_addstr(stdscr, y, 4, hint, _C.get("w", 0))
        y += 1

        missing = []
        if not state["hostname"]:
            missing.append("hostname")
        if not state["username"]:
            missing.append("username")
        if not state["disk"]:
            missing.append("disk")
        if not state["root_pass"]:
            missing.append(L("root password", "contraseña root"))

        if missing:
            safe_addstr(stdscr, y + 1, 4,
                L(f"✗  Missing: {', '.join(missing)}",
                  f"✗  Faltan: {', '.join(missing)}"),
                _C.get("e", 0) | curses.A_BOLD)
        else:
            safe_addstr(stdscr, y + 1, 4,
                L("✔  All good — ready to install!",
                  "✔  Todo listo — ¡listo para instalar!"),
                _C.get("s", 0) | curses.A_BOLD)

        draw_footer(stdscr,
            L("Enter=start install · Esc=back · q=quit",
              "Enter=iniciar · Esc=volver · q=salir"))
        stdscr.refresh()

        k = stdscr.getch()
        if k in (10, 13):
            if missing:
                continue
            return
        elif k == ESC:
            return BACK
        elif k == ord("q"):
            sys.exit(0)


class InstallerUI:
    def __init__(self, stdscr):
        self.stdscr     = stdscr
        self.logs       = []
        self.progress   = 0.0
        self.stage      = ""
        self.failed     = False
        self.done       = False
        self.lock       = threading.Lock()
        self.completed  = []
        self.spin_idx   = 0
        self.start_time = time.time()
        if stdscr:
            try:
                curses.curs_set(0)
            except Exception:
                pass

    def add_log(self, line):
        with self.lock:
            self.logs.append(line)
            if len(self.logs) > 500:
                self.logs = self.logs[-400:]
        append_buffer_add(line)

    def set_stage(self, msg):
        with self.lock:
            prev = self.stage
            if prev and prev not in self.completed:
                self.completed.append(prev)
            self.stage = msg
        self.add_log(f">>> {msg}")

    def set_progress(self, pct):
        with self.lock:
            clamped       = max(0.0, min(100.0, float(pct)))
            self.progress = max(self.progress, clamped)

    def redraw(self):
        if not self.stdscr or self.done:
            return
        s = self.stdscr
        try:
            s.erase()
            s.bkgd(" ", _C.get("n", 0))
            h, w = curses.LINES, curses.COLS

            if w < 20 or h < 10:
                return

            mid    = w // 2
            left_w = mid - 2
            log_x  = mid + 2
            log_w  = max(1, w - mid - 4)
            log_rows = max(1, (h - 3) - 5)

            with self.lock:
                pct_f     = self.progress
                comp      = list(self.completed)
                cur_stage = self.stage
                failed    = self.failed
                visible   = list(self.logs[-log_rows:])

            draw_box(s, 0, 0, h, w)
            draw_hline_split(s, 2,     w, mid, cross_top=True)
            draw_hline_split(s, h - 3, w, mid, cross_top=False)
            for row in range(3, h - 3):
                safe_addstr(s, row, mid, "║", _C.get("n", 0))

            safe_addstr(s, 1, 2, " ❱ Arch Linux Installer ",
                        _C.get("a", 0) | curses.A_BOLD)
            elapsed     = int(time.time() - self.start_time)
            elapsed_str = f" {elapsed//60:02d}:{elapsed%60:02d} "
            safe_addstr(s, 1, w - len(elapsed_str) - 1, elapsed_str,
                        _C.get("w", 0))

            safe_addstr(s, 3, 2,
                L("Installing Arch Linux…", "Instalando Arch Linux…"),
                _C.get("n", 0) | curses.A_BOLD)

            pct    = int(pct_f)
            bar_w  = max(4, left_w - 8)
            filled = min(bar_w, max(0, int((pct_f / 100.0) * bar_w)))
            bar    = "█" * filled + "░" * (bar_w - filled)

            prog_attr = _C.get("e", 0) if failed else _C.get("s", 0)
            safe_addstr(s, 5, 2, f"{pct:>3}%", _C.get("n", 0) | curses.A_BOLD)
            safe_addstr(s, 5, 7, bar,           prog_attr)

            spin       = SPINNER[self.spin_idx % len(SPINNER)]
            stage_attr = _C.get("e", 0) if failed else _C.get("a", 0)
            safe_addstr(s, 7, 2,
                (spin + " " + cur_stage)[:left_w - 2],
                stage_attr | curses.A_BOLD)

            safe_addstr(s, 9, 2, L("Steps:", "Pasos:"),
                        _C.get("n", 0) | curses.A_BOLD)

            y          = 10
            max_step_y = h - 4
            for st in comp:
                if y >= max_step_y:
                    break
                safe_addstr(s, y, 2, ("✔ " + st)[:left_w - 2],
                            _C.get("s", 0))
                y += 1

            if cur_stage and y < max_step_y and not failed:
                safe_addstr(s, y, 2,
                    (SPINNER[self.spin_idx % len(SPINNER)] + " " + cur_stage)[:left_w - 2],
                    _C.get("w", 0))

            safe_addstr(s, 3, log_x,
                L("Live log:", "Log en vivo:"),
                _C.get("a", 0) | curses.A_BOLD)

            for i, line in enumerate(visible):
                row = 5 + i
                if row >= h - 3:
                    break
                ll   = line.lower()
                attr = _C.get("n", 0)
                if "error" in ll or "fail" in ll or "✗" in line or "fatal" in ll:
                    attr = _C.get("e", 0)
                elif "warning" in ll or "warn" in ll:
                    attr = _C.get("w", 0)
                elif "✔" in line or "complete" in ll or "success" in ll:
                    attr = _C.get("s", 0)
                safe_addstr(s, row, log_x, line[:log_w], attr)

            s.refresh()
        except curses.error:
            pass

    def _gradual_progress(self, target, duration=0.8):
        with self.lock:
            base = self.progress
        steps = max(4, int(duration / 0.05))
        for i in range(1, steps + 1):
            self.set_progress(base + (target - base) * (i / steps))
            time.sleep(duration / steps)
        self.set_progress(target)

    def _run_pacman_progress(self, cmd, start_pct, end_pct, ignore_error=False):
        download_done = [False]
        half_pct      = start_pct + (end_pct - start_pct) * 0.5

        def on_line(line):
            self.add_log(line)
            m = _PAT_INSTALL.search(line)
            if m:
                download_done[0] = True
                cur, total = int(m.group(1)), int(m.group(2))
                if total > 0:
                    new_pct = half_pct + (cur / total) * (end_pct - half_pct)
                    self.set_progress(min(new_pct, end_pct - 0.5))
                return
            if not download_done[0] and _PAT_DOWNLOAD.search(line):
                cap = start_pct + (end_pct - start_pct) * 0.45
                with self.lock:
                    cur_p = self.progress
                if cur_p < cap:
                    self.set_progress(cur_p + 0.3)

        rc = run_stream(cmd, on_line=on_line, ignore_error=ignore_error)
        self.set_progress(end_pct)
        return rc

    def run_steps(self):
        disk_path   = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)

        def chroot(cmd, ignore_error=True):
            return run_stream(
                f"arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}",
                on_line=self.add_log, ignore_error=ignore_error
            )

        def chroot_passwd(user, pwd):
            return run_stream(
                f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} "
                f"| arch-chroot /mnt chpasswd",
                on_line=self.add_log, ignore_error=True
            )

        try:
            self.set_stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.add_log(L("✗  No network detected. Connect and retry.",
                               "✗  Sin red. Conéctese e intente de nuevo."))
                self.failed = True
                self.set_progress(100.0)
                return

            run_stream("pacman -Sy --noconfirm archlinux-keyring",
                       on_line=self.add_log, ignore_error=True)

            self.set_stage(L("Wiping disk…", "Borrando disco…"))
            self._gradual_progress(3)
            run_stream(f"sgdisk -Z {disk_path}", on_line=self.add_log)
            self.set_progress(5)

            self.set_stage(L("Creating partitions…", "Creando particiones…"))
            run_stream(f"sgdisk -n1:0:+1G               -t1:ef00 {disk_path}",
                       on_line=self.add_log)
            run_stream(f"sgdisk -n2:0:+{state['swap']}G  -t2:8200 {disk_path}",
                       on_line=self.add_log)
            run_stream(f"sgdisk -n3:0:0                -t3:8300 {disk_path}",
                       on_line=self.add_log)
            self.set_progress(10)

            self.set_stage(L("Formatting…", "Formateando…"))
            run_stream(f"mkfs.fat -F32 {p1}", on_line=self.add_log)
            run_stream(f"mkswap {p2}",        on_line=self.add_log)
            run_stream(f"swapon {p2}",        on_line=self.add_log)
            run_stream(f"mkfs.ext4 -F {p3}", on_line=self.add_log)
            self.set_progress(15)

            self.set_stage(L("Mounting filesystems…",
                             "Montando sistemas de archivos…"))
            run_stream(f"mount {p3} /mnt",          on_line=self.add_log)
            run_stream("mkdir -p /mnt/boot/efi",    on_line=self.add_log)
            run_stream(f"mount {p1} /mnt/boot/efi", on_line=self.add_log)
            self.set_progress(18)

            self.set_stage(L("Installing base system — this may take a while…",
                             "Instalando sistema base — esto puede tardar…"))
            pkgs = ("base linux linux-firmware linux-headers sof-firmware "
                    "base-devel grub efibootmgr vim nano networkmanager "
                    "sudo bash-completion")
            rc = self._run_pacman_progress(
                f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self.add_log(L("✗  pacstrap failed. Check /mnt/install_log.txt",
                               "✗  pacstrap falló. Revisa /mnt/install_log.txt"))
                self.failed = True
                self.set_progress(100.0)
                return

            self.set_stage(L("Generating fstab…", "Generando fstab…"))
            run_stream("genfstab -U /mnt >> /mnt/etc/fstab",
                       on_line=self.add_log)
            self.set_progress(53)

            self.set_stage(L("Configuring hostname…",
                             "Configurando hostname…"))
            with open("/mnt/etc/hostname", "w") as f:
                f.write(state["hostname"] + "\n")
            hn = state["hostname"]
            with open("/mnt/etc/hosts", "w") as f:
                f.write(f"127.0.0.1\tlocalhost\n"
                        f"::1\t\tlocalhost\n"
                        f"127.0.1.1\t{hn}.localdomain\t{hn}\n")
            self.set_progress(55)

            self.set_stage(L("Configuring locale & timezone…",
                             "Configurando locale y zona horaria…"))
            locale_map = {
                "en": "en_US.UTF-8",
                "es": "es_ES.UTF-8",
                "fr": "fr_FR.UTF-8",
                "de": "de_DE.UTF-8",
                "ru": "ru_RU.UTF-8",
                "ar": "ar_EG.UTF-8",
            }
            locale      = locale_map.get(state["lang"], "en_US.UTF-8")
            locale_line = f"{locale} UTF-8"
            chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' "
                   "/etc/locale.gen")
            if locale != "en_US.UTF-8":
                chroot(f"sed -i 's/^#{locale_line}/{locale_line}/' "
                       "/etc/locale.gen")
            chroot("locale-gen")
            chroot(f"echo 'LANG={locale}' > /etc/locale.conf")
            chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} "
                   "/etc/localtime")
            chroot("hwclock --systohc")
            km = state["keymap"]
            chroot(f"echo 'KEYMAP={km}' > /etc/vconsole.conf")
            self.set_progress(59)

            self.set_stage(L("Generating initramfs…",
                             "Generando initramfs…"))
            chroot("mkinitcpio -P")
            self.set_progress(63)

            self.set_stage(L("Setting passwords…",
                             "Estableciendo contraseñas…"))
            chroot_passwd("root", state["root_pass"])
            self.set_progress(65)

            uname = state["username"]
            self.set_stage(L(f"Creating user '{uname}'…",
                             f"Creando usuario '{uname}'…"))
            chroot(f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}")
            chroot_passwd(uname, state["user_pass"])
            chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/"
                   "%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers")
            self.set_progress(68)

            self.set_stage(L("Enabling NetworkManager…",
                             "Habilitando NetworkManager…"))
            chroot("systemctl enable NetworkManager")
            self.set_progress(71)

            if state["gpu"] == "NVIDIA":
                self.set_stage(L("Installing NVIDIA drivers…",
                                 "Instalando drivers NVIDIA…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "nvidia nvidia-utils nvidia-settings",
                    71, 77, ignore_error=True)
            elif state["gpu"] == "AMD/Intel":
                self.set_stage(L("Installing AMD/Intel drivers…",
                                 "Instalando drivers AMD/Intel…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "mesa vulkan-radeon libva-mesa-driver",
                    71, 77, ignore_error=True)
            else:
                self.set_progress(77)

            if state["desktop"] == "KDE Plasma":
                self.set_stage(L("Installing KDE Plasma…",
                                 "Instalando KDE Plasma…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr "
                    "xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "plasma-meta konsole dolphin ark kate plasma-nm "
                    "firefox sddm",
                    83, 93, ignore_error=True)
                chroot("systemctl enable sddm")
            elif state["desktop"] == "Cinnamon":
                self.set_stage(L("Installing Cinnamon…",
                                 "Instalando Cinnamon…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr "
                    "xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "cinnamon lightdm lightdm-gtk-greeter alacritty firefox",
                    83, 93, ignore_error=True)
                chroot("systemctl enable lightdm")
            else:
                self.set_progress(93)

            self.set_stage(L("Installing GRUB bootloader…",
                             "Instalando GRUB…"))
            chroot("grub-install --target=x86_64-efi "
                   "--efi-directory=/boot/efi --bootloader-id=GRUB")
            chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self.set_progress(100.0)
            self.set_stage(L("✔  Installation complete!",
                             "✔  ¡Instalación completa!"))

        except Exception as e:
            self.add_log(f"FATAL: {e}")
            self.failed = True
            self.set_progress(100.0)

    def _reboot_countdown(self, s):
        countdown = 5
        last_tick = time.time()
        s.timeout(200)

        while countdown > 0:
            now = time.time()
            if now - last_tick >= 1.0:
                countdown -= 1
                last_tick  = now
            try:
                s.erase()
                s.bkgd(" ", _C.get("n", 0))
                h, w = curses.LINES, curses.COLS
                draw_box(s, 0, 0, h, w)
                draw_hline(s, 2, w)
                safe_addstr(s, 1, 2, " ❱ Arch Linux Installer ",
                            _C.get("a", 0) | curses.A_BOLD)
                msg = L(f"Rebooting in {countdown}…",
                        f"Reiniciando en {countdown}…")
                safe_addstr(s, 4, max(2, (w - len(msg)) // 2), msg,
                            _C.get("s", 0) | curses.A_BOLD)
                bar_w  = 20
                filled = min(bar_w, int(((5 - countdown) / 5.0) * bar_w))
                bar    = "█" * filled + "░" * (bar_w - filled)
                safe_addstr(s, 6, max(2, (w - bar_w) // 2), bar,
                            _C.get("s", 0))
                cancel = L("Press any key to cancel.",
                           "Presiona cualquier tecla para cancelar.")
                safe_addstr(s, 8, max(2, (w - len(cancel)) // 2), cancel,
                            _C.get("w", 0))
                s.refresh()
            except curses.error:
                pass

            k = s.getch()
            if k != -1:
                s.timeout(-1)
                return

        s.timeout(-1)
        append_buffer_add("Rebooting…")
        subprocess.run("umount -R /mnt", shell=True)
        subprocess.run("reboot",         shell=True)
        sys.exit(0)

    def _reboot_prompt(self):
        self.done = True
        s = self.stdscr
        s.erase()
        s.refresh()

        if self.failed:
            s.timeout(-1)
            while True:
                try:
                    s.erase()
                    s.bkgd(" ", _C.get("n", 0))
                    h, w = curses.LINES, curses.COLS
                    draw_box(s, 0, 0, h, w)
                    draw_hline(s, 2, w)
                    safe_addstr(s, 1, 2, " ❱ Arch Linux Installer ",
                                _C.get("a", 0) | curses.A_BOLD)
                    safe_addstr(s, 4, 4,
                        L("✗  Installation failed.",
                          "✗  Instalación fallida."),
                        _C.get("e", 0) | curses.A_BOLD)
                    safe_addstr(s, 6, 4,
                        L("See /mnt/install_log.txt for details.",
                          "Revisa /mnt/install_log.txt para detalles."),
                        _C.get("w", 0))
                    safe_addstr(s, 8, 4,
                        L("Press any key to exit.",
                          "Presiona cualquier tecla para salir."),
                        _C.get("n", 0))
                    s.refresh()
                    s.getch()
                except curses.error:
                    pass
                sys.exit(1)

        opts = [
            L("Yes — reboot now",   "Sí — reiniciar ahora"),
            L("No — stay in shell", "No — quedarme en shell"),
        ]
        sel = 0
        s.timeout(-1)

        while True:
            try:
                s.erase()
                s.bkgd(" ", _C.get("n", 0))
                h, w = curses.LINES, curses.COLS
                draw_box(s, 0, 0, h, w)
                draw_hline(s, 2, w)
                safe_addstr(s, 1, 2, " ❱ Arch Linux Installer ",
                            _C.get("a", 0) | curses.A_BOLD)
                safe_addstr(s, 4, 4,
                    L("✔  Installation complete!",
                      "✔  ¡Instalación completa!"),
                    _C.get("s", 0) | curses.A_BOLD)
                safe_addstr(s, 5, 4,
                    L("Reboot now?", "¿Reiniciar ahora?"),
                    _C.get("n", 0) | curses.A_BOLD)
                for i, opt in enumerate(opts):
                    attr = _C.get("l", 0) if i == sel else _C.get("n", 0)
                    safe_addstr(s, 7 + i, 6, f"  {opt:<32}  ", attr)
                safe_addstr(s, 11, 6, "↑↓  Enter=confirm",
                            _C.get("a", 0))
                s.refresh()
            except curses.error:
                pass

            k = s.getch()
            if k == curses.KEY_UP:
                sel = (sel - 1) % len(opts)
            elif k == curses.KEY_DOWN:
                sel = (sel + 1) % len(opts)
            elif k in (10, 13):
                if sel == 1:
                    sys.exit(0)
                self._reboot_countdown(s)

    def start(self):
        t = threading.Thread(target=self.run_steps, daemon=True)
        t.start()
        if not self.stdscr:
            t.join()
            return
        while t.is_alive() or self.progress < 100.0:
            self.spin_idx += 1
            self.redraw()
            self.stdscr.timeout(150)
            self.stdscr.getch()
        self.stdscr.timeout(-1)
        self._reboot_prompt()


def screen_install_c(stdscr):
    InstallerUI(stdscr).start()


def main_curses(stdscr):
    init_colors()
    stdscr.bkgd(" ", _C.get("n", 0))
    curses.curs_set(0)
    funcs = {
        "welcome":   screen_welcome_c,
        "language":  screen_language_c,
        "identity":  screen_identity_c,
        "passwords": screen_passwords_c,
        "disk":      screen_disk_c,
        "keymap":    screen_keymap_c,
        "timezone":  screen_timezone_c,
        "desktop":   screen_desktop_c,
        "gpu":       screen_gpu_c,
        "review":    screen_review_c,
        "install":   screen_install_c,
    }
    idx = 0
    while 0 <= idx < len(screens):
        name   = screens[idx]
        result = funcs[name](stdscr)
        if name == "install":
            break
        if result == BACK:
            idx = max(0, idx - 1)
        else:
            idx += 1


def prompt_password_cli(prompt):
    sys.stdout.write(prompt + " ")
    sys.stdout.flush()
    fd  = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    pwd = ""
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ("\n", "\r"):
                sys.stdout.write("\n")
                break
            if ch == "\x7f":
                if pwd:
                    pwd = pwd[:-1]
                    sys.stdout.write("\b \b")
                    sys.stdout.flush()
            else:
                pwd += ch
                sys.stdout.write("*")
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return pwd


def get_input_cli(prompt, default=""):
    sys.stdout.write(prompt + (f" [{default}]" if default else "") + " ")
    sys.stdout.flush()
    s = input().strip()
    return s if s else default


def fallback_cli():
    print("=== Arch Linux Installer (text mode) ===\n")

    while True:
        c = input("Language: 1=English  2=Español > ").strip()
        if c == "1":
            state["lang"] = "en"
            break
        if c == "2":
            state["lang"] = "es"
            break

    while True:
        state["hostname"] = get_input_cli(L("Hostname:", "Nombre del equipo:"))
        if validate_name(state["hostname"]):
            break
        print(L("Invalid hostname (a-z 0-9 - _ · 1-32 chars).",
                "Hostname inválido."))

    while True:
        state["username"] = get_input_cli(L("Username:", "Usuario:"))
        if validate_name(state["username"]):
            break
        print(L("Invalid username.", "Usuario inválido."))

    while True:
        p1 = prompt_password_cli(L("Root password:", "Contraseña root:"))
        p2 = prompt_password_cli(L("Confirm root:", "Confirmar root:"))
        if p1 and p1 == p2:
            state["root_pass"] = p1
            break
        print(L("Empty or mismatch.", "Vacía o no coincide."))

    while True:
        p1 = prompt_password_cli(L("User password:", "Contraseña usuario:"))
        p2 = prompt_password_cli(L("Confirm user:", "Confirmar usuario:"))
        if p1 and p1 == p2:
            state["user_pass"] = p1
            break
        print(L("Empty or mismatch.", "Vacía o no coincide."))

    while True:
        val = get_input_cli(L("Swap GB (1-128):", "Swap GB (1-128):"), "8")
        if validate_swap(val):
            state["swap"] = val
            break

    disks = list_disks()
    if not disks:
        print(L("No disks found.", "No hay discos."))
        sys.exit(1)
    for i, (n, gb, model) in enumerate(disks, 1):
        print(f"  {i}. /dev/{n}  {gb} GB  {model}")
    while True:
        ch = input(L("Select disk: ", "Seleccionar disco: ")).strip()
        if ch.isdigit() and 1 <= int(ch) <= len(disks):
            state["disk"] = disks[int(ch) - 1][0]
            break

    print("\n─── Summary ───────────────────────")
    for k in ("lang", "hostname", "username", "disk", "swap", "desktop", "gpu"):
        print(f"  {k:<12} {state.get(k)}")
    print("───────────────────────────────────")
    ok = input(L("\n⚠  THIS WILL ERASE THE DISK. Proceed? (y/N): ",
                 "\n⚠  ESTO BORRARÁ EL DISCO. ¿Continuar? (y/N): ")).strip().lower()
    if ok == "y":
        InstallerUI(None).run_steps()
    else:
        print(L("Aborted.", "Cancelado."))


if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root in the Arch ISO live environment.")
        sys.exit(1)
    try:
        curses.wrapper(main_curses)
    except Exception as e:
        print(f"TUI unavailable ({e}), switching to text mode.")
        fallback_cli()
