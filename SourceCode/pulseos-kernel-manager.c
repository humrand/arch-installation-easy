#define _GNU_SOURCE
#include <gtk/gtk.h>
#include <gio/gio.h>
#include <glib.h>
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>

#define APP_ID       "com.pulseos.kernelmanager"
#define APP_TITLE    "PulseOS kernel manager"
#define PAGE_SIZE    20
#define MAX_KERNELS  256
#define MAX_TEXT     1024

#define KM_VERSION   "0.0.1"
#define UPD_URL_BIN  "https://raw.githubusercontent.com/PulseOS-community/" \
                     "pulseos/main/tools/pulseos-kernel-manager"
#define UPD_BIN_DST  "/usr/local/bin/pulseos-kernel-manager"
#define UPD_TMP_BIN  "/tmp/.pulseos-km.upd"

typedef struct {
    gchar  source[32];
    gchar  name[128];
    gchar  version[64];
    gchar  desc[512];
    gchar  size[64];
    gchar  install_cmd[256];
    gchar  remove_cmd[256];
    gboolean installed;
} KernelEntry;

enum {
    COL_CHECK = 0,
    COL_STATUS,
    COL_SOURCE,
    COL_NAME,
    COL_VERSION,
    COL_DESC,
    COL_SIZE,
    COL_INSTALL_CMD,
    COL_REMOVE_CMD,
    COL_INSTALLED,
    N_COLS
};

typedef struct {
    GtkApplication *app;
    GtkWidget      *window;
    GtkWidget      *notebook;
    GtkWidget      *status_label;
    GtkWidget      *spinner;

    GtkWidget      *avail_refresh_btn;
    GtkWidget      *avail_install_btn;
    GtkWidget      *avail_select_btn;
    GtkWidget      *avail_filter_entry;
    GtkTreeView    *avail_tree;
    GtkListStore   *avail_store;
    GArray         *avail_all;
    gint            avail_page;

    GtkWidget      *inst_refresh_btn;
    GtkWidget      *inst_remove_btn;
    GtkWidget      *inst_select_btn;
    GtkWidget      *inst_filter_entry;
    GtkTreeView    *inst_tree;
    GtkListStore   *inst_store;
    GArray         *inst_all;
    gint            inst_page;

    gchar           avail_filter[128];
    gchar           inst_filter[128];

    gboolean        busy;

    GtkWidget      *update_btn;
    GtkWidget      *ver_label;
} AppState;

static AppState *g_app_state = NULL;

static gboolean is_kernel_name(const gchar *name) {
    if (!name || !*name) return FALSE;
    if (!g_str_has_prefix(name, "linux")) return FALSE;
    if (g_str_has_suffix(name, "-headers")) return FALSE;
    if (g_str_has_suffix(name, "-docs")) return FALSE;
    if (g_str_has_prefix(name, "linux-firmware")) return FALSE;
    if (g_str_has_prefix(name, "linux-api-headers")) return FALSE;
    if (g_str_has_prefix(name, "linux-tools")) return FALSE;
    return TRUE;
}

static gchar *trim_dup(const gchar *s) {
    if (!s) return g_strdup("");
    gchar *cpy = g_strdup(s);
    g_strstrip(cpy);
    return cpy;
}

static void entry_clear(KernelEntry *e) {
    memset(e, 0, sizeof(*e));
}

static void append_entry(GArray *arr, const KernelEntry *src) {
    if (!arr || !src) return;
    g_array_append_val(arr, *src);
}

static void set_status(AppState *st, const gchar *text) {
    gtk_label_set_text(GTK_LABEL(st->status_label), text ? text : "");
}

static gchar *capture_command(const gchar *cmd) {
    if (!cmd) return g_strdup("");
    FILE *fp = popen(cmd, "r");
    if (!fp) return g_strdup("");
    GString *out = g_string_new(NULL);
    gchar buf[512];
    while (fgets(buf, sizeof(buf), fp)) {
        g_string_append(out, buf);
    }
    pclose(fp);
    return g_string_free(out, FALSE);
}

static GHashTable *build_installed_set(void) {
    GHashTable *set = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    FILE *fp = popen("pacman -Qq 2>/dev/null", "r");
    if (!fp) return set;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        g_strstrip(line);
        if (is_kernel_name(line)) g_hash_table_add(set, g_strdup(line));
    }
    pclose(fp);
    return set;
}

static void parse_pacman_search_output(const gchar *output,
                                       const gchar *default_source,
                                       GHashTable  *seen,
                                       GArray      *out,
                                       GHashTable  *installed)
{
    if (!output || !out) return;

    gchar **lines = g_strsplit(output, "\n", -1);
    KernelEntry cur;
    gboolean have = FALSE;
    entry_clear(&cur);
    if (default_source && *default_source)
        g_strlcpy(cur.source, default_source, sizeof(cur.source));

    for (gint i = 0; lines[i]; i++) {
        gchar *line = lines[i];
        g_strstrip(line);
        if (!*line) {
            if (have && is_kernel_name(cur.name)) {
                if (!seen || !g_hash_table_contains(seen, cur.name)) {
                    if (seen) g_hash_table_add(seen, g_strdup(cur.name));
                    if (installed && g_hash_table_contains(installed, cur.name))
                        cur.installed = TRUE;
                    snprintf(cur.install_cmd, sizeof(cur.install_cmd), "sudo pacman -S --needed %s", cur.name);
                    snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
                    append_entry(out, &cur);
                }
            }
            have = FALSE;
            entry_clear(&cur);
            if (default_source && *default_source)
                g_strlcpy(cur.source, default_source, sizeof(cur.source));
            continue;
        }

        if (g_ascii_isspace((guchar)line[0])) {
            if (have && !cur.desc[0]) {
                g_strlcpy(cur.desc, line, sizeof(cur.desc));
                g_strstrip(cur.desc);
            }
            continue;
        }

        if (have && is_kernel_name(cur.name)) {
            if (!seen || !g_hash_table_contains(seen, cur.name)) {
                if (seen) g_hash_table_add(seen, g_strdup(cur.name));
                if (installed && g_hash_table_contains(installed, cur.name))
                    cur.installed = TRUE;
                snprintf(cur.install_cmd, sizeof(cur.install_cmd), "sudo pacman -S --needed %s", cur.name);
                snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
                append_entry(out, &cur);
            }
        }

        entry_clear(&cur);

        gchar *slash = strchr(line, '/');
        gchar *space = strchr(line, ' ');
        if (!slash || !space || slash > space) {
            have = FALSE;
            continue;
        }

        *slash = '\0';
        *space = '\0';

        g_strlcpy(cur.source, line, sizeof(cur.source));
        g_strlcpy(cur.name, slash + 1, sizeof(cur.name));

        gchar *ver_start = space + 1;
        gchar *ver_end = strchr(ver_start, ' ');
        if (ver_end) *ver_end = '\0';
        g_strlcpy(cur.version, ver_start, sizeof(cur.version));
        if (ver_end && *(ver_end + 1))
            g_strlcpy(cur.desc, ver_end + 1, sizeof(cur.desc));

        have = TRUE;
    }

    if (have && is_kernel_name(cur.name)) {
        if (!seen || !g_hash_table_contains(seen, cur.name)) {
            if (seen) g_hash_table_add(seen, g_strdup(cur.name));
            if (installed && g_hash_table_contains(installed, cur.name))
                cur.installed = TRUE;
            snprintf(cur.install_cmd, sizeof(cur.install_cmd), "sudo pacman -S --needed %s", cur.name);
            snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
            append_entry(out, &cur);
        }
    }

    g_strfreev(lines);
}

static void parse_yay_search_output(const gchar *output,
                                    GHashTable  *seen,
                                    GArray      *out,
                                    GHashTable  *installed)
{
    if (!output || !out) return;

    gchar **lines = g_strsplit(output, "\n", -1);
    KernelEntry cur;
    gboolean have = FALSE;
    entry_clear(&cur);
    g_strlcpy(cur.source, "aur", sizeof(cur.source));

    for (gint i = 0; lines[i]; i++) {
        gchar *line = lines[i];
        g_strstrip(line);
        if (!*line) {
            if (have && is_kernel_name(cur.name)) {
                if (!seen || !g_hash_table_contains(seen, cur.name)) {
                    if (seen) g_hash_table_add(seen, g_strdup(cur.name));
                    if (installed && g_hash_table_contains(installed, cur.name))
                        cur.installed = TRUE;
                    snprintf(cur.install_cmd, sizeof(cur.install_cmd), "yay -S --needed %s", cur.name);
                    snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
                    append_entry(out, &cur);
                }
            }
            have = FALSE;
            entry_clear(&cur);
            g_strlcpy(cur.source, "aur", sizeof(cur.source));
            continue;
        }

        if (g_ascii_isspace((guchar)line[0])) {
            if (have && !cur.desc[0]) {
                g_strlcpy(cur.desc, line, sizeof(cur.desc));
                g_strstrip(cur.desc);
            }
            continue;
        }

        if (have && is_kernel_name(cur.name)) {
            if (!seen || !g_hash_table_contains(seen, cur.name)) {
                if (seen) g_hash_table_add(seen, g_strdup(cur.name));
                if (installed && g_hash_table_contains(installed, cur.name))
                    cur.installed = TRUE;
                snprintf(cur.install_cmd, sizeof(cur.install_cmd), "yay -S --needed %s", cur.name);
                snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
                append_entry(out, &cur);
            }
        }

        entry_clear(&cur);
        g_strlcpy(cur.source, "aur", sizeof(cur.source));

        gchar *slash = strchr(line, '/');
        gchar *space = strchr(line, ' ');
        if (!slash || !space || slash > space) {
            have = FALSE;
            continue;
        }

        *slash = '\0';
        *space = '\0';

        g_strlcpy(cur.name, slash + 1, sizeof(cur.name));
        gchar *ver_start = space + 1;
        gchar *ver_end = strchr(ver_start, ' ');
        if (ver_end) *ver_end = '\0';
        g_strlcpy(cur.version, ver_start, sizeof(cur.version));
        if (ver_end && *(ver_end + 1))
            g_strlcpy(cur.desc, ver_end + 1, sizeof(cur.desc));

        have = TRUE;
    }

    if (have && is_kernel_name(cur.name)) {
        if (!seen || !g_hash_table_contains(seen, cur.name)) {
            if (seen) g_hash_table_add(seen, g_strdup(cur.name));
            if (installed && g_hash_table_contains(installed, cur.name))
                cur.installed = TRUE;
            snprintf(cur.install_cmd, sizeof(cur.install_cmd), "yay -S --needed %s", cur.name);
            snprintf(cur.remove_cmd, sizeof(cur.remove_cmd), "sudo pacman -Rns %s", cur.name);
            append_entry(out, &cur);
        }
    }

    g_strfreev(lines);
}

static GArray *scan_available_kernels(void) {
    GArray *arr = g_array_new(FALSE, TRUE, sizeof(KernelEntry));
    GHashTable *seen = g_hash_table_new_full(g_str_hash, g_str_equal, g_free, NULL);
    GHashTable *installed = build_installed_set();

    gchar *repo_out = capture_command("pacman -Ss '^linux' 2>/dev/null");
    parse_pacman_search_output(repo_out, "repo", seen, arr, installed);
    g_free(repo_out);

    gchar *yay_path = g_find_program_in_path("yay");
    if (yay_path) {
        gchar *aur_out = capture_command("yay --color=never -Ss --aur '^linux' 2>/dev/null");
        parse_yay_search_output(aur_out, seen, arr, installed);
        g_free(aur_out);
        g_free(yay_path);
    }

    g_hash_table_destroy(seen);
    g_hash_table_destroy(installed);
    return arr;
}

static void parse_pkg_info_block(const gchar *output, KernelEntry *entry) {
    if (!output || !entry) return;
    gchar **lines = g_strsplit(output, "\n", -1);
    for (gint i = 0; lines[i]; i++) {
        gchar *line = lines[i];
        gchar *colon = strchr(line, ':');
        if (!colon) continue;
        *colon = '\0';
        gchar *key = line;
        gchar *val = colon + 1;
        g_strstrip(key);
        g_strstrip(val);
        if (g_ascii_strcasecmp(key, "Name") == 0) {
            g_strlcpy(entry->name, val, sizeof(entry->name));
        } else if (g_ascii_strcasecmp(key, "Version") == 0) {
            g_strlcpy(entry->version, val, sizeof(entry->version));
        } else if (g_ascii_strcasecmp(key, "Description") == 0) {
            g_strlcpy(entry->desc, val, sizeof(entry->desc));
        } else if (g_ascii_strcasecmp(key, "Installed Size") == 0) {
            g_strlcpy(entry->size, val, sizeof(entry->size));
        }
    }
    g_strfreev(lines);
}

static GArray *scan_installed_kernels(void) {
    GArray *arr = g_array_new(FALSE, TRUE, sizeof(KernelEntry));
    FILE *fp = popen("pacman -Qq 2>/dev/null", "r");
    if (!fp) return arr;

    char pkg[256];
    while (fgets(pkg, sizeof(pkg), fp)) {
        g_strstrip(pkg);
        if (!is_kernel_name(pkg)) continue;

        gchar cmd[512];
        g_snprintf(cmd, sizeof(cmd), "pacman -Qi %s 2>/dev/null", pkg);
        gchar *info = capture_command(cmd);

        KernelEntry e;
        entry_clear(&e);
        g_strlcpy(e.source, "installed", sizeof(e.source));
        g_strlcpy(e.name, pkg, sizeof(e.name));
        g_strlcpy(e.remove_cmd, "sudo pacman -Rns ", sizeof(e.remove_cmd));
        g_strlcat(e.remove_cmd, pkg, sizeof(e.remove_cmd));
        g_strlcpy(e.install_cmd, "sudo pacman -S --needed ", sizeof(e.install_cmd));
        g_strlcat(e.install_cmd, pkg, sizeof(e.install_cmd));
        e.installed = TRUE;

        parse_pkg_info_block(info, &e);
        g_free(info);
        append_entry(arr, &e);
    }

    pclose(fp);
    return arr;
}

static void clear_store(GtkListStore *store) {
    if (!store) return;
    gtk_list_store_clear(store);
}

static void free_array(GArray **arr) {
    if (arr && *arr) {
        g_array_free(*arr, TRUE);
        *arr = NULL;
    }
}

static void append_row(GtkListStore *store, const KernelEntry *e, gboolean with_check) {
    GtkTreeIter it;
    gtk_list_store_insert_with_values(
        store, &it, -1,
        COL_CHECK,      with_check ? FALSE : FALSE,
        COL_STATUS,     e->installed ? "Instalado" : "Disponible",
        COL_SOURCE,     e->source,
        COL_NAME,       e->name,
        COL_VERSION,    e->version,
        COL_DESC,       e->desc,
        COL_SIZE,       e->size,
        COL_INSTALL_CMD,e->install_cmd,
        COL_REMOVE_CMD, e->remove_cmd,
        COL_INSTALLED,  e->installed,
        -1
    );
}

static gboolean entry_matches(const KernelEntry *e, const gchar *filter) {
    if (!filter || !*filter) return TRUE;
    gchar *needle = g_ascii_strdown(filter, -1);
    gchar *name   = g_ascii_strdown(e->name, -1);
    gchar *src    = g_ascii_strdown(e->source, -1);
    gchar *ver    = g_ascii_strdown(e->version, -1);
    gchar *desc   = g_ascii_strdown(e->desc, -1);
    gboolean ok = (strstr(name, needle) || strstr(src, needle) ||
                   strstr(ver, needle) || strstr(desc, needle));
    g_free(needle); g_free(name); g_free(src); g_free(ver); g_free(desc);
    return ok;
}

static void render_available(AppState *st) {
    clear_store(st->avail_store);
    if (!st->avail_all) return;
    gint count = 0;
    for (guint i = 0; i < st->avail_all->len; i++) {
        KernelEntry *e = &g_array_index(st->avail_all, KernelEntry, i);
        if (entry_matches(e, st->avail_filter)) {
            append_row(st->avail_store, e, TRUE);
            count++;
        }
    }
    gchar *msg = g_strdup_printf("%d kernels disponibles", count);
    set_status(st, msg);
    g_free(msg);
}

static void render_installed(AppState *st) {
    clear_store(st->inst_store);
    if (!st->inst_all) return;
    gint count = 0;
    for (guint i = 0; i < st->inst_all->len; i++) {
        KernelEntry *e = &g_array_index(st->inst_all, KernelEntry, i);
        if (entry_matches(e, st->inst_filter)) {
            append_row(st->inst_store, e, TRUE);
            count++;
        }
    }
    gchar *msg = g_strdup_printf("%d kernels instalados", count);
    set_status(st, msg);
    g_free(msg);
}

static void refresh_lists(AppState *st) {
    free_array(&st->avail_all);
    free_array(&st->inst_all);

    st->avail_all = scan_available_kernels();
    st->inst_all  = scan_installed_kernels();

    render_available(st);
    render_installed(st);
}

static void set_all_checks(GtkListStore *store, gboolean state) {
    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &it);
    while (valid) {
        gtk_list_store_set(store, &it, COL_CHECK, state, -1);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &it);
    }
}

static void on_toggle_cell(GtkCellRendererToggle *cell, gchar *path_str, gpointer user_data) {
    GtkListStore *store = GTK_LIST_STORE(user_data);
    GtkTreePath *path = gtk_tree_path_new_from_string(path_str);
    GtkTreeIter it;
    gboolean checked = FALSE;
    if (gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &it, path)) {
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_CHECK, &checked, -1);
        gtk_list_store_set(store, &it, COL_CHECK, !checked, -1);
    }
    gtk_tree_path_free(path);
}

static void toggle_first_row_state(GtkListStore *store) {
    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &it);
    gboolean first = FALSE;
    if (valid)
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it, COL_CHECK, &first, -1);
    set_all_checks(store, !first);
}

static gchar *collect_selected_cmds(GtkListStore *store,
                                    gboolean want_installed,
                                    gboolean remove_mode,
                                    gchar   **group_source,
                                    gint     *count_out)
{
    GString *repo = g_string_new(NULL);
    GString *aur  = g_string_new(NULL);
    gint count = 0;

    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &it);
    while (valid) {
        gboolean checked = FALSE;
        gboolean installed = FALSE;
        gchar *cmd = NULL;
        gchar *src = NULL;
        gchar *name = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it,
                           COL_CHECK, &checked,
                           COL_INSTALLED, &installed,
                           COL_INSTALL_CMD, &cmd,
                           COL_SOURCE, &src,
                           COL_NAME, &name,
                           -1);

        gboolean ok = remove_mode ? (checked && installed) : (checked && !installed);
        if (ok && cmd && *cmd) {
            count++;
            if (remove_mode) {
                if (repo->len) g_string_append_c(repo, ' ');
                g_string_append(repo, name ? name : cmd);
            } else {
                if (g_strcmp0(src, "aur") == 0) {
                    if (aur->len) g_string_append_c(aur, ' ');
                    g_string_append(aur, name ? name : cmd);
                } else {
                    if (repo->len) g_string_append_c(repo, ' ');
                    g_string_append(repo, name ? name : cmd);
                }
            }
        }
        g_free(cmd);
        g_free(src);
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &it);
    }

    *count_out = count;

    gchar *result = NULL;
    if (remove_mode) {
        result = g_strdup(repo->str);
    } else if (group_source) {
        group_source[0] = g_strdup(repo->str);
        group_source[1] = g_strdup(aur->str);
        result = g_strdup("");
    } else {
        result = g_strdup(repo->str);
    }

    g_string_free(repo, TRUE);
    g_string_free(aur, TRUE);
    return result;
}

static gchar *make_bootloader_refresh_script(void) {
    GString *s = g_string_new(NULL);

    g_string_append(s, "\n");
    g_string_append(s, "echo ''\n");
    g_string_append(s, "echo '======= Actualización del bootloader ======='\n");

    g_string_append(s, "PULSE_BL=''\n");

    g_string_append(s, "if command -v bootctl >/dev/null 2>&1 && bootctl is-installed 2>/dev/null; then\n");
    g_string_append(s, "  PULSE_BL='systemd-boot'\n");
    g_string_append(s, "elif [ -f /boot/loader/loader.conf ] || [ -d /boot/loader/entries ]; then\n");
    g_string_append(s, "  PULSE_BL='systemd-boot'\n");
    g_string_append(s, "fi\n");

    g_string_append(s, "if [ -z \"$PULSE_BL\" ]; then\n");
    g_string_append(s, "  if [ -f /boot/grub/grub.cfg ]; then\n");
    g_string_append(s, "    PULSE_BL='grub'\n");
    g_string_append(s, "  elif [ -f /boot/grub2/grub.cfg ]; then\n");
    g_string_append(s, "    PULSE_BL='grub2'\n");
    g_string_append(s, "  fi\n");
    g_string_append(s, "fi\n");

    g_string_append(s, "if [ -z \"$PULSE_BL\" ]; then\n");
    g_string_append(s, "  for REFIND_CONF in /boot/EFI/refind/refind.conf \\\n");
    g_string_append(s, "                     /boot/efi/EFI/refind/refind.conf \\\n");
    g_string_append(s, "                     /efi/EFI/refind/refind.conf; do\n");
    g_string_append(s, "    if [ -f \"$REFIND_CONF\" ]; then PULSE_BL='refind'; break; fi\n");
    g_string_append(s, "  done\n");
    g_string_append(s, "fi\n");

    g_string_append(s, "if [ -z \"$PULSE_BL\" ]; then\n");
    g_string_append(s, "  for LIMINE_CONF in /boot/limine.conf \\\n");
    g_string_append(s, "                     /boot/EFI/LIMINE/limine.conf \\\n");
    g_string_append(s, "                     /boot/EFI/limine/limine.conf; do\n");
    g_string_append(s, "    if [ -f \"$LIMINE_CONF\" ]; then PULSE_BL='limine'; break; fi\n");
    g_string_append(s, "  done\n");
    g_string_append(s, "fi\n");

    g_string_append(s, "\n");
    g_string_append(s, "echo \"Bootloader detectado: ${PULSE_BL:-desconocido}\"\n");
    g_string_append(s, "echo ''\n");

    g_string_append(s, "case \"$PULSE_BL\" in\n");

    g_string_append(s, "  systemd-boot)\n");
    g_string_append(s, "    echo '[systemd-boot] Actualizando binario EFI...'\n");
    g_string_append(s, "    sudo bootctl update 2>/dev/null || true\n");
    g_string_append(s, "    if command -v mkinitcpio >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[systemd-boot] Regenerando initramfs (mkinitcpio -P)...'\n");
    g_string_append(s, "      sudo mkinitcpio -P || true\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    if command -v kernel-install >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[systemd-boot] Sincronizando entradas de arranque...'\n");
    g_string_append(s, "      for KVER in $(ls /usr/lib/modules/ 2>/dev/null); do\n");
    g_string_append(s, "        VMLINUZ=''\n");
    g_string_append(s, "        for CANDIDATE in \\\n");
    g_string_append(s, "              \"/boot/vmlinuz-${KVER}\" \\\n");
    g_string_append(s, "              \"/boot/vmlinuz-$(echo $KVER | sed 's/-[0-9]*$//')\"; do\n");
    g_string_append(s, "          if [ -f \"$CANDIDATE\" ]; then VMLINUZ=\"$CANDIDATE\"; break; fi\n");
    g_string_append(s, "        done\n");
    g_string_append(s, "        if [ -n \"$VMLINUZ\" ]; then\n");
    g_string_append(s, "          kernel-install add \"$KVER\" \"$VMLINUZ\" 2>/dev/null || true\n");
    g_string_append(s, "        fi\n");
    g_string_append(s, "      done\n");
    g_string_append(s, "    else\n");
    g_string_append(s, "      if [ -d /boot/loader/entries ]; then\n");
    g_string_append(s, "        echo '[systemd-boot] Entradas existentes en /boot/loader/entries/:'\n");
    g_string_append(s, "        ls /boot/loader/entries/ 2>/dev/null || true\n");
    g_string_append(s, "      else\n");
    g_string_append(s, "        echo 'AVISO: /boot/loader/entries/ no existe.'\n");
    g_string_append(s, "        echo '       Crea manualmente las entradas o instala kernel-install.'\n");
    g_string_append(s, "      fi\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "  grub)\n");
    g_string_append(s, "    if command -v grub-mkconfig >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[GRUB] Regenerando /boot/grub/grub.cfg...'\n");
    g_string_append(s, "      sudo grub-mkconfig -o /boot/grub/grub.cfg || true\n");
    g_string_append(s, "    else\n");
    g_string_append(s, "      echo 'AVISO: grub-mkconfig no encontrado.'\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "  grub2)\n");
    g_string_append(s, "    if command -v grub2-mkconfig >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[GRUB2] Regenerando /boot/grub2/grub.cfg...'\n");
    g_string_append(s, "      sudo grub2-mkconfig -o /boot/grub2/grub.cfg || true\n");
    g_string_append(s, "    else\n");
    g_string_append(s, "      echo 'AVISO: grub2-mkconfig no encontrado.'\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "  refind)\n");
    g_string_append(s, "    echo '[rEFInd] rEFInd detecta kernels automáticamente al arrancar.'\n");
    g_string_append(s, "    if command -v mkinitcpio >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[rEFInd] Regenerando initramfs (mkinitcpio -P)...'\n");
    g_string_append(s, "      sudo mkinitcpio -P || true\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "  limine)\n");
    g_string_append(s, "    if command -v limine-mkconfig >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[Limine] Ejecutando limine-mkconfig...'\n");
    g_string_append(s, "      sudo limine-mkconfig || true\n");
    g_string_append(s, "    elif command -v limine-update >/dev/null 2>&1; then\n");
    g_string_append(s, "      echo '[Limine] Ejecutando limine-update...'\n");
    g_string_append(s, "      sudo limine-update || true\n");
    g_string_append(s, "    else\n");
    g_string_append(s, "      echo 'AVISO: limine-mkconfig y limine-update no encontrados.'\n");
    g_string_append(s, "      echo '       Actualiza limine manualmente.'\n");
    g_string_append(s, "    fi\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "  *)\n");
    g_string_append(s, "    echo 'AVISO: No se detectó un bootloader compatible.'\n");
    g_string_append(s, "    echo '       Actualiza el bootloader manualmente.'\n");
    g_string_append(s, "    ;;\n");

    g_string_append(s, "esac\n");
    g_string_append(s, "echo '============================================'\n");

    return g_string_free(s, FALSE);
}

static gchar *write_temp_script(const gchar *body) {
    gchar *tmpl = g_strdup("/tmp/pulseos-kernel-manager-XXXXXX.sh");
    int fd = g_mkstemp(tmpl);
    if (fd < 0) {
        g_free(tmpl);
        return NULL;
    }
    FILE *fp = fdopen(fd, "w");
    if (!fp) {
        close(fd);
        unlink(tmpl);
        g_free(tmpl);
        return NULL;
    }
    fputs("#!/bin/sh\nset -eu\n", fp);
    if (body) fputs(body, fp);
    fputs("\necho\n", fp);
    fputs("echo 'PulseOS kernel manager: operación terminada.'\n", fp);
    fputs("echo 'Pulsa Enter para cerrar.'\n", fp);
    fputs("read -r _\n", fp);
    fclose(fp);
    chmod(tmpl, 0755);
    return tmpl;
}

typedef struct {
    AppState *st;
    gchar    *script_path;
} SpawnCtx;

static void on_terminal_finished(GPid pid, gint status, gpointer user_data) {
    SpawnCtx *ctx = user_data;
    g_spawn_close_pid(pid);
    if (ctx->script_path) {
        unlink(ctx->script_path);
        g_free(ctx->script_path);
    }

    ctx->st->busy = FALSE;
    gtk_widget_set_sensitive(ctx->st->avail_install_btn, TRUE);
    gtk_widget_set_sensitive(ctx->st->inst_remove_btn, TRUE);
    gtk_widget_set_sensitive(ctx->st->avail_refresh_btn, TRUE);
    gtk_widget_set_sensitive(ctx->st->inst_refresh_btn, TRUE);
    gtk_widget_set_sensitive(ctx->st->avail_select_btn, TRUE);
    gtk_widget_set_sensitive(ctx->st->inst_select_btn, TRUE);
    gtk_spinner_stop(GTK_SPINNER(ctx->st->spinner));

    refresh_lists(ctx->st);
    set_status(ctx->st, "Listo.");
    g_free(ctx);
}

static void spawn_in_alacritty(AppState *st, const gchar *script_path) {
    gchar *alacritty = g_find_program_in_path("alacritty");
    if (!alacritty) {
        set_status(st, "Alacritty no está instalado.");
        st->busy = FALSE;
        gtk_widget_set_sensitive(st->avail_install_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_remove_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_select_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_select_btn, TRUE);
        gtk_spinner_stop(GTK_SPINNER(st->spinner));
        return;
    }

    gchar *argv[] = {
        alacritty,
        (gchar *)"--hold",
        (gchar *)"-e",
        (gchar *)"sh",
        (gchar *)script_path,
        NULL
    };

    GError *err = NULL;
    GPid pid = 0;
    if (!g_spawn_async(NULL, argv, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err)) {
        gchar *msg = g_strdup_printf("No se pudo abrir Alacritty: %s", err ? err->message : "error");
        set_status(st, msg);
        g_free(msg);
        if (err) g_error_free(err);
        st->busy = FALSE;
        gtk_widget_set_sensitive(st->avail_install_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_remove_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_select_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_select_btn, TRUE);
        gtk_spinner_stop(GTK_SPINNER(st->spinner));
        return;
    }

    g_free(alacritty);

    SpawnCtx *ctx = g_new0(SpawnCtx, 1);
    ctx->st = st;
    ctx->script_path = g_strdup(script_path);
    g_child_watch_add(pid, on_terminal_finished, ctx);
}


static void on_install_clicked(GtkButton *btn, gpointer user_data) {
    AppState *st = user_data;
    if (st->busy) return;

    GString *repo = g_string_new(NULL);
    GString *aur  = g_string_new(NULL);
    gint count = 0;

    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(st->avail_store), &it);
    while (valid) {
        gboolean checked = FALSE;
        gboolean installed = FALSE;
        gchar *src = NULL;
        gchar *name = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(st->avail_store), &it,
                           COL_CHECK, &checked,
                           COL_INSTALLED, &installed,
                           COL_SOURCE, &src,
                           COL_NAME, &name,
                           -1);
        if (checked && !installed && name && *name) {
            count++;
            if (g_strcmp0(src, "aur") == 0) {
                if (aur->len) g_string_append_c(aur, ' ');
                g_string_append(aur, name);
            } else {
                if (repo->len) g_string_append_c(repo, ' ');
                g_string_append(repo, name);
            }
        }
        g_free(src);
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(st->avail_store), &it);
    }

    if (count == 0) {
        set_status(st, "Marca uno o más kernels para instalar.");
        g_string_free(repo, TRUE);
        g_string_free(aur, TRUE);
        return;
    }

    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(st->window),
                                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_QUESTION,
                                            GTK_BUTTONS_OK_CANCEL,
                                            "¿Instalar %d kernel(es)?", count);
    gtk_window_set_title(GTK_WINDOW(dlg), APP_TITLE);
    gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (resp != GTK_RESPONSE_OK) {
        g_string_free(repo, TRUE);
        g_string_free(aur, TRUE);
        return;
    }

    GString *body = g_string_new(NULL);
    g_string_append(body, "echo 'Descargando e instalando kernels...'\n");

    if (repo->len) {
        g_string_append(body, "echo '[pacman]'\n");
        g_string_append(body, "sudo pacman -S --needed ");
        gchar **names = g_strsplit(repo->str, " ", -1);
        for (gint i = 0; names[i]; i++) {
            if (!*names[i]) continue;
            gchar *q = g_shell_quote(names[i]);
            g_string_append_printf(body, "%s ", q);
            g_free(q);
        }
        g_strfreev(names);
        g_string_append(body, "\n");
    }

    if (aur->len) {
        g_string_append(body, "echo '[yay]'\n");
        g_string_append(body, "yay -S --needed ");
        gchar **names = g_strsplit(aur->str, " ", -1);
        for (gint i = 0; names[i]; i++) {
            if (!*names[i]) continue;
            gchar *q = g_shell_quote(names[i]);
            g_string_append_printf(body, "%s ", q);
            g_free(q);
        }
        g_strfreev(names);
        g_string_append(body, "\n");
    }

    gchar *boot = make_bootloader_refresh_script();
    g_string_append(body, boot);
    g_free(boot);

    gchar *path = write_temp_script(body->str);
    g_string_free(body, TRUE);
    g_string_free(repo, TRUE);
    g_string_free(aur, TRUE);

    if (!path) {
        set_status(st, "No se pudo crear el script temporal.");
        st->busy = FALSE;
        gtk_widget_set_sensitive(st->avail_install_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_remove_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_select_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_select_btn, TRUE);
        gtk_spinner_stop(GTK_SPINNER(st->spinner));
        return;
    }

    st->busy = TRUE;
    gtk_widget_set_sensitive(st->avail_install_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_remove_btn, FALSE);
    gtk_widget_set_sensitive(st->avail_refresh_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_refresh_btn, FALSE);
    gtk_widget_set_sensitive(st->avail_select_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_select_btn, FALSE);
    gtk_spinner_start(GTK_SPINNER(st->spinner));
    set_status(st, "Abriendo Alacritty...");

    spawn_in_alacritty(st, path);
}

static gint count_checked(GtkListStore *store, gboolean want_installed) {
    gint count = 0;
    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(store), &it);
    while (valid) {
        gboolean checked = FALSE;
        gboolean installed = FALSE;
        gtk_tree_model_get(GTK_TREE_MODEL(store), &it,
                           COL_CHECK, &checked,
                           COL_INSTALLED, &installed,
                           -1);
        if (checked && installed == want_installed) count++;
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(store), &it);
    }
    return count;
}

static gint count_total_installed(GtkListStore *store) {
    return gtk_tree_model_iter_n_children(GTK_TREE_MODEL(store), NULL);
}

static void on_remove_clicked(GtkButton *btn, gpointer user_data) {
    AppState *st = user_data;
    if (st->busy) return;

    gint selected = count_checked(st->inst_store, TRUE);
    gint total = st->inst_all ? (gint)st->inst_all->len : count_total_installed(st->inst_store);
    if (selected == 0) {
        set_status(st, "Marca uno o más kernels para borrar.");
        return;
    }
    if (total <= 1 || selected >= total) {
        set_status(st, "Por seguridad, debe quedar al menos 1 kernel instalado.");
        return;
    }

    GtkWidget *dlg = gtk_message_dialog_new(GTK_WINDOW(st->window),
                                            GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                                            GTK_MESSAGE_WARNING,
                                            GTK_BUTTONS_OK_CANCEL,
                                            "Vas a borrar %d kernel(es) y dejar %d instalado(s). ¿Continuar?",
                                            selected, total - selected);
    gtk_window_set_title(GTK_WINDOW(dlg), APP_TITLE);
    gint resp = gtk_dialog_run(GTK_DIALOG(dlg));
    gtk_widget_destroy(dlg);
    if (resp != GTK_RESPONSE_OK) return;

    GString *pkgs = g_string_new(NULL);
    GtkTreeIter it;
    gboolean valid = gtk_tree_model_get_iter_first(GTK_TREE_MODEL(st->inst_store), &it);
    while (valid) {
        gboolean checked = FALSE;
        gboolean installed = FALSE;
        gchar *name = NULL;
        gtk_tree_model_get(GTK_TREE_MODEL(st->inst_store), &it,
                           COL_CHECK, &checked,
                           COL_INSTALLED, &installed,
                           COL_NAME, &name,
                           -1);
        if (checked && installed && name && *name) {
            if (pkgs->len) g_string_append_c(pkgs, ' ');
            gchar *q = g_shell_quote(name);
            g_string_append(pkgs, q);
            g_free(q);
        }
        g_free(name);
        valid = gtk_tree_model_iter_next(GTK_TREE_MODEL(st->inst_store), &it);
    }

    if (!pkgs->len) {
        g_string_free(pkgs, TRUE);
        set_status(st, "No hay kernels válidos seleccionados.");
        return;
    }

    GString *body = g_string_new(NULL);
    g_string_append(body, "echo 'Eliminando kernels...'\n");
    g_string_append(body, "sudo pacman -Rns --noconfirm ");
    g_string_append(body, pkgs->str);
    g_string_append(body, "\n");
    gchar *boot = make_bootloader_refresh_script();
    g_string_append(body, boot);
    g_free(boot);

    gchar *path = write_temp_script(body->str);
    g_string_free(body, TRUE);
    g_string_free(pkgs, TRUE);

    if (!path) {
        set_status(st, "No se pudo crear el script temporal.");
        st->busy = FALSE;
        gtk_widget_set_sensitive(st->avail_install_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_remove_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_refresh_btn, TRUE);
        gtk_widget_set_sensitive(st->avail_select_btn, TRUE);
        gtk_widget_set_sensitive(st->inst_select_btn, TRUE);
        gtk_spinner_stop(GTK_SPINNER(st->spinner));
        return;
    }

    st->busy = TRUE;
    gtk_widget_set_sensitive(st->avail_install_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_remove_btn, FALSE);
    gtk_widget_set_sensitive(st->avail_refresh_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_refresh_btn, FALSE);
    gtk_widget_set_sensitive(st->avail_select_btn, FALSE);
    gtk_widget_set_sensitive(st->inst_select_btn, FALSE);
    gtk_spinner_start(GTK_SPINNER(st->spinner));
    set_status(st, "Abriendo Alacritty...");

    spawn_in_alacritty(st, path);
}

static void on_avail_refresh(GtkButton *btn, gpointer user_data) {
    refresh_lists(user_data);
}

static void on_inst_refresh(GtkButton *btn, gpointer user_data) {
    refresh_lists(user_data);
}

static void on_avail_select(GtkButton *btn, gpointer user_data) {
    AppState *st = user_data;
    if (st->avail_store) toggle_first_row_state(st->avail_store);
}

static void on_inst_select(GtkButton *btn, gpointer user_data) {
    AppState *st = user_data;
    if (st->inst_store) toggle_first_row_state(st->inst_store);
}

static void on_avail_filter_changed(GtkEditable *ed, gpointer user_data) {
    AppState *st = user_data;
    g_strlcpy(st->avail_filter, gtk_entry_get_text(GTK_ENTRY(ed)), sizeof(st->avail_filter));
    render_available(st);
}

static void on_inst_filter_changed(GtkEditable *ed, gpointer user_data) {
    AppState *st = user_data;
    g_strlcpy(st->inst_filter, gtk_entry_get_text(GTK_ENTRY(ed)), sizeof(st->inst_filter));
    render_installed(st);
}

static GtkTreeView *make_tree(GtkListStore **store_out, gboolean installed_tab) {
    GtkListStore *store = gtk_list_store_new(N_COLS,
                                             G_TYPE_BOOLEAN,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_STRING,
                                             G_TYPE_BOOLEAN);
    *store_out = store;

    GtkWidget *tree = gtk_tree_view_new_with_model(GTK_TREE_MODEL(store));
    gtk_tree_view_set_headers_clickable(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_search_column(GTK_TREE_VIEW(tree), COL_NAME);

    GtkCellRenderer *renderer_toggle = gtk_cell_renderer_toggle_new();
    g_signal_connect(renderer_toggle, "toggled", G_CALLBACK(on_toggle_cell), store);
    GtkTreeViewColumn *col_toggle = gtk_tree_view_column_new_with_attributes("✓", renderer_toggle, "active", COL_CHECK, NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree), col_toggle);

    GtkCellRenderer *r_status = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Estado", r_status, "text", COL_STATUS, NULL));

    GtkCellRenderer *r_source = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Fuente", r_source, "text", COL_SOURCE, NULL));

    GtkCellRenderer *r_name = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Kernel", r_name, "text", COL_NAME, NULL));

    GtkCellRenderer *r_ver = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Versión", r_ver, "text", COL_VERSION, NULL));

    GtkCellRenderer *r_desc = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Descripción", r_desc, "text", COL_DESC, NULL));

    GtkCellRenderer *r_size = gtk_cell_renderer_text_new();
    gtk_tree_view_append_column(GTK_TREE_VIEW(tree),
        gtk_tree_view_column_new_with_attributes("Tamaño", r_size, "text", COL_SIZE, NULL));

    gtk_tree_view_set_rules_hint(GTK_TREE_VIEW(tree), TRUE);
    gtk_tree_view_set_grid_lines(GTK_TREE_VIEW(tree), GTK_TREE_VIEW_GRID_LINES_BOTH);
    gtk_widget_set_hexpand(tree, TRUE);
    gtk_widget_set_vexpand(tree, TRUE);

    return GTK_TREE_VIEW(tree);
}

static GtkWidget *make_toolbar(GtkWidget *filter_entry,
                               GtkWidget *refresh_btn,
                               GtkWidget *select_btn,
                               GtkWidget *action_btn)
{
    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    gtk_box_pack_start(GTK_BOX(box), filter_entry, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(box), select_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), refresh_btn, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(box), action_btn, FALSE, FALSE, 0);
    return box;
}

static GtkWidget *make_page(GtkTreeView **tree_out,
                            GtkListStore **store_out,
                            GtkWidget **filter_entry_out,
                            GtkWidget **refresh_btn_out,
                            GtkWidget **select_btn_out,
                            GtkWidget **action_btn_out,
                            const gchar *action_label,
                            gboolean is_installed_page,
                            AppState *st)
{
    GtkWidget *page = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(page), 10);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(entry), "Filtrar kernels...");
    g_signal_connect(entry, "changed", G_CALLBACK(is_installed_page ? on_inst_filter_changed : on_avail_filter_changed), st);

    GtkWidget *refresh_btn = gtk_button_new_with_label("Reescanear");
    GtkWidget *select_btn  = gtk_button_new_with_label("Seleccionar todo");
    GtkWidget *action_btn  = gtk_button_new_with_label(action_label);

    g_signal_connect(refresh_btn, "clicked", G_CALLBACK(is_installed_page ? on_inst_refresh : on_avail_refresh), st);
    g_signal_connect(select_btn, "clicked", G_CALLBACK(is_installed_page ? on_inst_select : on_avail_select), st);
    g_signal_connect(action_btn, "clicked", G_CALLBACK(is_installed_page ? on_remove_clicked : on_install_clicked), st);

    GtkWidget *bar = make_toolbar(entry, refresh_btn, select_btn, action_btn);
    gtk_box_pack_start(GTK_BOX(page), bar, FALSE, FALSE, 0);

    GtkTreeView *tree = make_tree(store_out, is_installed_page);
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), GTK_WIDGET(tree));
    gtk_box_pack_start(GTK_BOX(page), scroll, TRUE, TRUE, 0);

    *tree_out = tree;
    *filter_entry_out = entry;
    *refresh_btn_out = refresh_btn;
    *select_btn_out = select_btn;
    *action_btn_out = action_btn;

    return page;
}

static void destroy_state(gpointer data) {
    AppState *st = data;
    if (!st) return;
    free_array(&st->avail_all);
    free_array(&st->inst_all);
    g_free(st);
}

typedef struct {
    gboolean  bin_updated;   
    gboolean  any_error;     
    gchar     self_path[512];
    gchar     res_file[256]; 
} KmUpdResult;

typedef struct {
    AppState    *st;
    gchar       *script_path;
    KmUpdResult *upd;
} SpawnUpdCtx;


static gchar *km_sha256_of(const gchar *path) {
    gchar cmd[600];
    g_snprintf(cmd, sizeof(cmd), "sha256sum '%s' 2>/dev/null", path);
    FILE *fp = popen(cmd, "r");
    if (!fp) return NULL;
    gchar line[128] = {0};
    fgets(line, sizeof(line), fp);
    pclose(fp);
    gchar *sp = strchr(line, ' ');  if (sp) *sp = '\0';
    gchar *nl = strchr(line, '\n'); if (nl) *nl = '\0';
    return (strlen(line) == 64) ? g_strdup(line) : NULL;
}


static gboolean km_restart_cb(gpointer data) {
    gchar *self_path = data;
    gchar *argv_r[]  = {
        (self_path && self_path[0]) ? self_path : (gchar *)UPD_BIN_DST,
        NULL
    };
    GError *err = NULL;
    g_spawn_async(NULL, argv_r, NULL, G_SPAWN_DEFAULT, NULL, NULL, NULL, &err);
    if (err) g_error_free(err);
    g_free(self_path);
    if (g_app_state)
        g_application_quit(G_APPLICATION(g_app_state->app));
    return G_SOURCE_REMOVE;
}


static void km_on_update_finish(GPid pid, gint status, gpointer data) {
    SpawnUpdCtx *ctx = data;
    g_spawn_close_pid(pid);

    if (ctx->script_path) {
        unlink(ctx->script_path);
        g_free(ctx->script_path);
    }

    gboolean ok = FALSE;
    {
        FILE *rf = fopen(ctx->upd->res_file, "r");
        if (rf) {
            gchar buf[8] = {0};
            fgets(buf, sizeof(buf), rf);
            fclose(rf);
            unlink(ctx->upd->res_file);
            ok = (strncmp(buf, "ok", 2) == 0);
        }
    }
    unlink(UPD_TMP_BIN);

    if (ok && ctx->upd->bin_updated) {
        set_status(ctx->st, "Actualización aplicada. Reiniciando...");
        g_timeout_add(2000, km_restart_cb, g_strdup(ctx->upd->self_path));
    } else if (ok) {
        set_status(ctx->st, "Actualización aplicada correctamente.");
        if (ctx->st->update_btn)
            gtk_widget_set_sensitive(ctx->st->update_btn, TRUE);
    } else {
        set_status(ctx->st, "Error al aplicar la actualización.");
        if (ctx->st->update_btn)
            gtk_widget_set_sensitive(ctx->st->update_btn, TRUE);
    }

    g_free(ctx->upd);
    g_free(ctx);
}


static void km_spawn_update(AppState *st, KmUpdResult *r) {
    gchar *alacritty = g_find_program_in_path("alacritty");
    if (!alacritty) {
        set_status(st, "Alacritty no encontrado. Actualiza manualmente.");
        if (st->update_btn) gtk_widget_set_sensitive(st->update_btn, TRUE);
        g_free(r);
        return;
    }


    GString *body = g_string_new(NULL);
    g_string_append(body, "echo 'Aplicando actualización de PulseOS Kernel Manager...'\n");
    g_string_append_printf(body,
        "sudo rm -f '%s' && "
        "sudo cp '%s' '%s' && "
        "sudo chmod 755 '%s' && "
        "echo ok > '%s' || "
        "echo fail > '%s'\n",
        r->self_path,
        UPD_TMP_BIN, r->self_path,
        r->self_path,
        r->res_file, r->res_file);
    g_string_append_printf(body, "chmod 666 '%s' 2>/dev/null\n", r->res_file);

    gchar *script_path = write_temp_script(body->str);
    g_string_free(body, TRUE);

    if (!script_path) {
        set_status(st, "Error al crear el script de actualización.");
        if (st->update_btn) gtk_widget_set_sensitive(st->update_btn, TRUE);
        g_free(alacritty);
        g_free(r);
        return;
    }

    gchar *argv_u[] = {
        alacritty,
        (gchar *)"--hold",
        (gchar *)"-e",
        (gchar *)"sh",
        script_path,
        NULL
    };

    GError *err = NULL;
    GPid    pid  = 0;

    set_status(st, "Aplicando actualización...");

    if (!g_spawn_async(NULL, argv_u, NULL,
                       G_SPAWN_SEARCH_PATH | G_SPAWN_DO_NOT_REAP_CHILD,
                       NULL, NULL, &pid, &err)) {
        gchar *msg = g_strdup_printf("No se pudo abrir Alacritty: %s",
                                     err ? err->message : "error");
        set_status(st, msg);
        g_free(msg);
        if (err) g_error_free(err);
        unlink(script_path);
        g_free(script_path);
        if (st->update_btn) gtk_widget_set_sensitive(st->update_btn, TRUE);
        g_free(alacritty);
        g_free(r);
        return;
    }
    g_free(alacritty);

    SpawnUpdCtx *ctx  = g_new0(SpawnUpdCtx, 1);
    ctx->st           = st;
    ctx->script_path  = script_path;
    ctx->upd          = r;
    g_child_watch_add(pid, km_on_update_finish, ctx);
}


static gboolean km_upd_notify_idle(gpointer data) {
    KmUpdResult *r  = data;
    AppState    *st = g_app_state;

    if (!st) { g_free(r); return G_SOURCE_REMOVE; }

    if (!r->bin_updated) {
        set_status(st, r->any_error
            ? "No se pudo comprobar las actualizaciones."
            : "La aplicación está actualizada.");
        if (st->update_btn)
            gtk_widget_set_sensitive(st->update_btn, TRUE);
        g_free(r);
        return G_SOURCE_REMOVE;
    }

    km_spawn_update(st, r);   
    return G_SOURCE_REMOVE;
}

static gboolean km_upd_checking_idle(gpointer data) {
    (void)data;
    if (g_app_state) {
        set_status(g_app_state, "Comprobando actualizaciones...");
        if (g_app_state->update_btn)
            gtk_widget_set_sensitive(g_app_state->update_btn, FALSE);
    }
    return G_SOURCE_REMOVE;
}


static gpointer km_update_check_thread(gpointer data) {
    (void)data;
    KmUpdResult *r = g_new0(KmUpdResult, 1);
    g_snprintf(r->res_file, sizeof(r->res_file),
               "/tmp/.pulseos-km-res-%d", getpid());

    g_idle_add(km_upd_checking_idle, NULL);

    gchar cmd[700];
    g_snprintf(cmd, sizeof(cmd),
               "curl -fsSL --max-time 30 -o '%s' '%s' 2>/dev/null",
               UPD_TMP_BIN, UPD_URL_BIN);

    if (system(cmd) == 0) {
        g_strlcpy(r->self_path, UPD_BIN_DST, sizeof(r->self_path));
        {
            gchar tmp[512] = {0};
            ssize_t n = readlink("/proc/self/exe", tmp, sizeof(tmp) - 1);
            if (n > 0) {
                tmp[n] = '\0';
                g_strlcpy(r->self_path, tmp, sizeof(r->self_path));
            }
        }

        gchar *sha_local  = km_sha256_of(r->self_path);
        gchar *sha_remote = km_sha256_of(UPD_TMP_BIN);

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

    g_idle_add(km_upd_notify_idle, r);
    return NULL;
}


static void km_launch_update_check(AppState *st) {
    if (st->update_btn)
        gtk_widget_set_sensitive(st->update_btn, FALSE);
    GThread *t = g_thread_new("km-upd-check", km_update_check_thread, NULL);
    g_thread_unref(t);
}

static void on_update_check_clicked(GtkButton *btn, gpointer user_data) {
    km_launch_update_check((AppState *)user_data);
}


static GtkWidget *build_ui(AppState *st) {
    GtkWidget *root = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    gtk_container_set_border_width(GTK_CONTAINER(root), 12);

    GtkWidget *top = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *title = gtk_label_new(NULL);
    gtk_label_set_markup(GTK_LABEL(title), "<span size='x-large' weight='bold'>PulseOS kernel manager</span>");
    gtk_widget_set_halign(title, GTK_ALIGN_START);

    st->spinner = gtk_spinner_new();
    gtk_widget_set_halign(st->spinner, GTK_ALIGN_END);

    gtk_box_pack_start(GTK_BOX(top), title, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(top), st->spinner, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(root), top, FALSE, FALSE, 0);

    st->notebook = gtk_notebook_new();

    GtkWidget *avail_page = make_page(&st->avail_tree,
                                      &st->avail_store,
                                      &st->avail_filter_entry,
                                      &st->avail_refresh_btn,
                                      &st->avail_select_btn,
                                      &st->avail_install_btn,
                                      "Instalar seleccionado(s)",
                                      FALSE,
                                      st);
    GtkWidget *inst_page = make_page(&st->inst_tree,
                                     &st->inst_store,
                                     &st->inst_filter_entry,
                                     &st->inst_refresh_btn,
                                     &st->inst_select_btn,
                                     &st->inst_remove_btn,
                                     "Borrar seleccionado(s)",
                                     TRUE,
                                     st);

    gtk_notebook_append_page(GTK_NOTEBOOK(st->notebook), avail_page, gtk_label_new("Disponibles"));
    gtk_notebook_append_page(GTK_NOTEBOOK(st->notebook), inst_page, gtk_label_new("Instalados"));
    gtk_box_pack_start(GTK_BOX(root), st->notebook, TRUE, TRUE, 0);

    GtkWidget *status_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    st->status_label = gtk_label_new("Listo.");
    gtk_widget_set_halign(st->status_label, GTK_ALIGN_START);
    gtk_label_set_ellipsize(GTK_LABEL(st->status_label), PANGO_ELLIPSIZE_END);
    gtk_box_pack_start(GTK_BOX(status_box), st->status_label, TRUE, TRUE, 0);

    st->ver_label = gtk_label_new(KM_VERSION);
    gtk_widget_set_opacity(st->ver_label, 0.38);
    gtk_box_pack_start(GTK_BOX(status_box), st->ver_label, FALSE, FALSE, 4);

    st->update_btn = gtk_button_new_with_label("Buscar actualizaciones");
    gtk_widget_set_tooltip_text(st->update_btn,
        "Comprueba si hay una nueva versión de PulseOS Kernel Manager");
    g_signal_connect(st->update_btn, "clicked",
                     G_CALLBACK(on_update_check_clicked), st);
    gtk_box_pack_end(GTK_BOX(status_box), st->update_btn, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(root), status_box, FALSE, FALSE, 0);

    return root;
}

static void activate(GtkApplication *app, gpointer user_data) {
    AppState *st = g_new0(AppState, 1);
    st->app = app;

    g_app_state = st;

    st->window = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(st->window), APP_TITLE);
    gtk_window_set_default_size(GTK_WINDOW(st->window), 1100, 700);
    gtk_window_set_position(GTK_WINDOW(st->window), GTK_WIN_POS_CENTER);

    GtkWidget *ui = build_ui(st);
    gtk_container_add(GTK_CONTAINER(st->window), ui);

    refresh_lists(st);

    gtk_widget_show_all(st->window);
    set_status(st, "Listo.");
    g_object_set_data_full(G_OBJECT(st->window), "app-state", st, destroy_state);

    km_launch_update_check(st);
}

int main(int argc, char **argv) {
    GtkApplication *app = gtk_application_new(APP_ID, G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    return status;
}
