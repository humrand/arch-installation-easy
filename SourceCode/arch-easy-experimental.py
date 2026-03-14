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

LOG_FILE = "/mnt/install_log.txt"
ESC = 27
BACK = "back"

_PAT_INSTALL  = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)

def nowtag():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except Exception:
        pass

append_buffer = []
append_lock = threading.Lock()

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
    "language", "identity", "passwords", "disk",
    "keymap", "timezone", "desktop", "gpu", "review", "install"
]

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def draw_header(stdscr, title, step=None, total=None):
    stdscr.erase()
    try:
        banner = "── Arch Linux Installer ──"
        stdscr.addstr(0, 2, banner, curses.A_BOLD)
        if step and total:
            tag = f"Step {step}/{total}"
            stdscr.addstr(0, curses.COLS - len(tag) - 2, tag)
        stdscr.addstr(1, 2, "─" * (curses.COLS - 4))
        stdscr.addstr(2, 4, title, curses.A_BOLD)
    except curses.error:
        pass

def draw_footer(stdscr, msg):
    try:
        stdscr.addstr(curses.LINES - 2, 2, "─" * (curses.COLS - 4))
        stdscr.addstr(curses.LINES - 1, 2, msg)
    except curses.error:
        pass

def input_curses(stdscr, y, x, prompt, initial="", secret=False):
    curses.curs_set(1)
    curses.noecho()
    stdscr.keypad(True)
    s = initial
    fx = x + len(prompt) + 1
    field_w = max(2, curses.COLS - fx - 2)
    while True:
        display = "*" * len(s) if secret else s
        try:
            stdscr.addstr(y, x, prompt, curses.A_BOLD)
            stdscr.addstr(y, fx, " " * field_w)
            stdscr.addstr(y, fx, display[:field_w])
            stdscr.move(y, min(fx + len(display), curses.COLS - 1))
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
    per_page = max(6, curses.LINES - 10)
    page_start = 0
    sel = 0
    while True:
        stdscr.erase()
        try:
            stdscr.addstr(1, 2, "── Arch Linux Installer ──", curses.A_BOLD)
            stdscr.addstr(2, 2, "─" * (curses.COLS - 4))
            stdscr.addstr(3, 4, title, curses.A_BOLD)
        except curses.error:
            pass
        page = options[page_start:page_start + per_page]
        for i, opt in enumerate(page):
            attr = curses.A_REVERSE if i == sel else curses.A_NORMAL
            try:
                stdscr.addstr(5 + i, 4, f"  {opt}  ", attr)
            except curses.error:
                pass
        total_pages = max(1, (len(options) - 1) // per_page + 1)
        cur_page = page_start // per_page + 1
        try:
            stdscr.addstr(curses.LINES - 3, 4, f"Page {cur_page}/{total_pages}")
            stdscr.addstr(
                curses.LINES - 2, 2, "─" * (curses.COLS - 4)
            )
            stdscr.addstr(
                curses.LINES - 1, 2,
                L("↑↓ move · PgUp/PgDn page · Enter=select · Esc/q=back",
                  "↑↓ mover · PgUp/PgDn página · Enter=seleccionar · Esc/q=volver")
            )
        except curses.error:
            pass
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
    node = tree
    path = []
    history = []
    while True:
        keys = sorted(node.keys())
        display = [k + ("/" if node[k] else "") for k in keys]
        loc = "/".join(path) if path else L("region", "región")
        idx = curses_picker(stdscr, display, f"Timezone › {loc}")
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

def screen_language_c(stdscr):
    curses.curs_set(0)
    opts = ["English", "Español"]
    sel = 0 if state["lang"] == "en" else 1
    while True:
        draw_header(stdscr, "Language / Idioma", 1, len(screens) - 1)
        for i, opt in enumerate(opts):
            attr = curses.A_REVERSE if i == sel else curses.A_NORMAL
            try:
                stdscr.addstr(5 + i, 6, f"  {opt}  ", attr)
            except curses.error:
                pass
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
        draw_header(stdscr, L("System Identity", "Identidad del sistema"),
                    2, len(screens) - 1)
        try:
            if error:
                stdscr.addstr(4, 4, error, curses.A_BOLD)
            stdscr.addstr(5, 4,
                          L("Esc=back  (letters, digits, - _ · max 32 chars)",
                            "Esc=volver  (letras, dígitos, - _ · máx 32 chars)"))
        except curses.error:
            pass
        stdscr.refresh()

        hostname = input_curses(stdscr, 7, 4,
                                L("Hostname   :", "Nombre equipo:"),
                                state.get("hostname", ""))
        if hostname is None:
            return BACK

        username = input_curses(stdscr, 9, 4,
                                L("Username   :", "Usuario      :"),
                                state.get("username", ""))
        if username is None:
            return BACK

        if not validate_name(hostname):
            error = L("✗  Invalid hostname.", "✗  Hostname inválido.")
            continue
        if not validate_name(username):
            error = L("✗  Invalid username.", "✗  Usuario inválido.")
            continue

        state["hostname"] = hostname
        state["username"] = username
        return

def screen_passwords_c(stdscr):
    error = ""
    while True:
        draw_header(stdscr, L("Passwords", "Contraseñas"), 3, len(screens) - 1)
        try:
            if error:
                stdscr.addstr(4, 4, error, curses.A_BOLD)
            stdscr.addstr(5, 4, L("Esc=back", "Esc=volver"))
        except curses.error:
            pass
        stdscr.refresh()

        prompts = [
            L("Root password       :", "Contraseña root     :"),
            L("Confirm root        :", "Confirmar root      :"),
            L("User password       :", "Contraseña usuario  :"),
            L("Confirm user        :", "Confirmar usuario   :"),
        ]
        vals = []
        cancelled = False
        for i, prompt in enumerate(prompts):
            v = input_curses(stdscr, 7 + i * 2, 4, prompt, secret=True)
            if v is None:
                cancelled = True
                break
            vals.append(v)
        if cancelled:
            return BACK

        rp1, rp2, up1, up2 = vals
        if not rp1:
            error = L("✗  Root password is empty.", "✗  Contraseña root vacía.")
            continue
        if rp1 != rp2:
            error = L("✗  Root passwords do not match.", "✗  Contraseñas root no coinciden.")
            continue
        if not up1:
            error = L("✗  User password is empty.", "✗  Contraseña usuario vacía.")
            continue
        if up1 != up2:
            error = L("✗  User passwords do not match.", "✗  Contraseñas usuario no coinciden.")
            continue

        state["root_pass"] = rp1
        state["user_pass"] = up1
        return

def screen_disk_c(stdscr):
    curses.curs_set(0)
    disks = list_disks()
    if not disks:
        draw_header(stdscr, L("Disk Selection", "Selección de disco"))
        try:
            stdscr.addstr(5, 4,
                          L("✗  No disks found. Aborting.",
                            "✗  No se encontraron discos. Abortando."), curses.A_BOLD)
        except curses.error:
            pass
        stdscr.refresh()
        stdscr.getch()
        sys.exit(1)

    opts = [f"/dev/{d}  {s:>4} GB  {m}" for d, s, m in disks]
    sel = 0
    for i, (d, _, _) in enumerate(disks):
        if d == state.get("disk"):
            sel = i
            break

    while True:
        draw_header(stdscr, L("Disk & Swap", "Disco y Swap"), 4, len(screens) - 1)
        try:
            stdscr.addstr(4, 4,
                          L("⚠  ALL DATA ON THE SELECTED DISK WILL BE ERASED",
                            "⚠  SE BORRARÁN TODOS LOS DATOS DEL DISCO SELECCIONADO"),
                          curses.A_BOLD)
            for i, opt in enumerate(opts):
                attr = curses.A_REVERSE if i == sel else curses.A_NORMAL
                stdscr.addstr(6 + i, 4, f"  {opt}  ", attr)
            stdscr.addstr(6 + len(opts) + 1, 4,
                          L(f"Swap: {state['swap']} GB  ·  press s to change",
                            f"Swap: {state['swap']} GB  ·  presiona s para cambiar"))
        except curses.error:
            pass
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
            curses.curs_set(1)
            curses.echo()
            row = 6 + len(opts) + 1
            try:
                stdscr.addstr(row, 4, L("Swap GB (1–128): ", "Swap GB (1–128): "))
                stdscr.refresh()
                val = stdscr.getstr(row, 22, 4).decode().strip()
            except Exception:
                val = ""
            curses.noecho()
            curses.curs_set(0)
            if validate_swap(val):
                state["swap"] = val
        elif k == ESC:
            return BACK
        elif k == ord("q"):
            sys.exit(0)

def screen_keymap_c(stdscr):
    try:
        out = subprocess.check_output(
            "localectl list-keymaps 2>/dev/null || true", shell=True, text=True
        )
        maps = [l for l in out.splitlines() if l]
    except Exception:
        maps = []
    wanted = ["us", "es", "fr", "de", "ru", "ara"]
    options = [m for m in wanted if m in maps] if maps else wanted
    idx = curses_picker(stdscr, options, L("Choose keymap", "Seleccionar teclado"))
    if idx is None:
        return BACK
    state["keymap"] = options[idx]
    run_simple(f"loadkeys {shlex.quote(state['keymap'])}", ignore_error=True)

def screen_timezone_c(stdscr):
    try:
        out = subprocess.check_output(
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
    tz = traverse_tree_picker(stdscr, tree)
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
        draw_header(stdscr, title, step, len(screens) - 1)
        for i, opt in enumerate(opts):
            attr = curses.A_REVERSE if i == sel else curses.A_NORMAL
            try:
                stdscr.addstr(5 + i, 6, f"  {opt}  ", attr)
            except curses.error:
                pass
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

def screen_review_c(stdscr):
    curses.curs_set(0)
    while True:
        draw_header(stdscr, L("Review & Confirm", "Revisar y confirmar"),
                    9, len(screens) - 1)
        disk_label = f"/dev/{state['disk']}" if state["disk"] else L("NOT SET", "SIN ASIGNAR")
        rows = [
            (L("Language",  "Idioma"),   state["lang"]),
            ("Hostname",                 state["hostname"] or "—"),
            (L("Username",  "Usuario"),  state["username"] or "—"),
            ("Disk",                     disk_label),
            ("Swap",                     f"{state['swap']} GB"),
            ("Keymap",                   state["keymap"]),
            ("Timezone",                 state["timezone"]),
            ("Desktop",                  state["desktop"]),
            ("GPU",                      state["gpu"]),
        ]
        y = 4
        for label, val in rows:
            try:
                stdscr.addstr(y, 4, f"  {label:<18} {val}")
            except curses.error:
                pass
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
            try:
                stdscr.addstr(y + 1, 4,
                              L(f"✗  Missing: {', '.join(missing)}",
                                f"✗  Faltan: {', '.join(missing)}"),
                              curses.A_BOLD)
            except curses.error:
                pass

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
        self.stdscr   = stdscr
        self.logs     = []
        self.progress = 0.0
        self.stage    = ""
        self.failed   = False
        self.lock     = threading.Lock()
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
        self.redraw()

    def set_stage(self, msg):
        self.stage = msg
        self.add_log(f">>> {msg}")

    def set_progress(self, pct):
        with self.lock:
            self.progress = max(0.0, min(100.0, pct))
        self.redraw()

    def redraw(self):
        if not self.stdscr:
            return
        s = self.stdscr
        try:
            s.erase()
            s.addstr(0, 2, "── Arch Linux Installer ──", curses.A_BOLD)
            s.addstr(1, 2, "─" * (curses.COLS - 4))
            s.addstr(2, 4, L("Installing Arch Linux…", "Instalando Arch Linux…"), curses.A_BOLD)
            s.addstr(3, 4, self.stage[: curses.COLS - 6])

            pct   = int(self.progress)
            width = max(10, min(60, curses.COLS - 14))
            filled = int((self.progress / 100.0) * width)
            bar = "[" + "█" * filled + "░" * (width - filled) + "]"
            s.addstr(5, 4, f"{bar} {pct:3}%")

            s.addstr(7, 4, "Log:")
            y = 8
            with self.lock:
                visible = self.logs[-(curses.LINES - y - 2):]
            for line in visible:
                try:
                    s.addstr(y, 4, line[: curses.COLS - 6])
                except curses.error:
                    pass
                y += 1
            s.refresh()
        except curses.error:
            pass

    def _gradual_progress(self, target, duration=0.8):
        base  = self.progress
        steps = max(4, int(duration / 0.05))
        for i in range(1, steps + 1):
            self.set_progress(base + (target - base) * (i / steps))
            time.sleep(duration / steps)
        self.set_progress(target)

    def _run_pacman_progress(self, cmd, start_pct, end_pct, ignore_error=False):
        download_done = [False]

        def on_line(line):
            self.add_log(line)
            m = _PAT_INSTALL.search(line)
            if m:
                download_done[0] = True
                cur, total = int(m.group(1)), int(m.group(2))
                mid = start_pct + (end_pct - start_pct) * 0.5
                if total > 0:
                    pct = mid + (cur / total) * (end_pct - mid)
                    self.set_progress(min(pct, end_pct - 0.5))
                return
            if not download_done[0] and _PAT_DOWNLOAD.search(line):
                cap = start_pct + (end_pct - start_pct) * 0.45
                if self.progress < cap:
                    self.set_progress(self.progress + 0.25)

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
                f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} | arch-chroot /mnt chpasswd",
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
            run_stream(f"sgdisk -n1:0:+1G            -t1:ef00 {disk_path}", on_line=self.add_log)
            run_stream(f"sgdisk -n2:0:+{state['swap']}G -t2:8200 {disk_path}", on_line=self.add_log)
            run_stream(f"sgdisk -n3:0:0             -t3:8300 {disk_path}", on_line=self.add_log)
            self.set_progress(10)

            self.set_stage(L("Formatting…", "Formateando…"))
            run_stream(f"mkfs.fat -F32 {p1}", on_line=self.add_log)
            run_stream(f"mkswap {p2}",        on_line=self.add_log)
            run_stream(f"swapon {p2}",        on_line=self.add_log)
            run_stream(f"mkfs.ext4 -F {p3}", on_line=self.add_log)
            self.set_progress(15)

            self.set_stage(L("Mounting filesystems…", "Montando sistemas de archivos…"))
            run_stream(f"mount {p3} /mnt",           on_line=self.add_log)
            run_stream("mkdir -p /mnt/boot/efi",     on_line=self.add_log)
            run_stream(f"mount {p1} /mnt/boot/efi",  on_line=self.add_log)
            self.set_progress(18)

            self.set_stage(L("Installing base system — this will take a while…",
                             "Instalando sistema base — esto tardará un rato…"))
            pkgs = ("base linux linux-firmware linux-headers sof-firmware "
                    "base-devel grub efibootmgr vim nano networkmanager "
                    "sudo bash-completion")
            rc = self._run_pacman_progress(f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self.add_log(L("✗  pacstrap failed. Check /mnt/install_log.txt",
                               "✗  pacstrap falló. Revisa /mnt/install_log.txt"))
                self.failed = True
                self.set_progress(100.0)
                return

            self.set_stage("Generating fstab…")
            run_stream("genfstab -U /mnt >> /mnt/etc/fstab", on_line=self.add_log)
            self.set_progress(53)

            self.set_stage(L("Configuring hostname…", "Configurando hostname…"))
            with open("/mnt/etc/hostname", "w") as f:
                f.write(state["hostname"] + "\n")
            with open("/mnt/etc/hosts", "w") as f:
                hn = state["hostname"]
                f.write(f"127.0.0.1\tlocalhost\n"
                        f"::1\t\tlocalhost\n"
                        f"127.0.1.1\t{hn}.localdomain\t{hn}\n")
            self.set_progress(55)

            self.set_stage(L("Configuring locale & timezone…",
                             "Configurando locale y zona horaria…"))
            chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            chroot("locale-gen")
            chroot("echo 'LANG=en_US.UTF-8' > /etc/locale.conf")
            chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
            chroot("hwclock --systohc")
            km = state['keymap']
            chroot(f"echo 'KEYMAP={km}' > /etc/vconsole.conf")
            self.set_progress(59)

            self.set_stage(L("Generating initramfs…", "Generando initramfs…"))
            chroot("mkinitcpio -P")
            self.set_progress(63)

            self.set_stage(L("Setting passwords…", "Estableciendo contraseñas…"))
            chroot_passwd("root", state["root_pass"])
            self.set_progress(65)

            uname = state["username"]
            self.set_stage(L(f"Creating user '{uname}'…", f"Creando usuario '{uname}'…"))
            chroot(f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}")
            chroot_passwd(uname, state["user_pass"])
            chroot("sed -i "
                   "'s/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' "
                   "/etc/sudoers")
            self.set_progress(68)

            self.set_stage("Enabling NetworkManager…")
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
                self.set_stage(L("Installing KDE Plasma…", "Instalando KDE Plasma…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm",
                    83, 93, ignore_error=True)
                chroot("systemctl enable sddm")
            elif state["desktop"] == "Cinnamon":
                self.set_stage(L("Installing Cinnamon…", "Instalando Cinnamon…"))
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._run_pacman_progress(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "cinnamon lightdm lightdm-gtk-greeter alacritty firefox",
                    83, 93, ignore_error=True)
                chroot("systemctl enable lightdm")
            else:
                self.set_progress(93)

            self.set_stage(L("Installing GRUB bootloader…", "Instalando GRUB…"))
            chroot("grub-install --target=x86_64-efi "
                   "--efi-directory=/boot/efi --bootloader-id=GRUB")
            chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self.set_progress(100.0)
            self.set_stage(L("✔  Installation complete!", "✔  ¡Instalación completa!"))

        except Exception as e:
            self.add_log(f"FATAL: {e}")
            self.failed = True
            self.set_progress(100.0)

    def _reboot_prompt(self):
        if self.failed:
            while True:
                try:
                    self.stdscr.erase()
                    self.stdscr.addstr(
                        2, 2,
                        L("✗  Installation failed. See /mnt/install_log.txt",
                          "✗  Instalación fallida. Revisa /mnt/install_log.txt"),
                        curses.A_BOLD
                    )
                    self.stdscr.addstr(
                        4, 2,
                        L("Press any key to exit.", "Presiona cualquier tecla para salir.")
                    )
                    self.stdscr.refresh()
                    self.stdscr.getch()
                except curses.error:
                    pass
                sys.exit(1)

        opts = [
            L("Yes — reboot now", "Sí — reiniciar ahora"),
            L("No — stay in shell", "No — quedarme en shell"),
        ]
        sel = 0
        while True:
            try:
                self.stdscr.erase()
                self.stdscr.addstr(1, 2, "── Arch Linux Installer ──", curses.A_BOLD)
                self.stdscr.addstr(3, 2,
                                   L("✔  Installation complete! Reboot now?",
                                     "✔  ¡Instalación completa! ¿Reiniciar ahora?"),
                                   curses.A_BOLD)
                for i, opt in enumerate(opts):
                    attr = curses.A_REVERSE if i == sel else curses.A_NORMAL
                    self.stdscr.addstr(5 + i, 4, f"  {opt}  ", attr)
                self.stdscr.addstr(9, 4, "↑↓  Enter=confirm")
                self.stdscr.refresh()
            except curses.error:
                pass
            k = self.stdscr.getch()
            if k == curses.KEY_UP:
                sel = (sel - 1) % len(opts)
            elif k == curses.KEY_DOWN:
                sel = (sel + 1) % len(opts)
            elif k in (10, 13):
                if sel == 0:
                    append_buffer_add("Rebooting…")
                    subprocess.run("umount -R /mnt", shell=True)
                    subprocess.run("reboot", shell=True)
                else:
                    sys.exit(0)

    def start(self):
        t = threading.Thread(target=self.run_steps, daemon=True)
        t.start()
        if not self.stdscr:
            t.join()
            return
        while t.is_alive() or self.progress < 100.0:
            self.redraw()
            self.stdscr.timeout(250)
            self.stdscr.getch()
        self.stdscr.timeout(-1)
        self._reboot_prompt()

def screen_install_c(stdscr):
    InstallerUI(stdscr).start()

def main_curses(stdscr):
    curses.curs_set(0)
    funcs = {
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
            state["lang"] = "en"; break
        if c == "2":
            state["lang"] = "es"; break

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
        p2 = prompt_password_cli(L("Confirm root password:", "Confirmar root:"))
        if p1 and p1 == p2:
            state["root_pass"] = p1; break
        print(L("Empty or mismatch.", "Vacía o no coincide."))

    while True:
        p1 = prompt_password_cli(L("User password:", "Contraseña usuario:"))
        p2 = prompt_password_cli(L("Confirm user password:", "Confirmar usuario:"))
        if p1 and p1 == p2:
            state["user_pass"] = p1; break
        print(L("Empty or mismatch.", "Vacía o no coincide."))

    while True:
        val = get_input_cli(L("Swap GB (1–128):", "Swap GB (1–128):"), "8")
        if validate_swap(val):
            state["swap"] = val; break

    disks = list_disks()
    if not disks:
        print(L("No disks found.", "No hay discos.")); sys.exit(1)
    for i, (n, gb, model) in enumerate(disks, 1):
        print(f"  {i}. /dev/{n}  {gb} GB  {model}")
    while True:
        ch = input(L("Select disk: ", "Seleccionar disco: ")).strip()
        if ch.isdigit() and 1 <= int(ch) <= len(disks):
            state["disk"] = disks[int(ch) - 1][0]; break

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
