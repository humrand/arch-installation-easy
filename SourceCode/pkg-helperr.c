#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>

#define MAX_NAME    256
#define MAX_VER      64
#define MAX_DESC    512
#define MAX_CMD     512
#define MAX_LINE   1024

#define APP_VERSION "0.0.5-beta"

typedef enum { LANG_ES = 0, LANG_EN = 1 } LangID;
static LangID g_lang = LANG_EN;

typedef enum {
    STR_PLACEHOLDER,
    STR_BTN_SEARCH,
    STR_SOURCES_LABEL,
    STR_PACMAN_CHECK,
    STR_STATUS_READY,
    STR_BTN_REMOVE,
    STR_BTN_INSTALL,
    STR_TOOLTIP_REMOVE,
    STR_TOOLTIP_INSTALL,
    STR_INSTALLED,
    STR_SEARCHING,
    STR_NO_RESULTS,
    STR_FOUND_PKGS,
    STR_MARK_INSTALL,
    STR_MARK_REMOVE,
    STR_INSTALLING,
    STR_REMOVING,
    STR_ALACRITTY_DONE,
    STR_ALACRITTY_ERR,
    STR_COL_STATUS,
    STR_COL_SOURCE,
    STR_COL_NAME,
    STR_COL_VERSION,
    STR_COL_DESC,
    STR_CHECKING_UPDATES,
    STR_UP_TO_DATE,
    STR_UPDATED_BIN,
    STR_UPDATED_ICON,
    STR_UPDATED_BOTH,
    STR_UPDATE_RESTART,
    STR_UPDATE_FAILED,
    STR_BTN_CHECK_UPDATES,
    STR_APPLYING_UPDATE,
    STR_TOOLTIP_UPDATE,
    STR_BTN_CHANGELOG,
    STR_CHANGELOG_TITLE,
    STR_BTN_UPDATE_SYS,
    STR_BTN_UPDATE_ALL,
    STR_TOOLTIP_UPDATE_SYS,
    STR_TOOLTIP_UPDATE_ALL,
    N_STRINGS
} StrID;

static const char *g_strings[N_STRINGS][2] = {
    { "Busca un paquete...",
      "Search for a package..."                        },
    { "Buscar",
      "Search"                                         },
    { "Fuentes:",
      "Sources:"                                       },
    { "pacman (oficial)",
      "pacman (official)"                              },
    { "Listo. Escribe un paquete y pulsa Buscar.",
      "Ready. Type a package and press Search."        },
    { "Eliminar",
      "Remove"                                         },
    { "Instalar",
      "Install"                                        },
    { "sudo pacman -Rns / flatpak uninstall",
      "sudo pacman -Rns / flatpak uninstall"           },
    { "Instala los paquetes marcados no instalados",
      "Install checked packages not yet installed"     },
    { "Instalado",
      "Installed"                                      },
    { "Buscando...",
      "Searching..."                                   },
    { "Sin resultados para \"%s\"",
      "No results for \"%s\""                          },
    { "%u paquetes encontrados para \"%s\"",
      "%u packages found for \"%s\""                  },
    { "Marca paquetes no instalados para instalar",
      "Check uninstalled packages to install"          },
    { "Marca paquetes instalados para eliminar",
      "Check installed packages to remove"             },
    { "Instalando %d paquete(s)...",
      "Installing %d package(s)..."                    },
    { "Eliminando %d paquete(s)...",
      "Removing %d package(s)..."                      },
    { "--- Listo. Pulsa Enter para cerrar ---",
      "--- Done. Press Enter to close ---"             },
    { "Error al abrir Alacritty: %s",
      "Error opening Alacritty: %s"                   },
    { "Estado",
      "Status"                                         },
    { "Fuente",
      "Source"                                         },
    { "Nombre",
      "Name"                                           },
    { "Versión",
      "Version"                                        },
    { "Descripción",
      "Description"                                    },
    { "Comprobando actualizaciones...",
      "Checking for updates..."                        },
    { "La aplicación está actualizada.",
      "Application is up to date."                     },
    { "App actualizada. Reinicia para aplicar.",
      "App updated. Restart to apply."                 },
    { "Icono actualizado.",
      "Icon updated."                                  },
    { "App e icono actualizados. Reinicia para aplicar.",
      "App and icon updated. Restart to apply."        },
    { "Reinicia la aplicación para usar la nueva versión.",
      "Restart the application to use the new version." },
    { "No se pudo comprobar las actualizaciones.",
      "Could not check for updates."                   },
    { "Buscar actualizaciones",
      "Check for updates"                              },
    { "Aplicando actualización...",
      "Applying update..."                             },
    { "Comprueba si hay una nueva versión",
      "Check if a new version is available"            },
    { "Historial de cambios",
      "Changelog"                                      },
    { "Historial de cambios — PKG Helper",
      "Changelog — PKG Helper"                         },
    { "Actualizar sistema",
      "Update system"                                  },
    { "Actualizar todo",
      "Update all"                                     },
    { "sudo pacman -Syu",
      "sudo pacman -Syu"                               },
    { "sudo pacman -Syu && yay -Syu && flatpak update",
      "sudo pacman -Syu && yay -Syu && flatpak update" },
};

#define T(id) g_strings[(id)][g_lang]


static char *lang_config_path(void) {
    const char *home = g_get_home_dir();
    return g_build_filename(home, ".config", "pkg-helper", "lang", NULL);
}

static void save_lang_pref(void) {
    char *path = lang_config_path();
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(g_lang == LANG_EN ? "EN" : "ES", fp);
        fclose(fp);
    }
    g_free(dir);
    g_free(path);
}

static void load_lang_pref(void) {
    char *path = lang_config_path();
    FILE *fp = fopen(path, "r");
    if (fp) {
        char buf[8] = {0};
        fgets(buf, sizeof(buf), fp);
        fclose(fp);
        if (strncmp(buf, "EN", 2) == 0)
            g_lang = LANG_EN;
        else if (strncmp(buf, "ES", 2) == 0)
            g_lang = LANG_ES;
    }
    g_free(path);
}


enum {
    COL_CHECK = 0,
    COL_STATUS,
    COL_SOURCE,
    COL_NAME,
    COL_VERSION,
    COL_DESC,
    COL_CMD,
    COL_REMOVE_CMD,
    COL_INSTALLED,
    N_COLS
};

static GtkWidget         *g_win;
static GtkWidget         *g_entry;
static GtkWidget         *g_btn_search;
static GtkWidget         *g_btn_install;
static GtkWidget         *g_btn_remove;
static GtkWidget         *g_btn_update;
static GtkWidget         *g_btn_changelog;
static GtkWidget         *g_btn_update_sys;
static GtkWidget         *g_btn_update_all;
static GtkWidget         *g_ver_label;
static GtkWidget         *g_status;
static GtkWidget         *g_tree;
static GtkWidget         *g_spinner;
static GtkListStore      *g_store;
static GtkWidget         *g_chk_pacman;
static GtkWidget         *g_chk_aur;
static GtkWidget         *g_chk_flatpak;
static GtkWidget         *g_label_sources;
static GtkWidget         *g_lang_combo;
static GtkTreeViewColumn *g_col_status_w;
static GtkTreeViewColumn *g_col_source_w;
static GtkTreeViewColumn *g_col_name_w;
static GtkTreeViewColumn *g_col_version_w;
static GtkTreeViewColumn *g_col_desc_w;

typedef struct {
    char     source[16];
    char     name[MAX_NAME];
    char     version[MAX_VER];
    char     desc[MAX_DESC];
    char     cmd[MAX_CMD];
    char     remove_cmd[MAX_CMD];
    gboolean installed;
} Pkg;

typedef struct {
    char     query[256];
    gboolean use_pacman;
    gboolean use_aur;
    gboolean use_flatpak;
} SearchCtx;

typedef struct {
    GArray  *pkgs;       
    char     query[256];  
    gboolean is_last;     
    guint    grand_total; 
} BatchCtx;

static void trim(char *s) {
    if (!s || !*s) return;
    char *end = s + strlen(s) - 1;
    while (end >= s && (*end=='\n'||*end=='\r'||*end==' '||*end=='\t')) *end-- = '\0';
    char *start = s;
    while (*start==' '||*start=='\t') start++;
    if (start != s) memmove(s, start, strlen(start)+1);
}

static void strip_ansi(char *dst, const char *src, size_t dstlen) {
    size_t di = 0;
    for (size_t si = 0; src[si] && di < dstlen-1; si++) {
        if (src[si] == '\033') {
            si++;
            if (src[si] == '[') { si++; while (src[si] && src[si]!='m') si++; }
        } else {
            dst[di++] = src[si];
        }
    }
    dst[di] = '\0';
}

static void filter_exact(GArray *pkgs, const char *query) {
    for (int i = (int)pkgs->len - 1; i >= 0; i--) {
        Pkg *p = &g_array_index(pkgs, Pkg, i);
        if (strcasecmp(p->name, query) != 0)
            g_array_remove_index_fast(pkgs, i);
    }
}

static GHashTable *build_pacman_installed(void) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    FILE *fp = popen("pacman -Q 2>/dev/null", "r");
    if (!fp) return ht;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        char *sp = strchr(line, ' '); if (sp) *sp = '\0';
        trim(line);
        if (line[0]) g_hash_table_add(ht, g_strdup(line));
    }
    pclose(fp);
    return ht;
}

static GHashTable *build_flatpak_installed(void) {
    GHashTable *ht = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    FILE *fp = popen("flatpak list --columns=application 2>/dev/null", "r");
    if (!fp) return ht;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim(line);
        if (line[0]) g_hash_table_add(ht, g_strdup(line));
    }
    pclose(fp);
    return ht;
}

static void parse_pacman_output(FILE *fp, GArray *results,
                                const char *src_label, const char *install_prefix)
{
    char raw[MAX_LINE], line[MAX_LINE];
    Pkg pkg; gboolean pending = FALSE;

    while (fgets(raw, sizeof(raw), fp)) {
        strip_ansi(line, raw, sizeof(line));
        if (line[0]==' ' || line[0]=='\t') {
            if (pending) {
                trim(line); strncpy(pkg.desc, line, sizeof(pkg.desc)-1);
                g_array_append_val(results, pkg); pending = FALSE;
            }
            continue;
        }
        trim(line); if (!line[0]) continue;
        char *slash = strchr(line, '/'), *space = strchr(line, ' ');
        if (!slash || !space || slash > space) continue;
        memset(&pkg, 0, sizeof(pkg));
        strncpy(pkg.source, src_label, sizeof(pkg.source)-1);
        size_t namelen = (size_t)(space-slash-1);
        if (namelen >= sizeof(pkg.name)) namelen = sizeof(pkg.name)-1;
        strncpy(pkg.name, slash+1, namelen); pkg.name[namelen] = '\0';
        char *ver_start = space+1, *ver_end = strchr(ver_start, ' ');
        size_t verlen = ver_end ? (size_t)(ver_end-ver_start) : strlen(ver_start);
        if (verlen >= sizeof(pkg.version)) verlen = sizeof(pkg.version)-1;
        strncpy(pkg.version, ver_start, verlen); pkg.version[verlen] = '\0';
        snprintf(pkg.cmd,        sizeof(pkg.cmd),        "%s %s", install_prefix, pkg.name);
        snprintf(pkg.remove_cmd, sizeof(pkg.remove_cmd), "sudo pacman -Rns %s", pkg.name);
        pending = TRUE;
    }
    if (pending && pkg.name[0]) g_array_append_val(results, pkg);
}

static void parse_flatpak_output(FILE *fp, GArray *results) {
    char header[MAX_LINE], line[MAX_LINE];
    if (!fgets(header, sizeof(header), fp)) return;

    int pos_desc=-1, pos_appid=-1, pos_ver=-1;
    char *p = strstr(header, "escripci");
    if (p) { while (p>header && *(p-1)!=' ') p--; pos_desc=(int)(p-header); }
    p = strstr(header, " ID");
    if (!p) p = strstr(header, "pplicat");
    if (p) { while (*p==' ') p++; pos_appid=(int)(p-header); }
    p = strstr(header, "ersi");
    if (p) { while (p>header && *(p-1)!=' ') p--; pos_ver=(int)(p-header); }
    if (pos_desc<0 || pos_appid<0) return;

    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n'); if (nl) *nl='\0';
        if (!line[0]) continue;
        int len = (int)strlen(line);
        Pkg pkg; memset(&pkg, 0, sizeof(pkg));
        strncpy(pkg.source, "flatpak", sizeof(pkg.source)-1);
        { int end=pos_desc<len?pos_desc:len, sz=end<(int)sizeof(pkg.name)?end:(int)sizeof(pkg.name)-1;
          strncpy(pkg.name, line, sz); trim(pkg.name); }
        if (pos_desc<len) {
            int end=pos_appid<len?pos_appid:len, dlen=end-pos_desc;
            if (dlen>0) { int sz=dlen<(int)sizeof(pkg.desc)?dlen:(int)sizeof(pkg.desc)-1;
                strncpy(pkg.desc, line+pos_desc, sz); trim(pkg.desc); }
        }
        char appid[256]={0};
        if (pos_appid<len) {
            int end=(pos_ver>0&&pos_ver<len)?pos_ver:len, alen=end-pos_appid;
            if (alen>0) { int sz=alen<(int)sizeof(appid)?alen:(int)sizeof(appid)-1;
                strncpy(appid, line+pos_appid, sz); trim(appid); }
        }
        if (pos_ver>0 && pos_ver<len) {
            char tmp[64]={0}; strncpy(tmp, line+pos_ver, sizeof(tmp)-1);
            char *sp=strchr(tmp,' '); if (sp) *sp='\0'; trim(tmp);
            strncpy(pkg.version, tmp, sizeof(pkg.version)-1);
        }
        if (!pkg.name[0]) continue;
        if (appid[0]) {
            snprintf(pkg.cmd,        sizeof(pkg.cmd),        "flatpak install flathub %s", appid);
            snprintf(pkg.remove_cmd, sizeof(pkg.remove_cmd), "flatpak uninstall %s", appid);
        } else {
            snprintf(pkg.cmd,        sizeof(pkg.cmd),        "flatpak install %s", pkg.name);
            snprintf(pkg.remove_cmd, sizeof(pkg.remove_cmd), "flatpak uninstall %s", pkg.name);
        }
        g_array_append_val(results, pkg);
    }
}


static gboolean batch_add_cb(gpointer data) {
    BatchCtx *ctx = data;

    gtk_widget_freeze_child_notify(g_tree);
    g_object_freeze_notify(G_OBJECT(g_store));

    for (guint i = 0; i < ctx->pkgs->len; i++) {
        Pkg *p = &g_array_index(ctx->pkgs, Pkg, i);
        GtkTreeIter it;
        gtk_list_store_append(g_store, &it);
        gtk_list_store_set(g_store, &it,
            COL_CHECK,      FALSE,
            COL_STATUS,     p->installed ? T(STR_INSTALLED) : "",
            COL_SOURCE,     p->source,
            COL_NAME,       p->name,
            COL_VERSION,    p->version,
            COL_DESC,       p->desc,
            COL_CMD,        p->cmd,
            COL_REMOVE_CMD, p->remove_cmd,
            COL_INSTALLED,  p->installed,
            -1);
    }

    g_object_thaw_notify(G_OBJECT(g_store));
    gtk_widget_thaw_child_notify(g_tree);

    if (ctx->is_last) {
        char status[128];
        if (ctx->grand_total == 0)
            snprintf(status, sizeof(status), T(STR_NO_RESULTS), ctx->query);
        else
            snprintf(status, sizeof(status), T(STR_FOUND_PKGS),
                     ctx->grand_total, ctx->query);
        gtk_label_set_text(GTK_LABEL(g_status), status);
        gtk_spinner_stop(GTK_SPINNER(g_spinner));
        gtk_widget_set_sensitive(g_btn_search,  TRUE);
        gtk_widget_set_sensitive(g_btn_install, TRUE);
        gtk_widget_set_sensitive(g_btn_remove,  TRUE);
    }

    g_array_free(ctx->pkgs, TRUE);
    g_free(ctx);
    return G_SOURCE_REMOVE;
}

static void dispatch_batch(GArray *pkgs, const char *query,
                           gboolean is_last, guint grand_total) {
    BatchCtx *ctx  = g_new0(BatchCtx, 1);
    ctx->pkgs       = pkgs;
    ctx->is_last    = is_last;
    ctx->grand_total= grand_total;
    strncpy(ctx->query, query, sizeof(ctx->query) - 1);
    g_idle_add(batch_add_cb, ctx);
}


static gpointer search_thread(gpointer data) {
    SearchCtx *ctx = data;
    char cmd[512]; FILE *fp;
    guint grand_total = 0;

    GHashTable *pacman_ht  = build_pacman_installed();
    GHashTable *flatpak_ht = build_flatpak_installed();

    gboolean do_aur     = ctx->use_aur     && system("which yay >/dev/null 2>&1") == 0;
    gboolean do_flatpak = ctx->use_flatpak && system("which flatpak >/dev/null 2>&1") == 0;

    int remaining = (ctx->use_pacman ? 1 : 0) + (do_aur ? 1 : 0) + (do_flatpak ? 1 : 0);

    if (remaining == 0) {
        dispatch_batch(g_array_new(FALSE, TRUE, sizeof(Pkg)),
                       ctx->query, TRUE, 0);
        goto cleanup;
    }

    if (ctx->use_pacman) {
        GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
        snprintf(cmd, sizeof(cmd), "pacman -Ss '%s' 2>/dev/null", ctx->query);
        fp = popen(cmd, "r");
        if (fp) { parse_pacman_output(fp, pkgs, "pacman", "sudo pacman -S"); pclose(fp); }
        filter_exact(pkgs, ctx->query);
        for (guint i = 0; i < pkgs->len; i++) {
            Pkg *p = &g_array_index(pkgs, Pkg, i);
            p->installed = g_hash_table_contains(pacman_ht, p->name);
        }
        grand_total += pkgs->len;
        remaining--;
        dispatch_batch(pkgs, ctx->query, remaining == 0, grand_total);
    }

    if (do_aur) {
        GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
        snprintf(cmd, sizeof(cmd), "yay --color=never -Ss --aur '%s' 2>/dev/null", ctx->query);
        fp = popen(cmd, "r");
        if (fp) { parse_pacman_output(fp, pkgs, "aur", "yay -S"); pclose(fp); }
        filter_exact(pkgs, ctx->query);
        for (guint i = 0; i < pkgs->len; i++) {
            Pkg *p = &g_array_index(pkgs, Pkg, i);
            p->installed = g_hash_table_contains(pacman_ht, p->name);
        }
        grand_total += pkgs->len;
        remaining--;
        dispatch_batch(pkgs, ctx->query, remaining == 0, grand_total);
    }

    if (do_flatpak) {
        GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
        snprintf(cmd, sizeof(cmd), "flatpak search '%s' 2>/dev/null", ctx->query);
        fp = popen(cmd, "r");
        if (fp) { parse_flatpak_output(fp, pkgs); pclose(fp); }
        filter_exact(pkgs, ctx->query);
        for (guint i = 0; i < pkgs->len; i++) {
            Pkg *p = &g_array_index(pkgs, Pkg, i);
            char *appid = strrchr(p->cmd, ' ');
            if (appid) appid++;
            p->installed = appid && g_hash_table_contains(flatpak_ht, appid);
        }
        grand_total += pkgs->len;
        remaining--;
        dispatch_batch(pkgs, ctx->query, remaining == 0, grand_total);
    }

cleanup:
    g_hash_table_destroy(pacman_ht);
    g_hash_table_destroy(flatpak_ht);
    g_free(ctx);
    return NULL;
}

static void on_toggle(GtkCellRendererToggle *cell, gchar *path_str, gpointer d) {
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    gboolean val;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(g_store), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(g_store), &iter, COL_CHECK, &val, -1);
    gtk_list_store_set(g_store, &iter, COL_CHECK, !val, -1);
    gtk_tree_path_free(path);
}

static void run_in_alacritty(GString *script, const char *status_msg) {
    char done_line[128];
    snprintf(done_line, sizeof(done_line),
             "; echo; echo '%s'; read", T(STR_ALACRITTY_DONE));
    g_string_append(script, done_line);
    char *argv[] = { "alacritty", "-e", "sh", "-c", script->str, NULL };
    GError *err = NULL;
    if (!g_spawn_async(NULL, argv, NULL, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, &err)) {
        char msg[256];
        snprintf(msg, sizeof(msg), T(STR_ALACRITTY_ERR), err ? err->message : "?");
        gtk_label_set_text(GTK_LABEL(g_status), msg);
        if (err) g_error_free(err);
    } else {
        gtk_label_set_text(GTK_LABEL(g_status), status_msg);
    }
}

static void on_install(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(g_store);
    GtkTreeIter iter; int count = 0;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;

    GString *pacman_pkgs   = g_string_new("");
    GString *aur_pkgs      = g_string_new("");
    GString *flatpak_fh    = g_string_new("");  
    GString *flatpak_other = g_string_new("");
    int aur_count = 0;

    static const char PFX_PACMAN[]  = "sudo pacman -S ";
    static const char PFX_AUR[]     = "yay -S ";
    static const char PFX_FLAT_FH[] = "flatpak install flathub ";
    static const char PFX_FLAT[]    = "flatpak install ";

    do {
        gboolean checked, installed; gchar *pkg_cmd = NULL;
        gtk_tree_model_get(model, &iter,
            COL_CHECK, &checked, COL_INSTALLED, &installed, COL_CMD, &pkg_cmd, -1);
        if (checked && !installed && pkg_cmd && pkg_cmd[0]) {
            count++;
            if (g_str_has_prefix(pkg_cmd, PFX_PACMAN)) {
                if (pacman_pkgs->len) g_string_append_c(pacman_pkgs, ' ');
                g_string_append(pacman_pkgs, pkg_cmd + strlen(PFX_PACMAN));
            } else if (g_str_has_prefix(pkg_cmd, PFX_AUR)) {
                if (aur_pkgs->len) g_string_append_c(aur_pkgs, ' ');
                g_string_append(aur_pkgs, pkg_cmd + strlen(PFX_AUR));
                aur_count++;
            } else if (g_str_has_prefix(pkg_cmd, PFX_FLAT_FH)) {
                if (flatpak_fh->len) g_string_append_c(flatpak_fh, ' ');
                g_string_append(flatpak_fh, pkg_cmd + strlen(PFX_FLAT_FH));
            } else if (g_str_has_prefix(pkg_cmd, PFX_FLAT)) {
                if (flatpak_other->len) g_string_append_c(flatpak_other, ' ');
                g_string_append(flatpak_other, pkg_cmd + strlen(PFX_FLAT));
            }
        }
        g_free(pkg_cmd);
    } while (gtk_tree_model_iter_next(model, &iter));

    if (count == 0) {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_MARK_INSTALL));
        g_string_free(pacman_pkgs,   TRUE);
        g_string_free(aur_pkgs,      TRUE);
        g_string_free(flatpak_fh,    TRUE);
        g_string_free(flatpak_other, TRUE);
        return;
    }

    GString *script = g_string_new(""); gboolean first = TRUE;
    if (pacman_pkgs->len) {
        g_string_append_printf(script, "sudo pacman -S %s", pacman_pkgs->str);
        first = FALSE;
    }
    if (aur_pkgs->len) {
        if (!first) g_string_append(script, " && ");
        if (aur_count > 2)
            g_string_append_printf(script, "yay -S --noconfirm %s", aur_pkgs->str);
        else
            g_string_append_printf(script, "yay -S %s", aur_pkgs->str);
        first = FALSE;
    }
    if (flatpak_fh->len) {
        if (!first) g_string_append(script, " && ");
        g_string_append_printf(script, "flatpak install flathub %s", flatpak_fh->str);
        first = FALSE;
    }
    if (flatpak_other->len) {
        if (!first) g_string_append(script, " && ");
        g_string_append_printf(script, "flatpak install %s", flatpak_other->str);
    }

    g_string_free(pacman_pkgs,   TRUE);
    g_string_free(aur_pkgs,      TRUE);
    g_string_free(flatpak_fh,    TRUE);
    g_string_free(flatpak_other, TRUE);

    char msg[64]; snprintf(msg, sizeof(msg), T(STR_INSTALLING), count);
    run_in_alacritty(script, msg);
    g_string_free(script, TRUE);
}

static void on_remove(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(g_store);
    GtkTreeIter iter; int count = 0;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;

    GString *pacman_pkgs  = g_string_new("");
    GString *flatpak_pkgs = g_string_new("");

    static const char PFX_PACMAN[]  = "sudo pacman -Rns ";
    static const char PFX_FLATPAK[] = "flatpak uninstall ";

    do {
        gboolean checked, installed; gchar *rem_cmd = NULL;
        gtk_tree_model_get(model, &iter,
            COL_CHECK, &checked, COL_INSTALLED, &installed, COL_REMOVE_CMD, &rem_cmd, -1);
        if (checked && installed && rem_cmd && rem_cmd[0]) {
            count++;
            if (g_str_has_prefix(rem_cmd, PFX_PACMAN)) {
                if (pacman_pkgs->len) g_string_append_c(pacman_pkgs, ' ');
                g_string_append(pacman_pkgs, rem_cmd + strlen(PFX_PACMAN));
            } else if (g_str_has_prefix(rem_cmd, PFX_FLATPAK)) {
                if (flatpak_pkgs->len) g_string_append_c(flatpak_pkgs, ' ');
                g_string_append(flatpak_pkgs, rem_cmd + strlen(PFX_FLATPAK));
            }
        }
        g_free(rem_cmd);
    } while (gtk_tree_model_iter_next(model, &iter));

    if (count == 0) {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_MARK_REMOVE));
        g_string_free(pacman_pkgs,  TRUE);
        g_string_free(flatpak_pkgs, TRUE);
        return;
    }

    GString *script = g_string_new(""); gboolean first = TRUE;
    if (pacman_pkgs->len) {
        g_string_append_printf(script, "sudo pacman -Rns %s", pacman_pkgs->str);
        first = FALSE;
    }
    if (flatpak_pkgs->len) {
        if (!first) g_string_append(script, " && ");
        g_string_append_printf(script, "flatpak uninstall %s", flatpak_pkgs->str);
    }

    g_string_free(pacman_pkgs,  TRUE);
    g_string_free(flatpak_pkgs, TRUE);

    char msg[64]; snprintf(msg, sizeof(msg), T(STR_REMOVING), count);
    run_in_alacritty(script, msg);
    g_string_free(script, TRUE);
}

static void on_search(GtkWidget *w, gpointer d) {
    const char *query = gtk_entry_get_text(GTK_ENTRY(g_entry));
    if (!query || !query[0]) return;
    gtk_widget_set_sensitive(g_btn_search,  FALSE);
    gtk_widget_set_sensitive(g_btn_install, FALSE);
    gtk_widget_set_sensitive(g_btn_remove,  FALSE);
    gtk_list_store_clear(g_store);
    gtk_label_set_text(GTK_LABEL(g_status), T(STR_SEARCHING));
    gtk_spinner_start(GTK_SPINNER(g_spinner));
    SearchCtx *ctx   = g_new0(SearchCtx, 1);
    ctx->use_pacman  = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_pacman));
    ctx->use_aur     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_aur));
    ctx->use_flatpak = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_flatpak));
    strncpy(ctx->query, query, sizeof(ctx->query)-1);
    GThread *t = g_thread_new("pkg-search", search_thread, ctx);
    g_thread_unref(t);
}

static void on_entry_activate(GtkWidget *w, gpointer d) { on_search(NULL, NULL); }

static void on_row_activated(GtkTreeView *tv, GtkTreePath *path,
                             GtkTreeViewColumn *col, gpointer d) {
    GtkTreeIter iter; gboolean val;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(g_store), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(g_store), &iter, COL_CHECK, &val, -1);
    gtk_list_store_set(g_store, &iter, COL_CHECK, !val, -1);
}

static void apply_lang(void) {
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_entry), T(STR_PLACEHOLDER));
    gtk_button_set_label(GTK_BUTTON(g_btn_search),  T(STR_BTN_SEARCH));

    gtk_label_set_text(GTK_LABEL(g_label_sources), T(STR_SOURCES_LABEL));
    gtk_button_set_label(GTK_BUTTON(g_chk_pacman), T(STR_PACMAN_CHECK));

    gtk_tree_view_column_set_title(g_col_status_w,  T(STR_COL_STATUS));
    gtk_tree_view_column_set_title(g_col_source_w,  T(STR_COL_SOURCE));
    gtk_tree_view_column_set_title(g_col_name_w,    T(STR_COL_NAME));
    gtk_tree_view_column_set_title(g_col_version_w, T(STR_COL_VERSION));
    gtk_tree_view_column_set_title(g_col_desc_w,    T(STR_COL_DESC));

    gtk_button_set_label(GTK_BUTTON(g_btn_remove),  T(STR_BTN_REMOVE));
    gtk_button_set_label(GTK_BUTTON(g_btn_install), T(STR_BTN_INSTALL));
    gtk_widget_set_tooltip_text(g_btn_remove,  T(STR_TOOLTIP_REMOVE));
    gtk_widget_set_tooltip_text(g_btn_install, T(STR_TOOLTIP_INSTALL));
    gtk_button_set_label(GTK_BUTTON(g_btn_update), T(STR_BTN_CHECK_UPDATES));
    gtk_widget_set_tooltip_text(g_btn_update, T(STR_TOOLTIP_UPDATE));
    gtk_button_set_label(GTK_BUTTON(g_btn_changelog), T(STR_BTN_CHANGELOG));
    gtk_widget_set_tooltip_text(g_btn_changelog, T(STR_BTN_CHANGELOG));

    const char *cur = gtk_label_get_text(GTK_LABEL(g_status));
    if (strcmp(cur, g_strings[STR_STATUS_READY][LANG_ES]) == 0 ||
        strcmp(cur, g_strings[STR_STATUS_READY][LANG_EN]) == 0)
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_STATUS_READY));

    GtkTreeModel *model = GTK_TREE_MODEL(g_store);
    GtkTreeIter iter;
    if (gtk_tree_model_get_iter_first(model, &iter)) {
        do {
            gboolean installed;
            gtk_tree_model_get(model, &iter, COL_INSTALLED, &installed, -1);
            gtk_list_store_set(g_store, &iter,
                COL_STATUS, installed ? T(STR_INSTALLED) : "", -1);
        } while (gtk_tree_model_iter_next(model, &iter));
    }
}

static void on_lang_changed(GtkComboBox *combo, gpointer d) {
    g_lang = (LangID)gtk_combo_box_get_active(combo);
    apply_lang();
    save_lang_pref();
}

static gboolean on_window_destroy(GtkWidget *w, GdkEvent *event, gpointer d) {
    save_lang_pref();   
    gtk_main_quit();
    return FALSE;
}

#define UPD_URL_BIN  "https://raw.githubusercontent.com/humrand/" \
                     "arch-installation-easy/main/SourceCode/pkg-helper"
#define UPD_URL_ICON "https://raw.githubusercontent.com/humrand/" \
                     "arch-installation-easy/main/SourceCode/images/pkg.png"
#define UPD_BIN_DST  "/usr/local/bin/pkg-helper"
#define UPD_ICO_DST  "/usr/share/icons/hicolor/256x256/apps/pkg-helper.png"
#define UPD_TMP_BIN  "/tmp/.pkg-helper.upd"
#define UPD_TMP_ICO  "/tmp/.pkg-helper-icon.upd"

typedef struct {
    gboolean bin_updated;
    gboolean icon_updated;
    gboolean any_error;
    char     self_path[512];
    char     res_file[256];
} UpdResult;

static char *sha256_of(const char *path) {
    char cmd[600];
    snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    char line[128] = {0};
    fgets(line, sizeof(line), fp);
    pclose(fp);
    char *sp = strchr(line, ' ');
    if (sp) *sp = '\0';
    char *nl = strchr(line, '\n');
    if (nl) *nl = '\0';
    return (strlen(line) == 64) ? g_strdup(line) : NULL;
}

static gboolean restart_cb(gpointer data) {
    char *path = data;
    char *argv[] = { (path && path[0]) ? path : (char *)UPD_BIN_DST, NULL };
    GError *err = NULL;
    g_spawn_async(NULL, argv, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, &err);
    if (err) g_error_free(err);
    g_free(path);
    gtk_main_quit();
    return G_SOURCE_REMOVE;
}

static void on_pkexec_done(GPid pid, gint status, gpointer data) {
    g_spawn_close_pid(pid);
    UpdResult *r = data;
    unlink("/tmp/.pkg-helper-upd.sh");
    unlink(UPD_TMP_BIN);
    unlink(UPD_TMP_ICO);

    gboolean ok = FALSE;
    {
        FILE *rf = fopen(r->res_file, "r");
        if (rf) {
            char buf[8] = {0};
            fgets(buf, sizeof(buf), rf);
            fclose(rf);
            unlink(r->res_file);
            ok = (strncmp(buf, "ok", 2) == 0);
        }
    }

    if (ok) {
        if (r->bin_updated && r->icon_updated)
            gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATED_BOTH));
        else if (r->icon_updated) {
            gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATED_ICON));
            GdkPixbuf *pb = gdk_pixbuf_new_from_file(UPD_ICO_DST, NULL);
            if (pb) { gtk_window_set_icon(GTK_WINDOW(g_win), pb); g_object_unref(pb); }
        } else {
            gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATED_BIN));
        }
        if (r->bin_updated) {
            gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATE_RESTART));
            g_timeout_add(2000, restart_cb, g_strdup(r->self_path));
        }
    } else {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATE_FAILED));
    }
    if (g_btn_update) gtk_widget_set_sensitive(g_btn_update, TRUE);
    g_free(r);
}

static gboolean upd_notify_idle(gpointer data) {
    UpdResult *r = data;

    if (!r->bin_updated && !r->icon_updated) {
        gtk_label_set_text(GTK_LABEL(g_status),
            r->any_error ? T(STR_UPDATE_FAILED) : T(STR_UP_TO_DATE));
        if (g_btn_update) gtk_widget_set_sensitive(g_btn_update, TRUE);
        g_free(r);
        return G_SOURCE_REMOVE;
    }

    GString *sh = g_string_new("");
    if (r->bin_updated)
        g_string_append_printf(sh,
            "rm -f '%s' && cp '%s' '%s' && chmod 755 '%s'",
            r->self_path, UPD_TMP_BIN, r->self_path, r->self_path);
    
    if (r->icon_updated) {
        if (r->bin_updated) g_string_append(sh, " && ");
        g_string_append_printf(sh,
            "mkdir -p '%s' && rm -f '%s' && cp '%s' '%s' && chmod 644 '%s'",
            "/usr/share/icons/hicolor/256x256/apps",
            UPD_ICO_DST, UPD_TMP_ICO, UPD_ICO_DST, UPD_ICO_DST);
    }

    {
        FILE *sf = fopen("/tmp/.pkg-helper-upd.sh", "w");
        if (sf) {
            fprintf(sf,
                "#!/bin/sh\n"
                "rm -f '%s'\n"
                "%s \\\n"
                "  && echo ok   > '%s' \\\n"
                "  || echo fail > '%s'\n"
                "chmod 666 '%s' 2>/dev/null\n"
                "echo\n"
                "echo '--- %s ---'\n"
                "read\n",
                r->res_file,
                sh->str, r->res_file, r->res_file, r->res_file, T(STR_ALACRITTY_DONE));
            fclose(sf);
            chmod("/tmp/.pkg-helper-upd.sh", 0755);
        }
    }
    g_string_free(sh, TRUE);

    char *argv[] = { "alacritty", "-e", "sudo",
                     "/tmp/.pkg-helper-upd.sh", NULL };
    GPid  pid;
    GError *err = NULL;

    gtk_label_set_text(GTK_LABEL(g_status), T(STR_APPLYING_UPDATE));

    if (g_spawn_async(NULL, argv, NULL,
            G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
            NULL, NULL, &pid, &err)) {
        g_child_watch_add(pid, on_pkexec_done, r);
    } else {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_UPDATE_FAILED));
        if (err) g_error_free(err);
        unlink("/tmp/.pkg-helper-upd.sh");
        unlink(UPD_TMP_BIN);
        unlink(UPD_TMP_ICO);
        if (g_btn_update) gtk_widget_set_sensitive(g_btn_update, TRUE);
        g_free(r);
    }
    return G_SOURCE_REMOVE;
}

static gboolean upd_checking_idle(gpointer data) {
    (void)data;
    gtk_label_set_text(GTK_LABEL(g_status), T(STR_CHECKING_UPDATES));
    if (g_btn_update) gtk_widget_set_sensitive(g_btn_update, FALSE);
    return G_SOURCE_REMOVE;
}

static gpointer update_check_thread(gpointer data) {
    (void)data;
    UpdResult *r = g_new0(UpdResult, 1);
    
    snprintf(r->res_file, sizeof(r->res_file), "/tmp/.pkg-helper-res-%d", getpid());

    g_idle_add(upd_checking_idle, NULL);

    {
        char cmd[700];
        snprintf(cmd, sizeof(cmd),
                 "curl -fsSL --max-time 30 -o '%s' '%s' 2>/dev/null",
                 UPD_TMP_BIN, UPD_URL_BIN);
        if (system(cmd) == 0) {
            strncpy(r->self_path, UPD_BIN_DST, sizeof(r->self_path)-1);
            {
                char tmp[512] = {0};
                ssize_t n = readlink("/proc/self/exe", tmp, sizeof(tmp)-1);
                if (n > 0) { tmp[n] = '\0'; strncpy(r->self_path, tmp, sizeof(r->self_path)-1); }
            }
            char *sha_local  = sha256_of(r->self_path);
            char *sha_remote = sha256_of(UPD_TMP_BIN);
            
            if (sha_remote && (!sha_local || strcmp(sha_local, sha_remote) != 0))
                r->bin_updated = TRUE;
            else if (!sha_remote)
                r->any_error = TRUE;
                
            g_free(sha_local);
            g_free(sha_remote);
            if (!r->bin_updated) unlink(UPD_TMP_BIN);
        } else {
            r->any_error = TRUE;
        }
    }

    {
        char cmd[700];
        snprintf(cmd, sizeof(cmd),
                 "curl -fsSL --max-time 20 -o '%s' '%s' 2>/dev/null",
                 UPD_TMP_ICO, UPD_URL_ICON);
        if (system(cmd) == 0) {
            char *sha_local  = sha256_of(UPD_ICO_DST);
            char *sha_remote = sha256_of(UPD_TMP_ICO);
            
            if (sha_remote && (!sha_local || strcmp(sha_local, sha_remote) != 0))
                r->icon_updated = TRUE;
                
            g_free(sha_local);
            g_free(sha_remote);
            if (!r->icon_updated) unlink(UPD_TMP_ICO);
        } else {
            r->any_error = TRUE;
        }
    }

    g_idle_add(upd_notify_idle, r);
    return NULL;
}

static void launch_update_check(void) {
    GThread *t = g_thread_new("pkg-update", update_check_thread, NULL);
    g_thread_unref(t);
}

static void on_check_updates(GtkWidget *w, gpointer d) {
    gtk_widget_set_sensitive(g_btn_update, FALSE);
    launch_update_check();
}

static void show_changelog_dialog(GtkWidget *parent) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        T(STR_CHANGELOG_TITLE),
        GTK_WINDOW(parent),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_OK", GTK_RESPONSE_OK,
        NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 560, 420);
    gtk_window_set_resizable(GTK_WINDOW(dlg), TRUE);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 12);
    gtk_box_set_spacing(GTK_BOX(content), 8);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv),  10);
    gtk_text_view_set_right_margin(GTK_TEXT_VIEW(tv), 10);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv),    6);
    gtk_text_view_set_bottom_margin(GTK_TEXT_VIEW(tv), 6);

    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

    GtkTextTag *tag_ver = gtk_text_buffer_create_tag(buf, "version",
        "weight", PANGO_WEIGHT_BOLD,
        "scale",  1.15,
        NULL);
    GtkTextTag *tag_item = gtk_text_buffer_create_tag(buf, "item",
        "left-margin", 16,
        NULL);
    (void)tag_ver; (void)tag_item;

    typedef struct { const char *ver; const char *date; const char *body_es; const char *body_en; } Entry;
    static const Entry entries[] = {

    
        {
            "v0.0.5-beta", "21 april 2026",
            "• boton de actualizacion de sistema y paquetes.\n",
            "• system update button and packages updater.\n"
        },
        {
            "v0.0.4-beta", "21 april 2026",
            "• mejora de rendimiento en yay.\n",
            "• performance improvement in yay.\n"
        },
        {
            "v0.0.3-beta", "21 april 2026",
            "• Rendimiento mejorado: todos los paquetes se descargan en paralelo en lugar de uno por uno.\n"
            "• La interfaz ya respeta el idioma configurado al mostrar el historial de cambios.\n",
            "• Performance improvement: all packages are now downloaded in parallel instead of one by one.\n"
            "• The interface now respects the configured language when displaying the changelog.\n"
        },
        {
            "v0.0.2-beta", "20 april 2026",
            "• Corrección de crash al buscar con fuentes desactivadas.\n",
            "• Fixed crash when searching with all sources disabled.\n"
        },
        {
            "v0.0.1-beta", "20 april 2026",
            "• Versión inicial de PKG Helper.\n"
            "• Búsqueda de paquetes vía flatpak yay y pacman.\n"
            "• Instalación y eliminación de paquetes a través de Alacritty.\n",
            "• Initial release of PKG Helper.\n"
            "• Package search via flatpak yay and pacman.\n"
            "• Package install and remove through Alacritty.\n"
        },
    };

    GtkTextIter it;
    gtk_text_buffer_get_start_iter(buf, &it);

    for (int i = 0; i < (int)(sizeof(entries)/sizeof(entries[0])); i++) {
        const Entry *e = &entries[i];
        char header[128];
        snprintf(header, sizeof(header), "%s  (%s)\n", e->ver, e->date);
        gtk_text_buffer_insert_with_tags_by_name(buf, &it, header, -1, "version", NULL);
        const char *body = (g_lang == LANG_EN) ? e->body_en : e->body_es;
        gtk_text_buffer_insert_with_tags_by_name(buf, &it, body, -1, "item", NULL);
        if (i < (int)(sizeof(entries)/sizeof(entries[0])) - 1)
            gtk_text_buffer_insert(buf, &it, "\n", -1);
    }

    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_changelog_clicked(GtkWidget *w, gpointer d) {
    show_changelog_dialog(g_win);
}

static void on_update_sys(GtkWidget *w, gpointer d) {
    GString *script = g_string_new("sudo pacman -Syu");
    run_in_alacritty(script, T(STR_TOOLTIP_UPDATE_SYS));
    g_string_free(script, TRUE);
}

static void on_update_all(GtkWidget *w, gpointer d) {
    GString *script = g_string_new("sudo pacman -Syu && yay -Syu && flatpak update");
    run_in_alacritty(script, T(STR_TOOLTIP_UPDATE_ALL));
    g_string_free(script, TRUE);
}

static void build_ui(void) {
    g_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_win), "PKG Helper - Arch Linux");
    gtk_window_set_default_size(GTK_WINDOW(g_win), 920, 580);
    gtk_window_set_position(GTK_WINDOW(g_win), GTK_WIN_POS_CENTER);
    g_signal_connect(g_win, "delete-event", G_CALLBACK(on_window_destroy), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(g_win), vbox);

    GtkWidget *hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_entry), T(STR_PLACEHOLDER));
    g_signal_connect(g_entry, "activate", G_CALLBACK(on_entry_activate), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_entry, TRUE, TRUE, 0);

    g_btn_search = gtk_button_new_with_label(T(STR_BTN_SEARCH));
    g_signal_connect(g_btn_search, "clicked", G_CALLBACK(on_search), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_search, FALSE, FALSE, 0);

    g_spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(hbox_top), g_spinner, FALSE, FALSE, 4);

    g_lang_combo = gtk_combo_box_text_new();
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_lang_combo), "ES");
    gtk_combo_box_text_append_text(GTK_COMBO_BOX_TEXT(g_lang_combo), "EN");
    gtk_combo_box_set_active(GTK_COMBO_BOX(g_lang_combo), (int)g_lang);
    gtk_widget_set_tooltip_text(g_lang_combo, "Idioma / Language");
    g_signal_connect(g_lang_combo, "changed", G_CALLBACK(on_lang_changed), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_lang_combo, FALSE, FALSE, 0);

    g_btn_update = gtk_button_new_with_label(T(STR_BTN_CHECK_UPDATES));
    gtk_widget_set_tooltip_text(g_btn_update, T(STR_TOOLTIP_UPDATE));
    g_signal_connect(g_btn_update, "clicked", G_CALLBACK(on_check_updates), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_update, FALSE, FALSE, 0);

    g_btn_changelog = gtk_button_new_with_label(T(STR_BTN_CHANGELOG));
    gtk_widget_set_tooltip_text(g_btn_changelog, T(STR_BTN_CHANGELOG));
    g_signal_connect(g_btn_changelog, "clicked", G_CALLBACK(on_changelog_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_changelog, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, 0);

    GtkWidget *hbox_src = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 16);
    g_label_sources = gtk_label_new(T(STR_SOURCES_LABEL));
    gtk_box_pack_start(GTK_BOX(hbox_src), g_label_sources, FALSE, FALSE, 0);
    g_chk_pacman  = gtk_check_button_new_with_label(T(STR_PACMAN_CHECK));
    g_chk_aur     = gtk_check_button_new_with_label("AUR (yay)");
    g_chk_flatpak = gtk_check_button_new_with_label("Flatpak");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_chk_pacman),  TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_chk_aur),     TRUE);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(g_chk_flatpak), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox_src), g_chk_pacman,  FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_src), g_chk_aur,     FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox_src), g_chk_flatpak, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), hbox_src, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    g_store = gtk_list_store_new(N_COLS,
        G_TYPE_BOOLEAN,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN);

    g_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_store));
    g_object_unref(g_store);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(g_tree), FALSE);
    g_signal_connect(g_tree, "row-activated", G_CALLBACK(on_row_activated), NULL);

    GtkCellRenderer *toggle_r = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggle_r, "toggled", G_CALLBACK(on_toggle), NULL);
    GtkTreeViewColumn *toggle_col = gtk_tree_view_column_new_with_attributes(
        "", toggle_r, "active", COL_CHECK, NULL);
    gtk_tree_view_column_set_min_width(toggle_col, 30);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_tree), toggle_col);

    {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        g_object_set(r, "foreground", "#44aa44", NULL);
        g_col_status_w = gtk_tree_view_column_new_with_attributes(
            T(STR_COL_STATUS), r, "text", COL_STATUS, NULL);
        gtk_tree_view_column_set_min_width(g_col_status_w, 90);
        gtk_tree_view_append_column(GTK_TREE_VIEW(g_tree), g_col_status_w);
    }

    {
        struct { int id; const char *title; int min_w; gboolean expand;
                 GtkTreeViewColumn **ref; } cols[] = {
            { COL_SOURCE,  T(STR_COL_SOURCE),  80,  FALSE, &g_col_source_w  },
            { COL_NAME,    T(STR_COL_NAME),    160, FALSE, &g_col_name_w    },
            { COL_VERSION, T(STR_COL_VERSION), 90,  FALSE, &g_col_version_w },
            { COL_DESC,    T(STR_COL_DESC),    200, TRUE,  &g_col_desc_w    },
        };
        for (int i=0; i<4; i++) {
            GtkCellRenderer *r = gtk_cell_renderer_text_new();
            GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                cols[i].title, r, "text", cols[i].id, NULL);
            gtk_tree_view_column_set_resizable(c, TRUE);
            gtk_tree_view_column_set_sort_column_id(c, cols[i].id);
            gtk_tree_view_column_set_min_width(c, cols[i].min_w);
            if (cols[i].expand) {
                gtk_tree_view_column_set_expand(c, TRUE);
                g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
            }
            *cols[i].ref = c;
            gtk_tree_view_append_column(GTK_TREE_VIEW(g_tree), c);
        }
    }

    GtkTreeSelection *sel = gtk_tree_view_get_selection(GTK_TREE_VIEW(g_tree));
    gtk_tree_selection_set_mode(sel, GTK_SELECTION_SINGLE);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), g_tree);
    gtk_box_pack_start(GTK_BOX(vbox), scroll, TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    GtkWidget *hbox_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_status = gtk_label_new(T(STR_STATUS_READY));
    gtk_label_set_xalign(GTK_LABEL(g_status), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(g_status), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox_bot), g_status, TRUE, TRUE, 0);

    g_ver_label = gtk_label_new(APP_VERSION);
    gtk_widget_set_opacity(g_ver_label, 0.35);
    gtk_widget_set_tooltip_text(g_ver_label, T(STR_BTN_CHANGELOG));
    gtk_box_pack_start(GTK_BOX(hbox_bot), g_ver_label, FALSE, FALSE, 4);

    g_btn_remove = gtk_button_new_with_label(T(STR_BTN_REMOVE));
    gtk_widget_set_tooltip_text(g_btn_remove, T(STR_TOOLTIP_REMOVE));
    g_signal_connect(g_btn_remove, "clicked", G_CALLBACK(on_remove), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_bot), g_btn_remove, FALSE, FALSE, 0);

    g_btn_install = gtk_button_new_with_label(T(STR_BTN_INSTALL));
    gtk_widget_set_tooltip_text(g_btn_install, T(STR_TOOLTIP_INSTALL));
    g_signal_connect(g_btn_install, "clicked", G_CALLBACK(on_install), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_bot), g_btn_install, FALSE, FALSE, 0);

    g_btn_update_sys = gtk_button_new_with_label(T(STR_BTN_UPDATE_SYS));
    gtk_widget_set_tooltip_text(g_btn_update_sys, T(STR_TOOLTIP_UPDATE_SYS));
    g_signal_connect(g_btn_update_sys, "clicked", G_CALLBACK(on_update_sys), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_bot), g_btn_update_sys, FALSE, FALSE, 0);

    g_btn_update_all = gtk_button_new_with_label(T(STR_BTN_UPDATE_ALL));
    gtk_widget_set_tooltip_text(g_btn_update_all, T(STR_TOOLTIP_UPDATE_ALL));
    g_signal_connect(g_btn_update_all, "clicked", G_CALLBACK(on_update_all), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_bot), g_btn_update_all, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox_bot, FALSE, FALSE, 0);
}

int main(int argc, char *argv[]) {
    gtk_init(&argc, &argv);

    load_lang_pref();   

    build_ui();

    GdkPixbuf *win_icon = gdk_pixbuf_new_from_file(UPD_ICO_DST, NULL);
    if (win_icon) {
        gtk_window_set_icon(GTK_WINDOW(g_win), win_icon);
        g_object_unref(win_icon);
    }

    gtk_widget_show_all(g_win);
    gtk_main();
    return 0;
}
