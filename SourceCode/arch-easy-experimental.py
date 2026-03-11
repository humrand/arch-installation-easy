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
import base64

LOG_FILE="/mnt/install_log.txt"

def nowtag():
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S")

def write_log(line):
    try:
        with open(LOG_FILE,"a") as f:
            f.write(line+"\n")
    except:
        pass

append_buffer=[]
append_lock=threading.Lock()

def append_buffer_add(s):
    line=f"[{nowtag()}] {s}"
    write_log(line)
    with append_lock:
        append_buffer.append(line)
        if len(append_buffer)>1000:
            del append_buffer[:500]

def run_stream(cmd,on_line=None,cwd=None,ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    p=subprocess.Popen(cmd,shell=True,stdout=subprocess.PIPE,stderr=subprocess.STDOUT,text=True,cwd=cwd,executable="/bin/bash")
    while True:
        line=p.stdout.readline()
        if line=="" and p.poll() is not None:
            break
        if line:
            l=line.rstrip("\n")
            append_buffer_add(l)
            if on_line:
                on_line(l)
    rc=p.wait()
    if rc!=0 and not ignore_error:
        append_buffer_add(f"ERROR: command returned {rc}: {cmd}")
    return rc

def run_simple(cmd,ignore_error=False):
    append_buffer_add(f"Running: {cmd}")
    r=subprocess.call(cmd,shell=True,executable="/bin/bash")
    if r!=0 and not ignore_error:
        append_buffer_add(f"ERROR: command returned {r}: {cmd}")
    return r

def ensure_network():
    rc=subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1",shell=True,executable="/bin/bash")
    if rc==0:
        return True
    if shutil.which("dhcpcd"):
        run_simple("dhcpcd --nobackground >/dev/null 2>&1 &",ignore_error=True)
        time.sleep(3)
        rc2=subprocess.call("ping -c1 -W2 8.8.8.8 >/dev/null 2>&1",shell=True,executable="/bin/bash")
        return rc2==0
    return False

def list_disks():
    try:
        out=subprocess.check_output("lsblk -b -d -o NAME,SIZE | tail -n +2",shell=True,text=True)
    except:
        return []
    disks=[]
    for line in out.splitlines():
        parts=line.split()
        if len(parts)<2:
            continue
        name=parts[0]
        try:
            size_gb=int(parts[1])//(1024**3)
        except:
            size_gb=0
        disks.append((name,size_gb))
    return disks

def partition_paths_for(disk_path):
    if "nvme" in disk_path or "mmcblk" in disk_path:
        return f"{disk_path}p1",f"{disk_path}p2",f"{disk_path}p3"
    return f"{disk_path}1",f"{disk_path}2",f"{disk_path}3"

def prompt_password(prompt):
    sys.stdout.write(prompt+" ")
    sys.stdout.flush()
    fd=sys.stdin.fileno()
    old=termios.tcgetattr(fd)
    passwd=""
    try:
        tty.setraw(fd)
        while True:
            ch=sys.stdin.read(1)
            if ch in ("\n","\r"):
                sys.stdout.write("\n")
                break
            if ch=="\x7f":
                if passwd:
                    passwd=passwd[:-1]
                    sys.stdout.write("\b \b")
                    sys.stdout.flush()
            else:
                passwd+=ch
                sys.stdout.write("*")
                sys.stdout.flush()
    finally:
        termios.tcsetattr(fd,termios.TCSADRAIN,old)
    return passwd

def validate_name(n):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$",n))

def validate_swap(s):
    return bool(re.match(r"^\d+$",s)) and int(s)>0

state={
"lang":"en",
"hostname":"",
"username":"",
"root_pass":"",
"user_pass":"",
"swap":"8",
"disk":None,
"desktop":"None",
"gpu":"None",
"keymap":"us",
"timezone":"UTC",
"locale":"en_US.UTF-8"
}

class InstallerUI:

    def __init__(self,stdscr):
        self.stdscr=stdscr
        self.logs=[]
        self.progress=0.0
        self.lock=threading.Lock()

    def add_log(self,line):
        with self.lock:
            self.logs.append(line)
        append_buffer_add(line)

    def set_progress(self,p):
        self.progress=p

    def run_steps(self):

        disk_device=state["disk"]

        p1,p2,p3=partition_paths_for(f"/dev/{disk_device}")

        run_stream(f"sgdisk -Z /dev/{disk_device}")
        run_stream(f"sgdisk -n1:0:+1G -t1:ef00 /dev/{disk_device}")
        run_stream(f"sgdisk -n2:0:+{state['swap']}G -t2:8200 /dev/{disk_device}")
        run_stream(f"sgdisk -n3:0:0 -t3:8300 /dev/{disk_device}")

        run_stream(f"mkfs.fat -F32 {p1}")
        run_stream(f"mkswap {p2}")
        run_stream(f"swapon {p2}")
        run_stream(f"mkfs.ext4 -F {p3}")

        run_stream(f"mount {p3} /mnt")
        run_stream("mkdir -p /mnt/boot/efi")
        run_stream(f"mount {p1} /mnt/boot/efi")

        pkgs="base linux linux-firmware linux-headers sof-firmware base-devel grub efibootmgr vim nano networkmanager sudo bash-completion"

        run_stream(f"pacstrap /mnt {pkgs}")
        run_stream("genfstab -U /mnt >> /mnt/etc/fstab")

        up=f"{state['username']}:{state['user_pass']}"
        rp=f"root:{state['root_pass']}"

        up_b64=base64.b64encode(up.encode()).decode()
        rp_b64=base64.b64encode(rp.encode()).decode()

        setup=f"""
set -e
echo '{state['hostname']}' > /etc/hostname
ln -sf /usr/share/zoneinfo/{state['timezone']} /etc/localtime
hwclock --systohc
sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen || true
locale-gen
echo 'LANG={state['locale']}' > /etc/locale.conf
useradd -m -G wheel -s /bin/bash {state['username']} || true
echo '{rp_b64}' | base64 -d | chpasswd
echo '{up_b64}' | base64 -d | chpasswd
sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers || true
systemctl enable NetworkManager || true
"""

        with open("/mnt/root/setup_user.sh","w") as f:
            f.write(setup)

        os.chmod("/mnt/root/setup_user.sh",0o755)

        run_stream("arch-chroot /mnt /bin/bash /root/setup_user.sh")

        run_stream('arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB')
        run_stream('arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg')

        self.add_log("Installation complete")

def fallback_cli():

    state["hostname"]=input("Hostname: ")
    state["username"]=input("Username: ")

    state["root_pass"]=prompt_password("Root password:")
    state["user_pass"]=prompt_password("User password:")

    disks=list_disks()

    for i,(name,size) in enumerate(disks):
        print(i+1,f"/dev/{name}",size,"GB")

    d=int(input("Disk number: "))
    state["disk"]=disks[d-1][0]

    ui=InstallerUI(None)
    ui.run_steps()

if __name__=="__main__":

    if os.geteuid()!=0:
        print("Run as root")
        sys.exit(1)

    fallback_cli()
