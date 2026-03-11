import subprocess
import sys
import os
import re
import threading
from datetime import datetime


def _bootstrap_tk():
    try:
        import tkinter
        tkinter.Tk().destroy()
        return
    except Exception:
        pass

    if os.environ.get("_ARCH_TK_INSTALLED") == "1":
        print("ERROR: tk was installed but still not working. Try: pacman -S tk")
        sys.exit(1)

    print("Tkinter not available. Installing tk...")
    result = subprocess.run(["pacman", "-Sy", "--noconfirm", "tk"])
    if result.returncode != 0:
        print("ERROR: Failed to install tk. Check your internet connection.")
        sys.exit(1)

    print("Done. Restarting...")
    env = os.environ.copy()
    env["_ARCH_TK_INSTALLED"] = "1"
    os.execve(sys.executable, [sys.executable] + sys.argv, env)


_bootstrap_tk()

import tkinter as tk
from tkinter import ttk, scrolledtext, messagebox

LOG_FILE = "/mnt/install_log.txt"
lang = "en"


def L(en, es):
    return en if lang == "en" else es


def log(msg, widget=None):
    timestamp = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
    line = f"[{timestamp}] {msg}\n"
    print(line, end="")
    try:
        with open(LOG_FILE, "a") as f:
            f.write(line)
    except Exception:
        pass
    if widget:
        widget.configure(state="normal")
        widget.insert(tk.END, line)
        widget.see(tk.END)
        widget.configure(state="disabled")


def run(cmd, log_widget=None, ignore_error=False):
    log(f"Running: {cmd}", log_widget)
    result = subprocess.run(cmd, shell=True, capture_output=True, text=True)
    if result.stdout:
        log(result.stdout.strip(), log_widget)
    if result.returncode != 0:
        log(f"ERROR: {result.stderr.strip()}", log_widget)
        if not ignore_error:
            return False
    return True


def chroot(cmd, log_widget=None):
    run(f'arch-chroot /mnt /bin/bash -c "{cmd}"', log_widget, ignore_error=True)


def list_disks():
    try:
        output = subprocess.check_output(
            "lsblk -b -d -o NAME,SIZE | tail -n +2", shell=True
        ).decode()
        disks = []
        for line in output.splitlines():
            parts = line.split()
            if len(parts) < 2:
                continue
            try:
                size_gb = int(parts[1]) // (1024 ** 3)
                disks.append((parts[0], size_gb))
            except ValueError:
                continue
        return disks
    except Exception:
        return []


def partition_suffix(disk, n):
    if "nvme" in disk or "mmcblk" in disk:
        return f"/dev/{disk}p{n}"
    return f"/dev/{disk}{n}"


def valid_name(name):
    return bool(re.match(r"^[a-zA-Z0-9_-]{1,32}$", name))


def valid_swap(s):
    return bool(re.match(r"^\d+$", s)) and int(s) > 0


COLORS = {
    "bg":       "#0d1117",
    "panel":    "#161b22",
    "border":   "#21262d",
    "accent":   "#58a6ff",
    "accent2":  "#3fb950",
    "warn":     "#d29922",
    "danger":   "#f85149",
    "text":     "#e6edf3",
    "muted":    "#8b949e",
    "input_bg": "#0d1117",
    "sel":      "#1f6feb",
}

FONT_TITLE  = ("JetBrains Mono", 22, "bold")
FONT_LABEL  = ("JetBrains Mono", 10)
FONT_SMALL  = ("JetBrains Mono", 9)
FONT_INPUT  = ("JetBrains Mono", 11)
FONT_LOG    = ("JetBrains Mono", 9)
FONT_BTN    = ("JetBrains Mono", 10, "bold")
FONT_STEP   = ("JetBrains Mono", 11, "bold")


class ArchInstaller(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Arch Linux Installer")
        self.configure(bg=COLORS["bg"])
        self.resizable(False, False)
        self.geometry("860x640")

        self.lang     = tk.StringVar(value="en")
        self.hostname = tk.StringVar()
        self.username = tk.StringVar()
        self.root_pass = tk.StringVar()
        self.user_pass = tk.StringVar()
        self.swap_size = tk.StringVar(value="8")
        self.desktop   = tk.StringVar(value="0")
        self.gpu       = tk.StringVar(value="0")
        self.disk      = tk.StringVar()

        self.disks = list_disks()
        self.step  = 0
        self.steps = [
            self._page_lang,
            self._page_system,
            self._page_passwords,
            self._page_disk,
            self._page_desktop,
            self._page_gpu,
            self._page_confirm,
            self._page_install,
        ]

        self._build_shell()
        self._show_step()

    def _build_shell(self):
        header = tk.Frame(self, bg=COLORS["panel"], height=56)
        header.pack(fill="x")
        header.pack_propagate(False)

        dot_frame = tk.Frame(header, bg=COLORS["panel"])
        dot_frame.pack(side="left", padx=16, pady=18)
        for color in ("#f85149", "#d29922", "#3fb950"):
            c = tk.Canvas(dot_frame, width=12, height=12,
                          bg=COLORS["panel"], highlightthickness=0)
            c.pack(side="left", padx=3)
            c.create_oval(1, 1, 11, 11, fill=color, outline="")

        tk.Label(header, text="arch-installer  ~  v2.0",
                 font=FONT_SMALL, bg=COLORS["panel"],
                 fg=COLORS["muted"]).pack(side="left", padx=8)

        self.progress_bar = tk.Canvas(self, height=3, bg=COLORS["border"],
                                      highlightthickness=0)
        self.progress_bar.pack(fill="x")

        self.content = tk.Frame(self, bg=COLORS["bg"])
        self.content.pack(fill="both", expand=True, padx=40, pady=30)

        footer = tk.Frame(self, bg=COLORS["panel"], height=56)
        footer.pack(fill="x", side="bottom")
        footer.pack_propagate(False)

        self.btn_back = tk.Button(
            footer, text="← Back", font=FONT_BTN,
            bg=COLORS["panel"], fg=COLORS["muted"],
            activebackground=COLORS["border"], activeforeground=COLORS["text"],
            relief="flat", cursor="hand2", bd=0, padx=20,
            command=self._go_back
        )
        self.btn_back.pack(side="left", padx=20, pady=12)

        self.btn_next = tk.Button(
            footer, text="Next →", font=FONT_BTN,
            bg=COLORS["accent"], fg=COLORS["bg"],
            activebackground="#79beff", activeforeground=COLORS["bg"],
            relief="flat", cursor="hand2", bd=0, padx=24,
            command=self._go_next
        )
        self.btn_next.pack(side="right", padx=20, pady=12)

    def _update_progress(self):
        total = len(self.steps) - 1
        pct   = self.step / total if total else 0
        w     = self.winfo_width() or 860
        self.progress_bar.delete("all")
        self.progress_bar.create_rectangle(
            0, 0, w * pct, 3, fill=COLORS["accent"], outline=""
        )

    def _clear(self):
        for w in self.content.winfo_children():
            w.destroy()

    def _title(self, text, sub=None):
        tk.Label(self.content, text=text, font=FONT_TITLE,
                 bg=COLORS["bg"], fg=COLORS["text"],
                 anchor="w").pack(anchor="w", pady=(0, 4))
        if sub:
            tk.Label(self.content, text=sub, font=FONT_SMALL,
                     bg=COLORS["bg"], fg=COLORS["muted"],
                     anchor="w").pack(anchor="w", pady=(0, 22))

    def _field(self, parent, label, var, show=None, width=34):
        f = tk.Frame(parent, bg=COLORS["bg"])
        f.pack(anchor="w", pady=6)
        tk.Label(f, text=label, font=FONT_LABEL,
                 bg=COLORS["bg"], fg=COLORS["muted"]).pack(anchor="w")
        entry = tk.Entry(f, textvariable=var, font=FONT_INPUT, width=width,
                         bg=COLORS["input_bg"], fg=COLORS["text"],
                         insertbackground=COLORS["accent"],
                         relief="flat", bd=0,
                         highlightthickness=1,
                         highlightbackground=COLORS["border"],
                         highlightcolor=COLORS["accent"],
                         show=show or "")
        entry.pack(ipady=7, fill="x")
        return entry

    def _radio_group(self, parent, var, options):
        for val, label, sub in options:
            row = tk.Frame(parent, bg=COLORS["panel"],
                           highlightthickness=1,
                           highlightbackground=COLORS["border"])
            row.pack(fill="x", pady=4)

            rb = tk.Radiobutton(
                row, variable=var, value=val,
                bg=COLORS["panel"], activebackground=COLORS["panel"],
                selectcolor=COLORS["accent"], relief="flat",
                cursor="hand2"
            )
            rb.pack(side="left", padx=12, pady=10)

            info = tk.Frame(row, bg=COLORS["panel"])
            info.pack(side="left", pady=10)
            tk.Label(info, text=label, font=FONT_STEP,
                     bg=COLORS["panel"], fg=COLORS["text"]).pack(anchor="w")
            if sub:
                tk.Label(info, text=sub, font=FONT_SMALL,
                         bg=COLORS["panel"], fg=COLORS["muted"]).pack(anchor="w")

    def _show_step(self):
        self._clear()
        self._update_progress()
        self.steps[self.step]()
        self.btn_back.configure(state="normal" if self.step > 0 else "disabled")
        is_install = self.step == len(self.steps) - 1
        self.btn_next.configure(
            text="Install" if self.step == len(self.steps) - 2 else
                 ("" if is_install else "Next →"),
            state="disabled" if is_install else "normal"
        )

    def _go_next(self):
        if not self._validate():
            return
        if self.step < len(self.steps) - 1:
            self.step += 1
            self._show_step()

    def _go_back(self):
        if self.step > 0:
            self.step -= 1
            self._show_step()

    def _validate(self):
        s = self.step
        if s == 1:
            if not valid_name(self.hostname.get()):
                messagebox.showerror("Error", L("Invalid hostname.", "Nombre de equipo inválido."))
                return False
            if not valid_name(self.username.get()):
                messagebox.showerror("Error", L("Invalid username.", "Nombre de usuario inválido."))
                return False
        if s == 2:
            if len(self.root_pass.get()) < 1:
                messagebox.showerror("Error", L("Root password required.", "Contraseña de root requerida."))
                return False
            if len(self.user_pass.get()) < 1:
                messagebox.showerror("Error", L("User password required.", "Contraseña de usuario requerida."))
                return False
        if s == 3:
            if not valid_swap(self.swap_size.get()):
                messagebox.showerror("Error", L("Invalid swap size.", "Tamaño de swap inválido."))
                return False
            if not self.disk.get():
                messagebox.showerror("Error", L("Select a disk.", "Seleccione un disco."))
                return False
        return True

    def _page_lang(self):
        self._title("Arch Linux Installer", "Select your language / Seleccione su idioma")
        self._radio_group(self.content, self.lang, [
            ("en", "English", "English interface"),
            ("es", "Español", "Interfaz en español"),
        ])
        def on_change(*_):
            global lang
            lang = self.lang.get()
        self.lang.trace_add("write", on_change)

    def _page_system(self):
        self._title(L("System Identity", "Identidad del sistema"),
                    L("Set hostname and user account", "Configura el equipo y cuenta de usuario"))
        self._field(self.content, L("Hostname", "Nombre del equipo"), self.hostname)
        self._field(self.content, L("Username", "Nombre de usuario"), self.username)

    def _page_passwords(self):
        self._title(L("Passwords", "Contraseñas"),
                    L("Set secure passwords for root and your user", "Define contraseñas seguras"))
        self._field(self.content, L("Root password", "Contraseña de root"), self.root_pass, show="•")
        self._field(self.content, L("User password", "Contraseña de usuario"), self.user_pass, show="•")

    def _page_disk(self):
        self._title(L("Disk & Swap", "Disco y Swap"),
                    L("⚠  This will erase the selected disk", "⚠  Esto borrará el disco seleccionado"))

        self._field(self.content, L("Swap size (GB)", "Tamaño de swap (GB)"), self.swap_size, width=8)

        tk.Label(self.content, text=L("Select disk", "Seleccione disco"),
                 font=FONT_LABEL, bg=COLORS["bg"], fg=COLORS["muted"]).pack(anchor="w", pady=(12, 4))

        if not self.disks:
            tk.Label(self.content,
                     text=L("No disks detected.", "No se detectaron discos."),
                     font=FONT_LABEL, bg=COLORS["bg"], fg=COLORS["danger"]).pack(anchor="w")
            return

        disk_var_options = []
        for name, size in self.disks:
            disk_var_options.append((name, f"/dev/{name}", f"{size} GB"))
        if not self.disk.get() and self.disks:
            self.disk.set(self.disks[0][0])

        self._radio_group(self.content, self.disk, disk_var_options)

    def _page_desktop(self):
        self._title(L("Desktop Environment", "Entorno de escritorio"),
                    L("Choose your graphical environment", "Elige tu entorno gráfico"))
        self._radio_group(self.content, self.desktop, [
            ("1", "KDE Plasma", L("Modern, feature-rich desktop", "Escritorio moderno y completo")),
            ("2", "Cinnamon",   L("Traditional, elegant desktop", "Escritorio tradicional y elegante")),
            ("0", L("None", "Ninguno"), L("CLI only", "Solo terminal")),
        ])

    def _page_gpu(self):
        self._title(L("GPU Drivers", "Drivers de GPU"),
                    L("Select your graphics card vendor", "Selecciona tu fabricante de GPU"))
        self._radio_group(self.content, self.gpu, [
            ("1", "NVIDIA",    "nvidia, nvidia-utils, nvidia-settings"),
            ("2", "AMD / Intel", "mesa, vulkan-radeon, libva-mesa-driver"),
            ("0", L("None / VM", "Ninguno / VM"), L("Skip GPU drivers", "Omitir drivers de GPU")),
        ])

    def _page_confirm(self):
        self._title(L("Review & Install", "Revisar e Instalar"),
                    L("Confirm your configuration before proceeding", "Confirma tu configuración"))

        desktop_names = {"0": L("None","Ninguno"), "1": "KDE Plasma", "2": "Cinnamon"}
        gpu_names     = {"0": L("None","Ninguno"), "1": "NVIDIA", "2": "AMD/Intel"}

        rows = [
            (L("Language","Idioma"),         L("English","Español") if self.lang.get()=="es" else "English"),
            (L("Hostname","Equipo"),          self.hostname.get()),
            (L("Username","Usuario"),         self.username.get()),
            (L("Disk","Disco"),               f"/dev/{self.disk.get()}"),
            (L("Swap","Swap"),                f"{self.swap_size.get()} GB"),
            (L("Desktop","Escritorio"),       desktop_names[self.desktop.get()]),
            (L("GPU","GPU"),                  gpu_names[self.gpu.get()]),
        ]

        for label, value in rows:
            row = tk.Frame(self.content, bg=COLORS["panel"])
            row.pack(fill="x", pady=2)
            tk.Label(row, text=label, font=FONT_LABEL, width=16,
                     bg=COLORS["panel"], fg=COLORS["muted"], anchor="w").pack(side="left", padx=12, pady=8)
            tk.Label(row, text=value, font=FONT_STEP,
                     bg=COLORS["panel"], fg=COLORS["accent"], anchor="w").pack(side="left")

        warn = tk.Label(self.content,
                        text=L("⚠  ALL DATA on the selected disk will be destroyed.",
                               "⚠  TODOS LOS DATOS del disco seleccionado serán destruidos."),
                        font=FONT_LABEL, bg=COLORS["bg"], fg=COLORS["danger"])
        warn.pack(anchor="w", pady=(18, 0))

    def _page_install(self):
        self._title(L("Installing...", "Instalando..."),
                    L("Do not close this window.", "No cierres esta ventana."))

        self.log_box = scrolledtext.ScrolledText(
            self.content, font=FONT_LOG, bg="#010409", fg=COLORS["accent2"],
            insertbackground=COLORS["accent2"], relief="flat",
            state="disabled", wrap="word",
            highlightthickness=1, highlightbackground=COLORS["border"]
        )
        self.log_box.pack(fill="both", expand=True)

        self.btn_next.configure(state="disabled", text="")
        self.btn_back.configure(state="disabled")

        thread = threading.Thread(target=self._run_install, daemon=True)
        thread.start()

    def _run_install(self):
        lw   = self.log_box
        disk = self.disk.get()
        dp   = f"/dev/{disk}"
        p1   = partition_suffix(disk, 1)
        p2   = partition_suffix(disk, 2)
        p3   = partition_suffix(disk, 3)

        log(L("Wiping disk...", "Borrando disco..."), lw)
        run(f"sgdisk -Z {dp}", lw)

        log(L("Creating partitions...", "Creando particiones..."), lw)
        run(f"sgdisk -n1:0:+1G   -t1:ef00 {dp}", lw)
        run(f"sgdisk -n2:0:+{self.swap_size.get()}G -t2:8200 {dp}", lw)
        run(f"sgdisk -n3:0:0     -t3:8300 {dp}", lw)

        log(L("Formatting...", "Formateando..."), lw)
        run(f"mkfs.fat -F32 {p1}", lw)
        run(f"mkswap {p2}", lw)
        run(f"swapon {p2}", lw)
        run(f"mkfs.ext4 -F {p3}", lw)

        log(L("Mounting...", "Montando..."), lw)
        run(f"mount {p3} /mnt", lw)
        run("mkdir -p /mnt/boot/efi", lw)
        run(f"mount {p1} /mnt/boot/efi", lw)

        log(L("Installing base system...", "Instalando sistema base..."), lw)
        pkgs = (
            "base linux linux-firmware linux-headers sof-firmware "
            "base-devel grub efibootmgr vim nano networkmanager "
            "sudo bash-completion"
        )
        run(f"pacstrap /mnt {pkgs}", lw)
        run("genfstab -U /mnt >> /mnt/etc/fstab", lw)

        log(L("Configuring system...", "Configurando sistema..."), lw)
        with open("/mnt/etc/hostname", "w") as f:
            f.write(self.hostname.get() + "\n")

        chroot("ln -sf /usr/share/zoneinfo/UTC /etc/localtime", lw)
        chroot("hwclock --systohc", lw)
        chroot("sed -i 's/^#en_US.UTF-8/en_US.UTF-8/' /etc/locale.gen", lw)
        chroot("locale-gen", lw)
        chroot("echo 'LANG=en_US.UTF-8' > /etc/locale.conf", lw)
        chroot("mkinitcpio -P", lw)

        chroot(f"echo 'root:{self.root_pass.get()}' | chpasswd", lw)
        chroot(f"useradd -m -G wheel -s /bin/bash {self.username.get()}", lw)
        chroot(f"echo '{self.username.get()}:{self.user_pass.get()}' | chpasswd", lw)
        chroot("sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers", lw)
        chroot("systemctl enable NetworkManager", lw)

        if self.desktop.get() == "1":
            log(L("Installing KDE Plasma...", "Instalando KDE Plasma..."), lw)
            chroot(
                "pacman -S --noconfirm "
                "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput "
                "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm", lw
            )
            chroot("systemctl enable sddm", lw)
        elif self.desktop.get() == "2":
            log(L("Installing Cinnamon...", "Instalando Cinnamon..."), lw)
            chroot(
                "pacman -S --noconfirm "
                "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput "
                "cinnamon lightdm lightdm-gtk-greeter alacritty firefox", lw
            )
            chroot("systemctl enable lightdm", lw)

        if self.gpu.get() == "1":
            log(L("Installing NVIDIA drivers...", "Instalando drivers NVIDIA..."), lw)
            chroot("pacman -S --noconfirm nvidia nvidia-utils nvidia-settings", lw)
        elif self.gpu.get() == "2":
            log(L("Installing AMD/Intel drivers...", "Instalando drivers AMD/Intel..."), lw)
            chroot("pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver", lw)

        log(L("Installing GRUB...", "Instalando GRUB..."), lw)
        run("arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB", lw)
        run("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg", lw)

        log(L("✔ Installation complete!", "✔ ¡Instalación completada!"), lw)

        self.btn_next.configure(
            state="normal",
            text=L("Reboot", "Reiniciar"),
            bg=COLORS["accent2"],
            command=self._reboot
        )

    def _reboot(self):
        run("umount -R /mnt")
        run("reboot")


if __name__ == "__main__":
    app = ArchInstaller()
    app.mainloop()
