import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

import tkinter as tk
from tkinter import ttk, messagebox, font as tkfont

VERSION  = "V1.1.0-gui"
LOG_FILE = "/mnt/install_log.txt"

BG        = "#0f1117"
BG2       = "#1a1d27"
BG3       = "#252836"
ACCENT    = "#1793d1"        # Arch blue
ACCENT2   = "#0e6fa0"
FG        = "#e2e8f0"
FG_DIM    = "#8892a4"
GREEN     = "#4ade80"
RED       = "#f87171"
YELLOW    = "#facc15"
BORDER    = "#2d3348"

BTN_STYLE = dict(
    bg=ACCENT, fg="white", activebackground=ACCENT2,
    activeforeground="white", relief="flat", cursor="hand2",
    padx=16, pady=8, font=("Inter", 11, "bold"), bd=0
)
BTN_GHOST = dict(
    bg=BG3, fg=FG_DIM, activebackground=BORDER,
    activeforeground=FG, relief="flat", cursor="hand2",
    padx=16, pady=8, font=("Inter", 11), bd=0
)
BTN_DANGER = dict(
    bg="#dc2626", fg="white", activebackground="#b91c1c",
    activeforeground="white", relief="flat", cursor="hand2",
    padx=16, pady=8, font=("Inter", 11, "bold"), bd=0
)

ENTRY_STYLE = dict(
    bg=BG3, fg=FG, insertbackground=FG,
    relief="flat", font=("Mono", 11),
    highlightthickness=1, highlightbackground=BORDER,
    highlightcolor=ACCENT
)

def nowtag():
    return datetime.now().strftime("%H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(f"[{datetime.now():%Y-%m-%d %H:%M:%S}] {line}\n")
    except Exception:
        pass

def run_stream(cmd, on_line=None, ignore_error=False):
    """BUG FIX: proper EOF+buffer drain, no race condition."""
    write_log(f"$ {cmd}")
    p = subprocess.Popen(
        cmd, shell=True, stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT, executable="/bin/bash"
    )
    buf = b""
    while True:
        chunk = p.stdout.read(512)
        if chunk:
            buf += chunk
        else:
            if p.poll() is not None:
                break
            time.sleep(0.02)
            continue
        parts = re.split(b"[\r\n]+", buf)
        buf = parts[-1]
        for part in parts[:-1]:
            line = part.decode("utf-8", errors="replace").strip()
            if line:
                write_log(line)
                if on_line:
                    on_line(line)
    if buf.strip():
        line = buf.decode("utf-8", errors="replace").strip()
        write_log(line)
        if on_line:
            on_line(line)
    rc = p.wait()
    if rc != 0 and not ignore_error:
        write_log(f"ERROR (rc={rc}): {cmd}")
    return rc

def run_simple(cmd, ignore_error=False):
    write_log(f"$ {cmd}")
    r = subprocess.call(cmd, shell=True, executable="/bin/bash")
    if r != 0 and not ignore_error:
        write_log(f"ERROR (rc={r}): {cmd}")
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

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def sep(parent, pady=8):
    tk.Frame(parent, bg=BORDER, height=1).pack(fill="x", padx=20, pady=pady)

def label(parent, text, size=11, bold=False, color=FG, anchor="w"):
    wt = "bold" if bold else "normal"
    return tk.Label(parent, text=text, bg=BG2, fg=color,
                    font=("Inter", size, wt), anchor=anchor)

def make_entry(parent, show=""):
    e = tk.Entry(parent, show=show, **ENTRY_STYLE)
    return e

def card(parent, **kw):
    f = tk.Frame(parent, bg=BG2, relief="flat",
                 highlightthickness=1, highlightbackground=BORDER, **kw)
    return f

def radio_row(parent, text, var, value, command=None):
    row = tk.Frame(parent, bg=BG3, cursor="hand2")
    row.pack(fill="x", padx=4, pady=3)
    def select(_e=None):
        var.set(value)
        if command:
            command(value)
    row.bind("<Button-1>", select)
    rb = tk.Radiobutton(
        row, variable=var, value=value, bg=BG3,
        activebackground=BG3, selectcolor=ACCENT,
        command=lambda: (command(value) if command else None)
    )
    rb.pack(side="left", padx=(8, 4))
    rb.bind("<Button-1>", select)
    lbl = tk.Label(row, text=text, bg=BG3, fg=FG,
                   font=("Inter", 11), anchor="w", cursor="hand2")
    lbl.pack(side="left", padx=4)
    lbl.bind("<Button-1>", select)
    return row

_PAT_INSTALL  = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)

class InstallBackend:
    def __init__(self, on_log, on_progress, on_stage, on_done):
        self.on_log      = on_log
        self.on_progress = on_progress
        self.on_stage    = on_stage
        self.on_done     = on_done          # called with (success: bool)
        self._progress   = 0.0
        self._lock       = threading.Lock()

    def _log(self, msg):
        write_log(msg)
        self.on_log(msg)

    def _stage(self, msg):
        self._log(f">>> {msg}")
        self.on_stage(msg)

    def _pct(self, p):
        with self._lock:
            clamped = max(0.0, min(100.0, float(p)))
            self._progress = max(self._progress, clamped)
        self.on_progress(self._progress)

    def _gradual(self, target, steps=20, delay=0.04):
        with self._lock:
            base = self._progress
        for i in range(1, steps + 1):
            self._pct(base + (target - base) * (i / steps))
            time.sleep(delay)

    def _pacman(self, cmd, start, end, ignore_error=False):
        """BUG FIX: track whether download phase done separately per call."""
        half = start + (end - start) * 0.5
        download_done = [False]

        def on_line(line):
            self._log(line)
            m = _PAT_INSTALL.search(line)
            if m:
                download_done[0] = True
                cur, total = int(m.group(1)), int(m.group(2))
                if total > 0:
                    self._pct(half + (cur / total) * (end - half))
                return
            if not download_done[0] and _PAT_DOWNLOAD.search(line):
                cap = start + (end - start) * 0.45
                with self._lock:
                    cur_p = self._progress
                if cur_p < cap:
                    self._pct(cur_p + 0.3)

        rc = run_stream(cmd, on_line=on_line, ignore_error=ignore_error)
        self._pct(end)
        return rc

    def _chroot(self, cmd, ignore_error=False):
        """BUG FIX: default ignore_error=False so failures surface."""
        return run_stream(
            f"arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}",
            on_line=self._log, ignore_error=ignore_error
        )

    def _chroot_passwd(self, user, pwd):
        return run_stream(
            f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} "
            f"| arch-chroot /mnt chpasswd",
            on_line=self._log, ignore_error=True
        )

    def run(self):
        disk_path   = f"/dev/{state['disk']}"
        p1, p2, p3  = partition_paths_for(disk_path)

        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self._log(L("✗  No network. Connect and retry.",
                            "✗  Sin red. Conéctese e intente de nuevo."))
                self.on_done(False)
                return

            run_stream("pacman -Sy --noconfirm archlinux-keyring",
                       on_line=self._log, ignore_error=True)

            self._stage(L("Wiping disk…", "Borrando disco…"))
            self._gradual(3)
            run_stream(f"sgdisk -Z {disk_path}", on_line=self._log)
            self._pct(5)

            self._stage(L("Creating partitions…", "Creando particiones…"))
            run_stream(f"sgdisk -n1:0:+1G -t1:ef00 {disk_path}", on_line=self._log)
            run_stream(f"sgdisk -n2:0:+{state['swap']}G -t2:8200 {disk_path}", on_line=self._log)
            run_stream(f"sgdisk -n3:0:0 -t3:8300 {disk_path}", on_line=self._log)
            self._pct(10)

            self._stage(L("Formatting partitions…", "Formateando particiones…"))
            run_stream(f"mkfs.fat -F32 {p1}", on_line=self._log)
            run_stream(f"mkswap {p2}",        on_line=self._log)
            run_stream(f"swapon {p2}",        on_line=self._log)
            run_stream(f"mkfs.ext4 -F {p3}", on_line=self._log)
            self._pct(15)

            self._stage(L("Mounting filesystems…", "Montando sistemas de archivos…"))
            run_stream(f"mount {p3} /mnt",          on_line=self._log)
            run_stream("mkdir -p /mnt/boot/efi",    on_line=self._log)
            run_stream(f"mount {p1} /mnt/boot/efi", on_line=self._log)
            self._pct(18)

            self._stage(L("Installing base system — this may take a while…",
                          "Instalando sistema base — esto puede tardar…"))
            pkgs = ("base linux linux-firmware linux-headers sof-firmware "
                    "base-devel grub efibootmgr vim nano networkmanager "
                    "sudo bash-completion")
            rc = self._pacman(f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self._log(L("✗  pacstrap failed. Check /mnt/install_log.txt",
                            "✗  pacstrap falló. Revisa /mnt/install_log.txt"))
                self.on_done(False)
                return

            self._stage(L("Generating fstab…", "Generando fstab…"))
            run_stream("genfstab -U /mnt >> /mnt/etc/fstab", on_line=self._log)
            self._pct(53)

            self._stage(L("Configuring hostname…", "Configurando hostname…"))
            hn = state["hostname"]
            with open("/mnt/etc/hostname", "w") as f:
                f.write(hn + "\n")
            with open("/mnt/etc/hosts", "w") as f:
                f.write(f"127.0.0.1\tlocalhost\n"
                        f"::1\t\tlocalhost\n"
                        f"127.0.1.1\t{hn}.localdomain\t{hn}\n")
            self._pct(55)

            self._stage(L("Configuring locale & timezone…",
                          "Configurando locale y zona horaria…"))
            locale = "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
            locale_line = f"{locale} UTF-8"
            self._chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            if locale != "en_US.UTF-8":
                self._chroot(f"sed -i 's/^#{locale_line}/{locale_line}/' /etc/locale.gen",
                             ignore_error=True)
            self._chroot("locale-gen")
            self._chroot(f"echo 'LANG={locale}' > /etc/locale.conf")
            self._chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
            self._chroot("hwclock --systohc")
            self._chroot(f"echo 'KEYMAP={state['keymap']}' > /etc/vconsole.conf")
            self._pct(59)

            self._stage(L("Generating initramfs…", "Generando initramfs…"))
            self._chroot("mkinitcpio -P")
            self._pct(63)

            self._stage(L("Setting passwords…", "Estableciendo contraseñas…"))
            self._chroot_passwd("root", state["root_pass"])
            self._pct(65)

            uname = state["username"]
            self._stage(L(f"Creating user '{uname}'…", f"Creando usuario '{uname}'…"))
            self._chroot(f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}")
            self._chroot_passwd(uname, state["user_pass"])
            self._chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/"
                         "%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers")
            self._pct(68)

            self._stage(L("Enabling NetworkManager…", "Habilitando NetworkManager…"))
            self._chroot("systemctl enable NetworkManager")
            self._pct(71)

            if state["gpu"] == "NVIDIA":
                self._stage(L("Installing NVIDIA drivers…", "Instalando drivers NVIDIA…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "nvidia nvidia-utils nvidia-settings",
                    71, 77, ignore_error=True)
            elif state["gpu"] == "AMD/Intel":
                self._stage(L("Installing AMD/Intel drivers…", "Instalando drivers AMD/Intel…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "mesa vulkan-radeon libva-mesa-driver",
                    71, 77, ignore_error=True)
            else:
                self._pct(77)

            if state["desktop"] == "KDE Plasma":
                self._stage(L("Installing KDE Plasma…", "Instalando KDE Plasma…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm",
                    83, 93, ignore_error=True)
                self._chroot("systemctl enable sddm")
            elif state["desktop"] == "Cinnamon":
                self._stage(L("Installing Cinnamon…", "Instalando Cinnamon…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
                    77, 83, ignore_error=True)
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "cinnamon lightdm lightdm-gtk-greeter alacritty firefox",
                    83, 93, ignore_error=True)
                self._chroot("systemctl enable lightdm")
            else:
                self._pct(93)

            self._stage(L("Installing GRUB bootloader…", "Instalando GRUB…"))
            self._chroot("grub-install --target=x86_64-efi "
                         "--efi-directory=/boot/efi --bootloader-id=GRUB")
            self._chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self._pct(100)
            self._stage(L("✔  Installation complete!", "✔  ¡Instalación completa!"))
            self.on_done(True)

        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False)

class App(tk.Tk):
    STEPS = [
        "Language", "Identity", "Passwords",
        "Disk", "Keymap", "Timezone",
        "Desktop", "GPU", "Review", "Install"
    ]

    def __init__(self):
        super().__init__()
        self.title("Arch Linux Installer")
        self.configure(bg=BG)
        self.minsize(820, 580)
        self.geometry("960x640")
        self.resizable(True, True)

        self.update_idletasks()
        x = (self.winfo_screenwidth()  - 960) // 2
        y = (self.winfo_screenheight() - 640) // 2
        self.geometry(f"960x640+{x}+{y}")

        self._step_idx = 0
        self._frames   = {}
        self._build_layout()
        self._show_step(0)

    def _build_layout(self):
        hdr = tk.Frame(self, bg=BG2, height=52)
        hdr.pack(fill="x")
        hdr.pack_propagate(False)
        tk.Label(hdr, text="❱ Arch Linux Installer",
                 bg=BG2, fg=ACCENT, font=("Inter", 15, "bold")).pack(side="left", padx=16, pady=12)
        tk.Label(hdr, text=VERSION,
                 bg=BG2, fg=FG_DIM, font=("Inter", 9)).pack(side="right", padx=16)
        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")

        body = tk.Frame(self, bg=BG)
        body.pack(fill="both", expand=True)

        self._sidebar = tk.Frame(body, bg=BG2, width=200)
        self._sidebar.pack(side="left", fill="y")
        self._sidebar.pack_propagate(False)
        tk.Frame(body, bg=BORDER, width=1).pack(side="left", fill="y")

        self._step_labels = []
        tk.Label(self._sidebar, text="Steps", bg=BG2, fg=FG_DIM,
                 font=("Inter", 9, "bold")).pack(anchor="w", padx=16, pady=(16, 4))
        for i, name in enumerate(self.STEPS):
            lbl = tk.Label(self._sidebar, text=f"  {i+1:02d}  {name}",
                           bg=BG2, fg=FG_DIM, font=("Inter", 10),
                           anchor="w", padx=4)
            lbl.pack(fill="x", padx=8, pady=1)
            self._step_labels.append(lbl)

        self._content = tk.Frame(body, bg=BG)
        self._content.pack(side="left", fill="both", expand=True)

        tk.Frame(self, bg=BORDER, height=1).pack(fill="x")
        ftr = tk.Frame(self, bg=BG2, height=56)
        ftr.pack(fill="x")
        ftr.pack_propagate(False)

        self._btn_back = tk.Button(ftr, text="← Back", command=self._go_back, **BTN_GHOST)
        self._btn_back.pack(side="left", padx=12, pady=10)

        self._btn_next = tk.Button(ftr, text="Next →", command=self._go_next, **BTN_STYLE)
        self._btn_next.pack(side="right", padx=12, pady=10)

        self._footer_msg = tk.Label(ftr, text="", bg=BG2, fg=FG_DIM, font=("Inter", 9))
        self._footer_msg.pack(side="left", padx=8)

    def _show_step(self, idx):
        self._step_idx = idx
        self._update_sidebar()

        for w in self._content.winfo_children():
            w.destroy()

        builders = [
            self._screen_language,
            self._screen_identity,
            self._screen_passwords,
            self._screen_disk,
            self._screen_keymap,
            self._screen_timezone,
            self._screen_desktop,
            self._screen_gpu,
            self._screen_review,
            self._screen_install,
        ]
        builders[idx]()

        is_install = (idx == len(self.STEPS) - 1)
        self._btn_next.pack_forget() if is_install else self._btn_next.pack(side="right", padx=12, pady=10)
        self._btn_back.pack_forget() if is_install else self._btn_back.pack(side="left", padx=12, pady=10)
        self._btn_back.config(state="disabled" if idx == 0 else "normal")

    def _update_sidebar(self):
        for i, lbl in enumerate(self._step_labels):
            if i < self._step_idx:
                lbl.config(fg=GREEN, font=("Inter", 10))
                lbl.config(text=f"  ✔   {self.STEPS[i]}")
            elif i == self._step_idx:
                lbl.config(fg=FG, font=("Inter", 10, "bold"),
                           text=f"  ▶   {self.STEPS[i]}")
            else:
                lbl.config(fg=FG_DIM, font=("Inter", 10),
                           text=f"  {i+1:02d}  {self.STEPS[i]}")

    def _go_next(self):
        validators = [
            self._validate_language,
            self._validate_identity,
            self._validate_passwords,
            self._validate_disk,
            lambda: True,   # keymap
            lambda: True,   # timezone
            lambda: True,   # desktop
            lambda: True,   # gpu
            self._validate_review,
            lambda: True,
        ]
        if validators[self._step_idx]():
            if self._step_idx < len(self.STEPS) - 1:
                self._show_step(self._step_idx + 1)

    def _go_back(self):
        if self._step_idx > 0:
            self._show_step(self._step_idx - 1)

    def _set_footer(self, msg, color=FG_DIM):
        self._footer_msg.config(text=msg, fg=color)

    def _page(self, title, subtitle=""):
        wrap = tk.Frame(self._content, bg=BG)
        wrap.pack(fill="both", expand=True, padx=32, pady=24)
        tk.Label(wrap, text=title, bg=BG, fg=FG,
                 font=("Inter", 18, "bold"), anchor="w").pack(fill="x")
        if subtitle:
            tk.Label(wrap, text=subtitle, bg=BG, fg=FG_DIM,
                     font=("Inter", 10), anchor="w").pack(fill="x", pady=(2, 0))
        tk.Frame(wrap, bg=BORDER, height=1).pack(fill="x", pady=12)
        return wrap

    def _labeled_entry(self, parent, label_text, initial="", show=""):
        row = tk.Frame(parent, bg=BG)
        row.pack(fill="x", pady=6)
        tk.Label(row, text=label_text, bg=BG, fg=FG_DIM,
                 font=("Inter", 10), width=22, anchor="w").pack(side="left")
        e = make_entry(row, show=show)
        e.pack(side="left", fill="x", expand=True, ipady=6, padx=(0, 4))
        if initial:
            e.insert(0, initial)
        return e

    def _error_label(self, parent):
        lbl = tk.Label(parent, text="", bg=BG, fg=RED, font=("Inter", 10), anchor="w")
        lbl.pack(fill="x", pady=(0, 4))
        return lbl

    def _screen_language(self):
        p = self._page("Language / Idioma",
                       "Choose the installer language.")
        self._lang_var = tk.StringVar(value=state["lang"])

        for code, name, flag in [("en", "English", "🇬🇧"), ("es", "Español", "🇪🇸")]:
            f = tk.Frame(p, bg=BG3, cursor="hand2",
                         highlightthickness=1, highlightbackground=BORDER)
            f.pack(fill="x", pady=5)
            def select(c=code):
                self._lang_var.set(c)
                state["lang"] = c
                self._update_all_texts()
            rb = tk.Radiobutton(f, variable=self._lang_var, value=code,
                                bg=BG3, activebackground=BG3, selectcolor=ACCENT,
                                command=select)
            rb.pack(side="left", padx=12, pady=12)
            tk.Label(f, text=f"{flag}  {name}", bg=BG3, fg=FG,
                     font=("Inter", 13), cursor="hand2").pack(side="left", pady=12)
            f.bind("<Button-1>", lambda _e, c=code: select(c))

    def _update_all_texts(self):
        pass   # stateless redraw on next() if needed

    def _validate_language(self):
        state["lang"] = self._lang_var.get()
        return True

    def _screen_identity(self):
        p = self._page(
            L("System Identity", "Identidad del sistema"),
            L("Letters, digits, - _ · max 32 chars",
              "Letras, dígitos, - _ · máx 32 caracteres")
        )
        self._err_identity = self._error_label(p)
        self._e_hostname   = self._labeled_entry(p, L("Hostname:", "Nombre de equipo:"), state.get("hostname", ""))
        self._e_username   = self._labeled_entry(p, L("Username:", "Usuario:"),          state.get("username", ""))

    def _validate_identity(self):
        hn = self._e_hostname.get().strip()
        un = self._e_username.get().strip()
        if not validate_name(hn):
            self._err_identity.config(text=L("✗  Invalid hostname (a-z 0-9 - _  1-32 chars).",
                                             "✗  Hostname inválido (a-z 0-9 - _  1-32 chars)."))
            return False
        if not validate_name(un):
            self._err_identity.config(text=L("✗  Invalid username (a-z 0-9 - _  1-32 chars).",
                                             "✗  Usuario inválido (a-z 0-9 - _  1-32 chars)."))
            return False
        state["hostname"] = hn
        state["username"] = un
        self._err_identity.config(text="")
        return True

    def _screen_passwords(self):
        p = self._page(L("Passwords", "Contraseñas"))
        self._err_pass = self._error_label(p)

        tk.Label(p, text=L("Root account", "Cuenta root"), bg=BG, fg=ACCENT,
                 font=("Inter", 11, "bold"), anchor="w").pack(fill="x", pady=(4, 0))
        self._e_rp1 = self._labeled_entry(p, L("Root password:", "Contraseña root:"), show="•")
        self._e_rp2 = self._labeled_entry(p, L("Confirm root:", "Confirmar root:"),   show="•")

        tk.Frame(p, bg=BORDER, height=1).pack(fill="x", pady=8)
        tk.Label(p, text=L("User account", "Cuenta de usuario"), bg=BG, fg=ACCENT,
                 font=("Inter", 11, "bold"), anchor="w").pack(fill="x", pady=(0, 0))
        self._e_up1 = self._labeled_entry(p, L("User password:", "Contraseña usuario:"), show="•")
        self._e_up2 = self._labeled_entry(p, L("Confirm user:", "Confirmar usuario:"),   show="•")

    def _validate_passwords(self):
        rp1 = self._e_rp1.get()
        rp2 = self._e_rp2.get()
        up1 = self._e_up1.get()
        up2 = self._e_up2.get()
        if not rp1:
            self._err_pass.config(text=L("✗  Root password is empty.", "✗  Contraseña root vacía."))
            return False
        if rp1 != rp2:
            self._err_pass.config(text=L("✗  Root passwords do not match.", "✗  Contraseñas root no coinciden."))
            return False
        if not up1:
            self._err_pass.config(text=L("✗  User password is empty.", "✗  Contraseña usuario vacía."))
            return False
        if up1 != up2:
            self._err_pass.config(text=L("✗  User passwords do not match.", "✗  Contraseñas usuario no coinciden."))
            return False
        state["root_pass"] = rp1
        state["user_pass"] = up1
        self._err_pass.config(text="")
        return True

    def _screen_disk(self):
        p = self._page(
            L("Disk & Swap", "Disco y Swap"),
            L("⚠  ALL DATA ON THE SELECTED DISK WILL BE ERASED",
              "⚠  SE BORRARÁN TODOS LOS DATOS DEL DISCO SELECCIONADO")
        )
        self._err_disk = self._error_label(p)

        disks = list_disks()
        if not disks:
            tk.Label(p, text=L("✗  No disks found.", "✗  No se encontraron discos."),
                     bg=BG, fg=RED, font=("Inter", 12, "bold")).pack(pady=20)
            self._btn_next.config(state="disabled")
            return

        self._disk_var = tk.StringVar(value=state.get("disk") or disks[0][0])
        cols_hdr = tk.Frame(p, bg=BG3)
        cols_hdr.pack(fill="x")
        for txt, w in [("Device", 14), ("Size", 8), ("Model", 0)]:
            tk.Label(cols_hdr, text=txt, bg=BG3, fg=ACCENT,
                     font=("Inter", 9, "bold"), width=w, anchor="w").pack(side="left", padx=8, pady=4)

        tk.Frame(p, bg=BORDER, height=1).pack(fill="x")

        for name, size_gb, model in disks:
            f = tk.Frame(p, bg=BG3, cursor="hand2")
            f.pack(fill="x", pady=2)
            def select(n=name):
                self._disk_var.set(n)
                state["disk"] = n
                for child in p.winfo_children():
                    if isinstance(child, tk.Frame):
                        for sub in child.winfo_children():
                            if isinstance(sub, tk.Radiobutton):
                                sub.config(bg=BG3)
            rb = tk.Radiobutton(f, variable=self._disk_var, value=name,
                                bg=BG3, activebackground=BG3, selectcolor=ACCENT,
                                command=select)
            rb.pack(side="left", padx=8, pady=8)
            tk.Label(f, text=f"/dev/{name}", bg=BG3, fg=FG,
                     font=("Mono", 10), width=14, anchor="w").pack(side="left")
            tk.Label(f, text=f"{size_gb} GB", bg=BG3, fg=YELLOW,
                     font=("Mono", 10), width=8, anchor="w").pack(side="left")
            tk.Label(f, text=model or "—", bg=BG3, fg=FG_DIM,
                     font=("Mono", 10), anchor="w").pack(side="left", padx=4)
            f.bind("<Button-1>", lambda _e, n=name: select(n))

        tk.Frame(p, bg=BORDER, height=1).pack(fill="x", pady=8)

        swap_row = tk.Frame(p, bg=BG)
        swap_row.pack(fill="x", pady=4)
        tk.Label(swap_row, text=L("Swap size (GB):", "Tamaño swap (GB):"),
                 bg=BG, fg=FG_DIM, font=("Inter", 10), width=22, anchor="w").pack(side="left")
        self._swap_var = tk.StringVar(value=state["swap"])
        swap_spin = tk.Spinbox(swap_row, from_=1, to=128, textvariable=self._swap_var,
                               width=6, bg=BG3, fg=FG, font=("Inter", 11),
                               relief="flat", buttonbackground=BG3,
                               highlightthickness=1, highlightbackground=BORDER)
        swap_spin.pack(side="left", ipady=4)

    def _validate_disk(self):
        if not hasattr(self, "_disk_var"):
            self._set_footer(L("No disks found.", "Sin discos."), RED)
            return False
        state["disk"] = self._disk_var.get()
        swap = self._swap_var.get().strip()
        if not validate_swap(swap):
            self._err_disk.config(text=L("✗  Swap must be 1-128 GB.", "✗  Swap debe ser 1-128 GB."))
            return False
        state["swap"] = swap
        if not state["disk"]:
            self._err_disk.config(text=L("✗  Select a disk.", "✗  Selecciona un disco."))
            return False
        return True

    def _screen_keymap(self):
        p = self._page(L("Keyboard Layout", "Distribución de teclado"))
        try:
            out  = subprocess.check_output(
                "localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
            maps = [l for l in out.splitlines() if l]
        except Exception:
            maps = []
        wanted  = ["us", "es", "uk", "fr", "de", "ru", "ara", "it", "pt-latin9"]
        options = [m for m in wanted if m in maps] if maps else wanted

        self._keymap_var = tk.StringVar(value=state["keymap"])

        cols = 3
        grid = tk.Frame(p, bg=BG)
        grid.pack(fill="both", expand=True)
        for i, km in enumerate(options):
            f = tk.Frame(grid, bg=BG3, cursor="hand2",
                         highlightthickness=1, highlightbackground=BORDER)
            f.grid(row=i//cols, column=i%cols, padx=6, pady=6, sticky="ew")
            grid.columnconfigure(i%cols, weight=1)
            def select(k=km):
                self._keymap_var.set(k)
                state["keymap"] = k
                run_simple(f"loadkeys {shlex.quote(k)}", ignore_error=True)
            rb = tk.Radiobutton(f, variable=self._keymap_var, value=km,
                                bg=BG3, activebackground=BG3, selectcolor=ACCENT,
                                command=select)
            rb.pack(side="left", padx=8, pady=10)
            tk.Label(f, text=km, bg=BG3, fg=FG,
                     font=("Mono", 11), cursor="hand2").pack(side="left", pady=10)
            f.bind("<Button-1>", lambda _e, k=km: select(k))

    def _screen_timezone(self):
        p = self._page(L("Timezone", "Zona horaria"))
        try:
            out   = subprocess.check_output(
                "timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
            zones = [l for l in out.splitlines() if l]
        except Exception:
            zones = []
        if not zones:
            zones = ["UTC", "Europe/Madrid", "Europe/London",
                     "America/New_York", "America/Los_Angeles", "Asia/Tokyo"]

        regions = sorted(set(z.split("/")[0] for z in zones if "/" in z))
        regions = ["UTC"] + regions

        self._tz_var    = tk.StringVar(value=state["timezone"])
        self._tz_region = tk.StringVar(value=state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC")

        search_row = tk.Frame(p, bg=BG)
        search_row.pack(fill="x", pady=(0, 8))
        tk.Label(search_row, text=L("Search:", "Buscar:"), bg=BG, fg=FG_DIM,
                 font=("Inter", 10)).pack(side="left")
        search_var = tk.StringVar()
        se = make_entry(search_row)
        se.pack(side="left", fill="x", expand=True, padx=8, ipady=5)
        se.config(textvariable=search_var)

        panes = tk.Frame(p, bg=BG)
        panes.pack(fill="both", expand=True)

        lf = tk.Frame(panes, bg=BG2, width=180)
        lf.pack(side="left", fill="y", padx=(0, 8))
        lf.pack_propagate(False)
        tk.Label(lf, text=L("Region", "Región"), bg=BG2, fg=ACCENT,
                 font=("Inter", 9, "bold")).pack(anchor="w", padx=8, pady=(6, 2))
        rl = tk.Listbox(lf, bg=BG3, fg=FG, selectbackground=ACCENT,
                        selectforeground="white", font=("Inter", 10),
                        relief="flat", borderwidth=0, activestyle="none",
                        highlightthickness=0)
        rl.pack(fill="both", expand=True, padx=4, pady=4)

        rf = tk.Frame(panes, bg=BG2)
        rf.pack(side="left", fill="both", expand=True)
        tk.Label(rf, text=L("City / Zone", "Ciudad / Zona"), bg=BG2, fg=ACCENT,
                 font=("Inter", 9, "bold")).pack(anchor="w", padx=8, pady=(6, 2))
        cl = tk.Listbox(rf, bg=BG3, fg=FG, selectbackground=ACCENT,
                        selectforeground="white", font=("Inter", 10),
                        relief="flat", borderwidth=0, activestyle="none",
                        highlightthickness=0)
        cl.pack(fill="both", expand=True, padx=4, pady=4)

        selected_lbl = tk.Label(p, text=f"Selected: {state['timezone']}",
                                bg=BG, fg=GREEN, font=("Inter", 10), anchor="w")
        selected_lbl.pack(fill="x", pady=(6, 0))

        def fill_regions(filter_text=""):
            rl.delete(0, tk.END)
            for r in regions:
                if filter_text.lower() in r.lower():
                    rl.insert(tk.END, r)

        def fill_cities(region, filter_text=""):
            cl.delete(0, tk.END)
            if region == "UTC":
                cl.insert(tk.END, "UTC")
                return
            for z in zones:
                if z.startswith(region + "/") or z == region:
                    city = z.split("/", 1)[1] if "/" in z else z
                    if filter_text.lower() in city.lower():
                        cl.insert(tk.END, city)

        def on_region_select(_e=None):
            sel = rl.curselection()
            if not sel:
                return
            region = rl.get(sel[0])
            fill_cities(region)

        def on_city_select(_e=None):
            sel_r = rl.curselection()
            sel_c = cl.curselection()
            if not sel_c:
                return
            city   = cl.get(sel_c[0])
            if sel_r:
                region = rl.get(sel_r[0])
                tz = "UTC" if region == "UTC" else f"{region}/{city}"
            else:
                tz = city
            state["timezone"] = tz
            self._tz_var.set(tz)
            selected_lbl.config(text=f"Selected: {tz}")

        def on_search(*_):
            q = search_var.get()
            fill_regions(q)
            if q:
                cl.delete(0, tk.END)
                for z in zones:
                    if q.lower() in z.lower():
                        cl.insert(tk.END, z)

        rl.bind("<<ListboxSelect>>", on_region_select)
        cl.bind("<<ListboxSelect>>", on_city_select)
        search_var.trace_add("write", on_search)
        fill_regions()
        cur_region = state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC"
        for i in range(rl.size()):
            if rl.get(i) == cur_region:
                rl.selection_set(i)
                rl.see(i)
                fill_cities(cur_region)
                break

    def _screen_desktop(self):
        p = self._page(
            L("Desktop Environment", "Entorno de escritorio"),
            L("Choose a desktop or install headless.", "Elige un escritorio o instala sin entorno gráfico.")
        )
        self._de_var = tk.StringVar(value=state["desktop"])
        options = [
            ("KDE Plasma",  L("Full-featured KDE desktop + Firefox", "KDE completo + Firefox")),
            ("Cinnamon",    L("Classic Cinnamon desktop + Firefox",  "Escritorio Cinnamon clásico + Firefox")),
            ("None",        L("No desktop — CLI only",               "Sin escritorio — solo terminal")),
        ]
        for val, desc in options:
            f = tk.Frame(p, bg=BG3, cursor="hand2",
                         highlightthickness=1, highlightbackground=BORDER)
            f.pack(fill="x", pady=5)
            def select(v=val):
                self._de_var.set(v)
                state["desktop"] = v
            rb = tk.Radiobutton(f, variable=self._de_var, value=val,
                                bg=BG3, activebackground=BG3, selectcolor=ACCENT,
                                command=select)
            rb.pack(side="left", padx=12, pady=12)
            col = tk.Frame(f, bg=BG3)
            col.pack(side="left", pady=12)
            tk.Label(col, text=val, bg=BG3, fg=FG,
                     font=("Inter", 12, "bold"), anchor="w").pack(anchor="w")
            tk.Label(col, text=desc, bg=BG3, fg=FG_DIM,
                     font=("Inter", 9), anchor="w").pack(anchor="w")
            f.bind("<Button-1>", lambda _e, v=val: select(v))

    def _screen_gpu(self):
        p = self._page(
            L("GPU Drivers", "Drivers GPU"),
            L("Select your graphics card vendor.", "Selecciona el fabricante de tu GPU.")
        )
        self._gpu_var = tk.StringVar(value=state["gpu"])
        options = [
            ("NVIDIA",    L("NVIDIA proprietary drivers", "Drivers propietarios NVIDIA")),
            ("AMD/Intel", L("Mesa open-source drivers",   "Drivers open-source Mesa")),
            ("None",      L("No extra GPU drivers",       "Sin drivers adicionales")),
        ]
        for val, desc in options:
            f = tk.Frame(p, bg=BG3, cursor="hand2",
                         highlightthickness=1, highlightbackground=BORDER)
            f.pack(fill="x", pady=5)
            def select(v=val):
                self._gpu_var.set(v)
                state["gpu"] = v
            rb = tk.Radiobutton(f, variable=self._gpu_var, value=val,
                                bg=BG3, activebackground=BG3, selectcolor=ACCENT,
                                command=select)
            rb.pack(side="left", padx=12, pady=12)
            col = tk.Frame(f, bg=BG3)
            col.pack(side="left", pady=12)
            tk.Label(col, text=val, bg=BG3, fg=FG,
                     font=("Inter", 12, "bold"), anchor="w").pack(anchor="w")
            tk.Label(col, text=desc, bg=BG3, fg=FG_DIM,
                     font=("Inter", 9), anchor="w").pack(anchor="w")
            f.bind("<Button-1>", lambda _e, v=val: select(v))

    def _screen_review(self):
        p = self._page(
            L("Review & Confirm", "Revisar y confirmar"),
            L("Check your settings before installing.", "Revisa la configuración antes de instalar.")
        )

        rows = [
            (L("Language", "Idioma"),   state["lang"],                              "lang"),
            ("Hostname",                state["hostname"] or "—",                   "hostname"),
            (L("Username", "Usuario"),  state["username"] or "—",                   "username"),
            ("Disk",                    (f"/dev/{state['disk']}" if state["disk"]
                                         else L("NOT SET", "SIN ASIGNAR")),         "disk"),
            ("Swap",                    f"{state['swap']} GB",                      "swap"),
            ("Keymap",                  state["keymap"],                            "keymap"),
            ("Timezone",                state["timezone"],                           "timezone"),
            ("Desktop",                 state["desktop"],                           "desktop"),
            ("GPU",                     state["gpu"],                               "gpu"),
        ]

        DEFAULTS = {"swap": "8", "desktop": "None", "gpu": "None",
                    "keymap": "us", "timezone": "UTC"}

        tbl = tk.Frame(p, bg=BG2, highlightthickness=1, highlightbackground=BORDER)
        tbl.pack(fill="x")

        for i, (label_text, val, key) in enumerate(rows):
            raw = state.get(key)
            if not raw:
                color = RED
            elif DEFAULTS.get(key) == raw:
                color = YELLOW
            else:
                color = GREEN

            row_bg = BG3 if i % 2 == 0 else BG2
            row = tk.Frame(tbl, bg=row_bg)
            row.pack(fill="x")
            tk.Label(row, text=label_text, bg=row_bg, fg=FG_DIM,
                     font=("Inter", 10), width=18, anchor="w").pack(side="left", padx=12, pady=7)
            tk.Label(row, text=val, bg=row_bg, fg=color,
                     font=("Mono", 10, "bold"), anchor="w").pack(side="left", padx=4)

        tk.Label(p,
                 text=L("Yellow = default  ·  Green = customized  ·  Red = missing",
                        "Amarillo = default  ·  Verde = personaliz.  ·  Rojo = falta"),
                 bg=BG, fg=FG_DIM, font=("Inter", 9)).pack(anchor="w", pady=(8, 4))

        missing = []
        if not state["hostname"]:  missing.append("hostname")
        if not state["username"]:  missing.append("username")
        if not state["disk"]:      missing.append("disk")
        if not state["root_pass"]: missing.append(L("root password", "contraseña root"))

        self._review_ok = not missing
        if missing:
            tk.Label(p, text=L(f"✗  Missing: {', '.join(missing)}",
                               f"✗  Faltan: {', '.join(missing)}"),
                     bg=BG, fg=RED, font=("Inter", 11, "bold"), anchor="w").pack(fill="x", pady=4)
        else:
            tk.Label(p, text=L("✔  All good — ready to install!",
                               "✔  Todo listo — ¡listo para instalar!"),
                     bg=BG, fg=GREEN, font=("Inter", 11, "bold"), anchor="w").pack(fill="x", pady=4)

        warn = tk.Frame(p, bg="#3b1f1f", highlightthickness=1, highlightbackground="#7f1d1d")
        warn.pack(fill="x", pady=8)
        tk.Label(warn,
                 text=L("⚠  THIS WILL PERMANENTLY ERASE  /dev/" + (state["disk"] or "???"),
                        "⚠  ESTO BORRARÁ PERMANENTEMENTE  /dev/" + (state["disk"] or "???")),
                 bg="#3b1f1f", fg="#fca5a5", font=("Inter", 11, "bold")).pack(pady=10)

        self._btn_next.config(text=L("Install Now →", "Instalar ahora →"), **BTN_DANGER)

    def _validate_review(self):
        if not self._review_ok:
            messagebox.showerror(
                L("Missing settings", "Configuración incompleta"),
                L("Please go back and complete all required fields.",
                  "Por favor, vuelve y completa todos los campos requeridos.")
            )
            return False
        ok = messagebox.askyesno(
            L("Confirm installation", "Confirmar instalación"),
            L(f"LAST WARNING:\n\nAll data on /dev/{state['disk']} will be ERASED.\n\nContinue?",
              f"ÚLTIMO AVISO:\n\nTodos los datos en /dev/{state['disk']} serán BORRADOS.\n\n¿Continuar?")
        )
        return ok

    def _screen_install(self):
        self._btn_next.config(text="Next →", **BTN_STYLE)   # reset label

        p = tk.Frame(self._content, bg=BG)
        p.pack(fill="both", expand=True)

        top = tk.Frame(p, bg=BG2, height=160)
        top.pack(fill="x")
        top.pack_propagate(False)

        self._stage_lbl = tk.Label(top,
            text=L("Preparing…", "Preparando…"),
            bg=BG2, fg=FG, font=("Inter", 12, "bold"), anchor="w")
        self._stage_lbl.pack(anchor="w", padx=20, pady=(18, 4))

        pct_row = tk.Frame(top, bg=BG2)
        pct_row.pack(fill="x", padx=20)
        self._pct_lbl = tk.Label(pct_row, text="0%", bg=BG2, fg=ACCENT,
                                  font=("Inter", 11, "bold"), width=5, anchor="w")
        self._pct_lbl.pack(side="left")

        bar_bg = tk.Frame(pct_row, bg=BORDER, height=14)
        bar_bg.pack(side="left", fill="x", expand=True, padx=8, ipady=0)
        self._bar_fill = tk.Frame(bar_bg, bg=ACCENT, height=14)
        self._bar_fill.place(x=0, y=0, relheight=1, relwidth=0)

        self._elapsed_lbl = tk.Label(top, text="00:00", bg=BG2, fg=FG_DIM,
                                      font=("Inter", 10), anchor="w")
        self._elapsed_lbl.pack(anchor="w", padx=20, pady=(6, 0))

        tk.Frame(p, bg=BORDER, height=1).pack(fill="x")

        log_hdr = tk.Frame(p, bg=BG)
        log_hdr.pack(fill="x", padx=16, pady=(8, 2))
        tk.Label(log_hdr, text=L("Live log", "Log en vivo"),
                 bg=BG, fg=ACCENT, font=("Inter", 10, "bold")).pack(side="left")

        log_frame = tk.Frame(p, bg=BG3)
        log_frame.pack(fill="both", expand=True, padx=16, pady=(0, 12))

        self._log_text = tk.Text(
            log_frame, bg=BG3, fg=FG_DIM, font=("Mono", 9),
            relief="flat", wrap="word", state="disabled",
            highlightthickness=0, padx=8, pady=6
        )
        scroll = tk.Scrollbar(log_frame, command=self._log_text.yview,
                              bg=BG3, troughcolor=BG3, relief="flat")
        self._log_text.config(yscrollcommand=scroll.set)
        scroll.pack(side="right", fill="y")
        self._log_text.pack(fill="both", expand=True)

        self._log_text.tag_config("err",  foreground=RED)
        self._log_text.tag_config("warn", foreground=YELLOW)
        self._log_text.tag_config("ok",   foreground=GREEN)
        self._log_text.tag_config("dim",  foreground=FG_DIM)

        self._install_start = time.time()
        self._install_done  = False

        self._tick_elapsed()

        backend = InstallBackend(
            on_log      = self._install_log,
            on_progress = self._install_progress,
            on_stage    = self._install_stage,
            on_done     = self._install_done,
        )
        t = threading.Thread(target=backend.run, daemon=True)
        t.start()

    def _tick_elapsed(self):
        if self._install_done:
            return
        secs = int(time.time() - self._install_start)
        self._elapsed_lbl.config(text=f"Elapsed: {secs//60:02d}:{secs%60:02d}")
        self.after(1000, self._tick_elapsed)

    def _install_log(self, line):
        def _update():
            self._log_text.config(state="normal")
            ll = line.lower()
            tag = "dim"
            if any(w in ll for w in ("error", "fail", "fatal", "✗")):
                tag = "err"
            elif any(w in ll for w in ("warning", "warn")):
                tag = "warn"
            elif any(w in ll for w in ("✔", "complete", "success")):
                tag = "ok"
            self._log_text.insert(tk.END, line + "\n", tag)
            self._log_text.see(tk.END)
            self._log_text.config(state="disabled")
        self.after(0, _update)

    def _install_progress(self, pct):
        def _update():
            self._pct_lbl.config(text=f"{int(pct)}%")
            self._bar_fill.place(relwidth=pct / 100.0)
            if pct >= 100:
                self._bar_fill.config(bg=GREEN)
        self.after(0, _update)

    def _install_stage(self, msg):
        def _update():
            self._stage_lbl.config(text=msg)
        self.after(0, _update)

    def _install_done(self, success):
        """BUG FIX: no infinite loop, clean path for both success and failure."""
        self._install_done = True

        def _show():
            self._tick_elapsed.__self__  # keep alive but done flag set

            overlay = tk.Frame(self._content, bg=BG)
            overlay.place(relx=0, rely=0, relwidth=1, relheight=1)

            if success:
                tk.Label(overlay,
                         text="✔",
                         bg=BG, fg=GREEN, font=("Inter", 60)).pack(pady=(60, 8))
                tk.Label(overlay,
                         text=L("Installation complete!", "¡Instalación completa!"),
                         bg=BG, fg=GREEN, font=("Inter", 18, "bold")).pack()
                tk.Label(overlay,
                         text=L("Remove the install media and reboot.",
                                "Extrae el medio de instalación y reinicia."),
                         bg=BG, fg=FG_DIM, font=("Inter", 11)).pack(pady=(4, 24))
                btn_row = tk.Frame(overlay, bg=BG)
                btn_row.pack()
                tk.Button(btn_row,
                          text=L("Reboot now", "Reiniciar ahora"),
                          command=self._do_reboot, **BTN_STYLE).pack(side="left", padx=8)
                tk.Button(btn_row,
                          text=L("Stay in shell", "Quedarme en shell"),
                          command=lambda: sys.exit(0), **BTN_GHOST).pack(side="left", padx=8)
            else:
                tk.Label(overlay,
                         text="✗",
                         bg=BG, fg=RED, font=("Inter", 60)).pack(pady=(60, 8))
                tk.Label(overlay,
                         text=L("Installation failed.", "Instalación fallida."),
                         bg=BG, fg=RED, font=("Inter", 18, "bold")).pack()
                tk.Label(overlay,
                         text=L("Check /mnt/install_log.txt for details.",
                                "Revisa /mnt/install_log.txt para más detalles."),
                         bg=BG, fg=FG_DIM, font=("Inter", 11)).pack(pady=(4, 24))
                tk.Button(overlay,
                          text=L("Exit to shell", "Salir al shell"),
                          command=lambda: sys.exit(1), **BTN_GHOST).pack()

        self.after(0, _show)

    def _do_reboot(self):
        subprocess.run("umount -R /mnt", shell=True)
        subprocess.run("reboot",         shell=True)
        sys.exit(0)


def main():
    if os.geteuid() != 0:
        print("Run as root in the Arch ISO live environment.")
        sys.exit(1)

    app = App()
    try:
        app.tk.call("tk", "scaling", 1.3)
    except Exception:
        pass
    app.mainloop()


if __name__ == "__main__":
    main()
