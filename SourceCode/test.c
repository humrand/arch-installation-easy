
#define _GNU_SOURCE
#include <ctype.h>
#include <errno.h>
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define VERSION   "V1.1.5-beta"
#define LOG_FILE  "/tmp/arch_install.log"
#define TITLE     "Arch Linux Installer"

typedef struct {
    char lang[8];
    char locale[32];
    char hostname[64];
    char username[64];
    char root_pass[128];
    char user_pass[128];
    char swap[16];
    char disk[64];
    char desktop[32];
    char gpu[32];
    char keymap[32];
    char timezone[64];
    char filesystem[16];
    char kernel[32];
    int mirrors;
    int quick;
    int yay;
    int snapper;
    char bootloader[32];
    int flatpak;
} state_t;

static state_t S = {
    .lang = "en",
    .locale = "en_US.UTF-8",
    .hostname = "",
    .username = "",
    .root_pass = "",
    .user_pass = "",
    .swap = "8",
    .disk = "",
    .desktop = "None",
    .gpu = "None",
    .keymap = "us",
    .timezone = "UTC",
    .filesystem = "ext4",
    .kernel = "linux",
    .mirrors = 1,
    .quick = 0,
    .yay = 0,
    .snapper = 0,
    .bootloader = "grub",
    .flatpak = 0,
};

typedef struct { const char *tag; const char *desc; } item_t;

static const item_t desktop_names[] = {
    {"KDE Plasma", "KDE Plasma desktop"},
    {"GNOME",      "GNOME desktop"},
    {"Cinnamon",   "Cinnamon desktop"},
    {"XFCE",       "XFCE desktop"},
    {"MATE",       "MATE desktop"},
    {"LXQt",       "LXQt desktop"},
    {"Hyprland",   "Hyprland Wayland compositor"},
    {"Sway",       "Sway Wayland compositor"},
    {"None",       "No desktop environment"},
    {NULL, NULL}
};

static void log_line(const char *line) {
    FILE *f = fopen(LOG_FILE, "a");
    if (!f) return;
    time_t t = time(NULL);
    struct tm tmv;
    localtime_r(&t, &tmv);
    char buf[64];
    strftime(buf, sizeof buf, "%Y-%m-%d %H:%M:%S", &tmv);
    fprintf(f, "[%s] %s\n", buf, line);
    fclose(f);
}

static int file_exists(const char *path) {
    struct stat st;
    return stat(path, &st) == 0;
}

static int is_uefi(void) {
    return file_exists("/sys/firmware/efi");
}

static int is_root(void) {
    return geteuid() == 0;
}

static void trim_newline(char *s) {
    if (!s) return;
    size_t n = strlen(s);
    while (n && (s[n-1] == '\n' || s[n-1] == '\r')) s[--n] = '\0';
}

static int run_cmd(const char *cmd) {
    log_line(cmd);
    int rc = system(cmd);
    if (rc == -1) {
        log_line("system() failed");
        return 1;
    }
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return 1;
}

static int run_cmd_ignore(const char *cmd) {
    (void)run_cmd(cmd);
    return 0;
}

static int capture_cmd(const char *cmd, char *out, size_t out_sz) {
    if (!out || out_sz == 0) return -1;
    out[0] = '\0';
    FILE *p = popen(cmd, "r");
    if (!p) return -1;
    size_t used = 0;
    while (fgets(out + used, (int)(out_sz - used), p)) {
        used = strlen(out);
        if (used + 2 >= out_sz) break;
    }
    int rc = pclose(p);
    if (rc == -1) return -1;
    if (WIFEXITED(rc)) return WEXITSTATUS(rc);
    return 1;
}

static void sh_quote(const char *src, char *dst, size_t dst_sz) {
    // Single-quote shell escaping: ' -> '\'' 
    size_t d = 0;
    if (!dst_sz) return;
    if (d < dst_sz - 1) dst[d++] = '\'';
    for (const char *p = src; *p && d + 4 < dst_sz; ++p) {
        if (*p == '\'') {
            if (d + 4 >= dst_sz) break;
            memcpy(dst + d, "'\\''", 4);
            d += 4;
        } else {
            dst[d++] = *p;
        }
    }
    if (d < dst_sz - 1) dst[d++] = '\'';
    dst[d] = '\0';
}

static int dlg_capture(char *out, size_t out_sz, const char *fmt, ...) {
    char cmd[8192];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(cmd, sizeof cmd, fmt, ap);
    va_end(ap);

    char full[9000];
    snprintf(full, sizeof full, "dialog --stdout --colors --backtitle '\\Zb\\Z4%s\\Zn  -  %s' %s",
             TITLE, VERSION, cmd);
    return capture_cmd(full, out, out_sz);
}

static void msgbox(const char *title, const char *text) {
    char cmd[8192];
    snprintf(cmd, sizeof cmd,
             "--title '%s' --msgbox '%s' 0 0",
             title, text);
    char out[8];
    dlg_capture(out, sizeof out, "%s", cmd);
}

static int yesno(const char *title, const char *text) {
    char cmd[8192];
    snprintf(cmd, sizeof cmd,
             "--title '%s' --yesno '%s' 0 0",
             title, text);
    char out[8];
    int rc = dlg_capture(out, sizeof out, "%s", cmd);
    return rc == 0;
}

static int inputbox(const char *title, const char *text, const char *init, char *out, size_t out_sz, int password) {
    char cmd[8192];
    if (password) {
        snprintf(cmd, sizeof cmd,
                 "--title '%s' --insecure --passwordbox '%s' 0 60 '%s'",
                 title, text, init ? init : "");
    } else {
        snprintf(cmd, sizeof cmd,
                 "--title '%s' --inputbox '%s' 0 60 '%s'",
                 title, text, init ? init : "");
    }
    return dlg_capture(out, out_sz, "%s", cmd);
}

static int menu(const char *title, const char *text, const item_t *items, const char *default_tag, char *out, size_t out_sz) {
    char cmd[8192];
    size_t off = 0;
    off += snprintf(cmd + off, sizeof cmd - off,
                    "--title '%s' --menu '%s' 20 76 20 ",
                    title, text);
    for (int i = 0; items[i].tag; ++i) {
        off += snprintf(cmd + off, sizeof cmd - off, "'%s' '%s' ", items[i].tag, items[i].desc);
    }
    (void)default_tag;
    return dlg_capture(out, out_sz, "%s", cmd);
}

static int radiolist(const char *title, const char *text, const item_t *items, const char *default_tag, char *out, size_t out_sz) {
    char cmd[8192];
    size_t off = 0;
    off += snprintf(cmd + off, sizeof cmd - off,
                    "--title '%s' --radiolist '%s' 22 76 20 ",
                    title, text);
    for (int i = 0; items[i].tag; ++i) {
        const char *status = (default_tag && strcmp(items[i].tag, default_tag) == 0) ? "on" : "off";
        off += snprintf(cmd + off, sizeof cmd - off, "'%s' '%s' '%s' ",
                        items[i].tag, items[i].desc, status);
    }
    return dlg_capture(out, out_sz, "%s", cmd);
}

static int validate_name(const char *s) {
    if (!s || !*s) return 0;
    size_t n = strlen(s);
    if (n > 32) return 0;
    for (size_t i = 0; i < n; ++i) {
        if (!(isalnum((unsigned char)s[i]) || s[i] == '-' || s[i] == '_')) return 0;
    }
    return 1;
}

static int validate_swap(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (!isdigit((unsigned char)*p)) return 0;
    int v = atoi(s);
    return v >= 1 && v <= 128;
}

static int check_connectivity(void) {
    const char *cmds[] = {
        "curl -sI --max-time 5 https://archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 8.8.8.8 >/dev/null 2>&1",
        NULL
    };
    for (int i = 0; cmds[i]; ++i) {
        if (run_cmd(cmds[i]) == 0) return 1;
    }
    return 0;
}

static int wifi_ifaces(char out[][32], int max) {
    FILE *p = popen("ls /sys/class/net/ 2>/dev/null", "r");
    if (!p) return 0;
    int n = 0;
    char buf[256];
    while (fgets(buf, sizeof buf, p) && n < max) {
        trim_newline(buf);
        if (!strncmp(buf, "wlan", 4) || !strncmp(buf, "wlp", 3) || !strncmp(buf, "wlo", 3)) {
            snprintf(out[n++], 32, "%s", buf);
        }
    }
    pclose(p);
    return n;
}

static int wifi_connect(void) {
    char ifaces[8][32];
    int n = wifi_ifaces(ifaces, 8);
    if (n <= 0) {
        msgbox("WiFi", "No wireless interfaces found.");
        return 0;
    }
    char iface[32];
    snprintf(iface, sizeof iface, "%s", ifaces[0]);

    char cmd[512];
    snprintf(cmd, sizeof cmd, "iwctl station '%s' scan >/dev/null 2>&1", iface);
    run_cmd_ignore(cmd);

    char ssid[128];
    if (inputbox("WiFi", "Enter SSID (or cancel):", "", ssid, sizeof ssid, 0) != 0) return 0;

    char pass[128];
    if (inputbox("WiFi Password", "Enter password (blank for open network):", "", pass, sizeof pass, 1) != 0) return 0;

    if (pass[0]) {
        snprintf(cmd, sizeof cmd, "iwctl --passphrase '%s' station '%s' connect '%s'", pass, iface, ssid);
    } else {
        snprintf(cmd, sizeof cmd, "iwctl station '%s' connect '%s'", iface, ssid);
    }
    run_cmd_ignore(cmd);
    sleep(5);
    return check_connectivity();
}

static int ensure_network(void) {
    if (check_connectivity()) return 1;
    if (wifi_connect()) return 1;
    return 0;
}

static int is_ssd(const char *disk) {
    char path[256];
    char name[64];
    snprintf(name, sizeof name, "%s", disk);
    char *p = name;
    if (!strncmp(p, "nvme", 4) || !strncmp(p, "mmcblk", 6)) {
        char *pp = strstr(p, "p");
        if (pp) *pp = '\0';
    } else {
        size_t n = strlen(p);
        while (n && isdigit((unsigned char)p[n-1])) p[--n] = '\0';
    }
    snprintf(path, sizeof path, "/sys/block/%s/queue/rotational", p);
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    char buf[8] = {0};
    fgets(buf, sizeof buf, f);
    fclose(f);
    return buf[0] == '0';
}

static int suggest_swap_gb(void) {
    FILE *f = fopen("/proc/meminfo", "r");
    if (!f) return 8;
    char line[256];
    while (fgets(line, sizeof line, f)) {
        if (!strncmp(line, "MemTotal:", 9)) {
            long kb = atol(line + 9);
            fclose(f);
            long ram = kb / (1024 * 1024);
            if (ram <= 2) return 4;
            if (ram <= 8) return (int)ram;
            return 8;
        }
    }
    fclose(f);
    return 8;
}

static int list_disks(item_t *items, int max_items) {
    FILE *p = popen("lsblk -b -d -o NAME,SIZE,MODEL | tail -n +2", "r");
    if (!p) return 0;
    char line[512];
    int n = 0;
    while (fgets(line, sizeof line, p) && n < max_items - 1) {
        trim_newline(line);
        char name[64] = {0}, size_s[64] = {0}, model[256] = {0};
        // simple parse: NAME SIZE MODEL...
        char *save = NULL;
        char *tok1 = strtok_r(line, " \t", &save);
        char *tok2 = strtok_r(NULL, " \t", &save);
        char *rest = save ? save : (char*)"";
        if (!tok1 || !tok2) continue;
        snprintf(name, sizeof name, "%s", tok1);
        snprintf(size_s, sizeof size_s, "%s", tok2);
        snprintf(model, sizeof model, "%s", rest && *rest ? rest : "Unknown");
        double gb = (double)atoll(size_s) / (1024.0 * 1024.0 * 1024.0);
        static char descbuf[128][128];
        snprintf(descbuf[n], sizeof descbuf[n], "%.0f GB - %s", gb, model);
        static char tagbuf[128][64];
        snprintf(tagbuf[n], sizeof tagbuf[n], "/dev/%s", name);
        items[n].tag = tagbuf[n];
        items[n].desc = descbuf[n];
        ++n;
    }
    pclose(p);
    items[n].tag = NULL;
    items[n].desc = NULL;
    return n;
}

static void partition_paths(const char *disk, char *p1, char *p2, char *p3) {
    if (strstr(disk, "nvme") || strstr(disk, "mmcblk")) {
        sprintf(p1, "%sp1", disk);
        sprintf(p2, "%sp2", disk);
        sprintf(p3, "%sp3", disk);
    } else {
        sprintf(p1, "%s1", disk);
        sprintf(p2, "%s2", disk);
        sprintf(p3, "%s3", disk);
    }
}

static int detect_cpu_microcode(char *out, size_t out_sz) {
    char buf[4096] = {0};
    if (capture_cmd("lscpu 2>/dev/null", buf, sizeof buf) != 0) return 0;
    if (strstr(buf, "GenuineIntel")) {
        snprintf(out, out_sz, "intel-ucode");
        return 1;
    }
    if (strstr(buf, "AuthenticAMD")) {
        snprintf(out, out_sz, "amd-ucode");
        return 1;
    }
    return 0;
}

static int detect_gpu(void) {
    char buf[4096] = {0};
    if (capture_cmd("lspci 2>/dev/null | grep -iE 'vga|3d|display'", buf, sizeof buf) != 0) return 0;
    for (char *p = buf; *p; ++p) *p = (char)tolower((unsigned char)*p);
    int intel = strstr(buf, "intel") != NULL;
    int nvidia = strstr(buf, "nvidia") != NULL;
    int amd = strstr(buf, "amd") != NULL || strstr(buf, "radeon") != NULL;
    if (intel && nvidia) return 1; // Intel+NVIDIA
    if (intel && amd) return 2;     // Intel+AMD
    if (nvidia) return 3;
    if (amd) return 4;
    if (intel) return 5;
    return 0;
}

static void screen_welcome(void) {
    const char *mode = is_uefi() ? "UEFI" : "BIOS (Legacy)";
    char text[1024];
    snprintf(text, sizeof text,
             "Welcome to the Arch Linux Installer\n\nVersion: %s    Boot mode: %s\n\nWARNING: This installer will ERASE and install Arch Linux to the selected disk.\n\nUse Tab and Arrow keys to navigate.\nPress OK to begin.",
             VERSION, mode);
    msgbox("Welcome", text);
}

static void screen_language(void) {
    const item_t items[] = { {"en", "English"}, {"es", "Espanol"}, {NULL, NULL} };
    char out[64];
    if (menu("Language / Idioma", "Choose installer language:", items, "en", out, sizeof out) == 0) {
        snprintf(S.lang, sizeof S.lang, "%s", out);
    }
}

static void L(char *dst, size_t dst_sz, const char *en, const char *es) {
    snprintf(dst, dst_sz, "%s", strcmp(S.lang, "en") == 0 ? en : es);
}

static int screen_network(void) {
    char text[512];
    L(text, sizeof text,
      "An active internet connection is required for installation.\n\nHow are you connected to the internet?",
      "Se necesita conexion a internet para la instalacion.\n\nComo estas conectado a internet?");
    const item_t items[] = {
        {"wired", "Wired (Ethernet)"},
        {"wifi",  "WiFi"},
        {NULL, NULL}
    };
    while (1) {
        char choice[32] = {0};
        if (menu("Network Connection", text, items, "wired", choice, sizeof choice) != 0) return 0;
        if (!strcmp(choice, "wired")) {
            if (check_connectivity()) return 1;
            msgbox("No connection", "No wired network detected.");
        } else if (!strcmp(choice, "wifi")) {
            if (wifi_connect()) return 1;
            msgbox("WiFi Failed", "Could not connect to WiFi.");
        }
    }
}

static int screen_mode(void) {
    const item_t items[] = {
        {"quick",  "Quick Install - BTRFS + KDE Plasma + linux + yay + snapper"},
        {"custom", "Custom Install - full control"},
        {NULL, NULL}
    };
    char choice[32] = {0};
    if (menu("Install Mode", "Choose install mode:", items, "custom", choice, sizeof choice) != 0) return 0;
    if (!strcmp(choice, "quick")) {
        S.quick = 1;
        snprintf(S.filesystem, sizeof S.filesystem, "btrfs");
        snprintf(S.kernel, sizeof S.kernel, "linux");
        snprintf(S.desktop, sizeof S.desktop, "KDE Plasma");
        S.mirrors = 1;
        snprintf(S.gpu, sizeof S.gpu, "%s",
                 detect_gpu() == 3 ? "NVIDIA" :
                 detect_gpu() == 4 ? "AMD" :
                 detect_gpu() == 5 ? "Intel" : "None");
        S.yay = 1;
        S.snapper = 1;
        snprintf(S.bootloader, sizeof S.bootloader, "grub");
        snprintf(S.swap, sizeof S.swap, "%d", suggest_swap_gb());
        return 1;
    }
    return 0;
}

static int screen_identity(void) {
    char hn[64], un[64];
    for (;;) {
        if (inputbox("Hostname", "Enter hostname:", S.hostname, hn, sizeof hn, 0) != 0) return 0;
        if (!validate_name(hn)) {
            msgbox("Invalid hostname", "Only letters, digits, hyphens and underscores. Max 32 chars.");
            continue;
        }
        if (inputbox("Username", "Enter username:", S.username, un, sizeof un, 0) != 0) return 0;
        if (!validate_name(un)) {
            msgbox("Invalid username", "Only letters, digits, hyphens and underscores. Max 32 chars.");
            continue;
        }
        snprintf(S.hostname, sizeof S.hostname, "%s", hn);
        snprintf(S.username, sizeof S.username, "%s", un);
        return 1;
    }
}

static int screen_passwords(void) {
    char rp1[128], rp2[128], up1[128], up2[128];
    for (;;) {
        if (inputbox("Passwords", "Enter ROOT password:", "", rp1, sizeof rp1, 1) != 0) return 0;
        if (inputbox("Passwords", "Confirm ROOT password:", "", rp2, sizeof rp2, 1) != 0) return 0;
        if (!rp1[0]) { msgbox("Error", "Root password cannot be empty."); continue; }
        if (strcmp(rp1, rp2)) { msgbox("Error", "Root passwords do not match."); continue; }

        if (inputbox("Passwords", "Enter USER password:", "", up1, sizeof up1, 1) != 0) return 0;
        if (inputbox("Passwords", "Confirm USER password:", "", up2, sizeof up2, 1) != 0) return 0;
        if (!up1[0]) { msgbox("Error", "User password cannot be empty."); continue; }
        if (strcmp(up1, up2)) { msgbox("Error", "User passwords do not match."); continue; }

        snprintf(S.root_pass, sizeof S.root_pass, "%s", rp1);
        snprintf(S.user_pass, sizeof S.user_pass, "%s", up1);
        return 1;
    }
}

static int screen_disk(void) {
    item_t items[32] = {0};
    if (!list_disks(items, 32)) {
        msgbox("No disks found", "No disks were detected. Cannot continue.");
        return 0;
    }

    // show overview
    FILE *p = popen("lsblk -f 2>/dev/null", "r");
    if (p) {
        char buf[4096] = {0};
        size_t used = 0;
        while (fgets(buf + used, (int)(sizeof buf - used), p)) used = strlen(buf);
        pclose(p);
        msgbox("Disk Overview", buf);
    }

    char choice[64] = {0};
    if (radiolist("Disk Selection", "Select the installation disk:", items, items[0].tag, choice, sizeof choice) != 0) return 0;
    char confirm[1024];
    snprintf(confirm, sizeof confirm, "You selected: %s\n\nALL data on this disk will be destroyed.\n\nAre you absolutely sure?", choice);
    if (!yesno("Confirm Disk Erase", confirm)) return 0;
    snprintf(S.disk, sizeof S.disk, "%s", choice + 5); // strip /dev/
    char swapbuf[16];
    snprintf(swapbuf, sizeof swapbuf, "%d", suggest_swap_gb());
    for (;;) {
        if (inputbox("Swap Size", "Enter swap size in GB (1-128):", swapbuf, swapbuf, sizeof swapbuf, 0) != 0) return 0;
        if (validate_swap(swapbuf)) {
            snprintf(S.swap, sizeof S.swap, "%s", swapbuf);
            return 1;
        }
        msgbox("Invalid swap", "Swap must be a number between 1 and 128.");
    }
}

static int screen_filesystem(void) {
    const item_t items[] = {
        {"ext4",  "ext4 - stable, widely supported"},
        {"btrfs", "btrfs - subvolumes + zstd compression"},
        {NULL, NULL}
    };
    char choice[32] = {0};
    if (radiolist("Filesystem", "Choose root filesystem:", items, S.filesystem, choice, sizeof choice) != 0) return 0;
    snprintf(S.filesystem, sizeof S.filesystem, "%s", choice);
    return 1;
}

static int screen_kernel(void) {
    const item_t items[] = {
        {"linux",         "linux - latest stable kernel"},
        {"linux-lts",     "linux-lts - long-term support"},
        {"linux-zen",     "linux-zen - desktop/gaming tuned"},
        {"linux-cachyos", "linux-cachyos - performance tuned"},
        {NULL, NULL}
    };
    char choice[32] = {0};
    if (radiolist("Kernel", "Select the kernel to install:", items, S.kernel, choice, sizeof choice) != 0) return 0;
    snprintf(S.kernel, sizeof S.kernel, "%s", choice);
    return 1;
}

static int screen_bootloader(void) {
    const item_t items_uefi[] = {
        {"grub",         "GRUB - stable, UEFI and BIOS"},
        {"systemd-boot",  "systemd-boot - fast, UEFI only"},
        {"limine",        "Limine - modern, lightweight"},
        {NULL, NULL}
    };
    const item_t items_bios[] = {
        {"grub", "GRUB - stable, recommended for BIOS"},
        {"limine", "Limine - modern, lightweight"},
        {NULL, NULL}
    };
    char choice[32] = {0};
    const item_t *items = is_uefi() ? items_uefi : items_bios;
    if (radiolist("Bootloader", "Choose a bootloader:", items, S.bootloader, choice, sizeof choice) != 0) return 0;
    snprintf(S.bootloader, sizeof S.bootloader, "%s", choice);
    return 1;
}

static int screen_mirrors(void) {
    const item_t items[] = {
        {"yes", "Yes - use reflector"},
        {"no",  "No - keep default mirrors"},
        {NULL, NULL}
    };
    char choice[8] = {0};
    if (radiolist("Mirror Optimization", "Use reflector to pick the 10 fastest mirrors?", items, S.mirrors ? "yes" : "no", choice, sizeof choice) != 0) return 0;
    S.mirrors = !strcmp(choice, "yes");
    return 1;
}

static int screen_timezone(void) {
    char tz[64];
    if (inputbox("Timezone", "Enter timezone (e.g. Europe/Madrid):", S.timezone, tz, sizeof tz, 0) != 0) return 0;
    snprintf(S.timezone, sizeof S.timezone, "%s", tz);
    return 1;
}

static int screen_keymap(void) {
    char km[32];
    if (inputbox("Keyboard", "Enter console keymap (e.g. us, es, uk):", S.keymap, km, sizeof km, 0) != 0) return 0;
    snprintf(S.keymap, sizeof S.keymap, "%s", km);
    return 1;
}

static int screen_desktop(void) {
    char choice[32];
    if (radiolist("Desktop", "Choose a desktop environment:", desktop_names, S.desktop, choice, sizeof choice) != 0) return 0;
    snprintf(S.desktop, sizeof S.desktop, "%s", choice);
    return 1;
}

static int screen_yay(void) {
    const item_t items[] = {{"yes", "Install yay"}, {"no", "Skip yay"}, {NULL, NULL}};
    char choice[8] = {0};
    if (radiolist("yay", "Install yay (AUR helper)?", items, S.yay ? "yes" : "no", choice, sizeof choice) != 0) return 0;
    S.yay = !strcmp(choice, "yes");
    return 1;
}

static int screen_flatpak(void) {
    const item_t items[] = {{"yes", "Install Flatpak + Flathub"}, {"no", "Skip Flatpak"}, {NULL, NULL}};
    char choice[8] = {0};
    if (radiolist("Flatpak", "Install Flatpak + Flathub?", items, S.flatpak ? "yes" : "no", choice, sizeof choice) != 0) return 0;
    S.flatpak = !strcmp(choice, "yes");
    return 1;
}

static int screen_snapper(void) {
    const item_t items[] = {{"yes", "Enable Snapper (BTRFS only)"}, {"no", "Skip Snapper"}, {NULL, NULL}};
    char choice[8] = {0};
    if (radiolist("Snapshots", "Enable Snapper snapshots?", items, S.snapper ? "yes" : "no", choice, sizeof choice) != 0) return 0;
    S.snapper = !strcmp(choice, "yes");
    return 1;
}

static int screen_gpu(void) {
    int g = detect_gpu();
    switch (g) {
        case 1: snprintf(S.gpu, sizeof S.gpu, "Intel+NVIDIA"); break;
        case 2: snprintf(S.gpu, sizeof S.gpu, "Intel+AMD"); break;
        case 3: snprintf(S.gpu, sizeof S.gpu, "NVIDIA"); break;
        case 4: snprintf(S.gpu, sizeof S.gpu, "AMD"); break;
        case 5: snprintf(S.gpu, sizeof S.gpu, "Intel"); break;
        default: snprintf(S.gpu, sizeof S.gpu, "None"); break;
    }
    return 1;
}

static void screen_review(void) {
    char text[4096];
    snprintf(text, sizeof text,
        "Review your settings:\n\n"
        "  locale:      %s\n"
        "  keymap:      %s\n"
        "  timezone:    %s\n"
        "  disk:        %s\n"
        "  filesystem:  %s\n"
        "  kernel:      %s\n"
        "  bootloader:  %s\n"
        "  desktop:     %s\n"
        "  gpu:         %s\n"
        "  hostname:    %s\n"
        "  username:    %s\n"
        "  swap:        %s GB\n"
        "  mirrors:     %s\n"
        "  yay:         %s\n"
        "  snapper:     %s\n"
        "  flatpak:     %s\n\n"
        "WARNING: THIS WILL ERASE %s.",
        S.locale, S.keymap, S.timezone, S.disk, S.filesystem, S.kernel, S.bootloader, S.desktop, S.gpu,
        S.hostname, S.username, S.swap,
        S.mirrors ? "yes" : "no",
        S.yay ? "yes" : "no",
        S.snapper ? "yes" : "no",
        S.flatpak ? "yes" : "no",
        S.disk[0] ? S.disk : "(no disk selected)"
    );
    if (S.hostname[0] == 0 || S.username[0] == 0 || S.disk[0] == 0 || S.root_pass[0] == 0) {
        msgbox("Review - Incomplete", "Missing hostname, username, disk or root password.");
    } else {
        yesno("Review & Confirm", text);
    }
}

static void pacman_sync_keyring(void) {
    run_cmd_ignore("pacman -Sy --noconfirm archlinux-keyring");
}

static void optimize_mirrors(void) {
    if (!S.mirrors) return;
    run_cmd_ignore("pacman -Sy --noconfirm reflector");
    run_cmd_ignore("reflector --latest 10 --sort rate --save /etc/pacman.d/mirrorlist");
}

static void add_cachyos_repo_live(void) {
    const char *block =
        "\n[cachyos]\n"
        "SigLevel = Optional TrustAll\n"
        "Server = https://mirror.cachyos.org/repo/$arch/$repo\n";
    FILE *f = fopen("/etc/pacman.conf", "a");
    if (!f) return;
    fputs(block, f);
    fclose(f);
    run_cmd_ignore("pacman -Sy --noconfirm");
}

static void add_cachyos_repo_chroot(void) {
    const char *block =
        "\n[cachyos]\n"
        "SigLevel = Optional TrustAll\n"
        "Server = https://mirror.cachyos.org/repo/$arch/$repo\n";
    FILE *f = fopen("/mnt/etc/pacman.conf", "a");
    if (!f) return;
    fputs(block, f);
    fclose(f);
}

static void setup_keyboard(void) {
    char q[256];
    char x11[32];
    snprintf(q, sizeof q, "echo 'KEYMAP=%s' > /mnt/etc/vconsole.conf", S.keymap);
    run_cmd_ignore(q);
    snprintf(x11, sizeof x11, "%s", !strcmp(S.keymap, "es") ? "es" : !strcmp(S.keymap, "uk") ? "gb" : S.keymap);
    char xorg[512];
    snprintf(xorg, sizeof xorg,
             "Section \"InputClass\"\n"
             "    Identifier \"system-keyboard\"\n"
             "    MatchIsKeyboard \"on\"\n"
             "    Option \"XkbLayout\" \"%s\"\n"
             "EndSection\n", x11);
    FILE *f = fopen("/mnt/etc/X11/xorg.conf.d/00-keyboard.conf", "w");
    if (f) {
        fputs(xorg, f);
        fclose(f);
    }
}

static void setup_btrfs(const char *p3, const char *disk) {
    char cmd[1024];
    run_cmd_ignore("mkfs.btrfs -f /tmp/__unused__"); // no-op protection; kept for symmetry
    char opts[256] = "noatime,compress=zstd,space_cache=v2";
    if (is_ssd(disk)) strcat(opts, ",ssd,discard=async");
    snprintf(cmd, sizeof cmd, "mkfs.btrfs -f '%s'", p3); run_cmd(cmd);
    snprintf(cmd, sizeof cmd, "mount '%s' /mnt", p3); run_cmd(cmd);
    run_cmd("btrfs subvolume create /mnt/@");
    run_cmd("btrfs subvolume create /mnt/@home");
    run_cmd("btrfs subvolume create /mnt/@var");
    run_cmd("btrfs subvolume create /mnt/@snapshots");
    run_cmd("umount /mnt");
    snprintf(cmd, sizeof cmd, "mount -o %s,subvol=@ '%s' /mnt", opts, p3); run_cmd(cmd);
    run_cmd("mkdir -p /mnt/home /mnt/var /mnt/.snapshots");
    snprintf(cmd, sizeof cmd, "mount -o %s,subvol=@home '%s' /mnt/home", opts, p3); run_cmd(cmd);
    snprintf(cmd, sizeof cmd, "mount -o %s,subvol=@var '%s' /mnt/var", opts, p3); run_cmd(cmd);
    snprintf(cmd, sizeof cmd, "mount -o %s,subvol=@snapshots '%s' /mnt/.snapshots", opts, p3); run_cmd(cmd);
}

static void install_grub(const char *disk) {
    char cmd[512];
    if (is_uefi()) {
        run_cmd_ignore("arch-chroot /mnt grub-install --target=x86_64-efi --efi-directory=/boot/efi --bootloader-id=GRUB");
    } else {
        snprintf(cmd, sizeof cmd, "arch-chroot /mnt grub-install --target=i386-pc '%s'", disk);
        run_cmd_ignore(cmd);
    }
    run_cmd_ignore("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");
}

static void install_systemd_boot(const char *root_dev) {
    (void)root_dev;
    run_cmd_ignore("arch-chroot /mnt bootctl install");
    FILE *f = fopen("/mnt/boot/loader/loader.conf", "w");
    if (f) {
        fputs("default arch.conf\ntimeout 4\nconsole-mode max\neditor no\n", f);
        fclose(f);
    }
    char conf[1024];
    char micro[32] = {0};
    detect_cpu_microcode(micro, sizeof micro);
    snprintf(conf, sizeof conf,
        "title   Arch Linux\n"
        "linux   /vmlinuz-%s\n"
        "%s"
        "initrd  /initramfs-%s.img\n"
        "options root=PARTUUID=CHANGEME rw quiet %s\n",
        S.kernel,
        micro[0] ? "initrd  /CHANGEME.img\n" : "",
        S.kernel,
        strcmp(S.filesystem, "btrfs") == 0 ? "rootflags=subvol=@ " : "");
    f = fopen("/mnt/boot/loader/entries/arch.conf", "w");
    if (f) {
        fputs(conf, f);
        fclose(f);
    }
}

static void install_limine(const char *disk, const char *root_dev) {
    (void)root_dev;
    char cmd[1024];
    run_cmd_ignore("mkdir -p /mnt/boot/limine");
    FILE *f = fopen("/mnt/boot/limine.conf", "w");
    if (f) {
        fputs("timeout: 5\n\n/Arch Linux\n    protocol: linux\n    path: boot():/boot/vmlinuz-linux\n    cmdline: root=/dev/sda3 rw quiet\n    module_path: boot():/boot/initramfs-linux.img\n", f);
        fclose(f);
    }
    if (is_uefi()) {
        run_cmd_ignore("arch-chroot /mnt mkdir -p /boot/efi/EFI/limine && cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/limine/");
        run_cmd_ignore("arch-chroot /mnt mkdir -p /boot/efi/EFI/BOOT && cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/BOOT/BOOTX64.EFI");
        snprintf(cmd, sizeof cmd, "efibootmgr --create --disk '%s' --part 1 --label 'Arch Linux Limine' --loader '\\EFI\\limine\\BOOTX64.EFI' --unicode", disk);
        run_cmd_ignore(cmd);
    } else {
        run_cmd_ignore("arch-chroot /mnt cp /usr/share/limine/limine-bios.sys /boot/limine/");
        snprintf(cmd, sizeof cmd, "limine bios-install '%s'", disk);
        run_cmd_ignore(cmd);
    }
}

static void install_flatpak(const char *uname) {
    run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm flatpak");
    char cmd[1024];
    snprintf(cmd, sizeof cmd, "arch-chroot /mnt su - '%s' -c 'flatpak remote-add --if-not-exists flathub https://dl.flathub.org/repo/flathub.flatpakrepo'", uname);
    run_cmd_ignore(cmd);
}

static void install_gpu_drivers(void) {
    char cmd[1024];
    if (!strcmp(S.gpu, "NVIDIA")) {
        snprintf(cmd, sizeof cmd, "arch-chroot /mnt pacman -S --noconfirm %s nvidia-utils nvidia-settings",
                 strcmp(S.kernel, "linux") == 0 ? "nvidia" : "nvidia-dkms");
        run_cmd_ignore(cmd);
    } else if (!strcmp(S.gpu, "AMD")) {
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver");
    } else if (!strcmp(S.gpu, "Intel")) {
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver");
    } else if (!strcmp(S.gpu, "Intel+NVIDIA")) {
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver");
        snprintf(cmd, sizeof cmd, "arch-chroot /mnt pacman -S --noconfirm %s nvidia-utils nvidia-settings nvidia-prime",
                 strcmp(S.kernel, "linux") == 0 ? "nvidia" : "nvidia-dkms");
        run_cmd_ignore(cmd);
    } else if (!strcmp(S.gpu, "Intel+AMD")) {
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver vulkan-radeon libva-mesa-driver");
    }
}

static void desktop_packages(const char *desktop, const char **groups, size_t *count, const char **dm) {
    *count = 0;
    *dm = NULL;
    if (!strcmp(desktop, "KDE Plasma")) {
        groups[(*count)++] = "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput";
        groups[(*count)++] = "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm";
        *dm = "sddm";
    } else if (!strcmp(desktop, "GNOME")) {
        groups[(*count)++] = "gnome gdm firefox";
        *dm = "gdm";
    } else if (!strcmp(desktop, "Cinnamon")) {
        groups[(*count)++] = "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput";
        groups[(*count)++] = "cinnamon lightdm lightdm-gtk-greeter alacritty firefox";
        *dm = "lightdm";
    } else if (!strcmp(desktop, "XFCE")) {
        groups[(*count)++] = "xorg-server xfce4 xfce4-goodies lightdm lightdm-gtk-greeter alacritty firefox";
        *dm = "lightdm";
    } else if (!strcmp(desktop, "MATE")) {
        groups[(*count)++] = "xorg-server mate mate-extra lightdm lightdm-gtk-greeter alacritty firefox";
        *dm = "lightdm";
    } else if (!strcmp(desktop, "LXQt")) {
        groups[(*count)++] = "xorg-server lxqt sddm breeze-icons alacritty firefox";
        *dm = "sddm";
    } else if (!strcmp(desktop, "Hyprland")) {
        groups[(*count)++] = "hyprland waybar wofi alacritty xdg-desktop-portal-hyprland polkit-gnome qt5-wayland qt6-wayland sddm firefox";
        *dm = "sddm";
    } else if (!strcmp(desktop, "Sway")) {
        groups[(*count)++] = "sway waybar wofi alacritty xdg-desktop-portal-wlr polkit-gnome qt5-wayland sddm firefox";
        *dm = "sddm";
    }
}

static void install_base_system(const char *disk_path, const char *p1, const char *p2, const char *p3) {
    char micro[32] = {0};
    detect_cpu_microcode(micro, sizeof micro);

    if (S.mirrors) {
        optimize_mirrors();
    }

    run_cmd_ignore("wipefs -a /dev/null"); // harmless placeholder to preserve sequence; real wipe below
    char cmd[2048];

    snprintf(cmd, sizeof cmd, "wipefs -a '%s'", disk_path); run_cmd_ignore(cmd);
    snprintf(cmd, sizeof cmd, "sgdisk -Z '%s'", disk_path); run_cmd(cmd);
    run_cmd_ignore("udevadm settle --timeout=10");
    run_cmd_ignore("sleep 1");

    if (is_uefi()) {
        snprintf(cmd, sizeof cmd, "sgdisk -n1:0:+1G -t1:ef00 '%s'", disk_path); run_cmd(cmd);
    } else {
        snprintf(cmd, sizeof cmd, "sgdisk -n1:0:+1M -t1:ef02 '%s'", disk_path); run_cmd(cmd);
    }
    snprintf(cmd, sizeof cmd, "sgdisk -n2:0:+%sG -t2:8200 '%s'", S.swap, disk_path); run_cmd(cmd);
    snprintf(cmd, sizeof cmd, "sgdisk -n3:0:0 -t3:8300 '%s'", disk_path); run_cmd(cmd);
    run_cmd_ignore("udevadm settle --timeout=10");
    run_cmd_ignore("sleep 1");

    if (is_uefi()) {
        snprintf(cmd, sizeof cmd, "mkfs.fat -F32 '%s'", p1); run_cmd(cmd);
    }
    snprintf(cmd, sizeof cmd, "mkswap '%s'", p2); run_cmd(cmd);
    snprintf(cmd, sizeof cmd, "swapon '%s'", p2); run_cmd_ignore(cmd);

    if (!strcmp(S.filesystem, "btrfs")) {
        setup_btrfs(p3, disk_path);
    } else {
        snprintf(cmd, sizeof cmd, "mkfs.ext4 -F '%s'", p3); run_cmd(cmd);
        snprintf(cmd, sizeof cmd, "mount '%s' /mnt", p3); run_cmd(cmd);
    }

    if (is_uefi()) {
        if (!strcmp(S.bootloader, "systemd-boot")) {
            run_cmd_ignore("mkdir -p /mnt/boot");
            snprintf(cmd, sizeof cmd, "mount '%s' /mnt/boot", p1); run_cmd(cmd);
        } else {
            run_cmd_ignore("mkdir -p /mnt/boot/efi");
            snprintf(cmd, sizeof cmd, "mount '%s' /mnt/boot/efi", p1); run_cmd(cmd);
        }
    }

    if (!strcmp(S.kernel, "linux-cachyos")) {
        add_cachyos_repo_live();
    }

    const char *boot_pkgs = "";
    if (!strcmp(S.bootloader, "systemd-boot")) {
        boot_pkgs = " efibootmgr";
    } else if (!strcmp(S.bootloader, "limine")) {
        boot_pkgs = is_uefi() ? " limine efibootmgr" : " limine";
    } else {
        boot_pkgs = is_uefi() ? " grub efibootmgr" : " grub";
    }

    char pkgs[2048];
    snprintf(pkgs, sizeof pkgs,
             "base %s linux-firmware %s-headers sof-firmware base-devel%s vim nano networkmanager git sudo bash-completion%s%s",
             S.kernel, S.kernel, boot_pkgs,
             !strcmp(S.filesystem, "btrfs") ? " btrfs-progs" : "",
             micro[0] ? " " : "");

    char final_pkgs[2304];
    if (micro[0]) {
        snprintf(final_pkgs, sizeof final_pkgs, "%s %s", pkgs, micro);
    } else {
        snprintf(final_pkgs, sizeof final_pkgs, "%s", pkgs);
    }

    snprintf(cmd, sizeof cmd, "pacstrap -K /mnt %s", final_pkgs);
    run_cmd(cmd);
    run_cmd_ignore("genfstab -U /mnt >> /mnt/etc/fstab");

    if (!strcmp(S.kernel, "linux-cachyos")) add_cachyos_repo_chroot();

    snprintf(cmd, sizeof cmd, "bash -c \"echo '%s' > /mnt/etc/hostname\"", S.hostname); run_cmd_ignore(cmd);
    FILE *hf = fopen("/mnt/etc/hosts", "w");
    if (hf) {
        fprintf(hf, "127.0.0.1\tlocalhost\n::1\t\tlocalhost\n127.0.1.1\t%s.localdomain\t%s\n", S.hostname, S.hostname);
        fclose(hf);
    }

    FILE *lg = fopen("/mnt/etc/locale.gen", "r+");
    if (lg) {
        char tmp[8192]; size_t n = fread(tmp, 1, sizeof(tmp)-1, lg); tmp[n] = 0;
        fclose(lg);
    }
    snprintf(cmd, sizeof cmd, "arch-chroot /mnt sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen"); run_cmd_ignore(cmd);
    char loc_line[128]; snprintf(loc_line, sizeof loc_line, "%s UTF-8", S.locale);
    snprintf(cmd, sizeof cmd, "arch-chroot /mnt sed -i 's/^#%s/%s/' /etc/locale.gen", loc_line, loc_line); run_cmd_ignore(cmd);
    run_cmd_ignore("arch-chroot /mnt locale-gen");
    snprintf(cmd, sizeof cmd, "bash -c \"echo 'LANG=%s' > /mnt/etc/locale.conf\"", S.locale); run_cmd_ignore(cmd);
    snprintf(cmd, sizeof cmd, "ln -sf /usr/share/zoneinfo/%s /mnt/etc/localtime", S.timezone); run_cmd_ignore(cmd);
    run_cmd_ignore("arch-chroot /mnt hwclock --systohc");

    setup_keyboard();
    run_cmd_ignore("arch-chroot /mnt mkinitcpio -P");

    snprintf(cmd, sizeof cmd, "arch-chroot /mnt useradd -m -G wheel -s /bin/bash '%s'", S.username); run_cmd_ignore(cmd);
    run_cmd_ignore("arch-chroot /mnt sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers");

    char q1[256], q2[256];
    sh_quote(S.root_pass, q1, sizeof q1);
    sh_quote(S.user_pass, q2, sizeof q2);
    snprintf(cmd, sizeof cmd, "bash -c \"printf 'root:%s\\n' | arch-chroot /mnt chpasswd\"", S.root_pass); run_cmd_ignore(cmd);
    snprintf(cmd, sizeof cmd, "bash -c \"printf '%s:%s\\n' | arch-chroot /mnt chpasswd\"", S.username, S.user_pass); run_cmd_ignore(cmd);

    run_cmd_ignore("arch-chroot /mnt systemctl enable NetworkManager");
    if (is_ssd(disk_path)) run_cmd_ignore("arch-chroot /mnt systemctl enable fstrim.timer");

    install_gpu_drivers();

    if (strcmp(S.desktop, "None") != 0) {
        const char *groups[4] = {0};
        size_t count = 0;
        const char *dm = NULL;
        desktop_packages(S.desktop, groups, &count, &dm);
        for (size_t i = 0; i < count; ++i) {
            snprintf(cmd, sizeof cmd, "arch-chroot /mnt pacman -S --noconfirm %s", groups[i]);
            run_cmd_ignore(cmd);
        }
        if (dm) {
            snprintf(cmd, sizeof cmd, "arch-chroot /mnt systemctl enable %s", dm);
            run_cmd_ignore(cmd);
        }
        if (!strcmp(S.desktop, "Hyprland") || !strcmp(S.desktop, "Sway")) {
            snprintf(cmd, sizeof cmd, "arch-chroot /mnt usermod -aG seat,input,video '%s'", S.username);
            run_cmd_ignore(cmd);
        }
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm pipewire pipewire-pulse wireplumber");
    }

    if (!strcmp(S.bootloader, "systemd-boot")) {
        install_systemd_boot(p3);
    } else if (!strcmp(S.bootloader, "limine")) {
        install_limine(disk_path, p3);
    } else {
        install_grub(disk_path);
    }

    if (S.snapper && !strcmp(S.filesystem, "btrfs")) {
        run_cmd_ignore("arch-chroot /mnt pacman -S --noconfirm snapper snap-pac grub-btrfs inotify-tools");
        run_cmd_ignore("arch-chroot /mnt snapper -c root create-config /");
        run_cmd_ignore("arch-chroot /mnt umount /.snapshots");
        run_cmd_ignore("arch-chroot /mnt rm -rf /.snapshots");
        run_cmd_ignore("arch-chroot /mnt mkdir -p /.snapshots");
        run_cmd_ignore("arch-chroot /mnt mount -a");
        run_cmd_ignore("arch-chroot /mnt chmod 750 /.snapshots");
        run_cmd_ignore("arch-chroot /mnt systemctl enable snapper-timeline.timer");
        run_cmd_ignore("arch-chroot /mnt systemctl enable snapper-cleanup.timer");
        if (!strcmp(S.bootloader, "grub")) {
            run_cmd_ignore("arch-chroot /mnt systemctl enable grub-btrfs.path");
            run_cmd_ignore("arch-chroot /mnt grub-mkconfig -o /boot/grub/grub.cfg");
        }
    }

    if (S.yay) {
        run_cmd_ignore("arch-chroot /mnt bash -c \"echo '%wheel ALL=(ALL) NOPASSWD: ALL' > /etc/sudoers.d/99_nopasswd_tmp\"");
        snprintf(cmd, sizeof cmd, "arch-chroot /mnt su - '%s' -c 'git clone https://aur.archlinux.org/yay.git /tmp/yay && cd /tmp/yay && makepkg -si --noconfirm'", S.username);
        run_cmd_ignore(cmd);
        run_cmd_ignore("arch-chroot /mnt rm -f /etc/sudoers.d/99_nopasswd_tmp");
    }

    if (S.flatpak && strcmp(S.desktop, "None") != 0) {
        install_flatpak(S.username);
    }
}

static void screen_finish(void) {
    if (yesno("Installation Complete!", "Arch Linux has been installed successfully.\n\nRemove the installation media. Reboot now?")) {
        run_cmd_ignore("umount -R /mnt");
        run_cmd_ignore("reboot");
    }
}

static int screen_install(void) {
    char disk_path[64];
    snprintf(disk_path, sizeof disk_path, "/dev/%s", S.disk);
    char p1[64], p2[64], p3[64];
    partition_paths(disk_path, p1, p2, p3);

    if (!ensure_network()) {
        msgbox("No network connection", "Connect to the internet and retry.");
        return 0;
    }

    pacman_sync_keyring();
    install_base_system(disk_path, p1, p2, p3);
    msgbox("Installation complete", "Installation complete!");
    return 1;
}

static void main_flow(void) {
    screen_welcome();
    screen_language();
    screen_network();
    int quick = screen_mode();

    if (quick) {
        if (!screen_keymap()) return;
        if (!screen_disk()) return;
        if (!screen_identity()) return;
        if (!screen_passwords()) return;
        screen_review();
        if (!yesno("Install", "Proceed with installation?")) return;
        if (!screen_install()) return;
        screen_finish();
        return;
    }

    if (!screen_filesystem()) return;
    if (!screen_kernel()) return;
    if (!screen_bootloader()) return;
    if (!screen_mirrors()) return;
    if (!screen_identity()) return;
    if (!screen_passwords()) return;
    if (!screen_keymap()) return;
    if (!screen_timezone()) return;
    if (!screen_desktop()) return;
    if (!screen_gpu()) return;
    if (!screen_yay()) return;
    if (!screen_flatpak()) return;
    if (!screen_snapper()) return;
    if (!screen_disk()) return;
    screen_review();
    if (!yesno("Install", "Proceed with installation?")) return;
    if (!screen_install()) return;
    screen_finish();
}

int main(void) {
    if (!is_root()) {
        fprintf(stderr, "This installer must be run as root.\n");
        return 1;
    }
    if (access("/usr/bin/dialog", X_OK) != 0 && access("/bin/dialog", X_OK) != 0) {
        fprintf(stderr, "dialog is required. Install it with: pacman -Sy --noconfirm dialog\n");
    }
    main_flow();
    return 0;
}
