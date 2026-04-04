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

#define VERSION   "V1.1.4"
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
    char gpu[32];
    char keymap[32];
    char timezone[128];
    char filesystem[8];
    char kernel[32];
    char bootloader[16];
    int  mirrors;
    int  quick;
    int  yay;
    int  snapper;
    int  flatpak;
} State;

static State st = {
    .lang       = "en",
    .locale     = "en_US.UTF-8",
    .hostname   = "",
    .username   = "",
    .root_pass  = "",
    .user_pass  = "",
    .swap       = "8",
    .disk       = "",
    .desktop    = "None",
    .gpu        = "None",
    .keymap     = "us",
    .timezone   = "UTC",
    .filesystem = "ext4",
    .kernel     = "linux",
    .bootloader = "grub",
    .mirrors    = 1,
    .quick      = 0,
    .yay        = 0,
    .snapper    = 0,
    .flatpak    = 0,
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
        "hyprland waybar wofi alacritty xdg-desktop-portal-hyprland "
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
            } else if (*src == ']') {  /* OSC: ends at BEL or ESC \ */
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
    for (const char *p = s; *p; p++)
        if (!isalnum((unsigned char)*p) && *p != '_' && *p != '-') return 0;
    return 1;
}

static int validate_swap(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; p++) if (!isdigit((unsigned char)*p)) return 0;
    int v = atoi(s);
    return v >= 1 && v <= 128;
}

typedef struct { char **v; int n; int cap; } DA;

static DA da_new(void) {
    DA da; da.cap=32; da.n=0;
    da.v = malloc(32*sizeof(char*));
    da.v[0] = NULL;
    return da;
}
static void da_push(DA *da, const char *s) {
    if (da->n+2 > da->cap) {
        da->cap *= 2;
        da->v = realloc(da->v, da->cap*sizeof(char*));
    }
    da->v[da->n++] = strdup(s);
    da->v[da->n]   = NULL;
}
static void da_free(DA *da) {
    for (int i=0;i<da->n;i++) free(da->v[i]);
    free(da->v); da->v=NULL; da->n=0;
}

static int da_exec(DA *da, char *out, size_t outsz) {
    char tmp[64] = "/tmp/.dlgXXXXXX";
    int tfd = mkstemp(tmp);
    if (tfd < 0) { if(out&&outsz) out[0]='\0'; return -1; }

    tcflush(STDIN_FILENO, TCIFLUSH);

    pid_t pid = fork();
    if (pid == 0) {
        dup2(tfd, STDERR_FILENO);
        close(tfd);
        execvp("dialog", da->v);
        _exit(127);
    }
    close(tfd);

    int status; waitpid(pid, &status, 0);

    if (out && outsz>0) {
        FILE *f = fopen(tmp,"r");
        if (f) {
            size_t n = fread(out, 1, outsz-1, f);
            out[n]='\0'; fclose(f);
            trim_nl(out);
        } else out[0]='\0';
    }
    unlink(tmp);
    return WIFEXITED(status) ? WEXITSTATUS(status) : 1;
}

static void da_hdr(DA *da, const char *title) {
    char bt[256], tt[256];
    snprintf(bt, sizeof(bt), "\\Zb\\Z4%s\\Zn  -  %s", TITLE, VERSION);
    snprintf(tt, sizeof(tt), " %s ", title);
    da_push(da,"dialog"); da_push(da,"--colors");
    da_push(da,"--backtitle"); da_push(da,bt);
    da_push(da,"--title");    da_push(da,tt);
}


static void msgbox(const char *title, const char *text) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--msgbox"); da_push(&da,text);
    da_push(&da,"0"); da_push(&da,"0");
    da_exec(&da,NULL,0); da_free(&da);
}

static int yesno_dlg(const char *title, const char *text) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--yesno"); da_push(&da,text);
    da_push(&da,"0"); da_push(&da,"0");
    int rc = da_exec(&da,NULL,0); da_free(&da);
    return rc == 0;
}

static int inputbox_dlg(const char *title, const char *text,
                         const char *init, char *out, size_t outsz) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--inputbox"); da_push(&da,text);
    da_push(&da,"0"); da_push(&da,"60");
    if (init && *init) da_push(&da,init);
    else da_push(&da,"");
    int rc = da_exec(&da,out,outsz); da_free(&da);
    return rc == 0;
}

static int passwordbox_dlg(const char *title, const char *text,
                            char *out, size_t outsz) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--insecure"); da_push(&da,"--passwordbox");
    da_push(&da,text); da_push(&da,"0"); da_push(&da,"60");
    int rc = da_exec(&da,out,outsz); da_free(&da);
    return rc == 0;
}

typedef struct { char tag[256]; char desc[512]; } MenuItem;

static int menu_dlg(const char *title, const char *text,
                     MenuItem *items, int n, char *out, size_t outsz) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--menu"); da_push(&da,text);
    char h[8]; snprintf(h,sizeof(h),"%d", n>28?40:n+12);
    char ns[8]; snprintf(ns,sizeof(ns),"%d",n);
    da_push(&da,h); da_push(&da,"76"); da_push(&da,ns);
    for (int i=0;i<n;i++) { da_push(&da,items[i].tag); da_push(&da,items[i].desc); }
    int rc = da_exec(&da,out,outsz); da_free(&da);
    return rc == 0;
}

static int radiolist_dlg(const char *title, const char *text,
                           MenuItem *items, int n, const char *def,
                           char *out, size_t outsz) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--radiolist"); da_push(&da,text);
    char h[8]; snprintf(h,sizeof(h),"%d", n>28?40:n+12);
    char ns[8]; snprintf(ns,sizeof(ns),"%d",n);
    da_push(&da,h); da_push(&da,"76"); da_push(&da,ns);
    for (int i=0;i<n;i++) {
        da_push(&da,items[i].tag); da_push(&da,items[i].desc);
        da_push(&da,(def && strcmp(items[i].tag,def)==0)?"on":"off");
    }
    int rc = da_exec(&da,out,outsz); da_free(&da);
    return rc == 0;
}

static void infobox_dlg(const char *title, const char *text) {
    DA da = da_new();
    da_hdr(&da,title);
    da_push(&da,"--infobox"); da_push(&da,text);
    da_push(&da,"5"); da_push(&da,"50");
    da_exec(&da,NULL,0); da_free(&da);
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
    char c[4]={0}; fread(c,1,3,f); fclose(f);
    return c[0]=='0';
}

static void detect_gpu(char *out, size_t sz) {
    FILE *fp = popen("lspci 2>/dev/null | grep -iE 'vga|3d|display'","r");
    char buf[4096]={0};
    if (fp) { fread(buf,1,sizeof(buf)-1,fp); pclose(fp); }
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
    system(scan_cmd);

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

    char ssid_sel[128]={0};
    if (nssids>0) {
        MenuItem items[16]; int ni2=nssids>15?15:nssids;
        for (int i=0;i<ni2;i++) {
            strncpy(items[i].tag,ssids[i],255);
            strncpy(items[i].desc,ssids[i],511);
        }
        char hdr[512];
        snprintf(hdr,sizeof(hdr),
                 L("Interface: %s\nSelect a network (Cancel = go back):",
                   "Interfaz: %s\nSelecciona una red (Cancelar = volver):"), iface);
        if (!radiolist_dlg(L("WiFi Networks","Redes WiFi"), hdr,
                           items,ni2,NULL,ssid_sel,sizeof(ssid_sel)))
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
    system(cmd);
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
            system(cmd); sleep(3);
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
    snprintf(cmd,sizeof(cmd),"mkfs.btrfs -f %s",p3); ib_run(ib,cmd,"mkfs.btrfs");
    snprintf(cmd,sizeof(cmd),"mount %s /mnt",p3);     ib_run(ib,cmd,"mount btrfs");
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
    if (fp) { fgets(partuuid,sizeof(partuuid),fp); pclose(fp); trim_nl(partuuid); }

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
    f = fopen("/mnt/boot/loader/entries/arch.conf","w");
    if (f) {
        fprintf(f,"title   Arch Linux\n");
        fprintf(f,"linux   /vmlinuz-%s\n",st.kernel);
        if (microcode[0]) fprintf(f,"initrd  /%s.img\n",microcode);
        fprintf(f,"initrd  /initramfs-%s.img\n",st.kernel);
        fprintf(f,"options %s rw quiet %s\n",root_opt,extra);
        fclose(f);
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
    if (fp) { fgets(partuuid,sizeof(partuuid),fp); pclose(fp); trim_nl(partuuid); }

    char root_opt[256];
    if (partuuid[0]) snprintf(root_opt,sizeof(root_opt),"root=PARTUUID=%s",partuuid);
    else             snprintf(root_opt,sizeof(root_opt),"root=%s",root_dev);

    char extra[64]={0};
    if (!strcmp(st.filesystem,"btrfs")) strcpy(extra,"rootflags=subvol=@ ");

    char kpath[512], ipath[512], ucode_line[512]={0};
    if (partuuid[0]) {
        snprintf(kpath,sizeof(kpath),"guid(%s):/boot/vmlinuz-%s",partuuid,st.kernel);
        snprintf(ipath,sizeof(ipath),"guid(%s):/boot/initramfs-%s.img",partuuid,st.kernel);
        if (microcode[0])
            snprintf(ucode_line,sizeof(ucode_line),
                     "    module_path: guid(%s):/boot/%s.img\n",partuuid,microcode);
    } else {
        snprintf(kpath,sizeof(kpath),"boot():/boot/vmlinuz-%s",st.kernel);
        snprintf(ipath,sizeof(ipath),"boot():/boot/initramfs-%s.img",st.kernel);
        if (microcode[0])
            snprintf(ucode_line,sizeof(ucode_line),
                     "    module_path: boot():/boot/%s.img\n",microcode);
    }

    run_simple("mkdir -p /mnt/boot/limine",0);
    fp = fopen("/mnt/boot/limine.conf","w");
    if (fp) {
        fprintf(fp,"timeout: 5\n\n/Arch Linux\n    protocol: linux\n");
        fprintf(fp,"    path: %s\n",kpath);
        fprintf(fp,"    cmdline: %s rw quiet %s\n",root_opt,extra);
        if (ucode_line[0]) fputs(ucode_line,fp);
        fprintf(fp,"    module_path: %s\n",ipath);
        fclose(fp);
    }
    write_log("limine.conf written to /mnt/boot/limine.conf");

    if (uefi) {
        ib_chroot_c(ib,
            "mkdir -p /boot/efi/EFI/limine && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/limine/",
            "limine copy EFI");
        ib_chroot(ib,
            "mkdir -p /boot/efi/EFI/BOOT && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/BOOT/BOOTX64.EFI",1);

        char src[512],dst[512];
        snprintf(src,sizeof(src),"/mnt/boot/limine.conf");
        snprintf(dst,sizeof(dst),"/mnt/boot/efi/limine.conf");
        run_simple("cp /mnt/boot/limine.conf /mnt/boot/efi/limine.conf",1);

        char p1[256]; char p2[256]; char p3[256];
        char disk_dev[256]; snprintf(disk_dev,sizeof(disk_dev),"/dev/%s",st.disk);
        partition_paths(disk_dev,p1,p2,p3,sizeof(p1));

        char partn[8]="1";
        snprintf(cmd,sizeof(cmd),"lsblk -no PARTN %s 2>/dev/null",p1);
        fp = popen(cmd,"r"); if(fp){fgets(partn,sizeof(partn),fp);pclose(fp);trim_nl(partn);}
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
            "'cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/limine/ && "
            "cp /usr/share/limine/BOOTX64.EFI /boot/efi/EFI/BOOT/BOOTX64.EFI'\n";
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
        ib_stage(ib, L("Installing Intel+NVIDIA (hybrid) drivers...",
                       "Instalando drivers Intel+NVIDIA (hybrid)..."));
        double mid = start + (end-start)*0.4;
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm mesa vulkan-intel intel-media-driver",
                  start,mid,1);
        snprintf(cmd,sizeof(cmd),
                 "arch-chroot /mnt pacman -S --noconfirm %s nvidia-utils nvidia-settings nvidia-prime",
                 nv_pkg);
        ib_pacman(ib,cmd,mid,end,1);
        ib_configure_nvidia_modeset(ib);
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

static void ib_configure_grub_cmdline(IB *ib) {
    if (strcmp(st.filesystem,"btrfs")) return;
    ib_chroot(ib,
        "grep -q 'rootflags=subvol=@' /etc/default/grub || "
        "sed -i 's|^\\(GRUB_CMDLINE_LINUX_DEFAULT=\"[^\"]*\\)\"|\\1 rootflags=subvol=@\"|' "
        "/etc/default/grub",
        1);
}


typedef struct {
    IB *ib;
} IBRunArg;

static void *ib_run_thread(void *arg) {
    IB *ib = ((IBRunArg*)arg)->ib;
    free(arg);

    compile_regexes();

    char disk[256]; snprintf(disk,sizeof(disk),"/dev/%s",st.disk);
    char p1[256],p2[256],p3[256];
    partition_paths(disk,p1,p2,p3,sizeof(p1));

    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    int  uefi       = is_uefi();
    const char *bl  = st.bootloader;
    const char *fs  = st.filesystem;
    const char *ker = st.kernel;
    const char *root_dev = p3;
    char cmd[MAX_CMD];

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

    ib_stage(ib, L("Wiping disk...","Borrando disco..."));
    ib_gradual(ib,7,20,0.04);
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

    if (!strcmp(fs,"btrfs")) {
        ib_setup_btrfs(ib,root_dev,disk);
    } else {
        snprintf(cmd,sizeof(cmd),"mkfs.ext4 -F %s",root_dev);
        ib_run(ib,cmd,"mkfs.ext4");
        snprintf(cmd,sizeof(cmd),"mount %s /mnt",root_dev);
        ib_run(ib,cmd,"mount root");
    }
    ib_pct(ib,16);

    if (uefi) {
        ib_stage(ib, L("Mounting EFI...","Montando EFI..."));
        if (!strcmp(bl,"systemd-boot")) {
            ib_run(ib,"mkdir -p /mnt/boot","mkdir /mnt/boot");
            snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot",p1);
            ib_run(ib,cmd,"mount ESP /mnt/boot");
        } else {
            ib_run(ib,"mkdir -p /mnt/boot/efi","mkdir /mnt/boot/efi");
            snprintf(cmd,sizeof(cmd),"mount %s /mnt/boot/efi",p1);
            ib_run(ib,cmd,"mount ESP /mnt/boot/efi");
        }
    }
    ib_pct(ib,18);

    if (!strcmp(ker,"linux-cachyos")) {
        ib_stage(ib, L("Adding CachyOS repository...","Añadiendo repositorio CachyOS..."));
        ib_add_cachyos_repo(ib,0);
    }

    char ucode_pkg[64]="", extra_pkg[32]="", boot_pkgs[128]="";
    if (microcode[0]) snprintf(ucode_pkg,sizeof(ucode_pkg)," %s",microcode);
    if (!strcmp(fs,"btrfs")) strcpy(extra_pkg," btrfs-progs");

    char kernel_hdrs[64]; snprintf(kernel_hdrs,sizeof(kernel_hdrs),"%s-headers",ker);

    if (!strcmp(bl,"systemd-boot")) {
        strcpy(boot_pkgs," efibootmgr");
    } else if (!strcmp(bl,"limine")) {
        snprintf(boot_pkgs,sizeof(boot_pkgs)," limine%s",uefi?" efibootmgr":"");
    } else {
        snprintf(boot_pkgs,sizeof(boot_pkgs)," grub%s",uefi?" efibootmgr":"");
    }

    snprintf(cmd,sizeof(cmd),
             "pacstrap -K /mnt "
             "base %s linux-firmware %s sof-firmware "
             "base-devel%s vim nano networkmanager git "
             "sudo bash-completion%s%s",
             ker, kernel_hdrs, boot_pkgs, extra_pkg, ucode_pkg);

    ib_stage(ib, L("Installing base system - this may take a while...",
                   "Instalando sistema base - esto puede tardar..."));
    ib_pacman_critical(ib,cmd,18,52,"pacstrap");

    ib_stage(ib, L("Generating fstab...","Generando fstab..."));
    ib_run(ib,"genfstab -U /mnt >> /mnt/etc/fstab","genfstab");
    ib_pct(ib,53);

    if (!strcmp(ker,"linux-cachyos")) ib_add_cachyos_repo(ib,1);

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

    const char *desktop = st.desktop;
    if (strcmp(desktop,"None")) {
        snprintf(stage_msg,sizeof(stage_msg),
                 L("Installing %s...","Instalando %s..."),desktop);
        ib_stage(ib,stage_msg);

        const DesktopDef *dd = get_desktop_def(desktop);
        if (dd) {
            int ng = dd->ngroups;
            double step = 14.0 / (ng>0?ng:1);
            for (int i=0;i<ng;i++) {
                double s=77+i*step, e=77+(i+1)*step;
                snprintf(cmd,sizeof(cmd),
                         "arch-chroot /mnt pacman -S --noconfirm %s",dd->groups[i]);
                ib_pacman(ib,cmd,s,e,1);
            }
        }
        const char *dm = get_desktop_dm(desktop);
        if (dm) {
            snprintf(cmd,sizeof(cmd),"systemctl enable %s",dm);
            ib_chroot(ib,cmd,0);
        }
        if (!strcmp(desktop,"Hyprland")||!strcmp(desktop,"Sway")) {
            snprintf(stage_msg,sizeof(stage_msg),
                     L("Enabling seat management for %s...","Habilitando seat para %s..."),desktop);
            ib_stage(ib,stage_msg);
            snprintf(cmd,sizeof(cmd),"usermod -aG seat,input,video %s",st.username);
            ib_chroot(ib,cmd,1);
        }
        ib_stage(ib, L("Installing audio (pipewire)...","Instalando audio (pipewire)..."));
        ib_pacman(ib,
                  "arch-chroot /mnt pacman -S --noconfirm "
                  "pipewire pipewire-pulse wireplumber",
                  91,94,1);
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

    if (st.flatpak && strcmp(desktop,"None"))
        ib_install_flatpak(ib);

    ib_pct(ib,100);
    ib_stage(ib, L("Installation complete!","Instalacion completa!"));
    ib->on_done(1,"",ib->ud);
    return NULL;
}

#define IS_MODE_PROGRESS 0
#define IS_MODE_DEBUG    1

typedef struct {
    double  pct;
    char    stage[512];
    char  **lines;
    int     nlines;
    int     cap_lines;
    int     mode;
    int     prev_mode;
    char    keybuf[32];
    int     running;
    struct termios old_tty;
    int            has_tty;
    pthread_mutex_t lock;
    pthread_t       render_tid;
    pthread_t       key_tid;
} IS;

static void is_write(const char *s) {
    write(STDOUT_FILENO, s, strlen(s));
}
static void is_writef(const char *fmt, ...) {
    char buf[1024]; va_list ap; va_start(ap,fmt);
    vsnprintf(buf,sizeof(buf),fmt,ap); va_end(ap);
    is_write(buf);
}

static IS *g_is = NULL;

static void is_feed(IS *is, const char *line) {
    pthread_mutex_lock(&is->lock);
    if (is->nlines >= MAX_LINES) {
        int keep = KEEP_LINES;
        memmove(is->lines, is->lines + (is->nlines - keep),
                keep * sizeof(char*));
        for (int i=keep; i<is->nlines; i++) { free(is->lines[i]); is->lines[i]=NULL; }
        is->nlines = keep;
    }
    if (is->nlines >= is->cap_lines) {
        is->cap_lines *= 2;
        is->lines = realloc(is->lines, is->cap_lines * sizeof(char*));
    }
    is->lines[is->nlines++] = strdup(line);
    pthread_mutex_unlock(&is->lock);
}

static void is_update(IS *is, double pct, const char *stage) {
    pthread_mutex_lock(&is->lock);
    is->pct = pct;
    strncpy(is->stage, stage, sizeof(is->stage)-1);
    pthread_mutex_unlock(&is->lock);
}

static void is_get_term_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws)==0 && ws.ws_row>0) {
        *rows = ws.ws_row; *cols = ws.ws_col;
    } else { *rows=24; *cols=80; }
}

static void is_trunc(const char *s, char *out, int n) {
    int len = (int)strlen(s);
    if (len <= n) { strcpy(out,s); return; }
    strncpy(out,s,n-3); out[n-3]='\0'; strcat(out,"...");
}

static void is_draw_progress(IS *is, double pct, const char *stage, int rows, int cols) {
    int W = cols-2 < 74 ? cols-2 : 74;
    if (W < 10) W=10;
    int lft = (cols-W)/2; if(lft<0)lft=0;
    int top = (rows-6)/2; if(top<1)top=1;

    char pad[128]={0};
    for (int i=0;i<lft&&i<(int)sizeof(pad)-1;i++) pad[i]=' ';

    for (int r=1;r<top;r++) is_writef("\033[%d;1H\033[2K",r);
    for (int r=top+7;r<=rows;r++) is_writef("\033[%d;1H\033[2K",r);

    char title[128];
    snprintf(title,sizeof(title)," %s  -  %s ",TITLE,VERSION);
    int tlen=(int)strlen(title);
    char title_padded[256]="";
    int left_pad=(W-tlen)/2; if(left_pad<0)left_pad=0;
    for(int i=0;i<left_pad;i++) strncat(title_padded," ",sizeof(title_padded)-strlen(title_padded)-1);
    strncat(title_padded,title,sizeof(title_padded)-strlen(title_padded)-1);
    while((int)strlen(title_padded)<W)
        strncat(title_padded," ",sizeof(title_padded)-strlen(title_padded)-1);
    is_writef("\033[%d;1H\033[2K%s\033[44m\033[97m\033[1m%s\033[0m",top,pad,title_padded);

    is_writef("\033[%d;1H\033[2K",top+1);

    char trunc_stage[512];
    is_trunc(stage, trunc_stage, W-4);
    is_writef("\033[%d;1H\033[2K%s  \033[1m%s\033[0m",top+2,pad,trunc_stage);

    is_writef("\033[%d;1H\033[2K",top+3);

    int bar_w = W-9; if(bar_w<4)bar_w=4;
    int filled = (int)(bar_w * pct / 100.0);
    int empty  = bar_w - filled;
    char bar[512]="";
    strncat(bar,"\033[44m\033[97m",sizeof(bar)-strlen(bar)-1);
    for(int i=0;i<filled;i++) strncat(bar," ",sizeof(bar)-strlen(bar)-1);
    strncat(bar,"\033[0m\033[100m",sizeof(bar)-strlen(bar)-1);
    for(int i=0;i<empty;i++) strncat(bar," ",sizeof(bar)-strlen(bar)-1);
    strncat(bar,"\033[0m",sizeof(bar)-strlen(bar)-1);
    is_writef("\033[%d;1H\033[2K%s  [%s] \033[1m%3d%%\033[0m",top+4,pad,bar,(int)pct);

    is_writef("\033[%d;1H\033[2K",top+5);
    is_writef("\033[%d;1H\033[2K",top+6);
}

static void is_draw_debug(IS *is, double pct, const char *stage,
                            char **lines, int nlines, int rows, int cols) {
    char hdr_l[512], trunc[512];
    is_trunc(stage, trunc, cols-28);
    snprintf(hdr_l,sizeof(hdr_l)," DEBUG  %.0f%%  %s",pct,trunc);
    const char *hdr_r = "write 'exit' to go back ";
    int gap = cols - (int)strlen(hdr_l) - (int)strlen(hdr_r);
    if (gap<0) gap=0;
    is_writef("\033[1;1H\033[2K\033[44m\033[97m\033[1m%s",hdr_l);
    for(int i=0;i<gap;i++) is_write(" ");
    is_writef("%s\033[0m",hdr_r);

    is_writef("\033[2;1H\033[2K\033[96m");
    for(int i=0;i<cols;i++) is_write("-");
    is_write("\033[0m");

    int available = rows-2;
    int start = nlines-available; if(start<0)start=0;
    for (int i=0;i<available;i++) {
        is_writef("\033[%d;1H\033[2K",3+i);
        if (start+i < nlines) {
            const char *ln = lines[start+i];
            if (strstr(ln,"ERROR")||strstr(ln,"FATAL")||strstr(ln,"CRITICAL"))
                is_write("\033[91m\033[1m");
            else if (strstr(ln,">>>"))
                is_write("\033[93m\033[1m");
            else if (ln[0]=='[')
                is_write("\033[90m");
            char tln[4096]; is_trunc(ln,tln,cols-1);
            is_write(tln);
            is_write("\033[0m");
        }
    }
}

static void *is_render_loop(void *arg) {
    IS *is = arg;
    while (1) {
        pthread_mutex_lock(&is->lock);
        int running = is->running;
        pthread_mutex_unlock(&is->lock);
        if (!running) break;

        pthread_mutex_lock(&is->lock);
        double pct        = is->pct;
        char stage[512];  strncpy(stage,is->stage,511); stage[511]='\0';
        int mode          = is->mode;
        int mode_changed  = (mode != is->prev_mode);
        is->prev_mode     = mode;
        int nlines = is->nlines;
        char **lines_snap = NULL;
        if (mode == IS_MODE_DEBUG && nlines > 0) {
            lines_snap = malloc(nlines * sizeof(char*));
            for (int i=0;i<nlines;i++) lines_snap[i] = strdup(is->lines[i]);
        }
        pthread_mutex_unlock(&is->lock);

        int rows,cols; is_get_term_size(&rows,&cols);
        if (mode_changed) is_write("\033[2J\033[H");
        is_write("\033[?25l");

        if (mode == IS_MODE_PROGRESS)
            is_draw_progress(is, pct, stage, rows, cols);
        else
            is_draw_debug(is, pct, stage, lines_snap, nlines, rows, cols);

        fflush(stdout);

        if (lines_snap) {
            for (int i=0;i<nlines;i++) free(lines_snap[i]);
            free(lines_snap);
        }
        usleep(80000);
    }
    return NULL;
}

static void *is_key_loop(void *arg) {
    IS *is = arg;
    while (1) {
        pthread_mutex_lock(&is->lock);
        int running = is->running;
        pthread_mutex_unlock(&is->lock);
        if (!running) break;

        fd_set fds; FD_ZERO(&fds); FD_SET(STDIN_FILENO,&fds);
        struct timeval tv = {0, 50000};
        if (select(STDIN_FILENO+1,&fds,NULL,NULL,&tv) > 0) {
            char ch[2]={0};
            ssize_t n = read(STDIN_FILENO,ch,1);
            if (n>0) {
                pthread_mutex_lock(&is->lock);
                size_t klen = strlen(is->keybuf);
                if (klen < sizeof(is->keybuf)-2)
                    is->keybuf[klen]=ch[0], is->keybuf[klen+1]='\0';
                else {
                    memmove(is->keybuf, is->keybuf+1, klen-1);
                    is->keybuf[klen-1]=ch[0]; is->keybuf[klen]='\0';
                }
                char *kb = is->keybuf;
                if (strstr(kb,"debug")) { is->mode=IS_MODE_DEBUG; kb[0]='\0'; }
                else if (strstr(kb,"exit")) { is->mode=IS_MODE_PROGRESS; kb[0]='\0'; }
                pthread_mutex_unlock(&is->lock);
            }
        }
    }
    return NULL;
}

static IS *is_create(void) {
    IS *is = calloc(1,sizeof(IS));
    is->cap_lines = 256;
    is->lines     = malloc(is->cap_lines * sizeof(char*));
    is->mode      = IS_MODE_PROGRESS;
    is->prev_mode = IS_MODE_PROGRESS;
    pthread_mutex_init(&is->lock, NULL);
    strncpy(is->stage, L("Preparing...","Preparando..."), sizeof(is->stage)-1);
    return is;
}

static void is_start(IS *is) {
    pthread_mutex_lock(&is->lock);
    is->running = 1;
    pthread_mutex_unlock(&is->lock);

    if (isatty(STDIN_FILENO)) {
        struct termios raw;
        tcgetattr(STDIN_FILENO, &is->old_tty);
        is->has_tty = 1;
        raw = is->old_tty;
        raw.c_lflag &= ~(ICANON|ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
    }
    is_write("\033[?25l\033[2J\033[H");
    fflush(stdout);
    pthread_create(&is->render_tid, NULL, is_render_loop, is);
    pthread_create(&is->key_tid,    NULL, is_key_loop,    is);
}

static void is_stop(IS *is) {
    pthread_mutex_lock(&is->lock);
    is->running = 0;
    pthread_mutex_unlock(&is->lock);
    pthread_join(is->render_tid, NULL);
    pthread_join(is->key_tid,    NULL);
    if (is->has_tty) tcsetattr(STDIN_FILENO, TCSADRAIN, &is->old_tty);
    is_write("\033[?25h\033[2J\033[H");
    fflush(stdout);
    for (int i=0;i<is->nlines;i++) free(is->lines[i]);
    free(is->lines);
    pthread_mutex_destroy(&is->lock);
    free(is);
}

typedef struct { IS *is; volatile int *stop; } TailerArg;

static void *tailer_thread(void *arg) {
    TailerArg *ta = arg;
    int fd = open(LOG_FILE, O_RDONLY|O_CREAT, 0644);
    if (fd<0) return NULL;
    lseek(fd, 0, SEEK_END);
    FILE *f = fdopen(fd,"r");
    if (!f) { close(fd); return NULL; }

    char line[4096];
    while (!*ta->stop) {
        if (fgets(line,sizeof(line),f)) {
            size_t len=strlen(line);
            while(len>0&&(line[len-1]=='\n'||line[len-1]=='\r')) line[--len]='\0';
            if (len>0) is_feed(ta->is,line);
        } else {
            clearerr(f);
            usleep(50000);
        }
    }
    fclose(f);
    return NULL;
}

typedef struct {
    IS    *is;
    double pct;
    char   stage[512];
    int    done;
    int    success;
    char   reason[1024];
    pthread_mutex_t mu;
    pthread_cond_t  cv;
} InstallState;

static void on_progress_cb(double pct, void *ud) {
    InstallState *iss = ud;
    pthread_mutex_lock(&iss->mu);
    iss->pct = pct;
    is_update(iss->is, pct, iss->stage);
    pthread_mutex_unlock(&iss->mu);
}

static void on_stage_cb(const char *msg, void *ud) {
    InstallState *iss = ud;
    pthread_mutex_lock(&iss->mu);
    strncpy(iss->stage,msg,sizeof(iss->stage)-1);
    is_update(iss->is, iss->pct, msg);
    pthread_mutex_unlock(&iss->mu);
}

static void on_done_cb(int ok, const char *reason, void *ud) {
    InstallState *iss = ud;
    pthread_mutex_lock(&iss->mu);
    iss->success = ok;
    strncpy(iss->reason,reason?reason:"",sizeof(iss->reason)-1);
    iss->done = 1;
    pthread_cond_signal(&iss->cv);
    pthread_mutex_unlock(&iss->mu);
}

static int screen_install(void) {
    { FILE *f=fopen(LOG_FILE,"a"); if(f) fclose(f); }

    IS           *is  = is_create();
    InstallState  iss = {
        .is      = is,
        .pct     = 0.0,
        .done    = 0,
        .success = 0,
        .mu      = PTHREAD_MUTEX_INITIALIZER,
        .cv      = PTHREAD_COND_INITIALIZER,
    };
    strncpy(iss.stage, L("Preparing...","Preparando..."), sizeof(iss.stage)-1);

    IB *ib = calloc(1,sizeof(IB));
    ib->on_progress = on_progress_cb;
    ib->on_stage    = on_stage_cb;
    ib->on_done     = on_done_cb;
    ib->ud          = &iss;
    pthread_mutex_init(&ib->lock,NULL);

    volatile int stop_tail = 0;
    TailerArg ta = {is, &stop_tail};
    pthread_t tailer_tid;
    pthread_create(&tailer_tid, NULL, tailer_thread, &ta);

    IBRunArg *ra = malloc(sizeof(IBRunArg));
    ra->ib = ib;

    is_start(is);

    pthread_t install_tid;
    pthread_create(&install_tid, NULL, ib_run_thread, ra);

    /* Wait for completion */
    pthread_mutex_lock(&iss.mu);
    while (!iss.done) pthread_cond_wait(&iss.cv, &iss.mu);
    pthread_mutex_unlock(&iss.mu);

    stop_tail = 1;
    pthread_join(tailer_tid,  NULL);
    pthread_join(install_tid, NULL);
    usleep(150000);
    is_stop(is);

    pthread_mutex_destroy(&ib->lock);
    free(ib);
    pthread_mutex_destroy(&iss.mu);
    pthread_cond_destroy(&iss.cv);

    if (!iss.success) {
        char msg[1536];
        snprintf(msg,sizeof(msg),
                 L("Installation failed.\n\n%s\n\nCheck %s for details.",
                   "La instalacion fallo.\n\n%s\n\nRevisa %s para detalles."),
                 iss.reason, LOG_FILE);
        msgbox(L("Installation Failed","Instalacion fallida"),msg);
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
        st.quick=1; strncpy(st.filesystem,"btrfs",sizeof(st.filesystem)-1);
        strncpy(st.kernel,"linux",sizeof(st.kernel)-1);
        strncpy(st.desktop,"KDE Plasma",sizeof(st.desktop)-1);
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
                   L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                     "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres."));
            continue;
        }
        char un[64]={0};
        if (!inputbox_dlg(L("System Identity","Identidad del sistema"),
                          L("Enter username (letters, digits, -, _ - max 32):",
                            "Ingresa el nombre de usuario (letras, digitos, -, _ - max 32):"),
                          st.username, un, sizeof(un))) return 0;
        if (!validate_name(un)) {
            msgbox(L("Invalid username","Usuario invalido"),
                   L("Only letters, digits, hyphens and underscores. Max 32 chars.",
                     "Solo letras, digitos, guiones y guiones bajos. Max 32 caracteres."));
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
    DiskInfo disks[32]; int nd = list_disks(disks,32);
    if (nd==0) {
        msgbox(L("No disks found","Sin discos"),
               L("No disks were detected. Cannot continue.",
                 "No se detectaron discos. No se puede continuar."));
        exit(1);
    }

    char lsblk[4096]={0};
    FILE *fp = popen("lsblk -f 2>/dev/null | head -40","r");
    if (fp) { fread(lsblk,1,sizeof(lsblk)-1,fp); pclose(fp); }
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
                 L("Suggested swap size based on your RAM: %s GB\n\nEnter swap size in GB (1-128):",
                   "Tamano de swap sugerido segun tu RAM: %s GB\n\nIngresa el tamano del swap en GB (1-128):"),
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
}

static int screen_filesystem(void) {
    MenuItem items[2];
    strncpy(items[0].tag,"ext4",255);
    snprintf(items[0].desc,511,"%s",L("ext4  - stable, widely supported",
                                       "ext4  - estable, amplio soporte"));
    strncpy(items[1].tag,"btrfs",255);
    snprintf(items[1].desc,511,"%s",L("btrfs - subvolumes (@,@home,@var,@snapshots) + zstd compression",
                                       "btrfs - subvolumenes (@,@home,@var,@snapshots) + compresion zstd"));
    char out[16]={0};
    if (!radiolist_dlg(L("Filesystem","Sistema de archivos"),
                       L("Choose the root filesystem:","Elige el sistema de archivos raiz:"),
                       items,2,st.filesystem,out,sizeof(out))) return 0;
    strncpy(st.filesystem,out,sizeof(st.filesystem)-1);
    return 1;
}

static int screen_kernel(void) {
    MenuItem items[4];
    strncpy(items[0].tag,"linux",255);
    snprintf(items[0].desc,511,"%s",L("linux         - latest stable kernel","linux         - kernel estable mas reciente"));
    strncpy(items[1].tag,"linux-lts",255);
    snprintf(items[1].desc,511,"%s",L("linux-lts     - long-term support kernel","linux-lts     - kernel de soporte a largo plazo"));
    strncpy(items[2].tag,"linux-zen",255);
    snprintf(items[2].desc,511,"%s",L("linux-zen     - optimized for desktop / gaming","linux-zen     - optimizado para escritorio / gaming"));
    strncpy(items[3].tag,"linux-cachyos",255);
    snprintf(items[3].desc,511,"%s",L("linux-cachyos - CachyOS kernel [RECOMMENDED for max speed]",
                                       "linux-cachyos - kernel CachyOS [RECOMENDADO para maxima velocidad]"));
    char out[32]={0};
    if (!radiolist_dlg(L("Kernel","Kernel"),
                       L("Select the kernel to install:","Selecciona el kernel a instalar:"),
                       items,4,st.kernel,out,sizeof(out))) return 0;
    strncpy(st.kernel,out,sizeof(st.kernel)-1);
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
        L("Choose the locale for the INSTALLED SYSTEM.\n"
          "This is INDEPENDENT of the installer UI language.\n\n"
          "Controls: system language, date/number formats, etc.",
          "Elige el locale para el SISTEMA INSTALADO.\n"
          "Es INDEPENDIENTE del idioma de este instalador.\n\n"
          "Controla: idioma del sistema, formatos de fecha/numeros, etc."),
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
    if (fp) { fread(avail,1,sizeof(avail)-1,fp); pclose(fp); }

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
    if (fp) { fread(zones_raw,1,sizeof(zones_raw)-1,fp); pclose(fp); }

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
        char *sl=strchr(zones[i],'/');
        if(!sl) continue;
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
    if(strchr(st.timezone,'/')) { char *sl=strchr(st.timezone,'/');
        int l=sl-st.timezone; if(l>63)l=63; strncpy(cur_reg,st.timezone,l); cur_reg[l]='\0'; }

    char sel_reg[64]={0};
    if(!radiolist_dlg(L("Timezone - Region","Zona horaria - Region"),
                      L("Select your region:","Selecciona tu region:"),
                      reg_items,nr+1,cur_reg,sel_reg,sizeof(sel_reg))) {
        for(int i=0;i<nr;i++) free(regions[i]);
        free(reg_items);
        for(int i=0;i<nz;i++) free(zones[i]); free(zones);
        return 0;
    }
    for(int i=0;i<nr;i++) free(regions[i]); free(reg_items);

    if(!strcmp(sel_reg,"UTC")) {
        strncpy(st.timezone,"UTC",sizeof(st.timezone)-1);
        for(int i=0;i<nz;i++) free(zones[i]); free(zones);
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
    for(int i=0;i<nz;i++) free(zones[i]); free(zones);

    if(nc==0) { if(city_items)free(city_items);
        strncpy(st.timezone,sel_reg,sizeof(st.timezone)-1); return 1; }

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
        {"Hyprland",  L("Hyprland  - tiling Wayland compositor, modern + animations",
                        "Hyprland  - compositor Wayland tiling, moderno + animaciones")},
        {"Sway",      L("Sway      - tiling Wayland compositor, i3-compatible",
                        "Sway      - compositor Wayland tiling, compatible con i3")},
        {"None",      L("None      - CLI only, no desktop","None      - solo terminal, sin escritorio")},
        {NULL,NULL}
    };
    int n=0; while(desktops[n][0]) n++;
    MenuItem *items=malloc(n*sizeof(MenuItem));
    for(int i=0;i<n;i++) {
        strncpy(items[i].tag,desktops[i][0],255);
        strncpy(items[i].desc,desktops[i][1],511);
    }
    char out[32]={0};
    int ok=radiolist_dlg(L("Desktop Environment","Entorno de escritorio"),
                          L("Choose a desktop environment:","Elige un entorno de escritorio:"),
                          items,n,st.desktop,out,sizeof(out));
    free(items);
    if(!ok) return 0;
    strncpy(st.desktop,out,sizeof(st.desktop)-1);
    return 1;
}

static int screen_gpu(void) {
    char detected[32]="None"; detect_gpu(detected,sizeof(detected));
    if(strcmp(detected,"None")&&!strcmp(st.gpu,"None"))
        strncpy(st.gpu,detected,sizeof(st.gpu)-1);

    char hint[128];
    snprintf(hint,sizeof(hint),L("Detected GPU: %s","GPU detectada: %s"),detected);

    const char *gpus[][2]={
        {"NVIDIA",      L("NVIDIA proprietary (nvidia/nvidia-dkms + utils)","NVIDIA propietario (nvidia/nvidia-dkms + utils)")},
        {"AMD",         L("AMD open-source (mesa + vulkan-radeon)","AMD open-source (mesa + vulkan-radeon)")},
        {"Intel",       L("Intel open-source (mesa + vulkan-intel + intel-media-driver)","Intel open-source (mesa + vulkan-intel + intel-media-driver)")},
        {"Intel+NVIDIA",L("Intel + NVIDIA hybrid (Mesa + proprietary NVIDIA)","Intel + NVIDIA hibrido (Mesa + NVIDIA propietario)")},
        {"Intel+AMD",   L("Intel + AMD hybrid (Mesa + vulkan-radeon)","Intel + AMD hibrido (Mesa + vulkan-radeon)")},
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
                      L("Install Flatpak and add the Flathub repository?\n\n"
                        "Flatpak lets you install thousands of apps from Flathub\n"
                        "independently of Arch packages.",
                        "Instalar Flatpak y anadir el repositorio Flathub?\n\n"
                        "Flatpak permite instalar miles de aplicaciones de Flathub\n"
                        "de forma independiente a los paquetes de Arch."),
                      items,2,st.flatpak?"yes":"no",out,sizeof(out))) return 0;
    st.flatpak=!strcmp(out,"yes");
    return 1;
}

static int screen_review(void) {
    char microcode[32]; detect_cpu(microcode,sizeof(microcode));
    if(!microcode[0]) strncpy(microcode,L("none detected","no detectado"),sizeof(microcode)-1);
    const char *x11 = kv_get(CONSOLE_TO_X11,st.keymap);
    if(!x11) x11=st.keymap;
    const char *boot_mode = is_uefi()?"UEFI":"BIOS";

    char text[4096]={0};
    char line[256];
#define ROW(label,val) snprintf(line,sizeof(line),"  %-18s %s\n",label,val); strncat(text,line,sizeof(text)-strlen(text)-1)
    snprintf(line,sizeof(line),"%s\n\n",L("Review your settings:","Revisa tu configuracion:"));
    strncat(text,line,sizeof(text)-strlen(text)-1);

    ROW("Mode",           st.quick?L("Quick","Rapida"):L("Custom","Personalizada"));
    ROW("Boot",           boot_mode);
    ROW("Installer lang", st.lang);
    ROW("System locale",  st.locale);
    ROW("Hostname",       st.hostname[0]?st.hostname:"NOT SET");
    ROW(L("Username","Usuario"), st.username[0]?st.username:"NOT SET");
    ROW("Filesystem",     st.filesystem);
    ROW("Kernel",         st.kernel);
    ROW("Bootloader",     st.bootloader);
    ROW("Microcode",      microcode);
    char disk_str[128]; snprintf(disk_str,sizeof(disk_str),"/dev/%s",st.disk);
    ROW("Disk",           st.disk[0]?disk_str:"NOT SET");
    char swap_str[32];   snprintf(swap_str,sizeof(swap_str),"%s GB",st.swap);
    ROW("Swap",           swap_str);
    ROW("Mirrors",        st.mirrors?L("reflector (auto)","reflector (auto)"):L("default","por defecto"));
    ROW("Keymap TTY",     st.keymap);
    ROW("Keymap X11",     x11);
    ROW("Timezone",       st.timezone);
    ROW("Desktop",        st.desktop);
    ROW("GPU",            st.gpu);
    ROW("Audio",          strcmp(st.desktop,"None")?"pipewire":L("none","ninguno"));
    ROW("Flatpak",        st.flatpak?L("yes","si"):"no");
    ROW("yay",            st.yay?L("yes","si"):"no");
    ROW("snapper",        st.snapper?L("yes","si"):"no");
#undef ROW

    char missing[256]={0};
    if(!st.hostname[0]) strncat(missing,"hostname, ",sizeof(missing)-strlen(missing)-1);
    if(!st.username[0]) strncat(missing,"username, ",sizeof(missing)-strlen(missing)-1);
    if(!st.disk[0])     strncat(missing,"disk, ",sizeof(missing)-strlen(missing)-1);
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
    char confirm_msg[5120];
    snprintf(confirm_msg,sizeof(confirm_msg),"%s%s",text,
             L("\n\nWARNING: THIS WILL ERASE /dev/","\n\nADVERTENCIA: SE BORRARA /dev/"));
    strncat(confirm_msg,st.disk,sizeof(confirm_msg)-strlen(confirm_msg)-1);
    strncat(confirm_msg,L(".\n\nProceed?",".\n\nProceder?"),sizeof(confirm_msg)-strlen(confirm_msg)-1);

    return yesno_dlg(L("Review & Confirm","Revisar y confirmar"),confirm_msg);
}

static void screen_finish(void) {
    if(yesno_dlg(L("Installation Complete!","Instalacion completa!"),
                  L("Arch Linux has been installed successfully.\n\n"
                    "Remove the installation media. Reboot now?",
                    "Arch Linux se ha instalado correctamente.\n\n"
                    "Extrae el medio de instalacion. Reiniciar ahora?"))) {
        infobox_dlg(L("Rebooting","Reiniciando"),
                    L("Unmounting filesystems and rebooting...",
                      "Desmontando sistemas de archivos y reiniciando..."));
        system("umount -R /mnt 2>/dev/null");
        system("reboot");
    }
    exit(0);
}

typedef struct {
    const char *name;
    int        (*fn)(void);
    int         can_go_back;
} Step;

static int screen_welcome_wrap(void)  { screen_welcome();  return 1; }
static int screen_language_wrap(void) { screen_language(); return 1; }
static int screen_network_wrap(void)  { screen_network();  return 1; }
static int screen_finish_wrap(void)   { screen_finish();   return 1; }
static int screen_install_wrap(void)  { return screen_install(); }

int main(void) {
    if(geteuid()!=0) {
        fprintf(stderr,"This installer must be run as root.\n"
                       "Example: sudo ./arch_installer\n");
        return 1;
    }
    if(system("which dialog >/dev/null 2>&1")!=0) {
        printf("[*] dialog not found - installing via pacman...\n");
        if(system("pacman -Sy --noconfirm dialog")!=0) {
            fprintf(stderr,"[!] Failed to install dialog. Check your network.\n");
            return 1;
        }
        printf("[+] dialog installed.\n\n");
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
        {L("Passwords","Contrasenas"),     screen_passwords,     1},
        {L("Review","Revision"),           screen_review,        1},
        {L("Install","Instalar"),          screen_install_wrap,  0},
        {L("Finish","Finalizar"),          screen_finish_wrap,   0},
        {NULL,NULL,0}
    };

    Step custom_steps[] = {
        {L("Locale","Idioma sistema"),     screen_locale,        1},
        {L("Disk","Disco"),                screen_disk,          1},
        {L("Filesystem","Sistema archivos"),screen_filesystem,   1},
        {L("Kernel","Kernel"),             screen_kernel,        1},
        {L("Bootloader","Bootloader"),     screen_bootloader,    1},
        {L("Mirrors","Mirrors"),           screen_mirrors,       1},
        {L("Identity","Identidad"),        screen_identity,      1},
        {L("Passwords","Contrasenas"),     screen_passwords,     1},
        {L("Keymap","Teclado"),            screen_keymap,        1},
        {L("Timezone","Zona horaria"),     screen_timezone,      1},
        {L("Desktop","Escritorio"),        screen_desktop,       1},
        {"GPU",                            screen_gpu,           1},
        {L("yay","yay"),                   screen_yay,           1},
        {L("Flatpak","Flatpak"),           screen_flatpak,       1},
        {L("Snapshots","Snapshots"),       screen_snapper,       1},
        {L("Review","Revision"),           screen_review,        1},
        {L("Install","Instalar"),          screen_install_wrap,  0},
        {L("Finish","Finalizar"),          screen_finish_wrap,   0},
        {NULL,NULL,0}
    };

    Step *steps = quick ? quick_steps : custom_steps;
    int idx=0;
    while(steps[idx].fn) {
        int result = steps[idx].fn();
        if(result==0 && steps[idx].can_go_back) {
            if(idx==0) {
                if(yesno_dlg(L("Exit","Salir"),
                             L("Exit the installer?","Salir del instalador?")))
                    exit(0);
            } else idx--;
        } else idx++;
    }
    return 0;
}
