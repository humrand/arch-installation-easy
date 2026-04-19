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

#define VERSION   "V3.0.0"
#define LOG_FILE  "/tmp/arch_install.log"
#define TITLE     "Arch Linux Installer"

#define MAX_CMD    8192
#define MAX_OUT    4096
#define MAX_ITEMS  96
#define MAX_LINES  2000
#define KEEP_LINES 1000

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
        "xorg-server xorg-apps xorg-xinit xorg-xrandr xf86-input-libinput",
        "plasma-meta konsole dolphin ark kate plasma-nm firefox sddm"
    }, 2},
    {"GNOME", {
        "gnome gdm firefox"
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
        "polkit-gnome qt5-wayland qt6-wayland sddm firefox"
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
static int g_fullscreen = 1;

static int g_home_requested = 0;

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

static void password_strength_label(const char *p, char *out, size_t sz,
                                    const char *lang) {
    int s = password_strength(p);
    const char *bar[]   = {"", "██░░░", "████░", "█████"};
    const char *elen[]  = {"(empty)", "WEAK", "MEDIUM", "STRONG"};
    const char *eses[]  = {"(vacía)", "DÉBIL", "MEDIA",  "FUERTE"};
    const char **lbl = strcmp(lang,"en")==0 ? elen : eses;
    snprintf(out, sz, "%s %s", s>0?bar[s]:"", lbl[s]);
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
    if (i < sz-1) out[i++] = '\'';
    for (; *s && i < sz-3; ++s) {
        if (*s == '\'') {
            out[i++] = '\''; out[i++] = '\\';
            out[i++] = '\''; out[i++] = '\'';
        } else {
            out[i++] = *s;
        }
    }
    if (i < sz-1) out[i++] = '\'';
    out[i] = '\0';
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

static void set_dark_theme_env(void) {
    setenv("GTK_THEME", "Adwaita:dark", 1);
    setenv("YAD_DISABLE_APPLICATION_INDICATOR", "1", 1);
}

static int yad_exec(char **argv, char *out, size_t outsz) {
    char **exec_argv = argv;
    char **fs_argv   = NULL;

    if (g_fullscreen) {
        int n = 0;
        while (argv[n]) n++;
        fs_argv = calloc((size_t)(n + 3), sizeof(char*));
        int j = 0;
        for (int i = 0; i < n; i++) {
            if (strncmp(argv[i], "--width=", 8) == 0) continue;
            if (strncmp(argv[i], "--height=", 9) == 0) continue;
            fs_argv[j++] = argv[i];
        }
        fs_argv[j++] = "--maximize";
        fs_argv[j]   = NULL;
        exec_argv    = fs_argv;
    }

    int pfd[2];
    if (pipe(pfd) != 0) {
        if (out && outsz) out[0] = '\0';
        if (fs_argv) free(fs_argv);
        return -1;
    }

    pid_t pid = fork();
    if (pid == 0) {
        set_dark_theme_env();
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) dup2(dn, STDERR_FILENO);
        execvp("yad", exec_argv);
        _exit(127);
    }
    close(pfd[1]);

    if (out && outsz > 0) {
        size_t total = 0;
        char buf[512];
        ssize_t n;
        while ((n = read(pfd[0], buf, sizeof(buf))) > 0) {
            if (total + (size_t)n < outsz) {
                memcpy(out + total, buf, (size_t)n);
                total += (size_t)n;
            }
        }
        out[total] = '\0';
        size_t len = strlen(out);
        while (len > 0 && (out[len-1] == '|' || out[len-1] == '\n' || out[len-1] == '\r'))
            out[--len] = '\0';
        trim_nl(out);
    }
    close(pfd[0]);

    int status;
    waitpid(pid, &status, 0);
    if (fs_argv) free(fs_argv);
    if (WIFEXITED(status) && WEXITSTATUS(status) == 77) {
        g_home_requested = 1;
        return 1;
    }
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

#define YAD_W   "--width=580", "--center"
#define YAD_WS  "--width=420", "--center"
#define YAD_WL  "--width=700", "--center"

static void msgbox(const char *title, const char *text) {
    char clean[4096]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--info","--title",(char*)title,"--text",clean,
                 "--button=OK:0", YAD_W, "--wrap", NULL};
    yad_exec(a, NULL, 0);
}

static int yesno_dlg(const char *title, const char *text) {
    char clean[4096]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--question","--title",(char*)title,"--text",clean,
                 "--button=Yes:0","--button=No:1", YAD_W, "--wrap", NULL};
    return yad_exec(a, NULL, 0) == 0;
}

static int inputbox_dlg(const char *title, const char *text,
                         const char *init, char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--entry","--title",(char*)title,"--text",clean,
                 "--entry-text",(char*)(init?init:""),
                 "--button=OK:0","--button=Cancel:1","--button= Home:77",
                 YAD_W, NULL};
    return yad_exec(a, out, outsz) == 0;
}

static int passwordbox_dlg(const char *title, const char *text,
                            char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    int show = 0;
    while (1) {
        char *hide_arg = show ? NULL : (char*)"--hide-text";
        char *eye_btn  = show
            ? (char*)"--button= Ocultar:3"
            : (char*)"--button= Mostrar:3";
        char *argv[32]; int ai = 0;
        argv[ai++] = "yad";
        argv[ai++] = "--entry";
        if (hide_arg) argv[ai++] = hide_arg;
        argv[ai++] = "--title";    argv[ai++] = (char*)title;
        argv[ai++] = "--text";     argv[ai++] = clean;
        if (out && out[0]) { argv[ai++] = "--entry-text"; argv[ai++] = out; }
        argv[ai++] = "--button=OK:0";
        argv[ai++] = "--button=Cancel:1";
        argv[ai++] = eye_btn;
        argv[ai++] = "--button= Home:77";
        argv[ai++] = (char*)YAD_WS;
        argv[ai] = NULL;
        char tmp[outsz > 0 ? outsz : 256];
        tmp[0] = '\0';
        int rc = yad_exec(argv, tmp, sizeof(tmp));
        if (rc == 3) {
            if (tmp[0] && out) strncpy(out, tmp, outsz-1);
            show = !show;
            continue;
        }
        if (rc == 0 && out) { strncpy(out, tmp, outsz-1); return 1; }
        return 0;
    }
}

static int menu_dlg(const char *title, const char *text,
                     MenuItem *items, int n, char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    int argc = 0;
    char **a = calloc((size_t)(n * 2 + 16), sizeof(char*));
    a[argc++] = "yad";
    a[argc++] = "--list";
    a[argc++] = "--title";    a[argc++] = (char*)title;
    a[argc++] = "--text";     a[argc++] = clean;
    a[argc++] = "--column=Option";
    a[argc++] = "--column=Description";
    a[argc++] = "--print-column=1";
    a[argc++] = YAD_WL;
    a[argc++] = "--button=OK:0";
    a[argc++] = "--button=Cancel:1";
    a[argc++] = "--button= Home:77";
    for (int i = 0; i < n; i++) {
        a[argc++] = items[i].tag;
        a[argc++] = items[i].desc;
    }
    a[argc] = NULL;

    int rc = yad_exec(a, out, outsz);
    free(a);
    return rc == 0;
}

static int radiolist_dlg(const char *title, const char *text,
                           MenuItem *items, int n, const char *def,
                           char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    int argc = 0;
    char **a = calloc((size_t)(n * 3 + 20), sizeof(char*));
    a[argc++] = "yad";
    a[argc++] = "--list";
    a[argc++] = "--radiolist";
    a[argc++] = "--title";    a[argc++] = (char*)title;
    a[argc++] = "--text";     a[argc++] = clean;
    a[argc++] = "--column= ";
    a[argc++] = "--column=Option";
    a[argc++] = "--column=Description";
    a[argc++] = "--print-column=2";
    a[argc++] = YAD_WL;
    a[argc++] = "--button=OK:0";
    a[argc++] = "--button=Cancel:1";
    a[argc++] = "--button= Home:77";
    for (int i = 0; i < n; i++) {
        a[argc++] = (def && strcmp(items[i].tag, def) == 0) ? "TRUE" : "FALSE";
        a[argc++] = items[i].tag;
        a[argc++] = items[i].desc;
    }
    a[argc] = NULL;

    int rc = yad_exec(a, out, outsz);
    free(a);
    return rc == 0;
}

static int checklist_dlg(const char *title, const char *text,
                          MenuItem *items, int n,
                          const char **defaults, int ndef,
                          char out[][256], int maxout) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));

    int argc = 0;
    char **a = calloc((size_t)(n * 3 + 20), sizeof(char*));
    a[argc++] = "yad";
    a[argc++] = "--list";
    a[argc++] = "--checklist";
    a[argc++] = "--title";    a[argc++] = (char*)title;
    a[argc++] = "--text";     a[argc++] = clean;
    a[argc++] = "--column= ";
    a[argc++] = "--column=Option";
    a[argc++] = "--column=Description";
    a[argc++] = "--print-column=2";
    a[argc++] = YAD_WL;
    a[argc++] = "--button=OK:0";
    a[argc++] = "--button=Cancel:1";
    a[argc++] = "--button= Home:77";
    for (int i = 0; i < n; i++) {
        int on = 0;
        for (int j = 0; j < ndef; j++)
            if (defaults[j] && strcmp(items[i].tag, defaults[j]) == 0) { on = 1; break; }
        a[argc++] = on ? "TRUE" : "FALSE";
        a[argc++] = items[i].tag;
        a[argc++] = items[i].desc;
    }
    a[argc] = NULL;

    char raw[4096] = {0};
    int rc = yad_exec(a, raw, sizeof(raw));
    free(a);
    if (rc != 0) return -1;

    int count = 0;
    char copy[4096]; strncpy(copy, raw, sizeof(copy)-1);
    char *p = copy;
    while (*p && count < maxout) {
        char *sep = p;
        while (*sep && *sep != '|' && *sep != '\n') sep++;
        int slen = (int)(sep - p);
        if (slen > 0 && slen < 255) {
            strncpy(out[count], p, slen);
            out[count][slen] = '\0';
            trim_nl(out[count]);
            if (out[count][0]) count++;
        }
        if (*sep) p = sep + 1;
        else break;
    }
    return count;
}

static void infobox_dlg(const char *title, const char *text) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--info","--title",(char*)title,"--text",clean,
                 "--timeout=60","--no-buttons", YAD_WS, NULL};
    pid_t pid = fork();
    if (pid == 0) {
        set_dark_theme_env();
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, STDOUT_FILENO); dup2(dn, STDERR_FILENO); }
        execvp("yad", a);
        _exit(0);
    }
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
    rc = (rc==-1)?-1:WEXITSTATUS(rc);
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
        strncat(report, L("   Not running as root\n",
                          "   No se esta ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("   Running as root\n","   Ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (!check_connectivity()) {
        strncat(report, L("   No internet connection\n",
                          "   Sin conexion a internet\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("   Internet connection OK\n","   Conexion a internet OK\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("which pacstrap >/dev/null 2>&1") != 0) {
        strncat(report, L("   pacstrap not found (are you in the Arch ISO?)\n",
                          "   pacstrap no encontrado (estas en la ISO de Arch?)\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("   pacstrap found\n","   pacstrap encontrado\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("mountpoint -q /mnt 2>/dev/null") == 0) {
        strncat(report, L("   /mnt is already mounted (may conflict)\n",
                          "   /mnt ya esta montado (puede haber conflicto)\n"),
                sizeof(report)-strlen(report)-1);
    } else {
        strncat(report, L("   /mnt is free\n","   /mnt libre\n"),
                sizeof(report)-strlen(report)-1);
    }

    st.laptop = is_laptop();
    if (st.laptop) {
        strncat(report, L("   Laptop detected (will install TLP power management)\n",
                          "   Laptop detectada (se instalara gestion de energia TLP)\n"),
                sizeof(report)-strlen(report)-1);
    }

    {
        struct statvfs vfs;
        if (statvfs("/", &vfs) == 0) {
            long long free_mb = ((long long)vfs.f_bavail * vfs.f_frsize) / (1024*1024);
            char line[128];
            snprintf(line,sizeof(line),
                     L("   Installer free space: %lld MB\n",
                       "   Espacio libre en instalador: %lld MB\n"), free_mb);
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
    snprintf(scan_cmd,sizeof(scan_cmd),"iwctl station '%s' scan 2>/dev/null",iface);
    (void)system(scan_cmd);

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
                 "iwctl station '%s' get-networks 2>/dev/null", iface, iface);
        FILE *fp2 = popen(scan_cmd2,"r");
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

    char q_ssid[256], q_pass[256];
    shell_quote(ssid_sel,q_ssid,sizeof(q_ssid));
    char cmd[MAX_CMD];
    if (pass[0]) {
        shell_quote(pass,q_pass,sizeof(q_pass));
        snprintf(cmd,sizeof(cmd),"iwctl --passphrase %s station '%s' connect %s",
                 q_pass,iface,q_ssid);
    } else {
        snprintf(cmd,sizeof(cmd),"iwctl station '%s' connect %s",iface,q_ssid);
    }
    (void)system(cmd);
    sleep(5);

    if (!check_connectivity()) {
        char fail_msg[512];
        snprintf(fail_msg,sizeof(fail_msg),
                 L(" Could not connect to '%s'.\n\nPossible causes:\n"
                   "  - Wrong password\n  - Network out of range\n  - DHCP not responding\n\n"
                   "Press OK to try again or Cancel to go back.",
                   " No se pudo conectar a '%s'.\n\nPosibles causas:\n"
                   "  - Contraseña incorrecta\n  - Red fuera de alcance\n  - DHCP sin respuesta\n\n"
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
                          "  - Is the network cable plugged in?\n"
                          "  - Is the router/switch turned on?\n"
                          "  - Did your router assign an IP? (try: dhclient eth0)\n\n"
                          "Try again now?",
                          "No se pudo alcanzar internet por cable.\n\n"
                          "Comprueba:\n"
                          "  - ¿Está el cable de red conectado?\n"
                          "  - ¿Está encendido el router/switch?\n"
                          "  - ¿Tu router asignó una IP? (prueba: dhclient eth0)\n\n"
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

static regex_t g_re_install;
static regex_t g_re_download;
static int      g_re_ready = 0;

static void compile_regexes(void) {
    if (g_re_ready) return;
    regcomp(&g_re_install,  "\\(([0-9]+)/([0-9]+)\\)", REG_EXTENDED);
    regcomp(&g_re_download,
            "[^[:space:]]+ +[0-9]+\\.?[0-9]* +(B|KiB|MiB|GiB)"
            " +[0-9]+\\.?[0-9]* +(B|KiB|MiB|GiB)/s", REG_EXTENDED);
    g_re_ready = 1;
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
                "  1. Use a ZFS-enabled Arch ISO:\n"
                "     https://archzfs.leibelt.de\n"
                "  2. Wait for archzfs to release an updated zfs-linux.\n"
                "  3. Choose a different filesystem (ext4, btrfs, xfs).",
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
            fprintf(f,"title   Arch Linux (%s)\n", tok);
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
                             "    module_path: boot():/%s.img\n",microcode);
            } else {
                snprintf(kpath,sizeof(kpath),"boot():%s/boot/vmlinuz-%s",btrfs_prefix,tok);
                snprintf(ipath,sizeof(ipath),"boot():%s/boot/initramfs-%s.img",btrfs_prefix,tok);
                if (microcode[0])
                    snprintf(ucode_line,sizeof(ucode_line),
                             "    module_path: boot():%s/boot/%s.img\n",btrfs_prefix,microcode);
            }
            fprintf(f,"/Arch Linux (%s)\n    protocol: linux\n",tok);
            fprintf(f,"    path: %s\n", kpath);
            fprintf(f,"    cmdline: %s rw quiet %s\n", root_opt, extra);
            if (ucode_line[0]) fputs(ucode_line, f);
            fprintf(f,"    module_path: %s\n\n", ipath);
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
                 "--label 'Arch Linux Limine' --loader '\\EFI\\limine\\BOOTX64.EFI' --unicode",
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
    const char *nv_pkg = !strcmp(st.kernel,"linux") ? "nvidia" : "nvidia-dkms";
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
             "    Identifier \"system-keyboard\"\n"
             "    MatchIsKeyboard \"on\"\n"
             "    Option \"XkbLayout\" \"%s\"\n"
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
        return NULL;
    }

    ib_pct(ib,100);
    ib_stage(ib, L("Installation complete!","Instalacion completa!"));
    ib->on_done(1,"",ib->ud);
    return NULL;
}

static int   g_prog_fd  = -1;
static pid_t g_prog_pid = -1;

static void on_progress_cb(double pct, void *ud) {
    (void)ud;
    if (g_prog_fd < 0) return;
    char buf[32];
    snprintf(buf, sizeof(buf), "%d\n", (int)pct);
    (void)write(g_prog_fd, buf, strlen(buf));
}

static void on_stage_cb(const char *msg, void *ud) {
    (void)ud;
    if (g_prog_fd < 0) return;
    char buf[1024];
    snprintf(buf, sizeof(buf), "# %s\n", msg);
    (void)write(g_prog_fd, buf, strlen(buf));
}

typedef struct {
    volatile int  done;
    int           success;
    char          reason[1024];
    pthread_mutex_t mu;
    pthread_cond_t  cv;
} InstallState;

static void on_done_cb(int ok, const char *reason, void *ud) {
    InstallState *iss = ud;
    pthread_mutex_lock(&iss->mu);
    iss->success = ok;
    if (reason && *reason) {
        strncpy(iss->reason, reason, sizeof(iss->reason) - 1);
        iss->reason[sizeof(iss->reason) - 1] = '\0';
    }
    iss->done = 1;
    pthread_cond_signal(&iss->cv);
    pthread_mutex_unlock(&iss->mu);
}

static int screen_install(void) {
    { FILE *f = fopen(LOG_FILE, "a"); if (f) fclose(f); }

    int pfd[2];
    if (pipe(pfd) != 0) {
        msgbox(L("Error","Error"),
               L("Could not create pipe for progress display.",
                 "No se pudo crear el pipe para progreso."));
        return 0;
    }

    pid_t yad_pid = fork();
    if (yad_pid == 0) {
        dup2(pfd[0], STDIN_FILENO);
        close(pfd[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) dup2(dn, STDERR_FILENO);
        set_dark_theme_env();
        char *a[] = {
            "yad", "--progress",
            "--title",  TITLE "  " VERSION,
            "--text",   L("Installing Arch Linux - please wait...",
                          "Instalando Arch Linux - por favor espere..."),
            "--percentage", "0",
            "--maximize",
            "--auto-kill",
            "--auto-close",
            "--no-buttons",
            "--enable-log",
            "--log-expanded",
            "--log-height=200",
            "--log-on-top",
            "--center",
            NULL
        };
        execvp("yad", a);
        char *b[] = {
            "yad", "--progress",
            "--title",  TITLE "  " VERSION,
            "--text",   L("Installing Arch Linux...", "Instalando Arch Linux..."),
            "--percentage", "0",
            "--maximize",
            "--auto-kill",
            "--auto-close",
            "--no-buttons",
            "--center",
            NULL
        };
        execvp("yad", b);
        _exit(1);
    }
    close(pfd[0]);
    g_prog_fd  = pfd[1];
    g_prog_pid = yad_pid;

    InstallState iss;
    memset(&iss, 0, sizeof(iss));
    pthread_mutex_init(&iss.mu, NULL);
    pthread_cond_init(&iss.cv,  NULL);

    IB *ib = calloc(1, sizeof(IB));
    ib->on_progress = on_progress_cb;
    ib->on_stage    = on_stage_cb;
    ib->on_done     = on_done_cb;
    ib->ud          = &iss;
    pthread_mutex_init(&ib->lock, NULL);

    IBRunArg *ra = malloc(sizeof(IBRunArg));
    ra->ib = ib;

    pthread_t install_tid;
    pthread_create(&install_tid, NULL, ib_run_thread, ra);

    pthread_mutex_lock(&iss.mu);
    while (!iss.done) pthread_cond_wait(&iss.cv, &iss.mu);
    pthread_mutex_unlock(&iss.mu);

    pthread_join(install_tid, NULL);

    if (iss.success && g_prog_fd >= 0) {
        const char *final_msg = "100\n# Installation complete!\n";
        (void)write(g_prog_fd, final_msg, strlen(final_msg));
        sleep(1);
    }

    if (g_prog_fd >= 0) { close(g_prog_fd); g_prog_fd = -1; }
    if (g_prog_pid > 0) { waitpid(g_prog_pid, NULL, 0); g_prog_pid = -1; }

    pthread_mutex_destroy(&ib->lock);
    free(ib);
    pthread_mutex_destroy(&iss.mu);
    pthread_cond_destroy(&iss.cv);

    if (!iss.success) {
        char msg[1536];
        if (!iss.reason[0]) {
            snprintf(iss.reason, sizeof(iss.reason),
                     "Installation failed. Check %s for details.", LOG_FILE);
        }
        snprintf(msg, sizeof(msg),
                 L("Installation failed.\n\n%s\n\nCheck %s for details.",
                   "La instalacion fallo.\n\n%s\n\nRevisa %s para detalles."),
                 iss.reason, LOG_FILE);
        msgbox(L("Installation Failed", "Instalacion fallida"), msg);
        return 0;
    }
    return 1;
}

static void screen_welcome(void) {
    char text[1024];
    snprintf(text,sizeof(text),
        "\\Zb\\Z4Welcome to the Arch Linux Installer\\Zn\n\n"
        "Version: %s    Boot mode: \\Zb%s\\Zn\n\n"
        "\\Zb\\Z1WARNING:\\Zn  This installer will ERASE and install Arch Linux "
        "to the selected disk.\n\n"
        "Use \\ZbTab\\Zn and \\ZbArrow keys\\Zn to navigate.\n"
        "\\ZbSpace\\Zn toggles items in multi-select screens.\n"
        "Press OK to begin.",
        VERSION, is_uefi()?"UEFI":"BIOS (Legacy)");
    msgbox("Welcome", text);
}

static void screen_language(void) {
    MenuItem items[2];
    strncpy(items[0].tag,"en",255); strncpy(items[0].desc,"English",511);
    strncpy(items[1].tag,"es",255); strncpy(items[1].desc,"Espanol",511);
    char out[8]={0};
    if (menu_dlg("Language / Idioma",
                 "Choose the installer language:\nSeleccione el idioma del instalador:",
                 items,2,out,sizeof(out)) && out[0])
        strncpy(st.lang,out,sizeof(st.lang)-1);
}

static int screen_mode(void) {
    MenuItem items[2];
    strncpy(items[0].tag,"quick",255);
    snprintf(items[0].desc,511,"%s",
        L("Quick Install   (sane defaults, installs yay + snapper)",
          "Instalacion rapida   (valores por defecto, instala yay + snapper)"));
    strncpy(items[1].tag,"custom",255);
    snprintf(items[1].desc,511,"%s",
        L("Custom Install  (full control)",
          "Instalacion personalizada  (control total)"));
    char out[16]={0};
    menu_dlg(L("Install Mode","Modo de instalacion"),
             L("Quick Install  -  BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
               "Custom Install -  configure everything step by step",
               "Instalacion rapida  -  BTRFS + KDE Plasma + linux + pipewire + yay + snapper\n"
               "Instalacion personalizada - configura todo paso a paso"),
             items,2,out,sizeof(out));
    if (!strcmp(out,"quick")) {
        st.quick=1;
        strncpy(st.filesystem,"btrfs",sizeof(st.filesystem)-1);
        strncpy(st.kernel,"linux",sizeof(st.kernel)-1);
        strncpy(st.kernel_list,"linux",sizeof(st.kernel_list)-1);
        strncpy(st.desktop,"KDE Plasma",sizeof(st.desktop)-1);
        strncpy(st.desktop_list,"KDE Plasma",sizeof(st.desktop_list)-1);
        st.mirrors=1; detect_gpu(st.gpu,sizeof(st.gpu));
        st.yay=1; st.snapper=1;
        strncpy(st.bootloader,"grub",sizeof(st.bootloader)-1);
        char sw[8]; snprintf(sw,sizeof(sw),"%d",suggest_swap_gb());
        strncpy(st.swap,sw,sizeof(st.swap)-1);
        return 1;
    }
    return 0;
}

static int screen_identity(void) {
    while(1) {

        char summary[256];
        snprintf(summary, sizeof(summary),
            L(" Summary: disk=%s  fs=%s  kernel=%s",
              " Resumen: disco=%s  fs=%s  kernel=%s"),
            st.disk[0]?st.disk:"?", st.filesystem, st.kernel);

        char hn_text[512];
        snprintf(hn_text, sizeof(hn_text),
            L("%s\n\n"
              " Computer name (hostname)\n"
              "   Used to identify this computer on the network.\n"
              "   Examples:  my-laptop   archpc   juan-desktop\n\n"
              "   Rules: letters, numbers, hyphens (-). Max 32 chars.\n"
              "          Must start with a letter.",
              "%s\n\n"
              " Nombre del equipo (hostname)\n"
              "   Se usa para identificar este equipo en la red.\n"
              "   Ejemplos:  mi-laptop   archpc   pc-de-juan\n\n"
              "   Reglas: letras, números, guiones (-). Máx 32 chars.\n"
              "           Debe empezar con una letra."),
            summary);

        char hn[64]={0};
        if (!inputbox_dlg(
                L("Step: Who are you? (1/2)","Paso: ¿Quién eres? (1/2)"),
                hn_text, st.hostname, hn, sizeof(hn))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        if (!validate_name(hn)) {
            char err[512];
            snprintf(err, sizeof(err),
                L(" '%s' is not valid.\n\n"
                  " Good:  my-pc   arch   juan2\n"
                  " Bad:   123abc  my pc  -start\n\n"
                  "Start with a letter, then letters/numbers/hyphens only.",
                  " '%s' no es válido.\n\n"
                  " Bien:  mi-pc   arch   juan2\n"
                  " Mal:   123abc  mi pc  -inicio\n\n"
                  "Empieza con letra, luego letras/números/guiones."),
                hn);
            msgbox(L("Invalid hostname","Nombre de equipo inválido"), err);
            continue;
        }

        char un_text[512];
        snprintf(un_text, sizeof(un_text),
            L("%s\n\n"
              " Your username\n"
              "   This will be your personal account.\n"
              "   Examples:  alice   bob   juan   myuser\n\n"
              "   Rules: letters, numbers, hyphens (-). Max 32 chars.\n"
              "          Must start with a letter. Use lowercase.",
              "%s\n\n"
              " Tu nombre de usuario\n"
              "   Esta será tu cuenta personal en el sistema.\n"
              "   Ejemplos:  alice   bob   juan   miusuario\n\n"
              "   Reglas: letras, números, guiones (-). Máx 32 chars.\n"
              "           Debe empezar con letra. Usa minúsculas."),
            summary);

        char un[64]={0};
        if (!inputbox_dlg(
                L("Step: Who are you? (2/2)","Paso: ¿Quién eres? (2/2)"),
                un_text, st.username, un, sizeof(un))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        if (!validate_name(un)) {
            char err[512];
            snprintf(err, sizeof(err),
                L(" '%s' is not a valid username.\n\n"
                  " Good:  alice   bob2   my-user\n"
                  " Bad:   2bob   My User   root\n\n"
                  "Start with a lowercase letter. Letters/numbers/hyphens only.",
                  " '%s' no es un nombre de usuario válido.\n\n"
                  " Bien:  alice   bob2   mi-usuario\n"
                  " Mal:   2bob   Mi Usuario   root\n\n"
                  "Empieza con letra minúscula. Solo letras/números/guiones."),
                un);
            msgbox(L("Invalid username","Nombre de usuario inválido"), err);
            continue;
        }
        strncpy(st.hostname,hn,sizeof(st.hostname)-1);
        strncpy(st.username,un,sizeof(st.username)-1);
        return 1;
    }
}

static int screen_passwords(void) {

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: user=%s  hostname=%s",
          " Resumen: usuario=%s  hostname=%s"),
        st.username[0]?st.username:"?",
        st.hostname[0]?st.hostname:"?");

    while(1) {

        char rp_hdr[2048];
        snprintf(rp_hdr,sizeof(rp_hdr),
            L("%s\n\n"
              " ROOT password (administrator / superuser)\n"
              "   This is the most powerful account on the system.\n"
              "   Choose something strong and write it down.",
              "%s\n\n"
              " Contraseña de ROOT (administrador)\n"
              "   Es la cuenta más poderosa del sistema.\n"
              "   Elige algo seguro y guárdalo en un lugar seguro."),
            summary);

        char rp1[256]={0},rp2[256]={0};
        if (!passwordbox_dlg(L(" Root password (1/2)"," Contraseña root (1/2)"),
                             rp_hdr, rp1, sizeof(rp1))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        {
            char slabel[64]; password_strength_label(rp1,slabel,sizeof(slabel),st.lang);
            int s = password_strength(rp1);
            if (s == 1) {
                char warn[512];
                snprintf(warn,sizeof(warn),
                    L("  Password strength: %s\n\n"
                      "Your root password is WEAK.\n\n"
                      "Tips:\n"
                      "  - Use at least 8 characters\n"
                      "  - Mix UPPERCASE, lowercase, numbers and symbols\n"
                      "  - Example: Arch!2025secured\n\n"
                      "Continue anyway? (not recommended)",
                      "  Fuerza de contraseña: %s\n\n"
                      "Tu contraseña de root es DÉBIL.\n\n"
                      "Consejos:\n"
                      "  - Usa al menos 8 caracteres\n"
                      "  - Mezcla MAYÚSCULAS, minúsculas, números y símbolos\n"
                      "  - Ejemplo: Arch!2025seguro\n\n"
                      "¿Continuar de todas formas? (no recomendado)"),
                    slabel);
                if (!yesno_dlg(L("Weak password!","¡Contraseña débil!"),warn))
                    continue;
            }
        }

        char rp2_hdr[256];
        snprintf(rp2_hdr,sizeof(rp2_hdr),
            L(" Confirm ROOT password\n   Type the same password again to verify.",
              " Confirma la contraseña de ROOT\n   Escríbela otra vez para verificar."));
        if (!passwordbox_dlg(L(" Root password (2/2)"," Contraseña root (2/2)"),
                             rp2_hdr, rp2, sizeof(rp2))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }
        if (!rp1[0]) {
            msgbox(L("Error","Error"),
                   L("Root password cannot be empty.",
                     "La contraseña root no puede estar vacía."));
            continue;
        }
        if (strcmp(rp1,rp2)) {
            msgbox(L("Passwords don't match","Las contraseñas no coinciden"),
                   L("The two root passwords you entered are different.\nPlease try again.",
                     "Las dos contraseñas de root no son iguales.\nInténtalo de nuevo."));
            continue;
        }

        char up_hdr[2048];
        snprintf(up_hdr,sizeof(up_hdr),
            L(" Password for user: %s\n\n"
              "   This is your everyday login password.\n"
              "   Can be the same or different from root.",
              " Contraseña del usuario: %s\n\n"
              "   Es la contraseña con la que inicias sesión a diario.\n"
              "   Puede ser igual o diferente a la de root."),
            st.username[0]?st.username:"user");

        char up1[256]={0},up2[256]={0};
        if (!passwordbox_dlg(L(" User password (1/2)"," Contraseña usuario (1/2)"),
                             up_hdr, up1, sizeof(up1))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        {
            char slabel[64]; password_strength_label(up1,slabel,sizeof(slabel),st.lang);
            int s = password_strength(up1);
            if (s == 1) {
                char warn[512];
                snprintf(warn,sizeof(warn),
                    L("  User password strength: %s\n\n"
                      "This password is WEAK. It could be guessed easily.\n\n"
                      "Continue anyway?",
                      "  Fuerza de contraseña de usuario: %s\n\n"
                      "Esta contraseña es DÉBIL. Podría adivinarse fácilmente.\n\n"
                      "¿Continuar de todas formas?"),
                    slabel);
                if (!yesno_dlg(L("Weak password!","¡Contraseña débil!"),warn))
                    continue;
            }
        }

        char up2_hdr[256];
        snprintf(up2_hdr,sizeof(up2_hdr),
            L(" Confirm user password\n   Type it again to verify.",
              " Confirma la contraseña de usuario\n   Escríbela otra vez."));
        if (!passwordbox_dlg(L(" User password (2/2)"," Contraseña usuario (2/2)"),
                             up2_hdr, up2, sizeof(up2))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }
        if (!up1[0]) {
            msgbox(L("Error","Error"),
                   L("User password cannot be empty.",
                     "La contraseña de usuario no puede estar vacía."));
            continue;
        }
        if (strcmp(up1,up2)) {
            msgbox(L("Passwords don't match","Las contraseñas no coinciden"),
                   L("The two user passwords you entered are different.\nPlease try again.",
                     "Las dos contraseñas de usuario no son iguales.\nInténtalo de nuevo."));
            continue;
        }
        strncpy(st.root_pass,rp1,sizeof(st.root_pass)-1);
        strncpy(st.user_pass,up1,sizeof(st.user_pass)-1);
        return 1;
    }
}

static int screen_disk(void) {

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: mode=%s  fs=%s  kernel=%s",
          " Resumen: modo=%s  fs=%s  kernel=%s"),
        is_uefi()?"UEFI":"BIOS", st.filesystem, st.kernel);

    {
        MenuItem mode_items[2];
        strncpy(mode_items[0].tag,"full",255);
        snprintf(mode_items[0].desc,511,"%s",
            L("Full Install    Easiest  — whole disk erased, Arch takes over",
              "Instalación completa  Más fácil — borra todo el disco para Arch"));
        strncpy(mode_items[1].tag,"dual",255);
        snprintf(mode_items[1].desc,511,"%s",
            L("Dual Boot      — keep Windows/Linux, share disk with Arch",
              "Dual Boot      — conservar Windows/Linux, compartir disco con Arch"));

        char mode_text[4096];
        snprintf(mode_text,sizeof(mode_text),"%s\n\n%s",summary,
            L(" How do you want to install Arch Linux?\n\n"
              "  FULL INSTALL (recommended for new installs)\n"
              "    -> The whole selected disk is erased and Arch is installed on it.\n"
              "    -> Simple, clean, no leftovers. Fastest option.\n"
              "      ALL data on that disk will be permanently deleted!\n\n"
              "  DUAL BOOT (keep another OS)\n"
              "    -> You already have Windows or another Linux on the disk.\n"
              "    -> You want to keep it and ALSO install Arch alongside it.\n"
              "    -> A new partition is created automatically in the free space.\n"
              "    -> At boot, a menu will let you choose which OS to start.\n"
              "      You only need free unpartitioned space on the disk.\n"
              "      Only the new Arch partition is touched — other OS is safe.",
              " ¿Cómo quieres instalar Arch Linux?\n\n"
              "  INSTALACIÓN COMPLETA (recomendado para instalaciones nuevas)\n"
              "    -> Todo el disco seleccionado se borra y Arch se instala en él.\n"
              "    -> Simple, limpio, sin restos. La opción más rápida.\n"
              "      ¡TODOS los datos de ese disco se borrarán para siempre!\n\n"
              "  DUAL BOOT (conservar otro sistema operativo)\n"
              "    -> Ya tienes Windows u otro Linux en el disco.\n"
              "    -> Quieres conservarlo E instalar Arch al lado.\n"
              "    -> Se crea una nueva particion automaticamente en el espacio libre.\n"
              "    -> Al arrancar, un menú te dejará elegir qué sistema iniciar.\n"
              "      Solo necesitas espacio libre sin particionar en el disco.\n"
              "      Solo se toca la nueva particion de Arch — el otro SO esta seguro."));

        char mode_out[16]={0};
        if (!radiolist_dlg(
                L(" Install Type"," Tipo de instalación"),
                mode_text, mode_items, 2,
                st.dualboot ? "dual" : "full",
                mode_out, sizeof(mode_out))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }
        st.dualboot = !strcmp(mode_out,"dual");
    }

    if (!st.dualboot) {
        DiskInfo disks[32]; int nd = list_disks(disks,32);
        if (nd==0) {
            msgbox(L("No disks found","Sin discos"),
                   L("No disks were detected. Cannot continue.",
                     "No se detectaron discos. No se puede continuar."));
            exit(1);
        }

        char lsblk[4096]={0};
        FILE *fp = popen("lsblk -f 2>/dev/null | head -40","r");
        if (fp) { (void)fread(lsblk,1,sizeof(lsblk)-1,fp); pclose(fp); }
        if (lsblk[0]) {
            char txt[MAX_OUT];
            snprintf(txt,sizeof(txt),
                     L("Current disk layout:\n\n%s\n  WARNING: The selected disk will be COMPLETELY ERASED.",
                       "Esquema actual de discos:\n\n%s\n  ADVERTENCIA: El disco seleccionado se BORRARÁ COMPLETAMENTE."),
                     lsblk);
            msgbox(L(" Disk overview — read before choosing!"," Vista de discos — ¡lee antes de elegir!"),txt);
        }

        MenuItem items[32];
        char cur_dev[128]="";
        if (st.disk[0]) snprintf(cur_dev,sizeof(cur_dev),"/dev/%s",st.disk);
        for (int i=0;i<nd;i++) {
            snprintf(items[i].tag,256,"/dev/%s",disks[i].name);
            int ssd = is_ssd(disks[i].name);
            const char *dtype = ssd > 0 ? "SSD " : (ssd == 0 ? "HDD " : "");
            snprintf(items[i].desc,512,"%lld GB  %s  %s",
                     disks[i].size_gb, dtype, disks[i].model);
        }

        char sel[128]={0};
        if (!radiolist_dlg(
                L(" Select Disk"," Selecciona el disco"),
                L("  ALL DATA on the selected disk will be PERMANENTLY DELETED!\n\n"
                  "   SSD  = fast solid-state drive\n"
                  "   HDD  = traditional spinning hard drive\n\n"
                  "   Select the disk where Arch Linux will be installed:",
                  "  ¡TODOS los datos del disco seleccionado se BORRARÁN PERMANENTEMENTE!\n\n"
                  "   SSD  = unidad de estado sólido rápida\n"
                  "   HDD  = disco duro tradicional giratorio\n\n"
                  "   Selecciona el disco donde se instalará Arch Linux:"),
                items,nd, cur_dev[0]?cur_dev:items[0].tag, sel, sizeof(sel))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        {
            const char *dname2 = sel; if (!strncmp(dname2,"/dev/",5)) dname2+=5;

            long long dsize_gb = 0; char dmodel[128]="";
            int ssd2 = -1;
            for (int i=0;i<nd;i++) {
                if (!strcmp(disks[i].name, dname2)) {
                    dsize_gb = disks[i].size_gb;
                    strncpy(dmodel, disks[i].model, sizeof(dmodel)-1);
                    ssd2 = is_ssd(dname2);
                    break;
                }
            }
            char c1[512];
            snprintf(c1,sizeof(c1),
                L("You selected:\n\n"
                  "  Disk:  %s\n"
                  "  Size:  %lld GB\n"
                  "  Type:  %s\n"
                  "  Model: %s\n\n"
                  "ALL data on this disk will be PERMANENTLY DELETED.\n\n"
                  "Are you sure this is the correct disk?",
                  "Has seleccionado:\n\n"
                  "  Disco:  %s\n"
                  "  Tamaño: %lld GB\n"
                  "  Tipo:   %s\n"
                  "  Modelo: %s\n\n"
                  "TODOS los datos de este disco se BORRARÁN PERMANENTEMENTE.\n\n"
                  "¿Estás seguro de que es el disco correcto?"),
                sel, dsize_gb,
                ssd2>0?"SSD ":(ssd2==0?"HDD ":"Unknown"),
                dmodel[0]?dmodel:"Unknown");
            if (!yesno_dlg(L(" Confirm disk selection"," Confirmar disco seleccionado"), c1))
                return 0;

            char c2[512];
            snprintf(c2,sizeof(c2),
                L(" LAST WARNING!\n\n"
                  "   %s  (%lld GB) will be completely erased.\n\n"
                  "   There is NO undo. All files, Windows, everything on it\n"
                  "   will be gone FOREVER.\n\n"
                  "   Are you ABSOLUTELY SURE you want to continue?",
                  " ¡ÚLTIMA ADVERTENCIA!\n\n"
                  "   %s  (%lld GB) se borrará completamente.\n\n"
                  "   NO hay vuelta atrás. Todos los archivos, Windows, todo\n"
                  "   lo que hay en él desaparecerá PARA SIEMPRE.\n\n"
                  "   ¿Estás COMPLETAMENTE SEGURO de que quieres continuar?"),
                sel, dsize_gb);
            if (!yesno_dlg(L(" Final confirmation"," Confirmación final"), c2))
                return 0;
        }

        const char *dname = sel;
        if (!strncmp(dname,"/dev/",5)) dname+=5;
        strncpy(st.disk,dname,sizeof(st.disk)-1);

        int sug_gb = suggest_swap_gb();
        {

            long long disk_gb = 0;
            for (int i=0;i<nd;i++)
                if (!strcmp(disks[i].name,st.disk)) { disk_gb=disks[i].size_gb; break; }
            int efi_mb = is_uefi() ? 512 : 0;
            long long swap_gb = sug_gb;
            long long root_gb = disk_gb - (efi_mb/1024) - swap_gb;
            char preview[1024];
            snprintf(preview,sizeof(preview),
                L(" Partition layout preview for /dev/%s (%lld GB):\n\n"
                  "  +-------------------------------------┐\n"
                  "  |  Partition 1: EFI boot   %4d MB    |\n"
                  "  |  Partition 2: Swap        %3lld GB    |\n"
                  "  |  Partition 3: Root (/)   ~%3lld GB    |  ← Arch installs here\n"
                  "  +-------------------------------------┘\n\n"
                  "  Filesystem: %s\n\n"
                  "  This layout will be applied when you confirm installation.\n"
                  "  Nothing is written to disk yet.",
                  " Vista previa del particionado de /dev/%s (%lld GB):\n\n"
                  "  +-------------------------------------┐\n"
                  "  |  Partición 1: Arranque EFI  %4d MB |\n"
                  "  |  Partición 2: Swap            %3lld GB |\n"
                  "  |  Partición 3: Raíz (/)       ~%3lld GB |  ← Arch se instala aquí\n"
                  "  +-------------------------------------┘\n\n"
                  "  Sistema de archivos: %s\n\n"
                  "  Este esquema se aplicará al confirmar la instalación.\n"
                  "  Aún no se escribe nada en el disco."),
                st.disk, disk_gb, efi_mb, swap_gb, root_gb, st.filesystem);
            msgbox(L(" Partition Preview"," Vista previa del particionado"), preview);
        }

        char sug_str[8]; snprintf(sug_str,sizeof(sug_str),"%d",sug_gb);
        while(1) {
            char swap_hdr[512];

            snprintf(swap_hdr,sizeof(swap_hdr),
                L(" Swap size\n\n"
                  "   Swap is extra 'emergency memory' on disk.\n"
                  "   If your RAM fills up, the system uses swap instead of crashing.\n"
                  "   It's also needed for hibernation (suspend-to-disk).\n\n"
                  "   Suggested: %s GB  (based on your RAM size)\n\n"
                  "   Enter swap size in GB (1–128):",
                  " Tamaño del swap\n\n"
                  "   El swap es 'memoria de emergencia' en el disco.\n"
                  "   Si tu RAM se llena, el sistema usa swap en vez de colgarse.\n"
                  "   También se necesita para hibernación (suspend-to-disk).\n\n"
                  "   Sugerido: %s GB  (basado en tu cantidad de RAM)\n\n"
                  "   Introduce el tamaño del swap en GB (1–128):"),
                sug_str);
            char sw[16]={0};
            if (!inputbox_dlg(L(" Swap Size"," Tamaño del swap"), swap_hdr,
                              st.swap[0]?st.swap:sug_str, sw, sizeof(sw))) {
                if (g_home_requested) { g_home_requested=0; return 2; }
                return 0;
            }
            trim_nl(sw);
            if (validate_swap(sw)) { strncpy(st.swap,sw,sizeof(st.swap)-1); return 1; }
            msgbox(L("Invalid swap","Swap inválido"),
                   L("Swap must be a number between 1 and 128.\nExample: 8",
                     "El swap debe ser un número entre 1 y 128.\nEjemplo: 8"));
        }

    } else {

        DiskInfo disks[32]; int nd = list_disks(disks, 32);
        if (nd == 0) {
            msgbox(L("No disks found","Sin discos"),
                   L("No disks were detected. Cannot continue.",
                     "No se detectaron discos. No se puede continuar."));
            st.dualboot = 0;
            return 0;
        }

        {
            char lsblk_out[4096]={0};
            FILE *fp_lb = popen("lsblk 2>/dev/null | head -50","r");
            if (fp_lb) { (void)fread(lsblk_out,1,sizeof(lsblk_out)-1,fp_lb); pclose(fp_lb); }

            char overview[MAX_OUT];
            snprintf(overview, sizeof(overview),
                L("DUAL BOOT — current disk layout:\n\n%s\n\n"
                  "Step 1 of 2: Select the disk that contains your existing OS\n"
                  "(Windows, another Linux, etc.)\n\n"
                  "A new partition for Arch will be created automatically\n"
                  "in the free space of the selected disk.",
                  "DUAL BOOT — esquema actual de discos:\n\n%s\n\n"
                  "Paso 1 de 2: Selecciona el disco que contiene tu sistema actual\n"
                  "(Windows, otro Linux, etc.)\n\n"
                  "Se creara automaticamente una nueva particion para Arch\n"
                  "en el espacio libre del disco seleccionado."),
                lsblk_out);
            msgbox(L("Dual Boot — Disk Overview","Dual Boot — Vista de discos"), overview);
        }

        MenuItem disk_items[32];
        char cur_dev[128]="";
        if (st.disk[0]) snprintf(cur_dev, sizeof(cur_dev), "/dev/%s", st.disk);
        for (int i = 0; i < nd; i++) {
            snprintf(disk_items[i].tag,  256, "/dev/%s", disks[i].name);
            int ssd = is_ssd(disks[i].name);
            snprintf(disk_items[i].desc, 512, "%lld GB  %s  %s",
                     disks[i].size_gb,
                     ssd > 0 ? "SSD" : (ssd == 0 ? "HDD" : ""),
                     disks[i].model);
        }

        char sel_disk[128]={0};
        if (!radiolist_dlg(
                L("Dual Boot — Step 1/2: Select disk",
                  "Dual Boot — Paso 1/2: Selecciona el disco"),
                L("Which disk contains the OS you want to keep?\n"
                  "(Arch Linux will also be installed on this disk.)\n\n"
                  "A new partition will be created automatically\n"
                  "in the available free space. Nothing is modified yet.",
                  "¿En que disco esta el sistema operativo que quieres conservar?\n"
                  "(Arch Linux tambien se instalara en este disco.)\n\n"
                  "Se creara automaticamente una nueva particion\n"
                  "en el espacio libre disponible. Todavia no se modifica nada."),
                disk_items, nd,
                cur_dev[0] ? cur_dev : disk_items[0].tag,
                sel_disk, sizeof(sel_disk))) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }

        const char *dname_db = sel_disk;
        if (!strncmp(dname_db,"/dev/",5)) dname_db += 5;
        strncpy(st.disk, dname_db, sizeof(st.disk)-1);

        {
            char lsblk2[4096]={0};
            char lsblk_cmd[256];
            snprintf(lsblk_cmd, sizeof(lsblk_cmd), "lsblk %s 2>/dev/null", sel_disk);
            FILE *fp2 = popen(lsblk_cmd, "r");
            if (fp2) { (void)fread(lsblk2,1,sizeof(lsblk2)-1,fp2); pclose(fp2); }

            char free_info[512]={0};
            char free_cmd[256];
            snprintf(free_cmd, sizeof(free_cmd),
                "parted -s %s unit GB print free 2>/dev/null"
                " | grep 'Free Space' | tail -1", sel_disk);
            FILE *fp3 = popen(free_cmd, "r");
            if (fp3) { (void)fread(free_info,1,sizeof(free_info)-1,fp3); pclose(fp3); trim_nl(free_info); }

            char txt[MAX_OUT];
            snprintf(txt, sizeof(txt),
                L("Partitions on %s:\n\n%s\n\n"
                  "Step 2 of 2: Choose the size for the new Arch Linux partition.\n\n"
                  "[+] A NEW partition will be created in the free space.\n"
                  "[+] All existing partitions (Windows, etc.) stay untouched.\n\n"
                  "Available free space: %s",
                  "Particiones en %s:\n\n%s\n\n"
                  "Paso 2 de 2: Elige el tamaño de la nueva particion de Arch Linux.\n\n"
                  "[+] Se creara una NUEVA particion en el espacio libre.\n"
                  "[+] Todas las particiones existentes (Windows, etc.) quedan intactas.\n\n"
                  "Espacio libre disponible: %s"),
                sel_disk, lsblk2,
                free_info[0] ? free_info : L("unknown","desconocido"));
            msgbox(L("Dual Boot — Partition Overview",
                     "Dual Boot — Vista de particiones"), txt);
        }

        while (1) {
            char size_prompt[512];
            snprintf(size_prompt, sizeof(size_prompt),
                L("How many GB should the new Arch Linux partition be?\n\n"
                  "   Minimum recommended: 20 GB\n"
                  "   Typical desktop install: 40-80 GB\n\n"
                  "   The partition will be created in the free space of %s.\n"
                  "   Your existing OS data will NOT be touched.\n\n"
                  "   Enter size in GB (10-2000):",
                  "¿Cuantos GB debe tener la nueva particion de Arch Linux?\n\n"
                  "   Minimo recomendado: 20 GB\n"
                  "   Instalacion de escritorio tipica: 40-80 GB\n\n"
                  "   La particion se creara en el espacio libre de %s.\n"
                  "   Los datos de tu OS actual NO seran tocados.\n\n"
                  "   Introduce el tamaño en GB (10-2000):"),
                sel_disk);
            char size_str[16]={0};
            char init_size[8]; snprintf(init_size,sizeof(init_size),"%d",st.db_size_gb>0?st.db_size_gb:30);
            if (!inputbox_dlg(
                    L("Dual Boot — Step 2/2: Partition size",
                      "Dual Boot — Paso 2/2: Tamaño de la particion"),
                    size_prompt, init_size, size_str, sizeof(size_str))) {
                if (g_home_requested) { g_home_requested=0; return 2; }
                return 0;
            }
            trim_nl(size_str);
            int gb = atoi(size_str);
            if (gb >= 10 && gb <= 2000) {
                st.db_size_gb = gb;
                break;
            }
            msgbox(L("Invalid size","Tamaño invalido"),
                   L("Please enter a number between 10 and 2000.\nExample: 40",
                     "Por favor introduce un numero entre 10 y 2000.\nEjemplo: 40"));
        }

        if (is_uefi()) {
            char auto_efi[128]={0};
            {
                char efi_cmd[256];
                snprintf(efi_cmd, sizeof(efi_cmd),
                    "lsblk -b -p -n -l -o NAME,FSTYPE,PARTTYPE %s 2>/dev/null"
                    " | grep -i 'c12a7328\\|vfat\\|fat32' | head -1", sel_disk);
                FILE *fp3 = popen(efi_cmd, "r");
                if (fp3) {
                    char line[256]={0};
                    if (fgets(line,sizeof(line),fp3))
                        sscanf(line, "%127s", auto_efi);
                    pclose(fp3);
                }
                if (!auto_efi[0]) {
                    snprintf(efi_cmd, sizeof(efi_cmd),
                        "blkid -o list 2>/dev/null | grep -i 'vfat\\|fat32' | grep '%s' | awk '{print $1}' | head -1",
                        sel_disk);
                    FILE *fp4 = popen(efi_cmd, "r");
                    if (fp4) {
                        char line[256]={0};
                        if (fgets(line,sizeof(line),fp4)) {
                            trim_nl(line);
                            if (line[0]) strncpy(auto_efi, line, sizeof(auto_efi)-1);
                        }
                        pclose(fp4);
                    }
                }
            }

            if (auto_efi[0]) {
                strncpy(st.db_efi, auto_efi, sizeof(st.db_efi)-1);
            } else {
                char efi_scan[256];
                snprintf(efi_scan, sizeof(efi_scan),
                    "lsblk -b -p -n -o PATH,FSTYPE,PARTTYPE --pairs %s 2>/dev/null"
                    " | grep -i 'c12a7328' | head -1", sel_disk);
                FILE *fp5 = popen(efi_scan,"r");
                if (fp5) {
                    char line[256]={0}; (void)fgets(line,sizeof(line),fp5); pclose(fp5);
                    char *pp = strstr(line,"PATH=\"");
                    if (pp) sscanf(pp+6,"%127[^\"]",st.db_efi);
                }
            }
        }

        st.db_swap[0] = '\0';

        return 1;
    }
    return 1;
}

static int screen_filesystem(void) {

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: disk=%s  kernel=%s",
          " Resumen: disco=%s  kernel=%s"),
        st.disk[0]?st.disk:"?", st.kernel);

    MenuItem items[4];
    strncpy(items[0].tag,"ext4",255);
    snprintf(items[0].desc,511,"%s",L(
        "ext4    Stable, fast, universal   No snapshots",
        "ext4    Estable, rápido, universal   Sin snapshots"));
    strncpy(items[1].tag,"btrfs",255);
    snprintf(items[1].desc,511,"%s",L(
        "btrfs   Snapshots, compression   Slightly more complex",
        "btrfs   Snapshots, compresión    Algo más complejo"));
    strncpy(items[2].tag,"xfs",255);
    snprintf(items[2].desc,511,"%s",L(
        "xfs     Great for large files    No snapshots, no shrink",
        "xfs     Excelente para archivos grandes   Sin snapshots"));
    strncpy(items[3].tag,"zfs",255);
    snprintf(items[3].desc,511,"%s",L(
        "zfs     Advanced checksums       [EXPERIMENTAL] complex setup",
        "zfs     Checksums avanzados      [EXPERIMENTAL] configuración compleja"));

    char dlg_text[512];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L("Choose the filesystem for your root partition.\n"
          "For most users: ext4 (simple) or btrfs (snapshots).",
          "Elige el sistema de archivos para la partición raíz.\n"
          "Para la mayoría: ext4 (simple) o btrfs (snapshots)."));

    char out[16]={0};
    if (!radiolist_dlg(L(" Filesystem"," Sistema de archivos"),
                       dlg_text,
                       items, 4, st.filesystem, out, sizeof(out))) {
        if (g_home_requested) { g_home_requested=0; return 2; }
        return 0;
    }

    if (!strcmp(out,"zfs")) {
        msgbox(L("ZFS - Important Notes","ZFS - Notas importantes"),
               L("ZFS on Arch Linux:\n\n"
                 "  - Requires the archzfs repository (added automatically).\n"
                 "  - Kernel forced to 'linux' (archzfs modules are version-locked).\n"
                 "  - GRUB will be used as bootloader (forced).\n"
                 "  - Snapper disabled (btrfs-only feature).\n"
                 "  - ZFS is experimental in this installer.\n\n"
                 "If you see ZFS-related errors, check the log.",
                 "ZFS en Arch Linux:\n\n"
                 "  - Requiere el repositorio archzfs (se agrega automáticamente).\n"
                 "  - Kernel forzado a 'linux' (módulos archzfs son versión-específicos).\n"
                 "  - Se usará GRUB como bootloader (forzado).\n"
                 "  - Snapper desactivado (solo para btrfs).\n"
                 "  - ZFS es experimental en este instalador.\n\n"
                 "Si hay errores de ZFS, revisa el log."));
        strncpy(st.bootloader,  "grub",  sizeof(st.bootloader)-1);
        strncpy(st.kernel,      "linux", sizeof(st.kernel)-1);
        strncpy(st.kernel_list, "linux", sizeof(st.kernel_list)-1);
        st.snapper = 0;
    }
    strncpy(st.filesystem, out, sizeof(st.filesystem)-1);
    return 1;
}

static int screen_kernel(void) {

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: disk=%s  fs=%s",
          " Resumen: disco=%s  fs=%s"),
        st.disk[0]?st.disk:"?", st.filesystem);

    MenuItem items[5];
    strncpy(items[0].tag,"linux",255);
    snprintf(items[0].desc,511,"%s",L(
        "linux           Latest stable   Best hardware support  (recommended)",
        "linux           Último estable   Mejor soporte hw  (recomendado)"));
    strncpy(items[1].tag,"linux-lts",255);
    snprintf(items[1].desc,511,"%s",L(
        "linux-lts       Rock-solid   Longer support   Older features",
        "linux-lts       Muy estable   Soporte largo   Funciones más antiguas"));
    strncpy(items[2].tag,"linux-zen",255);
    snprintf(items[2].desc,511,"%s",L(
        "linux-zen       Desktop/gaming tweaks   Slightly more power usage",
        "linux-zen       Optimizado escritorio/gaming   Algo más consumo"));
    strncpy(items[3].tag,"linux-hardened",255);
    snprintf(items[3].desc,511,"%s",L(
        "linux-hardened  Security patches   Some apps may break",
        "linux-hardened  Parches seguridad   Algunas apps pueden fallar"));
    strncpy(items[4].tag,"linux-cachyos",255);
    snprintf(items[4].desc,511,"%s",L(
        "linux-cachyos   Max performance   Needs cachyos repo   Less tested",
        "linux-cachyos   Máximo rendimiento   Requiere repo cachyos   Menos probado"));

    char kl_copy[512]; strncpy(kl_copy, st.kernel_list, sizeof(kl_copy)-1);
    const char *defs[8]={0}; int ndefs=0;
    char *tok = strtok(kl_copy, " ");
    while (tok && ndefs < 8) { defs[ndefs++] = tok; tok = strtok(NULL," "); }

    char dlg_text[512];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L("Choose one or more kernels. SPACE to toggle.\n"
          "The first selected kernel will be used for boot.",
          "Elige uno o más kernels. ESPACIO para marcar.\n"
          "El primer kernel seleccionado se usará para arrancar."));

    char sel[8][256]; int nsel = -1;
    while (nsel < 1) {
        nsel = checklist_dlg(
            L(" Kernel"," Kernel"),
            dlg_text,
            items, 5, defs, ndefs, sel, 8);
        if (nsel < 0) {
            if (g_home_requested) { g_home_requested=0; return 2; }
            return 0;
        }
        if (nsel == 0) {
            msgbox(L("No kernel selected","Sin kernel seleccionado"),
                   L("You must select at least one kernel.",
                     "Debes seleccionar al menos un kernel."));
        }
    }

    st.kernel_list[0] = '\0';
    for (int i = 0; i < nsel; i++) {
        if (i) strncat(st.kernel_list, " ", sizeof(st.kernel_list)-strlen(st.kernel_list)-1);
        strncat(st.kernel_list, sel[i], sizeof(st.kernel_list)-strlen(st.kernel_list)-1);
    }
    strncpy(st.kernel, sel[0], sizeof(st.kernel)-1);
    return 1;
}

static int screen_bootloader(void) {
    int uefi = is_uefi();

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: fs=%s  kernel=%s  mode=%s",
          " Resumen: fs=%s  kernel=%s  modo=%s"),
        st.filesystem, st.kernel, uefi?"UEFI":"BIOS");

    MenuItem items[3]; int ni;
    if (!uefi) {
        strncpy(items[0].tag,"grub",255);
        snprintf(items[0].desc,511,"%s",L(
            "GRUB    Works everywhere   Dual-boot friendly  (recommended)",
            "GRUB    Funciona en todo   Ideal para dual boot  (recomendado)"));
        strncpy(items[1].tag,"limine",255);
        snprintf(items[1].desc,511,"%s",L(
            "Limine  Fast, modern   Less common, less documentation",
            "Limine  Rápido, moderno   Menos común, menos documentación"));
        ni=2;
    } else {
        strncpy(items[0].tag,"grub",255);
        snprintf(items[0].desc,511,"%s",L(
            "GRUB          Universal   Dual-boot   UEFI+BIOS  (recommended)",
            "GRUB          Universal   Dual boot   UEFI+BIOS  (recomendado)"));
        strncpy(items[1].tag,"systemd-boot",255);
        snprintf(items[1].desc,511,"%s",L(
            "systemd-boot  Minimal & fast   UEFI only   No BIOS support",
            "systemd-boot  Mínimo y rápido   Solo UEFI   Sin soporte BIOS"));
        strncpy(items[2].tag,"limine",255);
        snprintf(items[2].desc,511,"%s",L(
            "Limine        Lightweight   UEFI   Less documentation",
            "Limine        Ligero   UEFI   Menos documentación"));
        ni=3;
    }

    char dlg_text[4096];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L(" The bootloader is the first program that runs when you turn on the PC.\n"
          "   It loads your Linux kernel.\n\n"
          "  GRUB -> The classic choice. Works on both old (BIOS) and new (UEFI) PCs.\n"
          "          PROS: well documented, supports dual boot (Windows+Linux),\n"
          "                theme support, rescue mode.\n"
          "          CONS: slower to start than alternatives.\n\n"
          "  systemd-boot -> Simple and fast (UEFI only).\n"
          "          PROS: boots very quickly, minimal config, integrated with systemd.\n"
          "          CONS: UEFI only, no graphical theme, limited dual-boot support.\n\n"
          "  Limine -> Modern and lightweight.\n"
          "          PROS: very fast, simple config file, supports UEFI and BIOS.\n"
          "          CONS: less community documentation than GRUB.\n\n"
          "   If unsure, choose GRUB.",
          " El gestor de arranque es el primer programa que arranca al encender el PC.\n"
          "   Carga el kernel de Linux.\n\n"
          "  GRUB -> La opción clásica. Funciona en PCs antiguos (BIOS) y modernos (UEFI).\n"
          "          PROS: muy documentado, soporta dual boot (Windows+Linux),\n"
          "                temas visuales, modo de rescate.\n"
          "          CONS: arranque algo más lento que las alternativas.\n\n"
          "  systemd-boot -> Simple y rápido (solo UEFI).\n"
          "          PROS: arranca muy rápido, config mínima, integrado con systemd.\n"
          "          CONS: solo UEFI, sin temas gráficos, soporte dual boot limitado.\n\n"
          "  Limine -> Moderno y ligero.\n"
          "          PROS: muy rápido, config simple, soporta UEFI y BIOS.\n"
          "          CONS: menos documentación en la comunidad que GRUB.\n\n"
          "   Si no sabes cuál elegir, elige GRUB."));

    char out[16]={0};
    if (!radiolist_dlg(L(" Bootloader"," Gestor de arranque"),
                       dlg_text, items, ni, st.bootloader, out, sizeof(out))) {
        if (g_home_requested) { g_home_requested=0; return 2; }
        return 0;
    }
    strncpy(st.bootloader,out,sizeof(st.bootloader)-1);
    return 1;
}

static int screen_mirrors(void) {
    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L("Yes - auto-select fastest mirrors","Si - seleccionar mirrors mas rapidos"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L("No  - keep default mirrors","No  - mantener mirrors por defecto"));
    char out[8]={0};
    if (!radiolist_dlg(L("Mirror Optimization","Optimizacion de mirrors"),
                       L("Use reflector to select the 10 fastest mirrors? (recommended)",
                         "Usar reflector para seleccionar los 10 mirrors mas rapidos? (recomendado)"),
                       items,2,st.mirrors?"yes":"no",out,sizeof(out))) return 0;
    st.mirrors = !strcmp(out,"yes");

    if (st.mirrors) {
        infobox_dlg(L("Testing network speed...","Probando velocidad de red..."),
                    L("Measuring download speed to archlinux.org...",
                      "Midiendo velocidad de descarga a archlinux.org..."));
        double speed = measure_mirror_speed("https://archlinux.org/packages/");
        char speed_msg[256];
        if (speed > 0) {
            double kbps = speed / 1024.0;
            snprintf(speed_msg,sizeof(speed_msg),
                     L("Network speed: %.0f KB/s\n\nreflector will find the fastest mirrors for your location.",
                       "Velocidad de red: %.0f KB/s\n\nreflector elegira los mirrors mas rapidos para tu ubicacion."),
                     kbps);
        } else {
            snprintf(speed_msg,sizeof(speed_msg),"%s",
                     L("Speed test inconclusive.\nreflector will still attempt to find fast mirrors.",
                       "Test de velocidad no concluyente.\nreflector intentara encontrar mirrors rapidos."));
        }
        msgbox(L("Speed Test","Test de velocidad"), speed_msg);
    }
    return 1;
}

static int screen_locale(void) {
    const char *locales[][2] = {
        {"en_US.UTF-8","English (United States)      en_US.UTF-8"},
        {"en_GB.UTF-8","English (United Kingdom)     en_GB.UTF-8"},
        {"es_ES.UTF-8","Espanol (Espana)              es_ES.UTF-8"},
        {"es_MX.UTF-8","Espanol (Mexico)              es_MX.UTF-8"},
        {"es_AR.UTF-8","Espanol (Argentina)           es_AR.UTF-8"},
        {"fr_FR.UTF-8","Francais (France)             fr_FR.UTF-8"},
        {"de_DE.UTF-8","Deutsch (Deutschland)         de_DE.UTF-8"},
        {"it_IT.UTF-8","Italiano (Italia)             it_IT.UTF-8"},
        {"pt_PT.UTF-8","Portugues (Portugal)          pt_PT.UTF-8"},
        {"pt_BR.UTF-8","Portugues (Brasil)            pt_BR.UTF-8"},
        {"ru_RU.UTF-8","Russkiy (Rossiya)             ru_RU.UTF-8"},
        {"pl_PL.UTF-8","Polski (Polska)               pl_PL.UTF-8"},
        {"nl_NL.UTF-8","Nederlands (Nederland)        nl_NL.UTF-8"},
        {"cs_CZ.UTF-8","Cestina (Ceska republika)     cs_CZ.UTF-8"},
        {"hu_HU.UTF-8","Magyar (Magyarorszag)         hu_HU.UTF-8"},
        {"ro_RO.UTF-8","Romana (Romania)              ro_RO.UTF-8"},
        {"da_DK.UTF-8","Dansk (Danmark)               da_DK.UTF-8"},
        {"nb_NO.UTF-8","Norsk (Norge)                 nb_NO.UTF-8"},
        {"sv_SE.UTF-8","Svenska (Sverige)             sv_SE.UTF-8"},
        {"fi_FI.UTF-8","Suomi (Suomi)                 fi_FI.UTF-8"},
        {"tr_TR.UTF-8","Turkce (Turkiye)              tr_TR.UTF-8"},
        {"ja_JP.UTF-8","Japanese (Japan)              ja_JP.UTF-8"},
        {"ko_KR.UTF-8","Korean (Korea)                ko_KR.UTF-8"},
        {"zh_CN.UTF-8","Chinese Simplified (China)    zh_CN.UTF-8"},
        {"ar_SA.UTF-8","Arabic (Saudi Arabia)         ar_SA.UTF-8"},
        {NULL,NULL}
    };
    int n=0; while(locales[n][0]) n++;
    MenuItem *items = malloc(n*sizeof(MenuItem));
    for (int i=0;i<n;i++) {
        strncpy(items[i].tag,locales[i][0],255);
        strncpy(items[i].desc,locales[i][1],511);
    }
    char out[32]={0};
    int ok = radiolist_dlg(
        L("System Locale","Idioma del sistema instalado"),
        L("Choose the locale for the INSTALLED SYSTEM.",
          "Elige el locale para el SISTEMA INSTALADO."),
        items, n, st.locale, out, sizeof(out));
    free(items);
    if (!ok) return 0;
    strncpy(st.locale,out,sizeof(st.locale)-1);

    const char *skm = kv_get(LOCALE_TO_KEYMAP,out);
    if (skm && strcmp(skm,st.keymap)) {
        char q[512];
        snprintf(q,sizeof(q),
                 L("The locale '%s' typically uses keymap '%s'.\n\nCurrent keymap: '%s'\n\nSwitch keymap to '%s'?",
                   "El locale '%s' suele usar el teclado '%s'.\n\nTeclado actual: '%s'\n\nCambiar teclado a '%s'?"),
                 out,skm,st.keymap,skm);
        if (yesno_dlg(L("Keyboard suggestion","Sugerencia de teclado"),q)) {
            strncpy(st.keymap,skm,sizeof(st.keymap)-1);
            char cmd[128]; snprintf(cmd,sizeof(cmd),"loadkeys '%s' >/dev/null 2>&1",skm);
            system(cmd);
        }
    }
    return 1;
}

static int screen_keymap(void) {
    const char *wanted[] = {
        "us","es","uk","fr","de","it","ru","ara",
        "pt-latin9","br-abnt2","pl2","hu","cz-qwerty",
        "sk-qwerty","ro_win","dk","no","sv-latin1",
        "fi","nl","tr_q-latin5","ja106","kr106", NULL
    };
    char avail[8192]={0};
    FILE *fp = popen("localectl list-keymaps 2>/dev/null || true","r");
    if (fp) { (void)fread(avail,1,sizeof(avail)-1,fp); pclose(fp); }

    MenuItem items[32]; int ni=0;
    for (int i=0; wanted[i] && ni<32; i++) {
        if (avail[0]) {
            char pat[64]; snprintf(pat,sizeof(pat),"\n%s\n",wanted[i]);
            char avail2[8200]; snprintf(avail2,sizeof(avail2),"\n%s\n",avail);
            if (!strstr(avail2,pat)) continue;
        }
        strncpy(items[ni].tag,wanted[i],255);
        snprintf(items[ni].desc,511,"Keyboard layout: %s",wanted[i]);
        ni++;
    }
    if (ni==0) {
        for (int i=0; wanted[i]&&ni<32; i++) {
            strncpy(items[ni].tag,wanted[i],255);
            snprintf(items[ni].desc,511,"Keyboard layout: %s",wanted[i]);
            ni++;
        }
    }

    char out[32]={0};
    if (!radiolist_dlg(L("Keyboard Layout","Distribucion de teclado"),
                       L("Select your keyboard layout.\n"
                         "Applied to both TTY console and the desktop (X11/Wayland).",
                         "Selecciona la distribucion de teclado.\n"
                         "Se aplica a la TTY y al escritorio (X11/Wayland)."),
                       items,ni,st.keymap,out,sizeof(out))) return 0;
    strncpy(st.keymap,out,sizeof(st.keymap)-1);
    char cmd[128]; snprintf(cmd,sizeof(cmd),"loadkeys '%s' >/dev/null 2>&1",out);
    system(cmd);
    return 1;
}

static int screen_timezone(void) {
    char zones_raw[65536]={0};
    FILE *fp = popen("timedatectl list-timezones 2>/dev/null || true","r");
    if (fp) { (void)fread(zones_raw,1,sizeof(zones_raw)-1,fp); pclose(fp); }

    char **zones=NULL; int nz=0, zc=0;
    if (zones_raw[0]) {
        char *p=zones_raw;
        while(*p) {
            char *nl=strchr(p,'\n'); if(!nl) nl=p+strlen(p);
            int len=nl-p;
            if(len>0) {
                if(nz>=zc) { zc=zc?zc*2:256; zones=realloc(zones,zc*sizeof(char*)); }
                zones[nz]=strndup(p,len); nz++;
            }
            p=(*nl)?nl+1:nl;
        }
    }
    if (nz==0) {
        const char *defaults[]={"UTC","Europe/Madrid","Europe/London",
                                 "America/New_York","America/Los_Angeles","Asia/Tokyo",NULL};
        for(int i=0;defaults[i];i++) {
            if(nz>=zc){zc=zc?zc*2:16;zones=realloc(zones,zc*sizeof(char*));}
            zones[nz++]=strdup(defaults[i]);
        }
    }

    char *regions[256]; int nr=0;
    for(int i=0;i<nz;i++) {
        char *sl=strchr(zones[i],'/'); if(!sl) continue;
        char reg[64]; int rlen=sl-zones[i]; if(rlen>63)rlen=63;
        strncpy(reg,zones[i],rlen); reg[rlen]='\0';
        int dup=0;
        for(int j=0;j<nr;j++) if(!strcmp(regions[j],reg)){dup=1;break;}
        if(!dup&&nr<255) regions[nr++]=strdup(reg);
    }

    MenuItem *reg_items = malloc((nr+1)*sizeof(MenuItem));
    strncpy(reg_items[0].tag,"UTC",255); strncpy(reg_items[0].desc,"UTC",511);
    for(int i=0;i<nr;i++) {
        strncpy(reg_items[i+1].tag,regions[i],255);
        strncpy(reg_items[i+1].desc,regions[i],511);
    }

    char cur_reg[64]="UTC";
    if(strchr(st.timezone,'/')) {
        char *sl=strchr(st.timezone,'/');
        int l=sl-st.timezone; if(l>63)l=63;
        strncpy(cur_reg,st.timezone,l); cur_reg[l]='\0';
    }

    char sel_reg[64]={0};
    if(!radiolist_dlg(L("Timezone - Region","Zona horaria - Region"),
                      L("Select your region:","Selecciona tu region:"),
                      reg_items,nr+1,cur_reg,sel_reg,sizeof(sel_reg))) {
        for(int i=0;i<nr;i++) free(regions[i]);
        free(reg_items);
        for(int i=0;i<nz;i++) free(zones[i]);
        free(zones);
        return 0;
    }
    for(int i=0;i<nr;i++) free(regions[i]);
    free(reg_items);

    if(!strcmp(sel_reg,"UTC")) {
        strncpy(st.timezone,"UTC",sizeof(st.timezone)-1);
        for(int i=0;i<nz;i++) free(zones[i]);
        free(zones);
        return 1;
    }

    MenuItem *city_items=NULL; int nc=0,cc=0;
    for(int i=0;i<nz;i++) {
        char *sl=strchr(zones[i],'/'); if(!sl) continue;
        int rlen=sl-zones[i];
        if(rlen!=(int)strlen(sel_reg)||strncmp(zones[i],sel_reg,rlen)) continue;
        if(nc>=cc){cc=cc?cc*2:64;city_items=realloc(city_items,cc*sizeof(MenuItem));}
        strncpy(city_items[nc].tag,sl+1,255);
        strncpy(city_items[nc].desc,sl+1,511);
        nc++;
    }
    for(int i=0;i<nz;i++) free(zones[i]);
    free(zones);

    if(nc==0) {
        if(city_items)free(city_items);
        strncpy(st.timezone,sel_reg,sizeof(st.timezone)-1); return 1;
    }

    char cur_city[64]="";
    char *sl=strchr(st.timezone,'/');
    if(sl) strncpy(cur_city,sl+1,sizeof(cur_city)-1);

    char hdr[128]; snprintf(hdr,sizeof(hdr),
        L("Region: %s\nSelect your city:","Region: %s\nSelecciona tu ciudad:"),sel_reg);
    char sel_city[128]={0};
    int ok=radiolist_dlg(L("Timezone - City","Zona horaria - Ciudad"),hdr,
                          city_items,nc,cur_city,sel_city,sizeof(sel_city));
    free(city_items);
    if(!ok) return 0;
    snprintf(st.timezone,sizeof(st.timezone),"%s/%s",sel_reg,sel_city);
    return 1;
}

static const char *get_desktop_preview_url(const char *name) {
    if (!strcmp(name, "KDE Plasma")) return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/KDE-6.png";
    if (!strcmp(name, "Cinnamon"))   return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/cinnamonn.jpg";
    if (!strcmp(name, "GNOME"))      return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/gnome.jpg";
    if (!strcmp(name, "Hyprland"))   return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/hyprland.png";
    if (!strcmp(name, "LXQt"))       return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/lxqt.jpg";
    if (!strcmp(name, "MATE"))       return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/mate.jpg";
    if (!strcmp(name, "None"))       return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/no.png";
    if (!strcmp(name, "Sway"))       return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/sway.jpg";
    if (!strcmp(name, "XFCE"))       return "https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/xfce.jpg";
    return NULL;
}

static int desktop_preview_confirm(const char *desktop_name) {
    const char *url = get_desktop_preview_url(desktop_name);
    if (!url) return 1;

    char safe[64]; int si = 0;
    for (const char *p = desktop_name; *p && si < 60; p++)
        safe[si++] = (*p == ' ') ? '_' : *p;
    safe[si] = '\0';

    const char *ext = strrchr(url, '.');
    if (!ext) ext = ".png";

    char local_path[256];
    snprintf(local_path, sizeof(local_path), "/tmp/arch_preview_%s%s", safe, ext);

    if (access(local_path, F_OK) != 0) {
        char cmd[512];
        snprintf(cmd, sizeof(cmd),
                 "curl -s -L --max-time 15 -o '%s' '%s' 2>/dev/null",
                 local_path, url);
        (void)system(cmd);
    }

    if (access(local_path, F_OK) != 0) return 1;

    char title[128];
    snprintf(title, sizeof(title),
             L("Preview: %s", "Vista previa: %s"), desktop_name);
    char text[512];
    snprintf(text, sizeof(text),
             L("This is how <b>%s</b> looks.\nAre you sure you want to install it?",
               "Así se verá <b>%s</b>.\n¿Estás seguro de que quieres instalarlo?"),
             desktop_name);
    char btn_ok[64], btn_back[64];
    snprintf(btn_ok,   sizeof(btn_ok),   "--button=%s:0",
             L("✓  Install this DE","✓  Instalar este escritorio"));
    snprintf(btn_back, sizeof(btn_back), "--button=%s:1",
             L("← Back","← Volver"));

    char *argv[32]; int ai = 0;
    argv[ai++] = "yad";
    argv[ai++] = "--picture";
    argv[ai++] = "--size=fit";
    argv[ai++] = "--title";    argv[ai++] = title;
    argv[ai++] = "--filename"; argv[ai++] = local_path;
    argv[ai++] = "--width=1100";
    argv[ai++] = "--height=650";
    argv[ai++] = "--center";
    argv[ai++] = btn_ok;
    argv[ai++] = btn_back;
    argv[ai] = NULL;

    return yad_exec(argv, NULL, 0) == 0;
}

static int screen_desktop(void) {
    const char *desktops[][2] = {
        {"KDE Plasma",L("KDE Plasma - full featured, modern","KDE Plasma - completo, moderno")},
        {"GNOME",     L("GNOME     - clean, Wayland-first","GNOME     - limpio, Wayland primero")},
        {"Cinnamon",  L("Cinnamon  - classic, Windows-like","Cinnamon  - clasico, similar a Windows")},
        {"XFCE",      L("XFCE      - lightweight, traditional","XFCE      - ligero, tradicional")},
        {"MATE",      L("MATE      - GNOME 2 fork, very stable","MATE      - fork de GNOME 2, muy estable")},
        {"LXQt",      L("LXQt      - minimal Qt desktop","LXQt      - escritorio Qt minimalista")},
        {"Hyprland",  L("Hyprland  - tiling Wayland compositor + animations",
                        "Hyprland  - compositor Wayland tiling + animaciones")},
        {"Sway",      L("Sway      - tiling Wayland compositor, i3-compatible",
                        "Sway      - compositor Wayland tiling, compatible con i3")},
        {"None",      L("None      - CLI only, no desktop","None      - solo terminal, sin escritorio")},
        {NULL,NULL}
    };
    int n=0; while(desktops[n][0]) n++;
    MenuItem *items = malloc(n*sizeof(MenuItem));
    for(int i=0;i<n;i++) {
        strncpy(items[i].tag,desktops[i][0],255);
        strncpy(items[i].desc,desktops[i][1],511);
    }

    char dl_copy[512]; strncpy(dl_copy, st.desktop_list, sizeof(dl_copy)-1);
    const char *defs[8]={0}; int ndefs=0;
    char *tok = strtok(dl_copy,"|");
    while (tok && ndefs < 8) { defs[ndefs++] = tok; tok = strtok(NULL,"|"); }

    char sel[8][256]; int nsel = -1;
    for (;;) {

        nsel = checklist_dlg(
            L("Desktop Environment","Entorno de escritorio"),
            L("Select one or more desktop environments to install.\n"
              "Use SPACE to toggle. The DM of the first selected DE will be enabled.\n"
              "Select 'None' if you want a CLI-only install.",
              "Selecciona uno o mas entornos de escritorio.\n"
              "Usa ESPACIO para activar/desactivar. El DM del primer DE se habilitara.\n"
              "Selecciona 'None' para una instalacion solo de terminal."),
            items, n, defs, ndefs, sel, 8);
        if (nsel < 0) { free(items); return 0; }
        if (nsel == 0) {
            msgbox(L("No selection","Sin seleccion"),
                   L("Select at least one option (or 'None' for CLI).",
                     "Selecciona al menos una opcion (o 'None' para solo terminal)."));
            continue;
        }

        char cleaned[8][256]; int nc = 0;
        int has_non_none = 0;
        for (int i = 0; i < nsel; i++) if (strcmp(sel[i], "None")) has_non_none = 1;
        for (int i = 0; i < nsel; i++) {
            if (has_non_none && !strcmp(sel[i], "None")) continue;
            strncpy(cleaned[nc++], sel[i], 255);
        }
        if (nc == 0) { strncpy(cleaned[0], "None", 255); nc = 1; }

        if (!desktop_preview_confirm(cleaned[0])) {

            nsel = -1;
            continue;
        }

        st.desktop_list[0] = '\0';
        for (int i = 0; i < nc; i++) {
            if (i) strncat(st.desktop_list, "|",
                           sizeof(st.desktop_list) - strlen(st.desktop_list) - 1);
            strncat(st.desktop_list, cleaned[i],
                    sizeof(st.desktop_list) - strlen(st.desktop_list) - 1);
        }
        strncpy(st.desktop, cleaned[0], sizeof(st.desktop) - 1);
        free(items);
        return 1;
    }
}

static int screen_gpu(void) {
    char detected[32]="None"; detect_gpu(detected,sizeof(detected));
    if(strcmp(detected,"None")&&!strcmp(st.gpu,"None"))
        strncpy(st.gpu,detected,sizeof(st.gpu)-1);

    char hint[128];
    snprintf(hint,sizeof(hint),L("Detected GPU: %s","GPU detectada: %s"),detected);

    const char *gpus[][2]={
        {"NVIDIA",      L("NVIDIA proprietary (nvidia/nvidia-dkms + utils)","NVIDIA propietario")},
        {"AMD",         L("AMD open-source (mesa + vulkan-radeon)","AMD open-source (mesa + vulkan-radeon)")},
        {"Intel",       L("Intel open-source (mesa + vulkan-intel)","Intel open-source (mesa + vulkan-intel)")},
        {"Intel+NVIDIA",L("Intel + NVIDIA hybrid","Intel + NVIDIA hibrido")},
        {"Intel+AMD",   L("Intel + AMD hybrid","Intel + AMD hibrido")},
        {"None",        L("No additional GPU drivers","Sin drivers adicionales de GPU")},
        {NULL,NULL}
    };
    int n=0; while(gpus[n][0]) n++;
    MenuItem *items=malloc(n*sizeof(MenuItem));
    for(int i=0;i<n;i++) {
        strncpy(items[i].tag,gpus[i][0],255);
        strncpy(items[i].desc,gpus[i][1],511);
    }
    char text[256]; snprintf(text,sizeof(text),"%s\n\n%s",hint,
        L("Select GPU driver:","Selecciona el driver de GPU:"));
    char out[32]={0};
    int ok=radiolist_dlg(L("GPU Drivers","Drivers GPU"),text,items,n,st.gpu,out,sizeof(out));
    free(items);
    if(!ok) return 0;
    strncpy(st.gpu,out,sizeof(st.gpu)-1);

    if (!strcmp(st.gpu,"Intel+NVIDIA")) {
        MenuItem om[3];
        strncpy(om[0].tag,"hybrid",255);
        snprintf(om[0].desc,511,"%s",
                 L("Hybrid   - iGPU for display, dGPU on demand (best battery)",
                   "Hibrido  - iGPU para pantalla, dGPU bajo demanda (mejor bateria)"));
        strncpy(om[1].tag,"integrated",255);
        snprintf(om[1].desc,511,"%s",
                 L("Integrated - iGPU only (max battery, no NVIDIA)",
                   "Integrada    - solo iGPU (maxima bateria, sin NVIDIA)"));
        strncpy(om[2].tag,"nvidia",255);
        snprintf(om[2].desc,511,"%s",
                 L("NVIDIA   - dGPU only (max performance, more power)",
                   "NVIDIA   - solo dGPU (maxima potencia, mas consumo)"));
        char om_out[16]={0};
        radiolist_dlg(L("Optimus Mode","Modo Optimus"),
                      L("Select the GPU mode for your hybrid laptop.\n"
                        "(Can be changed later with: envycontrol -s <mode>)",
                        "Selecciona el modo GPU para tu laptop hibrida.\n"
                        "(Se puede cambiar luego con: envycontrol -s <modo>)"),
                      om, 3, st.optimus_mode, om_out, sizeof(om_out));
        if (om_out[0]) strncpy(st.optimus_mode, om_out, sizeof(st.optimus_mode)-1);
    }
    return 1;
}

static int screen_yay(void) {

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: user=%s  desktop=%s  fs=%s",
          " Resumen: usuario=%s  escritorio=%s  fs=%s"),
        st.username[0]?st.username:"?", st.desktop, st.filesystem);

    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L(
        "Yes  Install yay  (recommended for most users)",
        "Sí   Instalar yay  (recomendado para la mayoría)"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L(
        "No   Skip  (only use official Arch packages)",
        "No   Omitir  (solo usar paquetes oficiales de Arch)"));

    char dlg_text[4096];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L(" What is yay?\n\n"
          "   Arch Linux has an official package repository (thousands of apps).\n"
          "   But the AUR (Arch User Repository) has tens of thousands MORE packages\n"
          "   contributed by the community — like Spotify, Discord, AnyDesk, etc.\n\n"
          "   yay is an AUR helper: it lets you install AUR packages just as easily\n"
          "   as official ones, using the same  'yay -S package'  command.\n\n"
          "   PROS: access to 80,000+ extra packages, easy to use.\n"
          "   CONS: AUR packages are community-maintained (review before installing).\n\n"
          "    Recommended: YES",
          " ¿Qué es yay?\n\n"
          "   Arch Linux tiene un repositorio oficial (miles de apps).\n"
          "   Pero el AUR (Arch User Repository) tiene decenas de miles MÁS,\n"
          "   mantenidos por la comunidad — como Spotify, Discord, AnyDesk, etc.\n\n"
          "   yay es un asistente AUR: te permite instalar paquetes del AUR igual\n"
          "   de fácil que los oficiales, con el mismo comando  'yay -S paquete'.\n\n"
          "   PROS: acceso a 80.000+ paquetes extra, fácil de usar.\n"
          "   CONS: los paquetes AUR son de la comunidad (revísalos antes de instalar).\n\n"
          "    Recomendado: SÍ"));

    char out[8]={0};
    if (!radiolist_dlg(L(" AUR Helper (yay)"," AUR Helper (yay)"),
                       dlg_text, items, 2, st.yay?"yes":"no", out, sizeof(out))) {
        if (g_home_requested) { g_home_requested=0; return 2; }
        return 0;
    }
    st.yay=!strcmp(out,"yes");
    return 1;
}

static int screen_snapper(void) {
    if(strcmp(st.filesystem,"btrfs")) { st.snapper=0; return 1; }

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: fs=btrfs  kernel=%s  yay=%s",
          " Resumen: fs=btrfs  kernel=%s  yay=%s"),
        st.kernel, st.yay?"yes":"no");

    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L(
        "Yes  Enable automatic snapshots  (recommended with btrfs)",
        "Sí   Activar snapshots automáticos  (recomendado con btrfs)"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L(
        "No   Skip","No   Omitir"));

    char dlg_text[4096];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L(" What is Snapper?\n\n"
          "   Snapper is a tool that automatically takes a 'photo' (snapshot)\n"
          "   of your system before and after every software installation or update.\n\n"
          "   If an update breaks something, you can roll back to the previous\n"
          "   snapshot in seconds — like a time machine for your system!\n\n"
          "   This installer also sets up grub-btrfs so you can boot into\n"
          "   any snapshot directly from the GRUB menu.\n\n"
          "   PROS: automatic safety net, easy recovery, no extra effort needed.\n"
          "   CONS: uses some extra disk space (snapshots store changed files).\n\n"
          "     Requires btrfs filesystem — which you already selected. \n\n"
          "    Recommended: YES",
          " ¿Qué es Snapper?\n\n"
          "   Snapper es una herramienta que toma automáticamente una 'foto' (snapshot)\n"
          "   del sistema antes y después de cada instalación o actualización.\n\n"
          "   Si una actualización rompe algo, puedes volver al estado anterior\n"
          "   en segundos — ¡como una máquina del tiempo para tu sistema!\n\n"
          "   Este instalador también configura grub-btrfs para que puedas arrancar\n"
          "   desde cualquier snapshot directamente en el menú de GRUB.\n\n"
          "   PROS: red de seguridad automática, recuperación fácil, sin esfuerzo.\n"
          "   CONS: usa algo más de espacio en disco (los snapshots guardan cambios).\n\n"
          "     Requiere el sistema de archivos btrfs — que ya tienes seleccionado. \n\n"
          "    Recomendado: SÍ"));

    char out[8]={0};
    if (!radiolist_dlg(L(" BTRFS Snapshots (Snapper)"," Snapshots BTRFS (Snapper)"),
                       dlg_text, items, 2, st.snapper?"yes":"no", out, sizeof(out))) {
        if (g_home_requested) { g_home_requested=0; return 2; }
        return 0;
    }
    st.snapper=!strcmp(out,"yes");
    return 1;
}

static int screen_flatpak(void) {
    if(!strcmp(st.desktop,"None")) { st.flatpak=0; return 1; }

    char summary[256];
    snprintf(summary,sizeof(summary),
        L(" Summary: desktop=%s  yay=%s",
          " Resumen: escritorio=%s  yay=%s"),
        st.desktop, st.yay?"yes":"no");

    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L(
        "Yes  Install Flatpak + Flathub  (recommended)",
        "Sí   Instalar Flatpak + Flathub  (recomendado)"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L(
        "No   Skip","No   Omitir"));

    char dlg_text[4096];
    snprintf(dlg_text,sizeof(dlg_text),"%s\n\n%s",summary,
        L(" What is Flatpak?\n\n"
          "   Flatpak is a universal way to install apps that works on ANY Linux distro.\n"
          "   Apps installed via Flatpak run in a sandbox (isolated from the system).\n\n"
          "   Flathub is the main Flatpak store — it has apps like:\n"
          "   GIMP, VLC, Spotify, VS Code, LibreOffice, OBS, Signal, and many more.\n\n"
          "   PROS: sandboxed (safer), always up to date, easy to install any app,\n"
          "         great for apps not in Arch repos.\n"
          "   CONS: apps are larger (each brings its own libraries),\n"
          "         slightly slower first launch.\n\n"
          "   With yay (AUR) AND Flatpak you'll have access to virtually any app.\n\n"
          "    Recommended: YES",
          " ¿Qué es Flatpak?\n\n"
          "   Flatpak es una forma universal de instalar apps que funciona en CUALQUIER\n"
          "   distribución Linux. Las apps se ejecutan en un sandbox (aisladas del sistema).\n\n"
          "   Flathub es la tienda principal de Flatpak — tiene apps como:\n"
          "   GIMP, VLC, Spotify, VS Code, LibreOffice, OBS, Signal, y muchas más.\n\n"
          "   PROS: sandboxed (más seguro), siempre actualizado, fácil instalar cualquier app,\n"
          "         ideal para apps no disponibles en los repos de Arch.\n"
          "   CONS: las apps son más grandes (cada una trae sus propias librerías),\n"
          "         primer arranque algo más lento.\n\n"
          "   Con yay (AUR) Y Flatpak tendrás acceso a prácticamente cualquier app.\n\n"
          "    Recomendado: SÍ"));

    char out[8]={0};
    if (!radiolist_dlg(L(" Flatpak"," Flatpak"),
                       dlg_text, items, 2, st.flatpak?"yes":"no", out, sizeof(out))) {
        if (g_home_requested) { g_home_requested=0; return 2; }
        return 0;
    }
    st.flatpak=!strcmp(out,"yes");
    return 1;
}

static int screen_profile(void) {
    MenuItem items[5];
    strncpy(items[0].tag,"none",255);
    snprintf(items[0].desc,511,"%s",
             L("None      - no profile, just base system",
               "Ninguno   - sin perfil, solo el sistema base"));
    strncpy(items[1].tag,"gaming",255);
    snprintf(items[1].desc,511,"%s",
             L("Gaming    - Steam + Lutris + GameMode + MangoHud + Wine + multilib",
               "Gaming    - Steam + Lutris + GameMode + MangoHud + Wine + multilib"));
    strncpy(items[2].tag,"developer",255);
    snprintf(items[2].desc,511,"%s",
             L("Developer - git + Docker + Python + Node + Go + Rust + JDK + gdb",
               "Desarrollador - git + Docker + Python + Node + Go + Rust + JDK + gdb"));
    strncpy(items[3].tag,"minimal",255);
    snprintf(items[3].desc,511,"%s",
             L("Minimal   - no profile extras (lightest possible install)",
               "Minimal   - sin extras de perfil (instalacion mas ligera)"));
    strncpy(items[4].tag,"privacy",255);
    snprintf(items[4].desc,511,"%s",
             L("Privacy   - Tor + ufw + fail2ban + firejail + KeePassXC + BleachBit",
               "Privacidad - Tor + ufw + fail2ban + firejail + KeePassXC + BleachBit"));

    char out[32]={0};
    if (!radiolist_dlg(
            L("Installation Profile","Perfil de instalacion"),
            L("Choose a profile to pre-select a set of packages.\n\n"
              "Gaming:    multilib is REQUIRED and will be enabled automatically.\n"
              "Developer: Docker, full toolchain.\n"
              "Privacy:   firewall + anonymization tools.\n"
              "Minimal:   only what you explicitly selected above.",
              "Elige un perfil para preseleccionar paquetes.\n\n"
              "Gaming:    se requiere y habilita multilib automaticamente.\n"
              "Desarrollador: Docker, toolchain completo.\n"
              "Privacidad: firewall + herramientas de anonimizacion.\n"
              "Minimal:   solo lo que seleccionaste manualmente."),
            items, 5, st.profile, out, sizeof(out)))
        return 0;
    strncpy(st.profile, out, sizeof(st.profile)-1);
    return 1;
}

static int screen_dotfiles(void) {
    if (!strstr(st.desktop_list, "Hyprland")) {
        strncpy(st.dotfiles, "none", sizeof(st.dotfiles)-1);
        st.dotfiles_url[0] = '\0';
        return 1;
    }

    MenuItem items[3];
    strncpy(items[0].tag,"none",255);
    snprintf(items[0].desc,511,"%s",
             L("None     - skip dotfiles","Ninguno - omitir dotfiles"));
    strncpy(items[1].tag,"caelestia",255);
    snprintf(items[1].desc,511,"%s",
             L("Caelestia - install caelestia-dots (Hyprland rice)",
               "Caelestia - instalar caelestia-dots (rice para Hyprland)"));
    strncpy(items[2].tag,"custom",255);
    snprintf(items[2].desc,511,"%s",
             L("Custom   - provide your own git repository URL",
               "Personalizado - URL de tu propio repositorio git"));

    char out[32]={0};
    if (!radiolist_dlg(
            L("Dotfiles / Post-install","Dotfiles / Post-instalacion"),
            L("Install dotfiles after the base system is set up?\n\n"
              "The script ~/dots/install.sh (or setup.sh) will be run as your user.\n"
              "Internet is required. Errors here are non-fatal.",
              "Instalar dotfiles despues de configurar el sistema base?\n\n"
              "Se ejecutara ~/dots/install.sh (o setup.sh) como tu usuario.\n"
              "Necesitas internet. Los errores aqui no son fatales."),
            items, 3, st.dotfiles, out, sizeof(out)))
        return 0;
    strncpy(st.dotfiles, out, sizeof(st.dotfiles)-1);

    if (!strcmp(out,"custom")) {
        char url[256]={0};
        if (!inputbox_dlg(
                L("Dotfiles URL","URL de dotfiles"),
                L("Enter the git repository URL for your dotfiles:",
                  "Ingresa la URL del repositorio git de tus dotfiles:"),
                st.dotfiles_url, url, sizeof(url)))
            return 0;
        strncpy(st.dotfiles_url, url, sizeof(st.dotfiles_url)-1);
    }
    return 1;
}

static int screen_extra_packages(void) {
    static const char *pkgs[][2] = {
        {"btop",                 "Resource monitor (CPU/RAM/disk/net)"},
        {"htop",                 "Interactive process viewer"},
        {"fastfetch",            "Fast system info display"},
        {"neofetch",             "Classic system info display"},
        {"tmux",                 "Terminal multiplexer"},
        {"neovim",               "Modern Vim-based text editor"},
        {"micro",                "Easy terminal editor (Ctrl+S saves)"},
        {"ranger",               "Terminal file manager (vim keys)"},
        {"nnn",                  "Ultra-fast terminal file manager"},
        {"bat",                  "cat with syntax highlighting"},
        {"eza",                  "Modern ls replacement with colors"},
        {"fd",                   "Fast alternative to find"},
        {"ripgrep",              "Extremely fast grep replacement"},
        {"fzf",                  "Fuzzy finder for the shell"},
        {"ncdu",                 "Disk usage analyzer (TUI)"},
        {"tree",                 "Directory tree viewer"},
        {"wget",                 "CLI download tool"},
        {"aria2",                "Fast multi-protocol downloader"},
        {"yt-dlp",               "Download YouTube and other videos"},
        {"nmap",                 "Network scanner"},
        {"p7zip",                "7z archive support"},
        {"unrar",                "RAR archive support"},
        {"mpv",                  "Fast, lightweight media player"},
        {"ffmpeg",               "Multimedia converter and toolkit"},
        {"imagemagick",          "CLI image manipulation"},
        {"zsh",                  "Z shell (use with oh-my-zsh)"},
        {"fish",                 "User-friendly interactive shell"},
        {"noto-fonts",           "Google Noto fonts (wide Unicode)"},
        {"ttf-hack-nerd-font",   "Hack font with Nerd Font icons"},
        {NULL, NULL}
    };

    int n = 0;
    while (pkgs[n][0]) n++;
    MenuItem *items = malloc(n * sizeof(MenuItem));
    for (int i = 0; i < n; i++) {
        strncpy(items[i].tag,  pkgs[i][0], 255);
        strncpy(items[i].desc, pkgs[i][1], 511);
    }

    char sel[64][256];
    int nsel = checklist_dlg(
        L("Additional Packages", "Paquetes adicionales"),
        L("Select packages to install (SPACE to toggle, ENTER to confirm).\n"
          "Press Cancel or ESC to skip all.",
          "Selecciona paquetes a instalar (ESPACIO activa, ENTER confirma).\n"
          "Cancela o ESC para omitir todos."),
        items, n, NULL, 0, sel, 64);
    free(items);

    st.extra_pkgs[0] = '\0';
    int fish_selected = 0;
    if (nsel > 0) {
        for (int i = 0; i < nsel; i++) {
            if (i) strncat(st.extra_pkgs, " ", sizeof(st.extra_pkgs) - strlen(st.extra_pkgs) - 1);
            strncat(st.extra_pkgs, sel[i], sizeof(st.extra_pkgs) - strlen(st.extra_pkgs) - 1);
            if (!strcmp(sel[i], "fish")) fish_selected = 1;
        }
    }
    if (fish_selected) {
        st.fish_default = yesno_dlg(
            L("fish shell", "fish shell"),
            L("fish was selected.\n\nSet fish as the default shell for your user?",
              "Has seleccionado fish.\n\n?Establecer fish como shell por defecto para tu usuario?"));
    } else {
        st.fish_default = 0;
    }
    return 1;
}

static int review_confirm_dlg(const char *title, const char *text) {
    const char *tmpfile = "/tmp/arch_review.txt";
    FILE *f = fopen(tmpfile, "w");
    if (!f) return yesno_dlg(title, text);
    fprintf(f, "%s", text);
    fclose(f);

    char *a[] = {
        "yad", "--text-info",
        "--title",    (char*)title,
        "--filename", (char*)tmpfile,
        "--button",   L("Install:0","Instalar:0"),
        "--button",   L("Back:1","Atras:1"),
        "--width=700", "--height=500",
        "--center",
        NULL
    };
    return yad_exec(a, NULL, 0) == 0;
}

static int screen_review(void) {
    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    if(!microcode[0]) strncpy(microcode,L("none detected","no detectado"),sizeof(microcode)-1);
    const char *x11 = kv_get(CONSOLE_TO_X11,st.keymap);
    if(!x11) x11=st.keymap;
    const char *boot_mode = is_uefi()?"UEFI":"BIOS";

    char text[5120]={0};
    char line[512];
#define ROW(label,val) do { snprintf(line,sizeof(line),"  %-22s %s\n",label,val); strncat(text,line,sizeof(text)-strlen(text)-1); } while(0)
    snprintf(line,sizeof(line),"%s\n\n",L("Review your settings:","Revisa tu configuracion:"));
    strncat(text,line,sizeof(text)-strlen(text)-1);

    ROW("Mode",           st.quick?L("Quick","Rapida"):L("Custom","Personalizada"));
    ROW("Boot",           boot_mode);
    ROW("Install type",   st.dualboot?L("Dual Boot","Dual Boot"):L("Full Disk","Disco completo"));
    ROW("Installer lang", st.lang);
    ROW("System locale",  st.locale);
    ROW("Hostname",       st.hostname[0]?st.hostname:"NOT SET");
    ROW(L("Username","Usuario"), st.username[0]?st.username:"NOT SET");
    ROW("Filesystem",     st.filesystem);
    ROW("Kernels",        st.kernel_list);
    ROW("Bootloader",     st.bootloader);
    ROW("Microcode",      microcode);
    if (st.dualboot) {
        char disk_str[128]; snprintf(disk_str,sizeof(disk_str),"/dev/%s",st.disk);
        ROW("Disk (dual)",  st.disk[0]?disk_str:"NOT SET");
        char sz_str[32]; snprintf(sz_str,sizeof(sz_str),"%d GB (new partition)", st.db_size_gb);
        ROW("Arch size",    sz_str);
        if (is_uefi()) ROW("EFI part", st.db_efi[0]?st.db_efi:"auto-detect");
    } else {
        char disk_str[128]; snprintf(disk_str,sizeof(disk_str),"/dev/%s",st.disk);
        ROW("Disk",       st.disk[0]?disk_str:"NOT SET");
        char swap_str[32]; snprintf(swap_str,sizeof(swap_str),"%s GB",st.swap);
        ROW("Swap",       swap_str);
    }
    ROW("Mirrors",        st.mirrors?L("reflector (auto)","reflector (auto)"):L("default","por defecto"));
    ROW("Keymap TTY",     st.keymap);
    ROW("Keymap X11",     x11);
    ROW("Timezone",       st.timezone);
    ROW("Desktops",       st.desktop_list);
    ROW("GPU",            st.gpu);
    ROW("Audio",          strcmp(st.desktop,"None")?"pipewire":L("none","ninguno"));
    ROW("Flatpak",        st.flatpak?L("yes","si"):"no");
    ROW("yay",            st.yay?L("yes","si"):"no");
    ROW("snapper",        st.snapper?L("yes","si"):"no");
    if (st.extra_pkgs[0]) ROW("Extra pkgs", st.extra_pkgs);
#undef ROW

    char missing[256]={0};
    if(!st.hostname[0]) strncat(missing,"hostname, ",sizeof(missing)-strlen(missing)-1);
    if(!st.username[0]) strncat(missing,"username, ",sizeof(missing)-strlen(missing)-1);
    if (!st.dualboot && !st.disk[0])
        strncat(missing,"disk, ",sizeof(missing)-strlen(missing)-1);
    if (st.dualboot && !st.disk[0])
        strncat(missing,"disk, ",sizeof(missing)-strlen(missing)-1);
    if (st.dualboot && st.db_size_gb <= 0)
        strncat(missing,"partition size, ",sizeof(missing)-strlen(missing)-1);
    if(!st.root_pass[0]) strncat(missing,L("root password","contrasena root"),sizeof(missing)-strlen(missing)-1);

    if(missing[0]) {
        strncat(text,"\n",sizeof(text)-strlen(text)-1);
        snprintf(line,sizeof(line),
                 L("MISSING: %s\n\nGo back to fix before continuing.",
                   "FALTA: %s\n\nVuelve atras para corregirlo."), missing);
        strncat(text,line,sizeof(text)-strlen(text)-1);
        msgbox(L("Review - Incomplete","Revision - Incompleto"),text);
        return 0;
    }

    strncat(text,L("\nAll settings look good.","Todo listo."),sizeof(text)-strlen(text)-1);
    char confirm_msg[6144];
    if (st.dualboot) {
        char dual_warn[256];
        snprintf(dual_warn, sizeof(dual_warn),
            L("\n\nWARNING: A new %d GB partition will be created on /dev/%s.\n\nProceed?",
              "\n\nADVERTENCIA: Se creara una nueva particion de %d GB en /dev/%s.\n\nProceder?"),
            st.db_size_gb, st.disk);
        snprintf(confirm_msg,sizeof(confirm_msg),"%s%s", text, dual_warn);
    } else {
        snprintf(confirm_msg,sizeof(confirm_msg),"%s%s%s%s",
                 text,
                 L("\n\nWARNING: THIS WILL ERASE /dev/","\n\nADVERTENCIA: SE BORRARA /dev/"),
                 st.disk,
                 L(".\n\nProceed?",".\n\nProceder?"));
    }
    return review_confirm_dlg(L("Review & Confirm","Revisar y confirmar"),confirm_msg);
}

static void screen_finish(void) {
    int cfd[2];
    if (pipe(cfd) != 0) {
        sleep(5);
        (void)system("umount -R /mnt 2>/dev/null");
        (void)system("reboot");
        exit(0);
    }

    pid_t yad_pid = fork();
    if (yad_pid == 0) {
        dup2(cfd[0], STDIN_FILENO);
        close(cfd[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) dup2(dn, STDERR_FILENO);
        set_dark_theme_env();
        char *a[] = {
            "yad", "--progress",
            "--title",      L("Installation Complete!", "Instalacion Completa!"),
            "--text",       L("Arch Linux installed successfully!\n\n"
                              "<b>Remove the USB / installation media now.</b>\n\n"
                              "The system will reboot automatically...",
                              "Arch Linux instalado correctamente!\n\n"
                              "<b>Extrae ahora el USB / medio de instalacion.</b>\n\n"
                              "El sistema se reiniciara automaticamente..."),
            "--percentage", "0",
            "--maximize",
            "--no-buttons",
            "--auto-close",
            "--center",
            NULL
        };
        execvp("yad", a);
        _exit(0);
    }
    close(cfd[0]);

    for (int i = 1; i <= 10; i++) {
        usleep(500000);
        char buf[128];
        int pct = i * 10;
        int secs_left = 5 - (i / 2);
        snprintf(buf, sizeof(buf),
                 "# %s %d...\n%d\n",
                 L("Rebooting in", "Reiniciando en"),
                 secs_left > 0 ? secs_left : 0,
                 pct);
        (void)write(cfd[1], buf, strlen(buf));
    }
    close(cfd[1]);
    if (yad_pid > 0) waitpid(yad_pid, NULL, 0);

    (void)system("umount -R /mnt 2>/dev/null");
    (void)system("reboot");
    exit(0);
}

typedef struct {
    const char *name;
    int        (*fn)(void);
    int         can_go_back;
} Step;

static int screen_welcome_wrap(void)  { screen_welcome();  return 1; }
static int screen_language_wrap(void)  { screen_language(); return 1; }
static int screen_network_wrap(void) {
    g_fullscreen = 0;
    screen_network();
    g_fullscreen = 1;
    return 1;
}
static int screen_finish_wrap(void)    { screen_finish();   return 1; }
static int screen_install_wrap(void)   { return screen_install(); }
static int screen_preflight_wrap(void) { return run_preflight(); }

static void ensure_x11_deps(void) {
    static const char *deps[] = {
        "xorg-server", "xorg-xinit", "xorg-xinput",
        "xf86-input-libinput", "xf86-video-fbdev", "xf86-video-vesa",
        "xdotool", "xorg-xsetroot", "openbox", "yad",
        "xterm", "pcmanfm", "feh", "imagemagick", "tint2",
        NULL
    };

    int missing = 0;
    for (int i = 0; deps[i]; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "pacman -Q %s >/dev/null 2>&1", deps[i]);
        if (system(cmd) != 0) {
            printf("[!] Missing: %s\n", deps[i]);
            missing = 1;
        }
    }

    if (missing) {
        printf("[*] Installing desktop session dependencies...\n");
        fflush(stdout);
        if (system("pacman -Sy --noconfirm "
                   "xorg-server xorg-xinit xorg-xinput xf86-input-libinput "
                   "xf86-video-fbdev xf86-video-vesa xdotool xorg-xsetroot "
                   "openbox yad xterm pcmanfm feh imagemagick tint2") != 0) {
            fprintf(stderr,
                "[!] WARNING: Some deps failed to install.\n"
                "    The installer will try to continue anyway.\n");
        } else {
            printf("[+] Desktop dependencies installed.\n");
        }
        fflush(stdout);
    }
}

static void write_openbox_env(void) {
    (void)system("mkdir -p /root/.config/openbox");
    (void)system("mkdir -p /root/.config/tint2");
    (void)system("mkdir -p /root/Desktop");

    FILE *m = fopen("/root/.config/openbox/menu.xml", "w");
    if (m) {
        fprintf(m, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(m, "<openbox_menu xmlns=\"http://openbox.org/3.4/menu\">\n");
        fprintf(m, "  <menu id=\"root-menu\" label=\"Desktop\">\n");
        fprintf(m, "    <item label=\"Terminal\">\n");
        fprintf(m, "      <action name=\"Execute\"><command>xterm</command></action>\n");
        fprintf(m, "    </item>\n");
        fprintf(m, "    <item label=\"File Manager\">\n");
        fprintf(m, "      <action name=\"Execute\"><command>pcmanfm</command></action>\n");
        fprintf(m, "    </item>\n");
        fprintf(m, "    <separator/>\n");
        fprintf(m, "    <item label=\"Reconfigure Openbox\">\n");
        fprintf(m, "      <action name=\"Reconfigure\"/>\n");
        fprintf(m, "    </item>\n");
        fprintf(m, "  </menu>\n");
        fprintf(m, "</openbox_menu>\n");
        fclose(m);
    }

    FILE *r = fopen("/root/.config/openbox/rc.xml", "w");
    if (r) {
        fprintf(r, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(r, "<openbox_config xmlns=\"http://openbox.org/3.4/rc\"\n");
        fprintf(r, "  xmlns:xi=\"http://www.w3.org/2001/XInclude\">\n");
        fprintf(r, "  <theme>\n");
        fprintf(r, "    <n>Clearlooks</n>\n");
        fprintf(r, "    <titleLayout>NLIMC</titleLayout>\n");
        fprintf(r, "    <keepBorder>yes</keepBorder>\n");
        fprintf(r, "    <animateIconify>no</animateIconify>\n");
        fprintf(r, "    <font place=\"ActiveWindow\">\n");
        fprintf(r, "      <n>Sans Bold</n><size>10</size>\n");
        fprintf(r, "      <weight>Bold</weight><slant>Normal</slant>\n");
        fprintf(r, "    </font>\n");
        fprintf(r, "    <font place=\"InactiveWindow\">\n");
        fprintf(r, "      <n>Sans</n><size>10</size>\n");
        fprintf(r, "      <weight>Normal</weight><slant>Normal</slant>\n");
        fprintf(r, "    </font>\n");
        fprintf(r, "  </theme>\n");
        fprintf(r, "  <desktops><number>1</number><firstdesk>1</firstdesk>"
                   "<names><n>Desktop</n></names></desktops>\n");
        fprintf(r, "  <resize><drawContents>yes</drawContents></resize>\n");
        fprintf(r, "  <focus><focusNew>yes</focusNew>"
                   "<followMouse>no</followMouse></focus>\n");
        fprintf(r, "  <keyboard>\n");
        fprintf(r, "    <chainQuitKey>C-g</chainQuitKey>\n");
        fprintf(r, "    <keybind key=\"super-t\">\n");
        fprintf(r, "      <action name=\"Execute\"><command>xterm</command></action>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "    <keybind key=\"C-A-t\">\n");
        fprintf(r, "      <action name=\"Execute\"><command>xterm</command></action>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "    <keybind key=\"super-e\">\n");
        fprintf(r, "      <action name=\"Execute\"><command>pcmanfm</command></action>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "    <keybind key=\"A-F4\">\n");
        fprintf(r, "      <action name=\"Close\"/>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "    <keybind key=\"super-Up\">\n");
        fprintf(r, "      <action name=\"ToggleMaximizeFull\"/>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "    <keybind key=\"super-Down\">\n");
        fprintf(r, "      <action name=\"Unmaximize\"/>\n");
        fprintf(r, "    </keybind>\n");
        fprintf(r, "  </keyboard>\n");
        fprintf(r, "  <mouse>\n");
        fprintf(r, "    <dragThreshold>1</dragThreshold>\n");
        fprintf(r, "    <doubleClickTime>200</doubleClickTime>\n");
        fprintf(r, "    <context name=\"Desktop\">\n");
        fprintf(r, "      <mousebind button=\"Right\" action=\"Press\">\n");
        fprintf(r, "        <action name=\"ShowMenu\"><menu>root-menu</menu></action>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Root\">\n");
        fprintf(r, "      <mousebind button=\"Right\" action=\"Press\">\n");
        fprintf(r, "        <action name=\"ShowMenu\"><menu>root-menu</menu></action>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Titlebar\">\n");
        fprintf(r, "      <mousebind button=\"Left\" action=\"Drag\">\n");
        fprintf(r, "        <action name=\"Move\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "      <mousebind button=\"Left\" action=\"DoubleClick\">\n");
        fprintf(r, "        <action name=\"ToggleMaximizeFull\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Frame\">\n");
        fprintf(r, "      <mousebind button=\"A-Left\" action=\"Drag\">\n");
        fprintf(r, "        <action name=\"Move\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "      <mousebind button=\"A-Right\" action=\"Drag\">\n");
        fprintf(r, "        <action name=\"Resize\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Close\">\n");
        fprintf(r, "      <mousebind button=\"Left\" action=\"Click\">\n");
        fprintf(r, "        <action name=\"Close\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Maximize\">\n");
        fprintf(r, "      <mousebind button=\"Left\" action=\"Click\">\n");
        fprintf(r, "        <action name=\"ToggleMaximizeFull\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "    <context name=\"Iconify\">\n");
        fprintf(r, "      <mousebind button=\"Left\" action=\"Click\">\n");
        fprintf(r, "        <action name=\"Iconify\"/>\n");
        fprintf(r, "      </mousebind>\n");
        fprintf(r, "    </context>\n");
        fprintf(r, "  </mouse>\n");
        fprintf(r, "  <applications>\n");

        fprintf(r, "    <application class=\"Yad\" type=\"normal\">\n");
        fprintf(r, "      <maximized>yes</maximized>\n");
        fprintf(r, "      <decor>no</decor>\n");
        fprintf(r, "    </application>\n");

        fprintf(r, "    <application class=\"XTerm\" type=\"normal\">\n");
        fprintf(r, "      <decor>yes</decor>\n");
        fprintf(r, "    </application>\n");
        fprintf(r, "    <application class=\"Pcmanfm\" type=\"normal\">\n");
        fprintf(r, "      <decor>yes</decor>\n");
        fprintf(r, "    </application>\n");
        fprintf(r, "  </applications>\n");
        fprintf(r, "</openbox_config>\n");
        fclose(r);
    }

    FILE *t = fopen("/root/.config/tint2/tint2rc", "w");
    if (t) {
        fprintf(t, "rounded = 0\nborder_width = 0\n"
                   "background_color = #0d1117 100\n"
                   "border_color = #30363d 0\n\n");
        fprintf(t, "rounded = 4\nborder_width = 1\n"
                   "background_color = #1f2d45 100\n"
                   "border_color = #58a6ff 70\n\n");
        fprintf(t, "rounded = 4\nborder_width = 0\n"
                   "background_color = #161b22 90\n"
                   "border_color = #30363d 40\n\n");
        fprintf(t, "panel_items = TSC\n");
        fprintf(t, "panel_size = 100%% 36\n");
        fprintf(t, "panel_margin = 0 0\n");
        fprintf(t, "panel_padding = 4 2 4\n");
        fprintf(t, "panel_background_id = 1\n");
        fprintf(t, "panel_position = bottom center horizontal\n");
        fprintf(t, "panel_layer = normal\n");
        fprintf(t, "panel_monitor = all\n");
        fprintf(t, "autohide = 0\n");
        fprintf(t, "wm_menu = 1\n");
        fprintf(t, "taskbar_mode = single_desktop\n\n");
        fprintf(t, "taskbar_padding = 0 2 4\n");
        fprintf(t, "taskbar_background_id = 0\n");
        fprintf(t, "taskbar_active_background_id = 0\n\n");
        fprintf(t, "task_icon = 1\n");
        fprintf(t, "task_text = 1\n");
        fprintf(t, "task_maximum_size = 200 30\n");
        fprintf(t, "task_centered = 1\n");
        fprintf(t, "task_padding = 4 2 4\n");
        fprintf(t, "task_font = Sans 9\n");
        fprintf(t, "task_font_color = #c9d1d9 100\n");
        fprintf(t, "task_active_font_color = #79c0ff 100\n");
        fprintf(t, "task_icon_asb = 100 0 0\n");
        fprintf(t, "task_background_id = 3\n");
        fprintf(t, "task_active_background_id = 2\n\n");
        fprintf(t, "systray_padding = 4 4 6\n");
        fprintf(t, "systray_background_id = 0\n");
        fprintf(t, "systray_sort = ascending\n");
        fprintf(t, "systray_icon_size = 22\n");
        fprintf(t, "systray_icon_asb = 100 0 0\n\n");
        fprintf(t, "time1_format = %%H:%%M\n");
        fprintf(t, "time2_format = %%d/%%m/%%Y\n");
        fprintf(t, "time1_font = Sans Bold 10\n");
        fprintf(t, "time2_font = Sans 8\n");
        fprintf(t, "clock_font_color = #c9d1d9 100\n");
        fprintf(t, "clock_padding = 8 0\n");
        fprintf(t, "clock_background_id = 0\n");
        fprintf(t, "clock_tooltip = %%A %%d %%B %%Y\n");
        fclose(t);
    }

    FILE *xr = fopen("/root/.Xresources", "w");
    if (xr) {
        fprintf(xr, "XTerm*background:   #0d1117\n");
        fprintf(xr, "XTerm*foreground:   #c9d1d9\n");
        fprintf(xr, "XTerm*cursorColor:  #58a6ff\n");
        fprintf(xr, "XTerm*color0:  #161b22\nXTerm*color1:  #ff7b72\n");
        fprintf(xr, "XTerm*color2:  #3fb950\nXTerm*color3:  #d29922\n");
        fprintf(xr, "XTerm*color4:  #58a6ff\nXTerm*color5:  #bc8cff\n");
        fprintf(xr, "XTerm*color6:  #39c5cf\nXTerm*color7:  #b1bac4\n");
        fprintf(xr, "XTerm*color8:  #6e7681\nXTerm*color9:  #ffa198\n");
        fprintf(xr, "XTerm*color10: #56d364\nXTerm*color11: #e3b341\n");
        fprintf(xr, "XTerm*color12: #79c0ff\nXTerm*color13: #d2a8ff\n");
        fprintf(xr, "XTerm*color14: #56d4dd\nXTerm*color15: #f0f6fc\n");
        fprintf(xr, "XTerm*faceName:     Monospace\n");
        fprintf(xr, "XTerm*faceSize:     11\n");
        fprintf(xr, "XTerm*geometry:     100x28\n");
        fprintf(xr, "XTerm*scrollBar:    false\n");
        fprintf(xr, "XTerm*borderWidth:  0\n");
        fprintf(xr, "XTerm*internalBorder: 8\n");
        fprintf(xr, "XTerm*title:        Terminal\n");
        fclose(xr);
    }

    FILE *d;
    d = fopen("/root/Desktop/terminal.desktop", "w");
    if (d) {
        fprintf(d, "[Desktop Entry]\nVersion=1.0\nType=Application\n");
        fprintf(d, "Name=Terminal\nComment=Open a terminal window\n");
        fprintf(d, "Exec=xterm\nIcon=utilities-terminal\n");
        fprintf(d, "Terminal=false\nCategories=System;TerminalEmulator;\n");
        fclose(d);
        (void)system("chmod +x /root/Desktop/terminal.desktop");
    }
    d = fopen("/root/Desktop/files.desktop", "w");
    if (d) {
        fprintf(d, "[Desktop Entry]\nVersion=1.0\nType=Application\n");
        fprintf(d, "Name=File Manager\nComment=Browse the file system\n");
        fprintf(d, "Exec=pcmanfm\nIcon=system-file-manager\n");
        fprintf(d, "Terminal=false\nCategories=System;FileManager;\n");
        fclose(d);
        (void)system("chmod +x /root/Desktop/files.desktop");
    }
}

static void ensure_display(void) {
    if (getenv("DISPLAY") != NULL) return;

    ensure_x11_deps();

    write_openbox_env();

    char exepath[1024] = {0};
    ssize_t elen = readlink("/proc/self/exe", exepath, sizeof(exepath) - 1);
    if (elen <= 0) strncpy(exepath, "/usr/local/bin/arch_installer", sizeof(exepath)-1);
    else exepath[elen] = '\0';

    const char *xinitrc = "/tmp/.arch_installer_xinitrc";
    FILE *f = fopen(xinitrc, "w");
    if (!f) {
        fprintf(stderr, "[!] Cannot write xinitrc – starting without X.\n");
        return;
    }
    fprintf(f, "#!/bin/sh\n");
    fprintf(f, "export XDG_SESSION_TYPE=x11\n");
    fprintf(f, "export LIBINPUT_ENABLE_DEVICE_GROUP=1\n");
    fprintf(f, "xrandr --auto 2>/dev/null || true\n");
    fprintf(f, "xset r rate 300 30 2>/dev/null || true\n");
    fprintf(f, "\n");

    fprintf(f, "xrdb -merge /root/.Xresources 2>/dev/null || true\n");
    fprintf(f, "\n");

    fprintf(f, "curl -s -L --max-time 15 "
               "-o /tmp/arch_wp.png "
               "'https://raw.githubusercontent.com/humrand/arch-installation-easy/main/SourceCode/images/wallpaper.png' "
               "2>/dev/null || "
               "convert -size 1920x1080 gradient:'#0d1117-#0f2040' "
               "/tmp/arch_wp.png 2>/dev/null || true\n");
    fprintf(f, "\n");

    fprintf(f, "openbox &\n");
    fprintf(f, "sleep 0.5\n");
    fprintf(f, "\n");

    fprintf(f, "feh --bg-fill /tmp/arch_wp.png 2>/dev/null "
               "|| xsetroot -solid '#0d1117'\n");
    fprintf(f, "\n");

    fprintf(f, "pcmanfm --desktop &\n");
    fprintf(f, "sleep 0.2\n");
    fprintf(f, "\n");

    fprintf(f, "tint2 &\n");
    fprintf(f, "\n");

    fprintf(f, "xsetroot -cursor_name left_ptr\n");
    fprintf(f, "xinput list >/tmp/xinput_debug.txt 2>&1\n");
    fprintf(f, "exec \"%s\"\n", exepath);
    fclose(f);
    chmod(xinitrc, 0755);

    printf("[*] Launching Xorg display server...\n");
    printf("    Log: /tmp/xorg_installer.log\n");
    fflush(stdout);

    char cmd[2048];
    snprintf(cmd, sizeof(cmd),
             "startx %s -- :0 -nolisten tcp "
             ">>/tmp/xorg_installer.log 2>&1",
             xinitrc);
    execl("/bin/sh", "sh", "-c", cmd, NULL);

    perror("[!] startx failed");
    fprintf(stderr, "    Continuing in terminal mode (no X).\n");
}

int main(void) {
    if (geteuid() != 0) {
        fprintf(stderr,
            "This installer must be run as root.\n"
            "Example: sudo ./arch_installer\n");
        return 1;
    }

    ensure_display();

    if (system("which yad >/dev/null 2>&1") != 0) {
        printf("[*] yad not found - installing...\n");
        if (system("pacman -Sy --noconfirm yad") != 0) {
            fprintf(stderr, "[!] Failed to install yad. Check network.\n");
            return 1;
        }
    }

    screen_welcome_wrap();
    screen_language_wrap();
    screen_network_wrap();
    int quick = screen_mode();

    Step quick_steps[] = {
        {L("Locale","Idioma sistema"),     screen_locale,        1},
        {L("Keymap","Teclado"),            screen_keymap,        1},
        {L("Disk","Disco"),                screen_disk,          1},
        {L("Identity","Identidad"),        screen_identity,      1},
        {L("Passwords","Contrasenas"),     screen_passwords,        1},
        {L("Profile","Perfil"),            screen_profile,          1},
        {L("Dotfiles","Dotfiles"),         screen_dotfiles,         1},
        {L("Extra Packages","Paquetes extra"), screen_extra_packages, 1},
        {L("Review","Revision"),           screen_review,           1},
        {L("Preflight","Preflight"),       screen_preflight_wrap,   0},
        {L("Install","Instalar"),          screen_install_wrap,     0},
        {L("Finish","Finalizar"),          screen_finish_wrap,      0},
        {NULL,NULL,0}
    };

    Step custom_steps[] = {
        {L("Locale","Idioma sistema"),      screen_locale,        1},
        {L("Disk","Disco"),                 screen_disk,          1},
        {L("Filesystem","Sistema archivos"),screen_filesystem,    1},
        {L("Kernel","Kernel"),              screen_kernel,        1},
        {L("Bootloader","Bootloader"),      screen_bootloader,    1},
        {L("Mirrors","Mirrors"),            screen_mirrors,       1},
        {L("Identity","Identidad"),         screen_identity,      1},
        {L("Passwords","Contrasenas"),      screen_passwords,     1},
        {L("Keymap","Teclado"),             screen_keymap,        1},
        {L("Timezone","Zona horaria"),      screen_timezone,      1},
        {L("Desktop","Escritorio"),         screen_desktop,       1},
        {"GPU",                              screen_gpu,           1},
        {L("Profile","Perfil"),             screen_profile,       1},
        {L("Dotfiles","Dotfiles"),          screen_dotfiles,      1},
        {L("yay","yay"),                    screen_yay,           1},
        {L("Flatpak","Flatpak"),            screen_flatpak,           1},
        {L("Snapshots","Snapshots"),        screen_snapper,           1},
        {L("Extra Packages","Paquetes extra"), screen_extra_packages, 1},
        {L("Review","Revision"),            screen_review,            1},
        {L("Preflight","Preflight"),        screen_preflight_wrap,    0},
        {L("Install","Instalar"),           screen_install_wrap,  0},
        {L("Finish","Finalizar"),           screen_finish_wrap,   0},
        {NULL,NULL,0}
    };

    Step *steps = quick ? quick_steps : custom_steps;
    int idx = 0;
    while (steps[idx].fn) {
        int result = steps[idx].fn();
        if (result == 2) {

            idx = 0;
        } else if (result == 0 && steps[idx].can_go_back) {
            if (idx == 0) {
                if (yesno_dlg(L("Exit","Salir"),
                              L("Exit the installer?","¿Salir del instalador?")))
                    exit(0);
            } else {
                idx--;
            }
        } else {
            idx++;
        }
    }
    return 0;
}
