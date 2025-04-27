/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef __TH_FILE_H__
#define __TH_FILE_H__

#include <gtk/gtk.h>
#include "usermanager.h"
#include "fileinfo.h"
#include "enumtypes.h"

G_BEGIN_DECLS

typedef enum
{
    FILE_ICON_STATE_DEFAULT,
    FILE_ICON_STATE_DROP,
    FILE_ICON_STATE_OPEN,

} FileIconState;

typedef enum
{
    FILE_DATE_ACCESSED,
    FILE_DATE_CHANGED,
    FILE_DATE_MODIFIED,

} FileDateType;

// ThunarFile -----------------------------------------------------------------

typedef struct _ThunarFile      ThunarFile;
typedef struct _ThunarFileClass ThunarFileClass;

GType th_file_get_type() G_GNUC_CONST;

#define TYPE_THUNARFILE (th_file_get_type())
#define THUNARFILE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARFILE, ThunarFile))
#define THUNARFILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARFILE, ThunarFileClass))
#define IS_THUNARFILE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARFILE))
#define IS_THUNARFILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_THUNARFILE))
#define THUNARFILE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARFILE, ThunarFileClass))

// callback type for loading ThunarFile's asynchronously. If you want to keep
// the ThunarFile, you need to ref it, else it will be destroyed

typedef void (*ThunarFileGetFunc) (GFile *location, ThunarFile *file,
                                   GError *error, gpointer user_data);

// allocate
ThunarFile* th_file_get(GFile *file, GError **error);
ThunarFile* th_file_get_for_uri(const gchar *uri, GError **error);
ThunarFile* th_file_get_parent(const ThunarFile *file, GError **error);
ThunarFile* th_file_get_with_info(GFile *file, GFileInfo *info,
                                  gboolean not_mounted);
void th_file_get_async(GFile *location, GCancellable *cancellable,
                       ThunarFileGetFunc func, gpointer user_data);
// destroy
void th_file_destroy(ThunarFile *file);

// get
guint64 th_file_get_date(const ThunarFile *file,
                         FileDateType date_type) G_GNUC_PURE;
GAppInfo* th_file_get_default_handler(const ThunarFile *file);
GFile* th_file_get_file(const ThunarFile *file) G_GNUC_PURE;
GFileType th_file_get_filetype(const ThunarFile *file) G_GNUC_PURE;
ThunarGroup* th_file_get_group(const ThunarFile *file);
GFileInfo* th_file_get_info(const ThunarFile *file) G_GNUC_PURE;
ThunarFileMode th_file_get_mode(const ThunarFile *file);
guint64 th_file_get_size(const ThunarFile *file);
GFile* th_file_get_target_location(const ThunarFile *file);
guint32 th_file_get_trash_count(const ThunarFile *file);
ThunarUser* th_file_get_user(const ThunarFile *file);
GVolume* th_file_get_volume(const ThunarFile *file);

// get string
gchar* th_file_get_date_string(const ThunarFile *file,
                               FileDateType date_type,
                               ThunarDateStyle date_style,
                               const gchar *date_custom_style)
                               G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* th_file_get_deletion_date(const ThunarFile *file,
                                 ThunarDateStyle date_style,
                                 const gchar *date_custom_style)
                                 G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* th_file_get_mode_string(const ThunarFile *file)
                               G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* th_file_get_size_in_bytes_string(
        const ThunarFile *file) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* th_file_get_size_string_formatted(const ThunarFile *file,
                                         const gboolean file_size_binary);
gchar* th_file_get_size_string_long(const ThunarFile *file,
                                    const gboolean file_size_binary);
#define th_file_get_uri(file) (g_file_get_uri(th_file_get_file(file)))

// get const string
const gchar* th_file_get_basename(const ThunarFile *file) G_GNUC_CONST;
const gchar* th_file_get_content_type(ThunarFile *file);
const gchar* th_file_get_custom_icon(const ThunarFile *file);
const gchar* th_file_get_display_name(const ThunarFile *file) G_GNUC_CONST;
const gchar* th_file_get_icon_name(ThunarFile *file,
                                   FileIconState icon_state,
                                   GtkIconTheme *icon_theme);
const gchar* th_file_get_original_path(const ThunarFile *file);
const gchar* th_file_get_symlink_target(const ThunarFile *file);

// tests ----------------------------------------------------------------------

GdkDragAction th_file_accepts_drop(ThunarFile *file, GList *path_list,
                                   GdkDragContext *context,
                                   GdkDragAction *suggested_action_return);
gboolean th_file_can_be_trashed(const ThunarFile *file);
gboolean th_file_check_loaded(ThunarFile *file);
gboolean th_file_exists(const ThunarFile *file);
gboolean th_file_load_content_type(ThunarFile *file);

#define th_file_has_parent(file) (!th_file_is_root(THUNARFILE((file))))
#define th_file_has_uri_scheme(file, uri_scheme) \
    (g_file_has_uri_scheme(th_file_get_file(file), (uri_scheme)))

gboolean th_file_is_ancestor(const ThunarFile *file,
                             const ThunarFile *ancestor);
gboolean th_file_is_chmodable(const ThunarFile *file);
gboolean th_file_is_desktop_file(const ThunarFile *file, gboolean *is_secure);
gboolean th_file_is_directory(const ThunarFile *file) G_GNUC_PURE;
gboolean th_file_is_executable(const ThunarFile *file);
gboolean th_file_is_gfile_ancestor(const ThunarFile *file, GFile *ancestor);
gboolean th_file_is_hidden(const ThunarFile *file);
gboolean th_file_is_local(const ThunarFile *file);
gboolean th_file_is_mountable(const ThunarFile *file) G_GNUC_PURE;
gboolean th_file_is_mounted(const ThunarFile *file);
gboolean th_file_is_parent(const ThunarFile *file, const ThunarFile *child);
gboolean th_file_is_regular(const ThunarFile *file) G_GNUC_PURE;
gboolean th_file_is_renameable(const ThunarFile *file);
#define  th_file_is_root(file) (e_file_is_root(th_file_get_file(file)))
gboolean th_file_is_shortcut(const ThunarFile *file) G_GNUC_PURE;
gboolean th_file_is_symlink(const ThunarFile *file);
gboolean th_file_is_trashed(const ThunarFile *file);
gboolean th_file_is_writable(const ThunarFile *file);

// compare --------------------------------------------------------------------

gint th_file_compare_by_name(const ThunarFile *file_a,
                             const ThunarFile *file_b,
                             gboolean case_sensitive) G_GNUC_PURE;
gint th_file_compare_by_type(ThunarFile *file_a, ThunarFile *file_b);

// actions --------------------------------------------------------------------

gboolean th_file_execute(ThunarFile *file, GFile *working_directory,
                         gpointer parent, GList *path_list,
                         const gchar *startup_id, GError **error);
gboolean th_file_launch(ThunarFile *file, gpointer parent,
                        const gchar *startup_id, GError **error);
gboolean th_file_rename(ThunarFile *file, const gchar *name,
                        GCancellable *cancellable, gboolean called_from_job,
                        GError **error);

// monitor --------------------------------------------------------------------

void th_file_watch(ThunarFile *file);
void th_file_unwatch(ThunarFile *file);
void monitor_print_event(GFileMonitor *monitor,
                         GFile *event_file, GFile *other_file,
                         GFileMonitorEvent event_type,
                         gpointer user_data);

// reload ---------------------------------------------------------------------

gboolean th_file_reload(ThunarFile *file);
void th_file_reload_idle_unref(ThunarFile *file);

// Emits the ::changed signal on file. This function is meant to be called
// by derived classes whenever they notice changes to the file.

#define th_file_changed(file) \
G_STMT_START{ \
    fileinfo_changed(FILEINFO((file))); \
}G_STMT_END

// cache ----------------------------------------------------------------------

ThunarFile* th_file_cache_lookup(const GFile *file);
gchar* th_file_cached_display_name(const GFile *file);
gboolean th_file_cache_dump(gpointer user_data);

// file list ------------------------------------------------------------------

GList* thlist_get_applications(GList *thlist);
GList* thlist_to_glist(GList *thlist);

G_END_DECLS

#endif // __TH_FILE_H__


