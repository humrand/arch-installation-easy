# Arch Linux Auto Installer (C Version)

A fully automated Arch Linux installer written in C, designed to run directly from the Arch ISO and guide the user through the installation using an interactive terminal interface.

This project started as a Python-based installer and has been completely rewritten in C for better performance, control, and reliability.

---


## Usage

Run from the Arch ISO:

```bash
./install
```

---

## Permissions

If the binary does not execute:

```bash
chmod +x install
./install
```

---

## Logs

If something fails, check:

```bash
cat /tmp/arch_install.log
```

---




## Migration from Python to C

The installer was originally implemented in Python, but has now been fully migrated to C

Key differences:

* Full rewrite in C (no Python dependency anymore)
* Faster execution and lower resource usage
* Better process control (fork, exec, pipes, real-time output parsing)
* Improved error handling and logging
* Thread-safe logging system using mutex
* More robust dialog integration (custom wrapper instead of subprocess calls)
* Cleaner state management using structs instead of dynamic dictionaries  

---

## Features

* Automatic disk detection (largest disk)
* Optional manual disk selection
* GPT partitioning:

  * 1GB EFI
  * Configurable swap
  * Root partition
* Filesystem support:

  * EXT4
  * BTRFS
  * XFS
  * ZFS (experimental)
* Bootloaders:

  * GRUB
  * systemd-boot
  * Limine
* GPU detection:

  * NVIDIA
  * AMD
  * Intel
  * Hybrid setups
* Desktop environments:

  * KDE Plasma
  * GNOME
  * Cinnamon
  * XFCE
  * MATE
  * LXQt
  * Sway
  * Hyprland
* Multi-desktop selection support
* Kernel selection (single or multiple)
* Network setup:

  * Ethernet (auto)
  * WiFi via `iwctl`
* Locale, timezone, and keyboard configuration
* User creation with sudo (wheel)
* NetworkManager enabled by default
* Optional components:

  * Flatpak
  * Yay (AUR helper)
* Full installation logging:

  ```
  /tmp/arch_install.log
  ```

---

## Requirements

* Arch Linux ISO
* Internet connection (Ethernet or WiFi)

---


## Notes

* This installer will modify disks. Use it carefully.
* UEFI and BIOS systems are supported
* ZFS support may depend on kernel compatibility
* WiFi requires a compatible adapter (`iwctl`)
