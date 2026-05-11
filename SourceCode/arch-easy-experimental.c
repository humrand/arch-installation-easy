#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <time.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>
#include <termios.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <dirent.h>
#include <signal.h>
#include <sys/statvfs.h>

#include <gtk/gtk.h>

#define VERSION   "V3.0.0"
#define LOG_FILE  "/tmp/arch_install.log"
#define TITLE     "PulseOS Installer"

#define MAX_CMD    8192
#define MAX_OUT    4096
#define MAX_ITEMS  96
#define MAX_LINES  2000
#define KEEP_LINES 1000

#define PULSE_LOGO_LEN 674
static const unsigned char PULSE_LOGO_DATA[PULSE_LOGO_LEN] = {
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa2,0x80, 0xe2,0xa3,0x80, 0xe2,0xa3,0x80, 0xe2,0xa3,0x80,
    0xe2,0xa3,0x80, 0xe2,0xa3,0x80, 0xe2,0xa3,0x80, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa2,0x80, 0xe2,0xa3,0xb4, 0xe2,0xa3,0xbf,
    0xe2,0xa0,0xbf, 0xe2,0xa0,0x9b, 0xe2,0xa0,0x9b, 0xe2,0xa0,0x9b,
    0xe2,0xa0,0x9b, 0xe2,0xa0,0xbb, 0xe2,0xa2,0xbf, 0xe2,0xa3,0xa6,
    0xe2,0xa1,0x80, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa3,0xb0, 0xe2,0xa3,0xbf, 0xe2,0xa0,0x8b, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x99,
    0xe2,0xa3,0xbf, 0xe2,0xa3,0x86, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbc,
    0xe2,0xa1,0xbf, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa3,0xa0, 0xe2,0xa3,0xb6, 0xe2,0xa3,0xb6, 0xe2,0xa3,0xa6,
    0xe2,0xa1,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa2,0xb8, 0xe2,0xa3,0xbf, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa2,0xb8, 0xe2,0xa3,0xbf,
    0xe2,0xa0,0x81, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbe,
    0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf,
    0xe2,0xa3,0xb7, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa3,0xbf, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbf, 0xe2,0xa1,0x87,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbf,
    0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbf, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa3,0xbf, 0xe2,0xa1,0x87,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0xb9,
    0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf, 0xe2,0xa3,0xbf,
    0xe2,0xa0,0x8f, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa3,0xbf, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa2,0xbf, 0xe2,0xa3,0x87,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x88, 0xe2,0xa0,0x89, 0xe2,0xa0,0x89, 0xe2,0xa0,0x81,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa3,0xb8, 0xe2,0xa1,0xbf, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x98, 0xe2,0xa3,0xbf,
    0xe2,0xa3,0xa6, 0xe2,0xa1,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa2,0x80, 0xe2,0xa3,0xb4,
    0xe2,0xa3,0xbf, 0xe2,0xa0,0x83, 0x0a,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80,
    0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x80, 0xe2,0xa0,0x88,
    0xe2,0xa0,0xbb, 0xe2,0xa3,0xbf, 0xe2,0xa3,0xb6, 0xe2,0xa3,0xa4,
    0xe2,0xa3,0x80, 0xe2,0xa3,0x80, 0xe2,0xa3,0x80, 0xe2,0xa3,0xa0,
    0xe2,0xa3,0xb4, 0xe2,0xa3,0xbe, 0xe2,0xa0,0xbf, 0xe2,0xa0,0x8b,
    0x0a, 0x0a,
    0x20,0x20,0x20,0x20,0x20,0x20,
    0x50,0x75,0x6c,0x73,0x65,0x4f,0x53,0x20,0x78,0x38,0x36, 0x0a,
};

typedef struct {
    char lang[8];
    char locale[32];
    char hostname[64];
    char username[64];
    char root_pass[256];
    char user_pass[256];
    char swap[8];
    char disk[64];
    char desktop[32];
    char desktop_list[512];
    char gpu[32];
    char keymap[32];
    char timezone[128];
    char filesystem[8];
    char kernel[32];
    char kernel_list[512];
    char bootloader[16];
    char extra_pkgs[2048];
    int  mirrors;
    int  quick;
    int  yay;
    int  snapper;
    int  flatpak;
    int  dualboot;
    char db_root[128];
    char db_efi[128];
    char db_swap[128];
    int  db_size_gb;
    char profile[32];
    int  laptop;
    char optimus_mode[16];
    char dotfiles[64];
    char dotfiles_url[256];
    int  fish_default;
} State;

static State st = {
    .lang         = "en",
    .locale       = "en_US.UTF-8",
    .hostname     = "",
    .username     = "",
    .root_pass    = "",
    .user_pass    = "",
    .swap         = "8",
    .disk         = "",
    .desktop      = "None",
    .desktop_list = "None",
    .gpu          = "None",
    .keymap       = "us",
    .timezone     = "UTC",
    .filesystem   = "ext4",
    .kernel       = "linux",
    .kernel_list  = "linux",
    .bootloader   = "grub",
    .extra_pkgs   = "",
    .mirrors      = 1,
    .quick        = 0,
    .yay          = 0,
    .snapper      = 0,
    .flatpak      = 0,
    .dualboot     = 0,
    .db_root      = "",
    .db_efi       = "",
    .db_swap      = "",
    .db_size_gb   = 30,
    .profile      = "none",
    .laptop       = 0,
    .optimus_mode = "hybrid",
    .dotfiles     = "none",
    .dotfiles_url = "",
    .fish_default = 0,
};

#define L(en, es) (strcmp(st.lang,"en")==0 ? (en) : (es))

typedef struct { const char *key; const char *val; } KV;

static const KV CONSOLE_TO_X11[] = {
    {"us","us"},{"es","es"},{"uk","gb"},{"fr","fr"},{"de","de"},
    {"it","it"},{"ru","ru"},{"ara","ara"},{"pt-latin9","pt"},
    {"br-abnt2","br"},{"pl2","pl"},{"hu","hu"},{"cz-qwerty","cz"},
    {"sk-qwerty","sk"},{"ro_win","ro"},{"dk","dk"},{"no","no"},
    {"sv-latin1","se"},{"fi","fi"},{"nl","nl"},{"tr_q-latin5","tr"},
    {"ja106","jp"},{"kr106","kr"},{NULL,NULL}
};

static const KV LOCALE_TO_KEYMAP[] = {
    {"es_ES.UTF-8","es"},{"es_MX.UTF-8","us"},{"es_AR.UTF-8","us"},
    {"en_US.UTF-8","us"},{"en_GB.UTF-8","uk"},{"fr_FR.UTF-8","fr"},
    {"de_DE.UTF-8","de"},{"it_IT.UTF-8","it"},{"pt_PT.UTF-8","pt-latin9"},
    {"pt_BR.UTF-8","br-abnt2"},{"ru_RU.UTF-8","ru"},{"nl_NL.UTF-8","nl"},
    {"pl_PL.UTF-8","pl2"},{"cs_CZ.UTF-8","cz-qwerty"},{"sk_SK.UTF-8","sk-qwerty"},
    {"hu_HU.UTF-8","hu"},{"ro_RO.UTF-8","ro_win"},{"da_DK.UTF-8","dk"},
    {"nb_NO.UTF-8","no"},{"sv_SE.UTF-8","sv-latin1"},{"fi_FI.UTF-8","fi"},
    {"tr_TR.UTF-8","tr_q-latin5"},{"ja_JP.UTF-8","ja106"},{"ko_KR.UTF-8","kr106"},
    {"zh_CN.UTF-8","us"},{"ar_SA.UTF-8","ara"},{NULL,NULL}
};

static const char *kv_get(const KV *table, const char *key) {
    for (int i = 0; table[i].key; i++)
        if (strcmp(table[i].key, key) == 0) return table[i].val;
    return NULL;
}

typedef struct {
    const char *name;
    const char *groups[4];
    int         ngroups;
} DesktopDef;

static const DesktopDef DESKTOP_DEFS[] = {
    {"KDE Plasma", {
        "xorg-server xorg-xinit xorg-xrandr xf86-input-libinput",
        "plasma konsole alacritty dolphin ark kate plasma-nm sddm firefox"
    }, 2},
    {"GNOME", {
        "gnome gdm firefox alacritty"
    }, 1},
    {"Cinnamon", {
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
        "cinnamon lightdm lightdm-gtk-greeter alacritty firefox"
    }, 2},
    {"XFCE", {
        "xorg-server xfce4 xfce4-goodies lightdm lightdm-gtk-greeter alacritty firefox"
    }, 1},
    {"MATE", {
        "xorg-server mate mate-extra lightdm lightdm-gtk-greeter alacritty firefox"
    }, 1},
    {"LXQt", {
        "xorg-server lxqt sddm breeze-icons alacritty firefox"
    }, 1},
    {"Hyprland", {
        "hyprland waybar wofi kitty xdg-desktop-portal-hyprland "
        "polkit-gnome qt5-wayland alacritty qt6-wayland sddm firefox"
    }, 1},
    {"Sway", {
        "sway waybar wofi alacritty xdg-desktop-portal-wlr "
        "polkit-gnome qt5-wayland sddm firefox"
    }, 1},
    {NULL, {NULL}, 0}
};

static const DesktopDef *get_desktop_def(const char *name) {
    for (int i = 0; DESKTOP_DEFS[i].name; i++)
        if (strcmp(DESKTOP_DEFS[i].name, name) == 0) return &DESKTOP_DEFS[i];
    return NULL;
}

static const char *get_desktop_dm(const char *name) {
    if (!strcmp(name,"KDE Plasma")) return "sddm";
    if (!strcmp(name,"GNOME"))      return "gdm";
    if (!strcmp(name,"Cinnamon"))   return "lightdm";
    if (!strcmp(name,"XFCE"))       return "lightdm";
    if (!strcmp(name,"MATE"))       return "lightdm";
    if (!strcmp(name,"LXQt"))       return "sddm";
    if (!strcmp(name,"Hyprland"))   return "sddm";
    if (!strcmp(name,"Sway"))       return "sddm";
    return NULL;
}

static pthread_mutex_t g_log_mutex = PTHREAD_MUTEX_INITIALIZER;

static int password_strength(const char *p) {
    if (!p || !p[0]) return 0;
    int len  = (int)strlen(p);
    int has_lower = 0, has_upper = 0, has_digit = 0, has_sym = 0;
    for (const char *c = p; *c; c++) {
        if (islower((unsigned char)*c))  has_lower = 1;
        if (isupper((unsigned char)*c))  has_upper = 1;
        if (isdigit((unsigned char)*c))  has_digit = 1;
        if (ispunct((unsigned char)*c))  has_sym   = 1;
    }
    int score = has_lower + has_upper + has_digit + has_sym;
    if (len < 6)  return 1;
    if (len < 8 || score < 2) return 1;
    if (len < 12 || score < 3) return 2;
    return 3;
}

static void write_log(const char *msg) {
    time_t t = time(NULL);
    struct tm tm;
    localtime_r(&t, &tm);
    char ts[32];
    strftime(ts, sizeof(ts), "%Y-%m-%d %H:%M:%S", &tm);
    pthread_mutex_lock(&g_log_mutex);
    FILE *f = fopen(LOG_FILE, "a");
    if (f) { fprintf(f, "[%s] %s\n", ts, msg); fclose(f); }
    pthread_mutex_unlock(&g_log_mutex);
}

static void write_log_fmt(const char *fmt, ...) {
    char buf[MAX_CMD];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    write_log(buf);
}

static void shell_quote(const char *s, char *out, size_t sz) {
    size_t i = 0;
    if (sz < 4) { if (sz) out[0] = '\0'; return; } 
    out[i++] = '\'';
    for (; *s; ++s) {
        if (*s == '\'') {
            if (i + 5 >= sz) break;
            out[i++] = '\''; out[i++] = '\\';
            out[i++] = '\''; out[i++] = '\'';
        } else {
            if (i + 2 >= sz) break;
            out[i++] = *s;
        }
    }
    out[i++] = '\'';
    out[i]   = '\0';
}

static void trim_nl(char *s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n-1]=='\n'||s[n-1]=='\r'||s[n-1]==' ')) s[--n]='\0';
    size_t lead = 0;
    while (s[lead] && isspace((unsigned char)s[lead])) lead++;
    if (lead) memmove(s, s+lead, strlen(s)-lead+1);
}

static void strip_ansi(const char *src, char *dst, size_t dsz) {
    size_t i = 0;
    while (*src && i < dsz-1) {
        if (*src == '\x1b') {
            src++;
            if (*src == '[' || *src == '(') {
                src++;
                while (*src && !isalpha((unsigned char)*src)) src++;
                if (*src) src++;
            } else if (*src == ']') {
                while (*src && *src != '\x07') {
                    if (*src == '\x1b' && *(src+1) == '\\') { src += 2; goto osc_done; }
                    src++;
                }
                if (*src == '\x07') src++;
                osc_done:;
            } else if (*src) src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}

static int validate_name(const char *s) {
    if (!s) return 0;
    size_t len = strlen(s);
    if (len == 0 || len > 32) return 0;
    if (!isalpha((unsigned char)s[0]) && s[0] != '_') return 0;
    for (const char *p = s + 1; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    return 1;
}

static int validate_swap(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    int v = atoi(s);
    return v >= 1 && v <= 128;
}

typedef struct { char tag[256]; char desc[512]; } MenuItem;

static void dlg_strip(const char *src, char *dst, size_t n) {
    size_t i = 0;
    while (*src && i < n - 1) {
        if (*src == '\\' && *(src+1) == 'Z') {
            src += 2;
            if (*src) src++;
        } else {
            dst[i++] = *src++;
        }
    }
    dst[i] = '\0';
}


static int g_fullscreen     = 1;
static int g_home_requested = 0;

static GtkWidget *g_main_window = NULL;

static const char *APP_CSS =
    "window { background-color: #0d1117; color: #c9d1d9; }"

    "box, grid, stack, overlay, paned { background-color: transparent; }"
    "scrolledwindow { background-color: #0d1117; }"
    "viewport        { background-color: #0d1117; }"
    "frame           { background-color: #0d1117; border: 1px solid #30363d; }"
    "frame > border  { border-color: #30363d; }"
    "frame label     { background-color: transparent; color: #8b949e; }"

    "scrollbar                 { background-color: #161b22; }"
    "scrollbar slider          { background-color: #484f58; border-radius:4px; min-width:6px; min-height:6px; }"
    "scrollbar slider:hover    { background-color: #6e7681; }"

    "label { background-color: transparent; color: #c9d1d9; }"

    ".sidebar          { background-color: #161b22; border-right: 1px solid #30363d; }"
    ".step-row         { padding: 10px 18px; border-left: 3px solid transparent; }"
    ".step-row.active  { background-color: #1c2128; border-left: 3px solid #58a6ff; }"
    ".step-row.done    { opacity: 0.7; }"
    ".step-num         { font-size: 10px; color: #6e7681; margin-right: 4px; }"
    ".step-name        { font-size: 13px; color: #8b949e; }"
    ".step-name.active { color: #e6edf3; font-weight: bold; }"
    ".step-name.done   { color: #3fb950; }"

    ".page-wrap  { padding: 36px 48px; background-color: #0d1117; }"
    ".page-title { font-size: 24px; font-weight: bold; color: #e6edf3; }"
    ".page-sub   { font-size: 13px; color: #8b949e; margin-top: 4px; }"
    ".divider    { background-color: #30363d; min-height: 1px; margin: 20px 0; }"
    ".card       { background-color: #161b22; border-radius: 8px; "
    "              border: 1px solid #30363d; padding: 20px; margin-top: 12px; }"
    ".card-title { font-size: 15px; font-weight: bold; margin-bottom: 6px; }"
    ".hint       { font-size: 12px; color: #6e7681; margin-top: 4px; }"
    ".warn       { color: #f85149; font-weight: bold; }"

    "entry        { background-color: #0d1117; color: #e6edf3; "
    "               border: 1px solid #30363d; border-radius: 6px; "
    "               padding: 8px 12px; caret-color: #58a6ff; }"
    "entry:focus  { border-color: #58a6ff; }"
    "entry.error  { border-color: #f85149; }"

    "button                        { background-color: #21262d; color: #c9d1d9; "
    "                                border: 1px solid #30363d; border-radius: 6px; padding: 7px 18px; }"
    "button:hover                  { background-color: #30363d; border-color: #58a6ff; }"
    "button:active                 { background-color: #161b22; }"
    "button.suggested-action       { background-color: #1f6feb; color: #fff; border: none; }"
    "button.suggested-action:hover { background-color: #388bfd; }"
    "button.destructive-action     { background-color: #da3633; color: #fff; border: none; }"
    "button.large-option           { padding: 14px 20px; margin: 6px 0; border-radius: 8px; }"
    "button.large-option:hover     { background-color: #1c2128; border-color: #58a6ff; }"
    "button.large-option.selected  { border-color: #58a6ff; background-color: #1c2128; }"

    "radiobutton, checkbutton                    { color: #c9d1d9; padding: 4px 0; }"
    "radiobutton label, checkbutton label        { color: #c9d1d9; background-color: transparent; }"
    "radiobutton:checked label, checkbutton:checked label { color: #58a6ff; font-weight: bold; }"

    ".nav-bar { background-color: #161b22; border-top: 1px solid #30363d; padding: 12px 24px; }"

    "progressbar progress { background-color: #1f6feb; border-radius: 4px; min-height: 10px; }"
    "progressbar trough   { background-color: #21262d; border-radius: 4px; min-height: 10px; }"

    "textview      { background-color: #0d1117; color: #e6edf3; font-family: monospace; font-size: 12px; }"
    "textview text { background-color: #0d1117; color: #e6edf3; }"

    "listbox                  { background-color: #0d1117; }"
    "listbox row              { background-color: #0d1117; padding: 10px 14px; "
    "                           border-bottom: 1px solid #21262d; }"
    "listbox row:selected     { background-color: #1c2128; border-left: 3px solid #58a6ff; }"
    "listbox row:hover        { background-color: #161b22; }"
    ".list-tag  { font-weight: bold; color: #e6edf3; }"
    ".list-desc { font-size: 12px; color: #8b949e; }"

    "listbox.disk-list row:selected { "
    "    background-color: rgba(248,81,73,0.12); "
    "    border-left: 3px solid #f85149; "
    "    box-shadow: inset 4px 0 18px rgba(248,81,73,0.18); }"
    "listbox.disk-list row:hover    { background-color: rgba(248,81,73,0.06); }"

    "combobox button       { background-color: #21262d; color: #c9d1d9; "
    "                        border: 1px solid #30363d; border-radius: 6px; }"
    "combobox button:hover { border-color: #58a6ff; }"

    "window.combo                           { background-color: #161b22; border: 1px solid #30363d; }"
    "window.combo scrolledwindow            { background-color: #161b22; }"
    "window.combo scrollbar                 { background-color: #21262d; }"
    "window.combo scrollbar slider          { background-color: #484f58; border-radius: 4px; }"
    "window.combo treeview                  { background-color: #161b22; color: #c9d1d9; }"
    "window.combo treeview:selected         { background-color: #1f6feb; color: #fff; }"
    "window.combo treeview:hover            { background-color: #1c2128; color: #e6edf3; }"
    "window.combo treeview.view             { background-color: #161b22; color: #c9d1d9; }"
    "window.combo treeview.view:selected    { background-color: #1f6feb; color: #fff; }"
    "window.combo treeview.view cell        { padding: 6px 12px; }"

    "menu            { background-color: #161b22; color: #c9d1d9; border: 1px solid #30363d; padding: 4px 0; }"
    "menuitem        { background-color: transparent; color: #c9d1d9; padding: 6px 16px; }"
    "menuitem:hover  { background-color: #1c2128; color: #e6edf3; }"
    "menuitem:selected { background-color: #1f6feb; color: #fff; }"

    "dialog                    { background-color: #0d1117; }"
    "dialog .dialog-action-area { background-color: #161b22; border-top: 1px solid #30363d; }"
    "dialog .dialog-action-area button { min-width: 80px; }"
    "messagedialog .message-dialog-text { font-size: 14px; color: #c9d1d9; }"

    "separator                { background-color: #30363d; }"
    "spinner                  { color: #58a6ff; }"
    "searchentry              { background-color: #0d1117; border-color: #30363d; }"
    "tooltip                  { background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; }"
    ".strength-weak           { color: #f85149; font-weight: bold; }"
    ".strength-medium         { color: #d29922; font-weight: bold; }"
    ".strength-strong         { color: #3fb950; font-weight: bold; }"
    ".welcome-title           { font-size: 32px; font-weight: bold; color: #58a6ff; }"
    ".welcome-ver             { font-size: 13px; color: #6e7681; }"
    ".badge-uefi { background-color: #1f6feb; color:#fff; border-radius:4px; padding: 2px 8px; font-size:12px; }"
    ".badge-bios { background-color: #d29922; color:#000; border-radius:4px; padding: 2px 8px; font-size:12px; }"
    ;


static void setup_css(void) {
    GtkCssProvider *p = gtk_css_provider_new();
    gtk_css_provider_load_from_data(p, APP_CSS, -1, NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(p),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    g_object_unref(p);
}

static void add_class(GtkWidget *w, const char *cls) {
    gtk_style_context_add_class(gtk_widget_get_style_context(w), cls);
}

static void msgbox(const char *title, const char *text) {
    char clean[4096]; dlg_strip(text, clean, sizeof(clean));
    GtkWidget *dlg = gtk_message_dialog_new(
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_INFO, GTK_BUTTONS_OK, "%s", clean);
    gtk_window_set_title(GTK_WINDOW(dlg), title);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static int yesno_dlg(const char *title, const char *text) {
    char clean[4096]; dlg_strip(text, clean, sizeof(clean));
    GtkWidget *dlg = gtk_message_dialog_new(
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO, "%s", clean);
    gtk_window_set_title(GTK_WINDOW(dlg), title);
    int r = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    return r == GTK_RESPONSE_YES;
}

static int inputbox_dlg(const char *title, const char *text,
                         const char *init, char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        title,
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        L("_Back","_Atrás"), GTK_RESPONSE_CANCEL,
        "_OK",               GTK_RESPONSE_OK,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

    GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(box), 10);
    gtk_container_set_border_width(GTK_CONTAINER(box), 20);

    GtkWidget *lbl = gtk_label_new(clean);
    gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    GtkWidget *entry = gtk_entry_new();
    if (init && *init) gtk_entry_set_text(GTK_ENTRY(entry), init);
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

    gtk_widget_show_all(dlg);
    int rc = gtk_dialog_run(GTK_DIALOG(dlg));
    if (rc == GTK_RESPONSE_OK && out)
        strncpy(out, gtk_entry_get_text(GTK_ENTRY(entry)), outsz-1);
    gtk_widget_destroy(dlg);
    return rc == GTK_RESPONSE_OK;
}

static int passwordbox_dlg(const char *title, const char *text,
                            char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    int show_pass = 0;
    int result = 0;

    while (1) {
        GtkWidget *dlg = gtk_dialog_new_with_buttons(
            title,
            g_main_window ? GTK_WINDOW(g_main_window) : NULL,
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            L("_Back","_Atrás"),         GTK_RESPONSE_CANCEL,
            show_pass ? L("🙈 Hide","🙈 Ocultar")
                      : L("👁 Show","👁 Mostrar"), 77,
            "_OK",                        GTK_RESPONSE_OK,
            NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);

        GtkWidget *box = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
        gtk_box_set_spacing(GTK_BOX(box), 10);
        gtk_container_set_border_width(GTK_CONTAINER(box), 20);

        GtkWidget *lbl = gtk_label_new(clean);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

        GtkWidget *entry = gtk_entry_new();
        gtk_entry_set_visibility(GTK_ENTRY(entry), show_pass);
        if (out && out[0]) gtk_entry_set_text(GTK_ENTRY(entry), out);
        gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
        gtk_box_pack_start(GTK_BOX(box), entry, FALSE, FALSE, 0);

        gtk_widget_show_all(dlg);
        int rc = gtk_dialog_run(GTK_DIALOG(dlg));
        const char *val = gtk_entry_get_text(GTK_ENTRY(entry));
        if (out && val) strncpy(out, val, outsz-1);

        gtk_widget_destroy(dlg);

        if (rc == 77) { show_pass = !show_pass; continue; }
        result = (rc == GTK_RESPONSE_OK);
        break;
    }
    return result;
}

static int radiolist_dlg(const char *title, const char *text,
                           MenuItem *items, int n, const char *def,
                           char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        title,
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        L("_Back","_Atrás"), GTK_RESPONSE_CANCEL,
        "_OK",               GTK_RESPONSE_OK,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 660, 520);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_box_set_spacing(GTK_BOX(content), 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 380);
    gtk_container_set_border_width(GTK_CONTAINER(content), 18);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget *desc_lbl = gtk_label_new(clean);
    gtk_label_set_line_wrap(GTK_LABEL(desc_lbl), TRUE);
    gtk_label_set_xalign(GTK_LABEL(desc_lbl), 0.0f);
    gtk_box_pack_start(GTK_BOX(vbox), desc_lbl, FALSE, FALSE, 0);

    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 4);

    GSList *group = NULL;
    GtkWidget **radios = g_new0(GtkWidget*, n);
    for (int i = 0; i < n; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        radios[i] = gtk_radio_button_new_with_label(group, items[i].tag);
        group = gtk_radio_button_get_group(GTK_RADIO_BUTTON(radios[i]));
        gtk_box_pack_start(GTK_BOX(row), radios[i], FALSE, FALSE, 0);

        if (items[i].desc[0] && strcmp(items[i].desc, items[i].tag) != 0) {
            GtkWidget *sub = gtk_label_new(items[i].desc);
            gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(sub), TRUE);
            add_class(sub, "hint");
            gtk_widget_set_margin_start(sub, 26);
            gtk_box_pack_start(GTK_BOX(row), sub, FALSE, FALSE, 0);
        }

        if (def && strcmp(items[i].tag, def) == 0)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(radios[i]), TRUE);

        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 4);
    }

    gtk_container_add(GTK_CONTAINER(scroll), vbox);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg);
    int rc = gtk_dialog_run(GTK_DIALOG(dlg));

    if (rc == GTK_RESPONSE_OK && out) {
        for (int i = 0; i < n; i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(radios[i]))) {
                strncpy(out, items[i].tag, outsz-1);
                break;
            }
        }
    }
    g_free(radios);
    gtk_widget_destroy(dlg);
    return rc == GTK_RESPONSE_OK;
}

typedef struct { GtkWidget *dlg; } MenuRowActivateData;
static void menu_row_activated_cb(GtkListBox *lb, GtkListBoxRow *row, gpointer data) {
    (void)lb; (void)row;
    MenuRowActivateData *d = (MenuRowActivateData *)data;
    gtk_dialog_response(GTK_DIALOG(d->dlg), GTK_RESPONSE_OK);
}

static int menu_dlg(const char *title, const char *text,
                     MenuItem *items, int n, char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        title,
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        L("_Back","_Atrás"), GTK_RESPONSE_CANCEL,
        "_OK",               GTK_RESPONSE_OK,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 660, 500);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 18);
    gtk_box_set_spacing(GTK_BOX(content), 8);

    if (clean[0]) {
        GtkWidget *lbl = gtk_label_new(clean);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 320);

    GtkWidget *lb = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(lb), GTK_SELECTION_SINGLE);
    add_class(lb, "menu-list");

    for (int i = 0; i < n; i++) {
        GtkWidget *row_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);
        gtk_container_set_border_width(GTK_CONTAINER(row_box), 4);

        GtkWidget *tag_lbl = gtk_label_new(items[i].tag);
        gtk_label_set_xalign(GTK_LABEL(tag_lbl), 0.0f);
        add_class(tag_lbl, "list-tag");
        gtk_box_pack_start(GTK_BOX(row_box), tag_lbl, FALSE, FALSE, 0);

        if (items[i].desc[0] && strcmp(items[i].desc, items[i].tag) != 0) {
            GtkWidget *d = gtk_label_new(items[i].desc);
            gtk_label_set_xalign(GTK_LABEL(d), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(d), TRUE);
            add_class(d, "list-desc");
            gtk_box_pack_start(GTK_BOX(row_box), d, FALSE, FALSE, 0);
        }

        gtk_container_add(GTK_CONTAINER(lb), row_box);
    }

    GtkListBoxRow *row0 = gtk_list_box_get_row_at_index(GTK_LIST_BOX(lb), 0);
    if (row0) gtk_list_box_select_row(GTK_LIST_BOX(lb), row0);

    MenuRowActivateData *rad = g_new0(MenuRowActivateData, 1);
    rad->dlg = dlg;
    g_signal_connect_data(lb, "row-activated",
        G_CALLBACK(menu_row_activated_cb),
        rad, (GClosureNotify)g_free, (GConnectFlags)0);

    gtk_container_add(GTK_CONTAINER(scroll), lb);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg);
    int rc = gtk_dialog_run(GTK_DIALOG(dlg));

    if (rc == GTK_RESPONSE_OK && out) {
        GtkListBoxRow *sel = gtk_list_box_get_selected_row(GTK_LIST_BOX(lb));
        if (sel) {
            int idx = gtk_list_box_row_get_index(sel);
            if (idx >= 0 && idx < n)
                strncpy(out, items[idx].tag, outsz-1);
        }
    }
    gtk_widget_destroy(dlg);
    return rc == GTK_RESPONSE_OK;
}

static int checklist_dlg(const char *title, const char *text,
                          MenuItem *items, int n,
                          const char **defaults, int ndef,
                          char out[][256], int maxout) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        title,
        g_main_window ? GTK_WINDOW(g_main_window) : NULL,
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        L("_Back","_Atrás"), GTK_RESPONSE_CANCEL,
        "_OK",               GTK_RESPONSE_OK,
        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dlg), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 680, 560);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 18);
    gtk_box_set_spacing(GTK_BOX(content), 8);

    if (clean[0]) {
        GtkWidget *lbl = gtk_label_new(clean);
        gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
        gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(content), lbl, FALSE, FALSE, 0);
    }

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
                                    GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(scroll, -1, 380);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 8);

    GtkWidget **checks = g_new0(GtkWidget*, n);
    for (int i = 0; i < n; i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL, 2);

        checks[i] = gtk_check_button_new_with_label(items[i].tag);

        int on = 0;
        for (int j = 0; j < ndef; j++)
            if (defaults[j] && strcmp(items[i].tag, defaults[j]) == 0) { on=1; break; }
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(checks[i]), on);

        gtk_box_pack_start(GTK_BOX(row), checks[i], FALSE, FALSE, 0);

        if (items[i].desc[0] && strcmp(items[i].desc, items[i].tag) != 0) {
            GtkWidget *sub = gtk_label_new(items[i].desc);
            gtk_label_set_xalign(GTK_LABEL(sub), 0.0f);
            gtk_label_set_line_wrap(GTK_LABEL(sub), TRUE);
            add_class(sub, "hint");
            gtk_widget_set_margin_start(sub, 26);
            gtk_box_pack_start(GTK_BOX(row), sub, FALSE, FALSE, 0);
        }

        gtk_box_pack_start(GTK_BOX(vbox), row, FALSE, FALSE, 0);
    }

    gtk_container_add(GTK_CONTAINER(scroll), vbox);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);

    gtk_widget_show_all(dlg);
    int rc = gtk_dialog_run(GTK_DIALOG(dlg));

    int count = 0;
    if (rc == GTK_RESPONSE_OK) {
        for (int i = 0; i < n && count < maxout; i++) {
            if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(checks[i])))
                strncpy(out[count++], items[i].tag, 255);
        }
    }
    g_free(checks);
    gtk_widget_destroy(dlg);
    return rc == GTK_RESPONSE_OK ? count : -1;
}

static GtkWidget *g_infobox_win = NULL;

static void infobox_dlg(const char *title, const char *text) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    if (g_infobox_win) { gtk_widget_destroy(g_infobox_win); g_infobox_win = NULL; }

    GtkWidget *w = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(w), title);
    gtk_window_set_default_size(GTK_WINDOW(w), 360, 120);
    gtk_window_set_position(GTK_WINDOW(w), GTK_WIN_POS_CENTER);
    gtk_window_set_resizable(GTK_WINDOW(w), FALSE);
    if (g_main_window)
        gtk_window_set_transient_for(GTK_WINDOW(w), GTK_WINDOW(g_main_window));

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    gtk_container_set_border_width(GTK_CONTAINER(box), 24);

    GtkWidget *spinner = gtk_spinner_new();
    gtk_spinner_start(GTK_SPINNER(spinner));
    gtk_box_pack_start(GTK_BOX(box), spinner, FALSE, FALSE, 0);

    GtkWidget *lbl = gtk_label_new(clean);
    gtk_label_set_line_wrap(GTK_LABEL(lbl), TRUE);
    gtk_box_pack_start(GTK_BOX(box), lbl, FALSE, FALSE, 0);

    gtk_container_add(GTK_CONTAINER(w), box);
    gtk_widget_show_all(w);
    g_infobox_win = w;

    while (gtk_events_pending()) gtk_main_iteration_do(FALSE);
}

static void infobox_close(void) {
    if (g_infobox_win) { gtk_widget_destroy(g_infobox_win); g_infobox_win = NULL; }
}

typedef void (*LineCallback)(const char *line, void *ud);

static int run_stream(const char *cmd, LineCallback cb, void *ud, int ignore_error);
static int run_simple(const char *cmd, int ignore_error) {
    return run_stream(cmd, NULL, NULL, ignore_error);
}

static int run_stream(const char *cmd, LineCallback cb, void *ud, int ignore_error) {
    write_log_fmt("$ %s", cmd);
    char full[MAX_CMD];
    snprintf(full,sizeof(full),"{ %s; } 2>&1",cmd);
    FILE *fp = popen(full,"r");
    if (!fp) {
        write_log_fmt("ERROR: popen failed: %s", cmd);
        return -1;
    }
    char line[4096];
    while (fgets(line,sizeof(line),fp)) {
        size_t len = strlen(line);
        while (len>0 && (line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
        if (len>0) {
            write_log(line);
            if (cb) cb(line,ud);
        }
    }
    int rc = pclose(fp);
    if (rc == -1) {
        rc = -1;
    } else if (WIFEXITED(rc)) {
        rc = WEXITSTATUS(rc);
    } else if (WIFSIGNALED(rc)) {
        write_log_fmt("Command killed by signal %d: %s", WTERMSIG(rc), cmd);
        rc = 128 + WTERMSIG(rc);
    } else {
        rc = -1;
    }
    if (rc!=0) {
        write_log_fmt("ERROR (rc=%d): %s", rc, cmd);
        if (ignore_error) write_log("Command failure was ignored by the caller.");
    }
    return rc;
}

static int is_laptop(void) {
    return access("/sys/class/power_supply/BAT0", F_OK) == 0 ||
           access("/sys/class/power_supply/BAT1", F_OK) == 0;
}

typedef struct {
    char microcode[32];
    char vendor[32];
    int  cores;
    int  threads;
    int  has_vmx;
    int  has_svm;
    int  has_avx2;
    int  has_avx512;
} CPUInfo;

static void detect_cpu_full(CPUInfo *ci) {
    memset(ci, 0, sizeof(*ci));
    FILE *fp = popen("lscpu 2>/dev/null", "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        if      (strstr(line, "GenuineIntel"))     { strncpy(ci->microcode,"intel-ucode",31); strncpy(ci->vendor,"Intel",31); }
        else if (strstr(line, "AuthenticAMD"))     { strncpy(ci->microcode,"amd-ucode",31);   strncpy(ci->vendor,"AMD",31);   }
        else if (strncmp(line,"CPU(s):",7)==0)     { sscanf(line+7," %d",&ci->threads); }
        else if (strstr(line,"Core(s) per socket")){ char *p=strchr(line,':'); if(p) sscanf(p+1," %d",&ci->cores); }
        else if (strstr(line,"Flags:")) {
            if (strstr(line," vmx "))    ci->has_vmx  = 1;
            if (strstr(line," svm "))    ci->has_svm  = 1;
            if (strstr(line," avx2 "))   ci->has_avx2 = 1;
            if (strstr(line," avx512f")) ci->has_avx512 = 1;
        }
    }
    pclose(fp);
    if (ci->cores < 1)   ci->cores   = 1;
    if (ci->threads < 1) ci->threads = ci->cores;
}

static void detect_cpu(char *out, size_t sz) {
    out[0]='\0';
    FILE *fp = popen("lscpu 2>/dev/null","r");
    if (!fp) return;
    char line[256];
    while (fgets(line,sizeof(line),fp)) {
        if (strstr(line,"GenuineIntel")) { strncpy(out,"intel-ucode",sz-1); break; }
        if (strstr(line,"AuthenticAMD")) { strncpy(out,"amd-ucode",sz-1);   break; }
    }
    pclose(fp);
}

static double measure_mirror_speed(const char *url) {
    char cmd[512];
    snprintf(cmd,sizeof(cmd),
             "curl -o /dev/null -s --max-time 5 -w '%%{speed_download}' '%s' 2>/dev/null",
             url);
    FILE *fp = popen(cmd,"r");
    if (!fp) return 0.0;
    double speed = 0.0;
    (void)fscanf(fp,"%lf",&speed);
    pclose(fp);
    return speed;
}

static int is_uefi(void) {
    struct stat st2;
    return stat("/sys/firmware/efi",&st2)==0;
}

static int is_ssd(const char *disk_path) {
    const char *name = strrchr(disk_path,'/');
    name = name ? name+1 : disk_path;
    char block[64]; strncpy(block,name,sizeof(block)-1); block[63]='\0';
    if (strstr(block,"nvme") || strstr(block,"mmcblk")) {
        char *p = block+strlen(block)-1;
        while (p>block && isdigit((unsigned char)*p)) p--;
        if (*p=='p') *p='\0';
    } else {
        char *p = block+strlen(block)-1;
        while (p>block && isdigit((unsigned char)*p)) *p--='\0';
    }
    char rot[256];
    snprintf(rot,sizeof(rot),"/sys/block/%s/queue/rotational",block);
    FILE *f = fopen(rot,"r");
    if (!f) return 0;
    char c[4]={0}; (void)fread(c,1,3,f); fclose(f);
    return c[0]=='0';
}

static int check_connectivity(void);

static int run_preflight(void) {
    char report[2048] = {0};
    int ok = 1;

    if (geteuid() != 0) {
        strncat(report, L("Not running as root\n",
                          "No se esta ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("Running as root\n","Ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (!check_connectivity()) {
        strncat(report, L("No internet connection\n",
                          "Sin conexion a internet\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("Internet connection OK\n","Conexion a internet OK\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("which pacstrap >/dev/null 2>&1") != 0) {
        strncat(report, L("pacstrap not found (are you in the Arch ISO?)\n",
                          "pacstrap no encontrado (estas en la ISO de Arch?)\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("pacstrap found\n","pacstrap encontrado\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("mountpoint -q /mnt 2>/dev/null") == 0) {
        strncat(report, L("/mnt is already mounted (may conflict)\n",
                          "/mnt ya esta montado (puede haber conflicto)\n"),
                sizeof(report)-strlen(report)-1);
    } else {
        strncat(report, L("/mnt is free\n","/mnt libre\n"),
                sizeof(report)-strlen(report)-1);
    }

    st.laptop = is_laptop();
    if (st.laptop) {
        strncat(report, L("Laptop detected (will install TLP power management)\n",
                          "Laptop detectada (se instalara gestion de energia TLP)\n"),
                sizeof(report)-strlen(report)-1);
    }

    {
        struct statvfs vfs;
        if (statvfs("/", &vfs) == 0) {
            long long free_mb = ((long long)vfs.f_bavail * vfs.f_frsize) / (1024*1024);
            char line[128];
            snprintf(line,sizeof(line),
                     L("Installer free space: %lld MB\n",
                       "Espacio libre en instalador: %lld MB\n"), free_mb);
            strncat(report,line,sizeof(report)-strlen(report)-1);
        }
    }

    if (!ok) {
        char msg[2560];
        snprintf(msg,sizeof(msg),
                 L("Preflight checks failed:\n\n%s\nFix the issues above before continuing.",
                   "Comprobaciones previas fallidas:\n\n%s\nCorrige los problemas antes de continuar."),
                 report);
        msgbox(L("Preflight Failed","Comprobacion previa fallida"), msg);
        return 0;
    }

    char msg[2560];
    snprintf(msg,sizeof(msg),
             L("System checks passed:\n\n%s\nReady to install.",
               "Comprobaciones del sistema OK:\n\n%s\nListo para instalar."),
             report);
    msgbox(L("Preflight OK","Comprobacion previa OK"), msg);
    return 1;
}

static void detect_gpu(char *out, size_t sz) {
    FILE *fp = popen("lspci 2>/dev/null | grep -iE 'vga|3d|display'","r");
    char buf[4096]={0};
    if (fp) { (void)fread(buf,1,sizeof(buf)-1,fp); pclose(fp); }
    for (char *p=buf; *p; p++) *p=tolower((unsigned char)*p);
    int nv = strstr(buf,"nvidia")!=NULL;
    int am = (strstr(buf,"amd")!=NULL)||(strstr(buf,"radeon")!=NULL);
    int in = strstr(buf,"intel")!=NULL;
    if      (in && nv) strncpy(out,"Intel+NVIDIA",sz-1);
    else if (in && am) strncpy(out,"Intel+AMD",sz-1);
    else if (nv)       strncpy(out,"NVIDIA",sz-1);
    else if (am)       strncpy(out,"AMD",sz-1);
    else if (in)       strncpy(out,"Intel",sz-1);
    else               strncpy(out,"None",sz-1);
    out[sz-1]='\0';
}

static int suggest_swap_gb(void) {
    FILE *f = fopen("/proc/meminfo","r");
    if (!f) return 8;
    char line[256]; long kb=0;
    while (fgets(line,sizeof(line),f)) {
        if (strncmp(line,"MemTotal:",9)==0) { sscanf(line+9,"%ld",&kb); break; }
    }
    fclose(f);
    int ram = (int)(kb/(1024*1024));
    if (ram<=2) return 4;
    if (ram<=8) return ram;
    return 8;
}

typedef struct { char name[64]; long long size_gb; char model[128]; } DiskInfo;

static int list_disks(DiskInfo *out, int max) {
    FILE *fp = popen("lsblk -b -d -o NAME,SIZE,MODEL | tail -n +2","r");
    if (!fp) return 0;
    int n=0; char line[256];
    while (n<max && fgets(line,sizeof(line),fp)) {
        trim_nl(line);
        char name[64]={0}; long long sz=0; char model[128]={0};
        int r = sscanf(line,"%63s %lld %127[^\n]",name,&sz,model);
        if (r<2) continue;
        strncpy(out[n].name,name,sizeof(out[n].name)-1);
        out[n].size_gb = sz/(1024LL*1024*1024);
        strncpy(out[n].model,model[0]?model:"Unknown",sizeof(out[n].model)-1);
        n++;
    }
    pclose(fp);
    return n;
}

typedef struct {
    char path[128];
    char fstype[32];
    char label[64];
    long long size_mb;
} PartEntry;

static int list_all_partitions(PartEntry *out, int max) {
    FILE *fp = popen(
        "lsblk -b -p -n -o PATH,SIZE,FSTYPE,LABEL,TYPE --pairs 2>/dev/null", "r");
    if (!fp) return 0;
    int n = 0;
    char line[512];
    while (n < max && fgets(line, sizeof(line), fp)) {
        char path[128]={0}, fstype[64]={0}, label[64]={0}, type[16]={0};
        long long sz = 0;
        char *p;
        if ((p = strstr(line, "PATH=\"")))   sscanf(p+6,  "%127[^\"]", path);
        if ((p = strstr(line, "SIZE=\"")))   sscanf(p+6,  "%lld",      &sz);
        if ((p = strstr(line, "FSTYPE=\""))) sscanf(p+8,  "%63[^\"]",  fstype);
        if ((p = strstr(line, "LABEL=\"")))  sscanf(p+7,  "%63[^\"]",  label);
        if ((p = strstr(line, "TYPE=\"")))   sscanf(p+6,  "%15[^\"]",  type);
        if (strcmp(type, "part") != 0) continue;
        if (!path[0]) continue;
        strncpy(out[n].path,   path,                     sizeof(out[n].path)-1);
        out[n].size_mb = sz / (1024LL * 1024);
        strncpy(out[n].fstype, fstype[0] ? fstype : "?", sizeof(out[n].fstype)-1);
        strncpy(out[n].label,  label,                    sizeof(out[n].label)-1);
        n++;
    }
    pclose(fp);
    return n;
}

static int list_partitions_on_disk(const char *disk, PartEntry *out, int max) {

    char prefix[128];
    int nvme = (strstr(disk,"nvme") || strstr(disk,"mmcblk")) ? 1 : 0;
    snprintf(prefix, sizeof(prefix), "%s%s", disk, nvme ? "p" : "");

    char cmd[256];
    snprintf(cmd, sizeof(cmd),
             "lsblk -b -p -n -l -o NAME,SIZE,FSTYPE,LABEL,TYPE %s 2>/dev/null", disk);
    FILE *fp = popen(cmd, "r");
    int n = 0;
    if (fp) {
        char line[512];
        while (n < max && fgets(line, sizeof(line), fp)) {
            char name[128]={0}, fstype[64]={0}, label[64]={0}, type[16]={0};
            long long sz = 0;

            int r = sscanf(line, "%127s %lld %63s %63s %15s",
                           name, &sz, fstype, label, type);
            if (r < 2) continue;

            int is_part = 0;
            if (r >= 5 && strcmp(type,"part")==0) is_part=1;
            else if (r >= 5 && strcmp(type,"disk")==0) is_part=0;
            else if (strcmp(name, disk) != 0) is_part=1;

            if (!is_part) continue;
            if (!name[0]) continue;

            char real_fs[64]="?", real_lbl[64]="";
            if (r >= 3) strncpy(real_fs, fstype, 63);
            if (r >= 4 && r < 5) strncpy(real_lbl, label, 63);
            if (r >= 5) strncpy(real_lbl, label, 63);

            strncpy(out[n].path,   name,                         sizeof(out[n].path)-1);
            out[n].size_mb = sz / (1024LL * 1024);
            strncpy(out[n].fstype, real_fs[0] ? real_fs : "?",   sizeof(out[n].fstype)-1);
            strncpy(out[n].label,  real_lbl,                      sizeof(out[n].label)-1);
            n++;
        }
        pclose(fp);
    }

    if (n == 0) {
        PartEntry all[64]; int na = list_all_partitions(all, 64);
        for (int i = 0; i < na && n < max; i++) {
            if (strncmp(all[i].path, disk, strlen(disk)) == 0) {
                out[n++] = all[i];
            }
        }
    }

    return n;
}

static void partition_paths(const char *disk, char *p1, char *p2, char *p3, size_t sz) {
    const char *sep = (strstr(disk,"nvme")||strstr(disk,"mmcblk")) ? "p" : "";
    snprintf(p1,sz,"%s%s1",disk,sep);
    snprintf(p2,sz,"%s%s2",disk,sep);
    snprintf(p3,sz,"%s%s3",disk,sep);
}

static void settle_partitions(const char *disk) {
    char cmd[MAX_CMD];
    snprintf(cmd,sizeof(cmd),"partprobe %s",disk);
    run_simple(cmd,1);
    run_simple("udevadm settle --timeout=10",1);
    sleep(1);
}

static int wifi_interfaces(char ifaces[][64], int max) {
    int n=0;
    FILE *fp = popen("ls /sys/class/net/","r");
    if (!fp) return 0;
    char name[64];
    while (n<max && fscanf(fp,"%63s",name)==1)
        if (!strncmp(name,"wlan",4)||!strncmp(name,"wlp",3)||!strncmp(name,"wlo",3))
            strncpy(ifaces[n++],name,63);
    pclose(fp);
    return n;
}

static int check_connectivity(void) {
    const char *cmds[] = {
        "curl -sI --max-time 5 https://archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 archlinux.org >/dev/null 2>&1",
        "ping -c1 -W3 8.8.8.8 >/dev/null 2>&1",
        NULL
    };
    for (int i=0; cmds[i]; i++)
        if (system(cmds[i])==0) return 1;
    return 0;
}

static int screen_wifi_connect(void) {
    char ifaces[4][64];
    int ni = wifi_interfaces(ifaces,4);
    if (ni==0) {
        msgbox(L("WiFi","WiFi"),
               L("No wireless interfaces found.\n\nMake sure your WiFi adapter is recognized.",
                 "No se encontraron interfaces inalámbricas.\n\nVerifica que tu adaptador WiFi sea reconocido."));
        return -1;
    }
    const char *iface = ifaces[0];

rescan_wifi:;

    {
        char info_msg[256];
        snprintf(info_msg,sizeof(info_msg),
                 L("Scanning for networks on %s...\nThis may take up to 12 seconds.",
                   "Buscando redes en %s...\nEsto puede tardar hasta 12 segundos."), iface);
        infobox_dlg(L(" Scanning..."," Escaneando..."), info_msg);
    }

    char scan_cmd[256];
    snprintf(scan_cmd,sizeof(scan_cmd),"iwctl station '%s' scan 2>/dev/null",iface);    (void)system(scan_cmd);

    char ssids[16][128]; int nssids=0;
    time_t deadline = time(NULL)+12;
    while (time(NULL)<deadline && nssids==0) {
        sleep(2);
        char get_cmd[256];
        snprintf(get_cmd,sizeof(get_cmd),"iwctl station '%s' get-networks 2>/dev/null",iface);
        FILE *fp = popen(get_cmd,"r");
        if (!fp) continue;
        char line[512];
        int skip=2;
        while (fgets(line,sizeof(line),fp)) {
            if (skip>0) { skip--; continue; }
            char clean[512]; strip_ansi(line,clean,sizeof(clean));
            trim_nl(clean);
            char *p = clean;
            while (*p==' '||*p=='>'||*p=='\t') p++;
            if (!*p) continue;
            if (*p=='-'||*p=='='||*p=='*') continue;
            if (strncasecmp(p,"Network",7)==0) continue;
            char ssid[128]={0}; sscanf(p,"%127s",ssid);
            if (!ssid[0]) continue;
            int dup=0;
            for (int i=0;i<nssids;i++) if (!strcmp(ssids[i],ssid)){dup=1;break;}
            if (!dup && nssids<15) strncpy(ssids[nssids++],ssid,127);
        }
        pclose(fp);
    }

    typedef struct { char ssid[128]; int signal; char security[32]; } NetEntry;
    NetEntry nets[16]; int nnets = 0;

    if (nssids > 0) {
        char scan_raw[8192]={0};
        char scan_cmd2[256];
        snprintf(scan_cmd2,sizeof(scan_cmd2),
                 "iw dev '%s' scan 2>/dev/null || "
                 "iwctl station '%s' get-networks 2>/dev/null", iface, iface);        FILE *fp2 = popen(scan_cmd2,"r");
        if (fp2) { (void)fread(scan_raw,1,sizeof(scan_raw)-1,fp2); pclose(fp2); }

        for (int i=0; i<nssids && nnets<15; i++) {
            strncpy(nets[nnets].ssid, ssids[i], 127);
            nets[nnets].signal   = -100;
            nets[nnets].security[0] = '\0';

            char *pos = scan_raw;
            while ((pos = strstr(pos, ssids[i])) != NULL) {
                char ctx[512]={0};
                char *end_p = strchr(pos, '\n');
                if (!end_p) end_p = pos + strlen(pos);
                int range = (int)(end_p - pos) + 300;
                if (range > (int)(sizeof(scan_raw) - (pos - scan_raw)))
                    range = (int)(sizeof(scan_raw) - (pos - scan_raw));
                strncpy(ctx, pos, range < (int)sizeof(ctx)-1 ? range : (int)sizeof(ctx)-1);

                char *sp;
                if ((sp = strstr(ctx,"signal:")) || (sp = strstr(ctx,"Signal:"))) {
                    float sv=0; sscanf(sp+7,"%f",&sv);
                    int pct = (int)((sv + 100) * 2);
                    if (pct > 100) pct = 100;
                    if (pct < 0)   pct = 0;
                    nets[nnets].signal = pct;
                }
                if ((sp = strstr(ctx,"WPA3"))) strncpy(nets[nnets].security,"WPA3",31);
                else if ((sp = strstr(ctx,"WPA2"))) strncpy(nets[nnets].security,"WPA2",31);
                else if ((sp = strstr(ctx,"WPA")))  strncpy(nets[nnets].security,"WPA",31);
                else if ((sp = strstr(ctx,"WEP")))  strncpy(nets[nnets].security,"WEP",31);
                else strncpy(nets[nnets].security,"Open",31);
                break;
            }
            nnets++;
        }

        for (int i=0; i<nnets-1; i++)
            for (int j=i+1; j<nnets; j++)
                if (nets[j].signal > nets[i].signal) {
                    NetEntry tmp = nets[i]; nets[i] = nets[j]; nets[j] = tmp;
                }
    }

    char filter[64]={0};
    {
        char filter_hdr[512];
        snprintf(filter_hdr,sizeof(filter_hdr),
            L("Found %d network(s) on interface %s.\n\n"
              "Type part of a network name to filter the list,\n"
              "or leave blank to show all networks:",
              "Se encontraron %d red(es) en la interfaz %s.\n\n"
              "Escribe parte del nombre de una red para filtrar la lista,\n"
              "o deja en blanco para mostrar todas:"),
            nnets, iface);
        inputbox_dlg(L(" Filter networks"," Filtrar redes"),
                     filter_hdr, "", filter, sizeof(filter));

    }

    char ssid_sel[128]={0};
    int show_nets = 0;
    {
        MenuItem filt_items[16]; int nf = 0;
        for (int i=0; i<nnets && nf<15; i++) {
            if (filter[0] && strcasestr(nets[i].ssid, filter) == NULL) continue;
            strncpy(filt_items[nf].tag, nets[i].ssid, 255);
            char bar[6]="-----";
            int pct = nets[i].signal < 0 ? 0 : nets[i].signal;
            int filled = (pct * 5) / 100;
            for (int b=0; b<filled && b<5; b++) bar[b]='#';
            char sec[32]; strncpy(sec, nets[i].security[0]?nets[i].security:"?", 31);
            snprintf(filt_items[nf].desc, 511, "[%s] %3d%%  %-6s  %s",
                     bar, pct, sec, nets[i].ssid);
            nf++; show_nets++;
        }

        if (nf > 0) {
            char hdr[512];
            snprintf(hdr,sizeof(hdr),
                     L("Interface: %s%s\n"
                       "Sorted by signal strength.\n"
                       "Press  Home to rescan, Cancel to go back.",
                       "Interfaz: %s%s\n"
                       "Ordenado por señal.\n"
                       "Pulsa  Home para reescanear, Cancelar para volver."),
                     iface, filter[0]?" (filtered)":"");

            int rc = radiolist_dlg(L(" WiFi Networks"," Redes WiFi"), hdr,
                                   filt_items, nf, NULL, ssid_sel, sizeof(ssid_sel));
            if (!rc) {
                if (g_home_requested) {
                    g_home_requested = 0;
                    goto rescan_wifi;
                }
                return -1;
            }
        } else {

            char hdr[512];
            snprintf(hdr,sizeof(hdr),
                     L("Interface: %s\nNo networks found%s.\nEnter SSID manually or Cancel:",
                       "Interfaz: %s\nNo se hallaron redes%s.\nIntroduce el SSID manualmente o Cancela:"),
                     iface, filter[0]?" matching filter":"");
            if (!inputbox_dlg(L(" WiFi — SSID"," WiFi — SSID"),hdr,"",ssid_sel,sizeof(ssid_sel))) {
                if (g_home_requested) { g_home_requested=0; goto rescan_wifi; }
                return -1;
            }
        }
    }
    (void)show_nets;
    if (!ssid_sel[0]) return -1;

    char pass[256]={0};
    char pass_hdr[512];
    snprintf(pass_hdr,sizeof(pass_hdr),
             L(" Password for '%s'\n(leave blank if the network is open, Cancel to go back):",
               " Contraseña de '%s'\n(deja vacío si la red es abierta, Cancelar para volver):"),
             ssid_sel);
    if (!passwordbox_dlg(L(" WiFi Password"," Contraseña WiFi"),pass_hdr,pass,sizeof(pass))) {
        if (g_home_requested) { g_home_requested=0; goto rescan_wifi; }
        return -1;
    }

    char conn_msg[256];
    snprintf(conn_msg,sizeof(conn_msg),
             L("Connecting to '%s'...","Conectando a '%s'..."),ssid_sel);
    infobox_dlg(L("Connecting...","Conectando..."),conn_msg);

    char q_ssid[256], q_pass[256], q_iface[128];
    shell_quote(ssid_sel, q_ssid, sizeof(q_ssid));
    shell_quote(iface,    q_iface, sizeof(q_iface));
    char cmd[MAX_CMD];
    if (pass[0]) {
        shell_quote(pass, q_pass, sizeof(q_pass));
        snprintf(cmd,sizeof(cmd),"iwctl --passphrase %s station %s connect %s",
                 q_pass, q_iface, q_ssid);
    } else {
        snprintf(cmd,sizeof(cmd),"iwctl station %s connect %s", q_iface, q_ssid);
    }
    (void)system(cmd);
    sleep(5);

    if (!check_connectivity()) {
        char fail_msg[512];
        snprintf(fail_msg,sizeof(fail_msg),
                 L(" Could not connect to '%s'.\n\nPossible causes:\n"
                   "- Wrong password\n  - Network out of range\n  - DHCP not responding\n\n"
                   "Press OK to try again or Cancel to go back.",
                   " No se pudo conectar a '%s'.\n\nPosibles causas:\n"
                   "- Contraseña incorrecta\n  - Red fuera de alcance\n  - DHCP sin respuesta\n\n"
                   "Pulsa OK para intentar de nuevo o Cancelar para volver."),
                 ssid_sel);
        if (yesno_dlg(L(" WiFi Failed"," WiFi fallido"),fail_msg))
            goto rescan_wifi;
        return 0;
    }
    return 1;
}

static void screen_network(void) {
    while (1) {
        MenuItem items[2];
        strncpy(items[0].tag,"wired",255);
        snprintf(items[0].desc,511,"%s",
            L(" Wired (Ethernet)   — cable already plugged in",
              " Cable (Ethernet)   — cable ya conectado"));
        strncpy(items[1].tag,"wifi",255);
        snprintf(items[1].desc,511,"%s",
            L(" WiFi               — connect to a wireless network",
              " WiFi               — conectar a una red inalámbrica"));

        char choice[64]={0};
        if (!menu_dlg(L(" Network Connection"," Conexión de red"),
                      L("An internet connection is needed to download Arch Linux.\n\n"
                        "How are you connected to the internet?",
                        "Se necesita internet para descargar Arch Linux.\n\n"
                        "¿Cómo estás conectado a internet?"),
                      items,2,choice,sizeof(choice))) {
            if (g_home_requested) { g_home_requested=0; }
            if (yesno_dlg(L("Exit","Salir"),L("Exit the installer?","¿Salir del instalador?")))
                exit(0);
            continue;
        }

        if (!strcmp(choice,"wired")) {

            while (1) {
                infobox_dlg(L(" Checking..."," Verificando..."),
                            L("Testing wired connection...","Probando conexión por cable..."));
                if (check_connectivity()) {
                    msgbox(L(" Connected!"," ¡Conectado!"),
                           L("Wired connection detected. Ready to continue.",
                             "Conexión por cable detectada. Listo para continuar."));
                    return;
                }

                if (!yesno_dlg(
                        L(" No connection"," Sin conexión"),
                        L("Could not reach the internet over the wired connection.\n\n"
                          "Check:\n"
                          "- Is the network cable plugged in?\n"
                          "- Is the router/switch turned on?\n"
                          "- Did your router assign an IP? (try: dhclient eth0)\n\n"
                          "Try again now?",
                          "No se pudo alcanzar internet por cable.\n\n"
                          "Comprueba:\n"
                          "- ¿Está el cable de red conectado?\n"
                          "- ¿Está encendido el router/switch?\n"
                          "- ¿Tu router asignó una IP? (prueba: dhclient eth0)\n\n"
                          "¿Reintentar ahora?")))
                    break;
            }
            continue;
        }
        if (!strcmp(choice,"wifi")) {
            int r = screen_wifi_connect();
            if (r==1) {
                msgbox(L("Connected!","Conectado!"),
                       L("WiFi connected. Ready to continue.",
                         "WiFi conectado. Listo para continuar."));
                return;
            }
        }
    }
}

static int ensure_network(void) {
    if (check_connectivity()) return 1;
    const char *tools[] = {"dhcpcd","dhclient",NULL};
    for (int i=0; tools[i]; i++) {
        char p[128];
        snprintf(p,sizeof(p),"which %s >/dev/null 2>&1",tools[i]);
        if (system(p)==0) {
            char cmd[128]; snprintf(cmd,sizeof(cmd),"%s >/dev/null 2>&1",tools[i]);
            (void)system(cmd); sleep(3);
            if (check_connectivity()) return 1;
        }
    }
    char ifaces[4][64];
    if (wifi_interfaces(ifaces,4)>0 && system("which iwctl >/dev/null 2>&1")==0) {
        if (yesno_dlg(L("No network detected","Sin red detectada"),
                      L("No wired connection found.\nConnect via WiFi?",
                        "No se detecto conexion cableada.\nConectar por WiFi?")))
            return screen_wifi_connect()==1;
    }
    return 0;
}


typedef struct {
    void (*on_progress)(double pct, void *ud);
    void (*on_stage)(const char *msg, void *ud);
    void (*on_done)(int ok, const char *reason, void *ud);
    void  *ud;
    double progress;
    int    had_error;
    char   first_error[1024];
    pthread_mutex_t lock;
} IB;

static regex_t         g_re_install;
static regex_t         g_re_download;
static pthread_once_t  g_re_once  = PTHREAD_ONCE_INIT;

static void compile_regexes_impl(void) {
    regcomp(&g_re_install,  "\\(([0-9]+)/([0-9]+)\\)", REG_EXTENDED);
    regcomp(&g_re_download,
            "[^[:space:]]+ +[0-9]+\\.?[0-9]* +(B|KiB|MiB|GiB)"
            " +[0-9]+\\.?[0-9]* +(B|KiB|MiB|GiB)/s", REG_EXTENDED);
}

static void compile_regexes(void) {
    pthread_once(&g_re_once, compile_regexes_impl);
}

static void ib_pct(IB *ib, double p) {
    pthread_mutex_lock(&ib->lock);
    double clamped = p < 0.0 ? 0.0 : (p > 100.0 ? 100.0 : p);
    if (clamped > ib->progress) ib->progress = clamped;
    double cur = ib->progress;
    pthread_mutex_unlock(&ib->lock);
    ib->on_progress(cur, ib->ud);
}

static void ib_gradual(IB *ib, double target, int steps, double delay_sec) {
    pthread_mutex_lock(&ib->lock);
    double base = ib->progress;
    pthread_mutex_unlock(&ib->lock);
    for (int i=1; i<=steps; i++) {
        ib_pct(ib, base + (target-base)*((double)i/steps));
        usleep((useconds_t)(delay_sec*1e6));
    }
}

static void ib_stage(IB *ib, const char *msg) {
    write_log_fmt(">>> %s", msg);
    ib->on_stage(msg, ib->ud);
}

static void ib_note_error(IB *ib, const char *cmd, int rc) {
    if (!ib || rc == 0) return;
    pthread_mutex_lock(&ib->lock);
    if (!ib->had_error) {
        snprintf(ib->first_error, sizeof(ib->first_error),
                 "%s failed (rc=%d). Check %s.", cmd, rc, LOG_FILE);
    }
    ib->had_error = 1;
    pthread_mutex_unlock(&ib->lock);
}

typedef struct {
    IB    *ib;
    double start, end;
    int    download_done;
} PacmanCbS;

static void pacman_cb(const char *line, void *ud) {
    PacmanCbS *ps = ud;
    regmatch_t m[3];
    if (regexec(&g_re_install, line, 3, m, 0)==0) {
        ps->download_done = 1;
        char ns[16]={0}, ts[16]={0};
        int len1 = (int)(m[1].rm_eo-m[1].rm_so); if(len1>15)len1=15;
        int len2 = (int)(m[2].rm_eo-m[2].rm_so); if(len2>15)len2=15;
        strncpy(ns, line+m[1].rm_so, len1);
        strncpy(ts, line+m[2].rm_so, len2);
        int cur=atoi(ns), tot=atoi(ts);
        double half = ps->start + (ps->end-ps->start)*0.5;
        if (tot>0) ib_pct(ps->ib, half + ((double)cur/tot)*(ps->end-half));
        return;
    }
    if (!ps->download_done && regexec(&g_re_download,line,0,NULL,0)==0) {
        double cap = ps->start + (ps->end-ps->start)*0.45;
        pthread_mutex_lock(&ps->ib->lock);
        double cur = ps->ib->progress;
        pthread_mutex_unlock(&ps->ib->lock);
        if (cur < cap) ib_pct(ps->ib, cur+0.3);
    }
}

static int ib_pacman(IB *ib, const char *cmd, double start, double end, int ignore_error) {
    PacmanCbS ps = {ib, start, end, 0};
    int rc = run_stream(cmd, pacman_cb, &ps, ignore_error);
    if (!ignore_error) ib_note_error(ib, cmd, rc);
    ib_pct(ib, end);
    return rc;
}

static void ib_pacman_critical(IB *ib, const char *cmd,
                                double start, double end, const char *label) {
    int rc = ib_pacman(ib,cmd,start,end,0);
    if (rc!=0) {
        char msg[512];
        snprintf(msg,sizeof(msg),
                 L("%s failed (rc=%d). Check %s.",
                   "%s fallo (rc=%d). Revisa %s."), label, rc, LOG_FILE);
        ib->on_done(0, msg, ib->ud);
        pthread_exit(NULL);
    }
}

static void ib_run(IB *ib, const char *cmd, const char *label) {
    int rc = run_stream(cmd, NULL, NULL, 0);
    ib_note_error(ib, cmd, rc);
    if (rc!=0) {
        char msg[512];
        snprintf(msg,sizeof(msg),
                 L("%s failed (rc=%d). Check %s.",
                   "%s fallo (rc=%d). Revisa %s."), label, rc, LOG_FILE);
        ib->on_done(0, msg, ib->ud);
        pthread_exit(NULL);
    }
}

static int ib_chroot(IB *ib, const char *cmd, int ignore_error) {
    char q[MAX_CMD], full[MAX_CMD];
    shell_quote(cmd,q,sizeof(q));
    snprintf(full,sizeof(full),"arch-chroot /mnt /bin/bash -c %s",q);
    int rc = run_stream(full, NULL, NULL, ignore_error);
    if (!ignore_error) ib_note_error(ib, full, rc);
    return rc;
}

static void ib_chroot_c(IB *ib, const char *cmd, const char *label) {
    int rc = ib_chroot(ib,cmd,0);
    ib_note_error(ib, cmd, rc);
    if (rc!=0) {
        char msg[512];
        snprintf(msg,sizeof(msg),
                 L("%s failed (rc=%d). Check %s.",
                   "%s fallo (rc=%d). Revisa %s."), label, rc, LOG_FILE);
        ib->on_done(0, msg, ib->ud);
        pthread_exit(NULL);
    }
}

static void ib_chroot_passwd(IB *ib, const char *user, const char *pwd) {
    char entry[512], q[MAX_CMD], cmd[MAX_CMD];
    snprintf(entry,sizeof(entry),"%s:%s",user,pwd);
    shell_quote(entry,q,sizeof(q));
    snprintf(cmd,sizeof(cmd),
             "printf '%%s\\n' %s | arch-chroot /mnt chpasswd",q);
    run_stream(cmd,NULL,NULL,1);
}

static void ib_setup_btrfs(IB *ib, const char *p3, const char *disk) {
    char opts[256]; strcpy(opts,"noatime,compress=zstd,space_cache=v2");
    if (is_ssd(disk)) {
        strncat(opts,",ssd,discard=async",sizeof(opts)-strlen(opts)-1);
        write_log_fmt("SSD detected on %s - adding ssd,discard=async",disk);
    }
    char cmd[MAX_CMD];
    run_simple("modprobe btrfs", 1);
    snprintf(cmd,sizeof(cmd),"mkfs.btrfs -f %s",p3); ib_run(ib,cmd,"mkfs.btrfs");
    run_simple("udevadm settle --timeout=5", 1);
    snprintf(cmd,sizeof(cmd),"mount -t btrfs %s /mnt",p3); ib_run(ib,cmd,"mount btrfs");
    ib_run(ib,"btrfs subvolume create /mnt/@",          "btrfs subvol @");
    ib_run(ib,"btrfs subvolume create /mnt/@home",      "btrfs subvol @home");
    ib_run(ib,"btrfs subvolume create /mnt/@var",       "btrfs subvol @var");
    ib_run(ib,"btrfs subvolume create /mnt/@snapshots", "btrfs subvol @snapshots");
    ib_run(ib,"umount /mnt",                            "umount btrfs");

    snprintf(cmd,sizeof(cmd),"mount -o %s,subvol=@ %s /mnt",opts,p3);
    ib_run(ib,cmd,"mount @");
    run_simple("mkdir -p /mnt/home",      0);
    run_simple("mkdir -p /mnt/var",       0);
    run_simple("mkdir -p /mnt/.snapshots",0);
    snprintf(cmd,sizeof(cmd),"mount -o %s,subvol=@home %s /mnt/home",opts,p3);
    ib_run(ib,cmd,"mount @home");
    snprintf(cmd,sizeof(cmd),"mount -o %s,subvol=@var %s /mnt/var",opts,p3);
    ib_run(ib,cmd,"mount @var");
    snprintf(cmd,sizeof(cmd),"mount -o %s,subvol=@snapshots %s /mnt/.snapshots",opts,p3);
    ib_run(ib,cmd,"mount @snapshots");
}

static void ib_setup_xfs(IB *ib, const char *part) {
    char cmd[MAX_CMD];
    snprintf(cmd, sizeof(cmd), "mkfs.xfs -f %s", part);
    ib_run(ib, cmd, "mkfs.xfs");
    run_simple("udevadm settle --timeout=10", 1);
    snprintf(cmd, sizeof(cmd), "mount %s /mnt", part);
    ib_run(ib, cmd, "mount xfs root");
}

static void ib_add_archzfs_repo(int in_chroot) {
    const char *block =
        "\n[archzfs]\n"
        "SigLevel = Optional TrustAll\n"
        "Server = https://archzfs.com/$repo/$arch\n";
    const char *conf = in_chroot ? "/mnt/etc/pacman.conf" : "/etc/pacman.conf";
    char q[MAX_CMD], cmd[MAX_CMD];
    shell_quote(block, q, sizeof(q));
    snprintf(cmd, sizeof(cmd),
             "grep -q '\\[archzfs\\]' %s || printf %%s %s >> %s",
             conf, q, conf);
    run_stream(cmd, NULL, NULL, 1);
    if (!in_chroot) {
        run_stream("pacman -Sy --noconfirm", NULL, NULL, 1);
    }
    write_log_fmt("archzfs repo added to %s", conf);
}

static void ib_setup_zfs(IB *ib, const char *part) {
    char cmd[MAX_CMD];

    ib_add_archzfs_repo(0);
    {
        int rc = run_stream(
            "pacman -S --noconfirm --needed zfs-linux zfs-utils",
            NULL, NULL, 1);
        if (rc != 0) {
            ib->on_done(0,
                "ZFS setup failed: zfs-linux does not match the running kernel.\n\n"
                "Solutions:\n"
                "1. Use a ZFS-enabled Arch ISO:\n"
                "https://archzfs.leibelt.de\n"
                "2. Wait for archzfs to release an updated zfs-linux.\n"
                "3. Choose a different filesystem (ext4, btrfs, xfs).",
                ib->ud);
            pthread_exit(NULL);
        }
    }
    run_stream("modprobe zfs 2>/dev/null || true", NULL, NULL, 1);

    snprintf(cmd, sizeof(cmd), "zpool labelclear -f %s 2>/dev/null || true", part);
    run_stream(cmd, NULL, NULL, 1);

    snprintf(cmd, sizeof(cmd),
        "zpool create -f "
        "-o ashift=12 "
        "-o autotrim=on "
        "-O acltype=posixacl "
        "-O xattr=sa "
        "-O dnodesize=auto "
        "-O compression=lz4 "
        "-O normalization=formD "
        "-O relatime=on "
        "-O canmount=off "
        "-O mountpoint=none "
        "-R /mnt "
        "zroot %s", part);
    ib_run(ib, cmd, "zpool create");

    ib_run(ib, "zfs create -o canmount=off   -o mountpoint=none zroot/data",      "zfs create data");
    ib_run(ib, "zfs create -o canmount=noauto -o mountpoint=/ zroot/data/root",   "zfs create root");
    ib_run(ib, "zfs create -o mountpoint=/home zroot/data/home",                  "zfs create home");
    ib_run(ib, "zfs create -o canmount=off  -o mountpoint=/var zroot/data/var",   "zfs create var");
    ib_run(ib, "zfs create -o mountpoint=none zroot/data/var/lib",                "zfs create var/lib");
    ib_run(ib, "zfs create -o mountpoint=none zroot/data/var/log",                "zfs create var/log");

    ib_run(ib, "zpool set bootfs=zroot/data/root zroot", "zpool set bootfs");

    run_simple("mkdir -p /etc/zfs", 0);
    ib_run(ib, "zpool set cachefile=/etc/zfs/zpool.cache zroot", "zpool set cachefile");

    ib_run(ib, "zpool export zroot", "zpool export");
    ib_run(ib, "zpool import -d /dev -R /mnt zroot", "zpool import");
    ib_run(ib, "zfs mount zroot/data/root", "zfs mount root");
    run_stream("zfs mount -a", NULL, NULL, 1);
    run_simple("mkdir -p /mnt/home /mnt/var/log /mnt/var/lib", 0);

    run_simple("mkdir -p /mnt/etc/zfs", 0);
    run_simple("cp /etc/zfs/zpool.cache /mnt/etc/zfs/zpool.cache 2>/dev/null || true", 0);

    write_log("ZFS pool 'zroot' created and mounted at /mnt");
}

static void ib_install_grub(IB *ib, const char *disk) {
    if (is_uefi()) {
        ib_chroot_c(ib,
            "grub-install --target=x86_64-efi "
            "--efi-directory=/boot/efi --bootloader-id=GRUB",
            "grub-install UEFI");
    } else {
        char cmd[256];
        snprintf(cmd,sizeof(cmd),"grub-install --target=i386-pc %s",disk);
        ib_chroot_c(ib,cmd,"grub-install BIOS");
    }
    ib_chroot_c(ib,"grub-mkconfig -o /boot/grub/grub.cfg","grub-mkconfig");
}

static void ib_install_systemd_boot(IB *ib, const char *root_dev) {
    ib_chroot_c(ib,"bootctl install","bootctl install");

    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    char partuuid[128]={0};
    char cmd[256];
    snprintf(cmd,sizeof(cmd),"blkid -s PARTUUID -o value %s 2>/dev/null",root_dev);
    FILE *fp = popen(cmd,"r");
    if (fp) { (void)fgets(partuuid,sizeof(partuuid),fp); pclose(fp); trim_nl(partuuid); }

    char root_opt[256];
    if (partuuid[0]) snprintf(root_opt,sizeof(root_opt),"root=PARTUUID=%s",partuuid);
    else             snprintf(root_opt,sizeof(root_opt),"root=%s",root_dev);

    char extra[64]={0};
    if (!strcmp(st.filesystem,"btrfs")) strcpy(extra,"rootflags=subvol=@ ");

    run_simple("mkdir -p /mnt/boot/loader",0);
    FILE *f = fopen("/mnt/boot/loader/loader.conf","w");
    if (f) {
        fprintf(f,"default arch.conf\ntimeout 4\nconsole-mode max\neditor no\n");
        fclose(f);
    }
    run_simple("mkdir -p /mnt/boot/loader/entries",0);

    char klist[512]; strncpy(klist, st.kernel_list, sizeof(klist)-1);
    char *tok = strtok(klist, " ");
    int kid = 0;
    while (tok) {
        char fname[64];
        snprintf(fname, sizeof(fname), "/mnt/boot/loader/entries/%s",
                 kid == 0 ? "arch.conf" : tok);
        if (kid > 0) {
            char tmp[80]; snprintf(tmp,sizeof(tmp),"/mnt/boot/loader/entries/arch-%s.conf",tok);
            strncpy(fname, tmp, sizeof(fname)-1);
        }
        f = fopen(fname, "w");
        if (f) {
            fprintf(f,"title   PulseOS (%s)\n", tok);
            fprintf(f,"linux   /vmlinuz-%s\n", tok);
            if (microcode[0]) fprintf(f,"initrd  /%s.img\n", microcode);
            fprintf(f,"initrd  /initramfs-%s.img\n", tok);
            fprintf(f,"options %s rw quiet %s\n", root_opt, extra);
            fclose(f);
        }
        tok = strtok(NULL, " ");
        kid++;
    }
    write_log("systemd-boot installed and configured.");
}

static void ib_install_limine(IB *ib, const char *disk, const char *root_dev) {
    int uefi = is_uefi();
    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    char partuuid[128]={0};
    char cmd[256];
    snprintf(cmd,sizeof(cmd),"blkid -s PARTUUID -o value %s 2>/dev/null",root_dev);
    FILE *fp = popen(cmd,"r");
    if (fp) { (void)fgets(partuuid,sizeof(partuuid),fp); pclose(fp); trim_nl(partuuid); }

    char root_opt[256];
    if (partuuid[0]) snprintf(root_opt,sizeof(root_opt),"root=PARTUUID=%s",partuuid);
    else             snprintf(root_opt,sizeof(root_opt),"root=%s",root_dev);

    char extra[64]={0};
    if (!strcmp(st.filesystem,"btrfs")) strcpy(extra,"rootflags=subvol=@ ");

    char btrfs_prefix[4] = "";
    if (!strcmp(st.filesystem,"btrfs")) strcpy(btrfs_prefix,"/@");

    run_simple("mkdir -p /mnt/boot/limine",0);
    FILE *f = fopen("/mnt/boot/limine.conf","w");
    if (f) {
        fprintf(f,"timeout: 5\n\n");
        char klist[512]; strncpy(klist, st.kernel_list, sizeof(klist)-1);
        char *tok = strtok(klist, " ");
        while (tok) {
            char kpath[512], ipath[512], ucode_line[512]={0};
            if (uefi) {
                snprintf(kpath,sizeof(kpath),"boot():/vmlinuz-%s",tok);
                snprintf(ipath,sizeof(ipath),"boot():/initramfs-%s.img",tok);
                if (microcode[0])
                    snprintf(ucode_line,sizeof(ucode_line),
                             "module_path: boot():/%s.img\n",microcode);
            } else {
                snprintf(kpath,sizeof(kpath),"boot():%s/boot/vmlinuz-%s",btrfs_prefix,tok);
                snprintf(ipath,sizeof(ipath),"boot():%s/boot/initramfs-%s.img",btrfs_prefix,tok);
                if (microcode[0])
                    snprintf(ucode_line,sizeof(ucode_line),
                             "module_path: boot():%s/boot/%s.img\n",btrfs_prefix,microcode);
            }
            fprintf(f,"/PulseOS (%s)\n    protocol: linux\n",tok);
            fprintf(f,"path: %s\n", kpath);
            fprintf(f,"cmdline: %s rw quiet %s\n", root_opt, extra);
            if (ucode_line[0]) fputs(ucode_line, f);
            fprintf(f,"module_path: %s\n\n", ipath);
            tok = strtok(NULL, " ");
        }
        fclose(f);
    }
    write_log("limine.conf written to /mnt/boot/limine.conf");

    if (uefi) {
        ib_chroot_c(ib,
            "mkdir -p /boot/EFI/limine && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/EFI/limine/",
            "limine copy EFI");
        ib_chroot(ib,
            "mkdir -p /boot/EFI/BOOT && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/EFI/BOOT/BOOTX64.EFI",1);

        char p1[256]; char p2[256]; char p3[256];
        char disk_dev[256]; snprintf(disk_dev,sizeof(disk_dev),"/dev/%s",st.disk);
        partition_paths(disk_dev,p1,p2,p3,sizeof(p1));

        char partn[8]="1";
        snprintf(cmd,sizeof(cmd),"lsblk -no PARTN %s 2>/dev/null",p1);
        fp = popen(cmd,"r"); if(fp){(void)fgets(partn,sizeof(partn),fp);pclose(fp);trim_nl(partn);}
        if (!partn[0]) strcpy(partn,"1");

        snprintf(cmd,sizeof(cmd),
                 "efibootmgr --create --disk %s --part %s "
                 "--label 'PulseOS' --loader '\\EFI\\limine\\BOOTX64.EFI' --unicode",
                 disk_dev, partn);
        run_stream(cmd,NULL,NULL,1);

        const char *hook =
            "[Trigger]\nOperation = Install\nOperation = Upgrade\n"
            "Type = Package\nTarget = limine\n\n"
            "[Action]\nDescription = Deploying Limine after upgrade...\n"
            "When = PostTransaction\n"
            "Exec = /bin/sh -c "
            "'cp /usr/share/limine/BOOTX64.EFI /boot/EFI/limine/ && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/EFI/BOOT/BOOTX64.EFI'\n";
        run_simple("mkdir -p /mnt/etc/pacman.d/hooks",0);
        fp = fopen("/mnt/etc/pacman.d/hooks/limine.hook","w");
        if (fp) { fputs(hook,fp); fclose(fp); }
        write_log("Limine UEFI fully configured.");
    } else {
        ib_chroot(ib,"cp /usr/share/limine/limine-bios.sys /boot/limine/",1);
        snprintf(cmd,sizeof(cmd),"limine bios-install %s",disk);
        int rc = run_stream(cmd,NULL,NULL,0);
        if (rc) write_log_fmt("limine bios-install rc=%d - check disk manually",rc);
        write_log("Limine BIOS installed.");
    }
}

static void ib_configure_nvidia_modeset(IB *ib) {
    ib_chroot(ib,
        "mkdir -p /etc/modprobe.d && "
        "echo 'options nvidia_drm modeset=1' > /etc/modprobe.d/nvidia.conf",0);
    write_log("nvidia_drm modeset=1 configured.");
}

static void ib_install_gpu(IB *ib, double start, double end) {
    const char *nv_pkg = "nvidia";
    {
        int needs_dkms = 0;
        char klist_tmp[512]; strncpy(klist_tmp, st.kernel_list, sizeof(klist_tmp)-1);
        char *tok2 = strtok(klist_tmp, " ");
        while (tok2) {
            if (strcmp(tok2, "linux") != 0) { needs_dkms = 1; break; }
            tok2 = strtok(NULL, " ");
        }
        if (needs_dkms) nv_pkg = "nvidia-dkms";
    }
    char cmd[MAX_CMD];

    if (!strcmp(st.gpu,"NVIDIA")) {
        ib_stage(ib, L("Installing NVIDIA drivers...","Instalando drivers NVIDIA..."));
        snprintf(cmd,sizeof(cmd),
                 "arch-chroot /mnt pacman -S --noconfirm %s nvidia-utils nvidia-settings",
                 nv_pkg);
        ib_pacman(ib,cmd,start,end,1);
        ib_configure_nvidia_modeset(ib);
    } else if (!strcmp(st.gpu,"AMD")) {
        ib_stage(ib, L("Installing AMD drivers...","Instalando drivers AMD..."));
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-radeon libva-mesa-driver",
                  start,end,1);
    } else if (!strcmp(st.gpu,"Intel")) {
        ib_stage(ib, L("Installing Intel drivers...","Instalando drivers Intel..."));
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver",
                  start,end,1);
    } else if (!strcmp(st.gpu,"Intel+NVIDIA")) {
        ib_stage(ib, L("Installing Intel+NVIDIA (Optimus hybrid) drivers...",
                       "Instalando drivers Intel+NVIDIA (Optimus hibrido)..."));
        double mid = start + (end-start)*0.4;
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver",
                  start,mid,1);
        snprintf(cmd,sizeof(cmd),
                 "arch-chroot /mnt pacman -S --noconfirm %s nvidia-utils nvidia-settings nvidia-prime",
                 nv_pkg);
        ib_pacman(ib,cmd,mid,end,1);
        ib_configure_nvidia_modeset(ib);
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm python-pip 2>/dev/null || true",
                  end,end,1);
        ib_chroot(ib,
                  "pip install envycontrol --break-system-packages 2>/dev/null || true",1);
        if (!strcmp(st.optimus_mode,"integrated")) {
            ib_chroot(ib,"envycontrol -s integrated 2>/dev/null || true",1);
            write_log("Optimus: integrated mode");
        } else if (!strcmp(st.optimus_mode,"nvidia")) {
            ib_chroot(ib,"envycontrol -s nvidia 2>/dev/null || true",1);
            write_log("Optimus: NVIDIA-only mode");
        } else {
            ib_chroot(ib,"envycontrol -s hybrid 2>/dev/null || true",1);
            write_log("Optimus: hybrid mode");
        }
    } else if (!strcmp(st.gpu,"Intel+AMD")) {
        ib_stage(ib, L("Installing Intel+AMD (hybrid) drivers...",
                       "Instalando drivers Intel+AMD (hybrid)..."));
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel"
                  " intel-media-driver vulkan-radeon libva-mesa-driver",
                  start,end,1);
    } else {
        ib_pct(ib,end);
    }
}

static void ib_configure_keyboard(IB *ib, const char *km) {
    char cmd[MAX_CMD];
    snprintf(cmd,sizeof(cmd),"echo 'KEYMAP=%s' > /etc/vconsole.conf",km);
    ib_chroot(ib,cmd,0);

    const char *x11 = kv_get(CONSOLE_TO_X11,km);
    if (!x11) x11 = km;

    char xorg[1024];
    snprintf(xorg,sizeof(xorg),
             "Section \"InputClass\"\n"
             "Identifier \"system-keyboard\"\n"
             "MatchIsKeyboard \"on\"\n"
             "Option \"XkbLayout\" \"%s\"\n"
             "EndSection\n", x11);
    char q[MAX_CMD];
    shell_quote(xorg,q,sizeof(q));
    snprintf(cmd,sizeof(cmd),
             "mkdir -p /etc/X11/xorg.conf.d && "
             "printf %%s %s > /etc/X11/xorg.conf.d/00-keyboard.conf",q);
    ib_chroot(ib,cmd,1);

    snprintf(cmd,sizeof(cmd),"localectl --no-ask-password set-x11-keymap %s || true",x11);
    ib_chroot(ib,cmd,1);
    write_log_fmt("Keyboard: console=%s x11=%s",km,x11);
}

static void ib_add_cachyos_repo(IB *ib, int chroot) {
    const char *block =
        "\n[cachyos]\n"
        "SigLevel = Optional TrustAll\n"
        "Server = https://mirror.cachyos.org/repo/$arch/$repo\n";
    const char *conf = chroot ? "/mnt/etc/pacman.conf" : "/etc/pacman.conf";
    char q[MAX_CMD], cmd[MAX_CMD];
    shell_quote(block,q,sizeof(q));
    snprintf(cmd,sizeof(cmd),
             "grep -q '\\[cachyos\\]' %s || printf %%s %s >> %s",
             conf, q, conf);
    run_stream(cmd,NULL,NULL,1);
    if (!chroot) {
        run_stream("pacman-key --recv-keys F3B607488DB35A47 "
                   "--keyserver keyserver.ubuntu.com",NULL,NULL,1);
        run_stream("pacman-key --lsign-key F3B607488DB35A47",NULL,NULL,1);
        int rc = run_stream("pacman -Sy --noconfirm",NULL,NULL,1);
        if (rc) run_stream("pacman -Sy --noconfirm",NULL,NULL,1);
    } else {
        ib_chroot(ib,"pacman-key --recv-keys F3B607488DB35A47 "
                     "--keyserver keyserver.ubuntu.com",1);
        ib_chroot(ib,"pacman-key --lsign-key F3B607488DB35A47",1);
    }
    write_log_fmt("CachyOS repo added to %s",conf);
}

static void ib_install_flatpak(IB *ib) {
    ib_stage(ib, L("Installing Flatpak + Flathub...","Instalando Flatpak + Flathub..."));
    ib_pacman(ib,"arch-chroot /mnt pacman -S --noconfirm flatpak",98,99,1);
    char q[MAX_CMD], cmd[MAX_CMD];
    shell_quote(st.username,q,sizeof(q));
    snprintf(cmd,sizeof(cmd),
             "su - %s -c "
             "'flatpak remote-add --if-not-exists flathub "
             "https://dl.flathub.org/repo/flathub.flatpakrepo'",q);
    ib_chroot(ib,cmd,1);
    write_log("Flatpak + Flathub configured.");
}

static void ib_install_laptop(IB *ib) {
    ib_stage(ib, L("Installing laptop power management (TLP)...",
                   "Instalando gestion de energia para laptop (TLP)..."));
    ib_pacman(ib,
              "arch-chroot /mnt pacman -S --noconfirm tlp tlp-rdw powertop acpi acpid",
              0,0,1);
    ib_chroot(ib,"systemctl enable tlp",1);
    ib_chroot(ib,"systemctl enable acpid",1);
    ib_chroot(ib,"systemctl mask systemd-rfkill.service systemd-rfkill.socket 2>/dev/null || true",1);
    write_log("Laptop: TLP + powertop + acpi installed and enabled.");
}

static void ib_enable_multilib(IB *ib) {
    ib_chroot(ib,
        "sed -i '/^#\\[multilib\\]/{N;s/#\\[multilib\\]\\n#Include/[multilib]\\nInclude/}' "
        "/etc/pacman.conf",1);
    ib_chroot(ib,"pacman -Sy --noconfirm",1);
    write_log("multilib repository enabled.");
}

static void ib_install_gaming(IB *ib, double start, double end) {
    ib_stage(ib, L("Enabling multilib for Steam...","Habilitando multilib para Steam..."));
    ib_enable_multilib(ib);
    ib_stage(ib, L("Installing gaming packages...","Instalando paquetes para gaming..."));
    ib_pacman(ib,
              "arch-chroot /mnt pacman -S --noconfirm "
              "steam lutris gamemode lib32-gamemode mangohud lib32-mangohud "
              "wine-staging winetricks wine-mono "
              "lib32-vulkan-icd-loader vulkan-icd-loader",
              start, end, 1);
    write_log("Gaming profile: steam, lutris, gamemode, mangohud, wine installed.");
}

static void ib_install_developer(IB *ib, double start, double end) {
    ib_stage(ib, L("Installing developer packages...","Instalando paquetes de desarrollo..."));
    ib_pacman(ib,
              "arch-chroot /mnt pacman -S --noconfirm "
              "git base-devel vim neovim tmux docker docker-compose "
              "python python-pip nodejs npm go rust jdk-openjdk "
              "gdb valgrind strace ltrace",
              start, end, 1);
    ib_chroot(ib,"systemctl enable docker",1);
    char q[MAX_CMD], cmd[MAX_CMD];
    shell_quote(st.username,q,sizeof(q));
    snprintf(cmd,sizeof(cmd),"usermod -aG docker %s",st.username);
    ib_chroot(ib,cmd,1);
    write_log("Developer profile installed.");
}

static void ib_install_privacy(IB *ib, double start, double end) {
    ib_stage(ib, L("Installing privacy packages...","Instalando paquetes de privacidad..."));
    ib_pacman(ib,
              "arch-chroot /mnt pacman -S --noconfirm "
              "tor torsocks ufw fail2ban bleachbit keepassxc "
              "firejail apparmor",
              start, end, 1);
    ib_chroot(ib,"systemctl enable ufw",1);
    ib_chroot(ib,"ufw default deny incoming 2>/dev/null || true",1);
    ib_chroot(ib,"systemctl enable fail2ban",1);
    write_log("Privacy profile installed.");
}

static void ib_configure_grub_cmdline(IB *ib) {
    if (!strcmp(st.filesystem,"btrfs")) {
        ib_chroot(ib,
            "grep -q 'rootflags=subvol=@' /etc/default/grub || "
            "sed -i 's|^\\(GRUB_CMDLINE_LINUX_DEFAULT=\"[^\"]*\\)\"|\\1 rootflags=subvol=@\"|' "
            "/etc/default/grub",
            1);
    }
    if (!strcmp(st.filesystem,"zfs")) {
        ib_chroot(ib,
            "grep -q 'zfs=bootfs' /etc/default/grub || "
            "sed -i 's|^\\(GRUB_CMDLINE_LINUX_DEFAULT=\"[^\"]*\\)\"|\\1 zfs=bootfs\"|' "
            "/etc/default/grub",
            1);
        ib_chroot(ib,
            "grep -q ZPOOL_VDEV_NAME_PATH /etc/default/grub || "
            "printf '\\nexport ZPOOL_VDEV_NAME_PATH=1\\n' >> /etc/default/grub",
            1);
    }
}

typedef struct {
    IB *ib;
} IBRunArg;

static void *ib_run_thread(void *arg) {
    IB *ib = ((IBRunArg*)arg)->ib;
    free(arg);

    compile_regexes();

    if (!st.disk[0]) {
        ib->on_done(0, "No disk selected. Aborting installation.", ib->ud);
        pthread_mutex_destroy(&ib->lock);
        free(ib);
        return NULL;
    }
    char pkernel[64]; strncpy(pkernel, st.kernel_list, sizeof(pkernel)-1);
    { char *sp = strchr(pkernel,' '); if(sp) *sp='\0'; }
    strncpy(st.kernel, pkernel, sizeof(st.kernel)-1);

    char pdesktop[64]; strncpy(pdesktop, st.desktop_list, sizeof(pdesktop)-1);
    { char *sp = strchr(pdesktop,'|'); if(sp) *sp='\0'; }
    strncpy(st.desktop, pdesktop, sizeof(st.desktop)-1);

    char disk[256];
    snprintf(disk, sizeof(disk), "/dev/%s", st.disk);

    char p1[256],p2[256],p3[256];
    if (!st.dualboot) partition_paths(disk,p1,p2,p3,sizeof(p1));

    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    int  uefi       = is_uefi();
    const char *bl  = st.bootloader;
    const char *fs  = st.filesystem;
    const char *root_dev = st.dualboot ? (const char*)st.db_root : (const char*)p3;

    if (!strcmp(fs,"zfs")) {
        strncpy(st.bootloader,"grub",sizeof(st.bootloader)-1);
        bl = st.bootloader;
    }

    if (!strcmp(bl,"systemd-boot") && !uefi) {
        write_log("systemd-boot requested but BIOS detected - falling back to GRUB");
        strncpy(st.bootloader,"grub",sizeof(st.bootloader)-1);
        bl = st.bootloader;
    }

    ib_stage(ib, L("Checking network...","Verificando red..."));
    if (!ensure_network()) {
        ib->on_done(0, L("No network connection. Connect and retry.",
                         "Sin conexion de red. Conectese e intente de nuevo."), ib->ud);
        return NULL;
    }

    run_stream("pacman -Sy --noconfirm archlinux-keyring",NULL,NULL,1);

    if (st.mirrors) {
        ib_stage(ib, L("Optimizing mirrors with reflector...","Optimizando mirrors con reflector..."));
        run_stream("pacman -Sy --noconfirm reflector",NULL,NULL,1);
        run_stream("reflector --latest 10 --sort rate --save /etc/pacman.d/mirrorlist",
                   NULL,NULL,1);
    }
    ib_pct(ib,5);

    if (!st.dualboot) {
        ib_stage(ib, L("Wiping disk...","Borrando disco..."));
        ib_gradual(ib,7,20,0.04);
        char cmd[MAX_CMD];
        snprintf(cmd,sizeof(cmd),"wipefs -a %s",disk); run_stream(cmd,NULL,NULL,1);
        snprintf(cmd,sizeof(cmd),"sgdisk -Z %s",disk); ib_run(ib,cmd,"sgdisk -Z");
        settle_partitions(disk);
        ib_pct(ib,8);

        ib_stage(ib, L("Creating partitions...","Creando particiones..."));
        if (uefi) {
            snprintf(cmd,sizeof(cmd),"sgdisk -n1:0:+1G -t1:ef00 %s",disk);
            ib_run(ib,cmd,"sgdisk EFI");
        } else {
            snprintf(cmd,sizeof(cmd),"sgdisk -n1:0:+1M -t1:ef02 %s",disk);
            ib_run(ib,cmd,"sgdisk BIOS boot");
        }
        snprintf(cmd,sizeof(cmd),"sgdisk -n2:0:+%sG -t2:8200 %s",st.swap,disk);
        ib_run(ib,cmd,"sgdisk swap");
        snprintf(cmd,sizeof(cmd),"sgdisk -n3:0:0 -t3:8300 %s",disk);
        ib_run(ib,cmd,"sgdisk root");
        settle_partitions(disk);
        ib_pct(ib,12);

        ib_stage(ib, L("Formatting partitions...","Formateando particiones..."));
        if (uefi) { snprintf(cmd,sizeof(cmd),"mkfs.fat -F32 %s",p1); ib_run(ib,cmd,"mkfs.fat"); }
        snprintf(cmd,sizeof(cmd),"mkswap %s",p2); ib_run(ib,cmd,"mkswap");
        snprintf(cmd,sizeof(cmd),"swapon %s",p2); run_stream(cmd,NULL,NULL,1);

        if (!strcmp(fs,"btrfs"))     ib_setup_btrfs(ib, root_dev, disk);
        else if (!strcmp(fs,"xfs"))  ib_setup_xfs(ib, root_dev);
        else if (!strcmp(fs,"zfs"))  ib_setup_zfs(ib, root_dev);
        else {
            snprintf(cmd,sizeof(cmd),"mkfs.ext4 -F %s",root_dev);
            ib_run(ib,cmd,"mkfs.ext4");
            run_simple("udevadm settle --timeout=10", 1);
            snprintf(cmd,sizeof(cmd),"mount %s /mnt",root_dev);
            ib_run(ib,cmd,"mount root");
        }
        ib_pct(ib,16);

        if (uefi) {
            ib_stage(ib, L("Mounting EFI...","Montando EFI..."));
            if (!strcmp(bl,"systemd-boot") || !strcmp(bl,"limine")) {
                ib_run(ib,"mkdir -p /mnt/boot","mkdir /mnt/boot");
                snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot",p1);
                ib_run(ib,cmd,"mount ESP /mnt/boot");
            } else {
                ib_run(ib,"mkdir -p /mnt/boot/efi","mkdir /mnt/boot/efi");
                snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot/efi",p1);
                ib_run(ib,cmd,"mount ESP /mnt/boot/efi");
            }
        }

    } else {
        char cmd[MAX_CMD];

        ib_stage(ib, L("Creating new Arch partition (dual-boot)...",
                       "Creando nueva particion para Arch (dual-boot)..."));
        ib_gradual(ib,8,14,0.04);

        {
            long long size_bytes = (long long)st.db_size_gb * 1024LL * 1024LL * 1024LL;
            long long min_start  = 1LL * 1024LL * 1024LL;

            char pt_type[16] = "msdos";
            {
                char ptcmd[256];
                snprintf(ptcmd, sizeof(ptcmd),
                    "parted -s %s print 2>/dev/null | awk '/Partition Table/{print $3}'",
                    disk);
                FILE *fp_pt2 = popen(ptcmd, "r");
                if (fp_pt2) {
                    char tmp[32]={0};
                    if (fgets(tmp,sizeof(tmp),fp_pt2)) {
                        trim_nl(tmp);
                        if (tmp[0]) strncpy(pt_type,tmp,sizeof(pt_type)-1);
                    }
                    pclose(fp_pt2);
                }
                write_log_fmt("Partition table type: %s", pt_type);
            }

            long long start_bytes = 0;
            long long avail_bytes = 0;
            {
                char free_cmd[512];
                snprintf(free_cmd, sizeof(free_cmd),
                    "parted -s %s unit B print free 2>/dev/null"
                    " | awk '/Free Space/{gsub(\"B\",\"\",$1); gsub(\"B\",\"\",$3);"
                    " if($3+0 > avail){avail=$3+0; start=$1+0}}"
                    " END{print start, avail}'",
                    disk);
                FILE *fp_free = popen(free_cmd, "r");
                if (fp_free) {
                    long long s=0, a=0;
                    if (fscanf(fp_free, "%lld %lld", &s, &a) == 2) {
                        start_bytes = s;
                        avail_bytes = a;
                    }
                    pclose(fp_free);
                }
                write_log_fmt("Largest free region: start=%lldB size=%lldB", start_bytes, avail_bytes);
            }

            if (avail_bytes < size_bytes) {
                char errmsg[256];
                snprintf(errmsg, sizeof(errmsg),
                    L("Not enough free space on %s.\n"
                      "Available: %lld GB  |  Requested: %d GB\n\n"
                      "Free up space and try again.",
                      "No hay suficiente espacio libre en %s.\n"
                      "Disponible: %lld GB  |  Solicitado: %d GB\n\n"
                      "Libera espacio e intenta de nuevo."),
                    disk, avail_bytes / (1024LL*1024LL*1024LL), st.db_size_gb);
                ib->on_done(0, errmsg, ib->ud);
                return NULL;
            }

            if (start_bytes < min_start) start_bytes = min_start;

            long long align = 1LL * 1024LL * 1024LL;
            start_bytes = ((start_bytes + align - 1) / align) * align;
            long long end_bytes = start_bytes + size_bytes;

            write_log_fmt("Dual-boot: creating partition %lldB - %lldB on %s",
                          start_bytes, end_bytes, disk);

            if (!strcmp(pt_type, "gpt"))
                snprintf(cmd, sizeof(cmd),
                    "parted -s %s mkpart arch %lldB %lldB",
                    disk, start_bytes, end_bytes);
            else
                snprintf(cmd, sizeof(cmd),
                    "parted -s %s mkpart primary %lldB %lldB",
                    disk, start_bytes, end_bytes);

            ib_run(ib, cmd, "parted mkpart dual-boot");
            settle_partitions(disk);

            char part_cmd[256];
            snprintf(part_cmd, sizeof(part_cmd),
                "parted -m %s unit B print 2>/dev/null"
                " | awk -F: '$1~/^[0-9]+$/{n=$1} END{print n}'",
                disk);
            FILE *fp_pt = popen(part_cmd, "r");
            if (fp_pt) {
                char pnum[16] = {0};
                if (fgets(pnum, sizeof(pnum), fp_pt)) {
                    trim_nl(pnum);
                    int n = atoi(pnum);
                    if (n > 0) {
                        if (strstr(disk,"nvme") || strstr(disk,"mmcblk"))
                            snprintf(st.db_root, sizeof(st.db_root), "%sp%d", disk, n);
                        else
                            snprintf(st.db_root, sizeof(st.db_root), "%s%d", disk, n);
                        write_log_fmt("Dual-boot new partition: %s", st.db_root);
                    }
                }
                pclose(fp_pt);
            }
        }

        settle_partitions(disk);
        root_dev = st.db_root;

        ib_stage(ib, L("Formatting new Arch partition (dual-boot)...",
                       "Formateando nueva particion de Arch (dual-boot)..."));

        if (!strcmp(fs,"btrfs"))     ib_setup_btrfs(ib, st.db_root, disk);
        else if (!strcmp(fs,"xfs"))  ib_setup_xfs(ib, st.db_root);
        else if (!strcmp(fs,"zfs"))  ib_setup_zfs(ib, st.db_root);
        else {
            snprintf(cmd,sizeof(cmd),"mkfs.ext4 -F %s", st.db_root);
            ib_run(ib, cmd, "mkfs.ext4 dual-boot");
            run_simple("udevadm settle --timeout=10", 1);
            snprintf(cmd,sizeof(cmd),"mount %s /mnt", st.db_root);
            ib_run(ib, cmd, "mount root dual-boot");
        }

        if (uefi && st.db_efi[0]) {
            ib_stage(ib, L("Mounting existing EFI partition...",
                           "Montando particion EFI existente..."));
            if (!strcmp(bl,"systemd-boot") || !strcmp(bl,"limine")) {
                run_simple("mkdir -p /mnt/boot", 0);
                snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot", st.db_efi);
                ib_run(ib, cmd, "mount EFI /mnt/boot dual-boot");
            } else {
                run_simple("mkdir -p /mnt/boot/efi", 0);
                snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot/efi", st.db_efi);
                ib_run(ib, cmd, "mount EFI /mnt/boot/efi dual-boot");
            }
        }
    }
    ib_pct(ib,18);

    char kpkgs[1024]={0};
    char kheaders[1024]={0};
    {
        char klist_tmp[512]; strncpy(klist_tmp, st.kernel_list, sizeof(klist_tmp)-1);
        char *tok = strtok(klist_tmp, " ");
        while (tok) {
            if (kpkgs[0]) strncat(kpkgs," ",sizeof(kpkgs)-strlen(kpkgs)-1);
            strncat(kpkgs, tok, sizeof(kpkgs)-strlen(kpkgs)-1);
            char hdr[64]; snprintf(hdr,sizeof(hdr)," %s-headers",tok);
            strncat(kheaders, hdr, sizeof(kheaders)-strlen(kheaders)-1);
            if (!strcmp(tok,"linux-cachyos")) ib_add_cachyos_repo(ib,0);
            tok = strtok(NULL," ");
        }
    }

    char ucode_pkg[64]="", extra_pkg[64]="", boot_pkgs[128]="";
    if (microcode[0]) snprintf(ucode_pkg,sizeof(ucode_pkg)," %s",microcode);
    if (!strcmp(fs,"btrfs")) strcpy(extra_pkg," btrfs-progs");
    if (!strcmp(fs,"xfs"))   strcpy(extra_pkg," xfsprogs");
    if (!strcmp(fs,"zfs"))   strcpy(extra_pkg," zfs-utils");

    if (!strcmp(bl,"systemd-boot")) {
        strcpy(boot_pkgs," efibootmgr");
    } else if (!strcmp(bl,"limine")) {
        snprintf(boot_pkgs,sizeof(boot_pkgs)," limine%s",uefi?" efibootmgr":"");
    } else {
        snprintf(boot_pkgs,sizeof(boot_pkgs)," grub%s",uefi?" efibootmgr":"");
    }

    char cmd[MAX_CMD];
    snprintf(cmd,sizeof(cmd),
             "pacstrap -K /mnt "
             "base %s linux-firmware%s sof-firmware "
             "base-devel%s vim nano networkmanager git "
             "sudo bash-completion%s%s",
             kpkgs, kheaders, boot_pkgs, extra_pkg, ucode_pkg);

    ib_stage(ib, L("Installing base system - this may take a while...",
                   "Instalando sistema base - esto puede tardar..."));
    ib_pacman_critical(ib,cmd,18,52,"pacstrap");

    {
        char klist_tmp[512]; strncpy(klist_tmp, st.kernel_list, sizeof(klist_tmp)-1);
        char *tok = strtok(klist_tmp, " ");
        while (tok) {
            if (!strcmp(tok,"linux-cachyos")) { ib_add_cachyos_repo(ib,1); break; }
            tok = strtok(NULL," ");
        }
    }
    if (!strcmp(fs,"zfs")) {
        ib_add_archzfs_repo(1);
        ib_chroot(ib, "pacman -Sy --noconfirm zfs-dkms zfs-utils", 1);

        ib_chroot(ib,
            "systemctl enable zfs-import-cache.service "
            "zfs-import.target zfs-mount.service zfs.target", 1);

        ib_chroot(ib,
            "sed -i 's/^HOOKS=.*$/"
            "HOOKS=(base udev autodetect microcode modconf kms keyboard keymap consolefont block zfs filesystems)/' "
            "/etc/mkinitcpio.conf", 1);

        ib_chroot(ib, "zgenhostid $(hostid)", 1);

        {
            char klist_tmp[512]; strncpy(klist_tmp, st.kernel_list, sizeof(klist_tmp)-1);
            char *tok = strtok(klist_tmp, " ");
            while (tok) {
                char mkcmd[256];
                snprintf(mkcmd, sizeof(mkcmd),
                         "mkinitcpio -p %s 2>&1 || mkinitcpio -P 2>&1", tok);
                ib_chroot(ib, mkcmd, 1);
                tok = strtok(NULL, " ");
            }
        }

        run_simple("cp /etc/zfs/zpool.cache /mnt/etc/zfs/zpool.cache 2>/dev/null || true", 0);
    }

    ib_stage(ib, L("Generating fstab...","Generando fstab..."));
    if (!strcmp(fs,"zfs")) {
        FILE *fstab = fopen("/mnt/etc/fstab","a");
        if (fstab) { fprintf(fstab,"# ZFS managed\n"); fclose(fstab); }
    } else {
        ib_run(ib,"genfstab -U /mnt >> /mnt/etc/fstab","genfstab");
    }
    ib_pct(ib,53);

    ib_stage(ib, L("Configuring hostname...","Configurando hostname..."));
    FILE *f = fopen("/mnt/etc/hostname","w");
    if (f) { fprintf(f,"%s\n",st.hostname); fclose(f); }
    f = fopen("/mnt/etc/hosts","w");
    if (f) {
        fprintf(f,"127.0.0.1\tlocalhost\n");
        fprintf(f,"::1\t\tlocalhost\n");
        fprintf(f,"127.0.1.1\t%s.localdomain\t%s\n",st.hostname,st.hostname);
        fclose(f);
    }
    ib_pct(ib,55);

    ib_stage(ib, L("Configuring locale & timezone...","Configurando locale y zona horaria..."));
    ib_chroot(ib,"sed -i 's/^#en_US.UTF-8 UTF-8/en_US.UTF-8 UTF-8/' /etc/locale.gen",0);
    if (strcmp(st.locale,"en_US.UTF-8")) {
        char sed[256];
        snprintf(sed,sizeof(sed),
                 "sed -i 's/^#%s UTF-8/%s UTF-8/' /etc/locale.gen",
                 st.locale,st.locale);
        ib_chroot(ib,sed,1);
    }
    ib_chroot_c(ib,"locale-gen","locale-gen");
    snprintf(cmd,sizeof(cmd),"echo 'LANG=%s' > /etc/locale.conf",st.locale);
    ib_chroot(ib,cmd,0);
    snprintf(cmd,sizeof(cmd),"ln -sf /usr/share/zoneinfo/%s /etc/localtime",st.timezone);
    ib_chroot(ib,cmd,0);
    ib_chroot(ib,"hwclock --systohc",0);
    ib_pct(ib,59);

    ib_stage(ib, L("Configuring keyboard layout...","Configurando distribucion de teclado..."));
    ib_configure_keyboard(ib,st.keymap);
    ib_pct(ib,61);

    ib_stage(ib, L("Generating initramfs...","Generando initramfs..."));
    ib_chroot_c(ib,"mkinitcpio -P","mkinitcpio");
    ib_pct(ib,63);

    char stage_msg[128];
    snprintf(stage_msg,sizeof(stage_msg),
             L("Creating user '%s'...","Creando usuario '%s'..."),st.username);
    ib_stage(ib,stage_msg);
    snprintf(cmd,sizeof(cmd),"useradd -m -G wheel -s /bin/bash %s",st.username);
    ib_chroot_c(ib,cmd,"useradd");
    ib_chroot(ib,
        "sed -i 's/^# %wheel ALL=(ALL:ALL) ALL/"
        "%wheel ALL=(ALL:ALL) ALL/' /etc/sudoers",0);
    ib_pct(ib,65);

    ib_stage(ib, L("Setting passwords...","Estableciendo contrasenas..."));
    ib_chroot_passwd(ib,"root",st.root_pass);
    ib_chroot_passwd(ib,st.username,st.user_pass);
    ib_pct(ib,68);

    ib_stage(ib, L("Enabling NetworkManager...","Habilitando NetworkManager..."));
    ib_chroot(ib,"systemctl enable NetworkManager",0);
    if (is_ssd(disk)) {
        ib_chroot(ib,"systemctl enable fstrim.timer",1);
        write_log("SSD detected - fstrim.timer enabled.");
    }
    ib_pct(ib,71);

    ib_install_gpu(ib,71,77);
    ib_pct(ib,77);

    {

        int any_desktop = 0;
        int dm_enabled  = 0;
        double de_start = 77.0, de_end = 91.0;
        int de_count = 0;
        {
            char tmp2[512]; strncpy(tmp2, st.desktop_list, sizeof(tmp2)-1);
            char *t2 = strtok(tmp2, "|");
            while (t2) { if (strcmp(t2,"None")) de_count++; t2 = strtok(NULL,"|"); }
        }
        if (de_count == 0) de_count = 1;
        double de_step = (de_end - de_start) / de_count;
        int de_idx = 0;

        char dlist[512]; strncpy(dlist, st.desktop_list, sizeof(dlist)-1);
        char *dtok = strtok(dlist, "|");

        while (dtok) {
            if (strcmp(dtok,"None") != 0) {
                any_desktop = 1;
                snprintf(stage_msg,sizeof(stage_msg),
                         L("Installing %s...","Instalando %s..."),dtok);
                ib_stage(ib, stage_msg);

                const DesktopDef *dd = get_desktop_def(dtok);
                if (dd) {
                    double gs = de_start + de_idx * de_step;
                    double ge = gs + de_step;
                    int ng = dd->ngroups;
                    double gstep = (ge - gs) / (ng > 0 ? ng : 1);
                    for (int i = 0; i < ng; i++) {
                        snprintf(cmd,sizeof(cmd),
                                 "arch-chroot /mnt pacman -S --noconfirm %s",dd->groups[i]);
                        ib_pacman(ib, cmd, gs + i*gstep, gs + (i+1)*gstep, 1);
                    }
                }
                const char *dm = get_desktop_dm(dtok);
                if (dm && !dm_enabled) {
                    snprintf(cmd,sizeof(cmd),"systemctl enable %s",dm);
                    ib_chroot(ib,cmd,0);
                    dm_enabled = 1;
                }
                if (!strcmp(dtok,"Hyprland")||!strcmp(dtok,"Sway")) {
                    snprintf(stage_msg,sizeof(stage_msg),
                             L("Enabling seat management for %s...",
                               "Habilitando seat para %s..."),dtok);
                    ib_stage(ib, stage_msg);
                    snprintf(cmd,sizeof(cmd),"usermod -aG seat,input,video %s",st.username);
                    ib_chroot(ib,cmd,1);
                }
                de_idx++;
            }
            dtok = strtok(NULL, "|");
        }

        if (any_desktop) {
            ib_stage(ib, L("Installing audio (pipewire)...","Instalando audio (pipewire)..."));
            ib_pacman(ib,
                      "arch-chroot /mnt pacman -S --noconfirm "
                      "pipewire pipewire-pulse wireplumber",
                      91, 94, 1);
        }
    }
    ib_pct(ib,94);

    if (!strcmp(bl,"systemd-boot")) {
        ib_stage(ib, L("Installing systemd-boot...","Instalando systemd-boot..."));
        ib_install_systemd_boot(ib,root_dev);
    } else if (!strcmp(bl,"limine")) {
        ib_stage(ib, L("Installing Limine bootloader...","Instalando Limine..."));
        ib_install_limine(ib,disk,root_dev);
    } else {
        ib_stage(ib, L("Installing GRUB bootloader...","Instalando GRUB..."));
        ib_configure_grub_cmdline(ib);
        ib_install_grub(ib,disk);
    }
    ib_pct(ib,97);

    if (st.snapper && !strcmp(fs,"btrfs")) {
        ib_stage(ib, L("Setting up snapper (BTRFS snapshots)...",
                       "Configurando snapper (snapshots BTRFS)..."));
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm "
                  "snapper snap-pac grub-btrfs inotify-tools",
                  97,98,1);
        ib_chroot(ib,"snapper -c root create-config /",0);
        ib_chroot(ib,"umount /.snapshots",1);
        ib_chroot(ib,"rm -rf /.snapshots",1);
        ib_chroot(ib,"mkdir -p /.snapshots",0);
        ib_chroot(ib,"mount -a",1);
        ib_chroot(ib,"chmod 750 /.snapshots",0);
        ib_chroot(ib,"systemctl enable snapper-timeline.timer",0);
        ib_chroot(ib,"systemctl enable snapper-cleanup.timer",0);
        if (!strcmp(bl,"grub")) {
            ib_chroot(ib,"systemctl enable grub-btrfs.path",0);
            ib_chroot(ib,"grub-mkconfig -o /boot/grub/grub.cfg",0);
        }
    }

    if (st.yay) {
        ib_stage(ib, L("Installing yay (AUR helper)...","Instalando yay (AUR helper)..."));
        ib_chroot(ib,
            "echo '%wheel ALL=(ALL) NOPASSWD: ALL' "
            "> /etc/sudoers.d/99_nopasswd_tmp",0);
        char q[MAX_CMD]; shell_quote(st.username,q,sizeof(q));
        snprintf(cmd,sizeof(cmd),
                 "su - %s -c "
                 "'git clone https://aur.archlinux.org/yay.git /tmp/yay "
                 "&& cd /tmp/yay && makepkg -si --noconfirm'",q);
        ib_chroot(ib,cmd,1);
        ib_chroot(ib,"rm -f /etc/sudoers.d/99_nopasswd_tmp",0);
    }

    if (st.flatpak && strcmp(st.desktop,"None"))
        ib_install_flatpak(ib);

    if (st.extra_pkgs[0]) {
        ib_stage(ib, L("Installing extra packages...","Instalando paquetes adicionales..."));
        snprintf(cmd,sizeof(cmd),
                 "arch-chroot /mnt pacman -S --noconfirm %s", st.extra_pkgs);
        ib_pacman(ib, cmd, 98, 99, 1);
        write_log_fmt("Extra packages installed: %s", st.extra_pkgs);

        if (strstr(st.extra_pkgs, "fish") && st.fish_default) {
            ib_stage(ib, L("Setting fish as default shell...","Estableciendo fish como shell por defecto..."));
            ib_chroot(ib,
                "grep -qxF /usr/bin/fish /etc/shells || echo /usr/bin/fish >> /etc/shells",
                1);
            snprintf(cmd, sizeof(cmd), "chsh -s /usr/bin/fish %s", st.username);
            ib_chroot(ib, cmd, 1);
            write_log("fish set as default shell for user.");
        }
    }

    {
        CPUInfo ci; detect_cpu_full(&ci);
        if (ci.threads > 1) {
            snprintf(cmd,sizeof(cmd),
                     "sed -i 's/^#MAKEFLAGS=.*/MAKEFLAGS=\"-j%d\"/' /etc/makepkg.conf || "
                     "echo 'MAKEFLAGS=\"-j%d\"' >> /etc/makepkg.conf",
                     ci.threads, ci.threads);
            ib_chroot(ib,cmd,1);
            write_log_fmt("MAKEFLAGS set to -j%d (%s CPU, %d cores, %d threads)",
                          ci.threads, ci.vendor, ci.cores, ci.threads);
        }
    }

    if (st.laptop) {
        ib_install_laptop(ib);
    }

    if (!strcmp(st.profile,"gaming")) {
        ib_install_gaming(ib, 98.5, 99.2);
    } else if (!strcmp(st.profile,"developer")) {
        ib_install_developer(ib, 98.5, 99.2);
    } else if (!strcmp(st.profile,"privacy")) {
        ib_install_privacy(ib, 98.5, 99.2);
    }

    if (strcmp(st.dotfiles,"none") != 0) {
        ib_stage(ib, L("Installing dotfiles...","Instalando dotfiles..."));
        char q_user[MAX_CMD];
        shell_quote(st.username,q_user,sizeof(q_user));

        ib_chroot(ib,"pacman -S --noconfirm --needed git 2>/dev/null || true",1);

        if (!strcmp(st.dotfiles,"caelestia")) {
            ib_stage(ib, L("Installing fish (required by caelestia)...",
                           "Instalando fish (requerido por caelestia)..."));
            ib_chroot(ib,
                "pacman -S --noconfirm --needed fish git 2>/dev/null || true",
                1);
            ib_chroot(ib,
                "grep -qxF /usr/bin/fish /etc/shells "
                "|| echo /usr/bin/fish >> /etc/shells",
                1);
            snprintf(cmd, sizeof(cmd), "chsh -s /usr/bin/fish %s", st.username);
            ib_chroot(ib, cmd, 1);
            write_log("fish installed and set as default shell for caelestia.");

            ib_stage(ib, L("Cloning caelestia dotfiles...",
                           "Clonando dotfiles caelestia..."));
            const char *aur_helper = st.yay ? "yay" : "paru";
            char caelestia_cmd[MAX_CMD];
            snprintf(caelestia_cmd, sizeof(caelestia_cmd),
                "mkdir -p /home/%s/.local/share && "
                "git clone --depth=1 https://github.com/caelestia-dots/caelestia "
                "/home/%s/.local/share/caelestia 2>&1 && "
                "cd /home/%s/.local/share/caelestia && "
                "fish install.fish --noconfirm --aur-helper=%s 2>&1",
                st.username, st.username, st.username, aur_helper);
            ib_chroot(ib, caelestia_cmd, 1);
            write_log("Caelestia dotfiles installed.");

        } else if (!strcmp(st.dotfiles,"custom") && st.dotfiles_url[0]) {
            ib_chroot(ib,"pacman -S --noconfirm --needed git 2>/dev/null || true",1);
            char install_cmd[MAX_CMD];
            snprintf(install_cmd, sizeof(install_cmd),
                     "su - %s -c '"
                     "git clone --depth=1 %s ~/dots 2>&1 && "
                     "cd ~/dots && "
                     "if [ -f install.sh ]; then bash install.sh --noconfirm 2>&1; "
                     "elif [ -f setup.sh ]; then bash setup.sh --noconfirm 2>&1; "
                     "else echo No install script found, dotfiles cloned to ~/dots; fi'",
                     st.username, st.dotfiles_url);
            ib_chroot(ib, install_cmd, 1);
            write_log_fmt("Custom dotfiles installed from: %s", st.dotfiles_url);
        }
    }

    {
        ib_stage(ib, L("Installing pkg-helper (package manager GUI)...",
                       "Instalando pkg-helper (gestor de paquetes GUI)..."));

        ib_chroot(ib,
            "curl -sL --max-time 60 "
            "-o /usr/local/bin/pkg-helper "
            "'https://raw.githubusercontent.com/humrand/arch-installation-easy"
            "/main/SourceCode/pkg-helper' "
            "&& chmod +x /usr/local/bin/pkg-helper",
            1);

        ib_chroot(ib,
            "mkdir -p /usr/share/icons/hicolor/256x256/apps && "
            "curl -sL --max-time 30 "
            "-o /usr/share/icons/hicolor/256x256/apps/pkg-helper.png "
            "'https://raw.githubusercontent.com/humrand/arch-installation-easy"
            "/main/SourceCode/images/pkg.png' && "
            "gtk-update-icon-cache -f /usr/share/icons/hicolor 2>/dev/null || true",
            1);

        ib_chroot(ib,
            "pacman -S --noconfirm --needed gtk3 2>/dev/null || true",
            1);

        ib_chroot(ib,
            "mkdir -p /usr/share/applications && "
            "printf '"
            "[Desktop Entry]\\n"
            "Name=PKG Helper\\n"
            "Comment=PulseOS Package Manager (pacman/AUR/Flatpak)\\n"
            "Exec=pkg-helper\\n"
            "Icon=pkg-helper\\n"
            "Terminal=false\\n"
            "Type=Application\\n"
            "Categories=System;PackageManager;\\n"
            "Keywords=pacman;aur;flatpak;packages;\\n"
            "' > /usr/share/applications/pkg-helper.desktop",
            1);

        write_log("pkg-helper: binary, icon and .desktop installed.");
    }

    {
        ib_stage(ib, L("Applying PulseOS branding...",
                       "Aplicando branding PulseOS..."));

        FILE *fos = fopen("/mnt/etc/os-release", "w");
        if (fos) {
            fprintf(fos,
                "NAME=\"PulseOS\"\n"
                "PRETTY_NAME=\"PulseOS x86\"\n"
                "ID=pulseos\n"
                "ID_LIKE=arch\n"
                "BUILD_ID=rolling\n"
                "ANSI_COLOR=\"38;2;120;120;255\"\n"
                "HOME_URL=\"https://github.com/humrand/arch-installation-easy\"\n"
                "SUPPORT_URL=\"https://github.com/humrand/arch-installation-easy/pulls\"\n"
                "BUG_REPORT_URL=\"https://github.com/humrand/arch-installation-easy/issues\"\n"
                "LOGO=pulseos\n");
            fclose(fos);
        }

        FILE *flsb = fopen("/mnt/etc/lsb-release", "w");
        if (flsb) {
            fprintf(flsb,
                "DISTRIB_ID=PulseOS\n"
                "DISTRIB_RELEASE=rolling\n"
                "DISTRIB_DESCRIPTION=\"PulseOS x86\"\n");
            fclose(flsb);
        }

        FILE *fiss = fopen("/mnt/etc/issue", "w");
        if (fiss) {
            fprintf(fiss, "Welcome to PulseOS x86  \\r  (\\l)\n\n");
            fclose(fiss);
        }

        FILE *fmotd = fopen("/mnt/etc/motd", "w");
        if (fmotd) {
            fprintf(fmotd, "Welcome to PulseOS x86\n");
            fclose(fmotd);
        }

        if (!strcmp(bl,"grub")) {
            ib_chroot(ib,
                "sed -i 's/^GRUB_DISTRIBUTOR=.*/GRUB_DISTRIBUTOR=\"PulseOS\"/' "
                "/etc/default/grub && grub-mkconfig -o /boot/grub/grub.cfg",
                1);
        }

        if (!strcmp(bl,"systemd-boot")) {
            ib_chroot(ib,
                "find /boot/loader/entries -name '*.conf' "
                "-exec sed -i 's/^title.*Arch Linux/title   PulseOS/' {} \\;",
                1);
        }

        ib_stage(ib, L("Installing fastfetch...", "Instalando fastfetch..."));
        ib_pacman(ib,
            "arch-chroot /mnt pacman -S --noconfirm --needed fastfetch",
            99.2, 99.6, 1);

        run_simple("mkdir -p /mnt/etc/fastfetch", 0);
        FILE *flogo = fopen("/mnt/etc/fastfetch/pulseos.txt", "wb");
        if (flogo) {
            fwrite(PULSE_LOGO_DATA, 1, PULSE_LOGO_LEN, flogo);
            fclose(flogo);
        }

        FILE *fcfg = fopen("/mnt/etc/fastfetch/config.jsonc", "w");
        if (fcfg) {
            fprintf(fcfg,
                "{\n"
                "  \"$schema\": \"https://github.com/fastfetch-cli/fastfetch/raw/dev/doc/json_schema.json\",\n"
                "  \"logo\": {\n"
                "    \"source\": \"/etc/fastfetch/pulseos.txt\",\n"
                "    \"color\": { \"1\": \"blue\", \"2\": \"white\" }\n"
                "  },\n"
                "  \"display\": { \"separator\": \" : \" },\n"
                "  \"modules\": [\n"
                "    \"title\", \"separator\", \"os\", \"kernel\",\n"
                "    \"uptime\", \"packages\", \"shell\", \"display\",\n"
                "    \"de\", \"wm\", \"terminal\", \"cpu\", \"gpu\",\n"
                "    \"memory\", \"disk\", \"battery\", \"locale\"\n"
                "  ]\n"
                "}\n");
            fclose(fcfg);
        }

        ib_chroot(ib,
            "if command -v neofetch >/dev/null 2>&1; then "
            "  sed -i '/\"Arch Linux\")/{ N; s/\"Arch Linux\")/\"PulseOS\"|\"Arch Linux\")/; }' "
            "  /usr/bin/neofetch 2>/dev/null || true; "
            "fi",
            1);

        char ff_user_cmd[MAX_CMD];
        snprintf(ff_user_cmd, sizeof(ff_user_cmd),
            "mkdir -p /home/%s/.config/fastfetch && "
            "cp /etc/fastfetch/pulseos.txt /home/%s/.config/fastfetch/pulseos.txt && "
            "printf '{\n"
            "  \"logo\": {\n"
            "    \"source\": \"~/.config/fastfetch/pulseos.txt\",\n"
            "    \"color\": { \"1\": \"blue\", \"2\": \"white\" }\n"
            "  },\n"
            "  \"display\": { \"separator\": \" : \" },\n"
            "  \"modules\": [\n"
            "    \"title\", \"separator\", \"os\", \"kernel\",\n"
            "    \"uptime\", \"packages\", \"shell\", \"display\",\n"
            "    \"de\", \"wm\", \"terminal\", \"cpu\", \"gpu\",\n"
            "    \"memory\", \"disk\", \"battery\", \"locale\"\n"
            "  ]\n"
            "}\\n' > /home/%s/.config/fastfetch/config.jsonc && "
            "chown -R %s:%s /home/%s/.config/fastfetch",
            st.username, st.username,
            st.username,
            st.username, st.username, st.username);
        ib_chroot(ib, ff_user_cmd, 1);

        ib_chroot(ib,
            "mkdir -p /root/.config/fastfetch && "
            "cp /etc/fastfetch/config.jsonc /root/.config/fastfetch/config.jsonc && "
            "cp /etc/fastfetch/pulseos.txt /root/.config/fastfetch/pulseos.txt",
            1);

        write_log("PulseOS branding applied successfully.");
    }

    pthread_mutex_lock(&ib->lock);
    int had_error = ib->had_error;
    char first_error[sizeof(ib->first_error)];
    strncpy(first_error, ib->first_error, sizeof(first_error)-1);
    first_error[sizeof(first_error)-1] = '\0';
    pthread_mutex_unlock(&ib->lock);

    if (had_error) {
        if (!first_error[0]) {
            snprintf(first_error, sizeof(first_error),
                     "An installation command failed. Check %s.", LOG_FILE);
        }
        ib->on_done(0, first_error, ib->ud);
        pthread_mutex_destroy(&ib->lock);
        free(ib);
        return NULL;
    }

    ib_pct(ib,100);
    ib_stage(ib, L("Installation complete!","Instalacion completa!"));
    ib->on_done(1,"",ib->ud);
    pthread_mutex_destroy(&ib->lock);
    free(ib);
    return NULL;
}
static void refresh_disk_list(void);

static void cb_check_wired(GtkButton *b, gpointer d) {
    (void)b;
    GtkWidget *lbl = GTK_WIDGET(d);
    if (check_connectivity())
        gtk_label_set_text(GTK_LABEL(lbl),
            strcmp(st.lang,"en")==0
            ? "✓  Connected — ready to continue."
            : "✓  Conectado — listo para continuar.");
    else
        gtk_label_set_text(GTK_LABEL(lbl),
            strcmp(st.lang,"en")==0
            ? "✗  No connection.  Check cable / router."
            : "✗  Sin conexión.  Revisa el cable / router.");
}

static void cb_connect_wifi(GtkButton *b, gpointer d) {
    (void)b;
    GtkWidget *lbl = GTK_WIDGET(d);
    int r = screen_wifi_connect();
    if (r == 1)
        gtk_label_set_text(GTK_LABEL(lbl),
            strcmp(st.lang,"en")==0
            ? "✓  WiFi connected — ready to continue."
            : "✓  WiFi conectado — listo para continuar.");
    else
        gtk_label_set_text(GTK_LABEL(lbl),
            strcmp(st.lang,"en")==0
            ? "✗  WiFi not connected."
            : "✗  WiFi no conectado.");
}

static void cb_refresh_disks(GtkButton *b, gpointer d) {
    (void)b; (void)d;
    refresh_disk_list();
}

static void cb_dualboot_toggled(GtkToggleButton *tb, gpointer d) {
    if (gtk_toggle_button_get_active(tb))
        gtk_widget_show_all(GTK_WIDGET(d));
    else
        gtk_widget_hide(GTK_WIDGET(d));
}

static void cb_gpu_combo_changed(GtkComboBoxText *cb, gpointer d) {
    const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(cb));
    if (id && !strcmp(id,"Intel+NVIDIA"))
        gtk_widget_show_all(GTK_WIDGET(d));
    else
        gtk_widget_hide(GTK_WIDGET(d));
}

static void cb_dot_radio_toggled(GtkToggleButton *tb, gpointer d) {
    GtkWidget *ubx = GTK_WIDGET(d);
    const char *dot_id = (const char*)g_object_get_data(G_OBJECT(tb),"dot_id");
    if (gtk_toggle_button_get_active(tb) && dot_id && !strcmp(dot_id,"custom"))
        gtk_widget_show_all(ubx);
    else if (gtk_toggle_button_get_active(tb))
        gtk_widget_hide(ubx);
}

typedef struct { GtkWidget **checks; int n; } FilterData;
static void cb_pkg_filter(GtkSearchEntry *e, gpointer d) {
    FilterData *f = (FilterData*)d;
    const char *q = gtk_entry_get_text(GTK_ENTRY(e));
    for (int i=0;i<f->n;i++) {
        if (!q||!q[0]) { gtk_widget_show(f->checks[i]); continue; }
        const char *lbl = gtk_button_get_label(GTK_BUTTON(f->checks[i]));
        char lo[256]={0}, ql[64]={0};
        for (int j=0;lbl[j]&&j<255;j++) lo[j]=tolower((unsigned char)lbl[j]);
        for (int j=0;q[j]&&j<63;j++)    ql[j]=tolower((unsigned char)q[j]);
        if (strstr(lo,ql)) gtk_widget_show(f->checks[i]);
        else               gtk_widget_hide(f->checks[i]);
    }
}

typedef struct { GtkWidget *a,*b,*c,*d; } FourEntries;
static void cb_show_passwords(GtkToggleButton *tb, gpointer d) {
    gboolean vis = gtk_toggle_button_get_active(tb);
    FourEntries *f = (FourEntries*)d;
    gtk_entry_set_visibility(GTK_ENTRY(f->a),vis);
    gtk_entry_set_visibility(GTK_ENTRY(f->b),vis);
    gtk_entry_set_visibility(GTK_ENTRY(f->c),vis);
    gtk_entry_set_visibility(GTK_ENTRY(f->d),vis);
}

static void cb_reboot(GtkButton *b, gpointer d) {
    (void)b; (void)d;
    system("reboot");
}

static GtkWidget *g_wizard_window = NULL;
static GtkWidget *g_stack         = NULL;
static GtkWidget *g_sidebar_box   = NULL;
static GtkWidget *g_back_btn      = NULL;
static GtkWidget *g_next_btn      = NULL;
static GtkWidget *g_cancel_btn    = NULL;
static GtkWidget *g_step_counter  = NULL;  

static const char *QUICK_PAGES[] = {
    "welcome","language","network","mode",
    "locale","keymap","disk",
    "identity","passwords","profile","dotfiles","extra_pkgs",
    "review","preflight","install","finish", NULL
};
static const char *CUSTOM_PAGES[] = {
    "welcome","language","network","mode",
    "locale","disk","filesystem","kernel","bootloader","mirrors",
    "identity","passwords","keymap","timezone","desktop","gpu",
    "profile","dotfiles","yay","flatpak","snapper","extra_pkgs",
    "review","preflight","install","finish", NULL
};
static const char *PRE_MODE_PAGES[] = {
    "welcome","language","network","mode", NULL
};

static const char *g_custom_pages_buf[32];
static const char **g_pages = PRE_MODE_PAGES;
static int          g_cur   = 0;

static void rebuild_custom_pages(void) {
    int n = 0;
    int use_btrfs = !strcmp(st.filesystem, "btrfs");
    int use_hypr  = (strstr(st.desktop_list, "Hyprland") != NULL);
    g_custom_pages_buf[n++] = "welcome";
    g_custom_pages_buf[n++] = "language";
    g_custom_pages_buf[n++] = "network";
    g_custom_pages_buf[n++] = "mode";
    g_custom_pages_buf[n++] = "locale";
    g_custom_pages_buf[n++] = "disk";
    g_custom_pages_buf[n++] = "filesystem";
    g_custom_pages_buf[n++] = "kernel";
    g_custom_pages_buf[n++] = "bootloader";
    g_custom_pages_buf[n++] = "mirrors";
    g_custom_pages_buf[n++] = "identity";
    g_custom_pages_buf[n++] = "passwords";
    g_custom_pages_buf[n++] = "keymap";
    g_custom_pages_buf[n++] = "timezone";
    g_custom_pages_buf[n++] = "desktop";
    g_custom_pages_buf[n++] = "gpu";
    g_custom_pages_buf[n++] = "profile";
    if (use_hypr)  g_custom_pages_buf[n++] = "dotfiles";
    g_custom_pages_buf[n++] = "yay";
    g_custom_pages_buf[n++] = "flatpak";
    if (use_btrfs) g_custom_pages_buf[n++] = "snapper";
    g_custom_pages_buf[n++] = "extra_pkgs";
    g_custom_pages_buf[n++] = "review";
    g_custom_pages_buf[n++] = "preflight";
    g_custom_pages_buf[n++] = "install";
    g_custom_pages_buf[n++] = "finish";
    g_custom_pages_buf[n]   = NULL;
    g_pages = g_custom_pages_buf;
}

typedef struct { const char *id; const char *en; const char *es; } PageMeta;
static const PageMeta PAGE_META[] = {
    {"welcome",    "Welcome",          "Bienvenida"},
    {"language",   "Language",         "Idioma"},
    {"network",    "Network",          "Red"},
    {"mode",       "Install Mode",     "Modo"},
    {"locale",     "System Locale",    "Locale"},
    {"keymap",     "Keyboard",         "Teclado"},
    {"disk",       "Disk",             "Disco"},
    {"filesystem", "Filesystem",       "Sistema arch."},
    {"kernel",     "Kernel",           "Kernel"},
    {"bootloader", "Bootloader",       "Bootloader"},
    {"mirrors",    "Mirrors",          "Mirrors"},
    {"identity",   "Identity",         "Identidad"},
    {"passwords",  "Passwords",        "Contraseñas"},
    {"timezone",   "Timezone",         "Zona horaria"},
    {"desktop",    "Desktop",          "Escritorio"},
    {"gpu",        "GPU Drivers",      "Drivers GPU"},
    {"profile",    "Profile",          "Perfil"},
    {"dotfiles",   "Dotfiles",         "Dotfiles"},
    {"yay",        "AUR Helper",       "AUR Helper"},
    {"flatpak",    "Flatpak",          "Flatpak"},
    {"snapper",    "Snapshots",        "Snapshots"},
    {"extra_pkgs", "Extra Packages",   "Paquetes extra"},
    {"review",     "Review",           "Revisión"},
    {"preflight",  "Pre-check",        "Verificación"},
    {"install",    "Installing",       "Instalando"},
    {"finish",     "Done!",            "¡Listo!"},
    {NULL,NULL,NULL}
};

static const char *page_title(const char *id) {
    for (int i = 0; PAGE_META[i].id; i++)
        if (!strcmp(PAGE_META[i].id, id))
            return strcmp(st.lang,"en")==0 ? PAGE_META[i].en : PAGE_META[i].es;
    return id;
}

static int page_count(void) {
    int n=0; while (g_pages[n]) n++; return n;
}

static void update_sidebar(void) {
    GList *children = gtk_container_get_children(GTK_CONTAINER(g_sidebar_box));
    for (GList *l = children; l; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    int total = page_count();
    for (int i = 0; i < total; i++) {
        const char *id = g_pages[i];
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        add_class(row, "step-row");
        if (i == g_cur)   add_class(row, "active");
        else if (i < g_cur) add_class(row, "done");

        char num[8]; snprintf(num, sizeof(num), "%d", i+1);
        GtkWidget *num_lbl = gtk_label_new(num);
        add_class(num_lbl, "step-num");
        gtk_box_pack_start(GTK_BOX(row), num_lbl, FALSE, FALSE, 0);

        GtkWidget *name_lbl = gtk_label_new(page_title(id));
        add_class(name_lbl, "step-name");
        if (i == g_cur)    add_class(name_lbl, "active");
        else if (i < g_cur) add_class(name_lbl, "done");
        gtk_label_set_xalign(GTK_LABEL(name_lbl), 0.0f);
        gtk_box_pack_start(GTK_BOX(row), name_lbl, TRUE, TRUE, 0);

        gtk_box_pack_start(GTK_BOX(g_sidebar_box), row, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(g_sidebar_box);

    if (g_step_counter) {
        char buf[32];
        snprintf(buf, sizeof(buf), "%d / %d", g_cur+1, total);
        gtk_label_set_text(GTK_LABEL(g_step_counter), buf);
    }
}

static void update_nav_buttons(void) {
    const char *id = g_pages[g_cur];
    gboolean is_last  = (g_pages[g_cur+1] == NULL);
    gboolean is_first = (g_cur == 0);
    gboolean is_install= !strcmp(id,"install");
    gboolean is_finish = !strcmp(id,"finish");

    gtk_widget_set_sensitive(g_back_btn, !is_first && !is_install && !is_finish);

    if (is_finish)
        gtk_button_set_label(GTK_BUTTON(g_next_btn), L("Reboot","Reiniciar"));
    else if (is_install)
        gtk_button_set_label(GTK_BUTTON(g_next_btn), "…");
    else if (is_last)
        gtk_button_set_label(GTK_BUTTON(g_next_btn), L("Finish ✓","Finalizar ✓"));
    else
        gtk_button_set_label(GTK_BUTTON(g_next_btn), L("Next →","Siguiente →"));

    gtk_widget_set_sensitive(g_cancel_btn, !is_install && !is_finish);
}

static void goto_page(int idx) {
    g_cur = idx;
    gtk_stack_set_visible_child_name(GTK_STACK(g_stack), g_pages[idx]);
    update_sidebar();
    update_nav_buttons();
}


static GtkWidget *W_boot_badge = NULL;

static GtkWidget *W_lang_en = NULL;
static GtkWidget *W_lang_es = NULL;

static GtkWidget *W_net_status = NULL;

static GtkWidget *W_locale_combo = NULL;

static GtkWidget *W_keymap_combo = NULL;

static GtkWidget *W_disk_lb       = NULL;  
static GtkWidget *W_swap_entry    = NULL;
static GtkWidget *W_dualboot_chk  = NULL;
static GtkWidget *W_dbsize_entry  = NULL;
static GtkWidget *W_dbsize_box    = NULL;
static DiskInfo   W_disks[32];
static int        W_ndisks = 0;

static GtkWidget *W_fs_radios[4];
static const char *FS_IDS[] = {"ext4","btrfs","xfs","zfs"};

static GtkWidget *W_kern_checks[5];
static const char *KERN_IDS[] = {"linux","linux-lts","linux-zen","linux-hardened","linux-cachyos"};

static GtkWidget *W_bl_radios[3];
static const char *BL_IDS[] = {"grub","systemd-boot","limine"};

static GtkWidget *W_mirrors_chk = NULL;

static GtkWidget *W_hostname_e = NULL;
static GtkWidget *W_username_e = NULL;
static GtkWidget *W_id_err     = NULL;

static GtkWidget *W_rpass1 = NULL, *W_rpass2 = NULL;
static GtkWidget *W_upass1 = NULL, *W_upass2 = NULL;
static GtkWidget *W_rstr   = NULL, *W_ustr   = NULL;
static GtkWidget *W_pass_err = NULL;

static GtkWidget *W_tz_region = NULL;
static GtkWidget *W_tz_city   = NULL;

static GtkWidget *W_de_checks[9];
static const char *DE_IDS[] = {"KDE Plasma","GNOME","Cinnamon","XFCE","MATE","LXQt","Hyprland","Sway","None"};

static GtkWidget *W_gpu_combo     = NULL;
static GtkWidget *W_optimus_box   = NULL;
static GtkWidget *W_optimus_combo = NULL;

static GtkWidget *W_prof_radios[5];
static const char *PROF_IDS[] = {"none","gaming","developer","minimal","privacy"};

static GtkWidget *W_dot_radios[3];
static GtkWidget *W_dot_url_box = NULL;
static GtkWidget *W_dot_url     = NULL;
static const char *DOT_IDS[]  = {"none","caelestia","custom"};

static GtkWidget *W_yay_yes     = NULL;
static GtkWidget *W_flatpak_yes = NULL;
static GtkWidget *W_snapper_yes = NULL;

#define MAX_EXTRA_PKGS 32
static GtkWidget  *W_pkg_checks[MAX_EXTRA_PKGS];
static const char *W_pkg_ids[MAX_EXTRA_PKGS];
static int         W_npkgs = 0;

static GtkWidget *W_review_tv = NULL;

static GtkWidget *W_pre_tv  = NULL;
static GtkWidget *W_pre_ok  = NULL;
static int        W_pre_result = 0;

static GtkWidget *W_inst_prog  = NULL;
static GtkWidget *W_inst_stage = NULL;
static GtkWidget *W_inst_tv    = NULL;
static volatile int W_inst_done    = 0;
static volatile int W_inst_success = 0;
static char         W_inst_reason[1024] = {0};

typedef struct {
    int    type; 
    double pct;
    char   msg[1024];
    int    success;
} IdleUpd;

static gboolean install_idle_cb(gpointer data) {
    IdleUpd *u = data;
    switch (u->type) {
    case 0:
        if (W_inst_prog) {
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(W_inst_prog), u->pct/100.0);
            char s[16]; snprintf(s,sizeof(s),"%.0f%%",u->pct);
            gtk_progress_bar_set_text(GTK_PROGRESS_BAR(W_inst_prog), s);
        }
        break;
    case 1:
        if (W_inst_stage) gtk_label_set_text(GTK_LABEL(W_inst_stage), u->msg);
        if (W_inst_tv) {
            GtkTextBuffer *b = gtk_text_view_get_buffer(GTK_TEXT_VIEW(W_inst_tv));
            GtkTextIter end;
            gtk_text_buffer_get_end_iter(b,&end);
            gtk_text_buffer_insert(b,&end,u->msg,-1);
            gtk_text_buffer_insert(b,&end,"\n",-1);
            GtkTextMark *m = gtk_text_buffer_get_insert(b);
            gtk_text_view_scroll_mark_onscreen(GTK_TEXT_VIEW(W_inst_tv),m);
        }
        break;
    case 2:
        W_inst_done    = 1;
        W_inst_success = u->success;
        strncpy(W_inst_reason, u->msg, sizeof(W_inst_reason)-1);
        if (u->success) {
            if (W_inst_stage)
                gtk_label_set_text(GTK_LABEL(W_inst_stage),
                    L("✓  Installation complete! Click Next to reboot.",
                      "✓  ¡Instalación completa! Haz clic en Siguiente para reiniciar."));
            if (W_inst_prog) gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(W_inst_prog),1.0);
        } else {
            if (W_inst_stage) {
                char err[1100];
                snprintf(err,sizeof(err),
                    L("✗  Installation failed: %s","✗  Error en instalación: %s"), u->msg);
                gtk_label_set_text(GTK_LABEL(W_inst_stage), err);
            }
        }
        gtk_button_set_label(GTK_BUTTON(g_next_btn),
            u->success ? L("Next →","Siguiente →") : L("Retry","Reintentar"));
        gtk_widget_set_sensitive(g_next_btn, TRUE);
        gtk_widget_set_sensitive(g_back_btn, !u->success);
        break;
    }
    g_free(u);
    return G_SOURCE_REMOVE;
}

static void inst_on_progress(double pct, void *ud) {
    (void)ud;
    IdleUpd *u = g_new0(IdleUpd,1);
    u->type = 0; u->pct = pct;
    g_idle_add(install_idle_cb, u);
}

static void inst_on_stage(const char *msg, void *ud) {
    (void)ud;
    IdleUpd *u = g_new0(IdleUpd,1);
    u->type = 1;
    strncpy(u->msg, msg, sizeof(u->msg)-1);
    g_idle_add(install_idle_cb, u);
    write_log_fmt(">>> %s", msg);
}

static void inst_on_done(int ok, const char *reason, void *ud) {
    (void)ud;
    IdleUpd *u = g_new0(IdleUpd,1);
    u->type = 2; u->success = ok;
    if (reason) strncpy(u->msg, reason, sizeof(u->msg)-1);
    g_idle_add(install_idle_cb, u);
}

static GtkWidget *make_page_wrap(void) {
    GtkWidget *w = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    add_class(w, "page-wrap");
    return w;
}

static GtkWidget *make_title(const char *t) {
    GtkWidget *l = gtk_label_new(t);
    add_class(l, "page-title");
    gtk_label_set_xalign(GTK_LABEL(l),0.0f);
    return l;
}

static GtkWidget *make_sub(const char *t) {
    GtkWidget *l = gtk_label_new(t);
    add_class(l, "page-sub");
    gtk_label_set_xalign(GTK_LABEL(l),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(l), TRUE);
    return l;
}

static GtkWidget *make_divider(void) {
    GtkWidget *s = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    add_class(s, "divider");
    return s;
}

static GtkWidget *make_card(void) {
    GtkWidget *c = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    add_class(c, "card");
    return c;
}

static GtkWidget *make_field_row(const char *label_text, GtkWidget *widget) {
    GtkWidget *row  = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *lbl  = gtk_label_new(label_text);
    gtk_label_set_xalign(GTK_LABEL(lbl), 0.0f);
    gtk_widget_set_margin_bottom(lbl, 2);
    gtk_box_pack_start(GTK_BOX(row), lbl, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(row), widget, FALSE, FALSE, 0);
    return row;
}

static GtkWidget *build_welcome(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 20);
    gtk_widget_set_valign(vbox, GTK_ALIGN_CENTER);
    gtk_widget_set_halign(vbox, GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(page), vbox, TRUE, TRUE, 0);

    GtkWidget *logo = gtk_label_new(
        "/\\   \n"
        "/  \\  \n"
        " / /\\ \\ \n"
        "/_/  \\_\\");
    add_class(logo, "welcome-title");
    gtk_label_set_justify(GTK_LABEL(logo), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), logo, FALSE, FALSE, 0);

    GtkWidget *title = gtk_label_new("Arch Linux Installer");
    add_class(title, "welcome-title");
    gtk_label_set_justify(GTK_LABEL(title), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

    GtkWidget *ver = gtk_label_new(VERSION);
    add_class(ver, "welcome-ver");
    gtk_label_set_justify(GTK_LABEL(ver), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), ver, FALSE, FALSE, 0);

    W_boot_badge = gtk_label_new(is_uefi() ? "UEFI  " : "BIOS  ");
    add_class(W_boot_badge, is_uefi() ? "badge-uefi" : "badge-bios");
    gtk_label_set_justify(GTK_LABEL(W_boot_badge), GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox), W_boot_badge, FALSE, FALSE, 0);

    GtkWidget *sep = make_divider();
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 10);

    GtkWidget *warn = gtk_label_new(
        L("⚠  This installer will ERASE the selected disk and install Arch Linux.\n"
          "Make sure you have backups.  Press Next to continue.",
          "⚠  Este instalador BORRARÁ el disco seleccionado e instalará Arch Linux.\n"
          "Asegúrate de tener copias de seguridad.  Pulsa Siguiente para continuar."));
    gtk_label_set_justify(GTK_LABEL(warn), GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(warn), TRUE);
    add_class(warn, "warn");
    gtk_box_pack_start(GTK_BOX(vbox), warn, FALSE, FALSE, 0);

    return page;
}

static GtkWidget *build_language(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),
        GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 16);
    gtk_container_set_border_width(GTK_CONTAINER(vbox),0);

    gtk_box_pack_start(GTK_BOX(vbox), make_title(
        "Language / Idioma"), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), make_sub(
        "Choose the installer language:\nSeleccione el idioma del instalador:"),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox), make_divider(), FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);

    W_lang_en = gtk_button_new_with_label("English");
    W_lang_es = gtk_button_new_with_label("Español");
    gtk_widget_set_size_request(W_lang_en, 180, 60);
    gtk_widget_set_size_request(W_lang_es, 180, 60);
    add_class(W_lang_en, "large-option");
    add_class(W_lang_es, "large-option");

    if (!strcmp(st.lang,"en")) add_class(W_lang_en,"selected");
    else                       add_class(W_lang_es,"selected");

    gtk_box_pack_start(GTK_BOX(hbox), W_lang_en, FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox), W_lang_es, FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(card), hbox, FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox), card, FALSE,FALSE,0);

    gtk_container_add(GTK_CONTAINER(sv), vbox);
    gtk_box_pack_start(GTK_BOX(page), sv, TRUE, TRUE, 0);
    return page;
}

static GtkWidget *build_network(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),
        GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Network Connection","Conexión de red")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("An internet connection is needed to download Arch Linux.",
                   "Se necesita internet para descargar Arch Linux.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox), make_divider(), FALSE,FALSE,0);

    GtkWidget *card = make_card();

    W_net_status = gtk_label_new(L("Status: unknown","Estado: desconocido"));
    gtk_label_set_xalign(GTK_LABEL(W_net_status),0.0f);
    gtk_box_pack_start(GTK_BOX(card), W_net_status, FALSE,FALSE,4);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
    GtkWidget *btn_check = gtk_button_new_with_label(
        L("Check Wired","Verificar cable"));
    GtkWidget *btn_wifi  = gtk_button_new_with_label(
        L("Connect WiFi","Conectar WiFi"));
    add_class(btn_check,"large-option");
    add_class(btn_wifi, "large-option");
    gtk_widget_set_size_request(btn_check, 200,56);
    gtk_widget_set_size_request(btn_wifi,  200,56);
    gtk_box_pack_start(GTK_BOX(hbox),btn_check,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(hbox),btn_wifi, FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(card),hbox,FALSE,FALSE,8);

    g_signal_connect(btn_check,"clicked",G_CALLBACK(cb_check_wired),(gpointer)W_net_status);
    g_signal_connect(btn_wifi, "clicked",G_CALLBACK(cb_connect_wifi),(gpointer)W_net_status);

    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);

    GtkWidget *hint = make_sub(
        L("If already connected (e.g. via Ethernet), just click Next.",
          "Si ya está conectado (p. ej. por Ethernet), haz clic en Siguiente."));
    gtk_box_pack_start(GTK_BOX(vbox),hint,FALSE,FALSE,8);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *W_mode_quick_radio = NULL;
static GtkWidget *W_mode_custom_radio = NULL;

static GtkWidget *build_mode(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),
        GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Install Mode","Modo de instalación")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Choose how to configure the installation.",
                   "Elige cómo configurar la instalación.")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *c1 = make_card();
    W_mode_quick_radio = gtk_radio_button_new_with_label(NULL,
        L("⚡  Quick Install",
          "⚡  Instalación Rápida"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_mode_quick_radio), st.quick==1);
    gtk_box_pack_start(GTK_BOX(c1),W_mode_quick_radio,FALSE,FALSE,0);
    GtkWidget *q_desc = gtk_label_new(
        L("BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
          "Sensible defaults, minimum questions.",
          "BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
          "Valores sensatos, mínimas preguntas."));
    gtk_label_set_xalign(GTK_LABEL(q_desc),0.0f);
    add_class(q_desc,"hint");
    gtk_widget_set_margin_start(q_desc,26);
    gtk_box_pack_start(GTK_BOX(c1),q_desc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),c1,FALSE,FALSE,0);

    GtkWidget *c2 = make_card();
    W_mode_custom_radio = gtk_radio_button_new_with_label(
        gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_mode_quick_radio)),
        L("Custom Install",
          "Instalación Personalizada"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_mode_custom_radio), st.quick==0);
    gtk_box_pack_start(GTK_BOX(c2),W_mode_custom_radio,FALSE,FALSE,0);
    GtkWidget *cu_desc = gtk_label_new(
        L("Choose filesystem, kernel, bootloader, desktop, GPU, and more.\n"
          "Full control over every setting.",
          "Elige sistema de archivos, kernel, bootloader, escritorio, GPU y más.\n"
          "Control total sobre cada ajuste."));
    gtk_label_set_xalign(GTK_LABEL(cu_desc),0.0f);
    add_class(cu_desc,"hint");
    gtk_widget_set_margin_start(cu_desc,26);
    gtk_box_pack_start(GTK_BOX(c2),cu_desc,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),c2,FALSE,FALSE,0);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_locale(void) {
    static const char *LOCALES[][2] = {
        {"en_US.UTF-8","English (United States)"},
        {"en_GB.UTF-8","English (United Kingdom)"},
        {"es_ES.UTF-8","Español (España)"},
        {"es_MX.UTF-8","Español (México)"},
        {"es_AR.UTF-8","Español (Argentina)"},
        {"fr_FR.UTF-8","Français (France)"},
        {"de_DE.UTF-8","Deutsch (Deutschland)"},
        {"it_IT.UTF-8","Italiano (Italia)"},
        {"pt_PT.UTF-8","Português (Portugal)"},
        {"pt_BR.UTF-8","Português (Brasil)"},
        {"ru_RU.UTF-8","Русский (Россия)"},
        {"pl_PL.UTF-8","Polski (Polska)"},
        {"nl_NL.UTF-8","Nederlands (Nederland)"},
        {"cs_CZ.UTF-8","Čeština (Česká republika)"},
        {"hu_HU.UTF-8","Magyar (Magyarország)"},
        {"ro_RO.UTF-8","Română (România)"},
        {"da_DK.UTF-8","Dansk (Danmark)"},
        {"nb_NO.UTF-8","Norsk (Norge)"},
        {"sv_SE.UTF-8","Svenska (Sverige)"},
        {"fi_FI.UTF-8","Suomi (Suomi)"},
        {"tr_TR.UTF-8","Türkçe (Türkiye)"},
        {"ja_JP.UTF-8","日本語 (日本)"},
        {"ko_KR.UTF-8","한국어 (대한민국)"},
        {"zh_CN.UTF-8","简体中文 (中国)"},
        {"ar_SA.UTF-8","العربية (السعودية)"},
        {NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("System Locale","Locale del sistema")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Choose the locale for the INSTALLED system (language, date, number formats).",
                   "Elige el locale para el sistema INSTALADO (idioma, fechas, formatos numéricos).")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    W_locale_combo = gtk_combo_box_text_new();
    int sel_idx = 0;
    for (int i = 0; LOCALES[i][0]; i++) {
        char buf[80];
        snprintf(buf,sizeof(buf),"%-32s  %s",LOCALES[i][0],LOCALES[i][1]);
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_locale_combo),
                                   LOCALES[i][0], buf);
        if (!strcmp(LOCALES[i][0], st.locale)) sel_idx = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(W_locale_combo), sel_idx);
    gtk_widget_set_size_request(W_locale_combo, 400, -1);

    GtkWidget *card = make_card();
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("Locale:","Locale:"), W_locale_combo),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_keymap(void) {
    static const char *KM_IDS[] = {
        "us","es","uk","fr","de","it","ru","ara",
        "pt-latin9","br-abnt2","pl2","hu","cz-qwerty",
        "sk-qwerty","ro_win","dk","no","sv-latin1",
        "fi","nl","tr_q-latin5","ja106","kr106", NULL
    };
    static const char *KM_NAMES[] = {
        "us — English (US)","es — Español","uk — English (UK)","fr — Français",
        "de — Deutsch","it — Italiano","ru — Русский","ara — Arabic",
        "pt-latin9 — Português (PT)","br-abnt2 — Português (BR)","pl2 — Polski",
        "hu — Magyar","cz-qwerty — Čeština","sk-qwerty — Slovenčina","ro_win — Română",
        "dk — Dansk","no — Norsk","sv-latin1 — Svenska","fi — Suomi",
        "nl — Nederlands","tr_q-latin5 — Türkçe","ja106 — 日本語","kr106 — 한국어", NULL
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Keyboard Layout","Distribución de teclado")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Applied to both the TTY console and the desktop (X11/Wayland).",
                   "Se aplica a la TTY y al escritorio (X11/Wayland).")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    W_keymap_combo = gtk_combo_box_text_new();
    int sel_idx = 0;
    for (int i = 0; KM_IDS[i]; i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_keymap_combo),
                                   KM_IDS[i], KM_NAMES[i]);
        if (!strcmp(KM_IDS[i], st.keymap)) sel_idx = i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(W_keymap_combo), sel_idx);
    gtk_widget_set_size_request(W_keymap_combo, 340, -1);

    GtkWidget *card = make_card();
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("Keyboard layout:","Distribución:"), W_keymap_combo),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void refresh_disk_list(void);

static GtkWidget *build_disk(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Disk","Disco")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Select the disk where Arch Linux will be installed.\n"
                   "⚠  The selected disk will be completely erased.",
                   "Selecciona el disco donde se instalará Arch Linux.\n"
                   "⚠  El disco seleccionado se borrará por completo.")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *disk_card = make_card();

    GtkWidget *list_sw = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(list_sw),
        GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(list_sw,-1,180);

    W_disk_lb = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(W_disk_lb),GTK_SELECTION_SINGLE);
    add_class(W_disk_lb,"disk-list");
    gtk_container_add(GTK_CONTAINER(list_sw),W_disk_lb);
    gtk_box_pack_start(GTK_BOX(disk_card),list_sw,FALSE,FALSE,0);

    GtkWidget *btn_refresh = gtk_button_new_with_label(
        L("Refresh disk list","Actualizar lista de discos"));
    g_signal_connect(btn_refresh,"clicked",G_CALLBACK(cb_refresh_disks),NULL);
    gtk_box_pack_start(GTK_BOX(disk_card),btn_refresh,FALSE,FALSE,4);

    gtk_box_pack_start(GTK_BOX(vbox),disk_card,FALSE,FALSE,0);

    GtkWidget *opt_card = make_card();
    GtkWidget *opt_title = gtk_label_new(L("Options","Opciones"));
    add_class(opt_title,"card-title");
    gtk_label_set_xalign(GTK_LABEL(opt_title),0.0f);
    gtk_box_pack_start(GTK_BOX(opt_card),opt_title,FALSE,FALSE,0);

    W_dualboot_chk = gtk_check_button_new_with_label(
        L("Dual boot  (create new partition in free space, keep existing OS)",
          "Dual boot  (crear nueva partición en espacio libre, conservar OS actual)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_dualboot_chk), st.dualboot);
    gtk_box_pack_start(GTK_BOX(opt_card),W_dualboot_chk,FALSE,FALSE,4);

    W_dbsize_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    GtkWidget *db_lbl = gtk_label_new(L("Arch partition size (GB):","Tamaño partición Arch (GB):"));
    gtk_label_set_xalign(GTK_LABEL(db_lbl),0.0f);
    W_dbsize_entry = gtk_entry_new();
    char db_str[8]; snprintf(db_str,sizeof(db_str),"%d",st.db_size_gb>0?st.db_size_gb:40);
    gtk_entry_set_text(GTK_ENTRY(W_dbsize_entry),db_str);
    gtk_widget_set_size_request(W_dbsize_entry,80,-1);
    gtk_widget_set_margin_start(W_dbsize_box,20);
    gtk_box_pack_start(GTK_BOX(W_dbsize_box),db_lbl,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(W_dbsize_box),W_dbsize_entry,FALSE,FALSE,0);
    gtk_widget_set_no_show_all(W_dbsize_box,TRUE);
    if (st.dualboot) gtk_widget_show_all(W_dbsize_box); else gtk_widget_hide(W_dbsize_box);
    gtk_box_pack_start(GTK_BOX(opt_card),W_dbsize_box,FALSE,FALSE,0);

    g_signal_connect(W_dualboot_chk,"toggled",G_CALLBACK(cb_dualboot_toggled),(gpointer)W_dbsize_box);

    char sw_str[8];
    snprintf(sw_str,sizeof(sw_str),"%s",st.swap[0]?st.swap:"8");
    W_swap_entry = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(W_swap_entry),sw_str);
    gtk_widget_set_size_request(W_swap_entry,80,-1);
    GtkWidget *sw_row = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,8);
    GtkWidget *sw_lbl = gtk_label_new(L("Swap size (GB, 1-128):","Tamaño swap (GB, 1-128):"));
    gtk_label_set_xalign(GTK_LABEL(sw_lbl),0.0f);
    gtk_box_pack_start(GTK_BOX(sw_row),sw_lbl,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(sw_row),W_swap_entry,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(opt_card),sw_row,FALSE,FALSE,4);

    gtk_box_pack_start(GTK_BOX(vbox),opt_card,FALSE,FALSE,0);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);


    return page;
}

static void refresh_disk_list(void) {
    if (!W_disk_lb) return;
    GList *ch = gtk_container_get_children(GTK_CONTAINER(W_disk_lb));
    for (GList *l=ch;l;l=l->next) gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(ch);

    W_ndisks = list_disks(W_disks, 32);
    for (int i=0;i<W_ndisks;i++) {
        GtkWidget *rbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        gtk_container_set_border_width(GTK_CONTAINER(rbox),4);
        char title[128];
        int ssd = is_ssd(W_disks[i].name);
        snprintf(title,sizeof(title),"/dev/%s   —   %lld GB   %s",
                 W_disks[i].name, W_disks[i].size_gb,
                 ssd>0?"SSD":"HDD");
        GtkWidget *t = gtk_label_new(title);
        gtk_label_set_xalign(GTK_LABEL(t),0.0f);
        add_class(t,"list-tag");
        GtkWidget *m = gtk_label_new(W_disks[i].model);
        gtk_label_set_xalign(GTK_LABEL(m),0.0f);
        add_class(m,"list-desc");
        gtk_box_pack_start(GTK_BOX(rbox),t,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(rbox),m,FALSE,FALSE,0);
        gtk_container_add(GTK_CONTAINER(W_disk_lb),rbox);
    }

    int pre = 0;
    if (st.disk[0]) {
        for (int i=0;i<W_ndisks;i++)
            if (!strcmp(W_disks[i].name,st.disk)) { pre=i; break; }
    }
    GtkListBoxRow *r0 = gtk_list_box_get_row_at_index(GTK_LIST_BOX(W_disk_lb),pre);
    if (r0) gtk_list_box_select_row(GTK_LIST_BOX(W_disk_lb),r0);
    gtk_widget_show_all(W_disk_lb);
}

static GtkWidget *build_filesystem(void) {
    static const struct { const char *id; const char *en; const char *es; } FS[] = {
        {"ext4",  "ext4 — Stable, fast, universal.  No snapshots.",
                  "ext4 — Estable, rápido, universal.  Sin snapshots."},
        {"btrfs", "btrfs — Snapshots + compression.  Slightly more complex.",
                  "btrfs — Snapshots + compresión.  Algo más complejo."},
        {"xfs",   "xfs — Great for large files.  No snapshots, no shrink.",
                  "xfs — Excelente para archivos grandes.  Sin snapshots."},
        {"zfs",   "zfs — Advanced checksums.  [EXPERIMENTAL] complex setup.",
                  "zfs — Checksums avanzados.  [EXPERIMENTAL] configuración compleja."},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Filesystem","Sistema de archivos")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Choose the filesystem for the root partition.\n"
                   "For most users: ext4 (simple) or btrfs (snapshots + compression).",
                   "Elige el sistema de archivos para la partición raíz.\n"
                   "Para la mayoría: ext4 (simple) o btrfs (snapshots + compresión).")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp = NULL;
    for (int i=0;FS[i].id;i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        W_fs_radios[i] = gtk_radio_button_new_with_label(grp, FS[i].id);
        grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_fs_radios[i]));
        if (!strcmp(FS[i].id, st.filesystem))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_fs_radios[i]),TRUE);
        gtk_box_pack_start(GTK_BOX(row),W_fs_radios[i],FALSE,FALSE,0);
        GtkWidget *d = gtk_label_new(strcmp(st.lang,"en")==0 ? FS[i].en : FS[i].es);
        gtk_label_set_xalign(GTK_LABEL(d),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(d),TRUE);
        add_class(d,"hint");
        gtk_widget_set_margin_start(d,26);
        gtk_box_pack_start(GTK_BOX(row),d,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(card),row,FALSE,FALSE,4);
    }
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_kernel(void) {
    static const struct { const char *id; const char *en; const char *es; } KN[] = {
        {"linux",           "linux — Latest stable. Best hardware support. (recommended)",
                            "linux — Último estable. Mejor soporte de hardware. (recomendado)"},
        {"linux-lts",       "linux-lts — Rock-solid LTS. Longer support cycle.",
                            "linux-lts — LTS muy estable. Ciclo de soporte largo."},
        {"linux-zen",       "linux-zen — Desktop / gaming tweaks. Slightly more power usage.",
                            "linux-zen — Optimizado escritorio/gaming. Algo más consumo."},
        {"linux-hardened",  "linux-hardened — Security patches. Some apps may break.",
                            "linux-hardened — Parches seguridad. Algunas apps pueden fallar."},
        {"linux-cachyos",   "linux-cachyos — Max performance. Needs CachyOS repo.",
                            "linux-cachyos — Máximo rendimiento. Requiere repo CachyOS."},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Kernel","Kernel")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Select one or more kernels to install.  The first checked will be the default boot kernel.",
                   "Selecciona uno o más kernels.  El primero marcado será el kernel de arranque predeterminado.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    for (int i=0;KN[i].id;i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        W_kern_checks[i] = gtk_check_button_new_with_label(KN[i].id);
        int on = (strstr(st.kernel_list, KN[i].id) != NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_kern_checks[i]),on);
        gtk_box_pack_start(GTK_BOX(row),W_kern_checks[i],FALSE,FALSE,0);
        GtkWidget *d = gtk_label_new(strcmp(st.lang,"en")==0?KN[i].en:KN[i].es);
        gtk_label_set_xalign(GTK_LABEL(d),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(d),TRUE);
        add_class(d,"hint");
        gtk_widget_set_margin_start(d,26);
        gtk_box_pack_start(GTK_BOX(row),d,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(card),row,FALSE,FALSE,4);
    }
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_bootloader(void) {
    static const struct { const char *id; const char *en; const char *es; } BL[] = {
        {"grub",         "GRUB — Universal, widely supported, multi-boot friendly. (recommended)",
                         "GRUB — Universal, amplio soporte, apto para multi-boot. (recomendado)"},
        {"systemd-boot", "systemd-boot — Fast, minimal, UEFI-only.",
                         "systemd-boot — Rápido, minimalista, solo UEFI."},
        {"limine",       "limine — Modern, minimal BIOS+UEFI bootloader.",
                         "limine — Moderno y mínimo, compatible BIOS+UEFI."},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Bootloader","Gestor de arranque")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("The bootloader is the first program that runs when you start your computer.",
                   "El gestor de arranque es el primer programa que se ejecuta al encender el equipo.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp = NULL;
    for (int i=0;BL[i].id;i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        W_bl_radios[i] = gtk_radio_button_new_with_label(grp,BL[i].id);
        grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_bl_radios[i]));
        if (!strcmp(BL[i].id,st.bootloader))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_bl_radios[i]),TRUE);
        if (!strcmp(BL[i].id,"systemd-boot") && !is_uefi())
            gtk_widget_set_sensitive(W_bl_radios[i],FALSE);
        gtk_box_pack_start(GTK_BOX(row),W_bl_radios[i],FALSE,FALSE,0);
        GtkWidget *d = gtk_label_new(strcmp(st.lang,"en")==0?BL[i].en:BL[i].es);
        gtk_label_set_xalign(GTK_LABEL(d),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(d),TRUE);
        add_class(d,"hint");
        gtk_widget_set_margin_start(d,26);
        gtk_box_pack_start(GTK_BOX(row),d,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(card),row,FALSE,FALSE,4);
    }
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_mirrors(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Mirror Speed Optimization","Optimización de mirrors")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Mirrors are servers that host Arch Linux packages.",
                   "Los mirrors son servidores que alojan los paquetes de Arch Linux.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    W_mirrors_chk = gtk_check_button_new_with_label(
        L("Use reflector to automatically select the 5 fastest mirrors  (recommended)",
          "Usar reflector para seleccionar automáticamente los 5 mirrors más rápidos  (recomendado)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_mirrors_chk), st.mirrors);
    gtk_box_pack_start(GTK_BOX(card),W_mirrors_chk,FALSE,FALSE,0);

    GtkWidget *info = gtk_label_new(
        L("Reflector will test mirror speeds and update /etc/pacman.d/mirrorlist.\n"
          "This adds ~30 seconds to the start of the installation but speeds up package downloads.\n"
          "Disable if you have a very slow internet connection.",
          "Reflector mide la velocidad de los mirrors y actualiza /etc/pacman.d/mirrorlist.\n"
          "Añade ~30 segundos al inicio de la instalación pero acelera la descarga de paquetes.\n"
          "Desactívalo si tienes una conexión muy lenta."));
    gtk_label_set_xalign(GTK_LABEL(info),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(info),TRUE);
    add_class(info,"hint");
    gtk_box_pack_start(GTK_BOX(card),info,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_identity(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Identity","Identidad")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Set your computer name and personal username.",
                   "Establece el nombre del equipo y tu nombre de usuario personal.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();

    W_hostname_e = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_hostname_e),"arch-pc");
    if (st.hostname[0]) gtk_entry_set_text(GTK_ENTRY(W_hostname_e),st.hostname);
    gtk_widget_set_size_request(W_hostname_e,320,-1);
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("Computer name (hostname):","Nombre del equipo (hostname):"),
                       W_hostname_e),FALSE,FALSE,0);

    GtkWidget *hn_hint = gtk_label_new(
        L("Letters, numbers, hyphens.  Max 32 chars.  Must start with a letter.  Example: my-arch",
          "Letras, números, guiones.  Máx 32 caracteres.  Debe empezar con letra.  Ejemplo: mi-arch"));
    gtk_label_set_xalign(GTK_LABEL(hn_hint),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(hn_hint),TRUE);
    add_class(hn_hint,"hint");
    gtk_widget_set_margin_start(hn_hint,4);
    gtk_box_pack_start(GTK_BOX(card),hn_hint,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(card),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,8);

    W_username_e = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_username_e),"alice");
    if (st.username[0]) gtk_entry_set_text(GTK_ENTRY(W_username_e),st.username);
    gtk_widget_set_size_request(W_username_e,320,-1);
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("Username:","Nombre de usuario:"), W_username_e),FALSE,FALSE,0);

    GtkWidget *un_hint = gtk_label_new(
        L("Lowercase letters, numbers, hyphens.  Max 32 chars.  Example: alice",
          "Letras minúsculas, números, guiones.  Máx 32 caracteres.  Ejemplo: alice"));
    gtk_label_set_xalign(GTK_LABEL(un_hint),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(un_hint),TRUE);
    add_class(un_hint,"hint");
    gtk_widget_set_margin_start(un_hint,4);
    gtk_box_pack_start(GTK_BOX(card),un_hint,FALSE,FALSE,0);

    W_id_err = gtk_label_new("");
    add_class(W_id_err,"warn");
    gtk_label_set_xalign(GTK_LABEL(W_id_err),0.0f);
    gtk_box_pack_start(GTK_BOX(card),W_id_err,FALSE,FALSE,4);

    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void update_strength_label(GtkEntry *e, gpointer lbl) {
    const char *p = gtk_entry_get_text(e);
    int s = password_strength(p);
    const char *texts_en[] = {"(empty)","⚠ WEAK","● MEDIUM","✓ STRONG"};
    const char *texts_es[] = {"(vacía)","⚠ DÉBIL","● MEDIA","✓ FUERTE"};
    const char *txt = (strcmp(st.lang,"en")==0?texts_en:texts_es)[s];
    const char *cls[] = {"","strength-weak","strength-medium","strength-strong"};
    GtkWidget *l = GTK_WIDGET(lbl);
    gtk_label_set_text(GTK_LABEL(l),txt);
    GtkStyleContext *ctx = gtk_widget_get_style_context(l);
    gtk_style_context_remove_class(ctx,"strength-weak");
    gtk_style_context_remove_class(ctx,"strength-medium");
    gtk_style_context_remove_class(ctx,"strength-strong");
    if (s>0) gtk_style_context_add_class(ctx,cls[s]);
}

static GtkWidget *build_passwords(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Passwords","Contraseñas")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Set passwords for the root (admin) and your personal user account.",
                   "Establece contraseñas para root (administrador) y tu cuenta personal.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *rc = make_card();
    GtkWidget *rt = gtk_label_new(L("🔑  Root Password  (administrator)","🔑  Contraseña de Root  (administrador)"));
    gtk_label_set_xalign(GTK_LABEL(rt),0.0f);
    add_class(rt,"card-title");
    gtk_box_pack_start(GTK_BOX(rc),rt,FALSE,FALSE,0);

    W_rpass1 = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(W_rpass1),FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_rpass1),L("Root password","Contraseña root"));
    if (st.root_pass[0]) gtk_entry_set_text(GTK_ENTRY(W_rpass1),st.root_pass);
    gtk_widget_set_size_request(W_rpass1,320,-1);
    gtk_box_pack_start(GTK_BOX(rc),
        make_field_row(L("Password:","Contraseña:"),W_rpass1),FALSE,FALSE,4);

    W_rpass2 = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(W_rpass2),FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_rpass2),L("Confirm root password","Confirmar contraseña root"));
    if (st.root_pass[0]) gtk_entry_set_text(GTK_ENTRY(W_rpass2),st.root_pass);
    gtk_widget_set_size_request(W_rpass2,320,-1);
    gtk_box_pack_start(GTK_BOX(rc),
        make_field_row(L("Confirm:","Confirmar:"),W_rpass2),FALSE,FALSE,0);

    W_rstr = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(W_rstr),0.0f);
    gtk_box_pack_start(GTK_BOX(rc),W_rstr,FALSE,FALSE,2);
    g_signal_connect(W_rpass1,"changed",G_CALLBACK(update_strength_label),(gpointer)W_rstr);
    gtk_box_pack_start(GTK_BOX(vbox),rc,FALSE,FALSE,0);

    GtkWidget *uc = make_card();
    char ulbl_text[128];
    snprintf(ulbl_text,sizeof(ulbl_text),
        L("👤  User Password  (%s)","👤  Contraseña de usuario  (%s)"),
        st.username[0] ? st.username : "your user");
    GtkWidget *ut = gtk_label_new(ulbl_text);
    gtk_label_set_xalign(GTK_LABEL(ut),0.0f);
    add_class(ut,"card-title");
    gtk_box_pack_start(GTK_BOX(uc),ut,FALSE,FALSE,0);

    W_upass1 = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(W_upass1),FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_upass1),L("User password","Contraseña usuario"));
    if (st.user_pass[0]) gtk_entry_set_text(GTK_ENTRY(W_upass1),st.user_pass);
    gtk_widget_set_size_request(W_upass1,320,-1);
    gtk_box_pack_start(GTK_BOX(uc),
        make_field_row(L("Password:","Contraseña:"),W_upass1),FALSE,FALSE,4);

    W_upass2 = gtk_entry_new();
    gtk_entry_set_visibility(GTK_ENTRY(W_upass2),FALSE);
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_upass2),L("Confirm user password","Confirmar contraseña usuario"));
    if (st.user_pass[0]) gtk_entry_set_text(GTK_ENTRY(W_upass2),st.user_pass);
    gtk_widget_set_size_request(W_upass2,320,-1);
    gtk_box_pack_start(GTK_BOX(uc),
        make_field_row(L("Confirm:","Confirmar:"),W_upass2),FALSE,FALSE,0);

    W_ustr = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(W_ustr),0.0f);
    gtk_box_pack_start(GTK_BOX(uc),W_ustr,FALSE,FALSE,2);
    g_signal_connect(W_upass1,"changed",G_CALLBACK(update_strength_label),(gpointer)W_ustr);
    gtk_box_pack_start(GTK_BOX(vbox),uc,FALSE,FALSE,0);

    GtkWidget *show_chk = gtk_check_button_new_with_label(
        L("Show passwords","Mostrar contraseñas"));
    typedef struct { GtkWidget *a,*b,*c,*d; } FourEntries;
    FourEntries *fe = g_new0(FourEntries,1);
    fe->a=W_rpass1; fe->b=W_rpass2; fe->c=W_upass1; fe->d=W_upass2;
    g_signal_connect_data(show_chk,"toggled",
        G_CALLBACK(cb_show_passwords),(gpointer)fe,(GClosureNotify)g_free,(GConnectFlags)0);
    gtk_box_pack_start(GTK_BOX(vbox),show_chk,FALSE,FALSE,4);

    W_pass_err = gtk_label_new("");
    add_class(W_pass_err,"warn");
    gtk_label_set_xalign(GTK_LABEL(W_pass_err),0.0f);
    gtk_box_pack_start(GTK_BOX(vbox),W_pass_err,FALSE,FALSE,4);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void tz_region_changed(GtkComboBoxText *reg, gpointer city_combo);

static GtkWidget *build_timezone(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Timezone","Zona horaria")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Select your timezone. This sets the system clock for the installed system.",
                   "Selecciona tu zona horaria. Configura el reloj del sistema instalado.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    char zones_raw[65536]={0};
    FILE *fp = popen("timedatectl list-timezones 2>/dev/null","r");
    if (fp) { (void)fread(zones_raw,1,sizeof(zones_raw)-1,fp); pclose(fp); }

    char *regions[256]; int nr=0;
    char zones_copy[65536];
    strncpy(zones_copy,zones_raw,sizeof(zones_copy)-1);
    char *p=zones_copy, *nl;
    while (*p && (nl=strchr(p,'\n'))) {
        *nl='\0';
        char *sl=strchr(p,'/');
        if (sl) {
            int rlen=sl-p; if(rlen<1){p=nl+1;continue;}
            char reg[64]={0}; strncpy(reg,p,rlen<63?rlen:63);
            int dup=0;
            for(int j=0;j<nr;j++) if(!strcmp(regions[j],reg)){dup=1;break;}
            if(!dup && nr<255) regions[nr++]=strdup(reg);
        }
        p=nl+1;
    }

    W_tz_region = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_tz_region),"UTC","UTC");
    int sel_reg=0;
    char cur_reg[64]="UTC";
    if (strchr(st.timezone,'/')) {
        char *sl=strchr(st.timezone,'/');
        int l=sl-st.timezone; if(l>63)l=63;
        strncpy(cur_reg,st.timezone,l); cur_reg[l]='\0';
    }
    for (int i=0;i<nr;i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_tz_region),regions[i],regions[i]);
        if (!strcmp(regions[i],cur_reg)) sel_reg=i+1;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(W_tz_region),sel_reg);

    W_tz_city = gtk_combo_box_text_new();

    g_signal_connect(W_tz_region,"changed",G_CALLBACK(tz_region_changed),(gpointer)W_tz_city);
    tz_region_changed(GTK_COMBO_BOX_TEXT(W_tz_region),(gpointer)W_tz_city);

    GtkWidget *card = make_card();
    gtk_widget_set_size_request(W_tz_region,300,-1);
    gtk_widget_set_size_request(W_tz_city,  300,-1);
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("Region:","Región:"),W_tz_region),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(card),gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,6);
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("City:","Ciudad:"),W_tz_city),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);

    for(int i=0;i<nr;i++) free(regions[i]);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void tz_region_changed(GtkComboBoxText *reg, gpointer city_widget) {
    GtkComboBoxText *city = GTK_COMBO_BOX_TEXT(city_widget);
    gtk_combo_box_text_remove_all(city);

    const char *sel_reg = gtk_combo_box_get_active_id(GTK_COMBO_BOX(reg));
    if (!sel_reg || !strcmp(sel_reg,"UTC")) { return; }

    char zones_raw[65536]={0};
    FILE *fp = popen("timedatectl list-timezones 2>/dev/null","r");
    if (fp) { (void)fread(zones_raw,1,sizeof(zones_raw)-1,fp); pclose(fp); }

    char *p=zones_raw, *nl;
    char cur_city[64]="";
    if (strchr(st.timezone,'/')) strncpy(cur_city,strchr(st.timezone,'/')+1,63);
    int sel_idx=0, idx=0;
    while (*p && (nl=strchr(p,'\n'))) {
        *nl='\0';
        char *sl=strchr(p,'/'); if(!sl){p=nl+1;continue;}
        int rlen=sl-p;
        if (rlen==(int)strlen(sel_reg) && !strncmp(p,sel_reg,rlen)) {
            gtk_combo_box_text_append(city,sl+1,sl+1);
            if (!strcmp(sl+1,cur_city)) sel_idx=idx;
            idx++;
        }
        p=nl+1;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(city),sel_idx);
}

static void cb_de_none_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    if (!gtk_toggle_button_get_active(btn)) return;
    for (int i = 0; i < 8; i++)
        if (W_de_checks[i])
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_de_checks[i]), FALSE);
}
static void cb_de_other_toggled(GtkToggleButton *btn, gpointer data) {
    (void)data;
    if (!gtk_toggle_button_get_active(btn)) return;
    if (W_de_checks[8])
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_de_checks[8]), FALSE);
}

static GtkWidget *build_desktop(void) {
    static const struct {const char *id; const char *en; const char *es;} DE[] = {
        {"KDE Plasma","KDE Plasma — Full-featured, modern, highly customizable",
                      "KDE Plasma — Completo, moderno, muy personalizable"},
        {"GNOME",     "GNOME — Clean, Wayland-first, minimalist",
                      "GNOME — Limpio, enfocado en Wayland, minimalista"},
        {"Cinnamon",  "Cinnamon — Classic feel, Windows-like layout",
                      "Cinnamon — Clásico, disposición similar a Windows"},
        {"XFCE",      "XFCE — Lightweight, stable, traditional",
                      "XFCE — Ligero, estable, tradicional"},
        {"MATE",      "MATE — GNOME 2 fork, very stable",
                      "MATE — Fork de GNOME 2, muy estable"},
        {"LXQt",      "LXQt — Minimal Qt desktop, very light",
                      "LXQt — Escritorio Qt mínimo, muy ligero"},
        {"Hyprland",  "Hyprland — Tiling Wayland compositor + animations",
                      "Hyprland — Compositor Wayland tiling + animaciones"},
        {"Sway",      "Sway — Tiling Wayland compositor, i3-compatible",
                      "Sway — Compositor Wayland tiling, compatible con i3"},
        {"None",      "None — CLI only, no desktop environment",
                      "Ninguno — Solo terminal, sin escritorio"},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Desktop Environment","Entorno de escritorio")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Select one or more desktop environments to install.\n"
                   "The display manager of the first selected DE will be enabled.",
                   "Selecciona uno o más entornos de escritorio.\n"
                   "El gestor de pantalla del primer DE seleccionado se habilitará.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    for (int i=0;DE[i].id;i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        W_de_checks[i] = gtk_check_button_new_with_label(DE[i].id);
        int on = (strstr(st.desktop_list,DE[i].id)!=NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_de_checks[i]),on);
        if (i == 8) 
            g_signal_connect(W_de_checks[i],"toggled",G_CALLBACK(cb_de_none_toggled),NULL);
        else
            g_signal_connect(W_de_checks[i],"toggled",G_CALLBACK(cb_de_other_toggled),NULL);
        gtk_box_pack_start(GTK_BOX(row),W_de_checks[i],FALSE,FALSE,0);
        GtkWidget *d = gtk_label_new(strcmp(st.lang,"en")==0?DE[i].en:DE[i].es);
        gtk_label_set_xalign(GTK_LABEL(d),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(d),TRUE);
        add_class(d,"hint");
        gtk_widget_set_margin_start(d,26);
        gtk_box_pack_start(GTK_BOX(row),d,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(card),row,FALSE,FALSE,4);
    }
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_gpu(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    char detected[32]={0}; detect_gpu(detected,sizeof(detected));

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("GPU Drivers","Drivers de GPU")),FALSE,FALSE,0);
    char sub_text[128];
    snprintf(sub_text,sizeof(sub_text),
        L("Detected GPU: %s","GPU detectada: %s"), detected[0]?detected:"Unknown");
    gtk_box_pack_start(GTK_BOX(vbox),make_sub(sub_text),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();

    static const char *GPU_OPTS[] = {
        "None","Intel","AMD","NVIDIA","Intel+NVIDIA","Intel+AMD",NULL
    };
    W_gpu_combo = gtk_combo_box_text_new();
    int sel_gpu=0;
    for (int i=0;GPU_OPTS[i];i++) {
        gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_gpu_combo),GPU_OPTS[i],GPU_OPTS[i]);
        if (!strcmp(GPU_OPTS[i],detected)||!strcmp(GPU_OPTS[i],st.gpu)) sel_gpu=i;
    }
    gtk_combo_box_set_active(GTK_COMBO_BOX(W_gpu_combo),sel_gpu);
    if (!st.gpu[0] || !strcmp(st.gpu,"None")) {
        for (int i=0;GPU_OPTS[i];i++)
            if (!strcmp(GPU_OPTS[i],detected)){gtk_combo_box_set_active(GTK_COMBO_BOX(W_gpu_combo),i);break;}
    }
    gtk_widget_set_size_request(W_gpu_combo,260,-1);
    gtk_box_pack_start(GTK_BOX(card),
        make_field_row(L("GPU:","GPU:"),W_gpu_combo),FALSE,FALSE,0);

    W_optimus_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    GtkWidget *om_lbl = gtk_label_new(
        L("Optimus mode (Intel+NVIDIA hybrid laptops):","Modo Optimus (portátiles Intel+NVIDIA):"));
    gtk_label_set_xalign(GTK_LABEL(om_lbl),0.0f);
    gtk_box_pack_start(GTK_BOX(W_optimus_box),om_lbl,FALSE,FALSE,0);

    W_optimus_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_optimus_combo),"hybrid",
        L("Hybrid — use both GPUs, best battery+performance","Híbrido — usa ambas GPUs, mejor batería+rendimiento"));
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_optimus_combo),"integrated",
        L("Integrated only — NVIDIA off, max battery","Solo integrada — NVIDIA apagada, máxima batería"));
    gtk_combo_box_text_append(GTK_COMBO_BOX_TEXT(W_optimus_combo),"nvidia",
        L("NVIDIA only — max performance, drains battery","Solo NVIDIA — máximo rendimiento, consume batería"));
    gtk_combo_box_set_active_id(GTK_COMBO_BOX(W_optimus_combo),
        st.optimus_mode[0]?st.optimus_mode:"hybrid");
    gtk_widget_set_size_request(W_optimus_combo,360,-1);
    gtk_box_pack_start(GTK_BOX(W_optimus_box),W_optimus_combo,FALSE,FALSE,0);
    gtk_widget_set_margin_start(W_optimus_box,4);
    gtk_widget_set_margin_top(W_optimus_box,8);
    gtk_box_pack_start(GTK_BOX(card),W_optimus_box,FALSE,FALSE,0);

    const char *cur_gpu = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_gpu_combo));
    gboolean show_opt = cur_gpu && !strcmp(cur_gpu,"Intel+NVIDIA");
    gtk_widget_set_no_show_all(W_optimus_box,TRUE);
    if (show_opt) gtk_widget_show_all(W_optimus_box); else gtk_widget_hide(W_optimus_box);

    g_signal_connect(W_gpu_combo,"changed",G_CALLBACK(cb_gpu_combo_changed),(gpointer)W_optimus_box);

    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_profile(void) {
    static const struct {const char *id; const char *en; const char *es;} PR[] = {
        {"none",      "None — base system only, no extras",
                      "Ninguno — solo sistema base, sin extras"},
        {"gaming",    "Gaming — Steam + Lutris + GameMode + MangoHud + Wine + multilib",
                      "Gaming — Steam + Lutris + GameMode + MangoHud + Wine + multilib"},
        {"developer", "Developer — git + Docker + Python + Node + Go + Rust + JDK + gdb",
                      "Desarrollador — git + Docker + Python + Node + Go + Rust + JDK + gdb"},
        {"minimal",   "Minimal — lightest possible (no profile extras)",
                      "Minimal — instalación más ligera posible (sin extras de perfil)"},
        {"privacy",   "Privacy — Tor + ufw + fail2ban + firejail + KeePassXC + BleachBit",
                      "Privacidad — Tor + ufw + fail2ban + firejail + KeePassXC + BleachBit"},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Installation Profile","Perfil de instalación")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Choose a pre-configured set of packages to install alongside the base system.",
                   "Elige un conjunto preconfigurado de paquetes a instalar junto al sistema base.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp=NULL;
    for (int i=0;PR[i].id;i++) {
        GtkWidget *row = gtk_box_new(GTK_ORIENTATION_VERTICAL,2);
        W_prof_radios[i] = gtk_radio_button_new_with_label(grp,PR[i].id);
        grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_prof_radios[i]));
        if (!strcmp(PR[i].id,st.profile))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_prof_radios[i]),TRUE);
        gtk_box_pack_start(GTK_BOX(row),W_prof_radios[i],FALSE,FALSE,0);
        GtkWidget *d=gtk_label_new(strcmp(st.lang,"en")==0?PR[i].en:PR[i].es);
        gtk_label_set_xalign(GTK_LABEL(d),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(d),TRUE);
        add_class(d,"hint");
        gtk_widget_set_margin_start(d,26);
        gtk_box_pack_start(GTK_BOX(row),d,FALSE,FALSE,0);
        gtk_box_pack_start(GTK_BOX(card),row,FALSE,FALSE,4);
    }
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_dotfiles(void) {
    static const struct {const char *id; const char *en; const char *es;} DOT[] = {
        {"none",      "None — skip dotfiles","Ninguno — omitir dotfiles"},
        {"caelestia", "Caelestia — install caelestia-dots (Hyprland rice)",
                      "Caelestia — instalar caelestia-dots (rice para Hyprland)"},
        {"custom",    "Custom — provide your own git repository URL",
                      "Personalizado — URL de tu propio repositorio git"},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Dotfiles","Dotfiles")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Optionally install dotfiles after the base system is set up.\n"
                   "(Requires Hyprland + internet. Non-fatal if it fails.)",
                   "Opcionalmente instala dotfiles después de configurar el sistema.\n"
                   "(Requiere Hyprland + internet. No es fatal si falla.)")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp=NULL;
    for (int i=0;DOT[i].id;i++) {
        W_dot_radios[i] = gtk_radio_button_new_with_label(grp,
            strcmp(st.lang,"en")==0?DOT[i].en:DOT[i].es);
        grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_dot_radios[i]));
        if (!strcmp(DOT[i].id,st.dotfiles))
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_dot_radios[i]),TRUE);
        g_object_set_data(G_OBJECT(W_dot_radios[i]),"dot_id",(gpointer)DOT[i].id);
        gtk_box_pack_start(GTK_BOX(card),W_dot_radios[i],FALSE,FALSE,4);
    }

    W_dot_url_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,4);
    GtkWidget *u_lbl = gtk_label_new(L("Git repository URL:","URL del repositorio git:"));
    gtk_label_set_xalign(GTK_LABEL(u_lbl),0.0f);
    W_dot_url = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(W_dot_url),"https://github.com/user/dotfiles.git");
    if (st.dotfiles_url[0]) gtk_entry_set_text(GTK_ENTRY(W_dot_url),st.dotfiles_url);
    gtk_widget_set_size_request(W_dot_url,380,-1);
    gtk_box_pack_start(GTK_BOX(W_dot_url_box),u_lbl,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(W_dot_url_box),W_dot_url,FALSE,FALSE,0);
    gtk_widget_set_margin_start(W_dot_url_box,24);
    gtk_widget_set_no_show_all(W_dot_url_box,TRUE);
    if (!strcmp(st.dotfiles,"custom")) gtk_widget_show_all(W_dot_url_box);
    else gtk_widget_hide(W_dot_url_box);
    gtk_box_pack_start(GTK_BOX(card),W_dot_url_box,FALSE,FALSE,0);

    for (int i=0;DOT[i].id;i++) {
        g_signal_connect(W_dot_radios[i],"toggled",
            G_CALLBACK(cb_dot_radio_toggled),(gpointer)W_dot_url_box);
    }

    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_yay(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("AUR Helper — yay","AUR Helper — yay")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("yay gives you access to the AUR — 80,000+ community packages\n"
                   "like Spotify, Discord, AnyDesk, and thousands more.",
                   "yay da acceso al AUR — más de 80.000 paquetes de la comunidad\n"
                   "como Spotify, Discord, AnyDesk, y miles más.")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp=NULL;
    W_yay_yes = gtk_radio_button_new_with_label(NULL,
        L("✓  Install yay  (recommended)","✓  Instalar yay  (recomendado)"));
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_yay_yes));
    GtkWidget *yay_no = gtk_radio_button_new_with_label(grp,
        L("✗  Skip  (official packages only)","✗  Omitir  (solo paquetes oficiales)"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_yay_yes),  st.yay);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(yay_no),    !st.yay);
    gtk_box_pack_start(GTK_BOX(card),W_yay_yes,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(card),yay_no,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_flatpak(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Flatpak + Flathub","Flatpak + Flathub")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Flatpak is a universal, sandboxed app delivery system.\n"
                   "Flathub has GIMP, VLC, Spotify, VS Code, OBS, Signal, and many more.",
                   "Flatpak es un sistema universal de distribución de apps en sandbox.\n"
                   "Flathub tiene GIMP, VLC, Spotify, VS Code, OBS, Signal, y muchas más.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    GSList *grp=NULL;
    W_flatpak_yes = gtk_radio_button_new_with_label(NULL,
        L("✓  Install Flatpak + Flathub  (recommended)","✓  Instalar Flatpak + Flathub  (recomendado)"));
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_flatpak_yes));
    GtkWidget *fp_no = gtk_radio_button_new_with_label(grp,L("✗  Skip","✗  Omitir"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_flatpak_yes),  st.flatpak);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(fp_no),         !st.flatpak);
    gtk_box_pack_start(GTK_BOX(card),W_flatpak_yes,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(card),fp_no,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_snapper(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("BTRFS Snapshots — Snapper","Snapshots BTRFS — Snapper")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Snapper automatically takes snapshots before/after every system update.\n"
                   "If something breaks, roll back in seconds — like a time machine for your OS.\n"
                   "(Requires btrfs filesystem.)",
                   "Snapper toma automáticamente snapshots antes/después de cada actualización.\n"
                   "Si algo se rompe, vuelve atrás en segundos — como una máquina del tiempo.\n"
                   "(Requiere sistema de archivos btrfs.)")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *card = make_card();
    if (strcmp(st.filesystem,"btrfs")) {
        GtkWidget *info = gtk_label_new(
            L("⚠  Snapper is not available — requires btrfs filesystem.\n"
              "Change filesystem to btrfs to enable snapshots.",
              "⚠  Snapper no está disponible — requiere sistema de archivos btrfs.\n"
              "Cambia el sistema de archivos a btrfs para habilitar snapshots."));
        gtk_label_set_xalign(GTK_LABEL(info),0.0f);
        gtk_label_set_line_wrap(GTK_LABEL(info),TRUE);
        add_class(info,"warn");
        gtk_box_pack_start(GTK_BOX(card),info,FALSE,FALSE,4);
    }
    GSList *grp=NULL;
    W_snapper_yes = gtk_radio_button_new_with_label(NULL,
        L("✓  Enable automatic snapshots  (recommended with btrfs)",
          "✓  Activar snapshots automáticos  (recomendado con btrfs)"));
    grp = gtk_radio_button_get_group(GTK_RADIO_BUTTON(W_snapper_yes));
    GtkWidget *sn_no = gtk_radio_button_new_with_label(grp,L("✗  Skip","✗  Omitir"));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_snapper_yes), st.snapper);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(sn_no),        !st.snapper);
    if (strcmp(st.filesystem,"btrfs")) {
        gtk_widget_set_sensitive(W_snapper_yes,FALSE);
        gtk_widget_set_sensitive(sn_no,FALSE);
    }
    gtk_box_pack_start(GTK_BOX(card),W_snapper_yes,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(card),sn_no,FALSE,FALSE,4);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_extra_pkgs(void) {
    static const struct {const char *id; const char *en; const char *es;} PKGS[] = {
        {"firefox",       "firefox — Web browser",                      "firefox — Navegador web"},
        {"chromium",      "chromium — Chrome-based browser",            "chromium — Navegador basado en Chrome"},
        {"libreoffice",   "libreoffice-fresh — Office suite",           "libreoffice-fresh — Suite ofimática"},
        {"vlc",           "vlc — Media player",                         "vlc — Reproductor multimedia"},
        {"gimp",          "gimp — Image editor",                        "gimp — Editor de imágenes"},
        {"inkscape",      "inkscape — Vector graphics",                 "inkscape — Gráficos vectoriales"},
        {"thunderbird",   "thunderbird — Email client",                 "thunderbird — Cliente de correo"},
        {"telegram",      "telegram-desktop — Messenger",               "telegram-desktop — Mensajería"},
        {"discord",       "discord — Voice/text chat (via Flatpak)",    "discord — Chat voz/texto (vía Flatpak)"},
        {"obs-studio",    "obs-studio — Screen recording",              "obs-studio — Grabación de pantalla"},
        {"kdenlive",      "kdenlive — Video editor",                    "kdenlive — Editor de vídeo"},
        {"krita",         "krita — Digital painting",                   "krita — Pintura digital"},
        {"blender",       "blender — 3D modeling",                      "blender — Modelado 3D"},
        {"audacity",      "audacity — Audio editor",                    "audacity — Editor de audio"},
        {"neovim",        "neovim — Text editor",                       "neovim — Editor de texto"},
        {"code",          "code — Visual Studio Code (OSS)",            "code — Visual Studio Code (OSS)"},
        {"htop",          "htop — Process monitor",                     "htop — Monitor de procesos"},
        {"tmux",          "tmux — Terminal multiplexer",                "tmux — Multiplexor de terminal"},
        {"git",           "git — Version control",                      "git — Control de versiones"},
        {"nmap",          "nmap — Network scanner",                     "nmap — Escáner de red"},
        {"wireshark-qt",  "wireshark — Packet analyser",                "wireshark — Analizador de paquetes"},
        {"cups",          "cups — Printing support",                    "cups — Soporte de impresión"},
        {"sane",          "sane — Scanner support",                     "sane — Soporte de escáner"},
        {"docker",        "docker — Container engine",                  "docker — Motor de contenedores"},
        {"virtualbox",    "virtualbox — Virtual machines",              "virtualbox — Máquinas virtuales"},
        {"wine",          "wine — Windows compatibility layer",         "wine — Capa de compatibilidad Windows"},
        {"steam",         "steam — Steam gaming client",                "steam — Cliente de juegos Steam"},
        {"lutris",        "lutris — Game launcher (Linux+Windows)",     "lutris — Lanzador de juegos (Linux+Windows)"},
        {"syncthing",     "syncthing — File synchronization",           "syncthing — Sincronización de archivos"},
        {"nextcloud",     "nextcloud-client — Cloud storage",           "nextcloud-client — Almacenamiento en nube"},
        {"keepassxc",     "keepassxc — Password manager",               "keepassxc — Gestor de contraseñas"},
        {"fish",          "fish — Friendly shell (set as default)",      "fish — Shell amigable (establecer como predeterminada)"},
        {NULL,NULL,NULL}
    };

    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Extra Packages","Paquetes extra")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Select additional packages to install.\n"
                   "You can always install more packages later with pacman or yay.",
                   "Selecciona paquetes adicionales a instalar.\n"
                   "Siempre puedes instalar más paquetes después con pacman o yay.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *search = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(search),
        L("Filter packages…","Filtrar paquetes…"));
    gtk_box_pack_start(GTK_BOX(vbox),search,FALSE,FALSE,0);

    GtkWidget *card = make_card();

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid),6);
    gtk_grid_set_column_spacing(GTK_GRID(grid),24);

    W_npkgs = 0;
    for (int i=0;PKGS[i].id && W_npkgs<MAX_EXTRA_PKGS;i++) {
        W_pkg_ids[W_npkgs] = PKGS[i].id;
        W_pkg_checks[W_npkgs] = gtk_check_button_new_with_label(
            strcmp(st.lang,"en")==0 ? PKGS[i].en : PKGS[i].es);
        int on = (strstr(st.extra_pkgs, PKGS[i].id) != NULL);
        gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(W_pkg_checks[W_npkgs]),on);

        int col = W_npkgs % 2;
        int row = W_npkgs / 2;
        gtk_grid_attach(GTK_GRID(grid), W_pkg_checks[W_npkgs], col, row, 1, 1);
        W_npkgs++;
    }

    FilterData *fd = g_new0(FilterData,1);
    fd->checks = W_pkg_checks;
    fd->n = W_npkgs;
    g_signal_connect_data(search,"search-changed",
        G_CALLBACK(cb_pkg_filter),(gpointer)fd,(GClosureNotify)g_free,(GConnectFlags)0);

    gtk_container_add(GTK_CONTAINER(card),grid);
    gtk_box_pack_start(GTK_BOX(vbox),card,FALSE,FALSE,0);
    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void refresh_review(void);

static GtkWidget *build_review(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Review Configuration","Revisar configuración")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Please review all settings before proceeding.\n"
                   "Click Back to change anything.",
                   "Revisa todos los ajustes antes de continuar.\n"
                   "Haz clic en Atrás para modificar cualquier valor.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *tv_sw = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tv_sw),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(tv_sw,TRUE);

    W_review_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(W_review_tv),FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(W_review_tv),FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(W_review_tv),GTK_WRAP_WORD_CHAR);
    gtk_container_set_border_width(GTK_CONTAINER(W_review_tv),10);
    gtk_container_add(GTK_CONTAINER(tv_sw),W_review_tv);
    gtk_box_pack_start(GTK_BOX(vbox),tv_sw,TRUE,TRUE,0);

    GtkWidget *warn = gtk_label_new(
        L("⚠  The installation will ERASE the selected disk.\n"
          "All data on that disk will be permanently lost.",
          "⚠  La instalación BORRARÁ el disco seleccionado.\n"
          "Todos los datos de ese disco se perderán de forma permanente."));
    add_class(warn,"warn");
    gtk_label_set_justify(GTK_LABEL(warn),GTK_JUSTIFY_CENTER);
    gtk_label_set_line_wrap(GTK_LABEL(warn),TRUE);
    gtk_box_pack_start(GTK_BOX(vbox),warn,FALSE,FALSE,8);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void refresh_review(void) {
    if (!W_review_tv) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(W_review_tv));
    char text[4096]={0};
    int n=0;
#define RV(fmt,...) n += snprintf(text+n,sizeof(text)-n,fmt"\n",##__VA_ARGS__)
    RV("══════════════════════════════════════════");
    RV("Arch Linux Installation Summary");
    RV("══════════════════════════════════════════");
    RV("");
    RV("Mode         : %s", st.quick?"Quick":"Custom");
    RV("Language     : %s", st.lang);
    RV("Locale       : %s", st.locale);
    RV("Keyboard     : %s", st.keymap);
    RV("Timezone     : %s", st.timezone);
    RV("");
    RV("Disk         : /dev/%s", st.disk);
    RV("Swap         : %s GB", st.swap);
    RV("Dual boot    : %s", st.dualboot?"Yes":"No");
    RV("Filesystem   : %s", st.filesystem);
    RV("Kernel(s)    : %s", st.kernel_list);
    RV("Bootloader   : %s", st.bootloader);
    RV("Mirrors      : %s", st.mirrors?"reflector (auto)":"no");
    RV("");
    RV("Hostname     : %s", st.hostname);
    RV("Username     : %s", st.username);
    RV("");
    RV("Desktop(s)   : %s", st.desktop_list);
    RV("GPU driver   : %s", st.gpu);
    if (!strcmp(st.gpu,"Intel+NVIDIA"))
    RV("Optimus mode : %s", st.optimus_mode);
    RV("Profile      : %s", st.profile);
    RV("Dotfiles     : %s", st.dotfiles);
    if (!strcmp(st.dotfiles,"custom"))
    RV("Dotfiles URL : %s", st.dotfiles_url);
    RV("yay          : %s", st.yay?"yes":"no");
    RV("Flatpak      : %s", st.flatpak?"yes":"no");
    RV("Snapper      : %s", st.snapper?"yes":"no");
    if (st.extra_pkgs[0])
    RV("Extra pkgs   : %s", st.extra_pkgs);
    RV("");
    RV("══════════════════════════════════════════");
#undef RV
    gtk_text_buffer_set_text(buf,text,-1);

    GtkTextIter start; gtk_text_buffer_get_start_iter(buf,&start);
    GtkTextTag *bold = gtk_text_buffer_create_tag(buf,NULL,"weight",PANGO_WEIGHT_BOLD,NULL);
    GtkTextTag *blue = gtk_text_buffer_create_tag(buf,NULL,"foreground","#58a6ff",NULL);
    GtkTextIter end;  gtk_text_buffer_get_end_iter(buf,&end);
    gtk_text_buffer_apply_tag(buf,bold,&start,&end);
    (void)blue;
}

static void gtk_run_preflight(void);

static GtkWidget *build_preflight(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *sv   = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sv),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Pre-install Checks","Verificación pre-instalación")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),
        make_sub(L("Verifying that the system is ready to install.",
                   "Verificando que el sistema está listo para instalar.")),
        FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *tv_sw = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tv_sw),GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(tv_sw,TRUE);

    W_pre_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(W_pre_tv),FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(W_pre_tv),GTK_WRAP_WORD_CHAR);
    gtk_container_set_border_width(GTK_CONTAINER(W_pre_tv),10);
    gtk_container_add(GTK_CONTAINER(tv_sw),W_pre_tv);
    gtk_box_pack_start(GTK_BOX(vbox),tv_sw,TRUE,TRUE,0);

    W_pre_ok = gtk_label_new("");
    add_class(W_pre_ok,"warn");
    gtk_label_set_xalign(GTK_LABEL(W_pre_ok),0.5f);
    gtk_box_pack_start(GTK_BOX(vbox),W_pre_ok,FALSE,FALSE,4);

    gtk_container_add(GTK_CONTAINER(sv),vbox);
    gtk_box_pack_start(GTK_BOX(page),sv,TRUE,TRUE,0);
    return page;
}

static void gtk_run_preflight(void) {
    if (!W_pre_tv) return;
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(W_pre_tv));
    char text[4096]={0};
    int n=0, ok=1;
#define PF(sym,fmt,...) n+=snprintf(text+n,sizeof(text)-n,"%s  "fmt"\n",sym,##__VA_ARGS__)

    PF("","Running pre-install checks…");
    n+=snprintf(text+n,sizeof(text)-n,"\n");

    int net_ok = check_connectivity();
    PF(net_ok?"✓":"✗","Internet connection: %s",net_ok?"OK":"FAIL — no internet!");
    if (!net_ok) ok=0;

    int disk_ok = (st.disk[0]!='\0');
    PF(disk_ok?"✓":"✗","Disk selected: %s",disk_ok?st.disk:"NONE");
    if (!disk_ok) ok=0;

    if (disk_ok) {
        char path[128]; snprintf(path,sizeof(path),"/dev/%s",st.disk);
        int acc_ok = (access(path,F_OK)==0);
        PF(acc_ok?"✓":"✗","Disk accessible (%s): %s",path,acc_ok?"OK":"FAIL");
        if (!acc_ok) ok=0;
    }

    int id_ok = (st.hostname[0]&&st.username[0]);
    PF(id_ok?"✓":"✗","Hostname/username: %s",id_ok?"OK":"MISSING");
    if (!id_ok) ok=0;

    int pw_ok = (st.root_pass[0]&&st.user_pass[0]);
    PF(pw_ok?"✓":"✗","Passwords set: %s",pw_ok?"OK":"MISSING");
    if (!pw_ok) ok=0;

    long ram_mb = 0;
    FILE *fp=fopen("/proc/meminfo","r");
    if (fp) {
        char line[128];
        while (fgets(line,sizeof(line),fp)) {
            if (strncmp(line,"MemTotal:",9)==0){sscanf(line+9,"%ld",&ram_mb);break;}
        }
        fclose(fp);
    }
    ram_mb /= 1024;
    int ram_ok = (ram_mb >= 512);
    PF(ram_ok?"✓":"⚠","RAM: %ld MB %s",ram_mb,ram_ok?"OK":"(low, may be slow)");

    int uefi = is_uefi();
    PF("ℹ","Boot mode: %s",uefi?"UEFI":"BIOS/MBR");

    int pac_ok = (access("/usr/bin/pacman",X_OK)==0);
    PF(pac_ok?"✓":"✗","pacman available: %s",pac_ok?"OK":"FAIL — not Arch live ISO?");
    if (!pac_ok) ok=0;

    n+=snprintf(text+n,sizeof(text)-n,"\n");
    if (ok)
        n+=snprintf(text+n,sizeof(text)-n,"✓  All checks passed — ready to install!\n");
    else
        n+=snprintf(text+n,sizeof(text)-n,
            "✗  Some checks FAILED.  Fix the issues before proceeding.\n");

#undef PF
    gtk_text_buffer_set_text(buf,text,-1);
    if (W_pre_ok) {
        gtk_label_set_text(GTK_LABEL(W_pre_ok),
            ok ? L("✓  System ready to install.","✓  Sistema listo para instalar.")
               : L("✗  Fix the issues above and click Back to correct them.",
                   "✗  Corrige los problemas anteriores y haz clic en Atrás para corregirlos."));
    }
    W_pre_result = ok;
}

static GtkWidget *build_install(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,16);

    gtk_box_pack_start(GTK_BOX(vbox),
        make_title(L("Installing Arch Linux…","Instalando Arch Linux…")),FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    W_inst_stage = gtk_label_new(
        L("Preparing installation…","Preparando instalación…"));
    gtk_label_set_xalign(GTK_LABEL(W_inst_stage),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(W_inst_stage),TRUE);
    add_class(W_inst_stage,"card-title");
    gtk_box_pack_start(GTK_BOX(vbox),W_inst_stage,FALSE,FALSE,0);

    W_inst_prog = gtk_progress_bar_new();
    gtk_progress_bar_set_show_text(GTK_PROGRESS_BAR(W_inst_prog),TRUE);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(W_inst_prog),0.0);
    gtk_widget_set_size_request(W_inst_prog,-1,18);
    gtk_box_pack_start(GTK_BOX(vbox),W_inst_prog,FALSE,FALSE,0);

    GtkWidget *log_frame = gtk_frame_new(L("Installation Log","Registro de instalación"));
    GtkWidget *log_sw = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(log_sw),
        GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(log_sw,TRUE);
    W_inst_tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(W_inst_tv),FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(W_inst_tv),FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(W_inst_tv),GTK_WRAP_CHAR);
    gtk_container_set_border_width(GTK_CONTAINER(W_inst_tv),8);
    gtk_container_add(GTK_CONTAINER(log_sw),W_inst_tv);
    gtk_container_add(GTK_CONTAINER(log_frame),log_sw);
    gtk_box_pack_start(GTK_BOX(vbox),log_frame,TRUE,TRUE,0);

    gtk_box_pack_start(GTK_BOX(page),vbox,TRUE,TRUE,0);
    return page;
}

static GtkWidget *build_finish(void) {
    GtkWidget *page = make_page_wrap();
    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL,20);
    gtk_widget_set_valign(vbox,GTK_ALIGN_CENTER);
    gtk_widget_set_halign(vbox,GTK_ALIGN_CENTER);
    gtk_box_pack_start(GTK_BOX(page),vbox,TRUE,TRUE,0);

    GtkWidget *icon = gtk_label_new("✓");
    gtk_widget_set_name(icon,"done-icon");
    {
        GtkCssProvider *p=gtk_css_provider_new();
        gtk_css_provider_load_from_data(p,"label#done-icon{font-size:64px;color:#3fb950;}",-1,NULL);
        gtk_style_context_add_provider(gtk_widget_get_style_context(icon),
            GTK_STYLE_PROVIDER(p),GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(p);
    }
    gtk_box_pack_start(GTK_BOX(vbox),icon,FALSE,FALSE,0);

    GtkWidget *title = gtk_label_new(
        L("Arch Linux installed successfully!","¡Arch Linux instalado correctamente!"));
    add_class(title,"welcome-title");
    gtk_label_set_justify(GTK_LABEL(title),GTK_JUSTIFY_CENTER);
    gtk_box_pack_start(GTK_BOX(vbox),title,FALSE,FALSE,0);

    char msg[256];
    snprintf(msg,sizeof(msg),
        L("Installed for: %s@%s","Instalado para: %s@%s"),
        st.username,st.hostname);
    GtkWidget *sub = gtk_label_new(msg);
    gtk_label_set_justify(GTK_LABEL(sub),GTK_JUSTIFY_CENTER);
    add_class(sub,"page-sub");
    gtk_box_pack_start(GTK_BOX(vbox),sub,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(vbox),make_divider(),FALSE,FALSE,0);

    GtkWidget *steps_lbl = gtk_label_new(
        L("Next steps after reboot:\n"
          "• Log in with your username and password\n"
          "• Run  pacman -Syu  to update packages\n"
          "• Check the Arch Wiki for tips",
          "Pasos tras el reinicio:\n"
          "• Inicia sesión con tu usuario y contraseña\n"
          "• Ejecuta  pacman -Syu  para actualizar paquetes\n"
          "• Consulta la Arch Wiki para consejos"));
    gtk_label_set_xalign(GTK_LABEL(steps_lbl),0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(steps_lbl),TRUE);
    add_class(steps_lbl,"hint");
    gtk_box_pack_start(GTK_BOX(vbox),steps_lbl,FALSE,FALSE,0);

    GtkWidget *reboot_btn = gtk_button_new_with_label(
        L("Reboot Now","Reiniciar ahora"));
    add_class(reboot_btn,"suggested-action");
    gtk_widget_set_size_request(reboot_btn,200,48);
    gtk_widget_set_halign(reboot_btn,GTK_ALIGN_CENTER);
    g_signal_connect(reboot_btn,"clicked",G_CALLBACK(cb_reboot),NULL);
    gtk_box_pack_start(GTK_BOX(vbox),reboot_btn,FALSE,FALSE,0);

    return page;
}


static int val_welcome(void)  { return 1; }

static int val_language(void) {
    if (W_lang_es && gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_lang_es)))
        strncpy(st.lang,"es",sizeof(st.lang)-1);
    else
        strncpy(st.lang,"en",sizeof(st.lang)-1);
    return 1;
}

static int val_network(void) { return 1; } 

static int val_mode(void) {
    int quick = (W_mode_quick_radio &&
                 gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_mode_quick_radio)));
    st.quick = quick;

    if (quick) {
        if (!st.filesystem[0] || !strcmp(st.filesystem,"ext4"))
            strncpy(st.filesystem,"btrfs",sizeof(st.filesystem)-1);
        if (!strcmp(st.desktop_list,"None")||!st.desktop_list[0])
            strncpy(st.desktop_list,"KDE Plasma",sizeof(st.desktop_list)-1);
        strncpy(st.desktop,"KDE Plasma",sizeof(st.desktop)-1);
        if (!strcmp(st.kernel,"linux"))
            strncpy(st.kernel_list,"linux",sizeof(st.kernel_list)-1);
        if (!strcmp(st.bootloader,"grub") || !st.bootloader[0])
            strncpy(st.bootloader,"grub",sizeof(st.bootloader)-1);
        st.yay=1; st.snapper=1; st.flatpak=1; st.mirrors=1;
    }

    if (quick) g_pages = QUICK_PAGES;
    else rebuild_custom_pages();
    update_sidebar();
    return 1;
}

static int val_locale(void) {
    if (W_locale_combo) {
        const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_locale_combo));
        if (id) {
            strncpy(st.locale,id,sizeof(st.locale)-1);
            const char *km = kv_get(LOCALE_TO_KEYMAP,id);
            if (km) strncpy(st.keymap,km,sizeof(st.keymap)-1);
        }
    }
    return 1;
}

static int val_keymap(void) {
    if (W_keymap_combo) {
        const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_keymap_combo));
        if (id) strncpy(st.keymap,id,sizeof(st.keymap)-1);
        char cmd[128]; snprintf(cmd,sizeof(cmd),"loadkeys %s 2>/dev/null",st.keymap);
        system(cmd);
    }
    return 1;
}

static int val_disk(void) {
    if (W_disk_lb) {
        GtkListBoxRow *sel = gtk_list_box_get_selected_row(GTK_LIST_BOX(W_disk_lb));
        if (!sel) {
            msgbox(L("No disk","Sin disco"),
                   L("Please select a disk to install Arch Linux.",
                     "Por favor selecciona un disco donde instalar Arch Linux."));
            return 0;
        }
        int idx = gtk_list_box_row_get_index(sel);
        if (idx >= 0 && idx < W_ndisks)
            strncpy(st.disk,W_disks[idx].name,sizeof(st.disk)-1);
    }

    if (W_swap_entry) {
        const char *s = gtk_entry_get_text(GTK_ENTRY(W_swap_entry));
        if (!validate_swap(s)) {
            msgbox(L("Invalid swap","Swap inválida"),
                   L("Swap size must be a number between 1 and 128.",
                     "El tamaño de swap debe ser un número entre 1 y 128."));
            return 0;
        }
        strncpy(st.swap,s,sizeof(st.swap)-1);
    }

    if (W_dualboot_chk) {
        st.dualboot = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_dualboot_chk));
        if (st.dualboot && W_dbsize_entry) {
            const char *s = gtk_entry_get_text(GTK_ENTRY(W_dbsize_entry));
            int v = atoi(s);
            if (v < 10 || v > 2000) {
                msgbox(L("Invalid size","Tamaño inválido"),
                       L("Partition size must be between 10 and 2000 GB.",
                         "El tamaño de la partición debe estar entre 10 y 2000 GB."));
                return 0;
            }
            st.db_size_gb = v;
        }
    }

    if (!st.dualboot && st.disk[0]) {
        for (int i = 0; i < W_ndisks; i++) {
            if (!strcmp(W_disks[i].name, st.disk) && W_disks[i].size_gb < 20) {
                char sz_warn[256];
                snprintf(sz_warn, sizeof(sz_warn),
                    L("⚠  /dev/%s is only %lld GB.  Arch Linux requires at least 20 GB.\n\nPlease choose a larger disk.",
                      "⚠  /dev/%s tiene solo %lld GB.  Arch Linux necesita al menos 20 GB.\n\nPor favor elige un disco mayor."),
                    W_disks[i].name, W_disks[i].size_gb);
                msgbox(L("Disk too small","Disco demasiado pequeño"), sz_warn);
                return 0;
            }
        }
    }

    char warn[256];
    snprintf(warn,sizeof(warn),
        L("⚠  This will ERASE all data on /dev/%s.\n\nAre you sure?",
          "⚠  Esto BORRARÁ todos los datos de /dev/%s.\n\n¿Estás seguro?"),
        st.disk);
    if (!yesno_dlg(L("Confirm disk erase","Confirmar borrado de disco"),warn))
        return 0;

    return 1;
}

static int val_filesystem(void) {
    for (int i=0;i<4;i++) {
        if (W_fs_radios[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_fs_radios[i]))) {
            strncpy(st.filesystem,FS_IDS[i],sizeof(st.filesystem)-1);
            break;
        }
    }
    if (!st.quick) { rebuild_custom_pages(); update_sidebar(); }
    return 1;
}

static int val_kernel(void) {
    char list[512]={0}; int first=1;
    for (int i=0;i<5;i++) {
        if (W_kern_checks[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_kern_checks[i]))) {
            if (!first) strncat(list," ",sizeof(list)-strlen(list)-1);
            strncat(list,KERN_IDS[i],sizeof(list)-strlen(list)-1);
            if (first) { strncpy(st.kernel,KERN_IDS[i],sizeof(st.kernel)-1); first=0; }
        }
    }
    if (!list[0]) {
        msgbox(L("No kernel","Sin kernel"),
               L("Please select at least one kernel.",
                 "Por favor selecciona al menos un kernel."));
        return 0;
    }
    strncpy(st.kernel_list,list,sizeof(st.kernel_list)-1);
    return 1;
}

static int val_bootloader(void) {
    for (int i=0;i<3;i++) {
        if (W_bl_radios[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_bl_radios[i]))) {
            strncpy(st.bootloader,BL_IDS[i],sizeof(st.bootloader)-1);
            break;
        }
    }
    if (!strcmp(st.bootloader,"systemd-boot") && !is_uefi()) {
        msgbox(L("BIOS system","Sistema BIOS"),
               L("systemd-boot requires UEFI.  Please choose GRUB or Limine.",
                 "systemd-boot requiere UEFI.  Por favor elige GRUB o Limine."));
        return 0;
    }
    return 1;
}

static int val_mirrors(void) {
    if (W_mirrors_chk)
        st.mirrors = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_mirrors_chk));
    return 1;
}

static int val_identity(void) {
    const char *hn="", *un="";
    if (W_hostname_e) hn = gtk_entry_get_text(GTK_ENTRY(W_hostname_e));
    if (W_username_e) un = gtk_entry_get_text(GTK_ENTRY(W_username_e));

    if (!validate_name(hn)) {
        if (W_id_err) gtk_label_set_text(GTK_LABEL(W_id_err),
            L("✗  Hostname must start with a letter, contain only letters/numbers/hyphens, max 32 chars.",
              "✗  El hostname debe empezar con letra, contener solo letras/números/guiones, máx 32 chars."));
        if (W_hostname_e) add_class(W_hostname_e,"error");
        return 0;
    }
    if (!validate_name(un)) {
        if (W_id_err) gtk_label_set_text(GTK_LABEL(W_id_err),
            L("✗  Username must start with a letter, contain only lowercase letters/numbers/hyphens, max 32 chars.",
              "✗  El usuario debe empezar con letra, contener solo letras minúsculas/números/guiones, máx 32 chars."));
        if (W_username_e) add_class(W_username_e,"error");
        return 0;
    }
    for (const char *p=un;*p;p++) {
        if (isupper((unsigned char)*p)) {
            if (W_id_err) gtk_label_set_text(GTK_LABEL(W_id_err),
                L("✗  Username must be all lowercase.",
                  "✗  El nombre de usuario debe estar todo en minúsculas."));
            return 0;
        }
    }

    if (W_id_err) gtk_label_set_text(GTK_LABEL(W_id_err),"");
    if (W_hostname_e) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(W_hostname_e);
        gtk_style_context_remove_class(ctx,"error");
    }
    if (W_username_e) {
        GtkStyleContext *ctx = gtk_widget_get_style_context(W_username_e);
        gtk_style_context_remove_class(ctx,"error");
    }
    strncpy(st.hostname,hn,sizeof(st.hostname)-1);
    strncpy(st.username,un,sizeof(st.username)-1);
    return 1;
}

static int val_passwords(void) {
    const char *rp1="",*rp2="",*up1="",*up2="";
    if (W_rpass1) rp1=gtk_entry_get_text(GTK_ENTRY(W_rpass1));
    if (W_rpass2) rp2=gtk_entry_get_text(GTK_ENTRY(W_rpass2));
    if (W_upass1) up1=gtk_entry_get_text(GTK_ENTRY(W_upass1));
    if (W_upass2) up2=gtk_entry_get_text(GTK_ENTRY(W_upass2));

    if (!rp1[0]) {
        if (W_pass_err) gtk_label_set_text(GTK_LABEL(W_pass_err),
            L("✗  Root password cannot be empty.","✗  La contraseña de root no puede estar vacía."));
        return 0;
    }
    if (strcmp(rp1,rp2)) {
        if (W_pass_err) gtk_label_set_text(GTK_LABEL(W_pass_err),
            L("✗  Root passwords do not match.","✗  Las contraseñas de root no coinciden."));
        return 0;
    }
    if (!up1[0]) {
        if (W_pass_err) gtk_label_set_text(GTK_LABEL(W_pass_err),
            L("✗  User password cannot be empty.","✗  La contraseña de usuario no puede estar vacía."));
        return 0;
    }
    if (strcmp(up1,up2)) {
        if (W_pass_err) gtk_label_set_text(GTK_LABEL(W_pass_err),
            L("✗  User passwords do not match.","✗  Las contraseñas de usuario no coinciden."));
        return 0;
    }
    if (W_pass_err) gtk_label_set_text(GTK_LABEL(W_pass_err),"");
    strncpy(st.root_pass,rp1,sizeof(st.root_pass)-1);
    strncpy(st.user_pass,up1,sizeof(st.user_pass)-1);
    return 1;
}

static int val_timezone(void) {
    if (W_tz_region) {
        const char *reg = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_tz_region));
        if (!reg||!strcmp(reg,"UTC")) {
            strncpy(st.timezone,"UTC",sizeof(st.timezone)-1);
        } else if (W_tz_city) {
            const char *city = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_tz_city));
            if (city) {
                char tz[128];
                snprintf(tz,sizeof(tz),"%s/%s",reg,city);
                strncpy(st.timezone,tz,sizeof(st.timezone)-1);
            }
        }
    }
    return 1;
}

static int val_desktop(void) {
    char list[512]={0}; int first=1;
    static const char *DE_NAMES[]={"KDE Plasma","GNOME","Cinnamon","XFCE","MATE","LXQt","Hyprland","Sway","None"};
    for (int i=0;i<9;i++) {
        if (W_de_checks[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_de_checks[i]))) {
            if (!first) strncat(list,"|",sizeof(list)-strlen(list)-1);
            strncat(list,DE_NAMES[i],sizeof(list)-strlen(list)-1);
            if (first) { strncpy(st.desktop,DE_NAMES[i],sizeof(st.desktop)-1); first=0; }
        }
    }
    if (!list[0]) strncpy(list,"None",sizeof(list)-1);
    strncpy(st.desktop_list,list,sizeof(st.desktop_list)-1);
    if (!st.quick) { rebuild_custom_pages(); update_sidebar(); }
    return 1;
}

static int val_gpu(void) {
    if (W_gpu_combo) {
        const char *id = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_gpu_combo));
        if (id) strncpy(st.gpu,id,sizeof(st.gpu)-1);
    }
    if (!strcmp(st.gpu,"Intel+NVIDIA") && W_optimus_combo) {
        const char *om = gtk_combo_box_get_active_id(GTK_COMBO_BOX(W_optimus_combo));
        if (om) strncpy(st.optimus_mode,om,sizeof(st.optimus_mode)-1);
    }
    return 1;
}

static int val_profile(void) {
    for (int i=0;i<5;i++) {
        if (W_prof_radios[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_prof_radios[i]))) {
            strncpy(st.profile,PROF_IDS[i],sizeof(st.profile)-1);
            break;
        }
    }
    return 1;
}

static int val_dotfiles(void) {
    for (int i=0;i<3;i++) {
        if (W_dot_radios[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_dot_radios[i]))) {
            strncpy(st.dotfiles,DOT_IDS[i],sizeof(st.dotfiles)-1);
            break;
        }
    }
    if (!strcmp(st.dotfiles,"custom") && W_dot_url) {
        const char *u = gtk_entry_get_text(GTK_ENTRY(W_dot_url));
        if (!u||!u[0]) {
            msgbox(L("URL required","URL requerida"),
                   L("Please enter the git repository URL for your dotfiles.",
                     "Por favor introduce la URL del repositorio git de tus dotfiles."));
            return 0;
        }
        strncpy(st.dotfiles_url,u,sizeof(st.dotfiles_url)-1);
    }
    return 1;
}

static int val_yay(void) {
    st.yay = (W_yay_yes &&
              gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_yay_yes)));
    return 1;
}

static int val_flatpak(void) {
    st.flatpak = (W_flatpak_yes &&
                  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_flatpak_yes)));
    return 1;
}

static int val_snapper(void) {
    st.snapper = (W_snapper_yes &&
                  gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_snapper_yes)));
    return 1;
}

static int val_extra_pkgs(void) {
    char pkgs[2048]={0};
    st.fish_default = 0; 
    for (int i=0;i<W_npkgs;i++) {
        if (W_pkg_checks[i] &&
            gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(W_pkg_checks[i]))) {
            if (pkgs[0]) strncat(pkgs," ",sizeof(pkgs)-strlen(pkgs)-1);
            strncat(pkgs,W_pkg_ids[i],sizeof(pkgs)-strlen(pkgs)-1);
            if (!strcmp(W_pkg_ids[i],"fish")) st.fish_default=1;
        }
    }
    strncpy(st.extra_pkgs,pkgs,sizeof(st.extra_pkgs)-1);
    return 1;
}

static int val_review(void) {
    return yesno_dlg(
        L("Start installation?","¿Iniciar instalación?"),
        L("All settings confirmed.  The disk will now be partitioned and\n"
          "Arch Linux will be installed.  This cannot be undone.\n\nContinue?",
          "Todos los ajustes confirmados.  El disco será particionado y\n"
          "Arch Linux será instalado.  Esto no se puede deshacer.\n\n¿Continuar?"));
}

static int val_preflight(void) {
    gtk_run_preflight();
    if (!W_pre_result) {
        msgbox(L("Pre-check failed","Verificación fallida"),
               L("Some checks failed.  Please fix the issues before continuing.",
                 "Algunas verificaciones fallaron.  Por favor corrige los problemas antes de continuar."));
        return 0;
    }
    return 1;
}

static int val_install_start(void) {
    if (W_inst_done) {
        if (!W_inst_success) {
            char msg[1100];
            snprintf(msg,sizeof(msg),
                L("Installation failed:\n%s","La instalación falló:\n%s"),
                W_inst_reason);
            msgbox(L("Installation failed","Instalación fallida"),msg);
            return 0; 
        }
        return 1;  
    }

    W_inst_done=0; W_inst_success=0; W_inst_reason[0]='\0';
    gtk_widget_set_sensitive(g_next_btn,FALSE);
    gtk_widget_set_sensitive(g_back_btn,FALSE);
    gtk_widget_set_sensitive(g_cancel_btn,FALSE);

    IB *ib = (IB*)malloc(sizeof(IB));
    if (!ib) return 0;
    memset(ib,0,sizeof(IB));
    pthread_mutex_init(&ib->lock,NULL);
    ib->on_progress = inst_on_progress;
    ib->on_stage    = inst_on_stage;
    ib->on_done     = inst_on_done;
    ib->ud          = NULL;

    IBRunArg *ibarg = malloc(sizeof(IBRunArg));
    if (!ibarg) { free(ib); return 0; }
    ibarg->ib = ib;
    pthread_t tid;
    pthread_create(&tid,NULL,ib_run_thread,(void*)ibarg);
    pthread_detach(tid);
    return 0; 
}

static int val_finish(void) {
    system("reboot");
    return 0;
}

typedef struct {
    const char *id;
    int       (*validate)(void);
    void      (*on_show)(void);   
} PageSpec;

static const PageSpec PAGE_SPECS[] = {
    {"welcome",    val_welcome,      NULL},
    {"language",   val_language,     NULL},
    {"network",    val_network,      NULL},
    {"mode",       val_mode,         NULL},
    {"locale",     val_locale,       NULL},
    {"keymap",     val_keymap,       NULL},
    {"disk",       val_disk,         refresh_disk_list},
    {"filesystem", val_filesystem,   NULL},
    {"kernel",     val_kernel,       NULL},
    {"bootloader", val_bootloader,   NULL},
    {"mirrors",    val_mirrors,      NULL},
    {"identity",   val_identity,     NULL},
    {"passwords",  val_passwords,    NULL},
    {"timezone",   val_timezone,     NULL},
    {"desktop",    val_desktop,      NULL},
    {"gpu",        val_gpu,          NULL},
    {"profile",    val_profile,      NULL},
    {"dotfiles",   val_dotfiles,     NULL},
    {"yay",        val_yay,          NULL},
    {"flatpak",    val_flatpak,      NULL},
    {"snapper",    val_snapper,      NULL},
    {"extra_pkgs", val_extra_pkgs,   NULL},
    {"review",     val_review,       refresh_review},
    {"preflight",  val_preflight,    NULL},
    {"install",    val_install_start,NULL},
    {"finish",     val_finish,       NULL},
    {NULL,NULL,NULL}
};

static const PageSpec *find_spec(const char *id) {
    for (int i=0;PAGE_SPECS[i].id;i++)
        if (!strcmp(PAGE_SPECS[i].id,id)) return &PAGE_SPECS[i];
    return NULL;
}



static void on_next_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    const char *cur_id = g_pages[g_cur];

    if (!strcmp(cur_id,"install") && W_inst_done) {
        if (W_inst_success && g_pages[g_cur+1]) {
            goto_page(g_cur+1);
        }
        return;
    }

    if (!strcmp(cur_id,"finish")) {
        system("reboot");
        return;
    }

    const PageSpec *spec = find_spec(cur_id);

    if (spec && spec->validate) {
        if (!spec->validate()) return; 
    }

    if (g_pages[g_cur+1]) {
        int next_idx = g_cur+1;
        goto_page(next_idx);
        const PageSpec *ns = find_spec(g_pages[next_idx]);
        if (ns && ns->on_show) ns->on_show();
    }
}

static void on_back_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (g_cur > 0) goto_page(g_cur-1);
}

static void on_cancel_clicked(GtkButton *btn, gpointer data) {
    (void)btn; (void)data;
    if (yesno_dlg(
            L("Cancel installation?","¿Cancelar instalación?"),
            L("Are you sure you want to quit the installer?",
              "¿Estás seguro de que quieres salir del instalador?"))) {
        gtk_main_quit();
    }
}

static gboolean on_delete_event(GtkWidget *w, GdkEvent *e, gpointer d) {
    (void)w;(void)e;(void)d;
    on_cancel_clicked(NULL,NULL);
    return TRUE; 
}

static GtkWidget *build_wizard_window(void) {
    GtkWidget *win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(win), TITLE " " VERSION);
    gtk_window_set_default_size(GTK_WINDOW(win), 1100, 720);
    gtk_window_maximize(GTK_WINDOW(win));
    gtk_window_set_position(GTK_WINDOW(win), GTK_WIN_POS_CENTER);
    g_signal_connect(win,"delete-event",G_CALLBACK(on_delete_event),NULL);

    g_main_window = win;

    GtkWidget *outer = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(win), outer);

    GtkWidget *header = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    add_class(header,"header-bar");
    gtk_widget_set_size_request(header,-1,48);
    gtk_container_set_border_width(GTK_CONTAINER(header),0);

    GtkWidget *logo_lbl = gtk_label_new("⟨ PulseOS Installer ⟩");
    add_class(logo_lbl,"step-name");
    gtk_widget_set_margin_start(logo_lbl,16);
    gtk_box_pack_start(GTK_BOX(header),logo_lbl,FALSE,FALSE,0);

    g_step_counter = gtk_label_new("1 / 1");
    add_class(g_step_counter,"step-num");
    gtk_widget_set_margin_end(g_step_counter,16);
    gtk_box_pack_end(GTK_BOX(header),g_step_counter,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(outer),header,FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(outer),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    GtkWidget *body = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,0);
    gtk_widget_set_vexpand(body,TRUE);
    gtk_box_pack_start(GTK_BOX(outer),body,TRUE,TRUE,0);

    GtkWidget *sidebar_sw = gtk_scrolled_window_new(NULL,NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(sidebar_sw),
        GTK_POLICY_NEVER,GTK_POLICY_AUTOMATIC);
    gtk_widget_set_size_request(sidebar_sw,220,-1);
    add_class(sidebar_sw,"sidebar");

    g_sidebar_box = gtk_box_new(GTK_ORIENTATION_VERTICAL,0);
    add_class(g_sidebar_box,"sidebar");
    gtk_container_add(GTK_CONTAINER(sidebar_sw),g_sidebar_box);
    gtk_box_pack_start(GTK_BOX(body),sidebar_sw,FALSE,FALSE,0);

    gtk_box_pack_start(GTK_BOX(body),
        gtk_separator_new(GTK_ORIENTATION_VERTICAL),FALSE,FALSE,0);

    g_stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(g_stack),
        GTK_STACK_TRANSITION_TYPE_SLIDE_LEFT_RIGHT);
    gtk_stack_set_transition_duration(GTK_STACK(g_stack),200);
    gtk_widget_set_hexpand(g_stack,TRUE);
    gtk_widget_set_vexpand(g_stack,TRUE);
    gtk_box_pack_start(GTK_BOX(body),g_stack,TRUE,TRUE,0);

    typedef struct { const char *id; GtkWidget *(*build)(void); } BuildPair;
    static const BuildPair BUILDERS[] = {
        {"welcome",    build_welcome},
        {"language",   build_language},
        {"network",    build_network},
        {"mode",       build_mode},
        {"locale",     build_locale},
        {"keymap",     build_keymap},
        {"disk",       build_disk},
        {"filesystem", build_filesystem},
        {"kernel",     build_kernel},
        {"bootloader", build_bootloader},
        {"mirrors",    build_mirrors},
        {"identity",   build_identity},
        {"passwords",  build_passwords},
        {"timezone",   build_timezone},
        {"desktop",    build_desktop},
        {"gpu",        build_gpu},
        {"profile",    build_profile},
        {"dotfiles",   build_dotfiles},
        {"yay",        build_yay},
        {"flatpak",    build_flatpak},
        {"snapper",    build_snapper},
        {"extra_pkgs", build_extra_pkgs},
        {"review",     build_review},
        {"preflight",  build_preflight},
        {"install",    build_install},
        {"finish",     build_finish},
        {NULL,NULL}
    };

    for (int i=0;BUILDERS[i].id;i++) {
        GtkWidget *page = BUILDERS[i].build();
        gtk_stack_add_named(GTK_STACK(g_stack),page,BUILDERS[i].id);
    }

    gtk_box_pack_start(GTK_BOX(outer),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL),FALSE,FALSE,0);

    GtkWidget *nav = gtk_box_new(GTK_ORIENTATION_HORIZONTAL,12);
    add_class(nav,"nav-bar");
    gtk_box_pack_start(GTK_BOX(outer),nav,FALSE,FALSE,0);

    g_back_btn   = gtk_button_new_with_label(L("← Back","← Atrás"));
    g_cancel_btn = gtk_button_new_with_label(L("✕ Cancel","✕ Cancelar"));
    g_next_btn   = gtk_button_new_with_label(L("Next →","Siguiente →"));
    add_class(g_next_btn,"suggested-action");
    add_class(g_cancel_btn,"destructive-action");

    gtk_widget_set_size_request(g_back_btn,  120,40);
    gtk_widget_set_size_request(g_cancel_btn,120,40);
    gtk_widget_set_size_request(g_next_btn,  150,40);

    g_signal_connect(g_back_btn,  "clicked",G_CALLBACK(on_back_clicked),  NULL);
    g_signal_connect(g_next_btn,  "clicked",G_CALLBACK(on_next_clicked),  NULL);
    g_signal_connect(g_cancel_btn,"clicked",G_CALLBACK(on_cancel_clicked),NULL);

    gtk_box_pack_start(GTK_BOX(nav),g_back_btn,  FALSE,FALSE,0);
    gtk_box_pack_start(GTK_BOX(nav),g_cancel_btn,FALSE,FALSE,0);
    gtk_box_pack_end  (GTK_BOX(nav),g_next_btn,  FALSE,FALSE,0);

    goto_page(0);
    gtk_widget_show_all(win);

    return win;
}


static void ensure_x11_deps(void) {
    if (access("/usr/lib/libgtk-3.so.0",F_OK)==0 ||
        access("/usr/lib/libgtk-3.so",  F_OK)==0) return;

    fprintf(stderr,"[installer] GTK3 not found, attempting to install…\n");
    system("pacman -Sy --noconfirm --needed gtk3 xorg-server xorg-xinit "
           "openbox xterm 2>/dev/null || true");
}

static void write_openbox_env(void) {
    system("mkdir -p /root/.config/openbox");
    FILE *f=fopen("/root/.config/openbox/rc.xml","w");
    if (!f) return;
    fprintf(f,
"<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
"<openbox_config xmlns=\"http://openbox.org/3.4/rc\">\n"
"<theme><font place=\"ActiveWindow\"><size>1</size></font>"
"<name>Bear2</name></theme>\n"
"<desktops><number>1</number></desktops>\n"
"<applications>\n"
"<application class=\"*\"><decor>no</decor>"
"<maximized>yes</maximized></application>\n"
"</applications>\n"
"</openbox_config>\n");
    fclose(f);
}

static void ensure_display(int argc, char **argv) {
    if (getenv("DISPLAY")) return;  
    if (getenv("WAYLAND_DISPLAY")) return; 

    ensure_x11_deps();
    write_openbox_env();

    char xinitrc[1024];
    snprintf(xinitrc,sizeof(xinitrc),
        "#!/bin/sh\n"
        "sleep 0.5\n"
        "HDMI_OUT=$(xrandr 2>/dev/null | grep -E '^HDMI[^ ]* connected' | head -1 | awk '{print $1}')\n"
        "DP_OUT=$(xrandr 2>/dev/null | grep -E '^(DP|DisplayPort)[^ ]* connected' | head -1 | awk '{print $1}')\n"
        "ANY_OUT=$(xrandr 2>/dev/null | grep ' connected' | head -1 | awk '{print $1}')\n"
        "PRIMARY=${HDMI_OUT:-${DP_OUT:-$ANY_OUT}}\n"
        "[ -n \"$PRIMARY\" ] && xrandr --output \"$PRIMARY\" --auto --primary 2>/dev/null || true\n"
        "openbox &\n"
        "exec %s\n", argv[0]);
    FILE *xi=fopen("/tmp/_arch_easy_xinitrc","w");
    if (xi) { fputs(xinitrc,xi); fclose(xi); chmod("/tmp/_arch_easy_xinitrc",0755); }

    setenv("DISPLAY",":0",1);
    char cmd[1024];
    snprintf(cmd,sizeof(cmd),
        "xinit /tmp/_arch_easy_xinitrc -- :0 -nolisten tcp &>/tmp/xinit.log &");
    system(cmd);
    sleep(2);

    execvp(argv[0], argv);
    perror("[installer] execvp failed");
    fprintf(stderr, "[installer] Could not re-exec after starting X11. "
                    "Trying to continue anyway...\n");
}

int main(int argc, char **argv) {
    ensure_display(argc, argv);

    gtk_init(&argc, &argv);

    setup_css();

    st.laptop = is_laptop();
    if (st.laptop) strncpy(st.optimus_mode,"hybrid",sizeof(st.optimus_mode)-1);

    {
        char detected[32]={0};
        detect_gpu(detected,sizeof(detected));
        if (detected[0] && strcmp(detected,"None"))
            strncpy(st.gpu,detected,sizeof(st.gpu)-1);
    }

    {
        int suggested = suggest_swap_gb();
        if (suggested > 0) {
            char sw[8];
            snprintf(sw,sizeof(sw),"%d",suggested);
            strncpy(st.swap,sw,sizeof(st.swap)-1);
        }
    }

    g_wizard_window = build_wizard_window();
    if (!g_wizard_window) {
        fprintf(stderr,"[installer] Failed to create wizard window.\n");
        return 1;
    }

    gtk_main();

    return 0;
}
