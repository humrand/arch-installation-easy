"""
Arch Linux Installer — GTK3 GUI Edition
MIT LICENSE — credits to humrand https://github.com/humrand/arch-anstallation-easy
DO NOT REMOVE THIS FROM YOUR CODE IF YOU USE IT TO MODIFY IT.
"""

import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

BOOTSTRAP_PKGS = "xorg-server xorg-xinit python-gobject gtk3 xf86-video-vesa xf86-input-libinput"
VERSION        = "V1.2.0-gtk"
LOG_FILE       = "/mnt/install_log.txt"

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

def nowtag():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(f"[{nowtag()}] {line}\n")
    except Exception:
        pass

def run_stream(cmd, on_line=None, ignore_error=False):
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
        model = parts[2].strip() if len(parts) > 2 else "Unknown"
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

_PAT_INSTALL  = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)


class InstallBackend:
    def __init__(self, on_progress, on_stage, on_log, on_done):
        self.on_progress = on_progress
        self.on_stage    = on_stage
        self.on_log      = on_log
        self.on_done     = on_done
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
        return run_stream(
            f"arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}",
            on_line=self._log, ignore_error=ignore_error
        )

    def _chroot_passwd(self, user, pwd):
        return run_stream(
            f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} | arch-chroot /mnt chpasswd",
            on_line=self._log, ignore_error=True
        )

    def run(self):
        disk_path  = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)
        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.on_done(False, L("No network connection.", "Sin conexion de red."))
                return
            run_stream("pacman -Sy --noconfirm archlinux-keyring", on_line=self._log, ignore_error=True)

            self._stage(L("Wiping disk…", "Borrando disco…"))
            self._gradual(3)
            run_stream(f"sgdisk -Z {disk_path}", on_line=self._log)
            self._pct(5)

            self._stage(L("Creating partitions…", "Creando particiones…"))
            run_stream(f"sgdisk -n1:0:+1G -t1:ef00 {disk_path}", on_line=self._log)
            run_stream(f"sgdisk -n2:0:+{state['swap']}G -t2:8200 {disk_path}", on_line=self._log)
            run_stream(f"sgdisk -n3:0:0 -t3:8300 {disk_path}", on_line=self._log)
            self._pct(10)

            self._stage(L("Formatting…", "Formateando…"))
            run_stream(f"mkfs.fat -F32 {p1}", on_line=self._log)
            run_stream(f"mkswap {p2}", on_line=self._log)
            run_stream(f"swapon {p2}", on_line=self._log)
            run_stream(f"mkfs.ext4 -F {p3}", on_line=self._log)
            self._pct(15)

            self._stage(L("Mounting filesystems…", "Montando sistemas de archivos…"))
            run_stream(f"mount {p3} /mnt", on_line=self._log)
            run_stream("mkdir -p /mnt/boot/efi", on_line=self._log)
            run_stream(f"mount {p1} /mnt/boot/efi", on_line=self._log)
            self._pct(18)

            self._stage(L("Installing base system — this may take a while…", "Instalando sistema base — esto puede tardar…"))
            pkgs = ("base linux linux-firmware linux-headers sof-firmware "
                    "base-devel grub efibootmgr vim nano networkmanager sudo bash-completion")
            rc = self._pacman(f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self.on_done(False, L("pacstrap failed. Check " + LOG_FILE, "pacstrap fallo. Revisa " + LOG_FILE))
                return

            self._stage(L("Generating fstab…", "Generando fstab…"))
            run_stream("genfstab -U /mnt >> /mnt/etc/fstab", on_line=self._log)
            self._pct(53)

            self._stage(L("Configuring hostname…", "Configurando hostname…"))
            hn = state["hostname"]
            with open("/mnt/etc/hostname", "w") as f:
                f.write(hn + "\n")
            with open("/mnt/etc/hosts", "w") as f:
                f.write(f"127.0.0.1\tlocalhost\n::1\t\tlocalhost\n127.0.1.1\t{hn}.localdomain\t{hn}\n")
            self._pct(55)

            self._stage(L("Configuring locale & timezone…", "Configurando locale y zona horaria…"))
            locale = "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
            locale_line = f"{locale} UTF-8"
            self._chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            if locale != "en_US.UTF-8":
                self._chroot(f"sed -i 's/^#{locale_line}/{locale_line}/' /etc/locale.gen", ignore_error=True)
            self._chroot("locale-gen")
            self._chroot(f"echo 'LANG={locale}' > /etc/locale.conf")
            self._chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
            self._chroot("hwclock --systohc")
            self._chroot(f"echo 'KEYMAP={state['keymap']}' > /etc/vconsole.conf")
            self._pct(59)

            self._stage(L("Generating initramfs…", "Generando initramfs…"))
            self._chroot("mkinitcpio -P")
            self._pct(63)

            self._stage(L("Setting passwords…", "Estableciendo contrasenas…"))
            self._chroot_passwd("root", state["root_pass"])
            self._pct(65)

            uname = state["username"]
            self._stage(L(f"Creating user '{uname}'…", f"Creando usuario '{uname}'…"))
            self._chroot(f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}")
            self._chroot_passwd(uname, state["user_pass"])
            self._chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers")
            self._pct(68)

            self._stage(L("Enabling NetworkManager…", "Habilitando NetworkManager…"))
            self._chroot("systemctl enable NetworkManager")
            self._pct(71)

            if state["gpu"] == "NVIDIA":
                self._stage(L("Installing NVIDIA drivers…", "Instalando drivers NVIDIA…"))
                self._pacman("arch-chroot /mnt pacman -S --noconfirm nvidia nvidia-utils nvidia-settings", 71, 77, ignore_error=True)
            elif state["gpu"] == "AMD/Intel":
                self._stage(L("Installing AMD/Intel drivers…", "Instalando drivers AMD/Intel…"))
                self._pacman("arch-chroot /mnt pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver", 71, 77, ignore_error=True)
            else:
                self._pct(77)

            if state["desktop"] == "KDE Plasma":
                self._stage(L("Installing KDE Plasma…", "Instalando KDE Plasma…"))
                self._pacman("arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput", 77, 83, ignore_error=True)
                self._pacman("arch-chroot /mnt pacman -S --noconfirm plasma-meta konsole dolphin ark kate plasma-nm firefox sddm", 83, 93, ignore_error=True)
                self._chroot("systemctl enable sddm")
            elif state["desktop"] == "Cinnamon":
                self._stage(L("Installing Cinnamon…", "Instalando Cinnamon…"))
                self._pacman("arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput", 77, 83, ignore_error=True)
                self._pacman("arch-chroot /mnt pacman -S --noconfirm cinnamon lightdm lightdm-gtk-greeter alacritty firefox", 83, 93, ignore_error=True)
                self._chroot("systemctl enable lightdm")
            else:
                self._pct(93)

            self._stage(L("Installing GRUB bootloader…", "Instalando GRUB…"))
            self._chroot("grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB")
            self._chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self._pct(100)
            self._stage(L("Installation complete!", "Instalacion completa!"))
            self.on_done(True, "")
        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))


def build_gtk_app():
    import gi
    gi.require_version("Gtk", "3.0")
    from gi.repository import Gtk, Gdk, GLib, Pango

    CSS = b"""
    * {
        font-family: "Noto Sans", "Liberation Sans", sans-serif;
    }
    window {
        background-color: #1b1f2e;
    }
    .sidebar {
        background-color: #0d1117;
        border-right: 1px solid #2d3348;
    }
    .sidebar-title {
        color: #1793d1;
        font-size: 15px;
        font-weight: bold;
        padding: 20px 16px 8px 16px;
    }
    .step-row {
        padding: 8px 16px;
        color: #8892a4;
        font-size: 12px;
        border-radius: 0;
    }
    .step-row.active {
        background-color: #1793d1;
        color: white;
        font-weight: bold;
    }
    .step-row.done {
        color: #4ade80;
    }
    .header-bar {
        background-color: #0d1117;
        border-bottom: 1px solid #2d3348;
        padding: 0 16px;
        min-height: 52px;
    }
    .header-title {
        color: #1793d1;
        font-size: 16px;
        font-weight: bold;
    }
    .header-version {
        color: #8892a4;
        font-size: 10px;
    }
    .page-title {
        color: #e2e8f0;
        font-size: 22px;
        font-weight: bold;
        margin-bottom: 4px;
    }
    .page-subtitle {
        color: #8892a4;
        font-size: 12px;
        margin-bottom: 16px;
    }
    .content-area {
        background-color: #1b1f2e;
        padding: 32px 40px;
    }
    .card {
        background-color: #252836;
        border-radius: 6px;
        border: 1px solid #2d3348;
        padding: 16px;
        margin-bottom: 8px;
    }
    .card:hover {
        border-color: #1793d1;
        background-color: #2a2f42;
    }
    .card.selected {
        border-color: #1793d1;
        background-color: #1a2535;
    }
    .card-title {
        color: #e2e8f0;
        font-size: 13px;
        font-weight: bold;
    }
    .card-desc {
        color: #8892a4;
        font-size: 11px;
        margin-top: 2px;
    }
    .field-label {
        color: #8892a4;
        font-size: 12px;
        margin-bottom: 4px;
    }
    entry {
        background-color: #252836;
        color: #e2e8f0;
        border: 1px solid #2d3348;
        border-radius: 4px;
        padding: 8px 12px;
        font-size: 13px;
        caret-color: #1793d1;
    }
    entry:focus {
        border-color: #1793d1;
        background-color: #1a2535;
    }
    .btn-primary {
        background: #1793d1;
        color: white;
        border: none;
        border-radius: 4px;
        padding: 10px 20px;
        font-size: 13px;
        font-weight: bold;
    }
    .btn-primary:hover {
        background: #1480bb;
    }
    .btn-ghost {
        background: #252836;
        color: #8892a4;
        border: 1px solid #2d3348;
        border-radius: 4px;
        padding: 10px 20px;
        font-size: 13px;
    }
    .btn-ghost:hover {
        background: #2d3348;
        color: #e2e8f0;
    }
    .btn-danger {
        background: #dc2626;
        color: white;
        border: none;
        border-radius: 4px;
        padding: 10px 20px;
        font-size: 13px;
        font-weight: bold;
    }
    .btn-danger:hover {
        background: #b91c1c;
    }
    .footer-bar {
        background-color: #0d1117;
        border-top: 1px solid #2d3348;
        padding: 10px 16px;
        min-height: 56px;
    }
    .error-label {
        color: #f87171;
        font-size: 12px;
    }
    .warn-box {
        background-color: #3b1f1f;
        border: 1px solid #7f1d1d;
        border-radius: 4px;
        padding: 12px 16px;
        color: #fca5a5;
        font-size: 12px;
        font-weight: bold;
    }
    .review-row-even {
        background-color: #252836;
    }
    .review-row-odd {
        background-color: #1e2130;
    }
    .review-key {
        color: #8892a4;
        font-size: 12px;
        padding: 7px 12px;
    }
    .review-val-ok {
        color: #4ade80;
        font-size: 12px;
        font-weight: bold;
        padding: 7px 8px;
    }
    .review-val-default {
        color: #facc15;
        font-size: 12px;
        font-weight: bold;
        padding: 7px 8px;
    }
    .review-val-missing {
        color: #f87171;
        font-size: 12px;
        font-weight: bold;
        padding: 7px 8px;
    }
    .log-view {
        background-color: #0d1117;
        color: #8892a4;
        font-family: monospace;
        font-size: 11px;
        padding: 8px;
    }
    .stage-label {
        color: #e2e8f0;
        font-size: 14px;
        font-weight: bold;
    }
    .pct-label {
        color: #1793d1;
        font-size: 18px;
        font-weight: bold;
    }
    progressbar trough {
        background-color: #2d3348;
        border-radius: 4px;
        min-height: 10px;
    }
    progressbar progress {
        background-color: #1793d1;
        border-radius: 4px;
        min-height: 10px;
    }
    progressbar.done progress {
        background-color: #4ade80;
    }
    .section-label {
        color: #1793d1;
        font-size: 12px;
        font-weight: bold;
        margin-top: 12px;
        margin-bottom: 4px;
    }
    separator {
        background-color: #2d3348;
        min-height: 1px;
        margin: 12px 0;
    }
    """

    STEP_NAMES_EN = ["Language", "Identity", "Passwords", "Disk", "Keymap", "Timezone", "Desktop", "GPU", "Review", "Install"]
    STEP_NAMES_ES = ["Idioma", "Identidad", "Contrasenas", "Disco", "Teclado", "Zona horaria", "Escritorio", "GPU", "Revision", "Instalar"]

    class InstallerWindow(Gtk.Window):
        def __init__(self):
            super().__init__(title=f"Arch Linux Installer — {VERSION}")
            self.set_default_size(1000, 660)
            self.set_position(Gtk.WindowPosition.CENTER)
            self.connect("delete-event", self._on_close)

            provider = Gtk.CssProvider()
            provider.load_from_data(CSS)
            Gtk.StyleContext.add_provider_for_screen(
                Gdk.Screen.get_default(),
                provider,
                Gtk.STYLE_PROVIDER_PRIORITY_APPLICATION
            )

            self._step = 0
            self._disk_buttons  = []
            self._de_buttons    = []
            self._gpu_buttons   = []
            self._km_buttons    = []

            outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            self.add(outer)

            outer.pack_start(self._build_header(), False, False, 0)

            mid = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            outer.pack_start(mid, True, True, 0)

            self._sidebar_box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            self._sidebar_box.get_style_context().add_class("sidebar")
            self._sidebar_box.set_size_request(210, -1)
            mid.pack_start(self._sidebar_box, False, False, 0)

            self._build_sidebar()

            self._stack = Gtk.Stack()
            self._stack.set_transition_type(Gtk.StackTransitionType.SLIDE_LEFT_RIGHT)
            self._stack.set_transition_duration(200)
            mid.pack_start(self._stack, True, True, 0)

            self._build_all_pages()

            outer.pack_start(self._build_footer(), False, False, 0)

            self._go_to(0)
            self.show_all()

        def _on_close(self, *_):
            Gtk.main_quit()

        def _build_header(self):
            box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            box.get_style_context().add_class("header-bar")
            lbl = Gtk.Label(label="❱  Arch Linux Installer")
            lbl.get_style_context().add_class("header-title")
            box.pack_start(lbl, False, False, 0)
            ver = Gtk.Label(label=VERSION)
            ver.get_style_context().add_class("header-version")
            box.pack_end(ver, False, False, 8)
            return box

        def _build_sidebar(self):
            title = Gtk.Label(label="Steps")
            title.get_style_context().add_class("sidebar-title")
            title.set_halign(Gtk.Align.START)
            self._sidebar_box.pack_start(title, False, False, 0)

            self._step_rows = []
            for i, name in enumerate(STEP_NAMES_EN):
                row = Gtk.Label(label=f"  {i+1:02d}  {name}")
                row.set_halign(Gtk.Align.START)
                row.get_style_context().add_class("step-row")
                self._sidebar_box.pack_start(row, False, False, 0)
                self._step_rows.append(row)

        def _refresh_sidebar(self):
            names = STEP_NAMES_ES if state["lang"] == "es" else STEP_NAMES_EN
            for i, row in enumerate(self._step_rows):
                ctx = row.get_style_context()
                ctx.remove_class("active")
                ctx.remove_class("done")
                if i < self._step:
                    row.set_text(f"  ✔   {names[i]}")
                    ctx.add_class("done")
                elif i == self._step:
                    row.set_text(f"  ▶   {names[i]}")
                    ctx.add_class("active")
                else:
                    row.set_text(f"  {i+1:02d}  {names[i]}")

        def _build_footer(self):
            box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            box.get_style_context().add_class("footer-bar")

            self._btn_back = Gtk.Button(label="← Back")
            self._btn_back.get_style_context().add_class("btn-ghost")
            self._btn_back.connect("clicked", lambda _: self._go_back())
            box.pack_start(self._btn_back, False, False, 0)

            self._footer_err = Gtk.Label(label="")
            self._footer_err.get_style_context().add_class("error-label")
            box.pack_start(self._footer_err, True, True, 8)

            self._btn_next = Gtk.Button(label="Next →")
            self._btn_next.get_style_context().add_class("btn-primary")
            self._btn_next.connect("clicked", lambda _: self._go_next())
            box.pack_end(self._btn_next, False, False, 0)

            return box

        def _page_wrap(self, title, subtitle=""):
            scroll = Gtk.ScrolledWindow()
            scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            outer = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            outer.get_style_context().add_class("content-area")
            scroll.add(outer)

            t = Gtk.Label(label=title)
            t.get_style_context().add_class("page-title")
            t.set_halign(Gtk.Align.START)
            outer.pack_start(t, False, False, 0)

            if subtitle:
                s = Gtk.Label(label=subtitle)
                s.get_style_context().add_class("page-subtitle")
                s.set_halign(Gtk.Align.START)
                outer.pack_start(s, False, False, 0)

            sep = Gtk.Separator()
            outer.pack_start(sep, False, False, 0)

            return scroll, outer

        def _field(self, parent, label_text, placeholder="", secret=False):
            lbl = Gtk.Label(label=label_text)
            lbl.get_style_context().add_class("field-label")
            lbl.set_halign(Gtk.Align.START)
            parent.pack_start(lbl, False, False, 0)
            entry = Gtk.Entry()
            if secret:
                entry.set_visibility(False)
                entry.set_invisible_char("•")
            if placeholder:
                entry.set_placeholder_text(placeholder)
            entry.set_max_width_chars(40)
            parent.pack_start(entry, False, False, 0)
            return entry

        def _section(self, parent, text):
            lbl = Gtk.Label(label=text)
            lbl.get_style_context().add_class("section-label")
            lbl.set_halign(Gtk.Align.START)
            parent.pack_start(lbl, False, False, 0)

        def _err_lbl(self, parent):
            lbl = Gtk.Label(label="")
            lbl.get_style_context().add_class("error-label")
            lbl.set_halign(Gtk.Align.START)
            parent.pack_start(lbl, False, False, 4)
            return lbl

        def _choice_card(self, parent, group, value, title, desc, buttons_list):
            box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
            box.get_style_context().add_class("card")
            rb = Gtk.RadioButton.new_with_label_from_widget(group, "")
            rb.set_active(False)
            col = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            t = Gtk.Label(label=title)
            t.get_style_context().add_class("card-title")
            t.set_halign(Gtk.Align.START)
            col.pack_start(t, False, False, 0)
            if desc:
                d = Gtk.Label(label=desc)
                d.get_style_context().add_class("card-desc")
                d.set_halign(Gtk.Align.START)
                col.pack_start(d, False, False, 0)
            box.pack_start(rb, False, False, 8)
            box.pack_start(col, True, True, 0)

            event_box = Gtk.EventBox()
            event_box.add(box)
            event_box.connect("button-press-event", lambda _w, _e, rb=rb: rb.set_active(True))
            parent.pack_start(event_box, False, False, 2)
            buttons_list.append((value, rb))
            return rb

        def _get_selected(self, buttons_list):
            for val, rb in buttons_list:
                if rb.get_active():
                    return val
            return None

        def _build_all_pages(self):
            self._pages = [
                self._page_language(),
                self._page_identity(),
                self._page_passwords(),
                self._page_disk(),
                self._page_keymap(),
                self._page_timezone(),
                self._page_desktop(),
                self._page_gpu(),
                self._page_review(),
                self._page_install(),
            ]
            for i, page in enumerate(self._pages):
                self._stack.add_named(page, str(i))

        def _go_to(self, idx):
            self._step = idx
            self._stack.set_visible_child_name(str(idx))
            self._refresh_sidebar()
            self._footer_err.set_text("")

            is_install = (idx == len(self._pages) - 1)
            self._btn_next.set_visible(not is_install)
            self._btn_back.set_visible(not is_install)
            self._btn_back.set_sensitive(idx > 0)

            if idx == len(self._pages) - 2:
                self._refresh_review()
                btn_ctx = self._btn_next.get_style_context()
                btn_ctx.remove_class("btn-primary")
                btn_ctx.add_class("btn-danger")
                self._btn_next.set_label(L("Install Now →", "Instalar ahora →"))
            else:
                btn_ctx = self._btn_next.get_style_context()
                btn_ctx.remove_class("btn-danger")
                btn_ctx.add_class("btn-primary")
                self._btn_next.set_label("Next →")

        def _go_next(self):
            validators = [
                self._val_language, self._val_identity, self._val_passwords,
                self._val_disk, self._val_keymap, self._val_timezone,
                self._val_desktop, self._val_gpu,
                self._val_review, lambda: True,
            ]
            if validators[self._step]():
                if self._step < len(self._pages) - 1:
                    self._go_to(self._step + 1)

        def _go_back(self):
            if self._step > 0:
                self._go_to(self._step - 1)

        def _show_err(self, msg):
            self._footer_err.set_text(msg)

        def _page_language(self):
            scroll, p = self._page_wrap("Language / Idioma", "Choose the installer language.")
            self._lang_btns = []
            group = None
            for code, name, flag in [("en", "English", "🇬🇧"), ("es", "Español", "🇪🇸")]:
                if group is None:
                    rb = self._choice_card(p, None, code, f"{flag}  {name}", "", self._lang_btns)
                    group = rb
                else:
                    self._choice_card(p, group, code, f"{flag}  {name}", "", self._lang_btns)
            for val, rb in self._lang_btns:
                if val == state["lang"]:
                    rb.set_active(True)
            return scroll

        def _val_language(self):
            val = self._get_selected(self._lang_btns)
            if val:
                state["lang"] = val
                self._refresh_sidebar()
            return True

        def _page_identity(self):
            scroll, p = self._page_wrap(
                L("System Identity", "Identidad del sistema"),
                L("Letters, digits, - or _ · max 32 chars", "Letras, digitos, - o _ · max 32 caracteres")
            )
            self._err_id = self._err_lbl(p)
            self._e_hostname = self._field(p, L("Hostname", "Nombre de equipo"), "e.g. archbox")
            self._e_hostname.set_text(state.get("hostname", ""))
            self._e_username = self._field(p, L("Username", "Usuario"), "e.g. alice")
            self._e_username.set_text(state.get("username", ""))
            return scroll

        def _val_identity(self):
            hn = self._e_hostname.get_text().strip()
            un = self._e_username.get_text().strip()
            if not validate_name(hn):
                self._err_id.set_text(L("✗  Invalid hostname.", "✗  Hostname invalido."))
                return False
            if not validate_name(un):
                self._err_id.set_text(L("✗  Invalid username.", "✗  Usuario invalido."))
                return False
            state["hostname"] = hn
            state["username"] = un
            self._err_id.set_text("")
            return True

        def _page_passwords(self):
            scroll, p = self._page_wrap(L("Passwords", "Contrasenas"))
            self._err_pw = self._err_lbl(p)
            self._section(p, L("Root account", "Cuenta root"))
            self._e_rp1 = self._field(p, L("Root password", "Contrasena root"), secret=True)
            self._e_rp2 = self._field(p, L("Confirm root password", "Confirmar contrasena root"), secret=True)
            sep = Gtk.Separator()
            p.pack_start(sep, False, False, 0)
            self._section(p, L("User account", "Cuenta de usuario"))
            self._e_up1 = self._field(p, L("User password", "Contrasena de usuario"), secret=True)
            self._e_up2 = self._field(p, L("Confirm user password", "Confirmar contrasena de usuario"), secret=True)
            return scroll

        def _val_passwords(self):
            rp1, rp2 = self._e_rp1.get_text(), self._e_rp2.get_text()
            up1, up2 = self._e_up1.get_text(), self._e_up2.get_text()
            if not rp1:
                self._err_pw.set_text(L("✗  Root password is empty.", "✗  Contrasena root vacia.")); return False
            if rp1 != rp2:
                self._err_pw.set_text(L("✗  Root passwords do not match.", "✗  Contrasenas root no coinciden.")); return False
            if not up1:
                self._err_pw.set_text(L("✗  User password is empty.", "✗  Contrasena de usuario vacia.")); return False
            if up1 != up2:
                self._err_pw.set_text(L("✗  User passwords do not match.", "✗  Contrasenas de usuario no coinciden.")); return False
            state["root_pass"] = rp1
            state["user_pass"] = up1
            self._err_pw.set_text("")
            return True

        def _page_disk(self):
            scroll, p = self._page_wrap(
                L("Disk & Swap", "Disco y Swap"),
                L("⚠  ALL DATA on the selected disk will be ERASED",
                  "⚠  SE BORRARAN TODOS LOS DATOS del disco seleccionado")
            )
            self._err_disk = self._err_lbl(p)
            disks = list_disks()
            self._disk_btns = []
            if not disks:
                lbl = Gtk.Label(label=L("✗  No disks found.", "✗  No se encontraron discos."))
                lbl.get_style_context().add_class("error-label")
                p.pack_start(lbl, False, False, 0)
                return scroll

            group = None
            for name, size_gb, model in disks:
                title = f"/dev/{name}   —   {size_gb} GB"
                desc  = model or "Unknown"
                if group is None:
                    rb = self._choice_card(p, None, name, title, desc, self._disk_btns)
                    group = rb
                else:
                    self._choice_card(p, group, name, title, desc, self._disk_btns)
                if name == state.get("disk"):
                    self._disk_btns[-1][1].set_active(True)

            if self._disk_btns and not any(rb.get_active() for _, rb in self._disk_btns):
                self._disk_btns[0][1].set_active(True)

            sep = Gtk.Separator()
            p.pack_start(sep, False, False, 0)
            self._section(p, L("Swap size", "Tamano de Swap"))

            swap_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            self._swap_spin = Gtk.SpinButton()
            adj = Gtk.Adjustment(value=int(state["swap"]), lower=1, upper=128, step_increment=1)
            self._swap_spin.set_adjustment(adj)
            self._swap_spin.set_digits(0)
            swap_box.pack_start(self._swap_spin, False, False, 0)
            swap_box.pack_start(Gtk.Label(label="GB"), False, False, 0)
            p.pack_start(swap_box, False, False, 0)
            return scroll

        def _val_disk(self):
            val = self._get_selected(self._disk_btns)
            if not val:
                self._err_disk.set_text(L("✗  Select a disk.", "✗  Selecciona un disco.")); return False
            state["disk"] = val
            state["swap"] = str(int(self._swap_spin.get_value()))
            self._err_disk.set_text("")
            return True

        def _page_keymap(self):
            scroll, p = self._page_wrap(L("Keyboard Layout", "Distribucion de teclado"))
            try:
                out   = subprocess.check_output("localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
                maps  = [l for l in out.splitlines() if l]
            except Exception:
                maps  = []
            wanted  = ["us", "es", "uk", "fr", "de", "it", "ru", "ara", "pt-latin9", "br-abnt2"]
            options = [m for m in wanted if m in maps] if maps else wanted

            self._km_btns = []
            flow = Gtk.FlowBox()
            flow.set_max_children_per_line(4)
            flow.set_selection_mode(Gtk.SelectionMode.NONE)
            p.pack_start(flow, False, False, 0)

            group = None
            for km in options:
                box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL)
                box.get_style_context().add_class("card")
                if group is None:
                    rb = Gtk.RadioButton.new_with_label_from_widget(None, km)
                    group = rb
                else:
                    rb = Gtk.RadioButton.new_with_label_from_widget(group, km)
                if km == state["keymap"]:
                    rb.set_active(True)
                box.pack_start(rb, False, False, 8)
                ev = Gtk.EventBox()
                ev.add(box)
                ev.connect("button-press-event", lambda _w, _e, r=rb: r.set_active(True))
                flow.add(ev)
                self._km_btns.append((km, rb))
            return scroll

        def _val_keymap(self):
            for val, rb in self._km_btns:
                if rb.get_active():
                    state["keymap"] = val
                    run_simple(f"loadkeys {shlex.quote(val)}", ignore_error=True)
                    return True
            return True

        def _page_timezone(self):
            scroll, p = self._page_wrap(L("Timezone", "Zona horaria"))
            try:
                out   = subprocess.check_output("timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
                zones = [l for l in out.splitlines() if l]
            except Exception:
                zones = []
            if not zones:
                zones = ["UTC", "Europe/Madrid", "Europe/London", "America/New_York", "America/Los_Angeles", "Asia/Tokyo"]

            self._all_zones = zones
            regions = sorted(set(z.split("/")[0] for z in zones if "/" in z))
            regions = ["UTC"] + regions

            search_box = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=8)
            search_icon = Gtk.Label(label="🔍")
            self._tz_search = Gtk.Entry()
            self._tz_search.set_placeholder_text(L("Search timezone…", "Buscar zona horaria…"))
            search_box.pack_start(search_icon, False, False, 0)
            search_box.pack_start(self._tz_search, True, True, 0)
            p.pack_start(search_box, False, False, 0)

            panes = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            p.pack_start(panes, True, True, 8)

            lf = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            lbl_r = Gtk.Label(label=L("Region", "Region"))
            lbl_r.get_style_context().add_class("section-label")
            lbl_r.set_halign(Gtk.Align.START)
            lf.pack_start(lbl_r, False, False, 0)
            r_scroll = Gtk.ScrolledWindow()
            r_scroll.set_size_request(190, 260)
            r_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            self._region_store = Gtk.ListStore(str)
            self._region_view  = Gtk.TreeView(model=self._region_store)
            self._region_view.append_column(Gtk.TreeViewColumn("", Gtk.CellRendererText(), text=0))
            self._region_view.set_headers_visible(False)
            r_scroll.add(self._region_view)
            lf.pack_start(r_scroll, True, True, 0)
            panes.pack_start(lf, False, False, 0)

            rf = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            lbl_c = Gtk.Label(label=L("City / Zone", "Ciudad / Zona"))
            lbl_c.get_style_context().add_class("section-label")
            lbl_c.set_halign(Gtk.Align.START)
            rf.pack_start(lbl_c, False, False, 0)
            c_scroll = Gtk.ScrolledWindow()
            c_scroll.set_size_request(260, 260)
            c_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            self._city_store = Gtk.ListStore(str)
            self._city_view  = Gtk.TreeView(model=self._city_store)
            self._city_view.append_column(Gtk.TreeViewColumn("", Gtk.CellRendererText(), text=0))
            self._city_view.set_headers_visible(False)
            c_scroll.add(self._city_view)
            rf.pack_start(c_scroll, True, True, 0)
            panes.pack_start(rf, True, True, 0)

            self._tz_selected_lbl = Gtk.Label(label=f"Selected: {state['timezone']}")
            self._tz_selected_lbl.get_style_context().add_class("section-label")
            self._tz_selected_lbl.set_halign(Gtk.Align.START)
            p.pack_start(self._tz_selected_lbl, False, False, 0)

            for r in regions:
                self._region_store.append([r])

            def fill_cities(region):
                self._city_store.clear()
                if region == "UTC":
                    self._city_store.append(["UTC"])
                    return
                for z in zones:
                    if z.startswith(region + "/"):
                        self._city_store.append([z.split("/", 1)[1]])

            def on_region_changed(_sel):
                model, it = _sel.get_selected()
                if it:
                    fill_cities(model[it][0])

            def on_city_changed(_sel):
                m_r, it_r = self._region_view.get_selection().get_selected()
                m_c, it_c = _sel.get_selected()
                if it_c:
                    city = m_c[it_c][0]
                    region = m_r[it_r][0] if it_r else "UTC"
                    tz = "UTC" if region == "UTC" else f"{region}/{city}"
                    state["timezone"] = tz
                    self._tz_selected_lbl.set_text(f"Selected: {tz}")

            def on_search(_entry):
                q = _entry.get_text().lower()
                self._city_store.clear()
                if q:
                    for z in zones:
                        if q in z.lower():
                            self._city_store.append([z])
                else:
                    model, it = self._region_view.get_selection().get_selected()
                    if it:
                        fill_cities(model[it][0])

            self._region_view.get_selection().connect("changed", on_region_changed)
            self._city_view.get_selection().connect("changed", on_city_changed)
            self._tz_search.connect("changed", on_search)

            cur_region = state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC"
            for i, row in enumerate(self._region_store):
                if row[0] == cur_region:
                    self._region_view.get_selection().select_path(Gtk.TreePath(i))
                    fill_cities(cur_region)
                    break

            return scroll

        def _val_timezone(self):
            return True

        def _page_desktop(self):
            scroll, p = self._page_wrap(
                L("Desktop Environment", "Entorno de escritorio"),
                L("Choose a desktop or install headless.", "Elige un escritorio o instala sin entorno grafico.")
            )
            self._de_btns = []
            options = [
                ("KDE Plasma", L("Full KDE desktop + Konsole + Dolphin + Firefox + SDDM",
                                 "KDE completo + Konsole + Dolphin + Firefox + SDDM")),
                ("Cinnamon",   L("Classic Cinnamon desktop + Alacritty + Firefox + LightDM",
                                 "Escritorio Cinnamon clasico + Alacritty + Firefox + LightDM")),
                ("None",       L("No desktop — command line only", "Sin escritorio — solo linea de comandos")),
            ]
            group = None
            for val, desc in options:
                if group is None:
                    rb = self._choice_card(p, None, val, val, desc, self._de_btns)
                    group = rb
                else:
                    self._choice_card(p, group, val, val, desc, self._de_btns)
            for v, rb in self._de_btns:
                if v == state["desktop"]:
                    rb.set_active(True)
            return scroll

        def _val_desktop(self):
            val = self._get_selected(self._de_btns)
            if val:
                state["desktop"] = val
            return True

        def _page_gpu(self):
            scroll, p = self._page_wrap(L("GPU Drivers", "Drivers GPU"),
                                         L("Select your GPU vendor.", "Selecciona tu GPU."))
            self._gpu_btns = []
            options = [
                ("NVIDIA",    L("NVIDIA proprietary drivers (nvidia + nvidia-utils)",
                                "Drivers propietarios NVIDIA (nvidia + nvidia-utils)")),
                ("AMD/Intel", L("Open-source Mesa drivers (mesa + vulkan-radeon)",
                                "Drivers open-source Mesa (mesa + vulkan-radeon)")),
                ("None",      L("No additional GPU drivers", "Sin drivers adicionales de GPU")),
            ]
            group = None
            for val, desc in options:
                if group is None:
                    rb = self._choice_card(p, None, val, val, desc, self._gpu_btns)
                    group = rb
                else:
                    self._choice_card(p, group, val, val, desc, self._gpu_btns)
            for v, rb in self._gpu_btns:
                if v == state["gpu"]:
                    rb.set_active(True)
            return scroll

        def _val_gpu(self):
            val = self._get_selected(self._gpu_btns)
            if val:
                state["gpu"] = val
            return True

        def _page_review(self):
            scroll, p = self._page_wrap(
                L("Review & Confirm", "Revisar y confirmar"),
                L("Check your settings before installing.", "Revisa la configuracion antes de instalar.")
            )
            self._review_container = p
            return scroll

        def _refresh_review(self):
            p = self._review_container
            for child in list(p.get_children())[3:]:
                p.remove(child)

            DEFAULTS = {"swap": "8", "desktop": "None", "gpu": "None", "keymap": "us", "timezone": "UTC"}
            rows = [
                (L("Language", "Idioma"), state["lang"],                                     "lang"),
                ("Hostname",              state["hostname"] or "—",                          "hostname"),
                (L("Username","Usuario"), state["username"] or "—",                          "username"),
                ("Disk",                  f"/dev/{state['disk']}" if state["disk"] else "NOT SET", "disk"),
                ("Swap",                  f"{state['swap']} GB",                             "swap"),
                ("Keymap",                state["keymap"],                                   "keymap"),
                ("Timezone",              state["timezone"],                                  "timezone"),
                ("Desktop",               state["desktop"],                                  "desktop"),
                ("GPU",                   state["gpu"],                                       "gpu"),
            ]

            grid = Gtk.Grid()
            grid.set_column_spacing(0)
            grid.set_row_spacing(0)
            for i, (key, val, k) in enumerate(rows):
                raw = state.get(k)
                if not raw:
                    val_class = "review-val-missing"
                elif DEFAULTS.get(k) == raw:
                    val_class = "review-val-default"
                else:
                    val_class = "review-val-ok"

                row_class = "review-row-even" if i % 2 == 0 else "review-row-odd"

                klbl = Gtk.Label(label=key)
                klbl.get_style_context().add_class("review-key")
                klbl.get_style_context().add_class(row_class)
                klbl.set_halign(Gtk.Align.START)
                klbl.set_size_request(160, -1)

                vlbl = Gtk.Label(label=val)
                vlbl.get_style_context().add_class(val_class)
                vlbl.get_style_context().add_class(row_class)
                vlbl.set_halign(Gtk.Align.START)

                grid.attach(klbl, 0, i, 1, 1)
                grid.attach(vlbl, 1, i, 1, 1)

            p.pack_start(grid, False, False, 0)

            hint = Gtk.Label(label=L("Yellow = default  ·  Green = custom  ·  Red = missing",
                                     "Amarillo = default  ·  Verde = personaliz.  ·  Rojo = falta"))
            hint.get_style_context().add_class("field-label")
            hint.set_halign(Gtk.Align.START)
            p.pack_start(hint, False, False, 4)

            missing = []
            if not state["hostname"]:  missing.append("hostname")
            if not state["username"]:  missing.append("username")
            if not state["disk"]:      missing.append("disk")
            if not state["root_pass"]: missing.append(L("root password", "contrasena root"))

            self._review_missing = missing

            if missing:
                lbl = Gtk.Label(label=L(f"✗  Missing: {', '.join(missing)}",
                                        f"✗  Falta: {', '.join(missing)}"))
                lbl.get_style_context().add_class("error-label")
                lbl.set_halign(Gtk.Align.START)
                p.pack_start(lbl, False, False, 4)
            else:
                lbl = Gtk.Label(label=L("✔  All good — ready to install!", "✔  Todo listo — listo para instalar!"))
                lbl.get_style_context().add_class("review-val-ok")
                lbl.set_halign(Gtk.Align.START)
                p.pack_start(lbl, False, False, 4)

            warn_box = Gtk.Box()
            warn_box.get_style_context().add_class("warn-box")
            warn_lbl = Gtk.Label(label=L(
                f"⚠  THIS WILL PERMANENTLY ERASE  /dev/{state['disk'] or '???'}",
                f"⚠  ESTO BORRARA PERMANENTEMENTE  /dev/{state['disk'] or '???'}"
            ))
            warn_box.pack_start(warn_lbl, False, False, 0)
            p.pack_start(warn_box, False, False, 8)
            p.show_all()

        def _val_review(self):
            if self._review_missing:
                self._show_err(L("Go back and fill in the missing fields.", "Vuelve y completa los campos que faltan."))
                return False

            dialog = Gtk.MessageDialog(
                transient_for=self,
                modal=True,
                message_type=Gtk.MessageType.WARNING,
                buttons=Gtk.ButtonsType.YES_NO,
                text=L("Last warning", "Ultimo aviso"),
            )
            dialog.format_secondary_text(
                L(f"ALL DATA on /dev/{state['disk']} will be PERMANENTLY ERASED.\n\nContinue with the installation?",
                  f"TODOS LOS DATOS en /dev/{state['disk']} se BORRARAN PERMANENTEMENTE.\n\n¿Continuar con la instalacion?")
            )
            resp = dialog.run()
            dialog.destroy()
            return resp == Gtk.ResponseType.YES

        def _page_install(self):
            box = Gtk.Box(orientation=Gtk.Orientation.VERTICAL)
            box.get_style_context().add_class("content-area")

            top = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=8)
            top.set_margin_bottom(16)

            title = Gtk.Label(label=L("Installing Arch Linux…", "Instalando Arch Linux…"))
            title.get_style_context().add_class("page-title")
            title.set_halign(Gtk.Align.START)
            top.pack_start(title, False, False, 0)

            self._stage_lbl = Gtk.Label(label=L("Preparing…", "Preparando…"))
            self._stage_lbl.get_style_context().add_class("stage-label")
            self._stage_lbl.set_halign(Gtk.Align.START)
            top.pack_start(self._stage_lbl, False, False, 0)

            pct_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
            self._pct_lbl = Gtk.Label(label="0%")
            self._pct_lbl.get_style_context().add_class("pct-label")
            self._pct_lbl.set_size_request(52, -1)
            pct_row.pack_start(self._pct_lbl, False, False, 0)
            self._progress_bar = Gtk.ProgressBar()
            self._progress_bar.set_fraction(0)
            pct_row.pack_start(self._progress_bar, True, True, 0)
            top.pack_start(pct_row, False, False, 0)

            self._elapsed_lbl = Gtk.Label(label="Elapsed: 00:00")
            self._elapsed_lbl.get_style_context().add_class("field-label")
            self._elapsed_lbl.set_halign(Gtk.Align.START)
            top.pack_start(self._elapsed_lbl, False, False, 0)

            box.pack_start(top, False, False, 0)

            sep = Gtk.Separator()
            box.pack_start(sep, False, False, 0)

            log_title = Gtk.Label(label=L("Live log", "Log en vivo"))
            log_title.get_style_context().add_class("section-label")
            log_title.set_halign(Gtk.Align.START)
            box.pack_start(log_title, False, False, 4)

            log_scroll = Gtk.ScrolledWindow()
            log_scroll.set_policy(Gtk.PolicyType.NEVER, Gtk.PolicyType.AUTOMATIC)
            self._log_buf  = Gtk.TextBuffer()
            self._log_view = Gtk.TextView(buffer=self._log_buf)
            self._log_view.get_style_context().add_class("log-view")
            self._log_view.set_editable(False)
            self._log_view.set_wrap_mode(Gtk.WrapMode.WORD_CHAR)
            self._log_tag_err  = self._log_buf.create_tag("err",  foreground="#f87171")
            self._log_tag_warn = self._log_buf.create_tag("warn", foreground="#facc15")
            self._log_tag_ok   = self._log_buf.create_tag("ok",   foreground="#4ade80")
            log_scroll.add(self._log_view)
            box.pack_start(log_scroll, True, True, 0)

            self._install_start_time = None
            return box

        def _start_install(self):
            self._install_start_time = time.time()
            self._tick_elapsed()
            backend = InstallBackend(
                on_progress = self._on_progress,
                on_stage    = self._on_stage,
                on_log      = self._on_log,
                on_done     = self._on_done,
            )
            t = threading.Thread(target=backend.run, daemon=True)
            t.start()

        def _tick_elapsed(self):
            if self._install_start_time is None:
                return False
            secs = int(time.time() - self._install_start_time)
            self._elapsed_lbl.set_text(f"Elapsed: {secs//60:02d}:{secs%60:02d}")
            GLib.timeout_add(1000, self._tick_elapsed)

        def _on_progress(self, pct):
            def _upd():
                self._progress_bar.set_fraction(pct / 100.0)
                self._pct_lbl.set_text(f"{int(pct)}%")
                if pct >= 100:
                    self._progress_bar.get_style_context().add_class("done")
                return False
            GLib.idle_add(_upd)

        def _on_stage(self, msg):
            GLib.idle_add(lambda: self._stage_lbl.set_text(msg) or False)

        def _on_log(self, line):
            def _upd():
                end = self._log_buf.get_end_iter()
                ll  = line.lower()
                if any(w in ll for w in ("error", "fail", "fatal")):
                    self._log_buf.insert_with_tags(end, line + "\n", self._log_tag_err)
                elif any(w in ll for w in ("warning", "warn")):
                    self._log_buf.insert_with_tags(end, line + "\n", self._log_tag_warn)
                elif any(w in ll for w in ("complete", "success", "✔")):
                    self._log_buf.insert_with_tags(end, line + "\n", self._log_tag_ok)
                else:
                    self._log_buf.insert(end, line + "\n")
                self._log_view.scroll_to_iter(self._log_buf.get_end_iter(), 0, False, 0, 0)
                return False
            GLib.idle_add(_upd)

        def _on_done(self, success, reason):
            self._install_start_time = None

            def _show():
                overlay = Gtk.Box(orientation=Gtk.Orientation.VERTICAL, spacing=16)
                overlay.get_style_context().add_class("content-area")
                overlay.set_valign(Gtk.Align.CENTER)
                overlay.set_halign(Gtk.Align.CENTER)

                icon = Gtk.Label(label="✔" if success else "✗")
                icon.set_markup(f'<span font="48" foreground="{"#4ade80" if success else "#f87171"}">{"✔" if success else "✗"}</span>')
                overlay.pack_start(icon, False, False, 0)

                msg = Gtk.Label()
                if success:
                    msg.set_markup(f'<span font="18" foreground="#4ade80" weight="bold">{L("Installation complete!", "Instalacion completa!")}</span>')
                else:
                    msg.set_markup(f'<span font="18" foreground="#f87171" weight="bold">{L("Installation failed.", "Instalacion fallida.")}</span>')
                overlay.pack_start(msg, False, False, 0)

                sub_text = (
                    L("Remove the installation media and reboot.", "Extrae el medio de instalacion y reinicia.")
                    if success else
                    L(f"Check {LOG_FILE} for details.", f"Revisa {LOG_FILE} para mas detalles.")
                )
                sub = Gtk.Label(label=sub_text)
                sub.get_style_context().add_class("field-label")
                overlay.pack_start(sub, False, False, 0)

                btn_row = Gtk.Box(orientation=Gtk.Orientation.HORIZONTAL, spacing=12)
                btn_row.set_halign(Gtk.Align.CENTER)

                if success:
                    btn_reboot = Gtk.Button(label=L("Reboot now", "Reiniciar ahora"))
                    btn_reboot.get_style_context().add_class("btn-primary")
                    btn_reboot.connect("clicked", lambda _: (
                        subprocess.run("umount -R /mnt", shell=True),
                        subprocess.run("reboot", shell=True),
                        sys.exit(0)
                    ))
                    btn_row.pack_start(btn_reboot, False, False, 0)

                btn_shell = Gtk.Button(label=L("Exit to shell", "Salir al shell"))
                btn_shell.get_style_context().add_class("btn-ghost")
                btn_shell.connect("clicked", lambda _: sys.exit(0 if success else 1))
                btn_row.pack_start(btn_shell, False, False, 0)
                overlay.pack_start(btn_row, False, False, 0)

                win = self._stack.get_child_by_name(str(len(self._pages) - 1))
                for child in win.get_children()[0].get_children():
                    child.destroy()
                win.get_children()[0].pack_start(overlay, True, True, 0)
                overlay.show_all()
                return False

            GLib.idle_add(_show)

    class App:
        def __init__(self):
            self.win = InstallerWindow()
            self.win.show_all()

        def run(self):
            self.win._go_to(0)

            def delayed_install():
                if self.win._step == len(self.win._pages) - 1:
                    self.win._start_install()
                return False

            orig_go_to = self.win._go_to
            def patched_go_to(idx):
                orig_go_to(idx)
                if idx == len(self.win._pages) - 1:
                    GLib.idle_add(lambda: (self.win._start_install(), False)[1])
            self.win._go_to = patched_go_to

            Gtk.main()

    app = App()
    app.run()


def bootstrap():
    if os.geteuid() != 0:
        print("This installer must be run as root.")
        sys.exit(1)

    missing_pkgs = []
    for pkg_check, pkg_name in [
        ("Xorg.log", None),
        (shutil.which("Xorg"),  "xorg-server"),
        (shutil.which("xinit"), "xorg-xinit"),
    ]:
        pass

    needs_xorg    = not shutil.which("Xorg")
    needs_xinit   = not shutil.which("xinit")
    needs_gobject = False
    try:
        import gi
        gi.require_version("Gtk", "3.0")
        from gi.repository import Gtk
    except Exception:
        needs_gobject = True

    if needs_xorg or needs_xinit or needs_gobject:
        print(f"[*] Installing graphical environment packages…")
        rc = subprocess.call(
            f"pacman -Sy --noconfirm {BOOTSTRAP_PKGS}",
            shell=True, executable="/bin/bash"
        )
        if rc != 0:
            print("[!] Failed to install packages. Check your network connection.")
            sys.exit(1)
        print("[+] Packages installed successfully.\n")

    if not os.environ.get("DISPLAY"):
        script_path = os.path.abspath(__file__)
        print("[*] No display detected — launching X server…")
        os.execv(
            "/usr/bin/xinit",
            ["xinit", sys.executable, script_path, "--", ":1", "-nolisten", "tcp"]
        )
    else:
        build_gtk_app()


if __name__ == "__main__":
    bootstrap()
