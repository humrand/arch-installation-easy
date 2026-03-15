import subprocess
import sys
import os
import re
import shutil
import shlex
import threading
import time
from datetime import datetime

VERSION  = "V1.1.0-dialog"
LOG_FILE = "/mnt/install_log.txt"
TITLE    = "Arch Linux Installer"

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


def dlg(*args):
    cmd = [
        "dialog",
        "--colors",
        "--backtitle", f"\\Zb\\Z4{TITLE}\\Zn  —  {VERSION}",
        "--title", "",
    ]
    cmd.extend(args)
    result = subprocess.run(cmd, stderr=subprocess.PIPE, text=True)
    return result.returncode, result.stderr.strip()


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
            "dialog",
            "--colors",
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

    def run(self):
        disk_path  = f"/dev/{state['disk']}"
        p1, p2, p3 = partition_paths_for(disk_path)

        try:
            self._stage(L("Checking network…", "Verificando red…"))
            if not ensure_network():
                self.on_done(False, L("No network connection. Connect and retry.",
                                      "Sin conexión de red. Conéctese e intente de nuevo."))
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
            self._stage(L("Installation complete!", "Instalacion completa!"))
            self.on_done(True, "")

        except Exception as e:
            self._log(f"FATAL: {e}")
            self.on_done(False, str(e))


def screen_welcome():
    text = (
        "\\Zb\\Z4Welcome to the Arch Linux Installer\\Zn\n\n"
        f"Version: {VERSION}\n\n"
        "\\Zb\\Z1WARNING:\\Zn  This installer will ERASE the selected disk.\n\n"
        "Use \\ZbTab\\Zn and \\ZbArrow keys\\Zn to navigate.\n"
        "\\ZbMouse clicks\\Zn work on buttons and menu items.\n\n"
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


def screen_identity():
    while True:
        hn = inputbox(
            L("System Identity", "Identidad del sistema"),
            L("Enter hostname (letters, digits, -, _ — max 32 chars):",
              "Ingresa el nombre del equipo (letras, digitos, -, _ — max 32):"),
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
            L("Enter username (letters, digits, -, _ — max 32 chars):",
              "Ingresa el nombre de usuario (letras, digitos, -, _ — max 32):"),
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

    items = [(f"/dev/{n}", f"{gb} GB  —  {model}") for n, gb, model in disks]
    default = f"/dev/{state['disk']}" if state["disk"] else items[0][0]

    result = radiolist(
        L("Disk Selection", "Seleccion de disco"),
        L("WARNING: ALL DATA on the selected disk will be ERASED!\n\nSelect the installation disk:",
          "ADVERTENCIA: Se borraran todos los datos del disco seleccionado!\n\nSelecciona el disco:"),
        items,
        default=default
    )
    if result is None:
        return False
    state["disk"] = result.replace("/dev/", "")

    while True:
        swap = inputbox(
            L("Swap Size", "Tamano de Swap"),
            L("Enter swap size in GB (1-128):", "Ingresa el tamano del swap en GB (1-128):"),
            state["swap"]
        )
        if swap is None:
            return False
        if validate_swap(swap.strip()):
            state["swap"] = swap.strip()
            return True
        msgbox(L("Invalid swap", "Swap invalido"),
               L("Swap must be a number between 1 and 128.",
                 "El swap debe ser un numero entre 1 y 128."))


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
        L("Keyboard Layout", "Distribucion de teclado"),
        L("Select your keyboard layout:", "Selecciona la distribucion de tu teclado:"),
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

    regions = sorted(set(z.split("/")[0] for z in zones if "/" in z))
    regions = ["UTC"] + regions
    region_items = [(r, r) for r in regions]

    cur_region = state["timezone"].split("/")[0] if "/" in state["timezone"] else "UTC"
    region = radiolist(
        L("Timezone — Region", "Zona horaria — Region"),
        L("Select your region:", "Selecciona tu region:"),
        region_items,
        default=cur_region
    )
    if region is None:
        return True

    if region == "UTC":
        state["timezone"] = "UTC"
        return True

    cities = []
    for z in zones:
        if z.startswith(region + "/"):
            city = z.split("/", 1)[1]
            cities.append((city, z))

    if not cities:
        state["timezone"] = region
        return True

    city_items = [(c, z) for c, z in cities]
    cur_city   = state["timezone"].split("/", 1)[1] if "/" in state["timezone"] else ""
    city = radiolist(
        L("Timezone — City", "Zona horaria — Ciudad"),
        L(f"Region: {region}\nSelect your city:",
          f"Region: {region}\nSelecciona tu ciudad:"),
        city_items,
        default=cur_city
    )
    if city:
        state["timezone"] = f"{region}/{city}"
    return True


def screen_desktop():
    options = [
        ("KDE Plasma", L("Full KDE desktop",
                         "KDE completo")),
        ("Cinnamon",   L("Classic Cinnamon desktop",
                         "Escritorio Cinnamon clasico")),
        ("None",       L("No desktop — command line only",
                         "Sin escritorio — solo linea de comandos")),
    ]
    result = radiolist(
        L("Desktop Environment", "Entorno de escritorio"),
        L("Choose a desktop environment to install:",
          "Elige un entorno de escritorio a instalar:"),
        options,
        default=state["desktop"]
    )
    if result:
        state["desktop"] = result
    return True


def screen_gpu():
    options = [
        ("NVIDIA",    L("NVIDIA proprietary drivers (nvidia + nvidia-utils)",
                        "Drivers propietarios NVIDIA (nvidia + nvidia-utils)")),
        ("AMD/Intel", L("Open-source Mesa drivers (mesa + vulkan-radeon)",
                        "Drivers open-source Mesa (mesa + vulkan-radeon)")),
        ("None",      L("No additional GPU drivers",
                        "Sin drivers adicionales de GPU")),
    ]
    result = radiolist(
        L("GPU Drivers", "Drivers GPU"),
        L("Select your GPU driver:", "Selecciona el driver de tu GPU:"),
        options,
        default=state["gpu"]
    )
    if result:
        state["gpu"] = result
    return True


def screen_review():
    DEFAULTS = {"swap": "8", "desktop": "None", "gpu": "None",
                "keymap": "us", "timezone": "UTC"}

    lines = [
        ("Language",                state["lang"]),
        ("Hostname",                state["hostname"] or "NOT SET"),
        (L("Username", "Usuario"),  state["username"] or "NOT SET"),
        ("Disk",                    f"/dev/{state['disk']}" if state["disk"] else "NOT SET"),
        ("Swap",                    f"{state['swap']} GB"),
        ("Keymap",                  state["keymap"]),
        ("Timezone",                state["timezone"]),
        ("Desktop",                 state["desktop"]),
        ("GPU",                     state["gpu"]),
    ]

    text = L("Review your settings before installing:\n\n",
             "Revisa tu configuracion antes de instalar:\n\n")

    missing = []
    for label, val in lines:
        text += f"  {label:<14} {val}\n"
    text += "\n"

    if not state["hostname"]:  missing.append("hostname")
    if not state["username"]:  missing.append("username")
    if not state["disk"]:      missing.append("disk")
    if not state["root_pass"]: missing.append(L("root password", "contrasena root"))

    if missing:
        text += L(f"MISSING: {', '.join(missing)}\n\nGo back to fix before continuing.",
                  f"FALTA: {', '.join(missing)}\n\nVuelve atras para corregirlo.")
        msgbox(L("Review — Incomplete", "Revision — Incompleto"), text)
        return False

    text += L("All settings look good.", "Todo listo.")

    ok = yesno(
        L("Review & Confirm", "Revisar y confirmar"),
        text + L(
            f"\n\nWARNING: THIS WILL ERASE /dev/{state['disk']}.\n\nProceed with installation?",
            f"\n\nADVERTENCIA: SE BORRARA /dev/{state['disk']}.\n\nProceder con la instalacion?"
        )
    )
    return ok


def screen_install():
    gauge = gauge_open(
        L("Installing Arch Linux", "Instalando Arch Linux"),
        L("Preparing…", "Preparando…"),
        pct=0
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

    backend = InstallBackend(on_progress, on_stage, on_done)
    t = threading.Thread(target=backend.run, daemon=True)
    t.start()

    done_event.wait()

    try:
        gauge.stdin.close()
    except Exception:
        pass
    gauge.wait()

    if failed[0]:
        msgbox(
            L("Installation Failed", "Instalacion fallida"),
            L(f"Installation failed.\n\n{fail_reason[0]}\n\nCheck {LOG_FILE} for full details.",
              f"La instalacion fallo.\n\n{fail_reason[0]}\n\nRevisa {LOG_FILE} para mas detalles.")
        )
        return False

    return True


def screen_finish():
    ok = yesno(
        L("Installation Complete!", "Instalacion completa!"),
        L("Arch Linux has been installed successfully.\n\n"
          "Remove the installation media. reboot now?",
          "Arch Linux se ha instalado correctamente.\n\n"
          "Extrae el medio de instalacion. Reiniciar ahora?")
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
    steps = [
        ("Welcome",                        screen_welcome,   False),
        ("Language",                        screen_language,  False),
        (L("Identity", "Identidad"),        screen_identity,  True),
        (L("Passwords", "Contrasenas"),     screen_passwords, True),
        (L("Disk", "Disco"),                screen_disk,      True),
        (L("Keymap", "Teclado"),            screen_keymap,    True),
        (L("Timezone", "Zona horaria"),     screen_timezone,  True),
        (L("Desktop", "Escritorio"),        screen_desktop,   True),
        ("GPU",                             screen_gpu,       True),
        (L("Review", "Revision"),           screen_review,    True),
        (L("Install", "Instalar"),          screen_install,   False),
        (L("Finish", "Finalizar"),          screen_finish,    False),
    ]

    idx = 0
    while idx < len(steps):
        name, fn, can_go_back = steps[idx]
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
