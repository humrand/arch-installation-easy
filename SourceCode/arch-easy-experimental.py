import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

VERSION  = "V1.1.4-beta"
LOG_FILE = "/tmp/arch_install.log"
TITLE    = "Arch Linux Installer"

state = {
    "lang":       "en",
    "locale":     "en_US.UTF-8",
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
    "yay":        False,
    "snapper":    False,
    "bootloader": "grub",
    "flatpak":    False,
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
    "Hyprland": [
        "hyprland waybar wofi alacritty xdg-desktop-portal-hyprland "
        "polkit-gnome qt5-wayland qt6-wayland sddm firefox",
    ],
    "Sway": [
        "sway waybar wofi alacritty xdg-desktop-portal-wlr "
        "polkit-gnome qt5-wayland sddm firefox",
    ],
}

DESKTOP_DM = {
    "KDE Plasma": "sddm",
    "GNOME":      "gdm",
    "Cinnamon":   "lightdm",
    "XFCE":       "lightdm",
    "MATE":       "lightdm",
    "LXQt":       "sddm",
    "Hyprland":   "sddm",
    "Sway":       "sddm",
}

CONSOLE_TO_X11 = {
    "us":          "us",
    "es":          "es",
    "uk":          "gb",
    "fr":          "fr",
    "de":          "de",
    "it":          "it",
    "ru":          "ru",
    "ara":         "ara",
    "pt-latin9":   "pt",
    "br-abnt2":    "br",
    "pl2":         "pl",
    "hu":          "hu",
    "cz-qwerty":   "cz",
    "sk-qwerty":   "sk",
    "ro_win":      "ro",
    "dk":          "dk",
    "no":          "no",
    "sv-latin1":   "se",
    "fi":          "fi",
    "nl":          "nl",
    "tr_q-latin5": "tr",
    "ja106":       "jp",
    "kr106":       "kr",
}

LOCALE_TO_KEYMAP = {
    "es_ES.UTF-8": "es",
    "es_MX.UTF-8": "us",
    "es_AR.UTF-8": "us",
    "en_US.UTF-8": "us",
    "en_GB.UTF-8": "uk",
    "fr_FR.UTF-8": "fr",
    "de_DE.UTF-8": "de",
    "it_IT.UTF-8": "it",
    "pt_PT.UTF-8": "pt-latin9",
    "pt_BR.UTF-8": "br-abnt2",
    "ru_RU.UTF-8": "ru",
    "nl_NL.UTF-8": "nl",
    "pl_PL.UTF-8": "pl2",
    "cs_CZ.UTF-8": "cz-qwerty",
    "sk_SK.UTF-8": "sk-qwerty",
    "hu_HU.UTF-8": "hu",
    "ro_RO.UTF-8": "ro_win",
    "da_DK.UTF-8": "dk",
    "nb_NO.UTF-8": "no",
    "sv_SE.UTF-8": "sv-latin1",
    "fi_FI.UTF-8": "fi",
    "tr_TR.UTF-8": "tr_q-latin5",
    "ja_JP.UTF-8": "ja106",
    "ko_KR.UTF-8": "kr106",
    "zh_CN.UTF-8": "us",
    "ar_SA.UTF-8": "ara",
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


def _flush_stdin():
    import termios
    try:
        termios.tcflush(sys.stdin.fileno(), termios.TCIFLUSH)
    except Exception:
        pass


def dlg_titled(title, *args):
    _flush_stdin()
    cmd = [
        "dialog",
        "--colors",
        "--backtitle", f"\\Zb\\Z4{TITLE}\\Zn  -  {VERSION}",
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
    height = min(len(items) + 12, 40)
    rc, val = dlg_titled(title, "--menu", text, str(height), "76", str(len(items)), *flat)
    if rc != 0:
        return None
    return val

def radiolist(title, text, items, default=None):
    flat = []
    for tag, desc in items:
        status = "on" if tag == default else "off"
        flat.extend([tag, desc, status])
    height = min(len(items) + 12, 40)
    rc, val = dlg_titled(title, "--radiolist", text, str(height), "76", str(len(items)), *flat)
    if rc != 0:
        return None
    return val

def gauge_open(title, text, pct=0):
    proc = subprocess.Popen(
        [
            "dialog", "--colors",
            "--backtitle", f"\\Zb\\Z4{TITLE}\\Zn  -  {VERSION}",
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
    if re.search(r"(nvme\d+n\d+|mmcblk\d+)", disk_path):
        return f"{disk_path}p1", f"{disk_path}p2", f"{disk_path}p3"
    return f"{disk_path}1", f"{disk_path}2", f"{disk_path}3"

def detect_gpu():
    try:
        out = subprocess.check_output(
            "lspci 2>/dev/null | grep -iE 'vga|3d|display'",
            shell=True, text=True, stderr=subprocess.DEVNULL
        ).lower()
    except Exception:
        return "None"

    has_nvidia = "nvidia" in out
    has_amd    = "amd" in out or "radeon" in out
    has_intel  = "intel" in out

    if has_intel and has_nvidia:
        return "Intel+NVIDIA"
    if has_intel and has_amd:
        return "Intel+AMD"
    if has_nvidia:
        return "NVIDIA"
    if has_amd:
        return "AMD"
    if has_intel:
        return "Intel"
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

def is_uefi():
    return os.path.exists("/sys/firmware/efi")

def is_ssd(disk_path):
    disk_name = os.path.basename(disk_path)
    if disk_name.startswith("nvme") or disk_name.startswith("mmcblk"):
        block = re.sub(r"p\d+$", "", disk_name)
    else:
        block = re.sub(r"\d+$", "", disk_name)
    rotational = f"/sys/block/{block}/queue/rotational"
    try:
        with open(rotational) as f:
            return f.read().strip() == "0"
    except FileNotFoundError:
        return False

def suggest_swap_gb():
    try:
        with open("/proc/meminfo") as f:
            for line in f:
                if line.startswith("MemTotal:"):
                    kb  = int(line.split()[1])
                    ram = kb // (1024 * 1024)
                    if ram <= 2:  return 4
                    if ram <= 8:  return ram
                    if ram <= 16: return 8
                    return 8
    except Exception:
        pass
    return 8


def _settle_partitions(disk_path, log_fn=None):
    for cmd in (
        f"partprobe {disk_path}",
        "udevadm settle --timeout=10",
    ):
        rc = subprocess.call(cmd, shell=True, executable="/bin/bash",
                             stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
        if log_fn:
            log_fn(f"[settle] {cmd}  rc={rc}")
    time.sleep(1)


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
            L(
                "No wireless interfaces found.\n\nMake sure your WiFi adapter is recognized.",
                "No se encontraron interfaces inalambricas.\n\nVerifica que tu adaptador WiFi sea reconocido."
            )
        )
        return None

    iface = ifaces[0]

    dlg_titled(
        L("Scanning...", "Escaneando..."),
        "--infobox",
        L(f"Scanning for networks on {iface}...", f"Buscando redes en {iface}..."),
        "5", "50"
    )
    subprocess.call(
        f"iwctl station {shlex.quote(iface)} scan",
        shell=True, stderr=subprocess.DEVNULL
    )

    ansi_escape = re.compile(r"\x1b\[[0-9;]*m")

    def _parse_ssids(raw):
        found = []
        for line in raw.splitlines():
            clean = ansi_escape.sub("", line).strip().lstrip("> ").strip()
            if not clean or re.match(r"^[-=*]+$", clean) or clean.lower().startswith("network"):
                continue
            parts = clean.split()
            if parts and len(parts[0]) > 0:
                found.append(parts[0])
        return list(dict.fromkeys(found))[:15]

    ssids    = []
    deadline = time.time() + 12
    while time.time() < deadline:
        time.sleep(2)
        try:
            raw = subprocess.check_output(
                f"iwctl station {shlex.quote(iface)} get-networks 2>/dev/null",
                shell=True, text=True
            )
            ssids = _parse_ssids(raw)
        except Exception:
            ssids = []
        if ssids:
            break

    if ssids:
        ssid = radiolist(
            L("WiFi Networks", "Redes WiFi"),
            L(
                f"Interface: {iface}\n"
                "Select a network (Cancel = go back):",
                f"Interfaz: {iface}\n"
                "Selecciona una red (Cancelar = volver):"
            ),
            [(s, s) for s in ssids]
        )
    else:
        ssid = inputbox(
            L("WiFi - SSID", "WiFi - SSID"),
            L(
                f"Interface: {iface}\n"
                "No networks found automatically.\n"
                "Enter network name (SSID) or Cancel to go back:",
                f"Interfaz: {iface}\n"
                "No se encontraron redes automaticamente.\n"
                "Ingresa el nombre de la red (SSID) o Cancelar para volver:"
            )
        )

    if not ssid:
        return None

    passphrase = passwordbox(
        L("WiFi Password", "Contrasena WiFi"),
        L(
            f"Password for '{ssid}'\n(leave blank for open networks, Cancel to go back):",
            f"Contrasena de '{ssid}'\n(vacio si es abierta, Cancelar para volver):"
        )
    )
    if passphrase is None:
        return None

    dlg_titled(
        L("Connecting...", "Conectando..."),
        "--infobox",
        L(f"Connecting to '{ssid}'...", f"Conectando a '{ssid}'..."),
        "5", "50"
    )

    if passphrase:
        cmd = (
            f"iwctl --passphrase {shlex.quote(passphrase)} "
            f"station {shlex.quote(iface)} connect {shlex.quote(ssid)}"
        )
    else:
        cmd = f"iwctl station {shlex.quote(iface)} connect {shlex.quote(ssid)}"

    subprocess.call(cmd, shell=True)
    time.sleep(5)

    ok = _check_connectivity()

    if not ok:
        msgbox(
            L("WiFi Failed", "WiFi fallido"),
            L(
                f"Could not connect to '{ssid}'.\n\n"
                "Possible causes:\n"
                "  - Wrong password\n"
                "  - Network out of range\n"
                "  - DHCP not responding\n\n"
                "Press OK to go back and try again.",
                f"No se pudo conectar a '{ssid}'.\n\n"
                "Posibles causas:\n"
                "  - Contrasena incorrecta\n"
                "  - Red fuera de alcance\n"
                "  - DHCP sin respuesta\n\n"
                "Presiona OK para volver e intentarlo de nuevo."
            )
        )
        return False

    return True


def _check_connectivity():
    for cmd in (
        "curl -sI --max-time 5 https://archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 8.8.8.8 >/dev/null 2>&1",
    ):
        rc = subprocess.call(cmd, shell=True, executable="/bin/bash")
        if rc == 0:
            return True
    return False

def screen_network():
    while True:
        choice = menu(
            L("Network Connection", "Conexion de red"),
            L(
                "An active internet connection is required for installation.\n\n"
                "How are you connected to the internet?",
                "Se necesita conexion a internet para la instalacion.\n\n"
                "Como estas conectado a internet?"
            ),
            [
                ("wired", L(
                    "Wired (Ethernet)  - cable already plugged in",
                    "Cable (Ethernet)  - cable ya conectado"
                )),
                ("wifi",  L(
                    "WiFi              - connect to a wireless network",
                    "WiFi              - conectar a una red inalambrica"
                )),
            ]
        )

        if choice is None:
            if yesno(L("Exit", "Salir"), L("Exit the installer?", "Salir del instalador?")):
                sys.exit(0)
            continue

        if choice == "wired":
            dlg_titled(
                L("Checking...", "Verificando..."),
                "--infobox",
                L("Testing wired connection...", "Probando conexion por cable..."),
                "5", "50"
            )
            if _check_connectivity():
                msgbox(
                    L("Connected!", "Conectado!"),
                    L("Wired connection detected. Ready to continue.",
                      "Conexion por cable detectada. Listo para continuar.")
                )
                return
            msgbox(
                L("No connection detected", "Sin conexion detectada"),
                L(
                    "Could not reach archlinux.org over the wired connection.\n\n"
                    "Check that:\n"
                    "  - The cable is securely plugged in\n"
                    "  - Your router/switch is on\n\n"
                    "Press OK to go back and choose again.",
                    "No se pudo alcanzar archlinux.org por cable.\n\n"
                    "Verifica que:\n"
                    "  - El cable este bien conectado\n"
                    "  - Tu router/switch este encendido\n\n"
                    "Presiona OK para volver y elegir de nuevo."
                )
            )
            continue

        if choice == "wifi":
            result = screen_wifi_connect()
            if result is True:
                msgbox(
                    L("Connected!", "Conectado!"),
                    L("WiFi connected successfully. Ready to continue.",
                      "WiFi conectado correctamente. Listo para continuar.")
                )
                return

def ensure_network():
    if _check_connectivity():
        return True

    for tool in ("dhcpcd", "dhclient"):
        if shutil.which(tool):
            run_simple(f"{tool} >/dev/null 2>&1", ignore_error=True)
            time.sleep(3)
            if _check_connectivity():
                return True

    if shutil.which("iwctl") and _wifi_interfaces():
        if yesno(
            L("No network detected", "Sin red detectada"),
            L("No wired connection found.\nConnect via WiFi?",
              "No se detecto conexion cableada.\nConectar por WiFi?")
        ):
            return screen_wifi_connect()

    return False


_PAT_INSTALL  = re.compile(r"\((\d+)/(\d+)\)")
_PAT_DOWNLOAD = re.compile(
    r"\S+\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)\s+\d+(?:\.\d+)?\s*(?:B|KiB|MiB|GiB)/s"
)

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

    def _run_critical(self, cmd, label="command"):
        rc = run_stream(cmd, on_line=self._log)
        if rc != 0:
            raise RuntimeError(
                L(f"{label} failed (rc={rc}). Check {LOG_FILE}.",
                  f"{label} fallo (rc={rc}). Revisa {LOG_FILE}.")
            )
        return rc

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

    def _pacman_critical(self, cmd, start, end, label="pacman"):
        rc = self._pacman(cmd, start, end)
        if rc != 0:
            raise RuntimeError(
                L(f"{label} failed (rc={rc}). Check {LOG_FILE}.",
                  f"{label} fallo (rc={rc}). Revisa {LOG_FILE}.")
            )
        return rc

    def _chroot(self, cmd, ignore_error=False):
        return run_stream(
            f"arch-chroot /mnt /bin/bash -c {shlex.quote(cmd)}",
            on_line=self._log, ignore_error=ignore_error
        )

    def _chroot_critical(self, cmd, label="chroot"):
        rc = self._chroot(cmd)
        if rc != 0:
            raise RuntimeError(
                L(f"{label} failed (rc={rc}). Check {LOG_FILE}.",
                  f"{label} fallo (rc={rc}). Revisa {LOG_FILE}.")
            )

    def _chroot_passwd(self, user, pwd):
        return run_stream(
            f"printf '%s\\n' {shlex.quote(user + ':' + pwd)} "
            f"| arch-chroot /mnt chpasswd",
            on_line=self._log, ignore_error=True
        )

    def _settle(self, disk_path):
        _settle_partitions(disk_path, log_fn=self._log)

    def _btrfs_opts(self, disk_path):
        opts = "noatime,compress=zstd,space_cache=v2"
        if is_ssd(disk_path):
            opts += ",ssd,discard=async"
            self._log(f"SSD detected on {disk_path} - adding ssd,discard=async to BTRFS opts")
        return opts

    def _setup_btrfs(self, p3, disk_path):
        opts = self._btrfs_opts(disk_path)
        self._run_critical(f"mkfs.btrfs -f {p3}",           "mkfs.btrfs")
        self._run_critical(f"mount {p3} /mnt",               "mount btrfs")
        self._run_critical("btrfs subvolume create /mnt/@",  "btrfs subvol @")
        self._run_critical("btrfs subvolume create /mnt/@home",     "btrfs subvol @home")
        self._run_critical("btrfs subvolume create /mnt/@var",      "btrfs subvol @var")
        self._run_critical("btrfs subvolume create /mnt/@snapshots","btrfs subvol @snapshots")
        self._run_critical("umount /mnt",                    "umount btrfs")
        self._run_critical(f"mount -o {opts},subvol=@ {p3} /mnt",          "mount @")
        self._run_critical("mkdir -p /mnt/{home,var,.snapshots}",           "mkdir")
        self._run_critical(f"mount -o {opts},subvol=@home {p3} /mnt/home", "mount @home")
        self._run_critical(f"mount -o {opts},subvol=@var  {p3} /mnt/var",  "mount @var")
        self._run_critical(
            f"mount -o {opts},subvol=@snapshots {p3} /mnt/.snapshots",     "mount @snapshots"
        )

    def _install_grub(self, disk_path):
        if is_uefi():
            self._chroot_critical(
                "grub-install --target=x86_64-efi "
                "--efi-directory=/boot/efi --bootloader-id=GRUB",
                "grub-install UEFI"
            )
        else:
            self._chroot_critical(
                f"grub-install --target=i386-pc {disk_path}",
                "grub-install BIOS"
            )
        self._chroot_critical("grub-mkconfig -o /boot/grub/grub.cfg", "grub-mkconfig")

    def _install_systemd_boot(self, root_dev):
        self._chroot_critical("bootctl install", "bootctl install")

        fs        = state["filesystem"]
        kernel    = state["kernel"]
        microcode = detect_cpu()

        try:
            partuuid = subprocess.check_output(
                f"blkid -s PARTUUID -o value {root_dev}",
                shell=True, text=True
            ).strip()
        except Exception:
            partuuid = ""

        root_opt   = f"root=PARTUUID={partuuid}" if partuuid else f"root={root_dev}"
        extra_opts = "rootflags=subvol=@ " if fs == "btrfs" else ""

        loader_conf = (
            "default arch.conf\n"
            "timeout 4\n"
            "console-mode max\n"
            "editor no\n"
        )
        os.makedirs("/mnt/boot/loader", exist_ok=True)
        with open("/mnt/boot/loader/loader.conf", "w") as f:
            f.write(loader_conf)

        ucode_line = f"initrd  /{microcode}.img\n" if microcode else ""
        arch_conf = (
            f"title   Arch Linux\n"
            f"linux   /vmlinuz-{kernel}\n"
            f"{ucode_line}"
            f"initrd  /initramfs-{kernel}.img\n"
            f"options {root_opt} rw quiet {extra_opts}\n"
        )
        os.makedirs("/mnt/boot/loader/entries", exist_ok=True)
        with open("/mnt/boot/loader/entries/arch.conf", "w") as f:
            f.write(arch_conf)

        write_log("systemd-boot installed and configured.")

    def _install_limine(self, disk_path, root_dev):
        uefi      = is_uefi()
        fs        = state["filesystem"]
        kernel    = state["kernel"]
        microcode = detect_cpu()

        try:
            partuuid = subprocess.check_output(
                f"blkid -s PARTUUID -o value {root_dev}",
                shell=True, text=True
            ).strip()
        except Exception:
            partuuid = ""

        root_opt   = f"root=PARTUUID={partuuid}" if partuuid else f"root={root_dev}"
        extra_opts = "rootflags=subvol=@ " if fs == "btrfs" else ""

        ucode_line = (
            f"    MODULE_PATH=boot():/boot/{microcode}.img\n"
            if microcode else ""
        )

        limine_conf = (
            "TIMEOUT=5\n\n"
            ":Arch Linux\n"
            "    PROTOCOL=linux\n"
            f"    KERNEL_PATH=boot():/boot/vmlinuz-{kernel}\n"
            f"{ucode_line}"
            f"    MODULE_PATH=boot():/boot/initramfs-{kernel}.img\n"
            f"    CMDLINE={root_opt} rw quiet {extra_opts}\n"
        )

        os.makedirs("/mnt/boot/limine", exist_ok=True)
        with open("/mnt/boot/limine.conf", "w") as f:
            f.write(limine_conf)
        write_log("limine.conf written to /mnt/boot/limine.conf")

        if uefi:
            self._chroot_critical(
                "mkdir -p /boot/efi/EFI/limine && "
                "cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/limine/",
                "limine copy EFI"
            )

            try:
                p1 = partition_paths_for(disk_path)[0]
                part_num = subprocess.check_output(
                    f"lsblk -no PARTN {p1} 2>/dev/null",
                    shell=True, text=True
                ).strip() or "1"
            except Exception:
                part_num = "1"

            self._chroot(
                f"efibootmgr --create "
                f"--disk {shlex.quote(disk_path)} "
                f"--part {part_num} "
                f"--label 'Arch Linux Limine' "
                f"--loader '\\\\EFI\\\\limine\\\\BOOTX64.EFI' "
                f"--unicode",
                ignore_error=True
            )

            pacman_hook = (
                "[Trigger]\n"
                "Operation = Install\n"
                "Operation = Upgrade\n"
                "Type = Package\n"
                "Target = limine\n\n"
                "[Action]\n"
                "Description = Deploying Limine after upgrade...\n"
                "When = PostTransaction\n"
                "Exec = /bin/sh -c "
                "'cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/limine/'\n"
            )
            self._chroot(
                "mkdir -p /etc/pacman.d/hooks && "
                f"printf {shlex.quote(pacman_hook)} "
                "> /etc/pacman.d/hooks/limine.hook",
                ignore_error=True
            )
            write_log("Limine UEFI installed with efibootmgr entry and pacman hook.")
        else:
            self._chroot(
                "cp /usr/share/limine/limine-bios.sys /boot/limine/",
                ignore_error=True
            )
            rc = run_stream(
                f"arch-chroot /mnt limine bios-install {shlex.quote(disk_path)}",
                on_line=self._log
            )
            if rc != 0:
                write_log(f"limine bios-install rc={rc} - check disk manually")
            write_log("Limine BIOS installed.")

    def _install_gpu_drivers(self, start_pct, end_pct):
        gpu    = state["gpu"]
        kernel = state["kernel"]

        nvidia_pkg = "nvidia" if kernel == "linux" else "nvidia-dkms"

        def _do(pkgs, label):
            self._stage(L(f"Installing {label} drivers...", f"Instalando drivers {label}..."))
            self._pacman(
                f"arch-chroot /mnt pacman -S --noconfirm {pkgs}",
                start_pct, end_pct, ignore_error=True
            )

        if gpu == "NVIDIA":
            _do(f"{nvidia_pkg} nvidia-utils nvidia-settings", "NVIDIA")
            self._configure_nvidia_modeset()

        elif gpu == "AMD":
            _do("mesa vulkan-radeon libva-mesa-driver", "AMD")

        elif gpu == "Intel":
            _do("mesa vulkan-intel intel-media-driver", "Intel")

        elif gpu == "Intel+NVIDIA":
            self._stage(L("Installing Intel+NVIDIA (hybrid) drivers...",
                          "Instalando drivers Intel+NVIDIA (hybrid)..."))
            self._pacman(
                "arch-chroot /mnt pacman -S --noconfirm "
                "mesa vulkan-intel intel-media-driver",
                start_pct, start_pct + (end_pct - start_pct) * 0.4,
                ignore_error=True
            )
            self._pacman(
                f"arch-chroot /mnt pacman -S --noconfirm "
                f"{nvidia_pkg} nvidia-utils nvidia-settings nvidia-prime",
                start_pct + (end_pct - start_pct) * 0.4, end_pct,
                ignore_error=True
            )
            self._configure_nvidia_modeset()

        elif gpu == "Intel+AMD":
            _do("mesa vulkan-intel intel-media-driver vulkan-radeon libva-mesa-driver",
                "Intel+AMD")

        else:
            self._pct(end_pct)

    def _configure_nvidia_modeset(self):
        modprobe_conf = "options nvidia_drm modeset=1\n"
        self._chroot(
            f"mkdir -p /etc/modprobe.d && "
            f"echo {shlex.quote(modprobe_conf)} > /etc/modprobe.d/nvidia.conf",
            ignore_error=True
        )
        write_log("nvidia_drm modeset=1 configured in /etc/modprobe.d/nvidia.conf")

    def _configure_mkinitcpio(self):
        self._chroot_critical("mkinitcpio -P", "mkinitcpio")

    def _configure_grub_cmdline(self):
        if state["filesystem"] == "btrfs":
            self._chroot(
                "grep -q 'rootflags=subvol=@' /etc/default/grub || "
                "sed -i "
                "'s|GRUB_CMDLINE_LINUX_DEFAULT=\"\\(.*\\)\"|GRUB_CMDLINE_LINUX_DEFAULT=\"\\1 rootflags=subvol=@\"|' "
                "/etc/default/grub",
                ignore_error=True
            )

    def _configure_keyboard(self, keymap_val):
        self._chroot(f"echo 'KEYMAP={keymap_val}' > /etc/vconsole.conf")

        x11_layout = CONSOLE_TO_X11.get(keymap_val, keymap_val)

        xorg_conf = (
            'Section "InputClass"\n'
            '    Identifier "system-keyboard"\n'
            '    MatchIsKeyboard "on"\n'
            f'    Option "XkbLayout" "{x11_layout}"\n'
            'EndSection\n'
        )
        self._chroot(
            f"mkdir -p /etc/X11/xorg.conf.d && "
            f"printf {shlex.quote(xorg_conf)} "
            f"> /etc/X11/xorg.conf.d/00-keyboard.conf",
            ignore_error=True
        )

        self._chroot(
            f"localectl --no-ask-password set-x11-keymap {shlex.quote(x11_layout)} || true",
            ignore_error=True
        )

        write_log(f"Keyboard configured: console={keymap_val}  x11={x11_layout}")

    def _install_flatpak(self, uname):
        self._stage(L("Installing Flatpak + Flathub...", "Instalando Flatpak + Flathub..."))
        self._pacman(
            "arch-chroot /mnt pacman -S --noconfirm flatpak",
            98, 99, ignore_error=True
        )
        self._chroot(
            f"su - {shlex.quote(uname)} -c "
            "'flatpak remote-add --if-not-exists flathub "
            "https://dl.flathub.org/repo/flathub.flatpakrepo'",
            ignore_error=True
        )
        write_log("Flatpak + Flathub configured.")


    def run(self):
        disk_path  = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)
        fs         = state["filesystem"]
        kernel     = state["kernel"]
        microcode  = detect_cpu()
        uefi       = is_uefi()
        bootloader = state.get("bootloader", "grub")
        uname      = state["username"]

        locale     = state.get("locale", "en_US.UTF-8")
        keymap_val = state["keymap"]

        if bootloader == "systemd-boot" and not uefi:
            write_log("systemd-boot requested but BIOS detected - falling back to GRUB")
            bootloader = "grub"
            state["bootloader"] = "grub"

        if bootloader == "limine" and not uefi and fs == "btrfs":
            write_log("Limine + BIOS + BTRFS: supported from Limine v8+, proceeding.")

        root_dev = p3

        try:
            self._stage(L("Checking network...", "Verificando red..."))
            if not ensure_network():
                self.on_done(False, L(
                    "No network connection. Connect and retry.",
                    "Sin conexion de red. Conectese e intente de nuevo."
                ))
                return

            run_stream("pacman -Sy --noconfirm archlinux-keyring",
                       on_line=self._log, ignore_error=True)

            if state.get("mirrors", True):
                self._stage(L("Optimizing mirrors with reflector...",
                              "Optimizando mirrors con reflector..."))
                run_stream("pacman -Sy --noconfirm reflector",
                           on_line=self._log, ignore_error=True)
                run_stream(
                    "reflector --latest 10 --sort rate --save /etc/pacman.d/mirrorlist",
                    on_line=self._log, ignore_error=True
                )
            self._pct(5)

            self._stage(L("Wiping disk...", "Borrando disco..."))
            self._gradual(7)
            run_stream(f"wipefs -a {disk_path}", on_line=self._log, ignore_error=True)
            self._run_critical(f"sgdisk -Z {disk_path}", "sgdisk -Z")
            self._settle(disk_path)
            self._pct(8)

            self._stage(L("Creating partitions...", "Creando particiones..."))
            if uefi:
                self._run_critical(f"sgdisk -n1:0:+1G -t1:ef00 {disk_path}", "sgdisk EFI")
            else:
                self._run_critical(f"sgdisk -n1:0:+1M -t1:ef02 {disk_path}", "sgdisk BIOS boot")
            self._run_critical(
                f"sgdisk -n2:0:+{state['swap']}G -t2:8200 {disk_path}", "sgdisk swap"
            )
            self._run_critical(f"sgdisk -n3:0:0 -t3:8300 {disk_path}", "sgdisk root")
            self._settle(disk_path)
            self._pct(12)

            self._stage(L("Formatting partitions...", "Formateando particiones..."))
            if uefi:
                self._run_critical(f"mkfs.fat -F32 {p1}", "mkfs.fat")
            self._run_critical(f"mkswap {p2}", "mkswap")
            run_stream(f"swapon {p2}", on_line=self._log, ignore_error=True)

            if fs == "btrfs":
                self._setup_btrfs(root_dev, disk_path)
            else:
                self._run_critical(f"mkfs.ext4 -F {root_dev}", "mkfs.ext4")
                self._run_critical(f"mount {root_dev} /mnt", "mount root")
            self._pct(16)

            if uefi:
                self._stage(L("Mounting EFI...", "Montando EFI..."))
                if bootloader == "systemd-boot":
                    self._run_critical("mkdir -p /mnt/boot",    "mkdir /mnt/boot")
                    self._run_critical(f"mount {p1} /mnt/boot", "mount ESP /mnt/boot")
                else:
                    self._run_critical("mkdir -p /mnt/boot/efi",    "mkdir /mnt/boot/efi")
                    self._run_critical(f"mount {p1} /mnt/boot/efi", "mount ESP /mnt/boot/efi")
            self._pct(18)

            ucode_pkg   = f" {microcode}" if microcode else ""
            extra_pkgs  = " btrfs-progs" if fs == "btrfs" else ""
            kernel_hdrs = f"{kernel}-headers"

            if bootloader == "systemd-boot":
                boot_pkgs = " efibootmgr"
            elif bootloader == "limine":
                boot_pkgs = " limine efibootmgr" if uefi else " limine"
            else:
                efi_flag  = " efibootmgr" if uefi else ""
                boot_pkgs = f" grub{efi_flag}"

            pkgs = (
                f"base {kernel} linux-firmware {kernel_hdrs} sof-firmware "
                f"base-devel{boot_pkgs} vim nano networkmanager git "
                f"sudo bash-completion{extra_pkgs}{ucode_pkg}"
            )

            self._stage(L("Installing base system - this may take a while...",
                          "Instalando sistema base - esto puede tardar..."))
            self._pacman_critical(f"pacstrap -K /mnt {pkgs}", 18, 52, "pacstrap")

            self._stage(L("Generating fstab...", "Generando fstab..."))
            self._run_critical("genfstab -U /mnt >> /mnt/etc/fstab", "genfstab")
            self._pct(53)

            self._stage(L("Configuring hostname...", "Configurando hostname..."))
            hn = state["hostname"]
            with open("/mnt/etc/hostname", "w") as f:
                f.write(hn + "\n")
            with open("/mnt/etc/hosts", "w") as f:
                f.write(
                    f"127.0.0.1\tlocalhost\n"
                    f"::1\t\tlocalhost\n"
                    f"127.0.1.1\t{hn}.localdomain\t{hn}\n"
                )
            self._pct(55)

            self._stage(L("Configuring locale & timezone...",
                          "Configurando locale y zona horaria..."))
            locale_line = f"{locale} UTF-8"
            self._chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            if locale != "en_US.UTF-8":
                self._chroot(
                    f"sed -i 's/^#{locale_line}/{locale_line}/' /etc/locale.gen",
                    ignore_error=True
                )
            self._chroot_critical("locale-gen", "locale-gen")
            self._chroot(f"echo 'LANG={locale}' > /etc/locale.conf")
            self._chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
            self._chroot("hwclock --systohc")
            self._pct(59)

            self._stage(L("Configuring keyboard layout...",
                          "Configurando distribucion de teclado..."))
            self._configure_keyboard(keymap_val)
            self._pct(61)

            self._stage(L("Generating initramfs...", "Generando initramfs..."))
            self._configure_mkinitcpio()
            self._pct(63)

            self._stage(L(f"Creating user '{uname}'...", f"Creando usuario '{uname}'..."))
            self._chroot_critical(
                f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}",
                "useradd"
            )
            self._chroot(
                "sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/"
                "%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers"
            )
            self._pct(63)

            self._stage(L("Setting passwords...", "Estableciendo contrasenas..."))
            self._chroot_passwd("root", state["root_pass"])
            self._chroot_passwd(uname, state["user_pass"])
            self._pct(68)

            self._stage(L("Enabling NetworkManager...", "Habilitando NetworkManager..."))
            self._chroot("systemctl enable NetworkManager")
            if is_ssd(disk_path):
                self._chroot("systemctl enable fstrim.timer", ignore_error=True)
                write_log("SSD detected - fstrim.timer enabled.")
            self._pct(71)

            self._install_gpu_drivers(71, 77)
            self._pct(77)

            desktop = state["desktop"]
            if desktop != "None":
                self._stage(L(f"Installing {desktop}...", f"Instalando {desktop}..."))
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

                if desktop in ("Hyprland", "Sway"):
                    self._stage(L(
                        f"Enabling seat management for {desktop}...",
                        f"Habilitando gestion de seat para {desktop}..."
                    ))
                    self._chroot(
                        f"usermod -aG seat,input,video {shlex.quote(uname)}",
                        ignore_error=True
                    )
                    write_log(f"{desktop}: user added to seat/input/video groups.")

                self._stage(L("Installing audio (pipewire)...", "Instalando audio (pipewire)..."))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "pipewire pipewire-pulse wireplumber",
                    91, 94, ignore_error=True
                )

            self._pct(94)

            if bootloader == "systemd-boot":
                self._stage(L("Installing systemd-boot...", "Instalando systemd-boot..."))
                self._install_systemd_boot(root_dev)
            elif bootloader == "limine":
                self._stage(L("Installing Limine bootloader...", "Instalando Limine..."))
                self._install_limine(disk_path, root_dev)
            else:
                self._stage(L("Installing GRUB bootloader...", "Instalando GRUB..."))
                self._configure_grub_cmdline()
                self._install_grub(disk_path)
            self._pct(97)

            if state.get("snapper") and fs == "btrfs":
                self._stage(L("Setting up snapper (BTRFS snapshots)...",
                              "Configurando snapper (snapshots BTRFS)..."))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "snapper snap-pac grub-btrfs inotify-tools",
                    97, 98, ignore_error=True
                )
                self._chroot("snapper -c root create-config /")
                self._chroot("umount /.snapshots", ignore_error=True)
                self._chroot("rm -rf /.snapshots", ignore_error=True)
                self._chroot("mkdir -p /.snapshots")
                self._chroot("mount -a", ignore_error=True)
                self._chroot("chmod 750 /.snapshots")
                self._chroot("systemctl enable snapper-timeline.timer")
                self._chroot("systemctl enable snapper-cleanup.timer")
                if bootloader == "grub":
                    self._chroot("systemctl enable grub-btrfs.path")
                    self._chroot("grub-mkconfig -o /boot/grub/grub.cfg")

            if state.get("yay"):
                self._stage(L("Installing yay (AUR helper)...", "Instalando yay (AUR helper)..."))
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

            if state.get("flatpak") and desktop != "None":
                self._install_flatpak(uname)

            self._pct(100)
            self._stage(L("Installation complete!", "Instalacion completa!"))
            self.on_done(True, "")

        except RuntimeError as e:
            self._log(f"CRITICAL ERROR: {e}")
            self.on_done(False, str(e))
        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))


def screen_welcome():
    boot_mode = L("UEFI", "UEFI") if is_uefi() else L("BIOS (Legacy)", "BIOS (Legacy)")
    text = (
        "\\Zb\\Z4Welcome to the Arch Linux Installer\\Zn\n\n"
        f"Version: {VERSION}    Boot mode: \\Zb{boot_mode}\\Zn\n\n"
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
        L("Install Mode", "Modo de instalacion"),
        L(
            "Quick Install  -  BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
            "Custom Install -  configure everything step by step",
            "Instalacion rapida  -  BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
            "Instalacion personalizada - configura todo paso a paso"
        ),
        [
            ("quick",  L("Quick Install   (sane defaults, installs yay + snapper)",
                         "Instalacion rapida   (valores por defecto, instala yay + snapper)")),
            ("custom", L("Custom Install  (full control)",
                         "Instalacion personalizada  (control total)")),
        ]
    )
    if result == "quick":
        state["quick"]      = True
        state["filesystem"] = "btrfs"
        state["kernel"]     = "linux"
        state["desktop"]    = "KDE Plasma"
        state["mirrors"]    = True
        state["gpu"]        = detect_gpu()
        state["yay"]        = True
        state["snapper"]    = True
        state["bootloader"] = "grub"
        state["swap"]       = str(suggest_swap_gb())
    return result == "quick"

def screen_identity():
    while True:
        hn = inputbox(
            L("System Identity", "Identidad del sistema"),
            L("Enter hostname (letters, digits, -, _ - max 32 chars):",
              "Ingresa el nombre del equipo (letras, digitos, -, _ - max 32):"),
            state.get("hostname", "")
        )
        if hn is None:
            return False
        if not validate_name(hn):
            msgbox(
                L("Invalid hostname", "Hostname invalido"),
                L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                  "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres.")
            )
            continue

        un = inputbox(
            L("System Identity", "Identidad del sistema"),
            L("Enter username (letters, digits, -, _ - max 32 chars):",
              "Ingresa el nombre de usuario (letras, digitos, -, _ - max 32):"),
            state.get("username", "")
        )
        if un is None:
            return False
        if not validate_name(un):
            msgbox(
                L("Invalid username", "Usuario invalido"),
                L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                  "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres.")
            )
            continue

        state["hostname"] = hn
        state["username"] = un
        return True

def screen_passwords():
    while True:
        rp1 = passwordbox(
            L("Passwords", "Contrasenas"),
            L("Enter ROOT password:", "Ingresa la contrasena de ROOT:")
        )
        if rp1 is None:
            return False
        rp2 = passwordbox(
            L("Passwords", "Contrasenas"),
            L("Confirm ROOT password:", "Confirma la contrasena de ROOT:")
        )
        if rp2 is None:
            return False
        if not rp1:
            msgbox(L("Error", "Error"), L("Root password cannot be empty.",
                                          "La contrasena root no puede estar vacia."))
            continue
        if rp1 != rp2:
            msgbox(L("Error", "Error"), L("Root passwords do not match.",
                                          "Las contrasenas root no coinciden."))
            continue

        up1 = passwordbox(
            L("Passwords", "Contrasenas"),
            L("Enter USER password:", "Ingresa la contrasena de USUARIO:")
        )
        if up1 is None:
            return False
        up2 = passwordbox(
            L("Passwords", "Contrasenas"),
            L("Confirm USER password:", "Confirma la contrasena de USUARIO:")
        )
        if up2 is None:
            return False
        if not up1:
            msgbox(L("Error", "Error"), L("User password cannot be empty.",
                                          "La contrasena de usuario no puede estar vacia."))
            continue
        if up1 != up2:
            msgbox(L("Error", "Error"), L("User passwords do not match.",
                                          "Las contrasenas de usuario no coinciden."))
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

    try:
        lsblk_info = subprocess.check_output(
            "lsblk -f 2>/dev/null | head -40", shell=True, text=True
        )
    except Exception:
        lsblk_info = ""

    if lsblk_info:
        msgbox(
            L("Disk Overview - Read before selecting!", "Vista de discos - Lee antes de elegir!"),
            L(
                "Current disk layout (lsblk -f):\n\n" + lsblk_info +
                "\nWARNING: The disk you select will be COMPLETELY ERASED.",
                "Layout actual de discos (lsblk -f):\n\n" + lsblk_info +
                "\nADVERTENCIA: El disco seleccionado se BORRARA COMPLETAMENTE."
            )
        )

    items   = [(f"/dev/{n}", f"{gb} GB  -  {model}") for n, gb, model in disks]
    default = f"/dev/{state['disk']}" if state["disk"] else items[0][0]

    result = radiolist(
        L("Disk Selection", "Seleccion de disco"),
        L("WARNING: ALL DATA on the selected disk will be ERASED!\n\n"
          "Select the installation disk:",
          "ADVERTENCIA: Se borraran todos los datos del disco seleccionado!\n\n"
          "Selecciona el disco:"),
        items,
        default=default
    )
    if result is None:
        return False

    if not yesno(
        L("Confirm Disk Erase", "Confirmar borrado de disco"),
        L(
            f"You selected: {result}\n\n"
            "ALL data on this disk will be permanently destroyed.\n\n"
            "Are you absolutely sure?",
            f"Seleccionaste: {result}\n\n"
            "TODOS los datos en este disco se destruiran permanentemente.\n\n"
            "Estas completamente seguro?"
        )
    ):
        return False

    state["disk"] = result.replace("/dev/", "")

    suggested = str(suggest_swap_gb())
    while True:
        swap = inputbox(
            L("Swap Size", "Tamano de Swap"),
            L(
                f"Suggested swap size based on your RAM: {suggested} GB\n\n"
                "Enter swap size in GB (1-128):",
                f"Tamano de swap sugerido segun tu RAM: {suggested} GB\n\n"
                "Ingresa el tamano del swap en GB (1-128):"
            ),
            state.get("swap", suggested)
        )
        if swap is None:
            return False
        if validate_swap(swap.strip()):
            state["swap"] = swap.strip()
            return True
        msgbox(L("Invalid swap", "Swap invalido"),
               L("Swap must be a number between 1 and 128.",
                 "El swap debe ser un numero entre 1 y 128."))

def screen_filesystem():
    options = [
        ("ext4",  L("ext4  - stable, widely supported",
                    "ext4  - estable, amplio soporte")),
        ("btrfs", L("btrfs - subvolumes (@, @home, @var, @snapshots) + zstd compression",
                    "btrfs - subvolumenes (@, @home, @var, @snapshots) + compresion zstd")),
    ]
    result = radiolist(
        L("Filesystem", "Sistema de archivos"),
        L("Choose the root filesystem:", "Elige el sistema de archivos raiz:"),
        options,
        default=state["filesystem"]
    )
    if result is None:
        return False
    state["filesystem"] = result
    return True

def screen_kernel():
    options = [
        ("linux",     L("linux     - latest stable kernel",
                        "linux     - kernel estable mas reciente")),
        ("linux-lts", L("linux-lts - long-term support kernel",
                        "linux-lts - kernel de soporte a largo plazo")),
        ("linux-zen", L("linux-zen - optimized for desktop / gaming",
                        "linux-zen - optimizado para escritorio / gaming")),
    ]
    result = radiolist(
        L("Kernel", "Kernel"),
        L("Select the kernel to install:", "Selecciona el kernel a instalar:"),
        options,
        default=state["kernel"]
    )
    if result is None:
        return False
    state["kernel"] = result
    return True

def screen_bootloader():
    uefi = is_uefi()

    if not uefi:
        options = [
            (
                "grub",
                L(
                    "GRUB         - stable, recommended for BIOS",
                    "GRUB         - estable, recomendado para BIOS",
                ),
            ),
            (
                "limine",
                L(
                    "Limine       - modern, lightweight, BIOS + UEFI",
                    "Limine       - moderno, ligero, BIOS + UEFI",
                ),
            ),
        ]
    else:
        options = [
            (
                "grub",
                L(
                    "GRUB         - stable, UEFI and BIOS",
                    "GRUB         - estable, UEFI y BIOS",
                ),
            ),
            (
                "systemd-boot",
                L(
                    "systemd-boot - fast, UEFI only",
                    "systemd-boot - rapido, solo UEFI",
                ),
            ),
            (
                "limine",
                L(
                    "Limine       - modern, lightweight, UEFI only",
                    "Limine       - moderno, ligero, solo UEFI",
                ),
            ),
        ]

    result = radiolist(
        L("Bootloader", "Gestor de arranque"),
        L(
            "Choose a bootloader:\n\n"
            "  GRUB         works on UEFI and BIOS legacy.\n"
            "  systemd-boot UEFI only, minimal and fast.\n"
            "  Limine       modern, lightweight, simple config.",
            "Elige un gestor de arranque:\n\n"
            "  GRUB         UEFI y BIOS.\n"
            "  systemd-boot Solo UEFI, minimalista y rapido.\n"
            "  Limine       Moderno, ligero, config sencilla.",
        ),
        options,
        default=state.get("bootloader", "grub"),
    )
    if result is None:
        return False
    state["bootloader"] = result
    return True

def screen_mirrors():
    result = radiolist(
        L("Mirror Optimization", "Optimizacion de mirrors"),
        L("Use reflector to select the 10 fastest mirrors? (recommended)",
          "Usar reflector para seleccionar los 10 mirrors mas rapidos? (recomendado)"),
        [
            ("yes", L("Yes - auto-select fastest mirrors",
                      "Si - seleccionar mirrors mas rapidos")),
            ("no",  L("No  - keep default mirrors",
                      "No  - mantener mirrors por defecto")),
        ],
        default="yes" if state["mirrors"] else "no"
    )
    if result is None:
        return False
    state["mirrors"] = (result == "yes")
    return True


def screen_locale():
    options = [
        ("en_US.UTF-8", "English (United States)      en_US.UTF-8"),
        ("en_GB.UTF-8", "English (United Kingdom)     en_GB.UTF-8"),
        ("es_ES.UTF-8", "Espanol (Espana)              es_ES.UTF-8"),
        ("es_MX.UTF-8", "Espanol (Mexico)              es_MX.UTF-8"),
        ("es_AR.UTF-8", "Espanol (Argentina)           es_AR.UTF-8"),
        ("fr_FR.UTF-8", "Francais (France)             fr_FR.UTF-8"),
        ("de_DE.UTF-8", "Deutsch (Deutschland)         de_DE.UTF-8"),
        ("it_IT.UTF-8", "Italiano (Italia)             it_IT.UTF-8"),
        ("pt_PT.UTF-8", "Portugues (Portugal)          pt_PT.UTF-8"),
        ("pt_BR.UTF-8", "Portugues (Brasil)            pt_BR.UTF-8"),
        ("ru_RU.UTF-8", "Russkiy (Rossiya)             ru_RU.UTF-8"),
        ("pl_PL.UTF-8", "Polski (Polska)               pl_PL.UTF-8"),
        ("nl_NL.UTF-8", "Nederlands (Nederland)        nl_NL.UTF-8"),
        ("cs_CZ.UTF-8", "Cestina (Ceska republika)     cs_CZ.UTF-8"),
        ("hu_HU.UTF-8", "Magyar (Magyarorszag)         hu_HU.UTF-8"),
        ("ro_RO.UTF-8", "Romana (Romania)              ro_RO.UTF-8"),
        ("da_DK.UTF-8", "Dansk (Danmark)               da_DK.UTF-8"),
        ("nb_NO.UTF-8", "Norsk (Norge)                 nb_NO.UTF-8"),
        ("sv_SE.UTF-8", "Svenska (Sverige)             sv_SE.UTF-8"),
        ("fi_FI.UTF-8", "Suomi (Suomi)                 fi_FI.UTF-8"),
        ("tr_TR.UTF-8", "Turkce (Turkiye)              tr_TR.UTF-8"),
        ("ja_JP.UTF-8", "Japanese (Japan)              ja_JP.UTF-8"),
        ("ko_KR.UTF-8", "Korean (Korea)                ko_KR.UTF-8"),
        ("zh_CN.UTF-8", "Chinese Simplified (China)    zh_CN.UTF-8"),
        ("ar_SA.UTF-8", "Arabic (Saudi Arabia)         ar_SA.UTF-8"),
    ]

    result = radiolist(
        L("System Locale", "Idioma del sistema instalado"),
        L(
            "Choose the locale for the INSTALLED SYSTEM.\n"
            "This is INDEPENDENT of the installer UI language.\n\n"
            "Controls: system language, date/number formats, etc.",
            "Elige el locale para el SISTEMA INSTALADO.\n"
            "Es INDEPENDIENTE del idioma de este instalador.\n\n"
            "Controla: idioma del sistema, formatos de fecha/numeros, etc."
        ),
        options,
        default=state.get("locale", "en_US.UTF-8")
    )
    if result is None:
        return False

    state["locale"] = result

    suggested_km = LOCALE_TO_KEYMAP.get(result)
    if suggested_km and suggested_km != state["keymap"]:
        if yesno(
            L("Keyboard suggestion", "Sugerencia de teclado"),
            L(
                f"The locale '{result}' typically uses keymap '{suggested_km}'.\n\n"
                f"Current keymap: '{state['keymap']}'\n\n"
                f"Switch keymap to '{suggested_km}'?",
                f"El locale '{result}' suele usar el teclado '{suggested_km}'.\n\n"
                f"Teclado actual: '{state['keymap']}'\n\n"
                f"Cambiar teclado a '{suggested_km}'?"
            )
        ):
            state["keymap"] = suggested_km
            run_simple(f"loadkeys {shlex.quote(suggested_km)}", ignore_error=True)

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

    wanted = [
        "us", "es", "uk", "fr", "de", "it", "ru", "ara",
        "pt-latin9", "br-abnt2", "pl2", "hu", "cz-qwerty",
        "sk-qwerty", "ro_win", "dk", "no", "sv-latin1",
        "fi", "nl", "tr_q-latin5", "ja106", "kr106",
    ]
    options = [m for m in wanted if m in maps] if maps else wanted
    items   = [(m, f"Keyboard layout: {m}") for m in options]

    result = radiolist(
        L("Keyboard Layout", "Distribucion de teclado"),
        L(
            "Select your keyboard layout.\n"
            "Applied to both TTY console and the desktop (X11/Wayland).",
            "Selecciona la distribucion de teclado.\n"
            "Se aplica a la TTY y al escritorio (X11/Wayland)."
        ),
        items,
        default=state["keymap"]
    )
    if result is None:
        return False
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
        L("Timezone - Region", "Zona horaria - Region"),
        L("Select your region:", "Selecciona tu region:"),
        [(r, r) for r in regions],
        default=cur_region
    )
    if region is None:
        return False
    if region == "UTC":
        state["timezone"] = "UTC"
        return True

    cities = [
        (z.split("/", 1)[1], z.split("/", 1)[1])
        for z in zones if z.startswith(region + "/")
    ]
    if not cities:
        state["timezone"] = region
        return True

    cur_city = state["timezone"].split("/", 1)[1] if "/" in state["timezone"] else ""
    city = radiolist(
        L("Timezone - City", "Zona horaria - Ciudad"),
        L(f"Region: {region}\nSelect your city:",
          f"Region: {region}\nSelecciona tu ciudad:"),
        cities,
        default=cur_city
    )
    if city:
        state["timezone"] = f"{region}/{city}"
    return True

def screen_desktop():
    options = [
        ("KDE Plasma", L("KDE Plasma - full featured, modern",
                         "KDE Plasma - completo, moderno")),
        ("GNOME",      L("GNOME     - clean, Wayland-first",
                         "GNOME     - limpio, Wayland primero")),
        ("Cinnamon",   L("Cinnamon  - classic, Windows-like",
                         "Cinnamon  - clasico, similar a Windows")),
        ("XFCE",       L("XFCE      - lightweight, traditional",
                         "XFCE      - ligero, tradicional")),
        ("MATE",       L("MATE      - GNOME 2 fork, very stable",
                         "MATE      - fork de GNOME 2, muy estable")),
        ("LXQt",       L("LXQt      - minimal Qt desktop",
                         "LXQt      - escritorio Qt minimalista")),
        ("Hyprland",   L("Hyprland  - tiling Wayland compositor, modern + animations",
                         "Hyprland  - compositor Wayland tiling, moderno + animaciones")),
        ("Sway",       L("Sway      - tiling Wayland compositor, i3-compatible",
                         "Sway      - compositor Wayland tiling, compatible con i3")),
        ("None",       L("None      - CLI only, no desktop",
                         "None      - solo terminal, sin escritorio")),
    ]
    result = radiolist(
        L("Desktop Environment", "Entorno de escritorio"),
        L("Choose a desktop environment:", "Elige un entorno de escritorio:"),
        options,
        default=state["desktop"]
    )
    if result is None:
        return False
    state["desktop"] = result
    return True

def screen_gpu():
    detected = detect_gpu()
    if detected != "None" and state["gpu"] == "None":
        state["gpu"] = detected

    color_map = {
        "NVIDIA":       "\\Z3NVIDIA\\Zn",
        "AMD":          "\\Z2AMD\\Zn",
        "Intel":        "\\Z6Intel\\Zn",
        "Intel+NVIDIA": "\\Z6Intel\\Zn + \\Z3NVIDIA\\Zn (hybrid)",
        "Intel+AMD":    "\\Z6Intel\\Zn + \\Z2AMD\\Zn (hybrid)",
    }
    tag  = color_map.get(detected, "none")
    hint = L(f"Detected GPU: {tag}", f"GPU detectada: {tag}")

    options = [
        ("NVIDIA",       L("NVIDIA proprietary (nvidia/nvidia-dkms + utils)",
                           "NVIDIA propietario (nvidia/nvidia-dkms + utils)")),
        ("AMD",          L("AMD open-source (mesa + vulkan-radeon)",
                           "AMD open-source (mesa + vulkan-radeon)")),
        ("Intel",        L("Intel open-source (mesa + vulkan-intel + intel-media-driver)",
                           "Intel open-source (mesa + vulkan-intel + intel-media-driver)")),
        ("Intel+NVIDIA", L("Intel + NVIDIA hybrid (Mesa + proprietary NVIDIA)",
                           "Intel + NVIDIA hibrido (Mesa + NVIDIA propietario)")),
        ("Intel+AMD",    L("Intel + AMD hybrid (Mesa + vulkan-radeon)",
                           "Intel + AMD hibrido (Mesa + vulkan-radeon)")),
        ("None",         L("No additional GPU drivers", "Sin drivers adicionales de GPU")),
    ]
    result = radiolist(
        L("GPU Drivers", "Drivers GPU"),
        L(f"{hint}\n\nSelect GPU driver:", f"{hint}\n\nSelecciona el driver de GPU:"),
        options,
        default=state["gpu"]
    )
    if result is None:
        return False
    state["gpu"] = result
    return True

def screen_yay():
    result = radiolist(
        L("AUR Helper", "AUR Helper"),
        L("Install yay? (AUR helper, lets you install packages from the AUR)",
          "Instalar yay? (AUR helper, permite instalar paquetes del AUR)"),
        [
            ("yes", L("Yes - install yay after base setup",
                      "Si - instalar yay al finalizar")),
            ("no",  L("No  - skip",
                      "No  - omitir")),
        ],
        default="yes" if state["yay"] else "no"
    )
    if result is None:
        return False
    state["yay"] = (result == "yes")
    return True

def screen_snapper():
    if state["filesystem"] != "btrfs":
        state["snapper"] = False
        return True
    result = radiolist(
        L("BTRFS Snapshots", "Snapshots BTRFS"),
        L(
            "Install snapper + grub-btrfs for automatic rollback snapshots?\n"
            "(Requires BTRFS filesystem - already selected)",
            "Instalar snapper + grub-btrfs para snapshots y rollback automatico?\n"
            "(Requiere BTRFS - ya seleccionado)"
        ),
        [
            ("yes", L("Yes - automatic snapshots on every pacman transaction",
                      "Si - snapshots automaticos en cada transaccion pacman")),
            ("no",  L("No  - skip",
                      "No  - omitir")),
        ],
        default="yes" if state["snapper"] else "no"
    )
    if result is None:
        return False
    state["snapper"] = (result == "yes")
    return True

def screen_flatpak():
    if state["desktop"] == "None":
        state["flatpak"] = False
        return True
    result = radiolist(
        L("Flatpak", "Flatpak"),
        L(
            "Install Flatpak and add the Flathub repository?\n\n"
            "Flatpak lets you install thousands of apps from Flathub\n"
            "independently of Arch packages.",
            "Instalar Flatpak y anadir el repositorio Flathub?\n\n"
            "Flatpak permite instalar miles de aplicaciones de Flathub\n"
            "de forma independiente a los paquetes de Arch."
        ),
        [
            ("yes", L("Yes - install Flatpak + add Flathub",
                      "Si - instalar Flatpak + anadir Flathub")),
            ("no",  L("No  - skip",
                      "No  - omitir")),
        ],
        default="yes" if state.get("flatpak") else "no"
    )
    if result is None:
        return False
    state["flatpak"] = (result == "yes")
    return True

def screen_review():
    microcode = detect_cpu() or L("none detected", "no detectado")
    quick_tag = L("  [Quick Install]", "  [Instalacion rapida]") if state["quick"] else ""
    boot_mode = L("UEFI", "UEFI") if is_uefi() else L("BIOS", "BIOS")

    x11_layout = CONSOLE_TO_X11.get(state["keymap"], state["keymap"])

    lines = [
        ("Mode",                    L("Quick", "Rapida") if state["quick"] else L("Custom", "Personalizada")),
        ("Boot",                    boot_mode),
        ("Installer lang",          state["lang"]),
        ("System locale",           state.get("locale", "en_US.UTF-8")),
        ("Hostname",                state["hostname"] or "NOT SET"),
        (L("Username", "Usuario"),  state["username"] or "NOT SET"),
        ("Filesystem",              state["filesystem"]),
        ("Kernel",                  state["kernel"]),
        ("Bootloader",              state["bootloader"]),
        ("Microcode",               microcode),
        ("Disk",                    f"/dev/{state['disk']}" if state["disk"] else "NOT SET"),
        ("Swap",                    f"{state['swap']} GB"),
        ("Mirrors",                 L("reflector (auto)", "reflector (auto)") if state["mirrors"] else L("default", "por defecto")),
        ("Keymap TTY",              state["keymap"]),
        ("Keymap X11",              x11_layout),
        ("Timezone",                state["timezone"]),
        ("Desktop",                 state["desktop"]),
        ("GPU",                     state["gpu"]),
        ("Audio",                   "pipewire" if state["desktop"] != "None" else L("none", "ninguno")),
        ("Flatpak",                 L("yes", "si") if state.get("flatpak") else "no"),
        ("yay",                     L("yes", "si") if state["yay"] else "no"),
        ("snapper",                 L("yes", "si") if state.get("snapper") else "no"),
    ]

    text    = L(f"Review your settings:{quick_tag}\n\n",
                f"Revisa tu configuracion:{quick_tag}\n\n")
    missing = []

    for label, val in lines:
        text += f"  {label:<18} {val}\n"
    text += "\n"

    if not state["hostname"]:  missing.append("hostname")
    if not state["username"]:  missing.append("username")
    if not state["disk"]:      missing.append("disk")
    if not state["root_pass"]: missing.append(L("root password", "contrasena root"))

    if missing:
        text += L(f"MISSING: {', '.join(missing)}\n\nGo back to fix before continuing.",
                  f"FALTA: {', '.join(missing)}\n\nVuelve atras para corregirlo.")
        msgbox(L("Review - Incomplete", "Revision - Incompleto"), text)
        return False

    text += L("All settings look good.", "Todo listo.")
    return yesno(
        L("Review & Confirm", "Revisar y confirmar"),
        text + L(
            f"\n\nWARNING: THIS WILL ERASE /dev/{state['disk']}.\n\nProceed?",
            f"\n\nADVERTENCIA: SE BORRARA /dev/{state['disk']}.\n\nProceder?"
        )
    )

class InstallScreen:
    _RST  = "\033[0m"
    _BOLD = "\033[1m"
    _DIM  = "\033[2m"
    _HIDE = "\033[?25l"
    _SHOW = "\033[?25h"
    _CLS  = "\033[2J\033[H"

    _BG_BLUE  = "\033[44m\033[97m"
    _BG_DARK  = "\033[100m"
    _FG_CYAN  = "\033[96m"
    _FG_YEL   = "\033[93m"
    _FG_RED   = "\033[91m"
    _FG_GRN   = "\033[92m"
    _FG_GRAY  = "\033[90m"

    def __init__(self, title, version):
        self._title        = title
        self._version      = version
        self._mode         = "progress"
        self._prev_mode    = "progress"
        self._pct          = 0.0
        self._stage        = L("Preparing...", "Preparando...")
        self._lines        = []
        self._lock         = threading.Lock()
        self._running      = False
        self._keybuf       = ""
        self._old_tty      = None

    def start(self):
        self._running = True
        if sys.stdin.isatty():
            import tty, termios
            self._old_tty = termios.tcgetattr(sys.stdin.fileno())
            tty.setraw(sys.stdin.fileno())
        sys.stdout.write(self._HIDE + self._CLS)
        sys.stdout.flush()
        threading.Thread(target=self._render_loop, daemon=True).start()
        threading.Thread(target=self._key_loop,    daemon=True).start()

    def stop(self):
        self._running = False
        time.sleep(0.15)
        if self._old_tty is not None:
            import termios
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self._old_tty)
        sys.stdout.write(self._SHOW + self._CLS)
        sys.stdout.flush()

    def update(self, pct, stage):
        with self._lock:
            self._pct   = pct
            self._stage = stage

    def feed(self, line):
        with self._lock:
            self._lines.append(line)
            if len(self._lines) > 2000:
                self._lines = self._lines[-1000:]

    def _size(self):
        sz = shutil.get_terminal_size((80, 24))
        return sz.lines, sz.columns

    def _goto(self, r, c=1):
        return f"\033[{r};{c}H"

    def _clr(self):
        return "\033[2K"

    def _trunc(self, s, n):
        if len(s) <= n:
            return s
        return s[:n - 1] + "..."

    def _render_loop(self):
        while self._running:
            try:
                self._draw()
            except Exception:
                pass
            time.sleep(0.08)

    def _draw(self):
        with self._lock:
            mode  = self._mode
            pct   = self._pct
            stage = self._stage
            lines = list(self._lines)
            mode_changed    = (mode != self._prev_mode)
            self._prev_mode = mode
        rows, cols = self._size()
        out = []
        if mode_changed:
            out.append(self._CLS)
        if mode == "progress":
            self._draw_progress(out, pct, stage, rows, cols)
        else:
            self._draw_debug(out, pct, stage, lines, rows, cols)
        sys.stdout.write("".join(out))
        sys.stdout.flush()

    def _draw_progress(self, out, pct, stage, rows, cols):
        W   = min(cols - 2, 74)
        lft = max(0, (cols - W) // 2)
        top = max(1, (rows - 6) // 2)
        pad = " " * lft

        def row(r, txt=""):
            out.append(self._goto(r) + self._clr() + pad + txt)

        for r in range(1, top):
            out.append(self._goto(r) + self._clr())
        for r in range(top + 7, rows + 1):
            out.append(self._goto(r) + self._clr())

        title_str = f" {self._title}  -  {self._version} "
        row(top,
            self._BG_BLUE + self._BOLD + title_str.center(W) + self._RST)
        row(top + 1)
        row(top + 2,
            f"  {self._BOLD}{self._trunc(stage, W - 4)}{self._RST}")
        row(top + 3)

        bar_w  = W - 9
        filled = int(bar_w * pct / 100)
        empty  = bar_w - filled
        bar = (self._BG_BLUE + " " * filled + self._RST +
               self._BG_DARK + " " * empty  + self._RST)
        pct_s = f"{int(pct):3d}%"
        row(top + 4, f"  [{bar}] {self._BOLD}{pct_s}{self._RST}")
        row(top + 5)
        row(top + 6)

    def _draw_debug(self, out, pct, stage, lines, rows, cols):
        W = cols

        pct_s    = f"{pct:.0f}%"
        hdr_l    = f" DEBUG  {pct_s}  {self._trunc(stage, W - 28)}"
        hdr_r    = "write 'exit' to go back "
        gap      = max(0, W - len(hdr_l) - len(hdr_r))
        hdr      = (self._BG_BLUE + self._BOLD +
                    hdr_l + " " * gap + hdr_r + self._RST)
        out.append(self._goto(1) + self._clr() + hdr)

        out.append(self._goto(2) + self._clr() +
                   self._FG_CYAN + "-" * W + self._RST)

        available = rows - 2
        visible   = lines[-available:] if lines else []
        for i, ln in enumerate(visible):
            if "ERROR" in ln or "FATAL" in ln or "CRITICAL" in ln:
                color = self._FG_RED + self._BOLD
            elif ">>>" in ln:
                color = self._FG_YEL + self._BOLD
            elif ln.startswith("[") and "]" in ln:
                color = self._FG_GRAY
            else:
                color = self._RST
            out.append(
                self._goto(3 + i) + self._clr() +
                color + self._trunc(ln, W - 1) + self._RST
            )
        for i in range(len(visible), available):
            out.append(self._goto(3 + i) + self._clr())

    def _key_loop(self):
        import select as _sel
        while self._running:
            try:
                ready = _sel.select([sys.stdin], [], [], 0.05)[0]
                if ready:
                    ch = os.read(sys.stdin.fileno(), 1).decode("utf-8", errors="ignore")
                    self._keybuf += ch
                    lb = self._keybuf.lower()
                    if lb.endswith("debug"):
                        with self._lock:
                            self._mode = "debug"
                        self._keybuf = ""
                    elif lb.endswith("exit"):
                        with self._lock:
                            self._mode = "progress"
                        self._keybuf = ""
                    if len(self._keybuf) > 20:
                        self._keybuf = self._keybuf[-10:]
            except Exception:
                pass


def screen_install():
    try:
        open(LOG_FILE, "a").close()
    except Exception:
        pass

    screen     = InstallScreen(TITLE, VERSION)
    stop_tail  = threading.Event()
    failed     = [False]
    fail_msg   = [""]
    done_event = threading.Event()
    _pct       = [0.0]
    _stage     = [L("Preparing...", "Preparando...")]

    def _tailer():
        try:
            with open(LOG_FILE, "r", errors="replace") as f:
                f.seek(0, 2)
                while not stop_tail.is_set():
                    line = f.readline()
                    if line:
                        screen.feed(line.rstrip("\n"))
                    else:
                        time.sleep(0.05)
        except Exception:
            pass

    def on_progress(pct):
        _pct[0] = pct
        screen.update(pct, _stage[0])

    def on_stage(msg):
        _stage[0] = msg
        screen.update(_pct[0], msg)

    def on_done(success, reason):
        failed[0]  = not success
        fail_msg[0] = reason
        done_event.set()

    screen.start()
    threading.Thread(target=_tailer, daemon=True).start()
    threading.Thread(
        target=InstallBackend(on_progress, on_stage, on_done).run,
        daemon=True
    ).start()

    done_event.wait()
    stop_tail.set()
    time.sleep(0.15)
    screen.stop()

    if failed[0]:
        msgbox(
            L("Installation Failed", "Instalacion fallida"),
            L(f"Installation failed.\n\n{fail_msg[0]}\n\nCheck {LOG_FILE} for details.",
              f"La instalacion fallo.\n\n{fail_msg[0]}\n\nRevisa {LOG_FILE} para detalles.")
        )
        return False
    return True

def screen_finish():
    ok = yesno(
        L("Installation Complete!", "Instalacion completa!"),
        L("Arch Linux has been installed successfully.\n\n"
          "Remove the installation media. Reboot now?",
          "Arch Linux se ha instalado correctamente.\n\n"
          "Extrae el medio de instalacion. Reiniciar ahora?")
    )
    if ok:
        dlg_titled(
            L("Rebooting", "Reiniciando"),
            "--infobox",
            L("Unmounting filesystems and rebooting...",
              "Desmontando sistemas de archivos y reiniciando..."),
            "5", "50"
        )
        subprocess.run("umount -R /mnt", shell=True)
        subprocess.run("reboot",         shell=True)
    sys.exit(0)


def main():
    screen_welcome()
    screen_language()
    screen_network()
    quick = screen_mode()

    if quick:
        steps = [
            (L("Locale",    "Idioma sistema"), screen_locale,    True),
            (L("Keymap",    "Teclado"),        screen_keymap,    True),
            (L("Disk",      "Disco"),          screen_disk,      True),
            (L("Identity",  "Identidad"),      screen_identity,  True),
            (L("Passwords", "Contrasenas"),    screen_passwords, True),
            (L("Review",    "Revision"),       screen_review,    True),
            (L("Install",   "Instalar"),       screen_install,   False),
            (L("Finish",    "Finalizar"),      screen_finish,    False),
        ]
    else:
        steps = [
            (L("Locale",      "Idioma sistema"),    screen_locale,      True),
            (L("Disk",        "Disco"),             screen_disk,        True),
            (L("Filesystem",  "Sistema archivos"),  screen_filesystem,  True),
            (L("Kernel",      "Kernel"),            screen_kernel,      True),
            (L("Bootloader",  "Bootloader"),        screen_bootloader,  True),
            (L("Mirrors",     "Mirrors"),           screen_mirrors,     True),
            (L("Identity",    "Identidad"),         screen_identity,    True),
            (L("Passwords",   "Contrasenas"),       screen_passwords,   True),
            (L("Keymap",      "Teclado"),           screen_keymap,      True),
            (L("Timezone",    "Zona horaria"),      screen_timezone,    True),
            (L("Desktop",     "Escritorio"),        screen_desktop,     True),
            ("GPU",                                 screen_gpu,         True),
            (L("yay",         "yay"),               screen_yay,         True),
            (L("Flatpak",     "Flatpak"),           screen_flatpak,     True),
            (L("Snapshots",   "Snapshots"),         screen_snapper,     True),
            (L("Review",      "Revision"),          screen_review,      True),
            (L("Install",     "Instalar"),          screen_install,     False),
            (L("Finish",      "Finalizar"),         screen_finish,      False),
        ]

    idx = 0
    while idx < len(steps):
        _, fn, can_go_back = steps[idx]
        result = fn()
        if result is False and can_go_back:
            if idx == 0:
                if yesno(L("Exit", "Salir"),
                         L("Exit the installer?", "Salir del instalador?")):
                    sys.exit(0)
            else:
                idx -= 1
        else:
            idx += 1

def bootstrap():
    if os.geteuid() != 0:
        print("This installer must be run as root.")
        print("Example: sudo python arch_installer_gui.py")
        sys.exit(1)

    if not shutil.which("dialog"):
        print("[*] dialog not found - installing via pacman...")
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
