#!/usr/bin/env python3
import curses
import subprocess
import sys
import os
import re
from datetime import datetime
import termios
import tty
import threading
import time

LOG_FILE = "/mnt/install_log.txt"

def nowtag():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except Exception:
        pass

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def append_buffer_add(s):
    line = f"[{nowtag()}] {s}"
    write_log(line)
    with append_lock:
        append_buffer.append(line)
        if len(append_buffer) > 1000:
            del append_buffer[:500]

def run_stream(cmd, on_line=None, cwd=None):
    append_buffer_add(f"Running: {cmd}")
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=cwd, executable="/bin/bash")
    while True:
        line = p.stdout.readline()
        if line == "" and p.poll() is not None:
            break
        if line:
            l = line.rstrip("\n")
            append_buffer_add(l)
            if on_line:
                on_line(l)
    return p.wait()

def run_simple(cmd):
    append_buffer_add(f"Running: {cmd}")
    r = subprocess.run(cmd, shell=True, executable="/bin/bash")
    return r.returncode

def check_network():
    ret = subprocess.run("ping -c1 -W1 8.8.8.8 >/dev/null 2>&1", shell=True)
    if ret.returncode == 0:
        return True
    if shutil_available():
        subprocess.run("dhcpcd --nobackground >/dev/null 2>&1 &", shell=True)
        time.sleep(2)
        ret2 = subprocess.run("ping -c1 -W1 8.8.8.8 >/dev/null 2>&1", shell=True)
        return ret2.returncode == 0
    return False

def shutil_available():
    return shutil_path() is not None

def shutil_path():
    try:
        out = subprocess.check_output("command -v dhcpcd || true", shell=True, text=True).strip()
        return out if out else None
    except Exception:
        return None

def list_disks():
    try:
        out = subprocess.check_output("lsblk -b -d -o NAME,SIZE | tail -n +2", shell=True, text=True)
        disks = []
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 2:
                continue
            name = parts[0]
            try:
                size = int(parts[1])
                gb = size // (1024 ** 3)
            except Exception:
                gb = 0
            disks.append((name, gb))
        return disks
    except Exception:
        return []

def partition_suffix(disk, n):
    if "nvme" in disk or "mmcblk" in disk:
        return f"/dev/{disk}p{n}"
    return f"/dev/{disk}{n}"

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

def paginate_select(options, title):
    per = 10
    idx = 0
    while True:
        os.system("clear")
        print(title)
        page = options[idx:idx+per]
        for i, opt in enumerate(page, start=1):
            print(f"{i}. {opt}")
        print()
        print("n: next page, p: prev page, q: cancel")
        choice = input("> ").strip()
        if choice.isdigit():
            n = int(choice)
            if 1 <= n <= len(page):
                return idx + n - 1
        elif choice == "n":
            if idx + per < len(options):
                idx += per
        elif choice == "p":
            if idx - per >= 0:
                idx -= per
        elif choice == "q":
            return None

def get_input(prompt, default=""):
    sys.stdout.write(prompt + (" [" + str(default) + "]" if default else "") + " ")
    sys.stdout.flush()
    s = input().strip()
    return s if s else default

def validate_name(n):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", n))

def validate_swap(s):
    return bool(re.match(r"^\d+$", s)) and int(s) > 0

def choose_keymap():
    try:
        out = subprocess.check_output("localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
        if out:
            maps = [l for l in out.splitlines() if l]
            common = ["us","es","fr","de","it","pt","la-latin1"]
            options = [m for m in common if m in maps]
            if not options:
                options = maps[:40]
            idx = paginate_select(options, L("Select keymap","Seleccione keymap"))
            if idx is None:
                return "us"
            return options[idx]
    except Exception:
        pass
    return "us"

def choose_timezone():
    try:
        out = subprocess.check_output("timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
        if out:
            zones = out.splitlines()
            idx = paginate_select(zones, L("Select timezone","Seleccione zona horaria"))
            if idx is None:
                return "UTC"
            return zones[idx]
    except Exception:
        pass
    return "UTC"

def ensure_keyring():
    run_stream("pacman -Sy --noconfirm archlinux-keyring")
    run_simple("pacman-key --init >/dev/null 2>&1 || true")
    run_simple("pacman-key --populate archlinux >/dev/null 2>&1 || true")

def safe_mkdir(path):
    try:
        os.makedirs(path, exist_ok=True)
    except Exception:
        pass

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

screens = ["language","identity","passwords","disk","desktop","gpu","review","install"]

append_buffer = []
append_lock = threading.Lock()

def screen_language_c(stdscr):
    curses.curs_set(0)
    opts = ["English","Español"]
    sel = 0 if state["lang"] == "en" else 1
    while True:
        stdscr.erase()
        stdscr.addstr(1,2,"Language / Idioma")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(8,4,"Use up/down Enter to select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["lang"] = "en" if sel==0 else "es"
            return
        elif k == ord("q"):
            sys.exit(0)

def input_curses(stdscr,y,x,prompt,initial="",secret=False):
    curses.echo() if not secret else curses.noecho()
    stdscr.addstr(y,x,prompt)
    stdscr.refresh()
    win = curses.newwin(1,60,y,x+len(prompt)+1)
    try:
        win.addstr(0,0,initial)
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
            if ch in ("\n","\r"):
                break
            if ch == "\x7f":
                if len(s)>0:
                    s = s[:-1]
                    yx = win.getyx()
                    if yx[1]>0:
                        win.delch(0,yx[1]-1)
            else:
                s += ch
                win.addstr("*")
    curses.echo()
    return s

def screen_identity_c(stdscr):
    curses.curs_set(1)
    stdscr.erase()
    stdscr.addstr(1,2,"System Identity")
    state["hostname"] = input_curses(stdscr,4,4,"Hostname:",state.get("hostname",""),secret=False)
    state["username"] = input_curses(stdscr,6,4,"Username:",state.get("username",""),secret=False)
    if not validate_name(state["hostname"]):
        state["hostname"] = ""
    if not validate_name(state["username"]):
        state["username"] = ""

def screen_passwords_c(stdscr):
    curses.curs_set(1)
    stdscr.erase()
    stdscr.addstr(1,2,"Passwords")
    p1 = input_curses(stdscr,4,4,"Root password:",secret=True)
    p2 = input_curses(stdscr,6,4,"User password:",secret=True)
    state["root_pass"] = p1
    state["user_pass"] = p2

def screen_disk_c(stdscr):
    curses.curs_set(0)
    disks = list_disks()
    if not disks:
        stdscr.addstr(2,2,L("No disks found. Aborting.","No se encontraron discos. Abortando."))
        stdscr.getch()
        sys.exit(1)
    opts = [f"/dev/{d} ({s} GB)" for d,s in disks]
    sel = 0
    while True:
        stdscr.erase()
        stdscr.addstr(1,2,"Disk & Swap")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(3+len(opts)+1,4,f"Swap size in GB: {state['swap']}")
        stdscr.addstr(3+len(opts)+3,4,"Up/Down Enter select, s change swap")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["disk"] = disks[sel][0]
            return
        elif k == ord("s"):
            curses.echo()
            stdscr.addstr(3+len(opts)+1,4,"Swap size in GB: ")
            try:
                val = stdscr.getstr(3+len(opts)+1,24,6).decode().strip()
            except Exception:
                val = ""
            curses.noecho()
            if validate_swap(val):
                state["swap"] = val
        elif k == ord("q"):
            sys.exit(0)

def screen_desktop_c(stdscr):
    curses.curs_set(0)
    opts = ["KDE Plasma","Cinnamon","None"]
    sel = opts.index(state["desktop"]) if state["desktop"] in opts else 2
    while True:
        stdscr.erase()
        stdscr.addstr(1,2,"Desktop Environment")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(7,4,"Up/Down Enter select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["desktop"] = opts[sel]
            return
        elif k == ord("q"):
            sys.exit(0)

def screen_gpu_c(stdscr):
    curses.curs_set(0)
    opts = ["NVIDIA","AMD / Intel","None"]
    sel = 0 if state["gpu"] == "NVIDIA" else (1 if state["gpu"]=="AMD/Intel" else 2)
    while True:
        stdscr.erase()
        stdscr.addstr(1,2,"GPU Drivers")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(7,4,"Up/Down Enter select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["gpu"] = "NVIDIA" if sel==0 else ("AMD/Intel" if sel==1 else "None")
            return
        elif k == ord("q"):
            sys.exit(0)

def screen_review_c(stdscr):
    curses.curs_set(0)
    while True:
        stdscr.erase()
        stdscr.addstr(1,2,"Review & Install")
        lines = [
            (L("Language","Idioma"), state["lang"]),
            (L("Hostname","Equipo"), state["hostname"]),
            (L("Username","Usuario"), state["username"]),
            (L("Disk","Disco"), state["disk"]),
            (L("Swap","Swap"), f"{state['swap']} GB"),
            (L("Desktop","Escritorio"), state["desktop"]),
            (L("GPU","GPU"), state["gpu"]),
            (L("Keymap","Keymap"), state["keymap"]),
            (L("Timezone","Zona horaria"), state["timezone"]),
            (L("Locale","Locale"), state["locale"])
        ]
        y = 3
        for k,v in lines:
            stdscr.addstr(y,4,f"{k}: {v}")
            y += 1
        stdscr.addstr(y+1,4,L("Enter to start install, q cancel","Enter para iniciar, q cancelar"))
        k = stdscr.getch()
        if k in (10,13):
            return True
        elif k == ord("q"):
            sys.exit(0)

class InstallerUI:
    def __init__(self, stdscr):
        self.stdscr = stdscr
        self.logs = []
        self.progress = 0.0
        self.lock = threading.Lock()
        curses.curs_set(0)

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
        s = self.stdscr
        s.erase()
        s.addstr(1,2,"Installing...")
        s.addstr(3,4,f"Progress: {int(self.progress)}%")
        width = min(60, max(20, curses.COLS-20))
        filled = int((self.progress/100.0)*width)
        bar = "[" + "#"*filled + "-"*(width-filled) + "]"
        s.addstr(4,4,bar)
        s.addstr(6,4,"Log:")
        y = 7
        with self.lock:
            for line in self.logs[-(curses.LINES - y - 4):]:
                try:
                    s.addstr(y,4,line[:curses.COLS-8])
                except curses.error:
                    pass
                y += 1
        s.addstr(curses.LINES-2,4,L("r reboot when finished, q quit","r reiniciar cuando termine, q salir"))
        s.refresh()

    def run_steps(self):
        steps = []
        steps.append(("Wipe disk",f"sgdisk -Z {state['disk']}",5))
        steps.append(("Create partitions",f"sgdisk -n1:0:+1G -t1:ef00 {state['disk']} && sgdisk -n2:0:+{state['swap']}G -t2:8200 {state['disk']} && sgdisk -n3:0:0 -t3:8300 {state['disk']}",10))
        diskname = os.path.basename(state['disk'])
        def p(n):
            if "nvme" in diskname or "mmcblk" in diskname:
                return f"/dev/{diskname}p{n}"
            return f"/dev/{diskname}{n}"
        p1,p2,p3 = p(1),p(2),p(3)
        steps.append(("Format",f"mkfs.fat -F32 {p1} && mkswap {p2} && swapon {p2} && mkfs.ext4 -F {p3}",10))
        steps.append(("Mount",f"mount {p3} /mnt && mkdir -p /mnt/boot/efi && mount {p1} /mnt/boot/efi",5))
        pkgs = "base linux linux-firmware linux-headers sof-firmware base-devel grub efibootmgr vim nano networkmanager sudo bash-completion"
        steps.append(("Pacstrap",f"pacstrap /mnt {pkgs}",30))
        steps.append(("fstab", "genfstab -U /mnt > /mnt/etc/fstab",5))
        cfg = []
        cfg.append(f"echo '{state['hostname']}' > /etc/hostname")
        cfg.append(f"ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime")
        cfg.append("hwclock --systohc")
        cfg.append("sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen || true")
        if state['locale'].startswith("es"):
            cfg.append("sed -i 's/^#es_ES.UTF-8/es_ES.UTF-8/' /etc/locale.gen || true")
        cfg.append("locale-gen")
        cfg.append(f"echo 'LANG={state['locale']}' > /etc/locale.conf")
        cfg.append("mkinitcpio -P")
        cfg.append(f"useradd -m -G wheel -s /bin/bash {state['username']}")
        cfg.append(f"echo '{state['username']}:{state['user_pass']}' | chpasswd")
        cfg.append(f"echo 'root:{state['root_pass']}' | chpasswd")
        cfg.append("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers || true")
        cfg_script = " && ".join(cfg)
        steps.append(("Configure system",f'arch-chroot /mnt /bin/bash -c "{cfg_script}"',10))
        desktop_cmds = []
        if state['desktop'] == "KDE Plasma":
            desktop_cmds.append("pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput plasma-meta konsole dolphin ark kate plasma-nm firefox sddm")
            desktop_cmds.append("systemctl enable sddm")
        elif state['desktop'] == "Cinnamon":
            desktop_cmds.append("pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput cinnamon lightdm lightdm-gtk-greeter alacritty firefox")
            desktop_cmds.append("systemctl enable lightdm")
        if state['gpu'] == "NVIDIA":
            desktop_cmds.append("pacman -S --noconfirm nvidia nvidia-utils nvidia-settings")
        elif state['gpu'] == "AMD/Intel":
            desktop_cmds.append("pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver")
        if desktop_cmds:
            joined = " && ".join([f'arch-chroot /mnt /bin/bash -c "{c}"' for c in desktop_cmds])
            steps.append(("Desktop & GPU", joined,10))
        else:
            steps.append(("Desktop & GPU","true",10))
        steps.append(("GRUB", 'arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB && arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg',10))
        total = sum(w for _,_,w in steps)
        done = 0.0
        if not check_network():
            self.add_log(L("Network unreachable. Please ensure network before installing.","Red inaccesible. Asegúrese de tener red antes de instalar."))
            return
        ensure_keyring()
        for desc,cmd,weight in steps:
            self.add_log(f"== {desc} ==")
            self.set_progress((done/total)*100.0)
            code = run_stream(cmd, on_line=self.add_log)
            if code != 0:
                self.add_log(f"ERROR: step failed ({desc}) code={code}")
                return
            done += weight
            self.set_progress((done/total)*100.0)
            time.sleep(0.2)
        self.add_log("✔ Installation complete")
        self.set_progress(100.0)

    def start(self):
        t = threading.Thread(target=self.run_steps, daemon=True)
        t.start()
        while True:
            self.redraw()
            k = self.stdscr.getch()
            if k == ord("q"):
                break
            if k == ord("r") and self.progress >= 99.0:
                append_buffer_add("Rebooting")
                subprocess.run("umount -R /mnt", shell=True)
                subprocess.run("reboot", shell=True)
                break

def screen_install_c(stdscr):
    ui = InstallerUI(stdscr)
    ui.start()

def main_curses(stdscr):
    idx = 0
    funcs = {
        "language": screen_language_c,
        "identity": screen_identity_c,
        "passwords": screen_passwords_c,
        "disk": screen_disk_c,
        "desktop": screen_desktop_c,
        "gpu": screen_gpu_c,
        "review": screen_review_c,
        "install": screen_install_c
    }
    while True:
        name = screens[idx]
        funcs[name](stdscr)
        if name == "install":
            break
        stdscr.addstr(curses.LINES-2,2,L("Use ← / → Enter to navigate q to quit","Use ← / → Enter para navegar q para salir"))
        k = stdscr.getch()
        if k == curses.KEY_RIGHT or k in (10,13):
            if idx < len(screens)-1:
                idx += 1
            else:
                break
        elif k == curses.KEY_LEFT:
            if idx > 0:
                idx -= 1
        elif k == ord("q"):
            break

def fallback_cli():
    print("TUI failed, falling back to CLI")
    while True:
        c = input(L("Select language: 1=EN 2=ES","Seleccione idioma: 1=EN 2=ES")).strip()
        if c == "1":
            state["lang"] = "en"
            break
        elif c == "2":
            state["lang"] = "es"
            break
    state["hostname"] = get_input(L("Enter hostname:","Ingrese nombre equipo:"),"")
    while not validate_name(state["hostname"]):
        state["hostname"] = get_input(L("Invalid. Enter hostname:","Inválido. Ingrese nombre:"),"")
    state["username"] = get_input(L("Enter username:","Ingrese usuario:"),"")
    while not validate_name(state["username"]):
        state["username"] = get_input(L("Invalid. Enter username:","Inválido. Ingrese usuario:"),"")
    state["root_pass"] = prompt_password(L("Enter root password:","Ingrese contraseña root:"))
    state["user_pass"] = prompt_password(L("Enter user password:","Ingrese contraseña usuario:"))
    state["swap"] = get_input(L("Enter swap GB (8):","Ingrese swap GB (8):"), "8")
    while not validate_swap(state["swap"]):
        state["swap"] = get_input(L("Invalid swap. Enter swap GB:","Swap inválido. Ingrese swap GB:"), "8")
    while True:
        d = list_disks()
        if not d:
            print(L("No disks found. Aborting.","No se encontraron discos. Abortando."))
            sys.exit(1)
        for i,(name,gb) in enumerate(d, start=1):
            print(f"{i}. /dev/{name} ({gb} GB)")
        choice = input(L("Select disk number:","Seleccione número disco:")).strip()
        if choice.isdigit() and 1 <= int(choice) <= len(d):
            state["disk"] = d[int(choice)-1][0]
            break
    print(L("Review config and start install","Revise la configuración e inicie"))
    for k in ("lang","hostname","username","disk","swap","desktop","gpu"):
        print(f"{k}: {state.get(k)}")
    ok = input(L("Proceed? (y/n)","Proceder? (y/n)")).strip().lower()
    if ok == "y":
        i = InstallerUI(None)
        i.run_steps()

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root in the Arch live environment.")
        sys.exit(1)
    try:
        import shutil
        curses.wrapper(main_curses)
    except Exception:
        fallback_cli()
