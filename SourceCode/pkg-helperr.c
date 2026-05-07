#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <errno.h>

#define MAX_NAME    256
#define MAX_VER      64
#define MAX_DESC    512
#define MAX_CMD     512
#define MAX_LINE   1024

#define APP_VERSION "1.2.0-stable"

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
    STR_DOWNLOADING,
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
    STR_BTN_DARK_MODE,
    STR_TOOLTIP_DARK_MODE,
    STR_TAB_SEARCH,
    STR_TAB_INSTALLED,
    STR_BTN_REFRESH_INSTALLED,
    STR_BTN_SELECT_ALL_PAGE,
    STR_WHATS_NEW_TITLE,
    STR_OPTIONS_MENU,
    STR_OPT_SHOW_WHATS_NEW,
    STR_OPT_LANGUAGE,
    STR_OPT_DARK_MODE,
    STR_OPT_CHECK_UPDATES,
    STR_OPT_CLOSE,
    STR_INST_LOADING,
    STR_INST_TOTAL,
    STR_FILTER_PLACEHOLDER,
    STR_CONFIRM_TITLE_INSTALL,
    STR_CONFIRM_TITLE_REMOVE,
    STR_CONFIRM_MSG_INSTALL,
    STR_CONFIRM_MSG_REMOVE,
    STR_PKG_INFO_TITLE,
    STR_NO_TERMINAL,
    STR_SYS_UPDATES_BADGE,
    STR_TAB_ORPHANS,
    STR_BTN_REMOVE_ORPHANS,
    STR_TOOLTIP_REMOVE_ORPHANS,
    STR_ORPHANS_LOADING,
    STR_ORPHANS_NONE,
    STR_ORPHANS_FOUND,
    STR_BTN_CLEAN_CACHE,
    STR_TOOLTIP_CLEAN_CACHE,
    STR_COL_SIZE,
    STR_DEPS_WARNING,
    STR_BTN_PREV,
    STR_BTN_NEXT,
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
    { "Descargando %d paquete(s)...",
      "Downloading %d package(s)..."          },
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
    { "Modo oscuro",
      "Dark mode"                                       },
    { "Activar/desactivar modo oscuro",
      "Toggle dark mode"                                },
    { "Buscar",
      "Search"                                          },
    { "Instalados",
      "Installed"                                       },
    { "Actualizar lista",
      "Refresh list"                                    },
    { "Seleccionar página",
      "Select page"                                     },
    { "Novedades de esta versión",
      "What's new in this version"                      },
    { "Opciones",
      "Options"                                         },
    { "Mostrar novedades al actualizar",
      "Show what's new after update"                    },
    { "Idioma / Language",
      "Language"                                        },
    { "Modo oscuro",
      "Dark mode"                                       },
    { "Buscar actualizaciones",
      "Check for updates"                               },
    { "Cerrar",
      "Close"                                           },
    { "Cargando paquetes instalados...",
      "Loading installed packages..."                   },
    { "Total: %d paquetes instalados",
      "Total: %d packages installed"                    },
    { "Filtrar instalados...",
      "Filter installed..."                             },
    { "Confirmar instalación",
      "Confirm install"                                 },
    { "Confirmar eliminación",
      "Confirm removal"                                 },
    { "¿Instalar los siguientes paquetes?\n\n%s",
      "Install the following packages?\n\n%s"           },
    { "¿Eliminar los siguientes paquetes?\n\n%s",
      "Remove the following packages?\n\n%s"            },
    { "Información del paquete — %s",
      "Package info — %s"                               },
    { "No se encontró ningún terminal compatible.",
      "No compatible terminal emulator found."          },
    { "Actualizar sistema (%d)",
      "Update system (%d)"                              },
    { "Huérfanos",
      "Orphans"                                         },
    { "Eliminar huérfanos",
      "Remove orphans"                                  },
    { "pacman -Rns $(pacman -Qdtq)",
      "pacman -Rns $(pacman -Qdtq)"                    },
    { "Cargando paquetes huérfanos...",
      "Loading orphan packages..."                      },
    { "No hay paquetes huérfanos.",
      "No orphan packages found."                       },
    { "%d paquete(s) huérfano(s)",
      "%d orphan package(s)"                            },
    { "Limpiar caché",
      "Clean cache"                                     },
    { "paccache -r (elimina versiones antiguas)",
      "paccache -r (removes old versions)"              },
    { "Tamaño",
      "Size"                                            },
    { "⚠ Dependencias inversas detectadas:\n%s\nEstos paquetes dependen de lo que vas a eliminar.\n¿Continuar de todas formas?",
      "⚠ Reverse dependencies detected:\n%s\nThese packages depend on what you are removing.\nContinue anyway?"  },
    { "◀  Anterior",
      "◀  Previous"                                         },
    { "Siguiente  ▶",
      "Next  ▶"                                             },
};

#define T(id) g_strings[(id)][g_lang]

static char *config_path(const char *filename) {
    const char *home = g_get_home_dir();
    return g_build_filename(home, ".config", "pkg-helper", filename, NULL);
}

static void save_lang_pref(void) {
    char *path = config_path("lang");
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
    char *path = config_path("lang");
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

static gboolean       g_dark_mode         = FALSE;
static gboolean       g_dark_mode_setting = FALSE;
static GtkWidget     *g_btn_dark_mode     = NULL;
static GtkCssProvider *g_css_dark         = NULL;

static void save_dark_pref(void) {
    char *path = config_path("darkmode");
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(g_dark_mode ? "1" : "0", fp);
        fclose(fp);
    }
    g_free(dir);
    g_free(path);
}

static void load_dark_pref(void) {
    char *path = config_path("darkmode");
    FILE *fp = fopen(path, "r");
    if (fp) {
        char buf[4] = {0};
        fgets(buf, sizeof(buf), fp);
        fclose(fp);
        g_dark_mode = (buf[0] == '1');
    }
    g_free(path);
}

static gboolean g_show_whats_new = TRUE;

static void save_show_whats_new_pref(void) {
    char *path = config_path("show_whats_new");
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    FILE *fp = fopen(path, "w");
    if (fp) {
        fputs(g_show_whats_new ? "1" : "0", fp);
        fclose(fp);
    }
    g_free(dir);
    g_free(path);
}

static void load_show_whats_new_pref(void) {
    char *path = config_path("show_whats_new");
    FILE *fp = fopen(path, "r");
    if (fp) {
        char buf[4] = {0};
        fgets(buf, sizeof(buf), fp);
        fclose(fp);
        g_show_whats_new = (buf[0] != '0');
    }
    g_free(path);
}

static const char DARK_CSS[] =
    "window, box, scrolledwindow, viewport, notebook, notebook tab, popover {"
    "  background-color: #1e1e2e;"
    "  color: #cdd6f4;"
    "}"
    "entry {"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border-color: #45475a;"
    "  caret-color: #cdd6f4;"
    "}"
    "entry:focus {"
    "  border-color: #89b4fa;"
    "}"
    "button {"
    "  background-color: #313244;"
    "  background-image: none;"
    "  color: #cdd6f4;"
    "  border-color: #45475a;"
    "  box-shadow: none;"
    "}"
    "button:hover {"
    "  background-color: #45475a;"
    "}"
    "button:active, button:checked {"
    "  background-color: #89b4fa;"
    "  color: #1e1e2e;"
    "}"
    "checkbutton, checkbutton label, radiobutton, radiobutton label {"
    "  color: #cdd6f4;"
    "}"
    "combobox, combobox * {"
    "  background-color: #313244;"
    "  color: #cdd6f4;"
    "  border-color: #45475a;"
    "}"
    "treeview {"
    "  background-color: #181825;"
    "  color: #cdd6f4;"
    "}"
    "treeview:selected {"
    "  background-color: #89b4fa;"
    "  color: #1e1e2e;"
    "}"
    "treeview header button {"
    "  background-color: #1e1e2e;"
    "  background-image: none;"
    "  color: #a6adc8;"
    "  border-color: #45475a;"
    "}"
    "label {"
    "  color: #cdd6f4;"
    "}"
    "scrollbar {"
    "  background-color: #1e1e2e;"
    "}"
    "scrollbar slider {"
    "  background-color: #45475a;"
    "  min-width: 8px;"
    "  min-height: 8px;"
    "}"
    "scrollbar slider:hover {"
    "  background-color: #585b70;"
    "}"
    "textview, textview text {"
    "  background-color: #181825;"
    "  color: #cdd6f4;"
    "}"
    "separator {"
    "  background-color: #45475a;"
    "}"
    "dialog {"
    "  background-color: #1e1e2e;"
    "}"
    "spinner {"
    "  color: #89b4fa;"
    "}"
    "popover {"
    "  background-color: #313244;"
    "  border-color: #45475a;"
    "}"
    "popover label {"
    "  color: #cdd6f4;"
    "}";

static void apply_dark_mode(void) {
    GdkScreen *screen = gdk_screen_get_default();

    if (g_dark_mode) {
        if (!g_css_dark) {
            g_css_dark = gtk_css_provider_new();
            gtk_css_provider_load_from_data(g_css_dark, DARK_CSS, -1, NULL);
        }
        gtk_style_context_add_provider_for_screen(screen,
            GTK_STYLE_PROVIDER(g_css_dark),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    } else {
        if (g_css_dark) {
            gtk_style_context_remove_provider_for_screen(screen,
                GTK_STYLE_PROVIDER(g_css_dark));
        }
    }

    if (g_btn_dark_mode) {
        g_dark_mode_setting = TRUE;
        gtk_switch_set_active(GTK_SWITCH(g_btn_dark_mode), g_dark_mode);
        g_dark_mode_setting = FALSE;
    }
}

static void on_dark_mode_toggled(GObject *obj, GParamSpec *pspec, gpointer d) {
    if (g_dark_mode_setting) return;
    g_dark_mode = gtk_switch_get_active(GTK_SWITCH(obj));
    apply_dark_mode();
    save_dark_pref();
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
    COL_SIZE,
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
static GArray            *g_all_pkgs     = NULL;
static gint               g_current_page = 0;
#define PAGE_SIZE         20
static GtkWidget         *g_btn_prev     = NULL;
static GtkWidget         *g_btn_next     = NULL;
static GtkWidget         *g_page_label   = NULL;
static GtkWidget         *g_status;
static GtkWidget         *g_tree;
static GtkWidget         *g_spinner;
static GtkListStore      *g_store;
static GtkWidget         *g_chk_pacman;
static GtkWidget         *g_chk_aur;
static GtkWidget         *g_chk_flatpak;
static GtkWidget         *g_label_sources;
static GtkTreeViewColumn *g_col_status_w;
static GtkTreeViewColumn *g_col_source_w;
static GtkTreeViewColumn *g_col_name_w;
static GtkTreeViewColumn *g_col_version_w;
static GtkTreeViewColumn *g_col_desc_w;

static GtkWidget         *g_notebook;
static GtkWidget         *g_inst_tree;
static GtkListStore      *g_inst_store;
static GtkWidget         *g_inst_refresh_btn;
static GtkWidget         *g_inst_select_all_btn;
static GtkWidget         *g_select_page_btn;
static GArray            *g_installed_all = NULL;
static gint               g_inst_page = 0;
static GtkWidget         *g_inst_prev;
static GtkWidget         *g_inst_next;
static GtkWidget         *g_inst_page_label;
static GtkWidget         *g_inst_total_label;
static GtkWidget         *g_inst_spinner;
static GtkWidget         *g_options_btn;

static char               g_inst_filter[256]   = {0};
static GtkWidget         *g_inst_filter_entry  = NULL;
static GArray            *g_installed_filtered = NULL;

static GtkWidget         *g_orphan_tree        = NULL;
static GtkListStore      *g_orphan_store       = NULL;
static GtkWidget         *g_orphan_spinner     = NULL;
static GtkWidget         *g_orphan_remove_btn  = NULL;
static GtkWidget         *g_orphan_status_lbl  = NULL;
static GtkWidget         *g_btn_clean_cache    = NULL;

static volatile gint      g_search_generation  = 0;

static GtkTreeViewColumn *g_inst_col_size_w    = NULL;

static GtkListStore      *g_hist_store         = NULL;
#define HISTORY_MAX 10

static gint               g_sys_update_count   = -1;

static void apply_lang(void);


typedef struct { const char *key; GtkTreeViewColumn **col; } ColDef;

static void save_column_widths(void) {
    char *path = config_path("columns");
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    FILE *fp = fopen(path, "w");
    if (!fp) { g_free(path); return; }
    ColDef defs[] = {
        { "inst_size", &g_inst_col_size_w },
        { NULL,         NULL              }
    };
    for (int i = 0; defs[i].key; i++) {
        if (*defs[i].col) {
            gint w = gtk_tree_view_column_get_width(*defs[i].col);
            if (w > 0) fprintf(fp, "%s=%d\n", defs[i].key, w);
        }
    }
    fclose(fp);
    g_free(path);
}

static void load_column_widths(void) {
    char *path = config_path("columns");
    FILE *fp = fopen(path, "r");
    if (!fp) { g_free(path); return; }
    char line[128];
    while (fgets(line, sizeof(line), fp)) {
        char *nl = strchr(line, '\n'); if (nl) *nl = '\0';
        char *eq = strchr(line, '=');
        if (!eq) continue;
        *eq = '\0';
        int w = atoi(eq + 1);
        if (w <= 0) continue;
        if (strcmp(line, "inst_size") == 0 && g_inst_col_size_w)
            gtk_tree_view_column_set_fixed_width(g_inst_col_size_w, w);
    }
    fclose(fp);
    g_free(path);
}

typedef struct {
    char     source[16];
    char     name[MAX_NAME];
    char     version[MAX_VER];
    char     desc[MAX_DESC];
    char     cmd[MAX_CMD];
    char     remove_cmd[MAX_CMD];
    char     size[32];
    gboolean installed;
} Pkg;

typedef struct {
    char     query[256];
    gboolean use_pacman;
    gboolean use_aur;
    gboolean use_flatpak;
    gint     generation;
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
    size_t qlen = strlen(query);
    for (int i = (int)pkgs->len - 1; i >= 0; i--) {
        Pkg *p = &g_array_index(pkgs, Pkg, i);
        if (strncasecmp(p->name, query, qlen) != 0)
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

static void store_append_pkg(Pkg *p) {
    GtkTreeIter it;
    gtk_list_store_insert_with_values(g_store, &it, -1,
        COL_CHECK,      FALSE,
        COL_STATUS,     p->installed ? T(STR_INSTALLED) : "",
        COL_SOURCE,     p->source,
        COL_NAME,       p->name,
        COL_VERSION,    p->version,
        COL_DESC,       p->desc,
        COL_CMD,        p->cmd,
        COL_REMOVE_CMD, p->remove_cmd,
        COL_INSTALLED,  p->installed,
        COL_SIZE,       p->size,
        -1);
}

static void update_pagination(gint total) {
    gint total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages < 1) total_pages = 1;

    gboolean show = (total > PAGE_SIZE);
    gtk_widget_set_visible(g_btn_prev,   show);
    gtk_widget_set_visible(g_btn_next,   show);
    gtk_widget_set_visible(g_page_label, show);
    gtk_widget_set_visible(g_select_page_btn, show);

    if (show) {
        char txt[32];
        snprintf(txt, sizeof(txt), "%d / %d", g_current_page + 1, total_pages);
        gtk_label_set_text(GTK_LABEL(g_page_label), txt);
        gtk_widget_set_sensitive(g_btn_prev, g_current_page > 0);
        gtk_widget_set_sensitive(g_btn_next, g_current_page < total_pages - 1);
    }
}

static void render_page(gint page) {
    if (!g_all_pkgs) return;

    gint total       = (gint)g_all_pkgs->len;
    gint total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages < 1) total_pages = 1;
    if (page < 0) page = 0;
    if (page >= total_pages) page = total_pages - 1;
    g_current_page = page;

    gtk_widget_freeze_child_notify(g_tree);
    g_object_freeze_notify(G_OBJECT(g_store));
    gtk_list_store_clear(g_store);

    gint start = page * PAGE_SIZE;
    gint end   = MIN(start + PAGE_SIZE, total);

    for (gint i = start; i < end; i++)
        store_append_pkg(&g_array_index(g_all_pkgs, Pkg, i));

    g_object_thaw_notify(G_OBJECT(g_store));
    gtk_widget_thaw_child_notify(g_tree);

    update_pagination(total);
}

static void on_page_prev(GtkWidget *w, gpointer d) { render_page(g_current_page - 1); }
static void on_page_next(GtkWidget *w, gpointer d) { render_page(g_current_page + 1); }

static void on_select_page(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(g_store);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    gboolean first_checked = FALSE;
    if (valid) {
        gtk_tree_model_get(model, &iter, COL_CHECK, &first_checked, -1);
    }
    gboolean new_state = !first_checked;
    valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        gtk_list_store_set(g_store, &iter, COL_CHECK, new_state, -1);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

static void inst_store_append_pkg(GtkListStore *store, Pkg *p) {
    GtkTreeIter it;
    gtk_list_store_insert_with_values(store, &it, -1,
        COL_CHECK,      FALSE,
        COL_STATUS,     "",
        COL_SOURCE,     p->source,
        COL_NAME,       p->name,
        COL_VERSION,    p->version,
        COL_DESC,       p->desc,
        COL_CMD,        p->cmd,
        COL_REMOVE_CMD, p->remove_cmd,
        COL_INSTALLED,  TRUE,
        COL_SIZE,       p->size,
        -1);
}

static void rebuild_installed_filtered(void) {
    if (g_installed_filtered) {
        g_array_free(g_installed_filtered, TRUE);
        g_installed_filtered = NULL;
    }
    if (!g_installed_all) return;

    const char *f = g_inst_filter;
    if (!f[0]) return;

    g_installed_filtered = g_array_sized_new(FALSE, TRUE, sizeof(Pkg),
                                              g_installed_all->len);
    for (guint i = 0; i < g_installed_all->len; i++) {
        Pkg *p = &g_array_index(g_installed_all, Pkg, i);
        if (strcasestr(p->name,    f)
            || strcasestr(p->source,  f)
            || strcasestr(p->desc,    f)
            || strcasestr(p->version, f)) {
            g_array_append_val(g_installed_filtered, *p);
        }
    }
}

static void update_inst_pagination(gint total) {
    gint total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages < 1) total_pages = 1;

    gboolean show = (total > PAGE_SIZE);
    gtk_widget_set_visible(g_inst_prev, show);
    gtk_widget_set_visible(g_inst_next, show);
    gtk_widget_set_visible(g_inst_page_label, show);

    if (show) {
        char txt[32];
        snprintf(txt, sizeof(txt), "%d / %d", g_inst_page + 1, total_pages);
        gtk_label_set_text(GTK_LABEL(g_inst_page_label), txt);
        gtk_widget_set_sensitive(g_inst_prev, g_inst_page > 0);
        gtk_widget_set_sensitive(g_inst_next, g_inst_page < total_pages - 1);
    }

    if (g_inst_total_label) {
        char txt[64];
        snprintf(txt, sizeof(txt), T(STR_INST_TOTAL), total);
        gtk_label_set_text(GTK_LABEL(g_inst_total_label), txt);
    }
}

static void render_installed_page(gint page) {
    GArray *src = g_installed_filtered ? g_installed_filtered : g_installed_all;
    if (!src) return;

    gint total       = (gint)src->len;
    gint total_pages = (total + PAGE_SIZE - 1) / PAGE_SIZE;
    if (total_pages < 1) total_pages = 1;
    if (page < 0) page = 0;
    if (page >= total_pages) page = total_pages - 1;
    g_inst_page = page;

    gtk_widget_freeze_child_notify(g_inst_tree);
    g_object_freeze_notify(G_OBJECT(g_inst_store));
    gtk_list_store_clear(g_inst_store);

    gint start = page * PAGE_SIZE;
    gint end   = MIN(start + PAGE_SIZE, total);

    for (gint i = start; i < end; i++)
        inst_store_append_pkg(g_inst_store, &g_array_index(src, Pkg, i));

    g_object_thaw_notify(G_OBJECT(g_inst_store));
    gtk_widget_thaw_child_notify(g_inst_tree);

    update_inst_pagination(total);
}

static void on_inst_page_prev(GtkWidget *w, gpointer d) { render_installed_page(g_inst_page - 1); }
static void on_inst_page_next(GtkWidget *w, gpointer d) { render_installed_page(g_inst_page + 1); }

typedef struct {
    GArray *pkgs;
    gboolean is_last;
} InstBatchCtx;

static gboolean inst_batch_add_cb(gpointer data) {
    InstBatchCtx *ctx = data;

    if (!g_installed_all)
        g_installed_all = g_array_new(FALSE, TRUE, sizeof(Pkg));

    gint old_total = (gint)g_installed_all->len;

    if (ctx->pkgs->len > 0)
        g_array_append_vals(g_installed_all, ctx->pkgs->data, ctx->pkgs->len);

    gint new_total = (gint)g_installed_all->len;

    rebuild_installed_filtered();
    GArray *src = g_installed_filtered ? g_installed_filtered : g_installed_all;
    gint filtered_total = src ? (gint)src->len : 0;

    gint page_start = g_inst_page * PAGE_SIZE;
    gint page_end   = page_start + PAGE_SIZE;

    if (new_total > page_start && old_total < page_end) {
        gint vis_start = MAX(old_total, page_start);
        gint vis_end   = MIN(new_total, page_end);

        for (gint i = vis_start; i < vis_end; i++)
            inst_store_append_pkg(g_inst_store, &g_array_index(g_installed_all, Pkg, i));
    }

    update_inst_pagination(filtered_total);

    if (ctx->is_last) {
        gtk_spinner_stop(GTK_SPINNER(g_inst_spinner));
        gtk_widget_set_sensitive(g_inst_refresh_btn, TRUE);
        gtk_widget_set_sensitive(g_inst_select_all_btn, TRUE);
        gtk_widget_set_sensitive(g_inst_prev, TRUE);
        gtk_widget_set_sensitive(g_inst_next, TRUE);
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_STATUS_READY));
    }

    g_array_free(ctx->pkgs, TRUE);
    g_free(ctx);
    return G_SOURCE_REMOVE;
}

static void inst_dispatch_batch(GArray *pkgs, gboolean is_last) {
    InstBatchCtx *ctx = g_new0(InstBatchCtx, 1);
    ctx->pkgs = pkgs;
    ctx->is_last = is_last;
    g_idle_add(inst_batch_add_cb, ctx);
}

static gpointer load_installed_thread(gpointer data) {
    GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
    char line[1024];
    FILE *fp;
    const int BATCH_SIZE = 50;
    int count = 0;

    {
        fp = popen("pacman -Qq 2>/dev/null | xargs pacman -Qi 2>/dev/null", "r");
        if (fp) {
            Pkg p = {0};
            gboolean have_pkg = FALSE;
            while (fgets(line, sizeof(line), fp)) {
                char tmp[MAX_LINE];
                strncpy(tmp, line, sizeof(tmp)-1); tmp[sizeof(tmp)-1] = '\0';
                trim(tmp);
                if (!tmp[0]) {
                    if (have_pkg && p.name[0]) {
                        g_array_append_val(pkgs, p);
                        count++;
                        if (count % BATCH_SIZE == 0) {
                            inst_dispatch_batch(pkgs, FALSE);
                            pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
                        }
                    }
                    memset(&p, 0, sizeof(p));
                    have_pkg = FALSE;
                    continue;
                }
                char *colon = strchr(tmp, ':');
                if (!colon) continue;
                *colon = '\0';
                char *key = tmp, *val = colon + 1;
                trim(key); trim(val);
                if (strcasecmp(key, "Name") == 0) {
                    strncpy(p.name, val, sizeof(p.name)-1);
                    strncpy(p.source, "pacman", sizeof(p.source)-1);
                    snprintf(p.cmd,        sizeof(p.cmd),        "sudo pacman -S %s",   p.name);
                    snprintf(p.remove_cmd, sizeof(p.remove_cmd), "sudo pacman -Rns %s", p.name);
                    p.installed = TRUE;
                    have_pkg = TRUE;
                } else if (strcasecmp(key, "Version") == 0) {
                    strncpy(p.version, val, sizeof(p.version)-1);
                } else if (strcasecmp(key, "Description") == 0) {
                    strncpy(p.desc, val, sizeof(p.desc)-1);
                } else if (strcasecmp(key, "Installed Size") == 0) {
                    strncpy(p.size, val, sizeof(p.size)-1);
                }
            }
            if (have_pkg && p.name[0]) {
                g_array_append_val(pkgs, p);
                count++;
            }
            pclose(fp);
        }
    }

    fp = popen("flatpak list --columns=name,application,version,description 2>/dev/null", "r");
    if (fp) {
        fgets(line, sizeof(line), fp);
        while (fgets(line, sizeof(line), fp)) {
            trim(line);
            if (!line[0]) continue;
            char *name = line;
            char *appid = strchr(name, '\t');
            if (!appid) continue;
            *appid++ = '\0';
            char *ver = strchr(appid, '\t');
            if (!ver) continue;
            *ver++ = '\0';
            char *desc = strchr(ver, '\t');
            if (desc) *desc++ = '\0';
            else desc = "";
            Pkg p = {0};
            strncpy(p.source, "flatpak", sizeof(p.source)-1);
            strncpy(p.name, name, sizeof(p.name)-1);
            strncpy(p.version, ver, sizeof(p.version)-1);
            strncpy(p.desc, desc, sizeof(p.desc)-1);
            snprintf(p.cmd, sizeof(p.cmd), "flatpak install flathub %s", appid);
            snprintf(p.remove_cmd, sizeof(p.remove_cmd), "flatpak uninstall %s", appid);
            p.installed = TRUE;
            g_array_append_val(pkgs, p);
            count++;
            if (count % BATCH_SIZE == 0) {
                inst_dispatch_batch(pkgs, FALSE);
                pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
            }
        }
        pclose(fp);
    }

    if (pkgs->len > 0 || count == 0) {
        inst_dispatch_batch(pkgs, TRUE);
    } else {
        g_array_free(pkgs, TRUE);
        inst_dispatch_batch(g_array_new(FALSE, TRUE, sizeof(Pkg)), TRUE);
    }

    return NULL;
}

static void on_installed_refresh(GtkWidget *w, gpointer d) {
    gtk_spinner_start(GTK_SPINNER(g_inst_spinner));
    gtk_widget_set_sensitive(g_inst_refresh_btn, FALSE);
    gtk_widget_set_sensitive(g_inst_select_all_btn, FALSE);
    gtk_widget_set_sensitive(g_inst_prev, FALSE);
    gtk_widget_set_sensitive(g_inst_next, FALSE);
    gtk_label_set_text(GTK_LABEL(g_status), T(STR_INST_LOADING));
    gtk_list_store_clear(g_inst_store);
    if (g_installed_all) {
        g_array_free(g_installed_all, TRUE);
        g_installed_all = NULL;
    }
    if (g_installed_filtered) {
        g_array_free(g_installed_filtered, TRUE);
        g_installed_filtered = NULL;
    }
    g_inst_page = 0;
    GThread *t = g_thread_new("load-installed", load_installed_thread, NULL);
    g_thread_unref(t);
}

static void on_installed_select_all(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(g_inst_store);
    GtkTreeIter iter;
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    gboolean first_checked = FALSE;
    if (valid) {
        gtk_tree_model_get(model, &iter, COL_CHECK, &first_checked, -1);
    }
    gboolean new_state = !first_checked;
    valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        gtk_list_store_set(g_inst_store, &iter, COL_CHECK, new_state, -1);
        valid = gtk_tree_model_iter_next(model, &iter);
    }
}

static void on_filter_icon_press(GtkEntry *entry, GtkEntryIconPosition pos,
                                 GdkEvent *event, gpointer data) {
    (void)event; (void)data;
    if (pos == GTK_ENTRY_ICON_SECONDARY)
        gtk_entry_set_text(entry, "");
}

static guint g_filter_debounce_id = 0;

static gboolean filter_debounce_cb(gpointer data) {
    (void)data;
    g_filter_debounce_id = 0;
    rebuild_installed_filtered();
    g_inst_page = 0;
    render_installed_page(0);
    return G_SOURCE_REMOVE;
}

static void on_inst_filter_changed(GtkEditable *editable, gpointer d) {
    const char *text = gtk_entry_get_text(GTK_ENTRY(editable));
    strncpy(g_inst_filter, text ? text : "", sizeof(g_inst_filter) - 1);
    g_inst_filter[sizeof(g_inst_filter) - 1] = '\0';
    if (g_filter_debounce_id)
        g_source_remove(g_filter_debounce_id);
    g_filter_debounce_id = g_timeout_add(120, filter_debounce_cb, NULL);
}

static gboolean batch_add_cb(gpointer data) {
    BatchCtx *ctx = data;

    gint old_total = (gint)g_all_pkgs->len;

    if (ctx->pkgs->len > 0)
        g_array_append_vals(g_all_pkgs, ctx->pkgs->data, ctx->pkgs->len);

    gint new_total  = (gint)g_all_pkgs->len;
    gint page_start = g_current_page * PAGE_SIZE;
    gint page_end   = page_start + PAGE_SIZE;

    if (new_total > page_start && old_total < page_end) {
        gint vis_start = MAX(old_total, page_start);
        gint vis_end   = MIN(new_total, page_end);

        gtk_widget_freeze_child_notify(g_tree);
        g_object_freeze_notify(G_OBJECT(g_store));

        for (gint i = vis_start; i < vis_end; i++)
            store_append_pkg(&g_array_index(g_all_pkgs, Pkg, i));

        g_object_thaw_notify(G_OBJECT(g_store));
        gtk_widget_thaw_child_notify(g_tree);
    }

    update_pagination(new_total);

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


typedef struct {
    char        query[256];
    gint        generation;
    GHashTable *pacman_ht;
    GHashTable *flatpak_ht;
    GMutex      mutex;
    gint        remaining;  
    guint       grand_total; 
} SharedSearchCtx;

typedef struct {
    SharedSearchCtx *shared;
    int              source_id;
} SourceSearchData;

static gpointer source_search_worker(gpointer data);

static gpointer search_thread(gpointer data) {
    SearchCtx *ctx = data;

    if (ctx->generation != g_atomic_int_get(&g_search_generation)) {
        g_free(ctx);
        return NULL;
    }

    GHashTable *pacman_ht  = build_pacman_installed();
    GHashTable *flatpak_ht = build_flatpak_installed();

    gboolean do_aur, do_flatpak;
    { gchar *p = g_find_program_in_path("yay");     do_aur     = ctx->use_aur     && p != NULL; g_free(p); }
    { gchar *p = g_find_program_in_path("flatpak"); do_flatpak = ctx->use_flatpak && p != NULL; g_free(p); }

    int total = (ctx->use_pacman ? 1 : 0) + (do_aur ? 1 : 0) + (do_flatpak ? 1 : 0);

    if (total == 0) {
        g_hash_table_destroy(pacman_ht);
        g_hash_table_destroy(flatpak_ht);
        dispatch_batch(g_array_new(FALSE, TRUE, sizeof(Pkg)),
                       ctx->query, TRUE, 0);
        g_free(ctx);
        return NULL;
    }

    SharedSearchCtx *shared = g_new0(SharedSearchCtx, 1);
    strncpy(shared->query, ctx->query, sizeof(shared->query) - 1);
    shared->generation  = ctx->generation;
    shared->pacman_ht   = pacman_ht;
    shared->flatpak_ht  = flatpak_ht;
    g_mutex_init(&shared->mutex);
    shared->remaining   = total;
    shared->grand_total = 0;

    if (ctx->use_pacman) {
        SourceSearchData *sd = g_new0(SourceSearchData, 1);
        sd->shared = shared; sd->source_id = 0;
        GThread *t = g_thread_new("pkg-search-pacman", source_search_worker, sd);
        g_thread_unref(t);
    }
    if (do_aur) {
        SourceSearchData *sd = g_new0(SourceSearchData, 1);
        sd->shared = shared; sd->source_id = 1;
        GThread *t = g_thread_new("pkg-search-aur", source_search_worker, sd);
        g_thread_unref(t);
    }
    if (do_flatpak) {
        SourceSearchData *sd = g_new0(SourceSearchData, 1);
        sd->shared = shared; sd->source_id = 2;
        GThread *t = g_thread_new("pkg-search-flatpak", source_search_worker, sd);
        g_thread_unref(t);
    }

    g_free(ctx);
    return NULL;
}

static gpointer source_search_worker(gpointer data) {
    SourceSearchData *sd  = data;
    SharedSearchCtx *shared = sd->shared;
    int source_id         = sd->source_id;
    g_free(sd);

    GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));

    if (shared->generation == g_atomic_int_get(&g_search_generation)) {
        char cmd[512]; FILE *fp;
        gchar *quoted = g_shell_quote(shared->query);

        if (source_id == 0) {                       
            snprintf(cmd, sizeof(cmd), "pacman -Ss %s 2>/dev/null", quoted);
            fp = popen(cmd, "r");
            if (fp) { parse_pacman_output(fp, pkgs, "pacman", "sudo pacman -S"); pclose(fp); }
            filter_exact(pkgs, shared->query);
            for (guint i = 0; i < pkgs->len; i++) {
                Pkg *p = &g_array_index(pkgs, Pkg, i);
                p->installed = g_hash_table_contains(shared->pacman_ht, p->name);
            }
        } else if (source_id == 1) {                
            snprintf(cmd, sizeof(cmd),
                     "yay --color=never -Ss --aur %s 2>/dev/null", quoted);
            fp = popen(cmd, "r");
            if (fp) { parse_pacman_output(fp, pkgs, "aur", "yay -S"); pclose(fp); }
            filter_exact(pkgs, shared->query);
            for (guint i = 0; i < pkgs->len; i++) {
                Pkg *p = &g_array_index(pkgs, Pkg, i);
                p->installed = g_hash_table_contains(shared->pacman_ht, p->name);
            }
        } else {                                    
            snprintf(cmd, sizeof(cmd), "flatpak search %s 2>/dev/null", quoted);
            fp = popen(cmd, "r");
            if (fp) { parse_flatpak_output(fp, pkgs); pclose(fp); }
            filter_exact(pkgs, shared->query);
            for (guint i = 0; i < pkgs->len; i++) {
                Pkg *p = &g_array_index(pkgs, Pkg, i);
                char *appid = strrchr(p->cmd, ' ');
                if (appid) appid++;
                p->installed = appid &&
                               g_hash_table_contains(shared->flatpak_ht, appid);
            }
        }
        g_free(quoted);
    }

    g_mutex_lock(&shared->mutex);
    shared->grand_total += pkgs->len;
    shared->remaining--;
    gboolean is_last = (shared->remaining == 0);
    guint    gt      = shared->grand_total;
    char     query[256];
    strncpy(query, shared->query, sizeof(query) - 1);
    g_mutex_unlock(&shared->mutex);

    if (is_last) {
        g_hash_table_destroy(shared->pacman_ht);
        g_hash_table_destroy(shared->flatpak_ht);
        g_mutex_clear(&shared->mutex);
        g_free(shared);
    }

    dispatch_batch(pkgs, query, is_last, gt);
    return NULL;
}

static void on_toggle(GtkCellRendererToggle *cell, gchar *path_str, gpointer store_ptr) {
    GtkListStore *store = GTK_LIST_STORE(store_ptr);
    GtkTreeIter iter;
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    gboolean val;
    gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path);
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter, COL_CHECK, &val, -1);
    gtk_list_store_set(store, &iter, COL_CHECK, !val, -1);
    gtk_tree_path_free(path);
}

static void show_pkg_info_dialog(GtkListStore *store, GtkTreePath *path) {
    GtkTreeIter iter;
    if (!gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, path)) return;
    gchar *name = NULL, *src = NULL, *cmd = NULL;
    gtk_tree_model_get(GTK_TREE_MODEL(store), &iter,
        COL_NAME, &name, COL_SOURCE, &src, COL_CMD, &cmd, -1);
    if (!name) { g_free(src); g_free(cmd); return; }

    char title[256];
    snprintf(title, sizeof(title), T(STR_PKG_INFO_TITLE), name);

    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        title, GTK_WINDOW(g_win),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "_OK", GTK_RESPONSE_OK, NULL);
    gtk_window_set_default_size(GTK_WINDOW(dlg), 600, 460);

    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dlg));
    gtk_container_set_border_width(GTK_CONTAINER(content), 10);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_widget_set_hexpand(scroll, TRUE);

    GtkWidget *tv = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(tv), FALSE);
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(tv), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(tv), GTK_WRAP_WORD_CHAR);
    gtk_text_view_set_left_margin(GTK_TEXT_VIEW(tv), 8);
    gtk_text_view_set_top_margin(GTK_TEXT_VIEW(tv), 6);
    GtkTextBuffer *buf = gtk_text_view_get_buffer(GTK_TEXT_VIEW(tv));

    char info_cmd[512];
    if (src && (strcmp(src, "aur") == 0))
        snprintf(info_cmd, sizeof(info_cmd), "yay -Si '%s' 2>&1", name);
    else if (src && strcmp(src, "flatpak") == 0) {
        gchar *appid = cmd ? g_strdup(strrchr(cmd, ' ') ? strrchr(cmd, ' ') + 1 : name) : g_strdup(name);
        snprintf(info_cmd, sizeof(info_cmd), "flatpak info '%s' 2>&1", appid);
        g_free(appid);
    } else
        snprintf(info_cmd, sizeof(info_cmd), "pacman -Si '%s' 2>/dev/null || pacman -Qi '%s' 2>&1", name, name);

    FILE *fp = popen(info_cmd, "r");
    GString *out = g_string_new("");
    if (fp) {
        char line[512];
        while (fgets(line, sizeof(line), fp)) g_string_append(out, line);
        pclose(fp);
    }
    if (!out->len) g_string_append(out, "(no info available)");
    gtk_text_buffer_set_text(buf, out->str, -1);
    g_string_free(out, TRUE);

    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);

    g_free(name); g_free(src); g_free(cmd);
}

static void on_row_activated(GtkTreeView *tv, GtkTreePath *path,
                             GtkTreeViewColumn *col, gpointer store_ptr) {
    show_pkg_info_dialog(GTK_LIST_STORE(store_ptr), path);
}

static GtkCssProvider *g_css_status_orange = NULL;

static void status_set_orange(const char *text) {
    if (!g_css_status_orange) {
        g_css_status_orange = gtk_css_provider_new();
        gtk_css_provider_load_from_data(g_css_status_orange,
            "#status_label { color: #e07020; font-weight: bold; }", -1, NULL);
    }
    GtkStyleContext *sc = gtk_widget_get_style_context(g_status);
    gtk_style_context_add_provider(sc,
        GTK_STYLE_PROVIDER(g_css_status_orange),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
    gtk_label_set_text(GTK_LABEL(g_status), text);
}

static void status_clear_orange(void) {
    if (g_css_status_orange) {
        GtkStyleContext *sc = gtk_widget_get_style_context(g_status);
        gtk_style_context_remove_provider(sc,
            GTK_STYLE_PROVIDER(g_css_status_orange));
    }
}

typedef struct {
    gchar  **pkg_names;
    gint     op;
} WatchCtx;

static void recheck_installed(gchar **pkg_names, gint op) {
    GHashTable *pacman_ht  = build_pacman_installed();
    GHashTable *flatpak_ht = build_flatpak_installed();

    GHashTable *affected = g_hash_table_new(g_str_hash, g_str_equal);
    for (gint i = 0; pkg_names[i]; i++)
        g_hash_table_add(affected, pkg_names[i]);

    if (g_all_pkgs) {
        for (guint i = 0; i < g_all_pkgs->len; i++) {
            Pkg *p = &g_array_index(g_all_pkgs, Pkg, i);
            if (!g_hash_table_contains(affected, p->name)) continue;
            if (g_strcmp0(p->source, "Flatpak") == 0) {
                const char *cmd = p->cmd;
                const char *appid = NULL;
                const char *fh = "flatpak install flathub ";
                const char *fo = "flatpak install ";
                if (g_str_has_prefix(cmd, fh))       appid = cmd + strlen(fh);
                else if (g_str_has_prefix(cmd, fo))  appid = cmd + strlen(fo);
                p->installed = appid && g_hash_table_contains(flatpak_ht, appid);
            } else {
                p->installed = g_hash_table_contains(pacman_ht, p->name);
            }
        }
    }

    gboolean inst = FALSE;
    GtkTreeModel *model = GTK_TREE_MODEL(g_store);
    GtkTreeIter it;
    if (gtk_tree_model_get_iter_first(model, &it)) {
        do {
            gchar *name = NULL;
            gtk_tree_model_get(model, &it, COL_NAME, &name, -1);
            if (name && g_hash_table_contains(affected, name)) {
                gchar *src = NULL;
                gchar *cmd = NULL;
                gtk_tree_model_get(model, &it, COL_SOURCE, &src, COL_CMD, &cmd, -1);
                if (g_strcmp0(src, "Flatpak") == 0) {
                    const char *appid = NULL;
                    const char *fh = "flatpak install flathub ";
                    const char *fo = "flatpak install ";
                    if (cmd && g_str_has_prefix(cmd, fh))      appid = cmd + strlen(fh);
                    else if (cmd && g_str_has_prefix(cmd, fo)) appid = cmd + strlen(fo);
                    inst = appid && g_hash_table_contains(flatpak_ht, appid);
                } else {
                    inst = g_hash_table_contains(pacman_ht, name);
                }
                gtk_list_store_set(GTK_LIST_STORE(model), &it,
                    COL_INSTALLED, inst,
                    COL_STATUS,    inst ? T(STR_INSTALLED) : "",
                    COL_CHECK,     FALSE,
                    -1);
                g_free(src);
                g_free(cmd);
            }
            g_free(name);
        } while (gtk_tree_model_iter_next(model, &it));
    }

    g_hash_table_destroy(affected);
    g_hash_table_destroy(pacman_ht);
    g_hash_table_destroy(flatpak_ht);
}

static void on_terminal_exit(GPid pid, gint status, gpointer user_data) {
    WatchCtx *ctx = user_data;

    status_clear_orange();

    recheck_installed(ctx->pkg_names, ctx->op);

    gtk_label_set_text(GTK_LABEL(g_status),
        ctx->op > 0 ? T(STR_BTN_INSTALL) : T(STR_BTN_REMOVE));

    gtk_widget_set_sensitive(g_btn_install, TRUE);
    gtk_widget_set_sensitive(g_btn_remove,  TRUE);
    gtk_widget_set_sensitive(g_btn_search,  TRUE);

    g_spawn_close_pid(pid);
    g_strfreev(ctx->pkg_names);
    g_free(ctx);
}

static const char *find_terminal(void) {
    static const struct { const char *bin; } terms[] = {
        {"alacritty"}, {"kitty"}, {"xterm"}, {"gnome-terminal"},
        {"konsole"}, {"xfce4-terminal"}, {"mate-terminal"},
        {"lxterminal"}, {"tilix"}, {"foot"}, {NULL}
    };
    for (int i = 0; terms[i].bin; i++) {
        char cmd[128];
        snprintf(cmd, sizeof(cmd), "which %s >/dev/null 2>&1", terms[i].bin);
        if (system(cmd) == 0) return terms[i].bin;
    }
    return NULL;
}

static void run_in_terminal(GString *script, int op, gchar **pkg_names) {
    char done_line[128];
    snprintf(done_line, sizeof(done_line),
             "; echo; echo '%s'; read", T(STR_ALACRITTY_DONE));
    g_string_append(script, done_line);

    const char *term = find_terminal();
    if (!term) {
        status_clear_orange();
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_NO_TERMINAL));
        gtk_widget_set_sensitive(g_btn_install, TRUE);
        gtk_widget_set_sensitive(g_btn_remove,  TRUE);
        gtk_widget_set_sensitive(g_btn_search,  TRUE);
        g_strfreev(pkg_names);
        return;
    }

    char *argv[8] = {0};
    int ai = 0;
    argv[ai++] = (char *)term;
    if (strcmp(term, "gnome-terminal") == 0)
        argv[ai++] = "--";
    else if (strcmp(term, "kitty") != 0)
        argv[ai++] = "-e";
    argv[ai++] = "sh";
    argv[ai++] = "-c";
    argv[ai++] = script->str;
    argv[ai]   = NULL;

    GError *err = NULL;
    GPid    pid;
    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err)) {
        char emsg[256];
        snprintf(emsg, sizeof(emsg), T(STR_ALACRITTY_ERR), err ? err->message : "?");
        status_clear_orange();
        gtk_label_set_text(GTK_LABEL(g_status), emsg);
        if (err) g_error_free(err);
        g_strfreev(pkg_names);
        return;
    }

    WatchCtx *ctx  = g_new0(WatchCtx, 1);
    ctx->pkg_names = pkg_names;
    ctx->op        = op;
    g_child_watch_add(pid, on_terminal_exit, ctx);
}

static GtkListStore *get_active_store(void) {
    gint tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(g_notebook));
    if (tab == 1) return g_inst_store;
    if (tab == 2) return g_orphan_store;
    return g_store;
}

static void on_install(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(get_active_store());
    GtkTreeIter iter; int count = 0;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;

    GString *pacman_pkgs   = g_string_new("");
    GString *aur_pkgs      = g_string_new("");
    GString *flatpak_fh    = g_string_new("");
    GString *flatpak_other = g_string_new("");
    GPtrArray *names       = g_ptr_array_new_with_free_func(g_free);
    int aur_count = 0;

    static const char PFX_PACMAN[]  = "sudo pacman -S ";
    static const char PFX_AUR[]     = "yay -S ";
    static const char PFX_FLAT_FH[] = "flatpak install flathub ";
    static const char PFX_FLAT[]    = "flatpak install ";

    do {
        gboolean checked, installed; gchar *pkg_cmd = NULL; gchar *pkg_name = NULL;
        gtk_tree_model_get(model, &iter,
            COL_CHECK, &checked, COL_INSTALLED, &installed,
            COL_CMD, &pkg_cmd, COL_NAME, &pkg_name, -1);
        if (checked && !installed && pkg_cmd && pkg_cmd[0]) {
            count++;
            g_ptr_array_add(names, g_strdup(pkg_name));
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
        g_free(pkg_name);
    } while (gtk_tree_model_iter_next(model, &iter));

    if (count == 0) {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_MARK_INSTALL));
        g_string_free(pacman_pkgs,   TRUE);
        g_string_free(aur_pkgs,      TRUE);
        g_string_free(flatpak_fh,    TRUE);
        g_string_free(flatpak_other, TRUE);
        g_ptr_array_free(names, TRUE);
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

    g_ptr_array_add(names, NULL);
    gchar **pkg_names = (gchar **)g_ptr_array_free(names, FALSE);

    {
        GString *list = g_string_new("");
        for (int i = 0; pkg_names[i]; i++)
            g_string_append_printf(list, "  \342\200\242 %s\n", pkg_names[i]);
        char conf_msg[2048];
        snprintf(conf_msg, sizeof(conf_msg), T(STR_CONFIRM_MSG_INSTALL), list->str);
        g_string_free(list, TRUE);
        GtkWidget *conf_dlg = gtk_message_dialog_new(GTK_WINDOW(g_win),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_QUESTION, GTK_BUTTONS_OK_CANCEL, "%s", conf_msg);
        gtk_window_set_title(GTK_WINDOW(conf_dlg), T(STR_CONFIRM_TITLE_INSTALL));
        gint resp = gtk_dialog_run(GTK_DIALOG(conf_dlg));
        gtk_widget_destroy(conf_dlg);
        if (resp != GTK_RESPONSE_OK) {
            g_strfreev(pkg_names);
            g_string_free(script, TRUE);
            return;
        }
    }

    char msg[64]; snprintf(msg, sizeof(msg), T(STR_DOWNLOADING), count);
    status_set_orange(msg);
    gtk_widget_set_sensitive(g_btn_install, FALSE);
    gtk_widget_set_sensitive(g_btn_remove,  FALSE);
    gtk_widget_set_sensitive(g_btn_search,  FALSE);
    run_in_terminal(script, +1, pkg_names);
    g_string_free(script, TRUE);
}

static void on_remove(GtkWidget *w, gpointer d) {
    GtkTreeModel *model = GTK_TREE_MODEL(get_active_store());
    GtkTreeIter iter; int count = 0;
    if (!gtk_tree_model_get_iter_first(model, &iter)) return;

    GString *pacman_pkgs  = g_string_new("");
    GString *flatpak_pkgs = g_string_new("");
    GPtrArray *names      = g_ptr_array_new_with_free_func(g_free);

    static const char PFX_PACMAN[]  = "sudo pacman -Rns ";
    static const char PFX_FLATPAK[] = "flatpak uninstall ";

    do {
        gboolean checked, installed; gchar *rem_cmd = NULL; gchar *pkg_name = NULL;
        gtk_tree_model_get(model, &iter,
            COL_CHECK, &checked, COL_INSTALLED, &installed,
            COL_REMOVE_CMD, &rem_cmd, COL_NAME, &pkg_name, -1);
        if (checked && installed && rem_cmd && rem_cmd[0]) {
            count++;
            g_ptr_array_add(names, g_strdup(pkg_name));
            if (g_str_has_prefix(rem_cmd, PFX_PACMAN)) {
                if (pacman_pkgs->len) g_string_append_c(pacman_pkgs, ' ');
                g_string_append(pacman_pkgs, rem_cmd + strlen(PFX_PACMAN));
            } else if (g_str_has_prefix(rem_cmd, PFX_FLATPAK)) {
                if (flatpak_pkgs->len) g_string_append_c(flatpak_pkgs, ' ');
                g_string_append(flatpak_pkgs, rem_cmd + strlen(PFX_FLATPAK));
            }
        }
        g_free(rem_cmd);
        g_free(pkg_name);
    } while (gtk_tree_model_iter_next(model, &iter));

    if (count == 0) {
        gtk_label_set_text(GTK_LABEL(g_status), T(STR_MARK_REMOVE));
        g_string_free(pacman_pkgs,  TRUE);
        g_string_free(flatpak_pkgs, TRUE);
        g_ptr_array_free(names, TRUE);
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

    g_ptr_array_add(names, NULL);
    gchar **pkg_names = (gchar **)g_ptr_array_free(names, FALSE);

    {
        GString *rdep_list = g_string_new("");
        for (int i = 0; pkg_names[i]; i++) {
            char dep_cmd[512];
            snprintf(dep_cmd, sizeof(dep_cmd),
                "pactree -r '%s' 2>/dev/null | grep -v '^%s$'", pkg_names[i], pkg_names[i]);
            FILE *fp_dep = popen(dep_cmd, "r");
            if (fp_dep) {
                char dep_line[256];
                while (fgets(dep_line, sizeof(dep_line), fp_dep)) {
                    trim(dep_line);
                    if (dep_line[0]) g_string_append_printf(rdep_list, "  %s\n", dep_line);
                }
                pclose(fp_dep);
            }
        }
        if (rdep_list->len > 0) {
            char warn_msg[4096];
            snprintf(warn_msg, sizeof(warn_msg), T(STR_DEPS_WARNING), rdep_list->str);
            GtkWidget *warn_dlg = gtk_message_dialog_new(GTK_WINDOW(g_win),
                GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "%s", warn_msg);
            gint resp_warn = gtk_dialog_run(GTK_DIALOG(warn_dlg));
            gtk_widget_destroy(warn_dlg);
            if (resp_warn != GTK_RESPONSE_OK) {
                g_string_free(rdep_list, TRUE);
                g_strfreev(pkg_names);
                g_string_free(script, TRUE);
                return;
            }
        }
        g_string_free(rdep_list, TRUE);
    }

    {
        GString *list = g_string_new("");
        for (int i = 0; pkg_names[i]; i++)
            g_string_append_printf(list, "  \342\200\242 %s\n", pkg_names[i]);
        char conf_msg[2048];
        snprintf(conf_msg, sizeof(conf_msg), T(STR_CONFIRM_MSG_REMOVE), list->str);
        g_string_free(list, TRUE);
        GtkWidget *conf_dlg = gtk_message_dialog_new(GTK_WINDOW(g_win),
            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
            GTK_MESSAGE_WARNING, GTK_BUTTONS_OK_CANCEL, "%s", conf_msg);
        gtk_window_set_title(GTK_WINDOW(conf_dlg), T(STR_CONFIRM_TITLE_REMOVE));
        gint resp = gtk_dialog_run(GTK_DIALOG(conf_dlg));
        gtk_widget_destroy(conf_dlg);
        if (resp != GTK_RESPONSE_OK) {
            g_strfreev(pkg_names);
            g_string_free(script, TRUE);
            return;
        }
    }

    char msg[64]; snprintf(msg, sizeof(msg), T(STR_REMOVING), count);
    status_set_orange(msg);
    gtk_widget_set_sensitive(g_btn_install, FALSE);
    gtk_widget_set_sensitive(g_btn_remove,  FALSE);
    gtk_widget_set_sensitive(g_btn_search,  FALSE);
    run_in_terminal(script, -1, pkg_names);
    g_string_free(script, TRUE);
}

static gboolean sys_updates_badge_cb(gpointer data) {
    gint n = GPOINTER_TO_INT(data);
    g_sys_update_count = n;
    if (g_btn_update_sys) {
        if (n > 0) {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), T(STR_SYS_UPDATES_BADGE), n);
            gtk_button_set_label(GTK_BUTTON(g_btn_update_sys), lbl);
        } else {
            gtk_button_set_label(GTK_BUTTON(g_btn_update_sys), T(STR_BTN_UPDATE_SYS));
        }
    }
    return G_SOURCE_REMOVE;
}

static gpointer check_sys_updates_thread(gpointer data) {
    (void)data;
    FILE *fp = popen("checkupdates 2>/dev/null | wc -l", "r");
    if (!fp) return NULL;
    char buf[32] = {0};
    fgets(buf, sizeof(buf), fp);
    pclose(fp);
    gint n = atoi(buf);
    g_idle_add(sys_updates_badge_cb, GINT_TO_POINTER(n));
    return NULL;
}

static void load_search_history(void) {
    g_hist_store = gtk_list_store_new(1, G_TYPE_STRING);
    char *path = config_path("search_history");
    FILE *fp = fopen(path, "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            trim(line);
            if (!line[0]) continue;
            GtkTreeIter it;
            gtk_list_store_append(g_hist_store, &it);
            gtk_list_store_set(g_hist_store, &it, 0, line, -1);
        }
        fclose(fp);
    }
    g_free(path);
}

static void save_search_history(void) {
    if (!g_hist_store) return;
    char *path = config_path("search_history");
    char *dir  = g_path_get_dirname(path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);
    FILE *fp = fopen(path, "w");
    if (fp) {
        GtkTreeIter it;
        gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(g_hist_store), &it);
        while (valid) {
            gchar *s = NULL;
            gtk_tree_model_get(GTK_TREE_MODEL(g_hist_store), &it, 0, &s, -1);
            if (s) { fprintf(fp, "%s\n", s); g_free(s); }
            valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(g_hist_store), &it);
        }
        fclose(fp);
    }
    g_free(path);
}

static void add_to_history(const char *query) {
    if (!g_hist_store || !query || !query[0]) return;
    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(g_hist_store), &it);
    while (valid) {
        gchar *s = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(g_hist_store), &it, 0, &s, -1);
        gboolean match = s && strcmp(s, query) == 0;
        g_free(s);
        if (match) { gtk_list_store_remove(g_hist_store, &it); break; }
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(g_hist_store), &it);
    }
    gtk_list_store_insert(g_hist_store, &it, 0);
    gtk_list_store_set(g_hist_store, &it, 0, query, -1);
    gint n = gtk_tree_model_iter_n_children(GTK_TREE_MODEL(g_hist_store), NULL);
    while (n > HISTORY_MAX) {
        GtkTreePath *last = gtk_tree_path_new_from_indices(n - 1, -1);
        GtkTreeIter lit;
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(g_hist_store), &lit, last))
            gtk_list_store_remove(g_hist_store, &lit);
        gtk_tree_path_free(last);
        n--;
    }
    save_search_history();
}

static void on_search(GtkWidget *w, gpointer d) {
    const char *query = gtk_entry_get_text(GTK_ENTRY(g_entry));
    if (!query || !query[0]) return;
    add_to_history(query);
    g_atomic_int_inc(&g_search_generation);
    gtk_widget_set_sensitive(g_btn_search,  FALSE);
    gtk_widget_set_sensitive(g_btn_install, FALSE);
    gtk_widget_set_sensitive(g_btn_remove,  FALSE);
    gtk_list_store_clear(g_store);
    if (g_all_pkgs) g_array_free(g_all_pkgs, TRUE);
    g_all_pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
    g_current_page = 0;
    gtk_widget_set_visible(g_btn_prev,   FALSE);
    gtk_widget_set_visible(g_btn_next,   FALSE);
    gtk_widget_set_visible(g_page_label, FALSE);
    gtk_widget_set_visible(g_select_page_btn, FALSE);
    gtk_label_set_text(GTK_LABEL(g_status), T(STR_SEARCHING));
    gtk_spinner_start(GTK_SPINNER(g_spinner));
    SearchCtx *ctx   = g_new0(SearchCtx, 1);
    ctx->use_pacman  = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_pacman));
    ctx->use_aur     = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_aur));
    ctx->use_flatpak = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(g_chk_flatpak));
    ctx->generation  = g_atomic_int_get(&g_search_generation);
    strncpy(ctx->query, query, sizeof(ctx->query)-1);
    GThread *t = g_thread_new("pkg-search", search_thread, ctx);
    g_thread_unref(t);
}

static void on_entry_activate(GtkWidget *w, gpointer d) { on_search(NULL, NULL); }

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

    const char *term = find_terminal();
    char *term_exec = term ? (char *)term : "xterm";
    char *exec_flag = (term && strcmp(term, "gnome-terminal") == 0) ? "--" : "-e";
    char *argv_upd[] = { term_exec, exec_flag, "sudo",
                     "/tmp/.pkg-helper-upd.sh", NULL };
    GPid  pid;
    GError *err = NULL;

    gtk_label_set_text(GTK_LABEL(g_status), T(STR_APPLYING_UPDATE));

    if (g_spawn_async(NULL, argv_upd, NULL,
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

static void show_changelog_dialog(GtkWidget *parent, gboolean only_latest) {
    GtkWidget *dlg = gtk_dialog_new_with_buttons(
        only_latest ? T(STR_WHATS_NEW_TITLE) : T(STR_CHANGELOG_TITLE),
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
            "v1.2.0-stable", "7 may 2026",
            "• Búsquedas paralelas: pacman, AUR y Flatpak se lanzan a la vez (hasta 3× más rápido con las tres fuentes activas).\n"
            "• Carga de instalados 2×: una sola llamada pacman -Qi reemplaza el patrón anterior de dos subprocesos + tabla hash.\n"
            "• Inserción en TreeView optimizada: gtk_list_store_insert_with_values reduce las señales GTK a la mitad por fila.\n"
            "• Paginación con freeze/thaw: los cambios de página en 'Instalados' ya no parpadean.\n"
            "• Copia de arrays en bloque (g_array_append_vals) en vez de elemento a elemento.\n"
            "• Filtro con debounce (120 ms): no se recalcula en cada tecla, sólo al dejar de escribir.\n"
            "• Fast-path de filtro vacío: sin filtro activo no se crea una copia redundante del array.\n",

            "• Parallel search: pacman, AUR and Flatpak launch simultaneously (up to 3× faster with all three sources enabled).\n"
            "• 2× faster installed-packages load: a single pacman -Qi call replaces the previous two-subprocess + hash-table pattern.\n"
            "• Optimised TreeView insertion: gtk_list_store_insert_with_values halves GTK signals per row.\n"
            "• Freeze/thaw pagination: page changes in the 'Installed' tab no longer flicker.\n"
            "• Bulk array copy (g_array_append_vals) instead of element-by-element loops.\n"
            "• Filter debounce (120 ms): filter is only rebuilt after the user stops typing.\n"
            "• Empty-filter fast-path: no redundant array copy is made when no filter is active.\n"
        },
        {
            "v1.1.0-stable", "5 may 2026",
            "• Carga de paquetes instalados hasta 50× más rápida (un único pacman -Qi en lugar de uno por paquete).\n"
            "• Botones Anterior/Siguiente ahora respetan el idioma seleccionado.\n"
            "• Los botones Instalar/Eliminar funcionan desde cualquier pestaña (Búsqueda, Instalados, Huérfanos).\n"
            "• El filtro de instalados también busca por versión.\n"
            "• Atajo de teclado Ctrl+R para refrescar la pestaña activa.\n"
            "• Corrección: el botón Eliminar mostraba «Descargando» en lugar de «Eliminando».\n"
            "• Corrección: el botón de limpiar filtro ahora funciona correctamente.\n"
            "• Detección de yay/flatpak sin lanzar un shell externo (más seguro).\n",

            "• Installed packages load up to 50× faster (single pacman -Qi instead of one per package).\n"
            "• Previous/Next pagination buttons now respect the selected language.\n"
            "• Install/Remove buttons now work from any tab (Search, Installed, Orphans).\n"
            "• Installed filter also searches by version.\n"
            "• Ctrl+R keyboard shortcut to refresh the active tab.\n"
            "• Fix: Remove button was showing «Downloading» instead of «Removing».\n"
            "• Fix: Filter clear button now works correctly.\n"
            "• yay/flatpak detection no longer spawns an external shell (safer).\n"
        },
        {
            "v1.0.0-stable", "23 april 2026",
            "• Filtro en tiempo real en 'Paquetes instalados'.\n"
            "• Detección automática de terminal (alacritty, kitty, xterm, gnome-terminal...).\n"
            "• Doble clic en paquete muestra información detallada (pacman -Si / flatpak info).\n"
            "• Diálogo de confirmación antes de instalar o eliminar paquetes.\n"
            "• Badge con número de actualizaciones pendientes del sistema.\n"
            "• Historial de las últimas 10 búsquedas con autocompletado.\n",

            "• Real-time filter in the 'Installed packages' tab.\n"
            "• Auto-detection of terminal emulator (alacritty, kitty, xterm, gnome-terminal...).\n"
            "• Double-click on a package shows detailed info (pacman -Si / flatpak info).\n"
            "• Confirmation dialog before installing or removing packages.\n"
            "• Badge showing pending system update count on the Update button.\n"
            "• Search history for the last 10 queries with autocomplete.\n"
        },
        {
            "v0.0.8-stable", "23 april 2026",
            "• boton de discord añadido.\n",
            "• added discord toggle.\n"
        },
        {
            "v0.0.7-stable", "22 april 2026",
            "• Modo oscuro: nuevo toggle On/Off.\n"
            "• Paginación en resultados.\n"
            "• Pestaña 'Paquetes instalados'.\n"
            "• Botón 'Seleccionar página'.\n"
            "• Menú de opciones (⋮) con idioma, modo oscuro y novedades.\n"
            "• Diálogo de novedades al iniciar por primera vez tras una actualización.\n",

            "• Dark mode: new On/Off toggle.\n"
            "• Pagination in search results.\n"
            "• 'Installed packages' tab.\n"
            "• 'Select page' button.\n"
            "• Options menu (⋮) with language, dark mode, and what's new toggle.\n"
            "• What's new dialog on first run after an update.\n"
        },
        {
            "v0.0.6-stable", "21 april 2026",
            "• mejora de busqueda mas precisa.\n",
            "• more precise search improvement.\n"
        },
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
            "• Rendimiento mejorado.\n"
            "• Interfaz respeta idioma.\n",
            "• Performance improvement.\n"
            "• Interface respects language.\n"
        },
        {
            "v0.0.2-beta", "20 april 2026",
            "• Corrección de crash al buscar con fuentes desactivadas.\n",
            "• Fixed crash when searching with all sources disabled.\n"
        },
        {
            "v0.0.1-beta", "20 april 2026",
            "• Versión inicial de PKG Helper.\n",
            "• Initial release of PKG Helper.\n"
        },
    };

    GtkTextIter it;
    gtk_text_buffer_get_start_iter(buf, &it);

    int start = 0;
    int end = only_latest ? 1 : (int)(sizeof(entries)/sizeof(entries[0]));

    for (int i = start; i < end; i++) {
        const Entry *e = &entries[i];
        char header[128];
        snprintf(header, sizeof(header), "%s  (%s)\n", e->ver, e->date);
        gtk_text_buffer_insert_with_tags_by_name(buf, &it, header, -1, "version", NULL);
        const char *body = (g_lang == LANG_EN) ? e->body_en : e->body_es;
        gtk_text_buffer_insert_with_tags_by_name(buf, &it, body, -1, "item", NULL);
        if (i < end - 1)
            gtk_text_buffer_insert(buf, &it, "\n", -1);
    }

    gtk_container_add(GTK_CONTAINER(scroll), tv);
    gtk_box_pack_start(GTK_BOX(content), scroll, TRUE, TRUE, 0);
    gtk_widget_show_all(dlg);
    gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
}

static void on_changelog_clicked(GtkWidget *w, gpointer d) {
    show_changelog_dialog(g_win, FALSE);
}

static void on_update_sys(GtkWidget *w, gpointer d) {
    GString *script = g_string_new("sudo pacman -Syu");
    gchar **no_names = g_new0(gchar *, 1);
    run_in_terminal(script, 0, no_names);
    g_string_free(script, TRUE);
}

static void on_update_all(GtkWidget *w, gpointer d) {
    GString *script = g_string_new("sudo pacman -Syu && yay -Syu && flatpak update");
    gchar **no_names = g_new0(gchar *, 1);
    run_in_terminal(script, 0, no_names);
    g_string_free(script, TRUE);
}

static void on_show_whats_new_toggled(GtkToggleButton *btn, gpointer data) {
    g_show_whats_new = gtk_toggle_button_get_active(btn);
    save_show_whats_new_pref();
}

static void on_lang_es_activate(GtkToggleButton *btn, gpointer data) {
    if (!gtk_toggle_button_get_active(btn)) return;
    g_lang = LANG_ES;
    apply_lang();
    save_lang_pref();
}

static void on_lang_en_activate(GtkToggleButton *btn, gpointer data) {
    if (!gtk_toggle_button_get_active(btn)) return;
    g_lang = LANG_EN;
    apply_lang();
    save_lang_pref();
}

static void on_opt_dark_mode_toggled(GObject *obj, GParamSpec *pspec, gpointer d) {
    g_dark_mode = gtk_switch_get_active(GTK_SWITCH(obj));
    apply_dark_mode();
    save_dark_pref();
}

static GtkWidget* create_options_popover(GtkWidget *relative_to) {
    GtkWidget *popover = gtk_popover_new(relative_to);
    gtk_popover_set_position(GTK_POPOVER(popover), GTK_POS_BOTTOM);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 12);
    gtk_container_add(GTK_CONTAINER(popover), vbox);

    GtkWidget *chk = gtk_check_button_new_with_label(T(STR_OPT_SHOW_WHATS_NEW));
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(chk), g_show_whats_new);
    g_signal_connect(chk, "toggled", G_CALLBACK(on_show_whats_new_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), chk, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 6);

    GtkWidget *lang_label = gtk_label_new(T(STR_OPT_LANGUAGE));
    gtk_widget_set_halign(lang_label, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(vbox), lang_label, FALSE, FALSE, 0);

    GtkWidget *es = gtk_radio_button_new_with_label(NULL, "Español");
    GtkWidget *en = gtk_radio_button_new_with_label_from_widget(GTK_RADIO_BUTTON(es), "English");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(es), g_lang == LANG_ES);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(en), g_lang == LANG_EN);
    g_signal_connect(es, "toggled", G_CALLBACK(on_lang_es_activate), NULL);
    g_signal_connect(en, "toggled", G_CALLBACK(on_lang_en_activate), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), es, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), en, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 6);

    GtkWidget *dark_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *dark_label = gtk_label_new(T(STR_OPT_DARK_MODE));
    gtk_box_pack_start(GTK_BOX(dark_box), dark_label, TRUE, TRUE, 0);
    GtkWidget *dark_switch = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(dark_switch), g_dark_mode);
    g_signal_connect(dark_switch, "notify::active", G_CALLBACK(on_opt_dark_mode_toggled), NULL);
    gtk_box_pack_start(GTK_BOX(dark_box), dark_switch, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), dark_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 6);

    GtkWidget *upd_btn = gtk_button_new_with_label(T(STR_OPT_CHECK_UPDATES));
    g_signal_connect(upd_btn, "clicked", G_CALLBACK(on_check_updates), NULL);
    gtk_box_pack_start(GTK_BOX(vbox), upd_btn, FALSE, FALSE, 0);

    gtk_widget_show_all(vbox);
    return popover;
}

static void on_options_clicked(GtkWidget *btn, gpointer data) {
    GtkWidget *popover = create_options_popover(btn);
    gtk_popover_popup(GTK_POPOVER(popover));
    g_object_ref(popover);
    g_signal_connect_swapped(popover, "closed", G_CALLBACK(g_object_unref), popover);
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
    gtk_button_set_label(GTK_BUTTON(g_inst_refresh_btn), T(STR_BTN_REFRESH_INSTALLED));
    gtk_button_set_label(GTK_BUTTON(g_select_page_btn), T(STR_BTN_SELECT_ALL_PAGE));
    gtk_button_set_label(GTK_BUTTON(g_inst_select_all_btn), T(STR_BTN_SELECT_ALL_PAGE));
    gtk_button_set_label(GTK_BUTTON(g_options_btn), "⋮");
    gtk_widget_set_tooltip_text(g_options_btn, T(STR_OPTIONS_MENU));

    if (g_inst_filter_entry)
        gtk_entry_set_placeholder_text(GTK_ENTRY(g_inst_filter_entry), T(STR_FILTER_PLACEHOLDER));

    if (g_btn_update_sys) {
        if (g_sys_update_count > 0) {
            char lbl[64];
            snprintf(lbl, sizeof(lbl), T(STR_SYS_UPDATES_BADGE), g_sys_update_count);
            gtk_button_set_label(GTK_BUTTON(g_btn_update_sys), lbl);
        } else {
            gtk_button_set_label(GTK_BUTTON(g_btn_update_sys), T(STR_BTN_UPDATE_SYS));
        }
    }
    gtk_button_set_label(GTK_BUTTON(g_btn_update_all), T(STR_BTN_UPDATE_ALL));

    if (g_btn_prev)  gtk_button_set_label(GTK_BUTTON(g_btn_prev),  T(STR_BTN_PREV));
    if (g_btn_next)  gtk_button_set_label(GTK_BUTTON(g_btn_next),  T(STR_BTN_NEXT));
    if (g_inst_prev) gtk_button_set_label(GTK_BUTTON(g_inst_prev), T(STR_BTN_PREV));
    if (g_inst_next) gtk_button_set_label(GTK_BUTTON(g_inst_next), T(STR_BTN_NEXT));

    GtkWidget *tab_label = gtk_label_new(T(STR_TAB_SEARCH));
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(g_notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_notebook), 0), tab_label);
    tab_label = gtk_label_new(T(STR_TAB_INSTALLED));
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(g_notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_notebook), 1), tab_label);
    tab_label = gtk_label_new(T(STR_TAB_ORPHANS));
    gtk_notebook_set_tab_label(GTK_NOTEBOOK(g_notebook), gtk_notebook_get_nth_page(GTK_NOTEBOOK(g_notebook), 2), tab_label);

    if (g_btn_clean_cache) {
        gtk_button_set_label(GTK_BUTTON(g_btn_clean_cache), T(STR_BTN_CLEAN_CACHE));
        gtk_widget_set_tooltip_text(g_btn_clean_cache, T(STR_TOOLTIP_CLEAN_CACHE));
    }
    if (g_orphan_remove_btn)
        gtk_button_set_label(GTK_BUTTON(g_orphan_remove_btn), T(STR_BTN_REMOVE_ORPHANS));
    if (g_inst_col_size_w)
        gtk_tree_view_column_set_title(g_inst_col_size_w, T(STR_COL_SIZE));

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

    if (g_installed_all && g_inst_total_label) {
        char txt[64];
        snprintf(txt, sizeof(txt), T(STR_INST_TOTAL), g_installed_all->len);
        gtk_label_set_text(GTK_LABEL(g_inst_total_label), txt);
    }
}

static gboolean on_window_destroy(GtkWidget *w, GdkEvent *event, gpointer d) {
    save_lang_pref();
    save_dark_pref();
    save_show_whats_new_pref();
    save_column_widths();
    gtk_main_quit();
    return FALSE;
}

static void check_and_show_whats_new(void) {
    if (!g_show_whats_new) return;

    char *version_path = config_path("last_seen_version");
    char *dir = g_path_get_dirname(version_path);
    g_mkdir_with_parents(dir, 0755);
    g_free(dir);

    char saved_version[64] = "";
    FILE *fp = fopen(version_path, "r");
    if (fp) {
        fgets(saved_version, sizeof(saved_version), fp);
        fclose(fp);
        trim(saved_version);
    }

    if (strcmp(saved_version, APP_VERSION) != 0) {
        show_changelog_dialog(g_win, TRUE);
        fp = fopen(version_path, "w");
        if (fp) {
            fputs(APP_VERSION, fp);
            fclose(fp);
        }
    }
    g_free(version_path);
}

static const char DISCORD_SVG[] =
    "<svg xmlns=\"http://www.w3.org/2000/svg\" viewBox=\"0 0 127.14 96.36\">"
    "<path fill=\"white\" d=\"M107.7,8.07A105.15,105.15,0,0,0,81.47,0a72.06,72.06,0,0,0-3.36,"
    "6.83A97.68,97.68,0,0,0,49,6.83,72.37,72.37,0,0,0,45.64,0,105.89,105.89,0,0,0,19.39,8.09C2.79,"
    "32.65-1.71,56.6.54,80.21h0A105.73,105.73,0,0,0,32.71,96.36,77.7,77.7,0,0,0,39.6,85.25a68.42,"
    "68.42,0,0,1-10.85-5.18c.91-.66,1.8-1.34,2.66-2a75.57,75.57,0,0,0,64.32,0c.87.71,1.76,1.39,"
    "2.66,2a68.68,68.68,0,0,1-10.87,5.19,77,77,0,0,0,6.89,11.1A105.25,105.25,0,0,0,126.6,80.22h0C"
    "129.24,52.84,122.09,29.11,107.7,8.07ZM42.45,65.69C36.18,65.69,31,60,31,53s5-12.74,11.43-12.74S"
    "54,46,53.89,53,48.84,65.69,42.45,65.69Zm42.24,0C78.41,65.69,73.25,60,73.25,53s5-12.74,11.44-12.74S"
    "96.23,46,96.12,53,91.08,65.69,84.69,65.69Z\"/>"
    "</svg>";

static const char DISCORD_BTN_CSS[] =
    "#discord_btn {"
    "  background-color: #5865F2;"
    "  background-image: none;"
    "  color: white;"
    "  border-color: #4752c4;"
    "  border-radius: 4px;"
    "  padding: 0 8px;"
    "}"
    "#discord_btn:hover {"
    "  background-color: #4752c4;"
    "  background-image: none;"
    "}"
    "#discord_btn:active {"
    "  background-color: #3c45a5;"
    "  background-image: none;"
    "}"
    "#discord_btn label {"
    "  color: white;"
    "}";

static GdkPixbuf *create_discord_pixbuf(int size) {
    GInputStream *stream = g_memory_input_stream_new_from_data(
        DISCORD_SVG, (gssize)strlen(DISCORD_SVG), NULL);
    GError *err = NULL;
    GdkPixbuf *pb = gdk_pixbuf_new_from_stream_at_scale(
        stream, size, size, TRUE, NULL, &err);
    g_object_unref(stream);
    if (err) {
        g_error_free(err);
        return NULL;
    }
    return pb;
}


typedef struct { GArray *pkgs; int count; } OrphanCtx;

static gboolean orphan_load_done_cb(gpointer data) {
    OrphanCtx *ctx = data;
    gtk_list_store_clear(g_orphan_store);
    for (guint i = 0; i < ctx->pkgs->len; i++) {
        Pkg *p = &g_array_index(ctx->pkgs, Pkg, i);
        GtkTreeIter it;
        gtk_list_store_append(g_orphan_store, &it);
        gtk_list_store_set(g_orphan_store, &it,
            COL_CHECK,  FALSE,
            COL_SOURCE, p->source,
            COL_NAME,   p->name,
            COL_VERSION,p->version,
            COL_DESC,   p->desc,
            COL_CMD,    p->cmd,
            COL_REMOVE_CMD, p->remove_cmd,
            COL_INSTALLED, TRUE,
            COL_SIZE,   p->size,
            -1);
    }
    char lbl[64];
    if (ctx->count == 0)
        gtk_label_set_text(GTK_LABEL(g_orphan_status_lbl), T(STR_ORPHANS_NONE));
    else {
        snprintf(lbl, sizeof(lbl), T(STR_ORPHANS_FOUND), ctx->count);
        gtk_label_set_text(GTK_LABEL(g_orphan_status_lbl), lbl);
    }
    gtk_widget_set_sensitive(g_orphan_remove_btn, ctx->count > 0);
    gtk_spinner_stop(GTK_SPINNER(g_orphan_spinner));
    g_array_free(ctx->pkgs, TRUE);
    g_free(ctx);
    return G_SOURCE_REMOVE;
}

static gpointer load_orphans_thread(gpointer data) {
    (void)data;
    GArray *pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));
    FILE *fp = popen("pacman -Qdtq 2>/dev/null", "r");
    if (fp) {
        char line[256];
        while (fgets(line, sizeof(line), fp)) {
            trim(line);
            if (!line[0]) continue;
            Pkg p = {0};
            strncpy(p.source,  "pacman",          sizeof(p.source)-1);
            strncpy(p.name,    line,               sizeof(p.name)-1);
            char qcmd[512];
            snprintf(qcmd, sizeof(qcmd),
                "pacman -Qi '%s' 2>/dev/null | grep -iE '^(Version|Installed Size|Description)'",
                p.name);
            FILE *fp2 = popen(qcmd, "r");
            if (fp2) {
                char tmp[512];
                while (fgets(tmp, sizeof(tmp), fp2)) {
                    char *col = strchr(tmp, ':'); if (!col) continue;
                    *col = '\0'; trim(tmp); char *val = col+1; trim(val);
                    if (strcasecmp(tmp, "Version") == 0)
                        strncpy(p.version, val, sizeof(p.version)-1);
                    else if (strcasecmp(tmp, "Installed Size") == 0)
                        strncpy(p.size, val, sizeof(p.size)-1);
                    else if (strcasecmp(tmp, "Description") == 0)
                        strncpy(p.desc, val, sizeof(p.desc)-1);
                }
                pclose(fp2);
            }
            snprintf(p.remove_cmd, sizeof(p.remove_cmd), "sudo pacman -Rns %s", p.name);
            g_array_append_val(pkgs, p);
        }
        pclose(fp);
    }
    OrphanCtx *ctx = g_new0(OrphanCtx, 1);
    ctx->pkgs  = pkgs;
    ctx->count = (int)pkgs->len;
    g_idle_add(orphan_load_done_cb, ctx);
    return NULL;
}

static void on_orphan_refresh(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    gtk_list_store_clear(g_orphan_store);
    gtk_label_set_text(GTK_LABEL(g_orphan_status_lbl), T(STR_ORPHANS_LOADING));
    gtk_widget_set_sensitive(g_orphan_remove_btn, FALSE);
    gtk_spinner_start(GTK_SPINNER(g_orphan_spinner));
    GThread *t = g_thread_new("load-orphans", load_orphans_thread, NULL);
    g_thread_unref(t);
}

static void on_orphan_remove(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    GtkTreeModel *model = GTK_TREE_MODEL(g_orphan_store);
    GtkTreeIter iter;
    GPtrArray *names = g_ptr_array_new_with_free_func(g_free);
    GString   *script = g_string_new("sudo pacman -Rns");
    gboolean valid = gtk_tree_model_get_iter_first(model, &iter);
    while (valid) {
        gchar *nm = NULL;
        gtk_tree_model_get(model, &iter, COL_NAME, &nm, -1);
        if (nm) { g_string_append_printf(script, " %s", nm); g_ptr_array_add(names, nm); }
        valid = gtk_tree_model_iter_next(model, &iter);
    }
    if (!names->len) { g_ptr_array_free(names, TRUE); g_string_free(script, TRUE); return; }
    g_ptr_array_add(names, NULL);
    gchar **pkg_names = (gchar **)g_ptr_array_free(names, FALSE);
    gtk_widget_set_sensitive(g_orphan_remove_btn, FALSE);
    run_in_terminal(script, -1, pkg_names);
    g_string_free(script, TRUE);
}


static void on_clean_cache(GtkWidget *w, gpointer d) {
    (void)w; (void)d;
    GString *script = g_string_new(
        "if which paccache >/dev/null 2>&1; then sudo paccache -r; "
        "else sudo pacman -Sc; fi");
    gchar **no_names = g_new0(gchar *, 1);
    run_in_terminal(script, 0, no_names);
    g_string_free(script, TRUE);
}

static void on_discord_clicked(GtkWidget *btn, gpointer data) {
    (void)btn; (void)data;
    gtk_show_uri_on_window(GTK_WINDOW(g_win), "https://discord.gg/esSm9wEcHQ", GDK_CURRENT_TIME, NULL);
}

static gboolean on_window_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data) {
    (void)widget; (void)data;
    guint key  = event->keyval;
    guint mods = event->state & gtk_accelerator_get_default_mod_mask();

    if (key == GDK_KEY_f && mods == GDK_CONTROL_MASK) {
        gtk_widget_grab_focus(g_entry);
        return TRUE;
    }
    if (key == GDK_KEY_F5 && mods == 0) {
        gint tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(g_notebook));
        if (tab == 1)
            on_installed_refresh(NULL, NULL);
        else if (tab == 2)
            on_orphan_refresh(NULL, NULL);
        return TRUE;
    }
    if ((key == GDK_KEY_r || key == GDK_KEY_R) && mods == GDK_CONTROL_MASK) {
        gint tab = gtk_notebook_get_current_page(GTK_NOTEBOOK(g_notebook));
        if (tab == 1)
            on_installed_refresh(NULL, NULL);
        else if (tab == 2)
            on_orphan_refresh(NULL, NULL);
        return TRUE;
    }
    if (key == GDK_KEY_Escape && mods == 0) {
        if (gtk_widget_has_focus(g_entry)) {
            gtk_entry_set_text(GTK_ENTRY(g_entry), "");
            return TRUE;
        }
    }
    return FALSE;
}

static void build_ui(void) {
    g_win = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(g_win), "PKG Helper - Arch Linux");
    gtk_window_set_default_size(GTK_WINDOW(g_win), 920, 580);
    gtk_window_set_position(GTK_WINDOW(g_win), GTK_WIN_POS_CENTER);
    g_signal_connect(g_win, "delete-event", G_CALLBACK(on_window_destroy), NULL);
    g_signal_connect(g_win, "key-press-event", G_CALLBACK(on_window_key_press), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(vbox), 10);
    gtk_container_add(GTK_CONTAINER(g_win), vbox);

    GtkWidget *hbox_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_entry), T(STR_PLACEHOLDER));
    g_signal_connect(g_entry, "activate", G_CALLBACK(on_entry_activate), NULL);
    if (g_hist_store) {
        GtkEntryCompletion *comp = gtk_entry_completion_new();
        gtk_entry_completion_set_model(comp, GTK_TREE_MODEL(g_hist_store));
        gtk_entry_completion_set_text_column(comp, 0);
        gtk_entry_completion_set_minimum_key_length(comp, 1);
        gtk_entry_completion_set_inline_completion(comp, TRUE);
        gtk_entry_set_completion(GTK_ENTRY(g_entry), comp);
        g_object_unref(comp);
    }
    gtk_box_pack_start(GTK_BOX(hbox_top), g_entry, TRUE, TRUE, 0);

    g_btn_search = gtk_button_new_with_label(T(STR_BTN_SEARCH));
    g_signal_connect(g_btn_search, "clicked", G_CALLBACK(on_search), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_search, FALSE, FALSE, 0);

    g_spinner = gtk_spinner_new();
    gtk_box_pack_start(GTK_BOX(hbox_top), g_spinner, FALSE, FALSE, 4);

    g_btn_update = gtk_button_new_with_label(T(STR_BTN_CHECK_UPDATES));
    gtk_widget_set_tooltip_text(g_btn_update, T(STR_TOOLTIP_UPDATE));
    g_signal_connect(g_btn_update, "clicked", G_CALLBACK(on_check_updates), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_update, FALSE, FALSE, 0);

    g_btn_changelog = gtk_button_new_with_label(T(STR_BTN_CHANGELOG));
    gtk_widget_set_tooltip_text(g_btn_changelog, T(STR_BTN_CHANGELOG));
    g_signal_connect(g_btn_changelog, "clicked", G_CALLBACK(on_changelog_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_btn_changelog, FALSE, FALSE, 0);

    g_options_btn = gtk_button_new_with_label("⋮");
    gtk_widget_set_tooltip_text(g_options_btn, T(STR_OPTIONS_MENU));
    g_signal_connect(g_options_btn, "clicked", G_CALLBACK(on_options_clicked), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_top), g_options_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(vbox), hbox_top, FALSE, FALSE, 0);

    g_notebook = gtk_notebook_new();
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(g_notebook), TRUE);
    gtk_box_pack_start(GTK_BOX(vbox), g_notebook, TRUE, TRUE, 0);

    GtkWidget *search_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(search_tab), 6);

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
    gtk_box_pack_start(GTK_BOX(search_tab), hbox_src, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(search_tab),
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
        G_TYPE_BOOLEAN,
        G_TYPE_STRING);

    g_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_store));
    g_object_unref(g_store);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(g_tree), FALSE);
    g_signal_connect(g_tree, "row-activated", G_CALLBACK(on_row_activated), g_store);

    GtkCellRenderer *toggle_r = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggle_r, "toggled", G_CALLBACK(on_toggle), g_store);
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

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), g_tree);
    gtk_box_pack_start(GTK_BOX(search_tab), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox_pag = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_halign(hbox_pag, GTK_ALIGN_CENTER);

    g_btn_prev = gtk_button_new_with_label(T(STR_BTN_PREV));
    g_signal_connect(g_btn_prev, "clicked", G_CALLBACK(on_page_prev), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_pag), g_btn_prev, FALSE, FALSE, 0);

    g_page_label = gtk_label_new("1 / 1");
    gtk_widget_set_margin_start(g_page_label, 8);
    gtk_widget_set_margin_end(g_page_label, 8);
    gtk_box_pack_start(GTK_BOX(hbox_pag), g_page_label, FALSE, FALSE, 0);

    g_btn_next = gtk_button_new_with_label(T(STR_BTN_NEXT));
    g_signal_connect(g_btn_next, "clicked", G_CALLBACK(on_page_next), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_pag), g_btn_next, FALSE, FALSE, 0);

    g_select_page_btn = gtk_button_new_with_label(T(STR_BTN_SELECT_ALL_PAGE));
    g_signal_connect(g_select_page_btn, "clicked", G_CALLBACK(on_select_page), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_pag), g_select_page_btn, FALSE, FALSE, 4);

    gtk_widget_set_visible(g_btn_prev,   FALSE);
    gtk_widget_set_visible(g_btn_next,   FALSE);
    gtk_widget_set_visible(g_page_label, FALSE);
    gtk_widget_set_visible(g_select_page_btn, FALSE);

    gtk_box_pack_start(GTK_BOX(search_tab), hbox_pag, FALSE, FALSE, 4);

    gtk_notebook_append_page(GTK_NOTEBOOK(g_notebook), search_tab,
                             gtk_label_new(T(STR_TAB_SEARCH)));

    GtkWidget *inst_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_set_border_width(GTK_CONTAINER(inst_tab), 6);

    GtkWidget *hbox_inst_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    g_inst_refresh_btn = gtk_button_new_with_label(T(STR_BTN_REFRESH_INSTALLED));
    g_signal_connect(g_inst_refresh_btn, "clicked", G_CALLBACK(on_installed_refresh), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_inst_top), g_inst_refresh_btn, FALSE, FALSE, 0);

    g_inst_select_all_btn = gtk_button_new_with_label(T(STR_BTN_SELECT_ALL_PAGE));
    g_signal_connect(g_inst_select_all_btn, "clicked", G_CALLBACK(on_installed_select_all), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_inst_top), g_inst_select_all_btn, FALSE, FALSE, 0);

    g_inst_filter_entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(g_inst_filter_entry), T(STR_FILTER_PLACEHOLDER));
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(g_inst_filter_entry),
        GTK_ENTRY_ICON_PRIMARY, "edit-find-symbolic");
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(g_inst_filter_entry),
        GTK_ENTRY_ICON_SECONDARY, "edit-clear-symbolic");
    g_signal_connect(g_inst_filter_entry, "changed",
        G_CALLBACK(on_inst_filter_changed), NULL);
    g_signal_connect(g_inst_filter_entry, "icon-press",
        G_CALLBACK(on_filter_icon_press), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_inst_top), g_inst_filter_entry, TRUE, TRUE, 4);

    g_inst_spinner = gtk_spinner_new();
    gtk_box_pack_end(GTK_BOX(hbox_inst_top), g_inst_spinner, FALSE, FALSE, 4);

    gtk_box_pack_start(GTK_BOX(inst_tab), hbox_inst_top, FALSE, FALSE, 0);

    g_inst_store = gtk_list_store_new(N_COLS,
        G_TYPE_BOOLEAN,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_STRING,
        G_TYPE_BOOLEAN,
        G_TYPE_STRING);

    g_inst_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_inst_store));
    g_object_unref(g_inst_store);
    gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(g_inst_tree), FALSE);
    g_signal_connect(g_inst_tree, "row-activated", G_CALLBACK(on_row_activated), g_inst_store);

    toggle_r = gtk_cell_renderer_toggle_new();
    g_signal_connect(toggle_r, "toggled", G_CALLBACK(on_toggle), g_inst_store);
    toggle_col = gtk_tree_view_column_new_with_attributes(
        "", toggle_r, "active", COL_CHECK, NULL);
    gtk_tree_view_column_set_min_width(toggle_col, 30);
    gtk_tree_view_append_column(GTK_TREE_VIEW(g_inst_tree), toggle_col);

    {
        GtkCellRenderer *r = gtk_cell_renderer_text_new();
        g_object_set(r, "foreground", "#44aa44", NULL);
        GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
            T(STR_COL_STATUS), r, "text", COL_STATUS, NULL);
        gtk_tree_view_column_set_min_width(c, 90);
        gtk_tree_view_append_column(GTK_TREE_VIEW(g_inst_tree), c);
    }

    {
        struct { int id; const char *title; int min_w; gboolean expand; GtkTreeViewColumn **ref; } cols[] = {
            { COL_SOURCE,  T(STR_COL_SOURCE),  80,  FALSE, NULL              },
            { COL_NAME,    T(STR_COL_NAME),    160, FALSE, NULL              },
            { COL_VERSION, T(STR_COL_VERSION), 90,  FALSE, NULL              },
            { COL_SIZE,    T(STR_COL_SIZE),    90,  FALSE, &g_inst_col_size_w},
            { COL_DESC,    T(STR_COL_DESC),    200, TRUE,  NULL              },
        };
        for (int i=0; i<5; i++) {
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
            if (cols[i].ref) *cols[i].ref = c;
            gtk_tree_view_append_column(GTK_TREE_VIEW(g_inst_tree), c);
        }
    }

    scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll),
        GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), g_inst_tree);
    gtk_box_pack_start(GTK_BOX(inst_tab), scroll, TRUE, TRUE, 0);

    GtkWidget *hbox_inst_pag = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_widget_set_halign(hbox_inst_pag, GTK_ALIGN_CENTER);

    g_inst_prev = gtk_button_new_with_label(T(STR_BTN_PREV));
    g_signal_connect(g_inst_prev, "clicked", G_CALLBACK(on_inst_page_prev), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_inst_pag), g_inst_prev, FALSE, FALSE, 0);

    g_inst_page_label = gtk_label_new("1 / 1");
    gtk_widget_set_margin_start(g_inst_page_label, 8);
    gtk_widget_set_margin_end(g_inst_page_label, 8);
    gtk_box_pack_start(GTK_BOX(hbox_inst_pag), g_inst_page_label, FALSE, FALSE, 0);

    g_inst_next = gtk_button_new_with_label(T(STR_BTN_NEXT));
    g_signal_connect(g_inst_next, "clicked", G_CALLBACK(on_inst_page_next), NULL);
    gtk_box_pack_start(GTK_BOX(hbox_inst_pag), g_inst_next, FALSE, FALSE, 0);

    g_inst_total_label = gtk_label_new("");
    gtk_box_pack_start(GTK_BOX(hbox_inst_pag), g_inst_total_label, FALSE, FALSE, 16);

    gtk_widget_set_visible(g_inst_prev, FALSE);
    gtk_widget_set_visible(g_inst_next, FALSE);
    gtk_widget_set_visible(g_inst_page_label, FALSE);

    gtk_box_pack_start(GTK_BOX(inst_tab), hbox_inst_pag, FALSE, FALSE, 4);

    gtk_notebook_append_page(GTK_NOTEBOOK(g_notebook), inst_tab,
                             gtk_label_new(T(STR_TAB_INSTALLED)));

    {
        GtkWidget *orp_tab = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(orp_tab), 6);

        GtkWidget *hbox_orp_top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
        GtkWidget *orp_refresh_btn = gtk_button_new_with_label("↻");
        gtk_widget_set_tooltip_text(orp_refresh_btn, T(STR_BTN_REFRESH_INSTALLED));
        g_signal_connect(orp_refresh_btn, "clicked", G_CALLBACK(on_orphan_refresh), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_orp_top), orp_refresh_btn, FALSE, FALSE, 0);

        g_orphan_remove_btn = gtk_button_new_with_label(T(STR_BTN_REMOVE_ORPHANS));
        gtk_widget_set_tooltip_text(g_orphan_remove_btn, T(STR_TOOLTIP_REMOVE_ORPHANS));
        gtk_widget_set_sensitive(g_orphan_remove_btn, FALSE);
        g_signal_connect(g_orphan_remove_btn, "clicked", G_CALLBACK(on_orphan_remove), NULL);
        gtk_box_pack_start(GTK_BOX(hbox_orp_top), g_orphan_remove_btn, FALSE, FALSE, 0);

        g_orphan_status_lbl = gtk_label_new("");
        gtk_widget_set_halign(g_orphan_status_lbl, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox_orp_top), g_orphan_status_lbl, TRUE, TRUE, 8);

        g_orphan_spinner = gtk_spinner_new();
        gtk_box_pack_end(GTK_BOX(hbox_orp_top), g_orphan_spinner, FALSE, FALSE, 4);
        gtk_box_pack_start(GTK_BOX(orp_tab), hbox_orp_top, FALSE, FALSE, 0);

        g_orphan_store = gtk_list_store_new(N_COLS,
            G_TYPE_BOOLEAN, G_TYPE_STRING, G_TYPE_STRING,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING,
            G_TYPE_STRING, G_TYPE_STRING, G_TYPE_BOOLEAN, G_TYPE_STRING);

        g_orphan_tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(g_orphan_store));
        g_object_unref(g_orphan_store);
        gtk_tree_view_set_activate_on_single_click(GTK_TREE_VIEW(g_orphan_tree), FALSE);
        g_signal_connect(g_orphan_tree, "row-activated", G_CALLBACK(on_row_activated), g_orphan_store);

        {
            struct { int id; const char *title; int min_w; gboolean expand; } ocols[] = {
                { COL_NAME,    T(STR_COL_NAME),    160, FALSE },
                { COL_VERSION, T(STR_COL_VERSION), 90,  FALSE },
                { COL_SIZE,    T(STR_COL_SIZE),    90,  FALSE },
                { COL_DESC,    T(STR_COL_DESC),    200, TRUE  },
            };
            for (int i=0; i<4; i++) {
                GtkCellRenderer *r = gtk_cell_renderer_text_new();
                GtkTreeViewColumn *c = gtk_tree_view_column_new_with_attributes(
                    ocols[i].title, r, "text", ocols[i].id, NULL);
                gtk_tree_view_column_set_resizable(c, TRUE);
                gtk_tree_view_column_set_sort_column_id(c, ocols[i].id);
                gtk_tree_view_column_set_min_width(c, ocols[i].min_w);
                if (ocols[i].expand) {
                    gtk_tree_view_column_set_expand(c, TRUE);
                    g_object_set(r, "ellipsize", PANGO_ELLIPSIZE_END, NULL);
                }
                gtk_tree_view_append_column(GTK_TREE_VIEW(g_orphan_tree), c);
            }
        }

        GtkWidget *orp_scroll = gtk_scrolled_window_new(NULL, NULL);
        gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(orp_scroll),
            GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
        gtk_container_add(GTK_CONTAINER(orp_scroll), g_orphan_tree);
        gtk_box_pack_start(GTK_BOX(orp_tab), orp_scroll, TRUE, TRUE, 0);

        gtk_notebook_append_page(GTK_NOTEBOOK(g_notebook), orp_tab,
                                 gtk_label_new(T(STR_TAB_ORPHANS)));
    }

    gtk_box_pack_start(GTK_BOX(vbox),
        gtk_separator_new(GTK_ORIENTATION_HORIZONTAL), FALSE, FALSE, 2);

    GtkWidget *hbox_bot = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    g_status = gtk_label_new(T(STR_STATUS_READY));
    gtk_widget_set_name(g_status, "status_label");
    gtk_label_set_xalign(GTK_LABEL(g_status), 0.0);
    gtk_label_set_ellipsize(GTK_LABEL(g_status), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(hbox_bot), g_status, TRUE, TRUE, 0);

    g_ver_label = gtk_label_new(APP_VERSION);
    gtk_widget_set_opacity(g_ver_label, 0.35);
    gtk_widget_set_tooltip_text(g_ver_label, T(STR_BTN_CHANGELOG));
    gtk_box_pack_start(GTK_BOX(hbox_bot), g_ver_label, FALSE, FALSE, 4);

    g_btn_clean_cache = gtk_button_new_with_label(T(STR_BTN_CLEAN_CACHE));
    gtk_widget_set_tooltip_text(g_btn_clean_cache, T(STR_TOOLTIP_CLEAN_CACHE));
    g_signal_connect(g_btn_clean_cache, "clicked", G_CALLBACK(on_clean_cache), NULL);
    gtk_box_pack_end(GTK_BOX(hbox_bot), g_btn_clean_cache, FALSE, FALSE, 0);

    {
        GtkCssProvider *discord_css = gtk_css_provider_new();
        gtk_css_provider_load_from_data(discord_css, DISCORD_BTN_CSS, -1, NULL);
        gtk_style_context_add_provider_for_screen(
            gdk_screen_get_default(),
            GTK_STYLE_PROVIDER(discord_css),
            GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
        g_object_unref(discord_css);

        GtkWidget *btn_discord = gtk_button_new();
        gtk_widget_set_name(btn_discord, "discord_btn");
        gtk_widget_set_tooltip_text(btn_discord, "Join our Discord — discord.gg/esSm9wEcHQ");
        g_signal_connect(btn_discord, "clicked", G_CALLBACK(on_discord_clicked), NULL);

        GtkWidget *discord_hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
        gtk_widget_set_valign(discord_hbox, GTK_ALIGN_CENTER);

        GdkPixbuf *discord_pb = create_discord_pixbuf(16);
        if (discord_pb) {
            GtkWidget *discord_img = gtk_image_new_from_pixbuf(discord_pb);
            g_object_unref(discord_pb);
            gtk_box_pack_start(GTK_BOX(discord_hbox), discord_img, FALSE, FALSE, 0);
        }

        GtkWidget *discord_lbl = gtk_label_new("Discord");
        gtk_box_pack_start(GTK_BOX(discord_hbox), discord_lbl, FALSE, FALSE, 0);
        gtk_container_add(GTK_CONTAINER(btn_discord), discord_hbox);
        gtk_box_pack_start(GTK_BOX(hbox_bot), btn_discord, FALSE, FALSE, 0);
    }

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
    load_dark_pref();
    load_show_whats_new_pref();
    load_search_history();

    g_all_pkgs = g_array_new(FALSE, TRUE, sizeof(Pkg));

    build_ui();
    apply_dark_mode();
    load_column_widths();

    GdkPixbuf *win_icon = gdk_pixbuf_new_from_file(UPD_ICO_DST, NULL);
    if (win_icon) {
        gtk_window_set_icon(GTK_WINDOW(g_win), win_icon);
        g_object_unref(win_icon);
    }

    gtk_widget_show_all(g_win);

    on_installed_refresh(NULL, NULL);

    {
        GThread *t = g_thread_new("sys-updates", check_sys_updates_thread, NULL);
        g_thread_unref(t);
    }

    check_and_show_whats_new();

    gtk_main();
    return 0;
}
