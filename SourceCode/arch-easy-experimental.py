import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

VERSION  = "V1.3.0-stable"
LOG_FILE = "/mnt/install_log.txt"
TITLE    = "Arch Linux Installer"

state = {
    "lang":       "en",
    "hostname":   "",
    "username":   "",
    "root_pass":  "",
    "user_pass":  "",
    "swap":       "8",
    "disk":       None,
    "desktop":    "None",
    "gpu":        "None",
    "keymap":     "us",
    "timezone":   "UTC",
    "filesystem": "ext4",
    "kernel":     "linux",
    "mirrors":    True,
    "quick":      False,
}

DESKTOP_PKGS = {
    "KDE Plasma": [
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
        "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm",
    ],
    "GNOME": [
        "gnome gdm firefox",
    ],
    "Cinnamon": [
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
        "cinnamon lightdm lightdm-gtk-greeter alacritty firefox",
    ],
    "XFCE": [
        "xorg-server xfce4 xfce4-goodies lightdm lightdm-gtk-greeter alacritty firefox",
    ],
    "MATE": [
        "xorg-server mate mate-extra lightdm lightdm-gtk-greeter alacritty firefox",
    ],
    "LXQt": [
        "xorg-server lxqt sddm breeze-icons alacritty firefox",
    ],
}

DESKTOP_DM = {
    "KDE Plasma": "sddm",
    "GNOME":      "gdm",
    "Cinnamon":   "lightdm",
    "XFCE":       "lightdm",
    "MATE":       "lightdm",
    "LXQt":       "sddm",
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

def dlg_titled(title, *args):
    cmd = [
        "dialog",
        "--colors",
        "--backtitle", f"\\Zb\\Z4{TITLE}\\Zn  —  {VERSION}",
        "--title", f" {title} ",
    ]
    cmd.extend(args)
    result = subprocess.run(cmd, stderr=subprocess.PIPE, text=True)
    return result.returncode, result.stderr.strip()

def msgbox(title, text):
    dlg_titled(title, "--msgbox", text, "0", "0")

def yesno(title, text):
    rc, _ = dlg_titled(title, "--yesno", text, "0", "0")
    return rc == 0

def inputbox(title, text, init=""):
    rc, val = dlg_titled(title, "--inputbox", text, "0", "60", init)
    if rc != 0:
        return None
    return val

def passwordbox(title, text):
    rc, val = dlg_titled(title, "--insecure", "--passwordbox", text, "0", "60")
    if rc != 0:
        return None
    return val

def menu(title, text, items):
    flat = []
    for tag, desc in items:
        flat.extend([tag, desc])
    height = min(len(items) + 8, 30)
    rc, val = dlg_titled(title, "--menu", text, str(height), "72", str(len(items)), *flat)
    if rc != 0:
        return None
    return val

def radiolist(title, text, items, default=None):
    flat = []
    for tag, desc in items:
        status = "on" if tag == default else "off"
        flat.extend([tag, desc, status])
    height = min(len(items) + 8, 30)
    rc, val = dlg_titled(title, "--radiolist", text, str(height), "72", str(len(items)), *flat)
    if rc != 0:
        return None
    return val

def gauge_open(title, text, pct=0):
    proc = subprocess.Popen(
        [
            "dialog", "--colors",
            "--backtitle", f"\\Zb\\Z4{TITLE}\\Zn  —  {VERSION}",
            "--title", f" {title} ",
            "--gauge", text, "8", "72", str(pct),
        ],
        stdin=subprocess.PIPE,
        text=True,
    )
    return proc

def gauge_update(proc, pct, message):
    try:
        proc.stdin.write(f"XXX\n{int(pct)}\n{message}\nXXX\n")
        proc.stdin.flush()
    except Exception:
        pass

def validate_name(n):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", n or ""))

def validate_swap(s):
    return bool(re.match(r"^\d+$", s or "")) and 1 <= int(s) <= 128

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

def detect_gpu():
    try:
        out = subprocess.check_output(
            "lspci 2>/dev/null | grep -iE 'vga|3d|display'",
            shell=True, text=True, stderr=subprocess.DEVNULL
        ).lower()
        if "nvidia" in out:
            return "NVIDIA"
        if "amd" in out or "radeon" in out or "intel" in out:
            return "AMD/Intel"
    except Exception:
        pass
    return "None"

def detect_cpu():
    try:
        out = subprocess.check_output("lscpu 2>/dev/null", shell=True, text=True)
        if "GenuineIntel" in out:
            return "intel-ucode"
        if "AuthenticAMD" in out:
            return "amd-ucode"
    except Exception:
        pass
    return None

def _wifi_interfaces():
    try:
        out = subprocess.check_output("ls /sys/class/net/", shell=True, text=True)
        return [i for i in out.split() if i.startswith(("wlan", "wlp", "wlo"))]
    except Exception:
        return []

def screen_wifi_connect():
    ifaces = _wifi_interfaces()
    if not ifaces:
        msgbox(
            L("WiFi", "WiFi"),
            L("No wireless interfaces found.", "No se encontraron interfaces inalámbricas.")
        )
        return False

    iface = ifaces[0]
    subprocess.call(
        f"iwctl station {shlex.quote(iface)} scan",
        shell=True, stderr=subprocess.DEVNULL
    )
    time.sleep(3)

    try:
        raw = subprocess.check_output(
            f"iwctl station {shlex.quote(iface)} get-networks 2>/dev/null",
            shell=True, text=True
        )
        ssids = []
        for line in raw.splitlines()[4:]:
            line = line.strip().lstrip("> ").strip()
            parts = line.split()
            if parts and not parts[0].startswith("-") and parts[0] != "Network":
                ssids.append(parts[0])
        ssids = list(dict.fromkeys(ssids))[:15]
    except Exception:
        ssids = []

    if ssids:
        ssid = radiolist(
            L("WiFi Networks", "Redes WiFi"),
            L(f"Interface: {iface}\nSelect a network:",
              f"Interfaz: {iface}\nSelecciona una red:"),
            [(s, s) for s in ssids]
        )
    else:
        ssid = inputbox(
            L("WiFi — SSID", "WiFi — SSID"),
            L(f"Interface: {iface}\nEnter network name (SSID):",
              f"Interfaz: {iface}\nIngresa el nombre de la red (SSID):")
        )

    if not ssid:
        return False

    passphrase = passwordbox(
        L("WiFi Password", "Contraseña WiFi"),
        L(f"Password for '{ssid}' (leave blank if open):",
          f"Contraseña de '{ssid}' (vacío si es abierta):")
    )
    if passphrase is None:
        return False

    if passphrase:
        cmd = (
            f"iwctl --passphrase {shlex.quote(passphrase)} "
            f"station {shlex.quote(iface)} connect {shlex.quote(ssid)}"
        )
    else:
        cmd = f"iwctl station {shlex.quote(iface)} connect {shlex.quote(ssid)}"

    subprocess.call(cmd, shell=True)
    time.sleep(5)

    ok = subprocess.call(
        "ping -c1 -W2 8.8.8.8 >/dev/null 2>&1",
        shell=True, executable="/bin/bash"
    ) == 0

    if not ok:
        msgbox(
            L("WiFi Failed", "WiFi fallido"),
            L(f"Could not connect to '{ssid}'.\nCheck the password and try again.",
              f"No se pudo conectar a '{ssid}'.\nVerifica la contraseña e intenta de nuevo.")
        )
    return ok

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

    if shutil.which("iwctl") and _wifi_interfaces():
        if yesno(
            L("No network detected", "Sin red detectada"),
            L("No wired connection found.\nConnect via WiFi?",
              "No se detectó conexión cableada.\n¿Conectar por WiFi?")
        ):
            return screen_wifi_connect()

    return False


_PAT_INSTALL  = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)


class InstallBackend:
    def __init__(self, on_progress, on_stage, on_done):
        self.on_progress = on_progress
        self.on_stage    = on_stage
        self.on_done     = on_done
        self._progress   = 0.0
        self._lock       = threading.Lock()

    def _log(self, msg):
        write_log(msg)

    def _stage(self, msg):
        write_log(f">>> {msg}")
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
            f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} "
            f"| arch-chroot /mnt chpasswd",
            on_line=self._log, ignore_error=True
        )

    def _setup_btrfs(self, p3):
        opts = "noatime,compress=zstd,space_cache=v2"
        run_stream(f"mkfs.btrfs -f {p3}",                on_line=self._log)
        run_stream(f"mount {p3} /mnt",                   on_line=self._log)
        run_stream("btrfs subvolume create /mnt/@",      on_line=self._log)
        run_stream("btrfs subvolume create /mnt/@home",  on_line=self._log)
        run_stream("btrfs subvolume create /mnt/@var",   on_line=self._log)
        run_stream("btrfs subvolume create /mnt/@snapshots", on_line=self._log)
        run_stream("umount /mnt",                        on_line=self._log)
        run_stream(f"mount -o {opts},subvol=@ {p3} /mnt",              on_line=self._log)
        run_stream("mkdir -p /mnt/{home,var,.snapshots}",               on_line=self._log)
        run_stream(f"mount -o {opts},subvol=@home {p3} /mnt/home",     on_line=self._log)
        run_stream(f"mount -o {opts},subvol=@var  {p3} /mnt/var",      on_line=self._log)
        run_stream(f"mount -o {opts},subvol=@snapshots {p3} /mnt/.snapshots", on_line=self._log)

    def run(self):
        disk_path  = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)
        fs         = state["filesystem"]
        kernel     = state["kernel"]
        microcode  = detect_cpu()

        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.on_done(False, L("No network connection. Connect and retry.",
                                      "Sin conexión de red. Conéctese e intente de nuevo."))
                return

            run_stream("pacman -Sy --noconfirm archlinux-keyring",
                       on_line=self._log, ignore_error=True)

            if state.get("mirrors", True):
                self._stage(L("Optimizing mirrors with reflector…",
                              "Optimizando mirrors con reflector…"))
                run_stream("pacman -Sy --noconfirm reflector",
                           on_line=self._log, ignore_error=True)
                run_stream(
                    "reflector --latest 10 --sort rate --save /etc/pacman.d/mirrorlist",
                    on_line=self._log, ignore_error=True
                )
            self._pct(5)

            self._stage(L("Wiping disk…", "Borrando disco…"))
            self._gradual(7)
            run_stream(f"sgdisk -Z {disk_path}", on_line=self._log)
            self._pct(8)

            self._stage(L("Creating partitions…", "Creando particiones…"))
            run_stream(f"sgdisk -n1:0:+1G -t1:ef00 {disk_path}",            on_line=self._log)
            run_stream(f"sgdisk -n2:0:+{state['swap']}G -t2:8200 {disk_path}", on_line=self._log)
            run_stream(f"sgdisk -n3:0:0 -t3:8300 {disk_path}",              on_line=self._log)
            self._pct(12)

            self._stage(L("Formatting partitions…", "Formateando particiones…"))
            run_stream(f"mkfs.fat -F32 {p1}", on_line=self._log)
            run_stream(f"mkswap {p2}",        on_line=self._log)
            run_stream(f"swapon {p2}",        on_line=self._log)

            if fs == "btrfs":
                self._setup_btrfs(p3)
            else:
                run_stream(f"mkfs.ext4 -F {p3}", on_line=self._log)
                run_stream(f"mount {p3} /mnt",   on_line=self._log)
            self._pct(16)

            self._stage(L("Mounting EFI…", "Montando EFI…"))
            run_stream("mkdir -p /mnt/boot/efi",    on_line=self._log)
            run_stream(f"mount {p1} /mnt/boot/efi", on_line=self._log)
            self._pct(18)

            ucode_pkg   = f" {microcode}" if microcode else ""
            extra_pkgs  = " btrfs-progs" if fs == "btrfs" else ""
            kernel_hdrs = f"{kernel}-headers"
            pkgs = (
                f"base {kernel} linux-firmware {kernel_hdrs} sof-firmware "
                f"base-devel grub efibootmgr vim nano networkmanager git "
                f"sudo bash-completion{extra_pkgs}{ucode_pkg}"
            )

            self._stage(L("Installing base system — this may take a while…",
                          "Instalando sistema base — esto puede tardar…"))
            rc = self._pacman(f"pacstrap -K /mnt {pkgs}", 18, 52)
            if rc != 0:
                self.on_done(False, L("pacstrap failed. Check " + LOG_FILE,
                                      "pacstrap falló. Revisa " + LOG_FILE))
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
            locale      = "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
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

            desktop = state["desktop"]
            if desktop != "None":
                self._stage(L(f"Installing {desktop}…", f"Instalando {desktop}…"))
                pkg_groups = DESKTOP_PKGS.get(desktop, [])
                n          = max(len(pkg_groups), 1)
                step       = 14.0 / n
                for i, grp in enumerate(pkg_groups):
                    s = 77 + i * step
                    e = 77 + (i + 1) * step
                    self._pacman(
                        f"arch-chroot /mnt pacman -S --noconfirm {grp}",
                        s, e, ignore_error=True
                    )
                dm = DESKTOP_DM.get(desktop)
                if dm:
                    self._chroot(f"systemctl enable {dm}")

                self._stage(L("Installing audio (pipewire)…", "Instalando audio (pipewire)…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "pipewire pipewire-pulse wireplumber",
                    91, 94, ignore_error=True
                )

            self._pct(94)

            self._stage(L("Installing GRUB bootloader…", "Instalando GRUB…"))
            self._chroot("grub-install --target=x86_64-efi "
                         "--efi-directory=/boot/efi --bootloader-id=GRUB")
            self._chroot("grub-mkconfig -o /boot/grub/grub.cfg")
            self._pct(97)

            if state.get("quick"):
                self._stage(L("Installing yay (AUR helper)…", "Instalando yay (AUR helper)…"))
                self._chroot(
                    "echo '%wheel ALL=(ALL) NOPASSWD: ALL' "
                    "> /etc/sudoers.d/99_nopasswd_tmp"
                )
                self._chroot(
                    f"su - {shlex.quote(uname)} -c "
                    "'git clone https://aur.archlinux.org/yay.git /tmp/yay "
                    "&& cd /tmp/yay && makepkg -si --noconfirm'",
                    ignore_error=True
                )
                self._chroot("rm -f /etc/sudoers.d/99_nopasswd_tmp")

            self._pct(100)
            self._stage(L("Installation complete!", "¡Instalación completa!"))
            self.on_done(True, "")

        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))


def screen_welcome():
    text = (
        "\\Zb\\Z4Welcome to the Arch Linux Installer\\Zn\n\n"
        f"Version: {VERSION}\n\n"
        "\\Zb\\Z1WARNING:\\Zn  This installer will ERASE and install Arch Linux "
        "to the selected disk.\n\n"
        "Use \\ZbTab\\Zn and \\ZbArrow keys\\Zn to navigate.\n"
        "Press OK to begin."
    )
    dlg_titled("Welcome", "--msgbox", text, "16", "60")

def screen_language():
    result = menu(
        "Language / Idioma",
        "Choose the installer language:\nSeleccione el idioma del instalador:",
        [("en", "English"), ("es", "Espanol")]
    )
    if result:
        state["lang"] = result

def screen_mode():
    result = menu(
        L("Install Mode", "Modo de instalación"),
        L(
            "Quick Install  —  BTRFS + KDE Plasma + linux + pipewire + yay\n"
            "Custom Install —  configure everything step by step",
            "Instalación rápida  —  BTRFS + KDE Plasma + linux + pipewire + yay\n"
            "Instalación personalizada — configura todo paso a paso"
        ),
        [
            ("quick",  L("Quick Install   (sane defaults, installs yay)",
                         "Instalación rápida   (valores por defecto, instala yay)")),
            ("custom", L("Custom Install  (full control)",
                         "Instalación personalizada  (control total)")),
        ]
    )
    if result == "quick":
        state["quick"]      = True
        state["filesystem"] = "btrfs"
        state["kernel"]     = "linux"
        state["desktop"]    = "KDE Plasma"
        state["mirrors"]    = True
        state["gpu"]        = detect_gpu()
    return result == "quick"

def screen_identity():
    while True:
        hn = inputbox(
            L("System Identity", "Identidad del sistema"),
            L("Enter hostname (letters, digits, -, _ — max 32 chars):",
              "Ingresa el nombre del equipo (letras, dígitos, -, _ — max 32):"),
            state.get("hostname", "")
        )
        if hn is None:
            return False
        if not validate_name(hn):
            msgbox(
                L("Invalid hostname", "Hostname inválido"),
                L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                  "Solo letras, dígitos, guiones y guiones bajos. Max 32 caracteres.")
            )
            continue

        un = inputbox(
            L("System Identity", "Identidad del sistema"),
            L("Enter username (letters, digits, -, _ — max 32 chars):",
              "Ingresa el nombre de usuario (letras, dígitos, -, _ — max 32):"),
            state.get("username", "")
        )
        if un is None:
            return False
        if not validate_name(un):
            msgbox(
                L("Invalid username", "Usuario inválido"),
                L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                  "Solo letras, dígitos, guiones y guiones bajos. Max 32 caracteres.")
            )
            continue

        state["hostname"] = hn
        state["username"] = un
        return True

def screen_passwords():
    while True:
        rp1 = passwordbox(
            L("Passwords", "Contraseñas"),
            L("Enter ROOT password:", "Ingresa la contraseña de ROOT:")
        )
        if rp1 is None:
            return False
        rp2 = passwordbox(
            L("Passwords", "Contraseñas"),
            L("Confirm ROOT password:", "Confirma la contraseña de ROOT:")
        )
        if rp2 is None:
            return False
        if not rp1:
            msgbox(L("Error", "Error"), L("Root password cannot be empty.",
                                          "La contraseña root no puede estar vacía."))
            continue
        if rp1 != rp2:
            msgbox(L("Error", "Error"), L("Root passwords do not match.",
                                          "Las contraseñas root no coinciden."))
            continue

        up1 = passwordbox(
            L("Passwords", "Contraseñas"),
            L("Enter USER password:", "Ingresa la contraseña de USUARIO:")
        )
        if up1 is None:
            return False
        up2 = passwordbox(
            L("Passwords", "Contraseñas"),
            L("Confirm USER password:", "Confirma la contraseña de USUARIO:")
        )
        if up2 is None:
            return False
        if not up1:
            msgbox(L("Error", "Error"), L("User password cannot be empty.",
                                          "La contraseña de usuario no puede estar vacía."))
            continue
        if up1 != up2:
            msgbox(L("Error", "Error"), L("User passwords do not match.",
                                          "Las contraseñas de usuario no coinciden."))
            continue

        state["root_pass"] = rp1
        state["user_pass"] = up1
        return True

def screen_disk():
    disks = list_disks()
    if not disks:
        msgbox(
            L("No disks found", "Sin discos"),
            L("No disks were detected. Cannot continue.",
              "No se detectaron discos. No se puede continuar.")
        )
        sys.exit(1)

    items   = [(f"/dev/{n}", f"{gb} GB  —  {model}") for n, gb, model in disks]
    default = f"/dev/{state['disk']}" if state["disk"] else items[0][0]

    result = radiolist(
        L("Disk Selection", "Selección de disco"),
        L("WARNING: ALL DATA on the selected disk will be ERASED!\n\n"
          "Select the installation disk:",
          "ADVERTENCIA: ¡Se borrarán todos los datos del disco seleccionado!\n\n"
          "Selecciona el disco:"),
        items,
        default=default
    )
    if result is None:
        return False
    state["disk"] = result.replace("/dev/", "")

    while True:
        swap = inputbox(
            L("Swap Size", "Tamaño de Swap"),
            L("Enter swap size in GB (1-128):", "Ingresa el tamaño del swap en GB (1-128):"),
            state["swap"]
        )
        if swap is None:
            return False
        if validate_swap(swap.strip()):
            state["swap"] = swap.strip()
            return True
        msgbox(L("Invalid swap", "Swap inválido"),
               L("Swap must be a number between 1 and 128.",
                 "El swap debe ser un número entre 1 y 128."))

def screen_filesystem():
    options = [
        ("ext4",  L("ext4  — stable, widely supported",
                    "ext4  — estable, amplio soporte")),
        ("btrfs", L("btrfs — subvolumes (@, @home, @var, @snapshots) + zstd compression",
                    "btrfs — subvolúmenes (@, @home, @var, @snapshots) + compresión zstd")),
    ]
    result = radiolist(
        L("Filesystem", "Sistema de archivos"),
        L("Choose the root filesystem:", "Elige el sistema de archivos raíz:"),
        options,
        default=state["filesystem"]
    )
    if result:
        state["filesystem"] = result
    return True

def screen_kernel():
    options = [
        ("linux",     L("linux     — latest stable",
                        "linux     — estable más reciente")),
        ("linux-lts", L("linux-lts — long-term support",
                        "linux-lts — soporte a largo plazo")),
        ("linux-zen", L("linux-zen — optimized for desktop/gaming",
                        "linux-zen — optimizado para escritorio/gaming")),
    ]
    result = radiolist(
        L("Kernel", "Kernel"),
        L("Select the kernel to install:", "Selecciona el kernel a instalar:"),
        options,
        default=state["kernel"]
    )
    if result:
        state["kernel"] = result
    return True

def screen_mirrors():
    result = radiolist(
        L("Mirror Optimization", "Optimización de mirrors"),
        L("Use reflector to select the 10 fastest mirrors? (recommended)",
          "¿Usar reflector para seleccionar los 10 mirrors más rápidos? (recomendado)"),
        [
            ("yes", L("Yes — auto-select fastest mirrors",
                      "Sí — seleccionar mirrors más rápidos")),
            ("no",  L("No  — keep default mirrors",
                      "No  — mantener mirrors por defecto")),
        ],
        default="yes" if state["mirrors"] else "no"
    )
    if result:
        state["mirrors"] = (result == "yes")
    return True

def screen_keymap():
    try:
        out  = subprocess.check_output(
            "localectl list-keymaps 2>/dev/null || true",
            shell=True, text=True
        )
        maps = [l for l in out.splitlines() if l]
    except Exception:
        maps = []

    wanted  = ["us", "es", "uk", "fr", "de", "it", "ru", "ara", "pt-latin9", "br-abnt2"]
    options = [m for m in wanted if m in maps] if maps else wanted
    items   = [(m, f"Keyboard layout: {m}") for m in options]

    result = radiolist(
        L("Keyboard Layout", "Distribución de teclado"),
        L("Select your keyboard layout:", "Selecciona la distribución de tu teclado:"),
        items,
        default=state["keymap"]
    )
    if result:
        state["keymap"] = result
        run_simple(f"loadkeys {shlex.quote(result)}", ignore_error=True)
    return True

def screen_timezone():
    try:
        out   = subprocess.check_output(
            "timedatectl list-timezones 2>/dev/null || true",
            shell=True, text=True
        )
        zones = [l for l in out.splitlines() if l]
    except Exception:
        zones = []
    if not zones:
        zones = ["UTC", "Europe/Madrid", "Europe/London",
                 "America/New_York", "America/Los_Angeles", "Asia/Tokyo"]

    regions    = sorted(set(z.split("/")[0] for z in zones if "/" in z))
    regions    = ["UTC"] + regions
    cur_region = state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC"

    region = radiolist(
        L("Timezone — Region", "Zona horaria — Región"),
        L("Select your region:", "Selecciona tu región:"),
        [(r, r) for r in regions],
        default=cur_region
    )
    if region is None:
        return True
    if region == "UTC":
        state["timezone"] = "UTC"
        return True

    cities   = [(z.split("/", 1)[1], z) for z in zones if z.startswith(region + "/")]
    if not cities:
        state["timezone"] = region
        return True

    cur_city = state["timezone"].split("/", 1)[1] if "/" in state["timezone"] else ""
    city = radiolist(
        L("Timezone — City", "Zona horaria — Ciudad"),
        L(f"Region: {region}\nSelect your city:",
          f"Región: {region}\nSelecciona tu ciudad:"),
        cities,
        default=cur_city
    )
    if city:
        state["timezone"] = f"{region}/{city}"
    return True

def screen_desktop():
    options = [
        ("KDE Plasma", L("KDE Plasma — full featured, modern",
                         "KDE Plasma — completo, moderno")),
        ("GNOME",      L("GNOME     — clean, Wayland-first",
                         "GNOME     — limpio, Wayland primero")),
        ("Cinnamon",   L("Cinnamon  — classic, Windows-like",
                         "Cinnamon  — clásico, similar a Windows")),
        ("XFCE",       L("XFCE      — lightweight, traditional",
                         "XFCE      — ligero, tradicional")),
        ("MATE",       L("MATE      — GNOME 2 fork, very stable",
                         "MATE      — fork de GNOME 2, muy estable")),
        ("LXQt",       L("LXQt      — minimal Qt desktop",
                         "LXQt      — escritorio Qt minimalista")),
        ("None",       L("None      — CLI only, no desktop",
                         "None      — solo terminal, sin escritorio")),
    ]
    result = radiolist(
        L("Desktop Environment", "Entorno de escritorio"),
        L("Choose a desktop environment:", "Elige un entorno de escritorio:"),
        options,
        default=state["desktop"]
    )
    if result:
        state["desktop"] = result
    return True

def screen_gpu():
    detected = detect_gpu()
    if detected != "None" and state["gpu"] == "None":
        state["gpu"] = detected

    tag  = {"NVIDIA": "\\Z3NVIDIA\\Zn", "AMD/Intel": "\\Z2AMD/Intel\\Zn"}.get(detected, "none")
    hint = L(f"Detected GPU: {tag}", f"GPU detectada: {tag}")

    options = [
        ("NVIDIA",    L("NVIDIA proprietary (nvidia + nvidia-utils)",
                        "NVIDIA propietario (nvidia + nvidia-utils)")),
        ("AMD/Intel", L("Open-source Mesa (mesa + vulkan-radeon)",
                        "Mesa open-source (mesa + vulkan-radeon)")),
        ("None",      L("No additional GPU drivers", "Sin drivers adicionales de GPU")),
    ]
    result = radiolist(
        L("GPU Drivers", "Drivers GPU"),
        L(f"{hint}\n\nSelect GPU driver:", f"{hint}\n\nSelecciona el driver de GPU:"),
        options,
        default=state["gpu"]
    )
    if result:
        state["gpu"] = result
    return True

def screen_review():
    microcode = detect_cpu() or L("none detected", "no detectado")
    quick_tag = L("  [Quick Install]", "  [Instalación rápida]") if state["quick"] else ""

    lines = [
        ("Mode",                    L("Quick", "Rápida") if state["quick"] else L("Custom", "Personalizada")),
        ("Language",                state["lang"]),
        ("Hostname",                state["hostname"] or "NOT SET"),
        (L("Username", "Usuario"),  state["username"] or "NOT SET"),
        ("Filesystem",              state["filesystem"]),
        ("Kernel",                  state["kernel"]),
        ("Microcode",               microcode),
        ("Disk",                    f"/dev/{state['disk']}" if state["disk"] else "NOT SET"),
        ("Swap",                    f"{state['swap']} GB"),
        ("Mirrors",                 L("reflector (auto)", "reflector (auto)") if state["mirrors"] else L("default", "por defecto")),
        ("Keymap",                  state["keymap"]),
        ("Timezone",                state["timezone"]),
        ("Desktop",                 state["desktop"]),
        ("GPU",                     state["gpu"]),
        ("Audio",                   "pipewire" if state["desktop"] != "None" else L("none", "ninguno")),
        ("yay",                     L("yes", "sí") if state["quick"] else "no"),
    ]

    text    = L(f"Review your settings:{quick_tag}\n\n",
                f"Revisa tu configuración:{quick_tag}\n\n")
    missing = []

    for label, val in lines:
        text += f"  {label:<14} {val}\n"
    text += "\n"

    if not state["hostname"]:  missing.append("hostname")
    if not state["username"]:  missing.append("username")
    if not state["disk"]:      missing.append("disk")
    if not state["root_pass"]: missing.append(L("root password", "contraseña root"))

    if missing:
        text += L(f"MISSING: {', '.join(missing)}\n\nGo back to fix before continuing.",
                  f"FALTA: {', '.join(missing)}\n\nVuelve atrás para corregirlo.")
        msgbox(L("Review — Incomplete", "Revisión — Incompleto"), text)
        return False

    text += L("All settings look good.", "Todo listo.")
    return yesno(
        L("Review & Confirm", "Revisar y confirmar"),
        text + L(
            f"\n\nWARNING: THIS WILL ERASE /dev/{state['disk']}.\n\nProceed?",
            f"\n\nADVERTENCIA: SE BORRARÁ /dev/{state['disk']}.\n\n¿Proceder?"
        )
    )

def screen_install():
    gauge         = gauge_open(
        L("Installing Arch Linux", "Instalando Arch Linux"),
        L("Preparing…", "Preparando…"), pct=0
    )
    current_stage = [L("Preparing…", "Preparando…")]
    current_pct   = [0.0]
    failed        = [False]
    fail_reason   = [""]
    done_event    = threading.Event()

    def on_progress(pct):
        current_pct[0] = pct
        gauge_update(gauge, pct, current_stage[0])

    def on_stage(msg):
        current_stage[0] = msg
        gauge_update(gauge, current_pct[0], msg)

    def on_done(success, reason):
        failed[0]      = not success
        fail_reason[0] = reason
        done_event.set()

    t = threading.Thread(
        target=InstallBackend(on_progress, on_stage, on_done).run,
        daemon=True
    )
    t.start()
    done_event.wait()

    try:
        gauge.stdin.close()
    except Exception:
        pass
    gauge.wait()

    if failed[0]:
        msgbox(
            L("Installation Failed", "Instalación fallida"),
            L(f"Installation failed.\n\n{fail_reason[0]}\n\nCheck {LOG_FILE} for details.",
              f"La instalación falló.\n\n{fail_reason[0]}\n\nRevisa {LOG_FILE} para detalles.")
        )
        return False
    return True

def screen_finish():
    ok = yesno(
        L("Installation Complete!", "¡Instalación completa!"),
        L("Arch Linux has been installed successfully.\n\n"
          "Remove the installation media. Reboot now?",
          "Arch Linux se ha instalado correctamente.\n\n"
          "Extrae el medio de instalación. ¿Reiniciar ahora?")
    )
    if ok:
        dlg_titled(
            L("Rebooting", "Reiniciando"),
            "--infobox",
            L("Unmounting filesystems and rebooting…",
              "Desmontando sistemas de archivos y reiniciando…"),
            "5", "50"
        )
        subprocess.run("umount -R /mnt", shell=True)
        subprocess.run("reboot",         shell=True)
    sys.exit(0)


def main():
    screen_welcome()
    screen_language()
    quick = screen_mode()

    if quick:
        steps = [
            (L("Disk",      "Disco"),       screen_disk,      True),
            (L("Identity",  "Identidad"),   screen_identity,  True),
            (L("Passwords", "Contraseñas"), screen_passwords, True),
            (L("Review",    "Revisión"),    screen_review,    True),
            (L("Install",   "Instalar"),    screen_install,   False),
            (L("Finish",    "Finalizar"),   screen_finish,    False),
        ]
    else:
        steps = [
            (L("Disk",       "Disco"),            screen_disk,       True),
            (L("Filesystem", "Sistema archivos"), screen_filesystem, True),
            (L("Kernel",     "Kernel"),           screen_kernel,     True),
            (L("Mirrors",    "Mirrors"),          screen_mirrors,    True),
            (L("Identity",   "Identidad"),        screen_identity,   True),
            (L("Passwords",  "Contraseñas"),      screen_passwords,  True),
            (L("Keymap",     "Teclado"),          screen_keymap,     True),
            (L("Timezone",   "Zona horaria"),     screen_timezone,   True),
            (L("Desktop",    "Escritorio"),       screen_desktop,    True),
            ("GPU",                               screen_gpu,        True),
            (L("Review",     "Revisión"),         screen_review,     True),
            (L("Install",    "Instalar"),         screen_install,    False),
            (L("Finish",     "Finalizar"),        screen_finish,     False),
        ]

    idx = 0
    while idx < len(steps):
        _, fn, can_go_back = steps[idx]
        result = fn()
        if result is False and can_go_back:
            idx = max(0, idx - 1)
        else:
            idx += 1


def bootstrap():
    if os.geteuid() != 0:
        print("This installer must be run as root.")
        print("Example: sudo python arch_installer_gui.py")
        sys.exit(1)

    if not shutil.which("dialog"):
        print("[*] dialog not found — installing via pacman...")
        rc = subprocess.call(
            "pacman -Sy --noconfirm dialog",
            shell=True, executable="/bin/bash"
        )
        if rc != 0:
            print("[!] Failed to install dialog. Check your network and try again.")
            sys.exit(1)
        print("[+] dialog installed successfully.\n")

    main()


if __name__ == "__main__":
    bootstrap()
