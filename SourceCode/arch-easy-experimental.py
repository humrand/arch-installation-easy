import curses
import subprocess
import sys
import os
import re
import shutil
from datetime import datetime
import termios
import tty
import threading
import time
import shlex

LOG_FILE = "/mnt/install_log.txt"

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

def run(cmd, ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True, executable="/bin/bash")
    if result.returncode != 0:
        append_buffer_add(f"ERROR: Command failed: {cmd}")
        if not ignore_error:
            print("Command failed. Aborting.")
            sys.exit(1)
    return result.returncode

def run_stream(cmd, on_line=None, cwd=None, ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                         cwd=cwd, executable="/bin/bash")
    buf = b""
    while True:
        chunk = p.stdout.read(512)
        if not chunk:
            if p.poll() is not None:
                break
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
        append_buffer_add(f"ERROR: command returned {rc}: {cmd}")
    return rc

def run_simple(cmd, ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    r = subprocess.call(cmd, shell=True, executable="/bin/bash")
    if r != 0 and not ignore_error:
        append_buffer_add(f"ERROR: command returned {r}: {cmd}")
    return r

def ensure_network():
    rc = subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1", shell=True, executable="/bin/bash")
    if rc == 0:
        return True
    if shutil.which("dhcpcd"):
        run_simple("dhcpcd --nobackground >/dev/null 2>&1 &", ignore_error=True)
        time.sleep(3)
        rc2 = subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1", shell=True, executable="/bin/bash")
        return rc2 == 0
    return False

def list_disks():
    try:
        out = subprocess.check_output("lsblk -b -d -o NAME,SIZE | tail -n +2", shell=True, text=True)
    except Exception:
        return []
    disks = []
    for line in out.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name = parts[0]
        try:
            size_gb = int(parts[1]) // (1024**3)
        except Exception:
            size_gb = 0
        disks.append((name, size_gb))
    return disks

def partition_paths_for(disk_path):
    if "nvme" in disk_path or "mmcblk" in disk_path:
        return f"{disk_path}p1", f"{disk_path}p2", f"{disk_path}p3"
    return f"{disk_path}1", f"{disk_path}2", f"{disk_path}3"

def prompt_password(prompt):
    sys.stdout.write(prompt + " ")
    sys.stdout.flush()
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    passwd = ""
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ("\n", "\r"):
                sys.stdout.write("\n")
                break
            if ch == "\x7f":
                if passwd:
                    passwd = passwd[:-1]
                    sys.stdout.write("\b \b")
                    sys.stdout.flush()
            else:
                passwd += ch
                sys.stdout.write("*")
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return passwd

def get_input(prompt, default=""):
    sys.stdout.write(prompt + (" [" + str(default) + "]" if default else "") + " ")
    sys.stdout.flush()
    s = input().strip()
    return s if s else default

def validate_name(n):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", n))

def validate_swap(s):
    return bool(re.match(r"^\d+$", s)) and int(s) > 0

def curses_picker(stdscr, options, title):
    per_page = max(6, curses.LINES - 6)
    idx = 0
    sel = 0
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, title)
        page = options[idx:idx+per_page]
        for i, opt in enumerate(page):
            marker = ">" if i == sel else " "
            try:
                stdscr.addstr(3+i, 4, f"{marker} {opt}")
            except curses.error:
                pass
        stdscr.addstr(curses.LINES-2, 4, "Use UP/DOWN PageUp/PageDown Enter=select q=cancel")
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(page)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(page)
        elif k == curses.KEY_NPAGE:
            if idx + per_page < len(options):
                idx += per_page
                sel = 0
        elif k == curses.KEY_PPAGE:
            if idx - per_page >= 0:
                idx -= per_page
                sel = 0
        elif k in (10, 13):
            return idx + sel
        elif k == ord("q"):
            return None

def input_curses(stdscr, y, x, prompt, initial="", secret=False):
    if not secret:
        curses.echo()
    else:
        curses.noecho()
    stdscr.addstr(y, x, prompt)
    stdscr.refresh()
    win = curses.newwin(1, 60, y, x+len(prompt)+1)
    try:
        win.addstr(0, 0, initial)
    except Exception:
        pass
    win.refresh()
    s = ""
    if not secret:
        try:
            s = win.getstr().decode().strip()
        except Exception:
            s = ""
    else:
        while True:
            ch = win.get_wch()
            if ch in ("\n", "\r"):
                break
            if ch == "\x7f":
                if len(s) > 0:
                    s = s[:-1]
                    yx = win.getyx()
                    if yx[1] > 0:
                        win.delch(0, yx[1]-1)
            else:
                s += str(ch)
                win.addstr("*")
    curses.echo()
    return s

def build_zone_tree(zones):
    tree = {}
    for z in zones:
        parts = z.split('/')
        node = tree
        for p in parts:
            node = node.setdefault(p, {})
    return tree

def traverse_tree_picker(stdscr, tree, prefix=""):
    node = tree
    path = []
    while True:
        keys = sorted(node.keys())
        display = [k + ("/" if node[k] else "") for k in keys]
        idx = curses_picker(stdscr, display, f"Choose: {'/'.join(path) if path else 'region'}")
        if idx is None:
            return None
        chosen = keys[idx]
        path.append(chosen)
        if node[chosen]:
            node = node[chosen]
            continue
        return "/".join(path)


state = {
    "lang": "en",
    "hostname": "",
    "username": "",
    "root_pass": "",
    "user_pass": "",
    "swap": "8",
    "disk": None,
    "desktop": "None",
    "gpu": "None",
    "keymap": "us",
    "timezone": "UTC",
    "locale": "en_US.UTF-8"
}

screens = ["language", "identity", "passwords", "disk", "keymap", "timezone", "desktop", "gpu", "review", "install"]

def L(en, es):
    return en if state.get("lang", "en") == "en" else es


def screen_language_c(stdscr):
    curses.curs_set(0)
    opts = ["English", "Español"]
    sel = 0 if state["lang"] == "en" else 1
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, "Language / Idioma")
        for i, opt in enumerate(opts):
            marker = "[x]" if i == sel else "[ ]"
            stdscr.addstr(3+i, 4, f"{marker} {opt}")
        stdscr.addstr(8, 4, "UP/DOWN to move, Enter to select")
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
    curses.curs_set(1)
    stdscr.erase()
    stdscr.addstr(1, 2, L("System Identity", "Identidad del sistema"))
    state["hostname"] = input_curses(stdscr, 4, 4, L("Hostname:", "Nombre del equipo:"), state.get("hostname", ""))
    state["username"] = input_curses(stdscr, 6, 4, L("Username:", "Usuario:"), state.get("username", ""))
    if not validate_name(state["hostname"]):
        state["hostname"] = ""
    if not validate_name(state["username"]):
        state["username"] = ""

def screen_passwords_c(stdscr):
    curses.curs_set(1)
    stdscr.erase()
    stdscr.addstr(1, 2, L("Passwords", "Contraseñas"))
    state["root_pass"] = input_curses(stdscr, 4, 4, L("Root password:", "Contraseña root:"), secret=True)
    state["user_pass"] = input_curses(stdscr, 6, 4, L("User password:", "Contraseña usuario:"), secret=True)

def screen_disk_c(stdscr):
    curses.curs_set(0)
    disks = list_disks()
    if not disks:
        stdscr.addstr(2, 2, L("No disks found. Aborting.", "No se encontraron discos. Abortando."))
        stdscr.getch()
        sys.exit(1)
    opts = [f"/dev/{d} ({s} GB)" for d, s in disks]
    sel = 0
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, L("Disk & Swap", "Disco y Swap"))
        for i, opt in enumerate(opts):
            marker = "[x]" if i == sel else "[ ]"
            stdscr.addstr(3+i, 4, f"{marker} {opt}")
        stdscr.addstr(3+len(opts)+1, 4, f"Swap: {state['swap']} GB")
        stdscr.addstr(3+len(opts)+3, 4, L("UP/DOWN Enter=select  s=change swap", "Arriba/Abajo Enter=seleccionar  s=cambiar swap"))
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10, 13):
            state["disk"] = disks[sel][0]
            return
        elif k == ord("s"):
            curses.echo()
            stdscr.addstr(3+len(opts)+1, 4, "Swap GB: ")
            try:
                val = stdscr.getstr(3+len(opts)+1, 13, 6).decode().strip()
            except Exception:
                val = ""
            curses.noecho()
            if validate_swap(val):
                state["swap"] = val
        elif k == ord("q"):
            sys.exit(0)

def screen_keymap_c(stdscr):
    curses.curs_set(0)
    stdscr.erase()
    stdscr.addstr(1, 2, L("Keymap / Teclado", "Keymap / Teclado"))
    try:
        out = subprocess.check_output("localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
        maps = [l for l in out.splitlines() if l]
    except Exception:
        maps = []
    common = ["us", "es", "fr", "de", "it", "pt", "la-latin1"]
    options = [m for m in common if m in maps]
    if not options:
        options = maps[:60] if maps else common
    idx = curses_picker(stdscr, options, L("Choose keymap", "Seleccionar keymap"))
    if idx is not None:
        km = options[idx]
        state["keymap"] = km
        run_simple(f"loadkeys {km}", ignore_error=True)

def screen_timezone_c(stdscr):
    curses.curs_set(0)
    try:
        out = subprocess.check_output("timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
        zones = [l for l in out.splitlines() if l]
    except Exception:
        zones = []
    if not zones:
        zones = ["UTC", "Europe/Madrid", "America/New_York", "America/Los_Angeles", "Asia/Tokyo"]
    tree = build_zone_tree(zones)
    tz = traverse_tree_picker(stdscr, tree)
    if tz is not None:
        state["timezone"] = tz

def screen_desktop_c(stdscr):
    curses.curs_set(0)
    opts = ["KDE Plasma", "Cinnamon", "None"]
    sel = opts.index(state["desktop"]) if state["desktop"] in opts else 2
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, L("Desktop Environment", "Entorno de escritorio"))
        for i, opt in enumerate(opts):
            marker = "[x]" if i == sel else "[ ]"
            stdscr.addstr(3+i, 4, f"{marker} {opt}")
        stdscr.addstr(7, 4, L("UP/DOWN Enter=select", "Arriba/Abajo Enter=seleccionar"))
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10, 13):
            state["desktop"] = opts[sel]
            return
        elif k == ord("q"):
            sys.exit(0)

def screen_gpu_c(stdscr):
    curses.curs_set(0)
    opts = ["NVIDIA", "AMD / Intel", "None"]
    if state["gpu"] == "NVIDIA":
        sel = 0
    elif state["gpu"] in ("AMD/Intel", "AMD / Intel"):
        sel = 1
    else:
        sel = 2
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, L("GPU Drivers", "Drivers GPU"))
        for i, opt in enumerate(opts):
            marker = "[x]" if i == sel else "[ ]"
            stdscr.addstr(3+i, 4, f"{marker} {opt}")
        stdscr.addstr(7, 4, L("UP/DOWN Enter=select", "Arriba/Abajo Enter=seleccionar"))
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10, 13):
            state["gpu"] = "NVIDIA" if sel == 0 else ("AMD/Intel" if sel == 1 else "None")
            return
        elif k == ord("q"):
            sys.exit(0)

def screen_review_c(stdscr):
    curses.curs_set(0)
    while True:
        stdscr.erase()
        stdscr.addstr(1, 2, L("Review & Install", "Revisar e instalar"))
        lines = [
            (L("Language", "Idioma"),   state["lang"]),
            ("Hostname",               state["hostname"]),
            (L("Username", "Usuario"), state["username"]),
            ("Disk",                   state["disk"]),
            ("Swap",                   f"{state['swap']} GB"),
            ("Desktop",                state["desktop"]),
            ("GPU",                    state["gpu"]),
            ("Keymap",                 state["keymap"]),
            ("Timezone",               state["timezone"]),
        ]
        y = 3
        for k, v in lines:
            stdscr.addstr(y, 4, f"{k}: {v}")
            y += 1
        stdscr.addstr(y+1, 4, L("Enter=start install   q=cancel", "Enter=iniciar instalación   q=cancelar"))
        stdscr.refresh()
        k = stdscr.getch()
        if k in (10, 13):
            return True
        elif k == ord("q"):
            sys.exit(0)


class InstallerUI:
    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.logs = []
        self.progress = 0.0
        self.lock = threading.Lock()
        if self.stdscr is not None:
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

    def set_progress(self, pct):
        with self.lock:
            self.progress = max(0.0, min(100.0, pct))
        self.redraw()

    def redraw(self):
        if self.stdscr is None:
            return
        s = self.stdscr
        s.erase()
        s.addstr(1, 2, L("Installing Arch Linux...", "Instalando Arch Linux..."))
        s.addstr(3, 4, f"Progress: {int(self.progress)}%")
        width = min(60, max(20, curses.COLS-20))
        filled = int((self.progress/100.0)*width)
        bar = "[" + "#"*filled + "-"*(width-filled) + "]"
        s.addstr(4, 4, bar)
        s.addstr(6, 4, "Log:")
        y = 7
        with self.lock:
            for line in self.logs[-(curses.LINES - y - 4):]:
                try:
                    s.addstr(y, 4, line[:curses.COLS-8])
                except curses.error:
                    pass
                y += 1
        s.addstr(curses.LINES-2, 4, "")
        s.refresh()

    def _gradual_progress(self, base, target, duration=0.9):
        steps = max(4, int(duration / 0.06))
        cur = base
        for i in range(steps):
            cur = base + (target - base) * ((i+1)/steps)
            self.set_progress(cur)
            time.sleep(duration/steps)
        self.set_progress(target)

    def _run_pacman_progress(self, cmd, start_pct, end_pct, ignore_error=False):
        pat_install = re.compile(r"\((\d+)/(\d+)\)")
        pat_download = re.compile(r"(\S+)\s+(\d+(?:\.\d+)?)\s*(B|KiB|MiB|GiB)\s+(\d+(?:\.\d+)?)\s*(B|KiB|MiB|GiB)/s")
        download_phase = [True]
        install_phase_start = [None]

        def on_line(line):
            self.add_log(line)
            m = pat_install.search(line)
            if m:
                download_phase[0] = False
                cur, total = int(m.group(1)), int(m.group(2))
                if install_phase_start[0] is None:
                    install_phase_start[0] = start_pct + (end_pct - start_pct) * 0.5
                phase_start = install_phase_start[0]
                if total > 0:
                    pct = phase_start + (cur / total) * (end_pct - phase_start)
                    self.set_progress(min(pct, end_pct - 0.5))
                return
            if download_phase[0] and pat_download.search(line):
                cur_p = self.progress
                if cur_p < start_pct + (end_pct - start_pct) * 0.45:
                    self.set_progress(cur_p + 0.3)

        rc = run_stream(cmd, on_line=on_line, ignore_error=ignore_error)
        self.set_progress(end_pct)
        return rc

    def run_steps(self):
        disk = state["disk"]
        disk_path = f"/dev/{disk}"
        swap_size = state["swap"]
        hostname  = state["hostname"]
        username  = state["username"]
        root_pass = state["root_pass"]
        user_pass = state["user_pass"]

        p1, p2, p3 = partition_paths_for(disk_path)

        def chroot(cmd):
            """Run a command inside the installed system."""
            rc = run_stream(f'arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}',
                            on_line=self.add_log, ignore_error=True)
            return rc


        if not ensure_network():
            self.add_log(L("ERROR: No network. Connect and retry.",
                           "ERROR: Sin red. Conéctese e intente de nuevo."))
            return

        run_stream("pacman -Sy --noconfirm archlinux-keyring",
                   on_line=self.add_log, ignore_error=True)


        self.add_log(L("== Wiping disk ==", "== Borrando disco =="))
        self._gradual_progress(self.progress, 3)
        run_stream(f"sgdisk -Z {disk_path}", on_line=self.add_log)
        self.set_progress(5)


        self.add_log(L("== Creating partitions ==", "== Creando particiones =="))
        run_stream(f"sgdisk -n1:0:+1G   -t1:ef00 {disk_path}", on_line=self.add_log)
        run_stream(f"sgdisk -n2:0:+{swap_size}G -t2:8200 {disk_path}", on_line=self.add_log)
        run_stream(f"sgdisk -n3:0:0     -t3:8300 {disk_path}", on_line=self.add_log)
        self.set_progress(10)


        self.add_log(L("== Formatting ==", "== Formateando =="))
        run_stream(f"mkfs.fat -F32 {p1}", on_line=self.add_log)
        run_stream(f"mkswap {p2}",        on_line=self.add_log)
        run_stream(f"swapon {p2}",        on_line=self.add_log)
        run_stream(f"mkfs.ext4 -F {p3}", on_line=self.add_log)
        self.set_progress(15)


        self.add_log(L("== Mounting ==", "== Montando =="))
        run_stream(f"mount {p3} /mnt",         on_line=self.add_log)
        run_stream("mkdir -p /mnt/boot/efi",   on_line=self.add_log)
        run_stream(f"mount {p1} /mnt/boot/efi", on_line=self.add_log)
        self.set_progress(18)


        self.add_log(L("== Installing base system (pacstrap) ==",
                       "== Instalando sistema base (pacstrap) =="))
        packages = ("base linux linux-firmware linux-headers sof-firmware "
                    "base-devel grub efibootmgr vim nano networkmanager "
                    "sudo bash-completion")
        rc = self._run_pacman_progress(f"pacstrap -K /mnt {packages}", 18, 52)
        if rc != 0:
            self.add_log(L("ERROR: pacstrap failed.", "ERROR: pacstrap falló."))
            return


        self.add_log("== fstab ==")
        run_stream("genfstab -U /mnt >> /mnt/etc/fstab", on_line=self.add_log)
        self.set_progress(53)


        self.add_log(L("== Configuring hostname ==", "== Configurando hostname =="))
        try:
            with open("/mnt/etc/hostname", "w") as f:
                f.write(hostname + "\n")
        except Exception as e:
            self.add_log(f"ERROR writing hostname: {e}")


        run_stream(
            f"echo -e '127.0.0.1\\tlocalhost\\n::1\\t\\tlocalhost\\n127.0.1.1\\t{hostname}.localdomain\\t{hostname}' "
            f"> /mnt/etc/hosts",
            on_line=self.add_log, ignore_error=True
        )
        self.set_progress(55)


        self.add_log(L("== Locale & timezone ==", "== Locale y zona horaria =="))
        chroot("sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen")
        chroot("locale-gen")
        chroot("echo 'LANG=en_US.UTF-8' > /etc/locale.conf")
        chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
        chroot("hwclock --systohc")
        km = state["keymap"]
        chroot(f"echo 'KEYMAP={km}' > /etc/vconsole.conf")
        self.set_progress(60)


        self.add_log(L("== Setting root password ==", "== Contraseña de root =="))
        chroot(f"echo 'root:{root_pass}' | chpasswd")
        self.set_progress(63)

        self.add_log(L(f"== Creating user {username} ==",
                       f"== Creando usuario {username} =="))
        chroot(f"useradd -m -G wheel -s /bin/bash {username}")
        chroot(f"echo '{username}:{user_pass}' | chpasswd")
        chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers")
        self.set_progress(67)


        self.add_log("== NetworkManager ==")
        chroot("systemctl enable NetworkManager")
        self.set_progress(70)


        if state["gpu"] == "NVIDIA":
            self.add_log(L("== Installing NVIDIA drivers ==", "== Instalando drivers NVIDIA =="))
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm nvidia nvidia-utils nvidia-settings",
                70, 76, ignore_error=True)
        elif state["gpu"] == "AMD/Intel":
            self.add_log(L("== Installing AMD/Intel drivers ==", "== Instalando drivers AMD/Intel =="))
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver",
                70, 76, ignore_error=True)
        else:
            self.set_progress(76)


        if state["desktop"] == "KDE Plasma":
            self.add_log(L("== Installing KDE Plasma ==", "== Instalando KDE Plasma =="))
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                76, 82, ignore_error=True)
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm plasma-meta konsole dolphin ark kate plasma-nm firefox sddm",
                82, 92, ignore_error=True)
            chroot("systemctl enable sddm")
        elif state["desktop"] == "Cinnamon":
            self.add_log(L("== Installing Cinnamon ==", "== Instalando Cinnamon =="))
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                76, 82, ignore_error=True)
            self._run_pacman_progress(
                "arch-chroot /mnt pacman -S --noconfirm cinnamon lightdm lightdm-gtk-greeter alacritty firefox",
                82, 92, ignore_error=True)
            chroot("systemctl enable lightdm")
        else:
            self.set_progress(92)


        self.add_log(L("== Installing GRUB ==", "== Instalando GRUB =="))
        chroot("grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB")
        chroot("grub-mkconfig -o /boot/grub/grub.cfg")
        self.set_progress(98)


        self.add_log(L("✔ Installation complete! Press r to reboot.",
                       "✔ ¡Instalación completa! Presiona r para reiniciar."))
        self.set_progress(100.0)

    def _reboot_prompt(self):
        opts = [L("Yes", "Sí"), "No"]
        sel = 0
        while True:
            self.stdscr.erase()
            self.stdscr.addstr(1, 2, L("Reboot now?", "¿Reiniciar ahora?"))
            for i, opt in enumerate(opts):
                marker = "[x]" if i == sel else "[ ]"
                self.stdscr.addstr(3+i, 4, f"{marker} {opt}")
            self.stdscr.addstr(7, 4, L("UP/DOWN Enter=select", "Arriba/Abajo Enter=seleccionar"))
            self.stdscr.refresh()
            k = self.stdscr.getch()
            if k == curses.KEY_UP:
                sel = (sel - 1) % len(opts)
            elif k == curses.KEY_DOWN:
                sel = (sel + 1) % len(opts)
            elif k in (10, 13):
                if sel == 0:
                    append_buffer_add("Rebooting...")
                    subprocess.run("umount -R /mnt", shell=True)
                    subprocess.run("reboot", shell=True)
                else:
                    sys.exit(0)

    def start(self):
        t = threading.Thread(target=self.run_steps, daemon=True)
        t.start()
        if self.stdscr is None:
            t.join()
            return
        while t.is_alive() or self.progress < 100.0:
            self.redraw()
            self.stdscr.timeout(200)
            self.stdscr.getch()
        self.stdscr.timeout(-1)
        self._reboot_prompt()

def screen_install_c(stdscr):
    ui = InstallerUI(stdscr)
    ui.start()


def main_curses(stdscr):
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
    while True:
        name = screens[idx]
        funcs[name](stdscr)
        if name == "install":
            break
        try:
            stdscr.addstr(curses.LINES-2, 2,
                          L("← → or Enter to navigate   q=quit",
                            "← → o Enter para navegar   q=salir"))
        except curses.error:
            pass
        stdscr.refresh()
        k = stdscr.getch()
        if k in (curses.KEY_RIGHT, 10, 13):
            if idx < len(screens) - 1:
                idx += 1
            else:
                break
        elif k == curses.KEY_LEFT:
            if idx > 0:
                idx -= 1
        elif k == ord("q"):
            break


def fallback_cli():
    while True:
        c = input("Select language: 1=EN 2=ES > ").strip()
        if c == "1":
            state["lang"] = "en"; break
        elif c == "2":
            state["lang"] = "es"; break

    state["hostname"] = get_input(L("Hostname:", "Nombre del equipo:"))
    while not validate_name(state["hostname"]):
        state["hostname"] = get_input(L("Invalid. Hostname:", "Inválido. Nombre del equipo:"))

    state["username"] = get_input(L("Username:", "Usuario:"))
    while not validate_name(state["username"]):
        state["username"] = get_input(L("Invalid. Username:", "Inválido. Usuario:"))

    state["root_pass"] = prompt_password(L("Root password:", "Contraseña root:"))
    state["user_pass"] = prompt_password(L("User password:", "Contraseña usuario:"))
    state["swap"] = get_input(L("Swap GB (8):", "Swap GB (8):"), "8")
    while not validate_swap(state["swap"]):
        state["swap"] = get_input(L("Invalid swap:", "Swap inválido:"), "8")

    disks = list_disks()
    if not disks:
        print(L("No disks found.", "No hay discos.")); sys.exit(1)
    for i, (n, gb) in enumerate(disks, 1):
        print(f"{i}. /dev/{n} ({gb} GB)")
    while True:
        ch = input(L("Select disk: ", "Seleccionar disco: ")).strip()
        if ch.isdigit() and 1 <= int(ch) <= len(disks):
            state["disk"] = disks[int(ch)-1][0]; break

    for k in ("lang","hostname","username","disk","swap","desktop","gpu"):
        print(f"{k}: {state.get(k)}")
    ok = input(L("Proceed? (y/n): ", "¿Continuar? (y/n): ")).strip().lower()
    if ok == "y":
        InstallerUI(None).run_steps()


if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root in the Arch live environment.")
        sys.exit(1)
    try:
        curses.wrapper(main_curses)
    except Exception:
        fallback_cli()
