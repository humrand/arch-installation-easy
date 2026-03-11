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

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

def log(msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {msg}"
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except Exception:
        pass
    return line

def run(cmd, stream=False):
    log(f"Running: {cmd}")
    if not stream:
        p = subprocess.run(cmd, shell=True)
        return p.returncode
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
    while True:
        line = p.stdout.readline()
        if line == "" and p.poll() is not None:
            break
        if line:
            line = line.rstrip("\n")
            append_log(line)
    return p.wait()

def chroot(cmd, stream=False):
    quoted = cmd.replace('"', '\\"')
    return run(f'arch-chroot /mnt /bin/bash -c "{quoted}"', stream=stream)

def append_log(line):
    s = log(line)
    try:
        with append_lock:
            append_buffer.append(s)
            if len(append_buffer) > 1000:
                del append_buffer[:500]
    except Exception:
        pass

def list_disks():
    try:
        out = subprocess.check_output("lsblk -b -d -o NAME,SIZE | tail -n +2", shell=True, text=True)
        disks = []
        for line in out.splitlines():
            parts = line.split()
            if len(parts) < 2:
                continue
            name, size = parts[0], parts[1]
            try:
                gb = int(size) // (1024**3)
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

def choose_language():
    while True:
        print("Select language / Seleccione idioma: 1 = English, 2 = Español")
        choice = input("> ").strip()
        if choice == "1":
            state["lang"] = "en"
            break
        elif choice == "2":
            state["lang"] = "es"
            break
        else:
            print("Invalid / Inválido. Try again.")

def confirm(msg):
    while True:
        resp = input(f"{msg} (y/n): ").strip().lower()
        if resp in ("y","n"):
            return resp == "y"
        print(L("Invalid input, try again.","Entrada inválida, intente de nuevo."))

def valid_name(name):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", name))

def valid_swap(s):
    return bool(re.match(r"^\d+$", s)) and int(s) > 0

def input_validated(prompt, validator, error_msg):
    while True:
        val = input(prompt + " ").strip()
        if validator(val):
            return val
        print(error_msg)

def input_password_prompt(prompt):
    print(prompt)
    password = ""
    fd = sys.stdin.fileno()
    old = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ("\n","\r"):
                print()
                break
            elif ch == "\x7f":
                if password:
                    password = password[:-1]
                    print("\b \b", end="", flush=True)
            else:
                password += ch
                print("*", end="", flush=True)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old)
    return password

def paginate_menu(options, title):
    per_page = 10
    idx = 0
    while True:
        os.system("clear")
        print(title)
        page = options[idx:idx+per_page]
        for i,opt in enumerate(page, start=1):
            print(f"{i}. {opt}")
        print()
        print(L("n: next page, p: prev page, q: cancel","n: siguiente página, p: página anterior, q: cancelar"))
        choice = input("> ").strip()
        if choice.isdigit():
            n = int(choice)
            if 1 <= n <= len(page):
                return idx + n - 1
        elif choice == "n":
            if idx + per_page < len(options):
                idx += per_page
        elif choice == "p":
            if idx - per_page >= 0:
                idx -= per_page
        elif choice == "q":
            return None
        else:
            print(L("Invalid, try again.","Inválido, intente de nuevo."))

def screen_language_tui(stdscr):
    curses.curs_set(0)
    opts = ["English","Español"]
    sel = 0 if state["lang"] == "en" else 1
    while True:
        stdscr.clear()
        stdscr.addstr(1,2,"Language / Idioma")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(8,4,"Use up/down to change, Enter to select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["lang"] = "en" if sel==0 else "es"
            break
        elif k == ord("q"):
            sys.exit(0)

def get_input_curses(stdscr, y, x, prompt, initial="", secret=False):
    curses.echo() if not secret else curses.noecho()
    stdscr.addstr(y, x, prompt)
    stdscr.refresh()
    win = curses.newwin(1,60,y,x+len(prompt)+1)
    win.addstr(0,0,initial)
    win.refresh()
    s = ""
    if not secret:
        s = win.getstr().decode().strip()
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

def screen_identity_tui(stdscr):
    curses.curs_set(1)
    stdscr.clear()
    stdscr.addstr(1,2,"System Identity")
    state["hostname"] = get_input_curses(stdscr,4,4,"Hostname:",state.get("hostname",""),secret=False)
    state["username"] = get_input_curses(stdscr,6,4,"Username:",state.get("username",""),secret=False)

def screen_passwords_tui(stdscr):
    curses.curs_set(1)
    stdscr.clear()
    stdscr.addstr(1,2,"Passwords")
    state["root_pass"] = get_input_curses(stdscr,4,4,"Root password:",secret=True)
    state["user_pass"] = get_input_curses(stdscr,6,4,"User password:",secret=True)

def screen_disk_tui(stdscr):
    curses.curs_set(0)
    disks = list_disks()
    if not disks:
        stdscr.addstr(2,2,L("No disks found. Aborting.","No se encontraron discos. Abortando."))
        stdscr.getch()
        sys.exit(1)
    opts = [f"/dev/{d} ({s} GB)" for d,s in disks]
    sel = 0
    while True:
        stdscr.clear()
        stdscr.addstr(1,2,"Disk & Swap")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(3+len(opts)+1,4,f"Swap size in GB: {state['swap']}")
        stdscr.addstr(3+len(opts)+3,4,"Use up/down, Enter to select disk, s to change swap")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["disk"] = disks[sel][0]
            break
        elif k == ord("s"):
            curses.echo()
            stdscr.addstr(3+len(opts)+1,4,"Swap size in GB: ")
            val = stdscr.getstr(3+len(opts)+1,24,6).decode().strip()
            curses.noecho()
            if re.match(r"^\d+$", val) and int(val)>0:
                state["swap"] = val
        elif k == ord("q"):
            sys.exit(0)

def screen_desktop_tui(stdscr):
    curses.curs_set(0)
    opts = ["KDE Plasma","Cinnamon","None"]
    sel = opts.index(state["desktop"]) if state["desktop"] in opts else 2
    while True:
        stdscr.clear()
        stdscr.addstr(1,2,"Desktop Environment")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(7,4,"Use up/down to change, Enter to select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["desktop"] = opts[sel]
            break
        elif k == ord("q"):
            sys.exit(0)

def screen_gpu_tui(stdscr):
    curses.curs_set(0)
    opts = ["NVIDIA","AMD / Intel","None"]
    sel = 0 if state["gpu"] == "NVIDIA" else (1 if state["gpu"]=="AMD/Intel" else 2)
    while True:
        stdscr.clear()
        stdscr.addstr(1,2,"GPU Drivers")
        for i,opt in enumerate(opts):
            marker = "[x]" if i==sel else "[ ]"
            stdscr.addstr(3+i,4,f"{marker} {opt}")
        stdscr.addstr(7,4,"Use up/down to change, Enter to select")
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["gpu"] = "NVIDIA" if sel==0 else ("AMD/Intel" if sel==1 else "None")
            break
        elif k == ord("q"):
            sys.exit(0)

def screen_review_tui(stdscr):
    curses.curs_set(0)
    while True:
        stdscr.clear()
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
        stdscr.addstr(y+1,4,L("Press Enter to start install, q to cancel","Presione Enter para iniciar, q para cancelar"))
        k = stdscr.getch()
        if k in (10,13):
            return True
        elif k == ord("q"):
            sys.exit(0)

class InstallerUI:
    def __init__(self,stdscr):
        self.stdscr = stdscr
        self.logs = []
        self.progress = 0.0
        self.lock = threading.Lock()
        curses.curs_set(0)

    def add_line(self,line):
        with self.lock:
            self.logs.append(line)
            if len(self.logs) > 500:
                self.logs = self.logs[-400:]
        append_log(line)
        self.redraw()

    def set_progress(self,pct):
        with self.lock:
            self.progress = max(0.0,min(100.0,pct))
        self.redraw()

    def redraw(self):
        s = self.stdscr
        s.erase()
        s.addstr(1,2,"Installing...")
        s.addstr(3,4,f"Progress: {int(self.progress)}%")
        width = 60
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
        s.addstr(curses.LINES-2,4,L("When finished press r to reboot, q to quit","Cuando termine presione r para reiniciar, q para salir"))
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
        for desc,cmd,weight in steps:
            self.add_line(f"== {desc} ==")
            self.set_progress((done/total)*100.0)
            code = run(cmd, stream=True)
            if code != 0:
                self.add_line(f"ERROR: step failed ({desc}) code={code}")
                return
            done += weight
            self.set_progress((done/total)*100.0)
            time.sleep(0.2)
        self.add_line("✔ Installation complete")
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
                append_log("Rebooting")
                subprocess.run("umount -R /mnt", shell=True)
                subprocess.run("reboot", shell=True)
                break

def screen_install_tui(stdscr):
    ui = InstallerUI(stdscr)
    ui.start()

def main_curses(stdscr):
    idx = 0
    funcs = {
        "language": screen_language_tui,
        "identity": screen_identity_tui,
        "passwords": screen_passwords_tui,
        "disk": screen_disk_tui,
        "desktop": screen_desktop_tui,
        "gpu": screen_gpu_tui,
        "review": screen_review_tui,
        "install": screen_install_tui
    }
    while True:
        name = screens[idx]
        funcs[name](stdscr)
        if name == "install":
            break
        stdscr.addstr(curses.LINES-2,2,L("Use ← / → to navigate, q to quit","Use ← / → para navegar, q para salir"))
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

def run_cli_fallback():
    choose_language()
    state["hostname"] = input_validated(L("Enter hostname:","Ingrese el nombre del equipo:"), valid_name, L("Invalid hostname.","Nombre inválido."))
    state["username"] = input_validated(L("Enter username:","Ingrese nombre de usuario:"), valid_name, L("Invalid username.","Usuario inválido."))
    state["root_pass"] = input_password_prompt(L("Enter root password:","Ingrese contraseña de root:"))
    state["user_pass"] = input_password_prompt(L("Enter user password:","Ingrese contraseña de usuario:"))
    state["swap"] = input_validated(L("Enter swap size in GB (e.g. 8):","Ingrese tamaño de swap en GB (ej. 8):"), valid_swap, L("Invalid swap size.","Tamaño inválido."))
    while True:
        c = input(L("Choose desktop: 1=KDE,2=Cinnamon,0=None:","Seleccione escritorio: 1=KDE,2=Cinnamon,0=Ninguno:")).strip()
        if c in ("0","1","2"):
            state["desktop"] = "KDE Plasma" if c=="1" else ("Cinnamon" if c=="2" else "None")
            break
    disks = list_disks()
    if not disks:
        print(L("No disks found. Aborting.","No se encontraron discos. Abortando."))
        sys.exit(1)
    for i,(d,s) in enumerate(disks, start=1):
        print(f"{i}. /dev/{d} ({s} GB)")
    while True:
        choice = input(L("Select disk number:","Seleccione número de disco:")).strip()
        if choice.isdigit() and 1 <= int(choice) <= len(disks):
            state["disk"] = disks[int(choice)-1][0]
            break
    print(L("Review:","Revisar:"))
    for k in ("lang","hostname","username","disk","swap","desktop","gpu"):
        print(f"{k}: {state.get(k)}")
    if confirm(L("Proceed with installation?","¿Proceder con la instalación?")):
        installer = InstallerUI(None)
        installer.run_steps()

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root in the Arch live environment.")
        sys.exit(1)
    try:
        curses.wrapper(main_curses)
    except Exception:
        run_cli_fallback()
