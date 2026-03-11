import subprocess
import sys
import re
from datetime import datetime
import termios
import tty

LOG_FILE = "/mnt/install_log.txt"


def log(msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    print(f"[{timestamp}] {msg}")
    try:
        with open(LOG_FILE, "a") as f:
            f.write(f"[{timestamp}] {msg}\n")
    except Exception:
        pass


def run(cmd, ignore_error=False):
    log(f"Running: {cmd}")
    result = subprocess.run(cmd, shell=True)
    if result.returncode != 0:
        log(f"ERROR: Command failed: {cmd}")
        if not ignore_error:
            print("Command failed. Aborting.")
            sys.exit(1)


def chroot(cmd):
    run(f"arch-chroot /mnt /bin/bash -c \"{cmd}\"", ignore_error=True)


def L(msg_en, msg_es):
    return msg_en if lang == "en" else msg_es


lang = None
while True:
    print("Select language / Seleccione idioma: 1 = English, 2 = Español")
    choice = input("> ").strip()
    if choice == "1":
        lang = "en"
        break
    elif choice == "2":
        lang = "es"
        break
    else:
        print("Invalid / Inválido. Try again.")


def confirm(msg):
    while True:
        resp = input(f"{msg} (y/n): ").strip().lower()
        if resp in ("y", "n"):
            return resp == "y"
        print(L("Invalid input, try again.", "Entrada inválida, intente de nuevo."))


def valid_name(name):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", name))


def valid_swap(size_str):
    return bool(re.match(r"^\d+$", size_str)) and int(size_str) > 0


def input_validated(prompt, validator, error_msg):
    while True:
        val = input(prompt + " ").strip()
        if validator(val):
            return val
        print(error_msg)


def input_password(prompt):
    print(prompt)
    password = ""
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ('\n', '\r'):
                print()
                break
            elif ch == '\x7f':
                if password:
                    password = password[:-1]
                    print('\b \b', end='', flush=True)
            else:
                password += ch
                print('*', end='', flush=True)
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return password


def set_password(user, password):
    chroot(f"echo '{user}:{password}' | chpasswd")


def list_disks():
    output = subprocess.check_output(
        "lsblk -b -d -o NAME,SIZE | tail -n +2", shell=True
    ).decode()
    disks = []
    for line in output.splitlines():
        parts = line.split()
        if len(parts) < 2:
            continue
        name, size = parts[0], parts[1]
        try:
            size_gb = int(size) // (1024 ** 3)
        except ValueError:
            continue
        disks.append((name, size_gb))
    return disks


def choose_disk():
    disks = list_disks()
    if not disks:
        print(L("No disks found. Aborting.", "No se encontraron discos. Abortando."))
        sys.exit(1)
    print(L("Available disks:", "Discos disponibles:"))
    for i, (name, size) in enumerate(disks):
        print(f"  {i+1}. /dev/{name} ({size} GB)")
    while True:
        choice = input(L("Select disk number: ", "Seleccione número de disco: ")).strip()
        if choice.isdigit() and 1 <= int(choice) <= len(disks):
            return disks[int(choice) - 1][0]
        print(L("Invalid, try again.", "Inválido, intente de nuevo."))


def choose_desktop():
    while True:
        choice = input(L(
            "Choose desktop: 1 = KDE Plasma, 2 = Cinnamon, 0 = None: ",
            "Seleccione escritorio: 1 = KDE Plasma, 2 = Cinnamon, 0 = Ninguno: "
        )).strip()
        if choice in ("0", "1", "2"):
            return choice
        print(L("Invalid, try again.", "Inválido, intente de nuevo."))


def partition_suffix(disk, n):
    if "nvme" in disk or "mmcblk" in disk:
        return f"/dev/{disk}p{n}"
    return f"/dev/{disk}{n}"


hostname = input_validated(L("Enter hostname:", "Ingrese el nombre del equipo:"), valid_name, L("Invalid hostname.", "Nombre inválido."))
username = input_validated(L("Enter username:", "Ingrese nombre de usuario:"), valid_name, L("Invalid username.", "Usuario inválido."))
root_pass = input_password(L("Enter root password:", "Ingrese contraseña de root:"))
user_pass = input_password(L("Enter user password:", "Ingrese contraseña de usuario:"))
swap_size = input_validated(L("Enter swap size in GB (e.g. 8):", "Ingrese tamaño de swap en GB (ej. 8):"), valid_swap, L("Invalid swap size.", "Tamaño inválido."))
desktop_choice = choose_desktop()
disk = choose_disk()
disk_path = f"/dev/{disk}"

log(f"{L('Selected disk', 'Disco seleccionado')}: {disk_path}")

partitions = subprocess.check_output(
    f"lsblk -n -o NAME {disk_path}", shell=True
).decode().splitlines()[1:]

if partitions:
    print(f"{L('Partitions detected on', 'Particiones detectadas en')} {disk_path}: {', '.join(p.strip() for p in partitions)}")
    if confirm(L("Erase all existing partitions?", "¿Borrar todas las particiones existentes?")):
        run(f"sgdisk -Z {disk_path}")
    else:
        print(L("Cannot continue with existing partitions. Aborting.", "No se puede continuar con particiones existentes. Abortando."))
        sys.exit(0)

log(L("Creating partitions...", "Creando particiones..."))
run(f"sgdisk -n1:0:+1G   -t1:ef00 {disk_path}")
run(f"sgdisk -n2:0:+{swap_size}G -t2:8200 {disk_path}")
run(f"sgdisk -n3:0:0     -t3:8300 {disk_path}")

p1 = partition_suffix(disk, 1)
p2 = partition_suffix(disk, 2)
p3 = partition_suffix(disk, 3)

log(L("Formatting partitions...", "Formateando particiones..."))
run(f"mkfs.fat -F32 {p1}")
run(f"mkswap {p2}")
run(f"swapon {p2}")
run(f"mkfs.ext4 -F {p3}")

log(L("Mounting partitions...", "Montando particiones..."))
run(f"mount {p3} /mnt")
run("mkdir -p /mnt/boot/efi")
run(f"mount {p1} /mnt/boot/efi")

log(L("Installing base system...", "Instalando sistema base..."))
packages = (
    "base linux linux-firmware linux-headers sof-firmware "
    "base-devel grub efibootmgr vim nano networkmanager "
    "sudo bash-completion"
)
run(f"pacstrap /mnt {packages}")
run("genfstab -U /mnt >> /mnt/etc/fstab")

log(L("Configuring system...", "Configurando sistema..."))

with open("/mnt/etc/hostname", "w") as f:
    f.write(hostname + "\n")

chroot("ln -sf /usr/share/zoneinfo/UTC /etc/localtime")
chroot("hwclock --systohc")
chroot("sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen")
chroot("locale-gen")
chroot("echo 'LANG=en_US.UTF-8' > /etc/locale.conf")

chroot("mkinitcpio -P")

set_password("root", root_pass)
chroot(f"useradd -m -G wheel -s /bin/bash {username}")
set_password(username, user_pass)

chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers")

chroot("systemctl enable NetworkManager")

gpu = None
while True:
    gpu_input = input(L(
        "Select GPU: 1 = NVIDIA, 2 = AMD/Intel, 0 = None: ",
        "Seleccione GPU: 1 = NVIDIA, 2 = AMD/Intel, 0 = Ninguna: "
    )).strip()
    if gpu_input in ("0", "1", "2"):
        gpu = gpu_input
        break
    print(L("Invalid, try again.", "Inválido, intente de nuevo."))

if gpu == "1":
    chroot("pacman -S --noconfirm nvidia nvidia-utils nvidia-settings")
elif gpu == "2":
    chroot("pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver")

if desktop_choice == "1":
    log(L("Installing KDE Plasma...", "Instalando KDE Plasma..."))
    chroot(
        "pacman -S --noconfirm "
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput "
        "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm"
    )
    chroot("systemctl enable sddm")

elif desktop_choice == "2":
    log(L("Installing Cinnamon...", "Instalando Cinnamon..."))
    chroot(
        "pacman -S --noconfirm "
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput "
        "cinnamon lightdm lightdm-gtk-greeter alacritty firefox"
    )
    chroot("systemctl enable lightdm")

log(L("Installing GRUB...", "Instalando GRUB..."))
run("arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB")
run("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg")

log(L("Installation complete!", "¡Instalación completada!"))
if confirm(L("Reboot now?", "¿Reiniciar ahora?")):
    run("umount -R /mnt")
    run("reboot")
