"""
Arch Linux Installer — urwid TUI Edition
MIT LICENSE — credits to humrand https://github.com/humrand/arch-anstallation-easy
DO NOT REMOVE THIS FROM YOUR CODE IF YOU USE IT TO MODIFY IT.
Requires: python-urwid  (pacman -S python-urwid)  ~1 MB, no X11 needed.
"""

import subprocess, sys, os, re, shutil, shlex, threading, time
from datetime import datetime

VERSION  = "V1.3.0-urwid"
LOG_FILE = "/mnt/install_log.txt"

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

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(f"[{datetime.now():%Y-%m-%d %H:%M:%S}] {line}\n")
    except Exception:
        pass

def run_stream(cmd, on_line=None, ignore_error=False):
    write_log(f"$ {cmd}")
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE,
                         stderr=subprocess.STDOUT, executable="/bin/bash")
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
        return subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1",
                               shell=True, executable="/bin/bash") == 0
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
        out = subprocess.check_output("lsblk -b -d -o NAME,SIZE,MODEL | tail -n +2",
                                      shell=True, text=True)
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
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s")


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
        dl_done = [False]
        def on_line(line):
            self._log(line)
            m = _PAT_INSTALL.search(line)
            if m:
                dl_done[0] = True
                cur, total = int(m.group(1)), int(m.group(2))
                if total > 0:
                    self._pct(half + (cur / total) * (end - half))
                return
            if not dl_done[0] and _PAT_DOWNLOAD.search(line):
                cap = start + (end - start) * 0.45
                with self._lock:
                    cur_p = self._progress
                if cur_p < cap:
                    self._pct(cur_p + 0.3)
        rc = run_stream(cmd, on_line=on_line, ignore_error=ignore_error)
        self._pct(end)
        return rc

    def _chroot(self, cmd, ignore_error=False):
        return run_stream(f"arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}",
                          on_line=self._log, ignore_error=ignore_error)

    def _chroot_passwd(self, user, pwd):
        return run_stream(
            f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} | arch-chroot /mnt chpasswd",
            on_line=self._log, ignore_error=True)

    def run(self):
        disk_path  = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)
        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.on_done(False, L("No network connection.", "Sin conexion de red.")); return
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

            self._stage(L("Installing base system…", "Instalando sistema base…"))
            pkgs = ("base linux linux-firmware linux-headers sof-firmware base-devel "
                    "grub efibootmgr vim nano networkmanager sudo bash-completion")
            rc = self._pacman(f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self.on_done(False, L("pacstrap failed. Check " + LOG_FILE,
                                      "pacstrap fallo. Revisa " + LOG_FILE)); return

            self._stage(L("Generating fstab…", "Generando fstab…"))
            run_stream("genfstab -U /mnt >> /mnt/etc/fstab", on_line=self._log)
            self._pct(53)

            self._stage(L("Configuring hostname…", "Configurando hostname…"))
            hn = state["hostname"]
            with open("/mnt/etc/hostname", "w") as f: f.write(hn + "\n")
            with open("/mnt/etc/hosts", "w") as f:
                f.write(f"127.0.0.1\tlocalhost\n::1\t\tlocalhost\n127.0.1.1\t{hn}.localdomain\t{hn}\n")
            self._pct(55)

            self._stage(L("Configuring locale & timezone…", "Configurando locale y zona horaria…"))
            locale = "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
            self._chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            if locale != "en_US.UTF-8":
                ll = f"{locale} UTF-8"
                self._chroot(f"sed -i 's/^#{ll}/{ll}/' /etc/locale.gen", ignore_error=True)
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
                self._pacman("arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xf86-input-libinput", 77, 83, ignore_error=True)
                self._pacman("arch-chroot /mnt pacman -S --noconfirm plasma-meta konsole dolphin firefox sddm", 83, 93, ignore_error=True)
                self._chroot("systemctl enable sddm")
            elif state["desktop"] == "Cinnamon":
                self._stage(L("Installing Cinnamon…", "Instalando Cinnamon…"))
                self._pacman("arch-chroot /mnt pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xf86-input-libinput", 77, 83, ignore_error=True)
                self._pacman("arch-chroot /mnt pacman -S --noconfirm cinnamon lightdm lightdm-gtk-greeter alacritty firefox", 83, 93, ignore_error=True)
                self._chroot("systemctl enable lightdm")
            else:
                self._pct(93)

            self._stage(L("Installing GRUB…", "Instalando GRUB…"))
            self._chroot("grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB")
            self._chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self._pct(100)
            self._stage(L("Done!", "Listo!"))
            self.on_done(True, "")
        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))


def run_gui():
    import urwid

    PALETTE = [
        ("bg",          "light gray",    "dark blue"),
        ("header",      "white",         "dark blue",      "bold"),
        ("header_acc",  "light cyan",    "dark blue",      "bold"),
        ("sidebar_bg",  "light gray",    "black"),
        ("sidebar_dim", "dark gray",     "black"),
        ("sidebar_act", "white",         "dark blue",      "bold"),
        ("sidebar_done","light green",   "black"),
        ("body",        "light gray",    "dark gray"),
        ("body_dark",   "light gray",    "black"),
        ("btn",         "black",         "light cyan",     "bold"),
        ("btn_focus",   "white",         "dark cyan",      "bold"),
        ("btn_danger",  "white",         "dark red",       "bold"),
        ("btn_danger_f","white",         "light red",      "bold"),
        ("btn_ghost",   "dark gray",     "black"),
        ("btn_ghost_f", "light gray",    "dark gray"),
        ("title",       "white",         "dark gray",      "bold"),
        ("subtitle",    "dark gray",     "dark gray"),
        ("label",       "dark gray",     "dark gray"),
        ("entry",       "white",         "black"),
        ("entry_focus", "white",         "dark blue"),
        ("card",        "light gray",    "black"),
        ("card_focus",  "white",         "dark blue"),
        ("card_sel",    "light cyan",    "dark blue",      "bold"),
        ("error",       "light red",     "dark gray"),
        ("success",     "light green",   "dark gray",      "bold"),
        ("warn",        "yellow",        "dark gray"),
        ("warn_box",    "light red",     "dark red"),
        ("progress_bg", "dark gray",     "dark gray"),
        ("progress_fg", "white",         "dark cyan"),
        ("log_dim",     "dark gray",     "black"),
        ("log_err",     "light red",     "black"),
        ("log_ok",      "light green",   "black"),
        ("log_warn",    "yellow",        "black"),
        ("divider",     "dark blue",     "dark blue"),
        ("pct",         "light cyan",    "dark gray",      "bold"),
        ("stage",       "white",         "dark gray",      "bold"),
        ("rev_key",     "dark gray",     "black"),
        ("rev_ok",      "light green",   "black",          "bold"),
        ("rev_def",     "yellow",        "black",          "bold"),
        ("rev_miss",    "light red",     "black",          "bold"),
    ]

    STEP_NAMES_EN = ["Language","Identity","Passwords","Disk","Keymap",
                     "Timezone","Desktop","GPU","Review","Install"]
    STEP_NAMES_ES = ["Idioma","Identidad","Contrasenas","Disco","Teclado",
                     "Zona horaria","Escritorio","GPU","Revision","Instalar"]

    loop   = None
    screen = None

    def set_loop(l, s):
        nonlocal loop, screen
        loop   = l
        screen = s

    def redraw():
        if loop:
            loop.draw_screen()

    class SelButton(urwid.WidgetWrap):
        signals = ["click"]
        def __init__(self, label, desc="", value=None, selected=False):
            self.value    = value
            self._selected = selected
            self._label   = label
            self._desc    = desc
            self._rebuild()
            super().__init__(self._pile)

        def _rebuild(self):
            attr   = "card_sel" if self._selected else "card"
            marker = ("card_sel", "◉ ") if self._selected else ("card", "○ ")
            row1   = urwid.Text([marker, ("card_sel" if self._selected else "card", self._label)])
            widgets = [row1]
            if self._desc:
                widgets.append(urwid.Text([("label", f"   {self._desc}")]))
            self._pile = urwid.AttrMap(
                urwid.Padding(urwid.Pile(widgets), left=1, right=1),
                "card", focus_map="card_focus"
            )
            if hasattr(self, "_w"):
                self._w = self._pile

        def set_selected(self, v):
            self._selected = v
            self._rebuild()

        def keypress(self, size, key):
            if key in (" ", "enter"):
                self._emit("click", self.value)
                return
            return key

        def mouse_event(self, size, event, button, col, row, focus):
            if event == "mouse press" and button == 1:
                self._emit("click", self.value)
                return True
            return False

        def selectable(self):
            return True

        def rows(self, size, focus=False):
            return 2 if self._desc else 1

    class RadioGroup:
        def __init__(self):
            self.buttons = []
            self._value  = None

        def add(self, label, desc, value, default=False):
            btn = SelButton(label, desc, value, selected=default)
            if default:
                self._value = value
            urwid.connect_signal(btn, "click", self._on_click)
            self.buttons.append(btn)
            return btn

        def _on_click(self, btn, value):
            self._value = value
            for b in self.buttons:
                b.set_selected(b.value == value)
            redraw()

        def get(self):
            return self._value

        def set(self, value):
            self._value = value
            for b in self.buttons:
                b.set_selected(b.value == value)

    class App:
        def __init__(self):
            self._step         = 0
            self._total_steps  = len(STEP_NAMES_EN)
            self._sidebar_rows = []
            self._err_text     = urwid.Text("")
            self._pages        = []
            self._page_data    = {}

            self._header  = self._build_header()
            self._sidebar = self._build_sidebar()
            self._footer  = self._build_footer()

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

            self._page_pile = urwid.WidgetPlaceholder(self._pages[0])

            body = urwid.Columns([
                ("fixed", 22, urwid.AttrMap(urwid.Filler(self._sidebar, valign="top"), "sidebar_bg")),
                ("fixed", 1,  urwid.AttrMap(urwid.SolidFill("│"), "divider")),
                self._page_pile,
            ])

            frame = urwid.Frame(
                body,
                header=self._header,
                footer=self._footer,
            )
            self._frame = frame
            self._go_to(0)

        def widget(self):
            return self._frame

        def _build_header(self):
            left  = urwid.Text([("header_acc", "❱ "), ("header", "Arch Linux Installer")])
            right = urwid.Text([("header", VERSION)], align="right")
            cols  = urwid.Columns([left, right])
            return urwid.AttrMap(urwid.Padding(cols, left=1, right=1), "header")

        def _build_sidebar(self):
            names = STEP_NAMES_ES if state["lang"] == "es" else STEP_NAMES_EN
            title = urwid.Text([("header_acc", "\n  Steps\n")], align="left")
            self._sidebar_rows = []
            widgets = [title]
            for i, name in enumerate(names):
                t = urwid.Text(f"  {i+1:02d}  {name}")
                self._sidebar_rows.append(t)
                widgets.append(t)
            return urwid.Pile(widgets)

        def _refresh_sidebar(self):
            names = STEP_NAMES_ES if state["lang"] == "es" else STEP_NAMES_EN
            for i, row in enumerate(self._sidebar_rows):
                if i < self._step:
                    row.set_text([("sidebar_done", f"  ✔   {names[i]}")])
                elif i == self._step:
                    row.set_text([("sidebar_act",  f"  ▶   {names[i]}")])
                else:
                    row.set_text([("sidebar_dim",  f"  {i+1:02d}  {names[i]}")])

        def _build_footer(self):
            self._btn_back = urwid.Button("← Back")
            self._btn_next = urwid.Button("Next →")
            urwid.connect_signal(self._btn_back, "click", lambda _: self._go_back())
            urwid.connect_signal(self._btn_next, "click", lambda _: self._go_next())
            back_w = urwid.AttrMap(self._btn_back, "btn_ghost", focus_map="btn_ghost_f")
            next_w = urwid.AttrMap(self._btn_next, "btn",       focus_map="btn_focus")
            self._btn_next_map = next_w
            row = urwid.Columns([
                ("fixed", 12, back_w),
                urwid.AttrMap(urwid.Padding(self._err_text, left=2), "error"),
                ("fixed", 12, next_w),
            ])
            return urwid.AttrMap(urwid.Padding(row, left=1, right=1), "sidebar_bg")

        def _set_err(self, msg):
            self._err_text.set_text([("error", msg)])

        def _go_to(self, idx):
            self._step = idx
            self._page_pile.original_widget = self._pages[idx]
            self._refresh_sidebar()
            self._set_err("")
            is_install = (idx == self._total_steps - 1)
            self._btn_back.set_label(L("← Back", "← Volver") if not is_install else "")
            self._btn_next.set_label(L("Next →", "Siguiente →") if not is_install else "")
            self._frame.set_focus("body")
            if idx == self._total_steps - 2:
                self._refresh_review()
                self._btn_next.set_label(L("Install Now →", "Instalar ahora →"))
                self._btn_next_map.attr_map    = {None: "btn_danger"}
                self._btn_next_map.focus_map   = {None: "btn_danger_f"}
            else:
                self._btn_next_map.attr_map    = {None: "btn"}
                self._btn_next_map.focus_map   = {None: "btn_focus"}
            if idx == self._total_steps - 1:
                self._start_install()

        def _go_next(self):
            validators = [
                self._val_language, self._val_identity, self._val_passwords,
                self._val_disk,     self._val_keymap,   self._val_timezone,
                self._val_desktop,  self._val_gpu,
                self._val_review,   lambda: True,
            ]
            if self._step < len(validators) and validators[self._step]():
                if self._step < self._total_steps - 1:
                    self._go_to(self._step + 1)

        def _go_back(self):
            if self._step > 0:
                self._go_to(self._step - 1)

        def _scrollable(self, widgets):
            body = urwid.SimpleFocusListWalker(widgets)
            return urwid.ListBox(body)

        def _page_header(self, title, subtitle=None):
            widgets = [
                urwid.Text([("title", f"\n  {title}")]),
                urwid.Text([("subtitle", f"  {subtitle}\n")]) if subtitle else urwid.Divider(),
                urwid.AttrMap(urwid.Divider("─"), "divider"),
                urwid.Divider(),
            ]
            return widgets

        def _page_language(self):
            g = RadioGroup()
            g.add("🇬🇧  English", "", "en", default=(state["lang"] == "en"))
            g.add("🇪🇸  Español", "", "es", default=(state["lang"] == "es"))
            self._lang_grp = g
            w = self._page_header("Language / Idioma", "Choose the installer language") + \
                [urwid.Padding(b, left=2, right=2) for b in g.buttons]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_language(self):
            state["lang"] = self._lang_grp.get() or "en"
            self._refresh_sidebar()
            return True

        def _page_identity(self):
            self._e_hostname = urwid.Edit(caption="  ")
            self._e_hostname.set_edit_text(state.get("hostname", ""))
            self._e_username = urwid.Edit(caption="  ")
            self._e_username.set_edit_text(state.get("username", ""))
            self._err_id = urwid.Text("")
            w = self._page_header(
                L("System Identity", "Identidad del sistema"),
                L("Letters, digits, - or _  ·  max 32 chars",
                  "Letras, digitos, - o _  ·  max 32 caracteres")
            ) + [
                urwid.AttrMap(self._err_id, "error"),
                urwid.Text([("label", "  Hostname")]),
                urwid.AttrMap(self._e_hostname, "entry", focus_map="entry_focus"),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('Username','Usuario')}")]),
                urwid.AttrMap(self._e_username, "entry", focus_map="entry_focus"),
            ]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_identity(self):
            hn = self._e_hostname.get_edit_text().strip()
            un = self._e_username.get_edit_text().strip()
            if not validate_name(hn):
                self._err_id.set_text(L("  ✗  Invalid hostname.", "  ✗  Hostname invalido."))
                return False
            if not validate_name(un):
                self._err_id.set_text(L("  ✗  Invalid username.", "  ✗  Usuario invalido."))
                return False
            state["hostname"] = hn
            state["username"] = un
            self._err_id.set_text("")
            return True

        def _page_passwords(self):
            self._e_rp1 = urwid.Edit(caption="  ", mask="•")
            self._e_rp2 = urwid.Edit(caption="  ", mask="•")
            self._e_up1 = urwid.Edit(caption="  ", mask="•")
            self._e_up2 = urwid.Edit(caption="  ", mask="•")
            self._err_pw = urwid.Text("")
            w = self._page_header(L("Passwords", "Contrasenas")) + [
                urwid.AttrMap(self._err_pw, "error"),
                urwid.Text([("label", f"  ── {L('Root account','Cuenta root')} ──")]),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('Root password','Contrasena root')}")]),
                urwid.AttrMap(self._e_rp1, "entry", focus_map="entry_focus"),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('Confirm root','Confirmar root')}")]),
                urwid.AttrMap(self._e_rp2, "entry", focus_map="entry_focus"),
                urwid.Divider(),
                urwid.AttrMap(urwid.Divider("─"), "divider"),
                urwid.Divider(),
                urwid.Text([("label", f"  ── {L('User account','Cuenta de usuario')} ──")]),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('User password','Contrasena de usuario')}")]),
                urwid.AttrMap(self._e_up1, "entry", focus_map="entry_focus"),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('Confirm user','Confirmar usuario')}")]),
                urwid.AttrMap(self._e_up2, "entry", focus_map="entry_focus"),
            ]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_passwords(self):
            rp1, rp2 = self._e_rp1.get_edit_text(), self._e_rp2.get_edit_text()
            up1, up2 = self._e_up1.get_edit_text(), self._e_up2.get_edit_text()
            if not rp1:
                self._err_pw.set_text(L("  ✗  Root password empty.", "  ✗  Contrasena root vacia.")); return False
            if rp1 != rp2:
                self._err_pw.set_text(L("  ✗  Root passwords do not match.", "  ✗  Contrasenas root no coinciden.")); return False
            if not up1:
                self._err_pw.set_text(L("  ✗  User password empty.", "  ✗  Contrasena usuario vacia.")); return False
            if up1 != up2:
                self._err_pw.set_text(L("  ✗  User passwords do not match.", "  ✗  Contrasenas usuario no coinciden.")); return False
            state["root_pass"] = rp1
            state["user_pass"] = up1
            self._err_pw.set_text("")
            return True

        def _page_disk(self):
            self._disk_grp  = RadioGroup()
            self._err_disk  = urwid.Text("")
            disks = list_disks()
            disk_widgets = []
            if not disks:
                disk_widgets.append(urwid.Text([("error", "  ✗  No disks found.")]))
            else:
                for name, size_gb, model in disks:
                    self._disk_grp.add(
                        f"/dev/{name}   {size_gb} GB",
                        model or "Unknown",
                        name,
                        default=(name == state.get("disk") or (not state.get("disk") and not self._disk_grp.buttons)),
                    )
                disk_widgets = [urwid.Padding(b, left=2, right=2) for b in self._disk_grp.buttons]

            self._swap_edit = urwid.Edit(caption="  Swap (GB): ", edit_text=state["swap"])
            w = self._page_header(
                L("Disk & Swap", "Disco y Swap"),
                L("⚠  ALL DATA on selected disk will be ERASED",
                  "⚠  SE BORRARAN TODOS LOS DATOS del disco seleccionado")
            ) + [urwid.AttrMap(self._err_disk, "error")] + disk_widgets + [
                urwid.Divider(),
                urwid.AttrMap(urwid.Divider("─"), "divider"),
                urwid.Divider(),
                urwid.Text([("label", f"  {L('Swap partition size','Tamano del swap')}")]),
                urwid.AttrMap(self._swap_edit, "entry", focus_map="entry_focus"),
            ]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_disk(self):
            val = self._disk_grp.get()
            if not val:
                self._err_disk.set_text(L("  ✗  Select a disk.", "  ✗  Selecciona un disco.")); return False
            swap = self._swap_edit.get_edit_text().strip()
            if not validate_swap(swap):
                self._err_disk.set_text(L("  ✗  Swap must be 1–128 GB.", "  ✗  Swap debe ser 1–128 GB.")); return False
            state["disk"] = val
            state["swap"] = swap
            self._err_disk.set_text("")
            return True

        def _page_keymap(self):
            try:
                out   = subprocess.check_output("localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
                maps  = [l for l in out.splitlines() if l]
            except Exception:
                maps  = []
            wanted  = ["us","es","uk","fr","de","it","ru","ara","pt-latin9","br-abnt2"]
            options = [m for m in wanted if m in maps] if maps else wanted
            self._km_grp = RadioGroup()
            for km in options:
                self._km_grp.add(km, "", km, default=(km == state["keymap"]))
            w = self._page_header(L("Keyboard Layout", "Distribucion de teclado")) + \
                [urwid.Padding(b, left=2, right=2) for b in self._km_grp.buttons]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_keymap(self):
            val = self._km_grp.get()
            if val:
                state["keymap"] = val
                run_simple(f"loadkeys {shlex.quote(val)}", ignore_error=True)
            return True

        def _page_timezone(self):
            try:
                out   = subprocess.check_output("timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
                zones = [l for l in out.splitlines() if l]
            except Exception:
                zones = []
            if not zones:
                zones = ["UTC","Europe/Madrid","Europe/London","America/New_York","America/Los_Angeles","Asia/Tokyo"]
            self._all_zones  = zones
            regions = ["UTC"] + sorted(set(z.split("/")[0] for z in zones if "/" in z))

            self._tz_sel_lbl = urwid.Text([("success", f"  Selected: {state['timezone']}")])
            self._tz_search  = urwid.Edit(caption="  🔍 ", edit_text="")
            self._region_grp = RadioGroup()
            self._city_grp   = RadioGroup()
            self._region_col = urwid.SimpleFocusListWalker([])
            self._city_col   = urwid.SimpleFocusListWalker([])

            cur_region = state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC"

            def fill_regions(q=""):
                self._region_col[:] = []
                self._region_grp.buttons.clear()
                self._region_grp._value = None
                for r in regions:
                    if q.lower() in r.lower():
                        btn = self._region_grp.add(r, "", r, default=(r == cur_region))
                        self._region_col.append(urwid.Padding(btn, left=1))

            def fill_cities(region):
                self._city_col[:] = []
                self._city_grp.buttons.clear()
                self._city_grp._value = None
                cur_city = state["timezone"].split("/", 1)[1] if "/" in state["timezone"] else ""
                if region == "UTC":
                    btn = self._city_grp.add("UTC", "", "UTC", default=True)
                    self._city_col.append(urwid.Padding(btn, left=1))
                    state["timezone"] = "UTC"
                    self._tz_sel_lbl.set_text([("success", "  Selected: UTC")])
                    return
                for z in zones:
                    if z.startswith(region + "/"):
                        city = z.split("/", 1)[1]
                        btn  = self._city_grp.add(city, "", city, default=(city == cur_city))
                        self._city_col.append(urwid.Padding(btn, left=1))

            def on_region_click(btn, region):
                fill_cities(region)
                redraw()

            def on_city_click(btn, city):
                region = self._region_grp.get() or "UTC"
                tz = "UTC" if region == "UTC" else f"{region}/{city}"
                state["timezone"] = tz
                self._tz_sel_lbl.set_text([("success", f"  Selected: {tz}")])
                redraw()

            def on_search_change(edit, text):
                q = text.strip()
                fill_regions(q)
                if q:
                    self._city_col[:] = []
                    self._city_grp.buttons.clear()
                    for z in zones:
                        if q.lower() in z.lower():
                            city = z
                            btn  = self._city_grp.add(z, "", z, default=False)
                            self._city_col.append(urwid.Padding(btn, left=1))
                redraw()

            self._orig_region_on_click = on_region_click
            self._orig_city_on_click   = on_city_click
            for btn in self._region_grp.buttons:
                urwid.connect_signal(btn, "click", on_region_click)
            def _patched_region_add(label, desc, value, default=False):
                btn = self._region_grp.__class__.add(self._region_grp, label, desc, value, default)
                urwid.connect_signal(btn, "click", on_region_click)
                return btn
            self._tz_fill_cities   = fill_cities
            self._tz_fill_regions  = fill_regions
            self._region_grp.add   = lambda l, d, v, default=False: self._tz_region_add(l, d, v, default, fill_cities, on_region_click)
            self._city_grp.add     = lambda l, d, v, default=False: self._tz_city_add(l, d, v, default, on_city_click)

            fill_regions()
            fill_cities(cur_region)

            urwid.connect_signal(self._tz_search, "postchange", on_search_change)

            cols = urwid.Columns([
                ("weight", 2, urwid.ListBox(self._region_col)),
                ("fixed",  1, urwid.AttrMap(urwid.SolidFill("│"), "divider")),
                ("weight", 3, urwid.ListBox(self._city_col)),
            ])
            frame_body = urwid.Frame(
                cols,
                header=urwid.Pile(
                    self._page_header(L("Timezone", "Zona horaria")) + [
                        urwid.AttrMap(urwid.Padding(self._tz_search, left=1, right=1), "entry", focus_map="entry_focus"),
                        urwid.Divider(),
                        self._tz_sel_lbl,
                        urwid.AttrMap(urwid.Divider("─"), "divider"),
                    ]
                )
            )
            return urwid.AttrMap(frame_body, "body_dark")

        def _tz_region_add(self, label, desc, value, default, fill_cities_fn, on_click_fn):
            btn = SelButton(label, desc, value, selected=default)
            if default:
                self._region_grp._value = value
            def _on(b, v):
                self._region_grp._value = v
                for b2 in self._region_grp.buttons:
                    b2.set_selected(b2.value == v)
                fill_cities_fn(v)
                redraw()
            urwid.connect_signal(btn, "click", _on)
            self._region_grp.buttons.append(btn)
            return btn

        def _tz_city_add(self, label, desc, value, default, on_click_fn):
            btn = SelButton(label, desc, value, selected=default)
            if default:
                self._city_grp._value = value
            def _on(b, v):
                self._city_grp._value = v
                for b2 in self._city_grp.buttons:
                    b2.set_selected(b2.value == v)
                region = self._region_grp.get() or "UTC"
                tz = "UTC" if region == "UTC" else f"{region}/{v}"
                state["timezone"] = tz
                self._tz_sel_lbl.set_text([("success", f"  Selected: {tz}")])
                redraw()
            urwid.connect_signal(btn, "click", _on)
            self._city_grp.buttons.append(btn)
            return btn

        def _val_timezone(self):
            return True

        def _page_desktop(self):
            self._de_grp = RadioGroup()
            opts = [
                ("KDE Plasma",  L("Full KDE + Konsole + Dolphin + Firefox + SDDM",
                                   "KDE completo + Konsole + Dolphin + Firefox + SDDM")),
                ("Cinnamon",    L("Classic Cinnamon + Alacritty + Firefox + LightDM",
                                   "Cinnamon clasico + Alacritty + Firefox + LightDM")),
                ("None",        L("No desktop — CLI only",
                                   "Sin escritorio — solo consola")),
            ]
            for val, desc in opts:
                self._de_grp.add(val, desc, val, default=(val == state["desktop"]))
            w = self._page_header(
                L("Desktop Environment", "Entorno de escritorio"),
                L("Choose a desktop or go headless.", "Elige un escritorio o instala sin entorno grafico.")
            ) + [urwid.Padding(b, left=2, right=2) for b in self._de_grp.buttons]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_desktop(self):
            val = self._de_grp.get()
            if val: state["desktop"] = val
            return True

        def _page_gpu(self):
            self._gpu_grp = RadioGroup()
            opts = [
                ("NVIDIA",    L("Proprietary drivers  (nvidia + nvidia-utils)",
                                 "Drivers propietarios  (nvidia + nvidia-utils)")),
                ("AMD/Intel", L("Open-source Mesa  (mesa + vulkan-radeon)",
                                 "Mesa open-source  (mesa + vulkan-radeon)")),
                ("None",      L("No extra GPU drivers", "Sin drivers adicionales")),
            ]
            for val, desc in opts:
                self._gpu_grp.add(val, desc, val, default=(val == state["gpu"]))
            w = self._page_header(L("GPU Drivers", "Drivers GPU"),
                                   L("Select your GPU vendor.", "Selecciona tu GPU.")) + \
                [urwid.Padding(b, left=2, right=2) for b in self._gpu_grp.buttons]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _val_gpu(self):
            val = self._gpu_grp.get()
            if val: state["gpu"] = val
            return True

        def _page_review(self):
            self._review_pile = urwid.Pile([urwid.Text("")])
            w = self._page_header(
                L("Review & Confirm", "Revisar y confirmar"),
                L("Check everything before installing.", "Revisa todo antes de instalar.")
            ) + [self._review_pile]
            return urwid.AttrMap(self._scrollable(w), "body_dark")

        def _refresh_review(self):
            DEFAULTS = {"swap":"8","desktop":"None","gpu":"None","keymap":"us","timezone":"UTC"}
            rows_data = [
                (L("Language","Idioma"),   state["lang"],                                        "lang"),
                ("Hostname",               state["hostname"] or "—",                             "hostname"),
                (L("Username","Usuario"),  state["username"] or "—",                             "username"),
                ("Disk",                   f"/dev/{state['disk']}" if state["disk"] else "NOT SET","disk"),
                ("Swap",                   f"{state['swap']} GB",                                "swap"),
                ("Keymap",                 state["keymap"],                                      "keymap"),
                ("Timezone",               state["timezone"],                                     "timezone"),
                ("Desktop",                state["desktop"],                                     "desktop"),
                ("GPU",                    state["gpu"],                                          "gpu"),
            ]
            widgets = []
            for key, val, k in rows_data:
                raw = state.get(k)
                if not raw:
                    vc = "rev_miss"
                elif DEFAULTS.get(k) == raw:
                    vc = "rev_def"
                else:
                    vc = "rev_ok"
                widgets.append(urwid.Columns([
                    ("fixed", 18, urwid.Text([("rev_key", f"  {key}")])),
                    urwid.Text([(vc, val)]),
                ]))
                widgets.append(urwid.Divider())

            missing = []
            if not state["hostname"]:  missing.append("hostname")
            if not state["username"]:  missing.append("username")
            if not state["disk"]:      missing.append("disk")
            if not state["root_pass"]: missing.append(L("root password", "contrasena root"))
            self._review_missing = missing

            widgets.append(urwid.AttrMap(urwid.Divider("─"), "divider"))
            widgets.append(urwid.Divider())
            widgets.append(urwid.Text([("label", "  Yellow = default  ·  Green = custom  ·  Red = missing")]))
            widgets.append(urwid.Divider())
            if missing:
                widgets.append(urwid.Text([("error",
                    L(f"  ✗  Missing: {', '.join(missing)}", f"  ✗  Falta: {', '.join(missing)}")
                )]))
            else:
                widgets.append(urwid.Text([("success",
                    L("  ✔  All good — ready to install!", "  ✔  Todo listo — listo para instalar!")
                )]))
            widgets.append(urwid.Divider())
            widgets.append(urwid.AttrMap(
                urwid.Text(L(f"  ⚠  THIS WILL PERMANENTLY ERASE  /dev/{state['disk'] or '???'}",
                              f"  ⚠  ESTO BORRARA PERMANENTEMENTE  /dev/{state['disk'] or '???'}")),
                "warn_box"
            ))
            self._review_pile.contents = [(w, ("weight",1)) for w in widgets]

        def _val_review(self):
            if self._review_missing:
                self._set_err(L("Go back and fill in the missing fields.",
                                 "Vuelve y completa los campos que faltan."))
                return False
            self._confirm_shown = False
            self._show_confirm_dialog()
            return self._confirm_result

        def _show_confirm_dialog(self):
            self._confirm_result = False
            result = [False]
            done   = threading.Event()

            btn_yes = urwid.Button(L("Yes, install", "Si, instalar"))
            btn_no  = urwid.Button(L("Cancel", "Cancelar"))
            yes_w   = urwid.AttrMap(btn_yes, "btn_danger", focus_map="btn_danger_f")
            no_w    = urwid.AttrMap(btn_no,  "btn_ghost",  focus_map="btn_ghost_f")

            def on_yes(_):
                result[0] = True
                loop.widget = self._frame
                redraw()
                done.set()

            def on_no(_):
                result[0] = False
                loop.widget = self._frame
                redraw()
                done.set()

            urwid.connect_signal(btn_yes, "click", on_yes)
            urwid.connect_signal(btn_no,  "click", on_no)

            body = urwid.LineBox(urwid.Pile([
                urwid.Text([("warn", L("\n  ⚠  LAST WARNING\n","\n  ⚠  ULTIMO AVISO\n"))], align="center"),
                urwid.Text([("label", L(
                    f"  All data on /dev/{state['disk']} will be\n  PERMANENTLY ERASED.\n\n  Continue?",
                    f"  Todos los datos en /dev/{state['disk']} seran\n  BORRADOS PERMANENTEMENTE.\n\n  ¿Continuar?"
                ))]),
                urwid.Divider(),
                urwid.Columns([
                    ("weight",1, urwid.Padding(no_w,  left=2, right=1)),
                    ("weight",1, urwid.Padding(yes_w, left=1, right=2)),
                ], dividechars=1),
                urwid.Divider(),
            ]))

            overlay = urwid.Overlay(
                urwid.AttrMap(body, "body_dark"),
                self._frame,
                align="center", valign="middle",
                width=52, height=14,
            )
            loop.widget = overlay
            redraw()

            while not done.is_set():
                loop.screen.get_input()
                for key in loop.screen.get_available_raw_input():
                    pass
                loop.draw_screen()

            self._confirm_result = result[0]

        def _page_install(self):
            self._stage_text    = urwid.Text([("stage", L("  Preparing…", "  Preparando…"))])
            self._pct_text      = urwid.Text([("pct",   "  0%")])
            self._bar_text      = urwid.Text("  " + "░" * 50)
            self._elapsed_text  = urwid.Text([("label", "  Elapsed: 00:00")])
            self._log_walker    = urwid.SimpleFocusListWalker([])
            self._log_box       = urwid.ListBox(self._log_walker)

            top = urwid.Pile([
                urwid.Text([("title", "\n  " + L("Installing Arch Linux…","Instalando Arch Linux…"))]),
                urwid.Divider(),
                self._stage_text,
                urwid.Divider(),
                urwid.Columns([("fixed",6, self._pct_text), self._bar_text]),
                self._elapsed_text,
                urwid.Divider(),
                urwid.AttrMap(urwid.Divider("─"), "divider"),
                urwid.Text([("label", f"  {L('Live log','Log en vivo')}\n")]),
            ])
            frame = urwid.Frame(
                urwid.AttrMap(self._log_box, "body_dark"),
                header=urwid.AttrMap(top, "body_dark"),
            )
            self._install_start = None
            return urwid.AttrMap(frame, "body_dark")

        def _start_install(self):
            self._install_start = time.time()
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
            if self._install_start is None:
                return
            secs = int(time.time() - self._install_start)
            self._elapsed_text.set_text([("label", f"  Elapsed: {secs//60:02d}:{secs%60:02d}")])
            if loop:
                loop.set_alarm_in(1.0, lambda _l, _d: self._tick_elapsed())

        def _on_progress(self, pct):
            filled = int(pct / 2)
            bar    = ("progress_fg", "█" * filled) , ("progress_bg", "░" * (50 - filled))
            self._bar_text.set_text(["  ", bar[0], bar[1]])
            self._pct_text.set_text([("pct", f"  {int(pct)}%")])
            if loop: loop.set_alarm_in(0, lambda *_: redraw())

        def _on_stage(self, msg):
            self._stage_text.set_text([("stage", f"  {msg}")])
            if loop: loop.set_alarm_in(0, lambda *_: redraw())

        def _on_log(self, line):
            ll  = line.lower()
            attr = "log_dim"
            if any(w in ll for w in ("error","fail","fatal","✗")):
                attr = "log_err"
            elif any(w in ll for w in ("warning","warn")):
                attr = "log_warn"
            elif any(w in ll for w in ("complete","success","✔")):
                attr = "log_ok"
            self._log_walker.append(urwid.Text([(attr, f"  {line}")]))
            self._log_box.set_focus(len(self._log_walker) - 1)
            if loop: loop.set_alarm_in(0, lambda *_: redraw())

        def _on_done(self, success, reason):
            self._install_start = None

            def show(*_):
                icon = "✔" if success else "✗"
                ic   = "success" if success else "error"
                msg  = L("Installation complete!","Instalacion completa!") if success \
                       else L("Installation failed.","Instalacion fallida.")
                sub  = L("Remove install media and reboot.", "Extrae el medio y reinicia.") if success \
                       else f"{L('Check','Revisa')} {LOG_FILE}"
                widgets = [
                    urwid.Text([(ic, f"\n\n  {icon}  {msg}")]),
                    urwid.Text([("label", f"\n  {sub}\n")]),
                ]
                if success:
                    btn_r = urwid.Button(L("Reboot now", "Reiniciar ahora"))
                    urwid.connect_signal(btn_r, "click", lambda _: (
                        subprocess.run("umount -R /mnt", shell=True),
                        subprocess.run("reboot", shell=True),
                        sys.exit(0)
                    ))
                    widgets.append(urwid.Padding(urwid.AttrMap(btn_r, "btn", focus_map="btn_focus"), left=2, width=20))
                btn_sh = urwid.Button(L("Exit to shell", "Salir al shell"))
                urwid.connect_signal(btn_sh, "click", lambda _: sys.exit(0 if success else 1))
                widgets.append(urwid.Padding(urwid.AttrMap(btn_sh, "btn_ghost", focus_map="btn_ghost_f"), left=2, width=20))

                self._pages[-1].original_widget.contents["body"] = (
                    urwid.AttrMap(self._scrollable(widgets), "body_dark"), None
                )
                redraw()

            if loop:
                loop.set_alarm_in(0, show)

    app = App()

    def unhandled(key):
        if key in ("q", "Q", "esc"):
            raise urwid.ExitMainLoop()

    scr = urwid.raw_display.Screen()
    scr.set_mouse_tracking(True)
    lp  = urwid.MainLoop(
        app.widget(),
        palette=PALETTE,
        screen=scr,
        unhandled_input=unhandled,
    )
    set_loop(lp, scr)
    lp.run()


def bootstrap():
    if os.geteuid() != 0:
        print("This installer must be run as root.")
        sys.exit(1)

    needs_urwid = False
    try:
        import urwid
    except ImportError:
        needs_urwid = True

    if needs_urwid:
        print("[*] Installing python-urwid (~1 MB, no X11 required)…")
        rc = subprocess.call("pacman -Sy --noconfirm python-urwid",
                             shell=True, executable="/bin/bash")
        if rc != 0:
            print("[!] Failed to install python-urwid. Check your network and try again.")
            sys.exit(1)
        print("[+] Done.\n")

    run_gui()


if __name__ == "__main__":
    bootstrap()
