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

def run_stream(cmd, on_line=None, progress_anim=None, cwd=None, ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    p = subprocess.Popen(cmd, shell=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True, cwd=cwd, executable="/bin/bash")
    stop_event = threading.Event()
    anim_thread = None
    if progress_anim:
        ui, base, target = progress_anim
        def _anim():
            cur = base
            ui.set_progress(cur)
            step = max(0.2, (target - base) / 200.0)
            while not stop_event.is_set() and cur < target:
                cur = min(target, cur + step)
                ui.set_progress(cur)
                time.sleep(0.12)
            ui.set_progress(target)
        anim_thread = threading.Thread(target=_anim, daemon=True)
        anim_thread.start()
    while True:
        line = p.stdout.readline()
        if line == "" and p.poll() is not None:
            break
        if line:
            l = line.rstrip("\n")
            append_buffer_add(l)
            if on_line:
                on_line(l)
    rc = p.wait()
    if progress_anim:
        stop_event.set()
        if anim_thread:
            anim_thread.join(timeout=1.0)
        ui.set_progress(progress_anim[2])
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
    except subprocess.CalledProcessError:
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

def partprobe_and_settle(disk_path):
    run_simple(f"partprobe {disk_path}", ignore_error=True)
    run_simple("udevadm settle --exit-if-exists=/dev/null", ignore_error=True)
    time.sleep(1)

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
    per = 12
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

def curses_picker(stdscr, options, title):
    per_page = curses.LINES - 6
    idx = 0
    sel = 0
    while True:
        stdscr.erase()
        stdscr.addstr(1,2, title)
        page = options[idx:idx+per_page]
        for i,opt in enumerate(page):
            marker = ">" if i==sel else " "
            try:
                stdscr.addstr(3+i,4,f"{marker} {opt}")
            except curses.error:
                pass
        stdscr.addstr(curses.LINES-2,4,"Use ↑/↓ PageUp/PageDown Enter select q cancel")
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
        elif k in (10,13):
            return idx + sel
        elif k == ord("q"):
            return None

append_buffer = []
append_lock = threading.Lock()

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

screens = ["language","identity","passwords","disk","keymap","timezone","desktop","gpu","review","install"]

def L(en, es):
    return en if state.get("lang", "en") == "en" else es

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
        stdscr.addstr(8,4,"Use up/down Enter to select, c to choose from list")
        stdscr.refresh()
        k = stdscr.getch()
        if k == curses.KEY_UP:
            sel = (sel - 1) % len(opts)
        elif k == curses.KEY_DOWN:
            sel = (sel + 1) % len(opts)
        elif k in (10,13):
            state["lang"] = "en" if sel==0 else "es"
            return
        elif k == ord("c"):
            idx = curses_picker(stdscr, opts, "Choose Language")
            if idx is not None:
                state["lang"] = "en" if idx==0 else "es"
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
        stdscr.refresh()
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

def screen_keymap_c(stdscr):
    curses.curs_set(0)
    stdscr.erase()
    stdscr.addstr(1,2,"Keymap / Teclado")
    try:
        out = subprocess.check_output("localectl list-keymaps 2>/dev/null || true", shell=True, text=True)
        maps = [l for l in out.splitlines() if l]
    except Exception:
        maps = []
    common = ["us","es","fr","de","it","pt","la-latin1"]
    options = [m for m in common if m in maps]
    if not options:
        options = maps[:60] if maps else common
    idx = curses_picker(stdscr, options, "Choose keymap")
    if idx is not None:
        km = options[idx]
        state["keymap"] = km
        run_simple(f"loadkeys {km}", ignore_error=True)
    return

def screen_timezone_c(stdscr):
    curses.curs_set(0)
    stdscr.erase()
    stdscr.addstr(1,2,"Timezone / Zona horaria")
    try:
        out = subprocess.check_output("timedatectl list-timezones 2>/dev/null || true", shell=True, text=True)
        zones = out.splitlines()
    except Exception:
        zones = ["UTC","Europe/Madrid","America/New_York","America/Los_Angeles","Asia/Tokyo"]
    idx = curses_picker(stdscr, zones, "Choose timezone")
    if idx is not None:
        state["timezone"] = zones[idx]
    return

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
        stdscr.refresh()
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
        stdscr.refresh()
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
            ("Language", state["lang"]),
            ("Hostname", state["hostname"]),
            ("Username", state["username"]),
            ("Disk", state["disk"]),
            ("Swap", f"{state['swap']} GB"),
            ("Desktop", state["desktop"]),
            ("GPU", state["gpu"]),
            ("Keymap", state["keymap"]),
            ("Timezone", state["timezone"]),
            ("Locale", state["locale"])
        ]
        y = 3
        for k,v in lines:
            stdscr.addstr(y,4,f"{k}: {v}")
            y += 1
        stdscr.addstr(y+1,4,"Enter to start install, q cancel")
        stdscr.refresh()
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
        s.addstr(curses.LINES-2,4,"r reboot when finished, q quit")
        s.refresh()

    def run_steps(self):
        steps = []
        disk_device = state["disk"]
        steps.append(("Wipe disk",f"sgdisk -Z /dev/{disk_device}",5))
        steps.append(("Create partitions",f"sgdisk -n1:0:+1G -t1:ef00 /dev/{disk_device} && sgdisk -n2:0:+{state['swap']}G -t2:8200 /dev/{disk_device} && sgdisk -n3:0:0 -t3:8300 /dev/{disk_device}",10))
        diskpath = f"/dev/{disk_device}"
        p1,p2,p3 = partition_paths_for(diskpath)
        steps.append(("Format",f"mkfs.fat -F32 {p1} && mkswap {p2} && swapon {p2} && mkfs.ext4 -F {p3}",10))
        steps.append(("Mount",f"mount {p3} /mnt && mkdir -p /mnt/boot/efi && mount {p1} /mnt/boot/efi",5))
        pkgs = "base linux linux-firmware linux-headers sof-firmware base-devel grub efibootmgr vim nano networkmanager sudo bash-completion"
        steps.append(("Pacstrap",f"pacstrap /mnt {pkgs}",30))
        steps.append(("fstab", "genfstab -U /mnt >> /mnt/etc/fstab",5))
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
            joined = " && ".join([f'arch-chroot /mnt /bin/bash -c \"{c}\"' for c in desktop_cmds])
            steps.append(("Desktop & GPU", joined,10))
        else:
            steps.append(("Desktop & GPU","true",10))
        steps.append(("GRUB", f'arch-chroot /mnt /bin/bash -c "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB" && arch-chroot /mnt /bin/bash -c "grub-mkconfig -o /boot/grub/grub.cfg"',10))
        total = sum(w for _,_,w in steps)
        done = 0.0
        if not ensure_network():
            self.add_log("Network unreachable. Ensure network before installing.")
            return
        run_stream("pacman -Sy --noconfirm archlinux-keyring", on_line=self.add_log, ignore_error=True)
        for desc,cmd,weight in steps:
            self.add_log(f"== {desc} ==")
            base = (done/total)*100.0
            target = ((done+weight)/total)*100.0
            if desc == "Pacstrap":
                base = max(base, 5.0)
                target = max(target, 15.0)
            self.set_progress(base + 0.5)
            code = run_stream(cmd, on_line=self.add_log, progress_anim=(self, base, target), ignore_error=False)
            if code != 0:
                self.add_log(f"ERROR: step failed ({desc}) code={code}")
                return
            done += weight
            self.set_progress((done/total)*100.0)
            time.sleep(0.25)
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
        "keymap": screen_keymap_c,
        "timezone": screen_timezone_c,
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
        stdscr.addstr(curses.LINES-2,2,"Use ← / → Enter to navigate q to quit")
        stdscr.refresh()
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
    while True:
        c = input("Select language: 1=EN 2=ES ").strip()
        if c == "1":
            state["lang"] = "en"
            break
        elif c == "2":
            state["lang"] = "es"
            break
    state["hostname"] = get_input("Enter hostname:", "")
    while not validate_name(state["hostname"]):
        state["hostname"] = get_input("Invalid. Enter hostname:", "")
    state["username"] = get_input("Enter username:", "")
    while not validate_name(state["username"]):
        state["username"] = get_input("Invalid. Enter username:", "")
    state["root_pass"] = prompt_password("Enter root password:")
    state["user_pass"] = prompt_password("Enter user password:")
    state["swap"] = get_input("Enter swap GB (8):", "8")
    while not validate_swap(state["swap"]):
        state["swap"] = get_input("Invalid swap. Enter swap GB:", "8")
    while True:
        d = list_disks()
        if not d:
            print("No disks found. Aborting.")
            sys.exit(1)
        for i,(name,gb) in enumerate(d, start=1):
            print(f"{i}. /dev/{name} ({gb} GB)")
        choice = input("Select disk number:").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(d):
            state["disk"] = d[int(choice)-1][0]
            break
    print("Review config and start install")
    for k in ("lang","hostname","username","disk","swap","desktop","gpu"):
        print(f"{k}: {state.get(k)}")
    ok = input("Proceed? (y/n)").strip().lower()
    if ok == "y":
        i = InstallerUI(None)
        i.run_steps()

def get_input(prompt, default=""):
    sys.stdout.write(prompt + (" [" + str(default) + "]" if default else "") + " ")
    sys.stdout.flush()
    s = input().strip()
    return s if s else default

if __name__ == "__main__":
    if os.geteuid() != 0:
        print("Run as root in the Arch live environment.")
        sys.exit(1)
    try:
        curses.wrapper(main_curses)
    except Exception:
        fallback_cli()
