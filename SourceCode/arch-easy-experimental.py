#omg bro why this fails....


import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

VERSION  = "V1.1.3"
LOG_FILE = "/tmp/arch_install.log"
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
    "locale":     "",
    "filesystem": "ext4",
    "kernel":     "linux",
    "shell":      "bash",
    "mirrors":    True,
    "quick":      False,
    "yay":        False,
    "snapper":    False,
    "bootloader": "grub",
    "flatpak":    False,
    "extras":     False,
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

EXTRA_PKGS = (
    "htop btop wget curl rsync p7zip unzip zip tree which "
    "man-db neofetch bat fd ripgrep lsof strace iotop usbutils"
)

SHELL_PKGS = {
    "bash": [],
    "zsh":  ["zsh", "zsh-completions", "zsh-autosuggestions", "zsh-syntax-highlighting"],
    "fish": ["fish"],
}

LOCALE_OPTIONS = [
    ("en_US.UTF-8", "English (United States)"),
    ("es_ES.UTF-8", "Espanol (Espana)"),
    ("es_MX.UTF-8", "Espanol (Mexico)"),
    ("pt_BR.UTF-8", "Portugues (Brasil)"),
    ("pt_PT.UTF-8", "Portugues (Portugal)"),
    ("fr_FR.UTF-8", "Francais (France)"),
    ("de_DE.UTF-8", "Deutsch (Deutschland)"),
    ("it_IT.UTF-8", "Italiano (Italia)"),
    ("ru_RU.UTF-8", "Russkiy (Rossiya)"),
    ("ja_JP.UTF-8", "Nihongo (Nihon)"),
    ("zh_CN.UTF-8", "Zhongwen (Jianti)"),
    ("ko_KR.UTF-8", "Hangugeo (Hanguk)"),
    ("pl_PL.UTF-8", "Polski (Polska)"),
    ("nl_NL.UTF-8", "Nederlands (Nederland)"),
    ("tr_TR.UTF-8", "Turkce (Turkiye)"),
    ("ar_EG.UTF-8", "Arabiya (Misr)"),
]

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def _nav():
    return L(
        "\n\\Z6 Tab/Arrow\\Zn: move   \\Z6Space\\Zn: select   \\Z6Enter\\Zn: confirm   \\Z6Cancel/Esc\\Zn: go back",
        "\n\\Z6 Tab/Flecha\\Zn: mover   \\Z6Espacio\\Zn: elegir   \\Z6Enter\\Zn: confirmar   \\Z6Cancelar/Esc\\Zn: volver",
    )

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
    rc, val = dlg_titled(title, "--inputbox", text, "0", "64", init)
    if rc != 0:
        return None
    return val

def passwordbox(title, text):
    rc, val = dlg_titled(title, "--insecure", "--passwordbox", text, "0", "64")
    if rc != 0:
        return None
    return val

def menu(title, text, items):
    flat = []
    for tag, desc in items:
        flat.extend([tag, desc])
    height = min(len(items) + 12, 40)
    rc, val = dlg_titled(title, "--menu", text, str(height), "78", str(len(items)), *flat)
    if rc != 0:
        return None
    return val

def radiolist(title, text, items, default=None):
    flat = []
    for tag, desc in items:
        status = "on" if tag == default else "off"
        flat.extend([tag, desc, status])
    height = min(len(items) + 12, 40)
    rc, val = dlg_titled(title, "--radiolist", text, str(height), "78", str(len(items)), *flat)
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
                "No wireless interfaces found.\n\n"
                "Make sure your WiFi adapter is recognized by the live system.\n"
                "You can check with:  ip link",
                "No se encontraron interfaces inalámbricas.\n\n"
                "Verifica que tu adaptador WiFi sea reconocido.\n"
                "Puedes comprobar con:  ip link"
            )
        )
        return None

    iface = ifaces[0]

    dlg_titled(
        L("Scanning…", "Escaneando…"),
        "--infobox",
        L(f"Scanning for networks on {iface}…\nThis takes about 10 seconds.",
          f"Buscando redes en {iface}…\nEsto tarda unos 10 segundos."),
        "5", "52"
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
                f"Interface: \\Zb{iface}\\Zn\n"
                "Select a network — Cancel to go back.\n"
                "(Open networks do not require a password.)",
                f"Interfaz: \\Zb{iface}\\Zn\n"
                "Selecciona una red — Cancelar para volver.\n"
                "(Las redes abiertas no requieren contraseña.)"
            ),
            [(s, s) for s in ssids]
        )
    else:
        ssid = inputbox(
            L("WiFi — SSID", "WiFi — SSID"),
            L(
                f"Interface: {iface}  (no networks found automatically)\n\n"
                "Type the network name (SSID) exactly as it appears,\n"
                "or Cancel to go back:",
                f"Interfaz: {iface}  (no se encontraron redes)\n\n"
                "Escribe el nombre exacto de la red (SSID),\n"
                "o Cancelar para volver:"
            )
        )

    if not ssid:
        return None

    passphrase = passwordbox(
        L("WiFi Password", "Contraseña WiFi"),
        L(
            f"Password for:  \"{ssid}\"\n\n"
            "Leave blank for open (public) networks.\n"
            "Cancel to go back.",
            f"Contraseña de:  \"{ssid}\"\n\n"
            "Deja en blanco si es una red abierta.\n"
            "Cancelar para volver."
        )
    )
    if passphrase is None:
        return None

    dlg_titled(
        L("Connecting…", "Conectando…"),
        "--infobox",
        L(f"Connecting to '{ssid}'…", f"Conectando a '{ssid}'…"),
        "5", "52"
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
            L("Connection Failed", "Conexión fallida"),
            L(
                f"Could not connect to \"{ssid}\".\n\n"
                "Possible reasons:\n"
                "  • Wrong password\n"
                "  • Network out of range\n"
                "  • DHCP not responding\n\n"
                "Press OK to try again.",
                f"No se pudo conectar a \"{ssid}\".\n\n"
                "Posibles causas:\n"
                "  • Contraseña incorrecta\n"
                "  • Red fuera de alcance\n"
                "  • DHCP sin respuesta\n\n"
                "Presiona OK para intentarlo de nuevo."
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
            L("Network Connection", "Conexión de red"),
            L(
                "An active internet connection is \\Zbrequired\\Zn for installation.\n"
                "Packages will be downloaded from the Arch mirrors.\n\n"
                "How are you connected?",
                "Se necesita conexión a internet \\Zbrequerida\\Zn para instalar.\n"
                "Los paquetes se descargan de los mirrors de Arch.\n\n"
                "¿Cómo estás conectado?"
            ),
            [
                ("wired", L(
                    "Wired Ethernet    — cable already plugged in",
                    "Cable Ethernet    — cable ya conectado"
                )),
                ("wifi",  L(
                    "WiFi              — connect to a wireless network",
                    "WiFi              — conectar a red inalámbrica"
                )),
            ]
        )

        if choice is None:
            if yesno(L("Exit", "Salir"), L("Exit the installer?", "¿Salir del instalador?")):
                sys.exit(0)
            continue

        if choice == "wired":
            dlg_titled(
                L("Checking…", "Verificando…"),
                "--infobox",
                L("Testing wired connection…", "Probando conexión por cable…"),
                "5", "50"
            )
            if _check_connectivity():
                msgbox(
                    L("Connected!", "¡Conectado!"),
                    L(
                        "\\Z2✔\\Zn  Wired connection detected — ready to continue.",
                        "\\Z2✔\\Zn  Conexión por cable detectada — listo para continuar."
                    )
                )
                return
            msgbox(
                L("No connection", "Sin conexión"),
                L(
                    "\\Z1✘\\Zn  Could not reach archlinux.org.\n\n"
                    "Check that:\n"
                    "  • The cable is firmly plugged in\n"
                    "  • Your router or switch is powered on\n"
                    "  • DHCP is available on your network\n\n"
                    "Press OK to go back.",
                    "\\Z1✘\\Zn  No se pudo alcanzar archlinux.org.\n\n"
                    "Verifica que:\n"
                    "  • El cable esté bien conectado\n"
                    "  • Tu router o switch esté encendido\n"
                    "  • El DHCP esté disponible en tu red\n\n"
                    "Presiona OK para volver."
                )
            )
            continue

        if choice == "wifi":
            result = screen_wifi_connect()
            if result is True:
                msgbox(
                    L("Connected!", "¡Conectado!"),
                    L(
                        "\\Z2✔\\Zn  WiFi connected — ready to continue.",
                        "\\Z2✔\\Zn  WiFi conectado — listo para continuar."
                    )
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
            L("No network", "Sin red"),
            L("No wired connection found.\nConnect via WiFi?",
              "No se detectó conexión cableada.\n¿Conectar por WiFi?")
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
                  f"{label} falló (rc={rc}). Revisa {LOG_FILE}.")
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
                  f"{label} falló (rc={rc}). Revisa {LOG_FILE}.")
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
                  f"{label} falló (rc={rc}). Revisa {LOG_FILE}.")
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
            self._log(f"SSD detected on {disk_path} — adding ssd,discard=async to BTRFS opts")
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
            f"mount -o {opts},subvol=@snapshots {p3} /mnt/.snapshots", "mount @snapshots"
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

    def _install_gpu_drivers(self, start_pct, end_pct):
        gpu    = state["gpu"]
        kernel = state["kernel"]

        nvidia_pkg = "nvidia" if kernel == "linux" else "nvidia-dkms"

        def _do(pkgs, label):
            self._stage(L(f"Installing {label} drivers…", f"Instalando drivers {label}…"))
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
            self._stage(L("Installing Intel+NVIDIA (hybrid) drivers…",
                          "Instalando drivers Intel+NVIDIA (hybrid)…"))
            self._pacman(
                "arch-chroot /mnt pacman -S --noconfirm "
                "mesa vulkan-intel intel-media-driver",
                start_pct, start_pct + (end_pct - start_pct) * 0.4, ignore_error=True
            )
            self._pacman(
                f"arch-chroot /mnt pacman -S --noconfirm "
                f"{nvidia_pkg} nvidia-utils nvidia-settings nvidia-prime",
                start_pct + (end_pct - start_pct) * 0.4, end_pct, ignore_error=True
            )
            self._configure_nvidia_modeset()
        elif gpu == "Intel+AMD":
            _do("mesa vulkan-intel intel-media-driver vulkan-radeon libva-mesa-driver",
                "Intel+AMD")
        else:
            self._pct(end_pct)

    def _configure_nvidia_modeset(self):
        self._chroot(
            "mkdir -p /etc/modprobe.d && "
            "printf 'options nvidia_drm modeset=1\\n' > /etc/modprobe.d/nvidia.conf",
            ignore_error=True
        )
        write_log("nvidia_drm modeset=1 written to /etc/modprobe.d/nvidia.conf")

    def _configure_mkinitcpio(self):
        self._chroot_critical("mkinitcpio -P", "mkinitcpio")

    def _configure_grub_cmdline(self):
        if state["filesystem"] == "btrfs":
            self._chroot(
                "grep -q 'rootflags=subvol=@' /etc/default/grub || "
                "sed -i "
                r"""'s|GRUB_CMDLINE_LINUX_DEFAULT="\(.*\)"|GRUB_CMDLINE_LINUX_DEFAULT="\1 rootflags=subvol=@"|'"""
                " /etc/default/grub",
                ignore_error=True
            )

    def _install_shell(self, uname):
        shell = state.get("shell", "bash")
        if shell == "bash":
            return
        pkgs = " ".join(SHELL_PKGS[shell])
        self._stage(L(f"Installing {shell}…", f"Instalando {shell}…"))
        self._pacman(
            f"arch-chroot /mnt pacman -S --noconfirm {pkgs}",
            67, 69, ignore_error=True
        )
        shell_bin = f"/usr/bin/{shell}"
        self._chroot(
            f"chsh -s {shell_bin} {shlex.quote(uname)}",
            ignore_error=True
        )
        write_log(f"Default shell for {uname} set to {shell_bin}")

    def _install_flatpak(self, uname):
        self._stage(L("Installing Flatpak + Flathub…", "Instalando Flatpak + Flathub…"))
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
        locale     = state.get("locale") or (
            "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
        )

        if bootloader == "systemd-boot" and not uefi:
            write_log("systemd-boot requested but BIOS detected — falling back to GRUB")
            bootloader = "grub"
            state["bootloader"] = "grub"

        root_dev = p3

        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.on_done(False, L(
                    "No network connection. Connect and retry.",
                    "Sin conexión de red. Conéctese e intente de nuevo."
                ))
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
            run_stream(f"wipefs -a {disk_path}", on_line=self._log, ignore_error=True)
            self._run_critical(f"sgdisk -Z {disk_path}", "sgdisk -Z")
            self._settle(disk_path)
            self._pct(8)

            self._stage(L("Creating partitions…", "Creando particiones…"))
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

            self._stage(L("Formatting partitions…", "Formateando particiones…"))
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
                self._stage(L("Mounting EFI partition…", "Montando partición EFI…"))
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
            else:
                efi_flag  = " efibootmgr" if uefi else ""
                boot_pkgs = f" grub{efi_flag}"

            pkgs = (
                f"base {kernel} linux-firmware {kernel_hdrs} sof-firmware "
                f"base-devel{boot_pkgs} vim nano networkmanager git "
                f"sudo bash-completion{extra_pkgs}{ucode_pkg}"
            )

            self._stage(L("Installing base system — this may take a while…",
                          "Instalando sistema base — esto puede tardar…"))
            self._pacman_critical(f"pacstrap -K /mnt {pkgs}", 18, 52, "pacstrap")

            self._stage(L("Generating fstab…", "Generando fstab…"))
            self._run_critical("genfstab -U /mnt >> /mnt/etc/fstab", "genfstab")
            self._pct(53)

            self._stage(L("Configuring hostname…", "Configurando hostname…"))
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

            self._stage(L("Configuring locale & timezone…",
                          "Configurando locale y zona horaria…"))
            locale_line = f"{locale} UTF-8"
            self._chroot("sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen")
            if locale != "en_US.UTF-8":
                escaped = locale.replace(".", "\\.").replace("-", "\\-")
                self._chroot(
                    f"sed -i 's/^#{escaped} UTF-8/{locale} UTF-8/' /etc/locale.gen",
                    ignore_error=True
                )
            self._chroot_critical("locale-gen", "locale-gen")
            self._chroot(f"echo 'LANG={locale}' > /etc/locale.conf")
            self._chroot(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
            self._chroot("hwclock --systohc")
            keymap_val = state["keymap"]
            self._chroot(f"echo 'KEYMAP={keymap_val}' > /etc/vconsole.conf")
            self._pct(59)

            self._stage(L("Generating initramfs…", "Generando initramfs…"))
            self._configure_mkinitcpio()
            self._pct(63)

            self._stage(L(f"Creating user '{uname}'…", f"Creando usuario '{uname}'…"))
            self._chroot_critical(
                f"useradd -m -G wheel -s /bin/bash {shlex.quote(uname)}",
                "useradd"
            )
            self._chroot(
                "sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/"
                "%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers"
            )
            self._pct(65)

            self._stage(L("Setting passwords…", "Estableciendo contraseñas…"))
            self._chroot_passwd("root", state["root_pass"])
            self._chroot_passwd(uname, state["user_pass"])
            self._pct(67)

            self._install_shell(uname)
            self._pct(69)

            self._stage(L("Enabling NetworkManager…", "Habilitando NetworkManager…"))
            self._chroot("systemctl enable NetworkManager")
            if is_ssd(disk_path):
                self._chroot("systemctl enable fstrim.timer", ignore_error=True)
                write_log("SSD detected — fstrim.timer enabled.")
            self._pct(71)

            self._install_gpu_drivers(71, 77)
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

            if bootloader == "systemd-boot":
                self._stage(L("Installing systemd-boot…", "Instalando systemd-boot…"))
                self._install_systemd_boot(root_dev)
            else:
                self._stage(L("Installing GRUB bootloader…", "Instalando GRUB…"))
                self._configure_grub_cmdline()
                self._install_grub(disk_path)
            self._pct(96)

            if state.get("snapper") and fs == "btrfs":
                self._stage(L("Setting up snapper (BTRFS snapshots)…",
                              "Configurando snapper (snapshots BTRFS)…"))
                self._pacman(
                    "arch-chroot /mnt pacman -S --noconfirm "
                    "snapper snap-pac grub-btrfs inotify-tools",
                    96, 97, ignore_error=True
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

            if state.get("extras"):
                self._stage(L("Installing extra utilities…", "Instalando utilidades extra…"))
                self._pacman(
                    f"arch-chroot /mnt pacman -S --noconfirm {EXTRA_PKGS}",
                    98, 99, ignore_error=True
                )

            if state.get("flatpak") and desktop != "None":
                self._install_flatpak(uname)

            write_log(f"Copying install log to /mnt/root/arch_install.log")
            run_simple(f"cp {LOG_FILE} /mnt/root/arch_install.log", ignore_error=True)

            self._pct(100)
            self._stage(L("Installation complete!", "¡Instalación completa!"))
            self.on_done(True, "")

        except RuntimeError as e:
            self._log(f"CRITICAL ERROR: {e}")
            self.on_done(False, str(e))
        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))

def screen_welcome():
    boot_mode = "\\Z2UEFI\\Zn" if is_uefi() else "\\Z3BIOS (Legacy)\\Zn"
    cpu_info  = detect_cpu() or L("unknown CPU", "CPU desconocida")
    gpu_info  = detect_gpu()

    text = (
        "\\Zb\\Z4Welcome to the Arch Linux Installer\\Zn\n\n"
        f"  Version  {VERSION}        Boot mode  {boot_mode}\n"
        f"  CPU      {cpu_info}       GPU  {gpu_info}\n\n"
        "\\Zb\\Z1 ⚠ WARNING \\Zn  The selected disk will be \\ZbCOMPLETELY ERASED\\Zn.\n"
        "  Back up any data you want to keep before continuing.\n\n"
        "\\ZbBefore you start, make sure you have:\\Zn\n"
        "  • An active internet connection (cable or WiFi)\n"
        "  • At least 20 GB of free disk space\n"
        "  • A hostname and username in mind\n\n"
        "Use \\ZbTab\\Zn and \\ZbArrows\\Zn to navigate, \\ZbEnter\\Zn to confirm.\n"
        "Press OK to begin."
    )
    dlg_titled(
        L("Welcome — Arch Linux Installer", "Bienvenido — Arch Linux Installer"),
        "--msgbox", text, "18", "64"
    )

def screen_language():
    result = menu(
        "Language / Idioma",
        "Choose the installer language:\nSeleccione el idioma del instalador:",
        [("en", "English"), ("es", "Espanol")]
    )
    if result:
        state["lang"] = result
        return True
    return False

def screen_mode():
    result = menu(
        L("Install Mode", "Modo de instalación"),
        L(
            "\\ZbQuick Install\\Zn sets sensible defaults and skips configuration screens.\n"
            "  → BTRFS + KDE Plasma + linux kernel + pipewire + yay + snapper\n\n"
            "\\ZbCustom Install\\Zn lets you choose filesystem, kernel, desktop, shell,\n"
            "  GPU drivers, bootloader, locale, extras, and more.\n\n"
            "Both modes ask for disk, hostname, username and password.",
            "\\ZbInstalación rápida\\Zn aplica valores por defecto y omite pantallas de config.\n"
            "  → BTRFS + KDE Plasma + kernel linux + pipewire + yay + snapper\n\n"
            "\\ZbInstalación personalizada\\Zn permite elegir filesystem, kernel, escritorio,\n"
            "  shell, drivers GPU, bootloader, locale, extras y más.\n\n"
            "Ambos modos piden disco, hostname, usuario y contraseña."
        ),
        [
            ("quick",  L("Quick Install   — sane defaults, ready fast",
                         "Instalación rápida   — valores por defecto, lista rápido")),
            ("custom", L("Custom Install  — full control over every option",
                         "Instalación personalizada  — control total")),
        ]
    )
    if result is None:
        return None
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
        state["shell"]      = "bash"
        state["extras"]     = False
        state["locale"]     = "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
        return True
    else:
        state["quick"] = False
        return False

def screen_identity():
    while True:
        hn = inputbox(
            L("Hostname", "Nombre del equipo"),
            L(
                "The \\Zbhostname\\Zn identifies your machine on the network.\n\n"
                "Rules: letters, digits, hyphens and underscores only.\n"
                "Max 32 characters. Example:  my-arch-pc\n\n"
                "Enter hostname:",
                "El \\Zbhostname\\Zn identifica tu equipo en la red.\n\n"
                "Reglas: solo letras, dígitos, guiones y guiones bajos.\n"
                "Máximo 32 caracteres. Ejemplo:  mi-arch-pc\n\n"
                "Ingresa el hostname:"
            ),
            state.get("hostname", "")
        )
        if hn is None:
            return False
        if not validate_name(hn):
            msgbox(
                L("Invalid hostname", "Hostname inválido"),
                L(
                    "Only letters, digits, hyphens and underscores are allowed.\n"
                    "Max 32 characters.",
                    "Solo letras, dígitos, guiones y guiones bajos.\n"
                    "Máximo 32 caracteres."
                )
            )
            continue

        un = inputbox(
            L("Username", "Nombre de usuario"),
            L(
                "The \\Zbusername\\Zn is your personal login account.\n\n"
                "Rules: letters, digits, hyphens and underscores only.\n"
                "Max 32 characters. Example:  john\n\n"
                "Enter username:",
                "El \\Zbnombre de usuario\\Zn es tu cuenta de acceso personal.\n\n"
                "Reglas: solo letras, dígitos, guiones y guiones bajos.\n"
                "Máximo 32 caracteres. Ejemplo:  juan\n\n"
                "Ingresa el nombre de usuario:"
            ),
            state.get("username", "")
        )
        if un is None:
            return False
        if not validate_name(un):
            msgbox(
                L("Invalid username", "Usuario inválido"),
                L(
                    "Only letters, digits, hyphens and underscores are allowed.\n"
                    "Max 32 characters.",
                    "Solo letras, dígitos, guiones y guiones bajos.\n"
                    "Máximo 32 caracteres."
                )
            )
            continue

        state["hostname"] = hn
        state["username"] = un
        return True

def screen_passwords():
    while True:
        rp1 = passwordbox(
            L("Root Password", "Contraseña root"),
            L(
                "The \\Zbroot\\Zn account is the system administrator.\n\n"
                "Choose a strong password — you will need it for system maintenance.\n\n"
                "Enter ROOT password:",
                "La cuenta \\Zbroot\\Zn es el administrador del sistema.\n\n"
                "Elige una contraseña fuerte — la necesitarás para el mantenimiento.\n\n"
                "Ingresa la contraseña de ROOT:"
            )
        )
        if rp1 is None:
            return False
        rp2 = passwordbox(
            L("Root Password — Confirm", "Contraseña root — Confirmar"),
            L("Confirm ROOT password:", "Confirma la contraseña de ROOT:")
        )
        if rp2 is None:
            return False
        if not rp1:
            msgbox(L("Error", "Error"), L(
                "Root password cannot be empty.",
                "La contraseña root no puede estar vacía."
            ))
            continue
        if rp1 != rp2:
            msgbox(L("Mismatch", "No coinciden"), L(
                "Root passwords do not match. Try again.",
                "Las contraseñas root no coinciden. Intenta de nuevo."
            ))
            continue

        up1 = passwordbox(
            L("User Password", "Contraseña de usuario"),
            L(
                f"Password for your user account: \\Zb{state.get('username', 'user')}\\Zn\n\n"
                "This is what you will type to log in every day.\n\n"
                "Enter USER password:",
                f"Contraseña de tu cuenta: \\Zb{state.get('username', 'user')}\\Zn\n\n"
                "Esta es la que escribirás para iniciar sesión cada día.\n\n"
                "Ingresa la contraseña de USUARIO:"
            )
        )
        if up1 is None:
            return False
        up2 = passwordbox(
            L("User Password — Confirm", "Contraseña usuario — Confirmar"),
            L("Confirm USER password:", "Confirma la contraseña de USUARIO:")
        )
        if up2 is None:
            return False
        if not up1:
            msgbox(L("Error", "Error"), L(
                "User password cannot be empty.",
                "La contraseña de usuario no puede estar vacía."
            ))
            continue
        if up1 != up2:
            msgbox(L("Mismatch", "No coinciden"), L(
                "User passwords do not match. Try again.",
                "Las contraseñas de usuario no coinciden. Intenta de nuevo."
            ))
            continue

        state["root_pass"] = rp1
        state["user_pass"] = up1
        return True

def screen_disk():
    disks = list_disks()
    if not disks:
        msgbox(
            L("No disks found", "Sin discos"),
            L(
                "No disks were detected.\n\n"
                "Check that your storage device is properly connected.",
                "No se detectaron discos.\n\n"
                "Verifica que tu dispositivo de almacenamiento esté conectado."
            )
        )
        sys.exit(1)

    try:
        lsblk_info = subprocess.check_output(
            "lsblk -o NAME,SIZE,TYPE,FSTYPE,MOUNTPOINT 2>/dev/null | head -40",
            shell=True, text=True
        )
    except Exception:
        lsblk_info = ""

    if lsblk_info:
        msgbox(
            L("Current Disk Layout", "Layout actual de discos"),
            L(
                "\\ZbReview your disks before selecting one to erase.\\Zn\n\n"
                + lsblk_info +
                "\n\\Z1ALL DATA on the selected disk will be permanently erased.\\Zn",
                "\\ZbRevisa tus discos antes de elegir uno para borrar.\\Zn\n\n"
                + lsblk_info +
                "\n\\Z1TODOS LOS DATOS del disco seleccionado se borrarán permanentemente.\\Zn"
            )
        )

    items   = [(f"/dev/{n}", f"{gb} GB  —  {model}") for n, gb, model in disks]
    default = f"/dev/{state['disk']}" if state["disk"] else items[0][0]

    result = radiolist(
        L("Select Installation Disk", "Seleccionar disco de instalación"),
        L(
            "\\Z1WARNING:\\Zn ALL DATA on the selected disk will be \\ZbERASED\\Zn.\n"
            "Make sure you pick the correct disk.\n\n"
            "Select the target disk:" + _nav(),
            "\\Z1ADVERTENCIA:\\Zn Se BORRARÁN TODOS LOS DATOS del disco seleccionado.\n"
            "Asegúrate de elegir el disco correcto.\n\n"
            "Selecciona el disco de instalación:" + _nav()
        ),
        items,
        default=default
    )
    if result is None:
        return False

    disk_size = next((gb for n, gb, m in disks if f"/dev/{n}" == result), "?")
    if not yesno(
        L("Confirm Erase", "Confirmar borrado"),
        L(
            f"You selected:  \\Zb{result}\\Zn  ({disk_size} GB)\n\n"
            "\\Z1ALL DATA on this disk will be permanently destroyed.\\Zn\n\n"
            "Are you absolutely sure you want to continue?",
            f"Seleccionaste:  \\Zb{result}\\Zn  ({disk_size} GB)\n\n"
            "\\Z1TODOS LOS DATOS en este disco se destruirán permanentemente.\\Zn\n\n"
            "¿Estás completamente seguro de continuar?"
        )
    ):
        return False

    state["disk"] = result.replace("/dev/", "")

    suggested = str(suggest_swap_gb())
    while True:
        swap = inputbox(
            L("Swap Size", "Tamaño de Swap"),
            L(
                "\\ZbSwap\\Zn is disk space used as overflow RAM.\n"
                "It also enables hibernation if sized >= your RAM.\n\n"
                f"Suggested for your system: \\Zb{suggested} GB\\Zn\n\n"
                "Enter swap size in GB (1–128).\n"
                "Press Enter to accept suggested, or Cancel to use suggested:",
                "\\ZbSwap\\Zn es espacio en disco que actúa como RAM de desbordamiento.\n"
                "También permite hibernación si es >= tu RAM.\n\n"
                f"Sugerido para tu sistema: \\Zb{suggested} GB\\Zn\n\n"
                "Ingresa el tamaño en GB (1–128).\n"
                "Presiona Enter para aceptar el sugerido, o Cancelar para usar el sugerido:"
            ),
            state.get("swap", suggested)
        )
        if swap is None or not swap.strip():
            swap = suggested
        if validate_swap(swap.strip()):
            state["swap"] = swap.strip()
            return True
        msgbox(L("Invalid swap", "Swap inválido"),
               L("Must be a whole number between 1 and 128.",
                 "Debe ser un número entero entre 1 y 128."))

def screen_filesystem():
    result = radiolist(
        L("Filesystem", "Sistema de archivos"),
        L(
            "Choose the root filesystem for your installation.\n\n"
            "\\Zbext4\\Zn  — Battle-tested, simple, very stable. Best choice if you\n"
            "         are new to Linux or want maximum compatibility.\n\n"
            "\\Zbbtrfs\\Zn — Modern copy-on-write filesystem with subvolumes, transparent\n"
            "         zstd compression and snapshot support. Pairs with snapper\n"
            "         for automatic rollback. Slightly more complex." + _nav(),
            "Elige el sistema de archivos raíz.\n\n"
            "\\Zbext4\\Zn  — Probado, simple, muy estable. La mejor opción si eres\n"
            "         nuevo en Linux o quieres máxima compatibilidad.\n\n"
            "\\Zbbtrfs\\Zn — Filesystem copy-on-write moderno con subvolúmenes, compresión\n"
            "         zstd transparente y soporte de snapshots. Combina con snapper\n"
            "         para rollback automático. Un poco más complejo." + _nav()
        ),
        [
            ("ext4",  L("ext4  — stable, widely supported, recommended for beginners",
                        "ext4  — estable, amplio soporte, recomendado para principiantes")),
            ("btrfs", L("btrfs — subvolumes + zstd compression + snapshot support",
                        "btrfs — subvolúmenes + compresión zstd + soporte de snapshots")),
        ],
        default=state["filesystem"]
    )
    if result is None:
        return False
    state["filesystem"] = result
    return True

def screen_kernel():
    result = radiolist(
        L("Kernel", "Kernel"),
        L(
            "Choose which Linux kernel to install.\n\n"
            "\\Zblinux\\Zn     — Latest stable kernel. Updated frequently.\n"
            "             Good default for most hardware.\n\n"
            "\\Zblinux-lts\\Zn — Long-Term Support kernel. Older but more stable.\n"
            "             Good for servers or finicky hardware.\n\n"
            "\\Zblinux-zen\\Zn — Community-patched for lower latency and better\n"
            "             desktop responsiveness. Great for gaming." + _nav(),
            "Elige qué kernel de Linux instalar.\n\n"
            "\\Zblinux\\Zn     — Kernel estable más reciente. Se actualiza frecuentemente.\n"
            "             Buena opción por defecto para la mayoría de hardware.\n\n"
            "\\Zblinux-lts\\Zn — Kernel de Soporte a Largo Plazo. Más antiguo pero estable.\n"
            "             Ideal para servidores o hardware problemático.\n\n"
            "\\Zblinux-zen\\Zn — Parcheado para menor latencia y mejor respuesta\n"
            "             en escritorio. Genial para gaming." + _nav()
        ),
        [
            ("linux",     L("linux     — latest stable kernel (recommended)",
                            "linux     — kernel estable más reciente (recomendado)")),
            ("linux-lts", L("linux-lts — long-term support kernel",
                            "linux-lts — kernel de soporte a largo plazo")),
            ("linux-zen", L("linux-zen — optimized for desktop and gaming",
                            "linux-zen — optimizado para escritorio y gaming")),
        ],
        default=state["kernel"]
    )
    if result is None:
        return False
    state["kernel"] = result
    return True

def screen_bootloader():
    uefi = is_uefi()
    if not uefi:
        state["bootloader"] = "grub"
        return True

    result = radiolist(
        L("Bootloader", "Gestor de arranque"),
        L(
            "The bootloader starts the operating system.\n\n"
            "\\ZbGRUB\\Zn         — Mature, feature-rich. Works on UEFI and BIOS.\n"
            "               Shows a graphical boot menu. Compatible with\n"
            "               dual-boot setups and grub-btrfs snapshots.\n\n"
            "\\Zbsystemd-boot\\Zn — Minimal, fast. \\ZbUEFI only\\Zn. Less configuration\n"
            "               overhead. Does not support grub-btrfs." + _nav(),
            "El gestor de arranque inicia el sistema operativo.\n\n"
            "\\ZbGRUB\\Zn         — Maduro, con muchas funciones. UEFI y BIOS.\n"
            "               Muestra un menú de arranque gráfico. Compatible\n"
            "               con dual-boot y snapshots grub-btrfs.\n\n"
            "\\Zbsystemd-boot\\Zn — Mínimo, rápido. \\ZbSolo UEFI\\Zn. Menos configuración.\n"
            "               No soporta grub-btrfs." + _nav()
        ),
        [
            ("grub",         L("GRUB         — stable, UEFI and BIOS, dual-boot friendly",
                               "GRUB         — estable, UEFI y BIOS, ideal para dual-boot")),
            ("systemd-boot", L("systemd-boot — fast, UEFI only, minimal overhead",
                               "systemd-boot — rápido, solo UEFI, mínima sobrecarga")),
        ],
        default=state.get("bootloader", "grub")
    )
    if result is None:
        return False
    state["bootloader"] = result
    return True

def screen_mirrors():
    default = "yes" if state["mirrors"] else "no"
    result = radiolist(
        L("Mirror Optimization", "Optimización de mirrors"),
        L(
            "Arch Linux downloads packages from mirror servers worldwide.\n\n"
            "Use arrow keys to move, Space to select, Enter to confirm." + _nav(),
            "Arch Linux descarga paquetes desde servidores mirror de todo el mundo.\n\n"
            "Usa las flechas para moverte, Espacio para seleccionar, Enter para confirmar." + _nav()
        ),
        [
            ("yes", L("Yes — auto-select the 10 fastest mirrors (recommended)",
                      "Sí — seleccionar los 10 mirrors más rápidos (recomendado)")),
            ("no",  L("No  — keep default mirrors",
                      "No  — mantener mirrors por defecto")),
        ],
        default=default
    )
    if result is None:
        return False
    state["mirrors"] = (result == "yes")
    return True

def screen_locale():
    default = state.get("locale") or (
        "es_ES.UTF-8" if state["lang"] == "es" else "en_US.UTF-8"
    )
    result = radiolist(
        L("System Locale", "Locale del sistema"),
        L(
            "The \\Zblocale\\Zn sets the system language, date format, and number format.\n\n"
            "This affects your installed system — not the installer language.\n"
            "English (en_US) is always enabled as a fallback." + _nav(),
            "El \\Zblocale\\Zn define el idioma del sistema, formato de fecha y números.\n\n"
            "Esto afecta al sistema instalado — no al idioma del instalador.\n"
            "Inglés (en_US) siempre se habilita como respaldo." + _nav()
        ),
        LOCALE_OPTIONS,
        default=default
    )
    if result is None:
        return False
    state["locale"] = result
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

    wanted  = ["us", "es", "uk", "fr", "de", "it", "ru", "ara", "pt-latin9", "br-abnt2",
               "dvorak", "colemak", "la-latin1"]
    options = [m for m in wanted if m in maps] if maps else wanted
    items   = [(m, f"Keyboard layout: {m}") for m in options]

    result = radiolist(
        L("Keyboard Layout", "Distribución de teclado"),
        L(
            "Select the \\Zbkeyboard layout\\Zn for the console (TTY).\n\n"
            "This affects key mapping in the virtual terminal.\n"
            "Graphical desktop environments have their own keyboard settings." + _nav(),
            "Selecciona la \\Zbdistribución de teclado\\Zn para la consola (TTY).\n\n"
            "Esto afecta el mapeo de teclas en la terminal virtual.\n"
            "Los entornos gráficos tienen su propia configuración de teclado." + _nav()
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
        L("Timezone — Region", "Zona horaria — Región"),
        L(
            "Select your \\Zbregion\\Zn first, then choose a city.\n\n"
            "The timezone is used to set the hardware clock and display\n"
            "correct local times." + _nav(),
            "Selecciona tu \\Zbregión\\Zn primero, luego elige una ciudad.\n\n"
            "La zona horaria se usa para ajustar el reloj de hardware y\n"
            "mostrar la hora local correcta." + _nav()
        ),
        [(r, r) for r in regions],
        default=cur_region
    )
    if region is None:
        return False
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
        L(
            f"Region: \\Zb{region}\\Zn\n"
            "Select your city or the nearest one:" + _nav(),
            f"Región: \\Zb{region}\\Zn\n"
            "Selecciona tu ciudad o la más cercana:" + _nav()
        ),
        cities,
        default=cur_city
    )
    if city is None:
        return False
    state["timezone"] = f"{region}/{city}"
    return True

def screen_desktop():
    result = radiolist(
        L("Desktop Environment", "Entorno de escritorio"),
        L(
            "Choose a \\Zbdesktop environment\\Zn. Each comes with a display manager,\n"
            "a terminal emulator (alacritty or konsole), Firefox, and audio.\n\n"
            "Select \\ZbNone\\Zn for a headless / server install." + _nav(),
            "Elige un \\Zbentorno de escritorio\\Zn. Cada uno incluye gestor de sesión,\n"
            "un emulador de terminal (alacritty o konsole), Firefox y audio.\n\n"
            "Elige \\ZbNone\\Zn para una instalación headless / servidor." + _nav()
        ),
        [
            ("KDE Plasma", L("KDE Plasma  — full-featured, highly customizable, modern",
                             "KDE Plasma  — completo, muy personalizable, moderno")),
            ("GNOME",      L("GNOME       — clean design, Wayland-first, minimal chrome",
                             "GNOME       — diseño limpio, Wayland primero, minimalista")),
            ("Cinnamon",   L("Cinnamon    — traditional layout, Windows-like, very familiar",
                             "Cinnamon    — diseño tradicional, similar a Windows")),
            ("XFCE",       L("XFCE        — lightweight, fast, traditional, low RAM usage",
                             "XFCE        — ligero, rápido, tradicional, poco uso de RAM")),
            ("MATE",       L("MATE        — GNOME 2 fork, stable, great for older hardware",
                             "MATE        — fork de GNOME 2, estable, bueno para hardware viejo")),
            ("LXQt",       L("LXQt        — minimal Qt desktop, very low resource usage",
                             "LXQt        — escritorio Qt minimalista, muy pocos recursos")),
            ("None",       L("None        — no desktop, CLI only (server / custom builds)",
                             "None        — sin escritorio, solo CLI (servidor / avanzado)")),
        ],
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

    detected_label = f"\\Z3{detected}\\Zn" if detected != "None" else L("none detected", "no detectada")

    result = radiolist(
        L("GPU Drivers", "Drivers GPU"),
        L(
            f"Auto-detected GPU:  {detected_label}\n\n"
            "Choose the driver package to install.\n"
            "For \\ZbNVIDIA\\Zn, the proprietary driver gives best performance.\n"
            "For \\ZbAMD/Intel\\Zn, open-source Mesa drivers are the standard.\n"
            "Hybrid laptops (Intel + NVIDIA/AMD) need the combined option." + _nav(),
            f"GPU detectada automáticamente:  {detected_label}\n\n"
            "Elige el paquete de driver a instalar.\n"
            "Para \\ZbNVIDIA\\Zn, el driver propietario da el mejor rendimiento.\n"
            "Para \\ZbAMD/Intel\\Zn, los drivers open-source Mesa son el estándar.\n"
            "Laptops híbridas (Intel + NVIDIA/AMD) necesitan la opción combinada." + _nav()
        ),
        [
            ("NVIDIA",       L("NVIDIA       — proprietary driver (nvidia / nvidia-dkms)",
                               "NVIDIA       — driver propietario (nvidia / nvidia-dkms)")),
            ("AMD",          L("AMD          — open-source Mesa + vulkan-radeon",
                               "AMD          — Mesa open-source + vulkan-radeon")),
            ("Intel",        L("Intel        — open-source Mesa + vulkan-intel",
                               "Intel        — Mesa open-source + vulkan-intel")),
            ("Intel+NVIDIA", L("Intel+NVIDIA — hybrid laptop (Mesa + proprietary NVIDIA)",
                               "Intel+NVIDIA — laptop híbrida (Mesa + NVIDIA propietario)")),
            ("Intel+AMD",    L("Intel+AMD    — hybrid laptop (Mesa + vulkan-radeon)",
                               "Intel+AMD    — laptop híbrida (Mesa + vulkan-radeon)")),
            ("None",         L("None         — no additional GPU drivers",
                               "None         — sin drivers adicionales de GPU")),
        ],
        default=state["gpu"]
    )
    if result is None:
        return False
    state["gpu"] = result
    return True

def screen_shell():
    result = radiolist(
        L("Default Shell", "Shell por defecto"),
        L(
            "Choose the default \\Zbcommand-line shell\\Zn for your user account.\n\n"
            "\\Zbbash\\Zn — The classic Unix shell. Pre-installed, no extras needed.\n\n"
            "\\Zbzsh\\Zn  — Feature-rich, with better tab completion and plugins.\n"
            "       Installs zsh-autosuggestions and syntax highlighting.\n\n"
            "\\Zbfish\\Zn — Friendly, beginner-oriented. Auto-suggests from history\n"
            "       out of the box. Not POSIX-compatible." + _nav(),
            "Elige la \\Zbshell de línea de comandos\\Zn por defecto para tu usuario.\n\n"
            "\\Zbbash\\Zn — La shell clásica de Unix. Preinstalada, sin extras.\n\n"
            "\\Zbzsh\\Zn  — Rica en funciones, mejor autocompletado y plugins.\n"
            "       Instala zsh-autosuggestions y resaltado de sintaxis.\n\n"
            "\\Zbfish\\Zn — Amigable, orientada a principiantes. Sugiere del historial\n"
            "       automáticamente. No compatible con POSIX." + _nav()
        ),
        [
            ("bash", L("bash — classic, always available, POSIX-compatible",
                       "bash — clásica, siempre disponible, compatible con POSIX")),
            ("zsh",  L("zsh  — powerful, great completion, popular with advanced users",
                       "zsh  — potente, gran autocompletado, popular entre usuarios avanzados")),
            ("fish", L("fish — friendly, beginner-friendly, beautiful prompts",
                       "fish — amigable, ideal para principiantes, prompts bonitos")),
        ],
        default=state.get("shell", "bash")
    )
    if result is None:
        return False
    state["shell"] = result
    return True

def screen_yay():
    result = radiolist(
        L("AUR Helper — yay", "AUR Helper — yay"),
        L(
            "The \\ZbAUR\\Zn (Arch User Repository) contains thousands of community\n"
            "packages not in the official repos (e.g. Discord, Spotify, VS Code).\n\n"
            "\\Zbyay\\Zn is a popular AUR helper that wraps pacman and lets you install\n"
            "AUR packages with the same syntax as pacman -S.\n\n"
            "Requires an internet connection and builds packages from source." + _nav(),
            "El \\ZbAUR\\Zn (Arch User Repository) tiene miles de paquetes comunitarios\n"
            "no disponibles en los repos oficiales (Discord, Spotify, VS Code, etc.).\n\n"
            "\\Zbyay\\Zn es un AUR helper popular que envuelve pacman y permite instalar\n"
            "paquetes del AUR con la misma sintaxis que pacman -S.\n\n"
            "Requiere internet y compila los paquetes desde el código fuente." + _nav()
        ),
        [
            ("yes", L("Yes — install yay after base setup",
                      "Sí — instalar yay al finalizar la instalación base")),
            ("no",  L("No  — skip (you can install it manually later)",
                      "No  — omitir (puedes instalarlo manualmente después)")),
        ],
        default="yes" if state["yay"] else "no"
    )
    if result is None:
        return False
    state["yay"] = (result == "yes")
    return True

def screen_extras():
    result = radiolist(
        L("Extra Utilities", "Utilidades extra"),
        L(
            "Install a curated set of \\Zbcommon CLI utilities\\Zn:\n\n"
            "  htop btop wget curl rsync bat fd ripgrep\n"
            "  p7zip unzip zip tree man-db neofetch\n"
            "  lsof strace iotop usbutils\n\n"
            "These are useful tools you would likely install anyway.\n"
            "Adds a few minutes to installation time." + _nav(),
            "Instalar un conjunto de \\Zbherramientas CLI comunes\\Zn:\n\n"
            "  htop btop wget curl rsync bat fd ripgrep\n"
            "  p7zip unzip zip tree man-db neofetch\n"
            "  lsof strace iotop usbutils\n\n"
            "Son herramientas útiles que instalarías de todas formas.\n"
            "Añade pocos minutos al tiempo de instalación." + _nav()
        ),
        [
            ("yes", L("Yes — install common CLI utilities",
                      "Sí — instalar utilidades CLI comunes")),
            ("no",  L("No  — minimal install, add later as needed",
                      "No  — instalación mínima, añadir después si es necesario")),
        ],
        default="yes" if state.get("extras") else "no"
    )
    if result is None:
        return False
    state["extras"] = (result == "yes")
    return True

def screen_snapper():
    if state["filesystem"] != "btrfs":
        state["snapper"] = False
        return True
    result = radiolist(
        L("BTRFS Snapshots — snapper", "Snapshots BTRFS — snapper"),
        L(
            "\\Zbsnapper\\Zn automatically creates BTRFS snapshots before and after\n"
            "every pacman transaction (installs, updates, removals).\n\n"
            "If an update breaks something, you can roll back by booting a\n"
            "previous snapshot from the GRUB menu (requires grub-btrfs).\n\n"
            "Only available with BTRFS filesystem — already selected." + _nav(),
            "\\Zbsnapper\\Zn crea snapshots BTRFS automáticamente antes y después\n"
            "de cada transacción de pacman (instalaciones, actualizaciones, borrados).\n\n"
            "Si una actualización rompe algo, puedes hacer rollback arrancando\n"
            "un snapshot anterior desde el menú GRUB (requiere grub-btrfs).\n\n"
            "Solo disponible con BTRFS — ya seleccionado." + _nav()
        ),
        [
            ("yes", L("Yes — automatic snapshots on every pacman transaction",
                      "Sí — snapshots automáticos en cada transacción pacman")),
            ("no",  L("No  — skip (you can set up snapper manually later)",
                      "No  — omitir (puedes configurar snapper manualmente después)")),
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
        L("Flatpak + Flathub", "Flatpak + Flathub"),
        L(
            "\\ZbFlatpak\\Zn is a universal package format that runs apps in sandboxes,\n"
            "independent of your Arch packages.\n\n"
            "\\ZbFlathub\\Zn is the main Flatpak repository with thousands of apps\n"
            "including GIMP, LibreOffice, Steam, OBS Studio, and more.\n\n"
            "Flatpak is added for your user and integrates with your desktop." + _nav(),
            "\\ZbFlatpak\\Zn es un formato de paquete universal que ejecuta apps en\n"
            "entornos aislados, independiente de los paquetes de Arch.\n\n"
            "\\ZbFlathub\\Zn es el repositorio principal de Flatpak con miles de apps\n"
            "incluyendo GIMP, LibreOffice, Steam, OBS Studio y más.\n\n"
            "Flatpak se configura para tu usuario y se integra con el escritorio." + _nav()
        ),
        [
            ("yes", L("Yes — install Flatpak and add Flathub",
                      "Sí — instalar Flatpak y añadir Flathub")),
            ("no",  L("No  — skip",
                      "No  — omitir")),
        ],
        default="yes" if state.get("flatpak") else "no"
    )
    if result is None:
        return False
    state["flatpak"] = (result == "yes")
    return True

def screen_review():
    microcode = detect_cpu() or L("none detected", "no detectado")
    quick_tag = L("  [Quick Install]", "  [Instalación rápida]") if state["quick"] else ""
    boot_mode = L("UEFI", "UEFI") if is_uefi() else L("BIOS", "BIOS")
    locale_val = state.get("locale") or L("(derived from language)", "(derivado del idioma)")
    shell_val  = state.get("shell", "bash")
    extras_val = L("yes", "sí") if state.get("extras") else "no"

    lines = [
        (L("Mode",       "Modo"),         L("Quick", "Rápida") if state["quick"] else L("Custom", "Personalizada")),
        (L("Boot mode",  "Arranque"),     boot_mode),
        (L("Language",   "Idioma"),       state["lang"]),
        (L("Locale",     "Locale"),       locale_val),
        ("Hostname",                      state["hostname"] or "\\Z1NOT SET\\Zn"),
        (L("Username",   "Usuario"),      state["username"] or "\\Z1NOT SET\\Zn"),
        ("Filesystem",                    state["filesystem"]),
        ("Kernel",                        state["kernel"]),
        ("Bootloader",                    state["bootloader"]),
        ("Microcode",                     microcode),
        ("Disk",                          f"/dev/{state['disk']}" if state["disk"] else "\\Z1NOT SET\\Zn"),
        ("Swap",                          f"{state['swap']} GB"),
        ("Mirrors",                       L("reflector (auto)", "reflector (auto)") if state["mirrors"] else L("default", "por defecto")),
        ("Keymap",                        state["keymap"]),
        ("Timezone",                      state["timezone"]),
        ("Shell",                         shell_val),
        ("Desktop",                       state["desktop"]),
        ("GPU",                           state["gpu"]),
        ("Audio",                         "pipewire" if state["desktop"] != "None" else L("none", "ninguno")),
        ("Extras",                        extras_val),
        ("Flatpak",                       L("yes", "sí") if state.get("flatpak") else "no"),
        ("yay",                           L("yes", "sí") if state["yay"] else "no"),
        ("snapper",                       L("yes", "sí") if state.get("snapper") else "no"),
    ]

    text = L(
        f"\\ZbReview your settings:\\Zn{quick_tag}\n\n",
        f"\\ZbRevisa tu configuración:\\Zn{quick_tag}\n\n"
    )
    missing = []

    for label, val in lines:
        text += f"  {label:<14} {val}\n"
    text += "\n"

    if not state["hostname"]:  missing.append("hostname")
    if not state["username"]:  missing.append(L("username", "usuario"))
    if not state["disk"]:      missing.append(L("disk", "disco"))
    if not state["root_pass"]: missing.append(L("root password", "contraseña root"))
    if not state["user_pass"]: missing.append(L("user password", "contraseña de usuario"))

    if missing:
        text += L(
            f"\\Z1MISSING: {', '.join(missing)}\\Zn\n\nGo back and fix before continuing.",
            f"\\Z1FALTA: {', '.join(missing)}\\Zn\n\nVuelve atrás y corrígelo antes de continuar."
        )
        msgbox(L("Review — Incomplete", "Revisión — Incompleto"), text)
        return False

    text += L("\\Z2All settings look good.\\Zn", "\\Z2Todo listo.\\Zn")
    return yesno(
        L("Review & Confirm", "Revisar y confirmar"),
        text + L(
            f"\n\n\\Z1WARNING: /dev/{state['disk']} will be erased.\\Zn\n\nProceed with installation?",
            f"\n\n\\Z1ADVERTENCIA: /dev/{state['disk']} será borrado.\\Zn\n\n¿Proceder con la instalación?"
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
        self._stage        = L("Preparing…", "Preparando…")
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
        return s[:n - 1] + "…"

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
        top = max(1, (rows - 10) // 2)
        pad = " " * lft

        def row(r, txt=""):
            out.append(self._goto(r) + self._clr() + pad + txt)

        for r in range(1, top):
            out.append(self._goto(r) + self._clr())
        for r in range(top + 11, rows + 1):
            out.append(self._goto(r) + self._clr())

        title_str = f"  {self._title}  —  {self._version}  "
        row(top,
            self._BG_BLUE + self._BOLD + title_str.center(W) + self._RST)
        row(top + 1)

        row(top + 2,
            f"  {self._BOLD}{self._trunc(stage, W - 4)}{self._RST}")
        row(top + 3)

        bar_w  = W - 10
        filled = int(bar_w * pct / 100)
        empty  = bar_w - filled
        bar = (self._BG_BLUE + " " * filled + self._RST +
               self._BG_DARK + " " * empty  + self._RST)
        pct_s = f"{int(pct):3d}%"
        row(top + 4, f"  [{bar}] {self._BOLD}{pct_s}{self._RST}")
        row(top + 5)

        milestones = [
            (5,  L("Network & mirrors", "Red y mirrors")),
            (18, L("Disk setup", "Preparar disco")),
            (52, L("Base system", "Sistema base")),
            (71, L("Locale & users", "Locale y usuarios")),
            (91, L("Desktop & audio", "Escritorio y audio")),
            (96, L("Bootloader", "Bootloader")),
            (100,L("Extras & finish", "Extras y fin")),
        ]
        done  = self._FG_GRN + "●" + self._RST
        wait  = self._FG_GRAY + "○" + self._RST
        cur   = self._FG_YEL + "◉" + self._RST

        milestone_line = ""
        for mp, ml in milestones:
            if pct >= mp:
                sym = done
            elif pct >= mp - 10:
                sym = cur
            else:
                sym = wait
            milestone_line += f" {sym}{self._FG_GRAY}{self._trunc(ml, 12)}{self._RST}"

        row(top + 6, milestone_line[:W + 20])
        row(top + 7)
        row(top + 8,
            f"  {self._FG_GRAY}Type \\Zb'debug'\\Zn to see live log  |  '{self._trunc(LOG_FILE,30)}'  {self._RST}")
        row(top + 9)
        row(top + 10)

    def _draw_debug(self, out, pct, stage, lines, rows, cols):
        W = cols

        pct_s    = f"{pct:.0f}%"
        hdr_l    = f" DEBUG  {pct_s}  {self._trunc(stage, W - 30)}"
        hdr_r    = " type 'exit' to go back "
        gap      = max(0, W - len(hdr_l) - len(hdr_r))
        hdr      = (self._BG_BLUE + self._BOLD +
                    hdr_l + " " * gap + hdr_r + self._RST)
        out.append(self._goto(1) + self._clr() + hdr)
        out.append(self._goto(2) + self._clr() +
                   self._FG_CYAN + "─" * W + self._RST)

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
    _stage     = [L("Preparing…", "Preparando…")]

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
        failed[0]   = not success
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
            L("Installation Failed", "Instalación fallida"),
            L(
                "\\Z1Installation failed.\\Zn\n\n"
                f"{fail_msg[0]}\n\n"
                f"The full log is saved at:\n  {LOG_FILE}\n"
                "  /root/arch_install.log  (on the new system, if partially complete)\n\n"
                "Common causes:\n"
                "  • Network dropped during pacstrap\n"
                "  • Disk write error\n"
                "  • Package signature failure (try refreshing keyring)",
                "\\Z1La instalación falló.\\Zn\n\n"
                f"{fail_msg[0]}\n\n"
                f"El log completo está en:\n  {LOG_FILE}\n"
                "  /root/arch_install.log  (en el nuevo sistema, si está parcialmente completo)\n\n"
                "Causas comunes:\n"
                "  • La red se cayó durante pacstrap\n"
                "  • Error de escritura en disco\n"
                "  • Fallo de firma de paquete (prueba refrescar el keyring)"
            )
        )
        return False
    return True

def screen_finish():
    uname = state.get("username", "user")
    disk  = f"/dev/{state['disk']}" if state["disk"] else "?"
    de    = state["desktop"]
    shell = state.get("shell", "bash")

    ok = yesno(
        L("Installation Complete!", "¡Instalación completa!"),
        L(
            "\\Z2✔  Arch Linux installed successfully!\\Zn\n\n"
            "─── Your system ─────────────────────────\n"
            f"  User      {uname}  (in wheel group — sudo enabled)\n"
            f"  Shell     {shell}\n"
            f"  Desktop   {de}\n"
            f"  Disk      {disk}\n"
            "─────────────────────────────────────────\n\n"
            "\\ZbNext steps after rebooting:\\Zn\n"
            "  • Log in as your user, then run  sudo pacman -Syu\n"
            "  • The install log is at  /root/arch_install.log\n\n"
            "Remove the installation media, then press Yes to reboot.",
            "\\Z2✔  ¡Arch Linux instalado correctamente!\\Zn\n\n"
            "─── Tu sistema ──────────────────────────\n"
            f"  Usuario   {uname}  (en grupo wheel — sudo habilitado)\n"
            f"  Shell     {shell}\n"
            f"  Escritorio {de}\n"
            f"  Disco     {disk}\n"
            "─────────────────────────────────────────\n\n"
            "\\ZbPróximos pasos al reiniciar:\\Zn\n"
            "  • Inicia sesión con tu usuario, luego ejecuta  sudo pacman -Syu\n"
            "  • El log de instalación está en  /root/arch_install.log\n\n"
            "Extrae el medio de instalación y presiona Sí para reiniciar."
        )
    )
    if ok:
        dlg_titled(
            L("Rebooting…", "Reiniciando…"),
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
    if not screen_language():
        sys.exit(0)
    screen_network()
    quick = screen_mode()
    if quick is None:
        sys.exit(0)

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
            (L("Disk",        "Disco"),            screen_disk,        True),
            (L("Filesystem",  "Sistema archivos"), screen_filesystem,  True),
            (L("Kernel",      "Kernel"),           screen_kernel,      True),
            (L("Bootloader",  "Bootloader"),       screen_bootloader,  True),
            (L("Mirrors",     "Mirrors"),          screen_mirrors,     True),
            (L("Identity",    "Identidad"),        screen_identity,    True),
            (L("Passwords",   "Contraseñas"),      screen_passwords,   True),
            (L("Shell",       "Shell"),            screen_shell,       True),
            (L("Keymap",      "Teclado"),          screen_keymap,      True),
            (L("Locale",      "Locale"),           screen_locale,      True),
            (L("Timezone",    "Zona horaria"),     screen_timezone,    True),
            (L("Desktop",     "Escritorio"),       screen_desktop,     True),
            ("GPU",                                screen_gpu,         True),
            (L("yay",         "yay"),              screen_yay,         True),
            (L("Extras",      "Extras"),           screen_extras,      True),
            (L("Flatpak",     "Flatpak"),          screen_flatpak,     True),
            (L("Snapshots",   "Snapshots"),        screen_snapper,     True),
            (L("Review",      "Revisión"),         screen_review,      True),
            (L("Install",     "Instalar"),         screen_install,     False),
            (L("Finish",      "Finalizar"),        screen_finish,      False),
        ]

    idx = 0
    while idx < len(steps):
        name, fn, can_go_back = steps[idx]
        result = fn()
        # Si la instalación falla, salir inmediatamente
        if fn is screen_install and not result:
            sys.exit(1)
        if result is False and can_go_back:
            if idx == 0:
                if yesno(L("Exit", "Salir"),
                         L("Exit the installer?", "¿Salir del instalador?")):
                    sys.exit(0)
            else:
                idx -= 1
        else:
            idx += 1

def bootstrap():
    if os.geteuid() != 0:
        print("This installer must be run as root.")
        print("Example: sudo python arch_installer.py")
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
        print("[+] dialog installed.\n")

    main()

if __name__ == "__main__":
    bootstrap()
