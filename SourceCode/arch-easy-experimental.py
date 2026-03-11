import curses
import subprocess
import sys
import re
from datetime import datetime

LOG_FILE = "/mnt/install_log.txt"

state = {
    "lang": "en",
    "hostname": "",
    "username": "",
    "root_pass": "",
    "user_pass": "",
    "swap": "8",
    "disk": None,
    "desktop": "KDE Plasma",
    "gpu": "None"
}

screens = [
    "language",
    "identity",
    "passwords",
    "disk",
    "desktop",
    "gpu",
    "review",
    "install"
]

current_screen = 0


def log(msg):
    timestamp = datetime.now().strftime("%H:%M:%S")
    line = f"[{timestamp}] {msg}"
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except:
        pass
    return line


def run(cmd):
    subprocess.run(cmd, shell=True)


def list_disks():
    try:
        output = subprocess.check_output(
            "lsblk -b -d -o NAME,SIZE | tail -n +2",
            shell=True
        ).decode()

        disks = []
        for line in output.splitlines():
            name, size = line.split()
            gb = int(size) // (1024**3)
            disks.append(f"/dev/{name} ({gb}GB)")
        return disks
    except:
        return []


disks_cache = list_disks()


def draw_header(stdscr, title):
    stdscr.clear()
    stdscr.addstr(1, 4, "Arch Linux Installer", curses.A_BOLD)
    stdscr.addstr(2, 4, title)
    stdscr.hline(3, 4, "-", 60)


def draw_footer(stdscr):
    stdscr.hline(20, 4, "-", 60)
    stdscr.addstr(21, 4, "← Back    → Next")


def input_field(stdscr, y, label, value):
    stdscr.addstr(y, 4, label)
    stdscr.addstr(y, 25, value)
    curses.echo()
    val = stdscr.getstr(y, 25).decode()
    curses.noecho()
    return val


def password_field(stdscr, y, label):
    stdscr.addstr(y, 4, label)
    pwd = ""
    while True:
        ch = stdscr.getch()
        if ch == 10:
            break
        pwd += chr(ch)
        stdscr.addstr("*")
    return pwd


def menu(stdscr, y, options, selected):
    while True:
        for i, opt in enumerate(options):
            marker = "(x)" if i == selected else "( )"
            stdscr.addstr(y + i, 4, f"{marker} {opt}")
        key = stdscr.getch()
        if key == curses.KEY_UP:
            selected = (selected - 1) % len(options)
        elif key == curses.KEY_DOWN:
            selected = (selected + 1) % len(options)
        elif key == 10:
            return selected


def screen_language(stdscr):
    draw_header(stdscr, "Language")
    choice = menu(stdscr, 6, ["English", "Español"], 0)
    state["lang"] = "en" if choice == 0 else "es"


def screen_identity(stdscr):
    draw_header(stdscr, "System Identity")
    state["hostname"] = input_field(stdscr, 6, "Hostname:", state["hostname"])
    state["username"] = input_field(stdscr, 8, "Username:", state["username"])


def screen_passwords(stdscr):
    draw_header(stdscr, "Passwords")
    stdscr.addstr(6,4,"Root password:")
    state["root_pass"] = password_field(stdscr,6,"")
    stdscr.addstr(8,4,"User password:")
    state["user_pass"] = password_field(stdscr,8,"")


def screen_disk(stdscr):
    draw_header(stdscr, "Disk & Swap")

    state["swap"] = input_field(stdscr,6,"Swap size (GB):",state["swap"])

    selected = 0
    choice = menu(stdscr,9,disks_cache,selected)
    state["disk"] = disks_cache[choice]


def screen_desktop(stdscr):
    draw_header(stdscr, "Desktop")
    opts = ["KDE Plasma", "Cinnamon", "None"]
    choice = menu(stdscr,6,opts,0)
    state["desktop"] = opts[choice]


def screen_gpu(stdscr):
    draw_header(stdscr, "GPU")
    opts = ["NVIDIA", "AMD / Intel", "None"]
    choice = menu(stdscr,6,opts,0)
    state["gpu"] = opts[choice]


def screen_review(stdscr):
    draw_header(stdscr, "Review configuration")

    y = 6
    for k,v in state.items():
        stdscr.addstr(y,4,f"{k}: {v}")
        y += 1

    stdscr.addstr(y+2,4,"Press Enter to start installation")
    stdscr.getch()


def screen_install(stdscr):
    draw_header(stdscr, "Installing")

    logs = []

    commands = [
        "pacstrap /mnt base linux linux-firmware",
        "genfstab -U /mnt >> /mnt/etc/fstab"
    ]

    y = 6
    for cmd in commands:
        line = log(f"Running: {cmd}")
        logs.append(line)
        stdscr.addstr(y,4,line)
        stdscr.refresh()
        run(cmd)
        y += 1

    stdscr.addstr(y+2,4,"Installation finished.")
    stdscr.addstr(y+4,4,"Press r to reboot")

    while True:
        key = stdscr.getch()
        if key == ord("r"):
            run("reboot")
            break


screen_functions = {
    "language": screen_language,
    "identity": screen_identity,
    "passwords": screen_passwords,
    "disk": screen_disk,
    "desktop": screen_desktop,
    "gpu": screen_gpu,
    "review": screen_review,
    "install": screen_install
}


def main(stdscr):
    global current_screen

    curses.curs_set(0)

    while True:
        screen_name = screens[current_screen]
        screen_functions[screen_name](stdscr)

        if screen_name == "install":
            break

        draw_footer(stdscr)

        key = stdscr.getch()

        if key == curses.KEY_RIGHT and current_screen < len(screens)-1:
            current_screen += 1
        elif key == curses.KEY_LEFT and current_screen > 0:
            current_screen -= 1


curses.wrapper(main)
