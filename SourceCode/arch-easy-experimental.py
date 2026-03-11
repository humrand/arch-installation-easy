import subprocess
import sys
import re
import os
from datetime import datetime
import termios
import tty
import time

# MIT LICENSE, YOU CAN USE IT BUT YOU GOTTA GIVE ME CREDITS,
# made by humrand https://github.com/humrand/arch-anstallation-easy
# DO NOT REMOVE THIS FROM YOUR CODE IF YOU USE IT TO MODIFY IT.

LOG_FILE = "/mnt/install_log.txt"

def log(msg):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {msg}"
    print(line)
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line + "\n")
    except:
        pass

def run(cmd, ignore_error=False, capture=False):
    log(f"Running: {cmd}")
    try:
        if capture:
            p = subprocess.run(cmd, shell=True, text=True, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, executable="/bin/bash")
            if p.stdout:
                for l in p.stdout.splitlines():
                    log(l)
            rc = p.returncode
        else:
            rc = subprocess.call(cmd, shell=True, executable="/bin/bash")
        if rc != 0:
            log(f"ERROR: Command failed ({rc}): {cmd}")
            if not ignore_error:
                print("Command failed. Aborting.")
                sys.exit(1)
        return rc
    except Exception as e:
        log(f"EXCEPTION running command: {e}")
        if not ignore_error:
            sys.exit(1)
        return 1

def confirm(msg):
    while True:
        resp = input(f"{msg} (y/n): ").strip().lower()
        if resp in ("y","n"):
            return resp == "y"
        print("Invalid command, try again.")

def valid_name(name):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", name))

def input_validated(prompt, validator, error_msg):
    while True:
        val = input(prompt).strip()
        if validator(val):
            return val
        print(error_msg)

def valid_swap(size_str):
    return bool(re.match(r"^\d+$", size_str)) and int(size_str) > 0

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

def choose_disk():
    disks = list_disks()
    if not disks:
        print("No disks found. Aborting.")
        sys.exit(1)
    print("Available disks:")
    for i, (name, size) in enumerate(disks, start=1):
        print(f"  {i}. /dev/{name} ({size} GB)")
    while True:
        choice = input("Select disk number: ").strip()
        if choice.isdigit() and 1 <= int(choice) <= len(disks):
            sel = disks[int(choice)-1][0]
            if os.path.exists(f"/dev/{sel}"):
                return sel
            else:
                print("Device not present, choose another.")
        else:
            print("Invalid command, try again.")

def input_password(prompt):
    sys.stdout.write(prompt + " ")
    sys.stdout.flush()
    password = ""
    fd = sys.stdin.fileno()
    old_settings = termios.tcgetattr(fd)
    try:
        tty.setraw(fd)
        while True:
            ch = sys.stdin.read(1)
            if ch in ('\n', '\r'):
                sys.stdout.write("\n")
                break
            elif ch == '\x7f':
                if password:
                    password = password[:-1]
                    sys.stdout.write('\b \b')
                    sys.stdout.flush()
            else:
                password += ch
                sys.stdout.write('*')
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd, termios.TCSADRAIN, old_settings)
    return password

def partition_paths_for(disk_path):
    if "nvme" in disk_path or "mmcblk" in disk_path:
        return f"{disk_path}p1", f"{disk_path}p2", f"{disk_path}p3"
    return f"{disk_path}1", f"{disk_path}2", f"{disk_path}3"

def ensure_network():
    rc = subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1", shell=True, executable="/bin/bash")
    if rc == 0:
        return True
    if shutil_exists("dhcpcd"):
        run("dhcpcd --nobackground >/dev/null 2>&1 &", ignore_error=True)
        time.sleep(3)
        rc2 = subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1", shell=True, executable="/bin/bash")
        return rc2 == 0
    return False

def shutil_exists(cmd):
    try:
        return bool(subprocess.check_output(f"command -v {cmd} || true", shell=True, text=True).strip())
    except Exception:
        return False

def partprobe_and_settle(disk_path):
    run(f"partprobe {disk_path}", ignore_error=True)
    run("udevadm settle --exit-if-exists=/dev/null", ignore_error=True)
    time.sleep(1)

if os.geteuid() != 0:
    print("Run as root in the Arch live environment.")
    sys.exit(1)

lang = None
while True:
    print("Select language: 1 = English, 2 = Español")
    choice = input("> ").strip()
    if choice == "1":
        lang = "en"
        break
    elif choice == "2":
        lang = "es"
        break
    else:
        print("Invalid command, try again")

def L(msg_en, msg_es):
    return msg_en if lang == "en" else msg_es

hostname = input_validated(L("Enter hostname:","Ingrese el nombre del equipo:"), valid_name, L("Invalid hostname.","Nombre inválido."))
username = input_validated(L("Enter username:","Ingrese nombre de usuario:"), valid_name, L("Invalid username.","Usuario inválido."))
root_pass = input_password(L("Enter root password:","Ingrese contraseña de root:"))
user_pass = input_password(L("Enter user password:","Ingrese contraseña de usuario:"))
swap_size = input_validated(L("Enter swap size in GB (example 8):","Ingrese tamaño de swap en GB (ej 8):"), valid_swap, L("Invalid swap size.","Tamaño de swap inválido."))
desktop_choice = None
while True:
    c = input(L("Choose desktop: 1 = KDE Plasma, 2 = Cinnamon, 0 = None: ","Seleccione escritorio: 1 = KDE Plasma, 2 = Cinnamon, 0 = Ninguno: ")).strip()
    if c in ("0","1","2"):
        desktop_choice = c
        break
    print(L("Invalid command, try again.","Comando inválido, intente de nuevo."))

disk = choose_disk()
disk_path = f"/dev/{disk}"
if not os.path.exists(disk_path):
    print(L("Selected disk not found. Aborting.","Disco seleccionado no encontrado. Abortando."))
    sys.exit(1)
log(L("Selected disk","Disco seleccionado") + f": {disk_path}")

parts_out = subprocess.check_output(f"lsblk -n -o NAME {disk_path}", shell=True, text=True).splitlines()[1:]
if parts_out:
    print(f"{L('Partitions detected on','Particiones detectadas en')} {disk_path}: {', '.join(p.strip() for p in parts_out)}")
    if confirm(L("Do you want to erase all existing partitions?","Desea borrar todas las particiones existentes?")):
        run(f"sgdisk -Z {disk_path}")
        partprobe_and_settle(disk_path)
    else:
        print(L("Cannot continue with existing partitions. Aborting.","No se puede continuar con particiones existentes. Abortando."))
        sys.exit(0)

log(L("Creating partitions...","Creando particiones..."))
rc = run(f"sgdisk -n1:0:+1G -t1:ef00 {disk_path}", ignore_error=True, capture=True)
rc |= run(f"sgdisk -n2:0:+{swap_size}G -t2:8200 {disk_path}", ignore_error=True, capture=True)
rc |= run(f"sgdisk -n3:0:0 -t3:8300 {disk_path}", ignore_error=True, capture=True)
partprobe_and_settle(disk_path)

p1, p2, p3 = partition_paths_for(disk_path)

if not os.path.exists(p1) or not os.path.exists(p2) or not os.path.exists(p3):
    time.sleep(1)
    partprobe_and_settle(disk_path)
if not os.path.exists(p1) or not os.path.exists(p2) or not os.path.exists(p3):
    log("ERROR: partitions not visible after creation.")
    print(L("Partitions not created or not visible. Aborting.","Particiones no creadas o no visibles. Abortando."))
    sys.exit(1)

log(L("Formatting partitions...","Formateando particiones..."))
run(f"mkfs.fat -F32 {p1}")
run(f"mkswap {p2}")
run(f"swapon {p2}")
run(f"mkfs.ext4 -F {p3}")

log(L("Mounting partitions...","Montando particiones..."))
if os.path.ismount("/mnt"):
    log("/mnt already mounted. Attempting to unmount first.")
    run("umount -R /mnt", ignore_error=True)
run(f"mount {p3} /mnt")
run("mkdir -p /mnt/boot/efi")
run(f"mount {p1} /mnt/boot/efi")

log(L("Checking network...","Comprobando red..."))
if not ensure_network():
    print(L("Network unreachable. Try enabling network (e.g. 'dhcpcd') and rerun.","Red inaccesible. Active la red (ej. 'dhcpcd') y reintente."))
    sys.exit(1)

log(L("Updating keyring and mirrors...","Actualizando keyring y mirrors..."))
run("pacman -Sy --noconfirm archlinux-keyring", ignore_error=True, capture=True)
run("pacman -Syu --noconfirm --needed", ignore_error=True, capture=True)

log(L("Installing base system...","Instalando sistema base..."))
packages = "base linux linux-firmware linux-headers sof-firmware base-devel grub efibootmgr vim nano networkmanager sudo bash-completion"
rc = run(f"pacstrap /mnt {packages}", capture=True, ignore_error=True)
if rc != 0:
    log("ERROR: pacstrap failed. Checking common causes.")
    log("Attempting to refresh mirrors and retry pacstrap once.")
    run("pacman -Sy --noconfirm --needed pacman", ignore_error=True, capture=True)
    run("pacman -S --noconfirm --needed reflector || true", ignore_error=True, capture=True)
    run("reflector --latest 20 --sort rate --save /etc/pacman.d/mirrorlist || true", ignore_error=True, capture=True)
    rc2 = run(f"pacstrap /mnt {packages}", capture=True, ignore_error=True)
    if rc2 != 0:
        log("ERROR: pacstrap retry failed. Aborting.")
        print(L("Failed to install packages to new root. Check network, mirrors and pacman keys.","Fallo al instalar paquetes en la nueva raíz. Revise red, mirrors y llaves de pacman."))
        sys.exit(1)

run("genfstab -U /mnt >> /mnt/etc/fstab")

log(L("Configuring system...","Configurando sistema..."))
with open("/mnt/etc/hostname", "w") as f:
    f.write(hostname + "\n")

cfg_lines = []
cfg_lines.append(f"ln -sf /usr/share/zoneinfo/UTC /etc/localtime")
cfg_lines.append("hwclock --systohc")
cfg_lines.append("sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen || true")
if lang == "es":
    cfg_lines.append("sed -i 's/^#es_ES.UTF-8/es_ES.UTF-8/' /etc/locale.gen || true")
cfg_lines.append("locale-gen")
cfg_lines.append("echo 'LANG=en_US.UTF-8' > /etc/locale.conf")
cfg_lines.append("mkinitcpio -P")
cfg_lines.append(f"useradd -m -G wheel -s /bin/bash {username}")
cfg_lines.append(f"echo '{username}:{user_pass}' | chpasswd")
cfg_lines.append(f"echo 'root:{root_pass}' | chpasswd")
cfg_lines.append("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers || true")
cfg_script = " && ".join(cfg_lines)
run(f'arch-chroot /mnt /bin/bash -c "{cfg_script}"', capture=True, ignore_error=False)

run("arch-chroot /mnt systemctl enable NetworkManager", ignore_error=True)

if desktop_choice == "1":
    log(L("Installing KDE Plasma...","Instalando KDE Plasma..."))
    run('arch-chroot /mnt /bin/bash -c "pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput plasma-meta konsole dolphin ark kate plasma-nm firefox sddm"', capture=True, ignore_error=False)
    run('arch-chroot /mnt /bin/bash -c "systemctl enable sddm"', ignore_error=True)
elif desktop_choice == "2":
    log(L("Installing Cinnamon...","Instalando Cinnamon..."))
    run('arch-chroot /mnt /bin/bash -c "pacman -S --noconfirm xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput cinnamon lightdm lightdm-gtk-greeter alacritty firefox"', capture=True, ignore_error=False)
    run('arch-chroot /mnt /bin/bash -c "systemctl enable lightdm"', ignore_error=True)

gpu = None
while True:
    g = input(L("Select GPU: 1 = NVIDIA, 2 = AMD/Intel, 0 = None:","Seleccione GPU: 1 = NVIDIA, 2 = AMD/Intel, 0 = Ninguna:")).strip()
    if g in ("0","1","2"):
        gpu = g
        break
    print(L("Invalid command, try again.","Comando inválido, intente de nuevo."))

if gpu == "1":
    run('arch-chroot /mnt /bin/bash -c "pacman -S --noconfirm nvidia nvidia-utils nvidia-settings"', capture=True, ignore_error=False)
elif gpu == "2":
    run('arch-chroot /mnt /bin/bash -c "pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver"', capture=True, ignore_error=False)

log(L("Installing GRUB...","Instalando GRUB..."))
if "nvme" in disk_path or "mmcblk" in disk_path:
    run(f'arch-chroot /mnt /bin/bash -c "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB"', capture=True, ignore_error=False)
else:
    run(f'arch-chroot /mnt /bin/bash -c "grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB"', capture=True, ignore_error=False)
run('arch-chroot /mnt /bin/bash -c "grub-mkconfig -o /boot/grub/grub.cfg"', capture=True, ignore_error=False)

log(L("Installation finished.","Instalación finalizada."))
if confirm(L("Reboot now?","Reiniciar ahora?")):
    run("umount -R /mnt", ignore_error=True)
    run("reboot", ignore_error=True)
