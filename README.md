# Arch Linux Auto Installer

A fully automated Arch Linux installer written in Python, designed to run directly from an Arch ISO and guide the user through the installation interactively.  

This installer simplifies Arch installation while keeping full control over the system. It now includes **GPU selection, multiple desktop environments, configurable swap, proper password input handling, automatic language selection.

## How to run the instaler

```bash
python install.py
```


# ISO image

The ISO image is available in the releases.

## Features

- Automatic detection of the largest disk, with optional manual selection  
- Checks for existing partitions and optionally wipes them  
- Automatic GPT partitioning:
  - 1GB EFI partition
  - Configurable swap partition
  - Remaining space for root
- Filesystem setup:
  - FAT32 for EFI
  - EXT4 for root
  - BTRFS for roor if sleected
  - Swap activation
- Automatic mounting of partitions
- Hostname configuration
- Root password setup
- User creation with sudo (wheel group)
- NetworkManager enabled by default
- GPU driver selection
  - NVIDIA
  - AMD/Intel (Mesa)
  - None
- Optional desktop environments:
  - KDE Plasma
  - Cinnamon
  - Gnome
  - Lxqt
  - Xfce
  - mate
- Desktop installation includes:
  - Necessary Xorg packages
  - Firefox and basic apps (Alacritty, Konsole, Dolphin, Kate, and Ark)
  - Display manager setup (sddm for KDE, lightdm for Cinnamon)
- Final prompt to reboot

## Requirements

- Internet connection  

