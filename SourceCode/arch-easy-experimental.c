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

#define VERSION   "V2.1.0"
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

static int yad_exec(char **argv, char *out, size_t outsz) {
    int pfd[2];
    if (pipe(pfd) != 0) { if (out && outsz) out[0] = '\0'; return -1; }

    pid_t pid = fork();
    if (pid == 0) {
        close(pfd[0]);
        dup2(pfd[1], STDOUT_FILENO);
        close(pfd[1]);
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) dup2(dn, STDERR_FILENO);
        execvp("yad", argv);
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
                 "--button=OK:0","--button=Cancel:1", YAD_W, NULL};
    return yad_exec(a, out, outsz) == 0;
}

static int passwordbox_dlg(const char *title, const char *text,
                            char *out, size_t outsz) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--entry","--hide-text","--title",(char*)title,
                 "--text",clean,
                 "--button=OK:0","--button=Cancel:1", YAD_WS, NULL};
    return yad_exec(a, out, outsz) == 0;
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
    char *p = raw;
    while (*p && count < maxout) {
        char *nl = strchr(p, '\n');
        int len = nl ? (int)(nl - p) : (int)strlen(p);
        if (len > 0 && len < 255) {
            strncpy(out[count], p, (size_t)len);
            out[count][len] = '\0';
            count++;
        }
        if (!nl) break;
        p = nl + 1;
    }
    if (count == 0 && raw[0]) {
        char copy[4096]; strncpy(copy, raw, sizeof(copy)-1);
        char *tok = strtok(copy, "|");
        while (tok && count < maxout) {
            trim_nl(tok);
            if (tok[0]) { strncpy(out[count++], tok, 255); }
            tok = strtok(NULL, "|");
        }
    }
    return count;
}

static void infobox_dlg(const char *title, const char *text) {
    char clean[2048]; dlg_strip(text, clean, sizeof(clean));
    char *a[] = {"yad","--info","--title",(char*)title,"--text",clean,
                 "--timeout=60","--no-buttons", YAD_WS, NULL};
    pid_t pid = fork();
    if (pid == 0) {
        int dn = open("/dev/null", O_RDWR);
        if (dn >= 0) { dup2(dn, STDOUT_FILENO); dup2(dn, STDERR_FILENO); }
        execvp("yad", a);
        _exit(0);
    }
}

typedef void (*LineCallback)(const char *line, void *ud);

static int run_simple(const char *cmd, int ignore_error) {
    write_log_fmt("$ %s", cmd);
    char full[MAX_CMD];
    snprintf(full,sizeof(full),"{ %s; } >/dev/null 2>&1",cmd);
    int rc = system(full);
    rc = (rc==-1)?-1:WEXITSTATUS(rc);
    if (rc!=0 && !ignore_error) write_log_fmt("ERROR (rc=%d): %s",rc,cmd);
    return rc;
}

static int run_stream(const char *cmd, LineCallback cb, void *ud, int ignore_error) {
    write_log_fmt("$ %s", cmd);
    char full[MAX_CMD];
    snprintf(full,sizeof(full),"{ %s; } 2>&1",cmd);
    FILE *fp = popen(full,"r");
    if (!fp) { write_log_fmt("ERROR: popen failed: %s",cmd); return -1; }
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
    if (rc!=0 && !ignore_error) write_log_fmt("ERROR (rc=%d): %s",rc,cmd);
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
        strncat(report, L("  ✗ Not running as root\n",
                          "  ✗ No se esta ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("  ✓ Running as root\n","  ✓ Ejecutando como root\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (!check_connectivity()) {
        strncat(report, L("  ✗ No internet connection\n",
                          "  ✗ Sin conexion a internet\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("  ✓ Internet connection OK\n","  ✓ Conexion a internet OK\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("which pacstrap >/dev/null 2>&1") != 0) {
        strncat(report, L("  ✗ pacstrap not found (are you in the Arch ISO?)\n",
                          "  ✗ pacstrap no encontrado (estas en la ISO de Arch?)\n"),
                sizeof(report)-strlen(report)-1);
        ok = 0;
    } else {
        strncat(report, L("  ✓ pacstrap found\n","  ✓ pacstrap encontrado\n"),
                sizeof(report)-strlen(report)-1);
    }

    if (system("mountpoint -q /mnt 2>/dev/null") == 0) {
        strncat(report, L("  ⚠ /mnt is already mounted (may conflict)\n",
                          "  ⚠ /mnt ya esta montado (puede haber conflicto)\n"),
                sizeof(report)-strlen(report)-1);
    } else {
        strncat(report, L("  ✓ /mnt is free\n","  ✓ /mnt libre\n"),
                sizeof(report)-strlen(report)-1);
    }

    st.laptop = is_laptop();
    if (st.laptop) {
        strncat(report, L("  ✓ Laptop detected (will install TLP power management)\n",
                          "  ✓ Laptop detectada (se instalara gestion de energia TLP)\n"),
                sizeof(report)-strlen(report)-1);
    }

    {
        struct statvfs vfs;
        if (statvfs("/", &vfs) == 0) {
            long long free_mb = ((long long)vfs.f_bavail * vfs.f_frsize) / (1024*1024);
            char line[128];
            snprintf(line,sizeof(line),
                     L("  ✓ Installer free space: %lld MB\n",
                       "  ✓ Espacio libre en instalador: %lld MB\n"), free_mb);
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
                 "No se encontraron interfaces inalambricas.\n\nVerifica que tu adaptador WiFi sea reconocido."));
        return -1;
    }
    const char *iface = ifaces[0];

    char info_msg[256];
    snprintf(info_msg,sizeof(info_msg),
             L("Scanning for networks on %s...","Buscando redes en %s..."), iface);
    infobox_dlg(L("Scanning...","Escaneando..."), info_msg);

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
                char *sig_p = pos;
                while (sig_p > scan_raw && *sig_p != '\n') sig_p--;
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

    char ssid_sel[128]={0};
    if (nnets > 0) {
        MenuItem items[16];
        for (int i=0; i<nnets; i++) {
            strncpy(items[i].tag, nets[i].ssid, 255);
            char bar[6]="-----";
            int pct = nets[i].signal;
            if (pct < 0) pct = 0;
            int filled = (pct * 5) / 100;
            for (int b=0; b<filled && b<5; b++) bar[b]='#';
            char sec[32]; strncpy(sec, nets[i].security[0]?nets[i].security:"?", 31);
            if (pct >= 0)
                snprintf(items[i].desc, 511, "[%s] %3d%%  %-6s  %s",
                         bar, pct, sec, nets[i].ssid);
            else
                snprintf(items[i].desc, 511, "[-----]  ?%%  %-6s  %s",
                         sec, nets[i].ssid);
        }
        char hdr[512];
        snprintf(hdr,sizeof(hdr),
                 L("Interface: %s\nSorted by signal strength. Select a network:",
                   "Interfaz: %s\nOrdenado por senal. Selecciona una red:"), iface);
        if (!radiolist_dlg(L("WiFi Networks","Redes WiFi"), hdr,
                           items, nnets, NULL, ssid_sel, sizeof(ssid_sel)))
            return -1;
    } else {
        char hdr[512];
        snprintf(hdr,sizeof(hdr),
                 L("Interface: %s\nNo networks found.\nEnter SSID or Cancel:",
                   "Interfaz: %s\nNo se hallaron redes.\nIngresa el SSID o Cancela:"), iface);
        if (!inputbox_dlg(L("WiFi - SSID","WiFi - SSID"),hdr,"",ssid_sel,sizeof(ssid_sel)))
            return -1;
    }
    if (!ssid_sel[0]) return -1;

    char pass[256]={0};
    char pass_hdr[512];
    snprintf(pass_hdr,sizeof(pass_hdr),
             L("Password for '%s'\n(leave blank for open, Cancel to go back):",
               "Contrasena de '%s'\n(vacio si es abierta, Cancelar para volver):"), ssid_sel);
    if (!passwordbox_dlg(L("WiFi Password","Contrasena WiFi"),pass_hdr,pass,sizeof(pass)))
        return -1;

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
                 L("Could not connect to '%s'.\n\nPossible causes:\n"
                   "  - Wrong password\n  - Network out of range\n  - DHCP not responding\n\n"
                   "Press OK to try again.",
                   "No se pudo conectar a '%s'.\n\nPosibles causas:\n"
                   "  - Contrasena incorrecta\n  - Red fuera de alcance\n  - DHCP sin respuesta\n\n"
                   "Presiona OK para intentar de nuevo."), ssid_sel);
        msgbox(L("WiFi Failed","WiFi fallido"),fail_msg);
        return 0;
    }
    return 1;
}

static void screen_network(void) {
    while (1) {
        MenuItem items[2];
        strncpy(items[0].tag,"wired",255);
        snprintf(items[0].desc,511,"%s",
            L("Wired (Ethernet)  - cable already plugged in",
              "Cable (Ethernet)  - cable ya conectado"));
        strncpy(items[1].tag,"wifi",255);
        snprintf(items[1].desc,511,"%s",
            L("WiFi              - connect to a wireless network",
              "WiFi              - conectar a una red inalambrica"));

        char choice[64]={0};
        if (!menu_dlg(L("Network Connection","Conexion de red"),
                      L("An active internet connection is required.\n\nHow are you connected?",
                        "Se necesita conexion a internet.\n\n Como estas conectado?"),
                      items,2,choice,sizeof(choice))) {
            if (yesno_dlg(L("Exit","Salir"),L("Exit the installer?","Salir del instalador?")))
                exit(0);
            continue;
        }

        if (!strcmp(choice,"wired")) {
            infobox_dlg(L("Checking...","Verificando..."),
                        L("Testing wired connection...","Probando conexion por cable..."));
            if (check_connectivity()) {
                msgbox(L("Connected!","Conectado!"),
                       L("Wired connection detected. Ready to continue.",
                         "Conexion por cable detectada. Listo para continuar."));
                return;
            }
            msgbox(L("No connection detected","Sin conexion detectada"),
                   L("Could not reach archlinux.org over wired.\n\n"
                     "Check: cable plugged in, router/switch on.\n\nPress OK to retry.",
                     "No se pudo alcanzar archlinux.org por cable.\n\n"
                     "Verifica: cable conectado, router encendido.\n\nOK para reintentar."));
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
    (void)ib;
    char q[MAX_CMD], full[MAX_CMD];
    shell_quote(cmd,q,sizeof(q));
    snprintf(full,sizeof(full),"arch-chroot /mnt /bin/bash -c %s",q);
    return run_stream(full, NULL, NULL, ignore_error);
}

static void ib_chroot_c(IB *ib, const char *cmd, const char *label) {
    int rc = ib_chroot(ib,cmd,0);
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
        int rc = run_stream("pacman -Sy --noconfirm",NULL,NULL,1);
        if (rc) run_stream("pacman -Sy --noconfirm",NULL,NULL,1);
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
    { char *sp = strchr(pdesktop,' '); if(sp) *sp='\0'; }
    strncpy(st.desktop, pdesktop, sizeof(st.desktop)-1);

    char disk[256];
    if (st.dualboot) {
        strncpy(disk, st.db_root, sizeof(disk)-1);
        char *end = disk + strlen(disk) - 1;
        if (strstr(disk,"nvme") || strstr(disk,"mmcblk")) {
            while (end > disk && isdigit((unsigned char)*end)) end--;
            if (*end == 'p') *end = '\0';
        } else {
            while (end > disk && isdigit((unsigned char)*end)) *end-- = '\0';
        }
    } else {
        snprintf(disk, sizeof(disk), "/dev/%s", st.disk);
    }

    char p1[256],p2[256],p3[256];
    if (!st.dualboot) partition_paths(disk,p1,p2,p3,sizeof(p1));

    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    int  uefi       = is_uefi();
    const char *bl  = st.bootloader;
    const char *fs  = st.filesystem;
    const char *root_dev = st.dualboot ? st.db_root : p3;

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
        ib_stage(ib, L("Formatting root partition (dual-boot)...",
                       "Formateando particion raiz (dual-boot)..."));
        ib_gradual(ib,10,15,0.04);

if (!strcmp(fs,"btrfs"))    ib_setup_btrfs(ib, st.db_root, disk);
        else if (!strcmp(fs,"xfs")) ib_setup_xfs(ib, st.db_root);
        else if (!strcmp(fs,"zfs")) ib_setup_zfs(ib, st.db_root);
        else {
            snprintf(cmd,sizeof(cmd),"mkfs.ext4 -F %s", st.db_root);
            ib_run(ib, cmd, "mkfs.ext4 dual-boot");
            run_simple("udevadm settle --timeout=10", 1);
            snprintf(cmd,sizeof(cmd),"mount %s /mnt", st.db_root);
            ib_run(ib, cmd, "mount root dual-boot");
        }

        if (st.db_swap[0]) {
            snprintf(cmd,sizeof(cmd),"swapon %s", st.db_swap);
            run_stream(cmd, NULL, NULL, 1);
            write_log_fmt("Swap enabled: %s", st.db_swap);
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
        char dlist[512]; strncpy(dlist, st.desktop_list, sizeof(dlist)-1);
        char *dtok = strtok(dlist, " ");
        int any_desktop = 0;
        int dm_enabled  = 0;
        double de_start = 77.0, de_end = 91.0;
        int de_count = 0;
        {
            char tmp2[512]; strncpy(tmp2, st.desktop_list, sizeof(tmp2)-1);
            char *t2 = strtok(tmp2, " ");
            while (t2) { if (strcmp(t2,"None")) de_count++; t2 = strtok(NULL," "); }
        }
        if (de_count == 0) de_count = 1;
        double de_step = (de_end - de_start) / de_count;
        int de_idx = 0;

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
            dtok = strtok(NULL, " ");
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
    strncpy(iss->reason, reason ? reason : "", sizeof(iss->reason) - 1);
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
        char *a[] = {
            "yad", "--progress",
            "--title",  TITLE "  " VERSION,
            "--text",   L("Installing Arch Linux - please wait...",
                          "Instalando Arch Linux - por favor espere..."),
            "--percentage", "0",
            "--width",  "620",
            "--auto-kill",
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
            "--width",  "620",
            "--auto-kill",
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

    if (g_prog_fd >= 0) { close(g_prog_fd); g_prog_fd = -1; }
    if (g_prog_pid > 0) { waitpid(g_prog_pid, NULL, 0); g_prog_pid = -1; }

    pthread_mutex_destroy(&ib->lock);
    free(ib);
    pthread_mutex_destroy(&iss.mu);
    pthread_cond_destroy(&iss.cv);

    if (!iss.success) {
        char msg[1536];
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
        char hn[64]={0};
        if (!inputbox_dlg(L("System Identity","Identidad del sistema"),
                          L("Enter hostname (letters, digits, -, _ - max 32):",
                            "Ingresa el nombre del equipo (letras, digitos, -, _ - max 32):"),
                          st.hostname, hn, sizeof(hn))) return 0;
        if (!validate_name(hn)) {
            msgbox(L("Invalid hostname","Hostname invalido"),
                   L("Only letters, digits, hyphens and underscores. Max 32 chars. Must start with a letter.",
                     "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres. Debe empezar con una letra."));
            continue;
        }
        char un[64]={0};
        if (!inputbox_dlg(L("System Identity","Identidad del sistema"),
                          L("Enter username (letters, digits, -, _ - max 32):",
                            "Ingresa el nombre de usuario (letras, digitos, -, _ - max 32):"),
                          st.username, un, sizeof(un))) return 0;
        if (!validate_name(un)) {
            msgbox(L("Invalid username","Usuario invalido"),
                   L("Only letters, digits, hyphens and underscores. Max 32 chars. Must start with a letter.",
                     "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres. Debe empezar con una letra."));
            continue;
        }
        strncpy(st.hostname,hn,sizeof(st.hostname)-1);
        strncpy(st.username,un,sizeof(st.username)-1);
        return 1;
    }
}

static int screen_passwords(void) {
    while(1) {
        char rp1[256]={0},rp2[256]={0};
        if (!passwordbox_dlg(L("Passwords","Contrasenas"),
                             L("Enter ROOT password:","Ingresa la contrasena de ROOT:"),
                             rp1,sizeof(rp1))) return 0;
        if (!passwordbox_dlg(L("Passwords","Contrasenas"),
                             L("Confirm ROOT password:","Confirma la contrasena de ROOT:"),
                             rp2,sizeof(rp2))) return 0;
        if (!rp1[0]) { msgbox(L("Error","Error"),L("Root password cannot be empty.",
                                                    "La contrasena root no puede estar vacia.")); continue; }
        if (strcmp(rp1,rp2)) { msgbox(L("Error","Error"),L("Root passwords do not match.",
                                                            "Las contrasenas root no coinciden.")); continue; }
        char up1[256]={0},up2[256]={0};
        if (!passwordbox_dlg(L("Passwords","Contrasenas"),
                             L("Enter USER password:","Ingresa la contrasena de USUARIO:"),
                             up1,sizeof(up1))) return 0;
        if (!passwordbox_dlg(L("Passwords","Contrasenas"),
                             L("Confirm USER password:","Confirma la contrasena de USUARIO:"),
                             up2,sizeof(up2))) return 0;
        if (!up1[0]) { msgbox(L("Error","Error"),L("User password cannot be empty.",
                                                    "La contrasena de usuario no puede estar vacia.")); continue; }
        if (strcmp(up1,up2)) { msgbox(L("Error","Error"),L("User passwords do not match.",
                                                            "Las contrasenas de usuario no coinciden.")); continue; }
        strncpy(st.root_pass,rp1,sizeof(st.root_pass)-1);
        strncpy(st.user_pass,up1,sizeof(st.user_pass)-1);
        return 1;
    }
}

static int screen_disk(void) {
    {
        MenuItem mode_items[2];
        strncpy(mode_items[0].tag,"full",255);
        snprintf(mode_items[0].desc,511,"%s",
            L("Full Install   - erase entire disk (recommended for new installs)",
              "Instalacion completa - borrar todo el disco (recomendado para instalaciones nuevas)"));
        strncpy(mode_items[1].tag,"dual",255);
        snprintf(mode_items[1].desc,511,"%s",
            L("Dual Boot      - install alongside existing OS (Windows / Linux)",
              "Dual Boot      - instalar junto a un OS existente (Windows / Linux)"));
        char mode_out[16]={0};
        if (!radiolist_dlg(
                L("Install Type","Tipo de instalacion"),
                L("How do you want to install Arch Linux?\n\n"
                  "DUAL BOOT: You will manually select which partition to use.\n"
                  "           Your existing OS and other partitions are kept.\n\n"
                  "FULL INSTALL: The selected disk is completely erased.",
                  "Como quieres instalar Arch Linux?\n\n"
                  "DUAL BOOT: Seleccionaras manualmente la particion a usar.\n"
                  "           Tu OS existente y otras particiones se conservan.\n\n"
                  "INSTALACION COMPLETA: El disco seleccionado se borra completamente."),
                mode_items, 2,
                st.dualboot ? "dual" : "full",
                mode_out, sizeof(mode_out)))
            return 0;
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
                     L("Current disk layout (lsblk -f):\n\n%s\nWARNING: Selected disk will be ERASED.",
                       "Layout actual de discos (lsblk -f):\n\n%s\nADVERTENCIA: El disco seleccionado se BORRARA."),
                     lsblk);
            msgbox(L("Disk Overview - Read before selecting!","Vista de discos - Lee antes de elegir!"),txt);
        }

        MenuItem items[32];
        char cur_dev[128]="";
        if (st.disk[0]) snprintf(cur_dev,sizeof(cur_dev),"/dev/%s",st.disk);
        for (int i=0;i<nd;i++) {
            snprintf(items[i].tag,256,"/dev/%s",disks[i].name);
            snprintf(items[i].desc,512,"%lld GB  -  %s",disks[i].size_gb,disks[i].model);
        }

        char sel[128]={0};
        if (!radiolist_dlg(
                L("Disk Selection","Seleccion de disco"),
                L("WARNING: ALL DATA on the selected disk will be ERASED!\n\nSelect the installation disk:",
                  "ADVERTENCIA: Se borraran todos los datos del disco seleccionado!\n\nSelecciona el disco:"),
                items,nd, cur_dev[0]?cur_dev:items[0].tag, sel, sizeof(sel))) return 0;

        char confirm_msg[512];
        snprintf(confirm_msg,sizeof(confirm_msg),
                 L("You selected: %s\n\nALL data will be permanently destroyed.\n\nAre you absolutely sure?",
                   "Seleccionaste: %s\n\nTODOS los datos se destruiran permanentemente.\n\nEstas completamente seguro?"),
                 sel);
        if (!yesno_dlg(L("Confirm Disk Erase","Confirmar borrado de disco"),confirm_msg)) return 0;

        const char *dname = sel;
        if (!strncmp(dname,"/dev/",5)) dname+=5;
        strncpy(st.disk,dname,sizeof(st.disk)-1);

        char sug[8]; snprintf(sug,sizeof(sug),"%d",suggest_swap_gb());
        while(1) {
            char swap_hdr[512];
            snprintf(swap_hdr,sizeof(swap_hdr),
                     L("Suggested swap: %s GB\n\nEnter swap size in GB (1-128):",
                       "Swap sugerido: %s GB\n\nIngresa el tamano del swap en GB (1-128):"),
                     sug);
            char sw[16]={0};
            if (!inputbox_dlg(L("Swap Size","Tamano de Swap"),swap_hdr,
                              st.swap[0]?st.swap:sug, sw, sizeof(sw))) return 0;
            trim_nl(sw);
            if (validate_swap(sw)) { strncpy(st.swap,sw,sizeof(st.swap)-1); return 1; }
            msgbox(L("Invalid swap","Swap invalido"),
                   L("Swap must be a number between 1 and 128.",
                     "El swap debe ser un numero entre 1 y 128."));
        }

    } else {
        PartEntry parts[64];
        int np = list_all_partitions(parts, 64);
        if (np == 0) {
            msgbox(L("No partitions found","Sin particiones"),
                   L("No existing partitions were found.\n\nUse full-install mode or create partitions with a tool like GParted first.",
                     "No se encontraron particiones existentes.\n\nUsa el modo de instalacion completa o crea particiones con GParted primero."));
            st.dualboot = 0;
            return 0;
        }

        {
            char lsblk[4096]={0};
            FILE *fp = popen("lsblk 2>/dev/null | head -50","r");
            if (fp) { (void)fread(lsblk,1,sizeof(lsblk)-1,fp); pclose(fp); }
            char txt[MAX_OUT];
            snprintf(txt,sizeof(txt),
                     L("Current partition layout:\n\n%s\n\n"
                       "Select the partition where Arch Linux will be installed.\n"
                       "WARNING: that partition WILL BE FORMATTED.",
                       "Esquema de particiones actual:\n\n%s\n\n"
                       "Selecciona la particion donde se instalara Arch Linux.\n"
                       "ADVERTENCIA: esa particion sera FORMATEADA."),
                     lsblk);
            msgbox(L("Dual Boot - Partition Overview","Dual Boot - Vista de particiones"), txt);
        }

        MenuItem *part_items = malloc(np * sizeof(MenuItem));
        for (int i = 0; i < np; i++) {
            strncpy(part_items[i].tag, parts[i].path, 255);
            snprintf(part_items[i].desc, 511, "%lld MB  [%s]  %s",
                     parts[i].size_mb, parts[i].fstype,
                     parts[i].label[0] ? parts[i].label : "");
        }
        char sel_root[128]={0};
        int ok = radiolist_dlg(
            L("Dual Boot - Root Partition","Dual Boot - Particion raiz"),
            L("Select the partition for Arch Linux root (/).\n"
              "This partition WILL BE ERASED AND FORMATTED.\n\n"
              "Choose a free/empty partition, NOT your Windows partition!",
              "Selecciona la particion para la raiz (/) de Arch Linux.\n"
              "Esta particion SE BORRARA Y FORMATEARA.\n\n"
              "Elige una particion libre/vacia, NO la particion de Windows!"),
            part_items, np,
            st.db_root[0] ? st.db_root : part_items[0].tag,
            sel_root, sizeof(sel_root));
        free(part_items);
        if (!ok || !sel_root[0]) return 0;

        char conf_msg[256];
        snprintf(conf_msg, sizeof(conf_msg),
                 L("CONFIRM: %s will be ERASED and formatted.\n\nProceed?",
                   "CONFIRMAR: %s sera BORRADA y formateada.\n\nProceder?"),
                 sel_root);
        if (!yesno_dlg(L("Confirm Format","Confirmar formateo"), conf_msg)) return 0;
        strncpy(st.db_root, sel_root, sizeof(st.db_root)-1);

        if (is_uefi()) {
            PartEntry parts2[64]; int np2 = list_all_partitions(parts2, 64);
            MenuItem *efi_items = malloc((np2+1) * sizeof(MenuItem));
            strncpy(efi_items[0].tag, "none", 255);
            snprintf(efi_items[0].desc, 511, "%s",
                     L("None / Auto-detect","Ninguna / Autodetectar"));
            int ni2 = 1;
            for (int i = 0; i < np2; i++) {
                if (!strcmp(parts2[i].path, st.db_root)) continue;
                strncpy(efi_items[ni2].tag, parts2[i].path, 255);
                snprintf(efi_items[ni2].desc, 511, "%lld MB  [%s]  %s",
                         parts2[i].size_mb, parts2[i].fstype,
                         parts2[i].label[0] ? parts2[i].label : "");
                ni2++;
            }
            char sel_efi[128]={0};
            radiolist_dlg(
                L("Dual Boot - EFI Partition","Dual Boot - Particion EFI"),
                L("Select the existing EFI System Partition (ESP).\n"
                  "This is usually a small FAT32 partition (~100-500 MB).\n"
                  "It will NOT be formatted - existing boot entries are preserved.",
                  "Selecciona la particion EFI del sistema (ESP) existente.\n"
                  "Suele ser una particion FAT32 pequena (~100-500 MB).\n"
                  "NO se formateara - las entradas de arranque existentes se conservan."),
                efi_items, ni2,
                st.db_efi[0] ? st.db_efi : "none",
                sel_efi, sizeof(sel_efi));
            free(efi_items);
            if (strcmp(sel_efi,"none") != 0)
                strncpy(st.db_efi, sel_efi, sizeof(st.db_efi)-1);
            else {
                FILE *fp2 = popen(
                    "lsblk -b -p -n -o PATH,FSTYPE,PARTTYPE --pairs 2>/dev/null | "
                    "grep -i 'PARTTYPE=\"c12a7328' | head -1", "r");
                if (fp2) {
                    char line[256]={0}; (void)fgets(line,sizeof(line),fp2); pclose(fp2);
                    char *pp = strstr(line,"PATH=\"");
                    if (pp) { sscanf(pp+6,"%127[^\"]",st.db_efi); }
                }
            }
        }

        {
            PartEntry parts3[64]; int np3 = list_all_partitions(parts3, 64);
            MenuItem *sw_items = malloc((np3+1) * sizeof(MenuItem));
            strncpy(sw_items[0].tag, "none", 255);
            snprintf(sw_items[0].desc, 511, "%s", L("None - no swap partition","Ninguno - sin swap"));
            int ni3 = 1;
            for (int i = 0; i < np3; i++) {
                if (!strcmp(parts3[i].path, st.db_root)) continue;
                if (!strcmp(parts3[i].path, st.db_efi))  continue;
                strncpy(sw_items[ni3].tag, parts3[i].path, 255);
                snprintf(sw_items[ni3].desc, 511, "%lld MB  [%s]  %s",
                         parts3[i].size_mb, parts3[i].fstype,
                         parts3[i].label[0] ? parts3[i].label : "");
                ni3++;
            }
            char sel_swap[128]={0};
            radiolist_dlg(
                L("Dual Boot - Swap Partition","Dual Boot - Particion swap"),
                L("Select an existing swap partition (optional).\n"
                  "Choose 'None' to skip swap.",
                  "Selecciona una particion swap existente (opcional).\n"
                  "Elige 'Ninguno' para omitir el swap."),
                sw_items, ni3,
                st.db_swap[0] ? st.db_swap : "none",
                sel_swap, sizeof(sel_swap));
            free(sw_items);
            if (strcmp(sel_swap,"none") != 0)
                strncpy(st.db_swap, sel_swap, sizeof(st.db_swap)-1);
            else
                st.db_swap[0] = '\0';
        }

        return 1;
    }
    return 1;
}

static int screen_filesystem(void) {
    MenuItem items[4];
    strncpy(items[0].tag,"ext4",255);
    snprintf(items[0].desc,511,"%s",L(
        "ext4   - stable, widely supported, proven",
        "ext4   - estable, amplio soporte, probado"));
    strncpy(items[1].tag,"btrfs",255);
    snprintf(items[1].desc,511,"%s",L(
        "btrfs  - subvolumes + zstd compression + snapshots",
        "btrfs  - subvolumenes + compresion zstd + snapshots"));
    strncpy(items[2].tag,"xfs",255);
    snprintf(items[2].desc,511,"%s",L(
        "xfs    - high-performance, great for large files",
        "xfs    - alto rendimiento, ideal para archivos grandes"));
    strncpy(items[3].tag,"zfs",255);
    snprintf(items[3].desc,511,"%s",L(
        "zfs    - [EXPERIMENTAL] copy-on-write, checksums, snapshots (archzfs repo)",
        "zfs    - [EXPERIMENTAL] copy-on-write, checksums, snapshots (repo archzfs)"));

    char out[16]={0};
    if (!radiolist_dlg(L("Filesystem","Sistema de archivos"),
                       L("Choose the root filesystem:\n\n"
                         "  ext4   - best general-purpose choice.\n"
                         "  btrfs  - modern features, recommended with snapper.\n"
                         "  xfs    - excellent for large files and servers.\n"
                         "  zfs    - advanced but requires archzfs repo.",
                         "Elige el sistema de archivos raiz:\n\n"
                         "  ext4   - mejor opcion general.\n"
                         "  btrfs  - funciones modernas, recomendado con snapper.\n"
                         "  xfs    - excelente para archivos grandes y servidores.\n"
                         "  zfs    - avanzado, requiere repo archzfs."),
                       items, 4, st.filesystem, out, sizeof(out))) return 0;

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
                 "  - Requiere el repositorio archzfs (se agrega automaticamente).\n"
                 "  - Kernel forzado a 'linux' (modulos archzfs son version-especificos).\n"
                 "  - Se usara GRUB como bootloader (forzado).\n"
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
    MenuItem items[5];
    strncpy(items[0].tag,"linux",255);
    snprintf(items[0].desc,511,"%s",L(
        "linux         - latest stable kernel",
        "linux         - kernel estable mas reciente"));
    strncpy(items[1].tag,"linux-lts",255);
    snprintf(items[1].desc,511,"%s",L(
        "linux-lts     - long-term support kernel",
        "linux-lts     - kernel de soporte a largo plazo"));
    strncpy(items[2].tag,"linux-zen",255);
    snprintf(items[2].desc,511,"%s",L(
        "linux-zen     - optimized for desktop / gaming",
        "linux-zen     - optimizado para escritorio / gaming"));
    strncpy(items[3].tag,"linux-hardened",255);
    snprintf(items[3].desc,511,"%s",L(
        "linux-hardened- security-hardened kernel",
        "linux-hardened- kernel endurecido para seguridad"));
    strncpy(items[4].tag,"linux-cachyos",255);
    snprintf(items[4].desc,511,"%s",L(
        "linux-cachyos - CachyOS kernel (max speed, needs cachyos repo)",
        "linux-cachyos - kernel CachyOS (maxima velocidad, requiere repo cachyos)"));

    char kl_copy[512]; strncpy(kl_copy, st.kernel_list, sizeof(kl_copy)-1);
    const char *defs[8]={0}; int ndefs=0;
    char *tok = strtok(kl_copy, " ");
    while (tok && ndefs < 8) { defs[ndefs++] = tok; tok = strtok(NULL," "); }

    char sel[8][256]; int nsel = -1;
    while (nsel < 1) {
        nsel = checklist_dlg(
            L("Kernel Selection","Seleccion de Kernel"),
            L("Select one or more kernels to install.\n"
              "Use SPACE to toggle. The first selected kernel is used for bootloader config.\n"
              "Having linux + linux-lts gives you a fallback option.",
              "Selecciona uno o mas kernels para instalar.\n"
              "Usa ESPACIO para activar/desactivar. El primer kernel seleccionado se usa en el bootloader.\n"
              "Tener linux + linux-lts te da una opcion de respaldo."),
            items, 5, defs, ndefs, sel, 8);
        if (nsel < 0) return 0;       
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
    MenuItem items[3]; int ni;
    if (!uefi) {
        strncpy(items[0].tag,"grub",255);
        snprintf(items[0].desc,511,"%s",L("GRUB         - stable, recommended for BIOS","GRUB         - estable, recomendado para BIOS"));
        strncpy(items[1].tag,"limine",255);
        snprintf(items[1].desc,511,"%s",L("Limine       - modern, lightweight, BIOS + UEFI","Limine       - moderno, ligero, BIOS + UEFI"));
        ni=2;
    } else {
        strncpy(items[0].tag,"grub",255);
        snprintf(items[0].desc,511,"%s",L("GRUB         - stable, UEFI and BIOS","GRUB         - estable, UEFI y BIOS"));
        strncpy(items[1].tag,"systemd-boot",255);
        snprintf(items[1].desc,511,"%s",L("systemd-boot - fast, UEFI only","systemd-boot - rapido, solo UEFI"));
        strncpy(items[2].tag,"limine",255);
        snprintf(items[2].desc,511,"%s",L("Limine       - modern, lightweight, UEFI only","Limine       - moderno, ligero, solo UEFI"));
        ni=3;
    }
    char out[16]={0};
    if (!radiolist_dlg(L("Bootloader","Gestor de arranque"),
                       L("Choose a bootloader:\n\n"
                         "  GRUB         works on UEFI and BIOS legacy.\n"
                         "  systemd-boot UEFI only, minimal and fast.\n"
                         "  Limine       modern, lightweight, simple config.",
                         "Elige un gestor de arranque:\n\n"
                         "  GRUB         UEFI y BIOS.\n"
                         "  systemd-boot Solo UEFI, minimalista y rapido.\n"
                         "  Limine       Moderno, ligero, config sencilla."),
                       items,ni,st.bootloader,out,sizeof(out))) return 0;
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
    char *tok = strtok(dl_copy," ");
    while (tok && ndefs < 8) { defs[ndefs++] = tok; tok = strtok(NULL," "); }

    char sel[8][256]; int nsel = -1;
    while (nsel < 1) {
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
        }
    }
    free(items);

    char cleaned[8][256]; int nc=0;
    int has_non_none = 0;
    for(int i=0;i<nsel;i++) if(strcmp(sel[i],"None")) has_non_none=1;
    for(int i=0;i<nsel;i++) {
        if(has_non_none && !strcmp(sel[i],"None")) continue;
        strncpy(cleaned[nc++], sel[i], 255);
    }
    if (nc == 0) { strncpy(cleaned[0],"None",255); nc=1; }

    st.desktop_list[0] = '\0';
    for (int i=0;i<nc;i++) {
        if(i) strncat(st.desktop_list," ",sizeof(st.desktop_list)-strlen(st.desktop_list)-1);
        strncat(st.desktop_list, cleaned[i], sizeof(st.desktop_list)-strlen(st.desktop_list)-1);
    }
    strncpy(st.desktop, cleaned[0], sizeof(st.desktop)-1);
    return 1;
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
    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L("Yes - install yay after base setup","Si - instalar yay al finalizar"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L("No  - skip","No  - omitir"));
    char out[8]={0};
    if(!radiolist_dlg(L("AUR Helper","AUR Helper"),
                      L("Install yay? (AUR helper, lets you install packages from the AUR)",
                        "Instalar yay? (AUR helper, permite instalar paquetes del AUR)"),
                      items,2,st.yay?"yes":"no",out,sizeof(out))) return 0;
    st.yay=!strcmp(out,"yes");
    return 1;
}

static int screen_snapper(void) {
    if(strcmp(st.filesystem,"btrfs")) { st.snapper=0; return 1; }
    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L("Yes - automatic snapshots on every pacman transaction",
                                       "Si - snapshots automaticos en cada transaccion pacman"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L("No  - skip","No  - omitir"));
    char out[8]={0};
    if(!radiolist_dlg(L("BTRFS Snapshots","Snapshots BTRFS"),
                      L("Install snapper + grub-btrfs for automatic rollback snapshots?\n"
                        "(Requires BTRFS filesystem - already selected)",
                        "Instalar snapper + grub-btrfs para snapshots y rollback automatico?\n"
                        "(Requiere BTRFS - ya seleccionado)"),
                      items,2,st.snapper?"yes":"no",out,sizeof(out))) return 0;
    st.snapper=!strcmp(out,"yes");
    return 1;
}

static int screen_flatpak(void) {
    if(!strcmp(st.desktop,"None")) { st.flatpak=0; return 1; }
    MenuItem items[2];
    strncpy(items[0].tag,"yes",255);
    snprintf(items[0].desc,511,"%s",L("Yes - install Flatpak + add Flathub","Si - instalar Flatpak + anadir Flathub"));
    strncpy(items[1].tag,"no",255);
    snprintf(items[1].desc,511,"%s",L("No  - skip","No  - omitir"));
    char out[8]={0};
    if(!radiolist_dlg(L("Flatpak","Flatpak"),
                      L("Install Flatpak and add the Flathub repository?",
                        "Instalar Flatpak y anadir el repositorio Flathub?"),
                      items,2,st.flatpak?"yes":"no",out,sizeof(out))) return 0;
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
        ROW("Root part",  st.db_root[0]?st.db_root:"NOT SET");
        if (is_uefi()) ROW("EFI part", st.db_efi[0]?st.db_efi:"auto-detect");
        ROW("Swap part",  st.db_swap[0]?st.db_swap:"none");
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
    if (st.dualboot && !st.db_root[0])
        strncat(missing,"root partition, ",sizeof(missing)-strlen(missing)-1);
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
        snprintf(confirm_msg,sizeof(confirm_msg),"%s%s%s%s",
                 text,
                 L("\n\nWARNING: THIS WILL FORMAT ","\n\nADVERTENCIA: SE FORMATEARA "),
                 st.db_root,
                 L(".\n\nProceed?",".\n\nProceder?"));
    } else {
        snprintf(confirm_msg,sizeof(confirm_msg),"%s%s%s%s",
                 text,
                 L("\n\nWARNING: THIS WILL ERASE /dev/","\n\nADVERTENCIA: SE BORRARA /dev/"),
                 st.disk,
                 L(".\n\nProceed?",".\n\nProceder?"));
    }
    return yesno_dlg(L("Review & Confirm","Revisar y confirmar"),confirm_msg);
}


static void screen_finish(void) {
    if (yesno_dlg(L("Reboot?","¿Reiniciar?"),
                  L("Arch Linux has been installed!\n\n"
                    "Remove the installation media.\n\n"
                    "Reboot now?",
                    "¡Arch Linux se ha instalado!\n\n"
                    "Extrae el medio de instalaci\xc3\xb3n.\n\n"
                    "¿Reiniciar ahora?"))) {
        infobox_dlg(L("Rebooting","Reiniciando"),
                    L("Unmounting filesystems and rebooting...",
                      "Desmontando sistemas de archivos y reiniciando..."));
        (void)system("umount -R /mnt 2>/dev/null");
        (void)system("reboot");
    }
    exit(0);
}

typedef struct {
    const char *name;
    int        (*fn)(void);
    int         can_go_back;
} Step;

static int screen_welcome_wrap(void)  { screen_welcome();  return 1; }
static int screen_language_wrap(void)  { screen_language(); return 1; }
static int screen_network_wrap(void)   { screen_network();  return 1; }
static int screen_finish_wrap(void)    { screen_finish();   return 1; }
static int screen_install_wrap(void)   { return screen_install(); }
static int screen_preflight_wrap(void) { return run_preflight(); }



static void ensure_x11_deps(void) {
    static const char *deps[] = {
        "xorg-server",
        "xorg-xinit",
        "xf86-video-fbdev",
        "xf86-video-vesa",
        "xdotool",
        "yad",
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
        printf("[*] Installing X11 + yad dependencies via pacman...\n");
        fflush(stdout);
        if (system("pacman -Sy --noconfirm "
                   "xorg-server xorg-xinit "
                   "xf86-video-fbdev xf86-video-vesa "
                   "xdotool yad") != 0) {
            fprintf(stderr,
                "[!] WARNING: Some X11 deps failed to install.\n"
                "    The installer will try to continue anyway.\n");
        } else {
            printf("[+] X11 dependencies installed.\n");
        }
        fflush(stdout);
    }
}

static void ensure_display(void) {
    if (getenv("DISPLAY") != NULL) return;  

    ensure_x11_deps();

    
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
    fprintf(f, "xrandr --auto 2>/dev/null || true\n");
    fprintf(f, "xset r rate 300 30 2>/dev/null || true\n");
    fprintf(f, "xinput create-master \"VirtualPointer\" 2>/dev/null || true\n");
    fprintf(f, "sleep 0.3\n");
    fprintf(f, "xdotool mousemove 640 400 2>/dev/null || true\n");
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
        if (result == 0 && steps[idx].can_go_back) {
            if (idx == 0) {
                if (yesno_dlg(L("Exit","Salir"),
                              L("Exit the installer?","Salir del instalador?")))
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
