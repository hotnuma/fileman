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

#ifndef __THUNAR_FILE_H__
#define __THUNAR_FILE_H__

#include <gtk/gtk.h>
#include <usermanager.h>
#include <fileinfo.h>
#include <enumtypes.h>

G_BEGIN_DECLS

typedef struct _ThunarFileClass ThunarFileClass;
typedef struct _ThunarFile      ThunarFile;

#define THUNAR_TYPE_FILE (th_file_get_type())
#define THUNAR_FILE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  THUNAR_TYPE_FILE, ThunarFile))
#define THUNAR_FILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   THUNAR_TYPE_FILE, ThunarFileClass))
#define THUNAR_IS_FILE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  THUNAR_TYPE_FILE))
#define THUNAR_IS_FILE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   THUNAR_TYPE_FILE))
#define THUNAR_FILE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   THUNAR_TYPE_FILE, ThunarFileClass))

typedef enum
{
    THUNAR_FILE_DATE_ACCESSED,
    THUNAR_FILE_DATE_CHANGED,
    THUNAR_FILE_DATE_MODIFIED,

} ThunarFileDateType;

typedef enum
{
    THUNAR_FILE_ICON_STATE_DEFAULT,
    THUNAR_FILE_ICON_STATE_DROP,
    THUNAR_FILE_ICON_STATE_OPEN,

} ThunarFileIconState;

// Callback type for loading ThunarFile's asynchronously. If you want to keep
// the #ThunarFile, you need to ref it, else it will be destroyed.
typedef void (*ThunarFileGetFunc) (GFile      *location,
                                   ThunarFile *file,
                                   GError     *error,
                                   gpointer   user_data);

GType th_file_get_type() G_GNUC_CONST;

// Get ------------------------------------------------------------------------

ThunarFile* th_file_get(GFile *file, GError **error);
ThunarFile* th_file_get_with_info(GFile *file, GFileInfo *info,
                                  gboolean not_mounted);
ThunarFile* th_file_get_for_uri(const gchar *uri, GError **error);
void th_file_get_async(GFile *location, GCancellable *cancellable,
                       ThunarFileGetFunc func, gpointer user_data);

GFile* th_file_get_file(const ThunarFile *file) G_GNUC_PURE;
GFileInfo* th_file_get_info(const ThunarFile *file) G_GNUC_PURE;
ThunarFile* th_file_get_parent(const ThunarFile *file, GError **error);

gboolean th_file_check_loaded(ThunarFile *file);

gboolean th_file_execute(ThunarFile *file, GFile *working_directory,
                         gpointer parent, GList *path_list,
                         const gchar *startup_id, GError **error);

gboolean        th_file_launch(ThunarFile *file,
                               gpointer parent,
                               const gchar *startup_id,
                               GError **error);

gboolean        th_file_rename(ThunarFile *file,
                               const gchar *name,
                               GCancellable *cancellable,
                               gboolean called_from_job,
                               GError **error);

GdkDragAction   th_file_accepts_drop(ThunarFile *file,
                                     GList *path_list,
                                     GdkDragContext *context,
                                     GdkDragAction *suggested_action_return);

guint64         th_file_get_date(const ThunarFile *file,
                                 ThunarFileDateType date_type) G_GNUC_PURE;

gchar*          th_file_get_date_string(const ThunarFile *file,
                                        ThunarFileDateType date_type,
                                        ThunarDateStyle date_style,
                                        const gchar *date_custom_style)
                                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar*          th_file_get_mode_string(const ThunarFile *file)
                                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gchar*          th_file_get_size_in_bytes_string(const ThunarFile *file)
                                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar*          th_file_get_size_string_formatted(const ThunarFile *file,
                                                  const gboolean file_size_binary);
gchar*          th_file_get_size_string_long(const ThunarFile *file,
                                             const gboolean file_size_binary);

GVolume*        th_file_get_volume(const ThunarFile *file);

ThunarGroup*    th_file_get_group(const ThunarFile *file);
ThunarUser*     th_file_get_user(const ThunarFile *file);

const gchar*    th_file_get_content_type(ThunarFile *file);
gboolean        th_file_load_content_type(ThunarFile *file);
const gchar*    th_file_get_symlink_target(const ThunarFile *file);
const gchar*    th_file_get_basename(const ThunarFile *file) G_GNUC_CONST;
gboolean        th_file_is_symlink(const ThunarFile *file);
guint64         th_file_get_size(const ThunarFile *file);
GAppInfo*       th_file_get_default_handler(const ThunarFile *file);
GFileType       th_file_get_kind(const ThunarFile *file) G_GNUC_PURE;
GFile*          th_file_get_target_location(const ThunarFile *file);
ThunarFileMode  th_file_get_mode(const ThunarFile *file);
gboolean        th_file_is_mounted(const ThunarFile *file);
gboolean        th_file_exists(const ThunarFile *file);
gboolean        th_file_is_directory(const ThunarFile *file) G_GNUC_PURE;
gboolean        th_file_is_shortcut(const ThunarFile *file) G_GNUC_PURE;
gboolean        th_file_is_mountable(const ThunarFile *file) G_GNUC_PURE;
gboolean        th_file_is_local(const ThunarFile *file);
gboolean        th_file_is_parent(const ThunarFile *file, const ThunarFile *child);
gboolean        th_file_is_gfile_ancestor(const ThunarFile *file, GFile *ancestor);
gboolean        th_file_is_ancestor(const ThunarFile *file, const ThunarFile *ancestor);
gboolean        th_file_is_executable(const ThunarFile *file);
gboolean        th_file_is_writable(const ThunarFile *file);
gboolean        th_file_is_hidden(const ThunarFile *file);
gboolean        th_file_is_regular(const ThunarFile *file) G_GNUC_PURE;
gboolean        th_file_is_trashed(const ThunarFile *file);
gboolean        th_file_is_desktop_file(const ThunarFile *file, gboolean *is_secure);
const gchar*    th_file_get_display_name(const ThunarFile *file) G_GNUC_CONST;

gchar*          th_file_get_deletion_date(const ThunarFile *file,
                                          ThunarDateStyle date_style,
                                          const gchar *date_custom_style)
                                          G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
const gchar*    th_file_get_original_path(const ThunarFile *file);
guint32         th_file_get_item_count(const ThunarFile *file);

gboolean        th_file_is_chmodable(const ThunarFile *file);
gboolean        th_file_is_renameable(const ThunarFile *file);
gboolean        th_file_can_be_trashed(const ThunarFile *file);


const gchar*    th_file_get_custom_icon(const ThunarFile *file);

const gchar*    th_file_get_icon_name(ThunarFile *file,
                                      ThunarFileIconState icon_state,
                                      GtkIconTheme *icon_theme);

void            th_file_watch(ThunarFile *file);
void            th_file_unwatch(ThunarFile *file);

gboolean        th_file_reload(ThunarFile *file);
void            th_file_reload_idle_unref(ThunarFile *file);

void            th_file_destroy(ThunarFile *file);

gint            th_file_compare_by_type(ThunarFile *file_a, ThunarFile *file_b);
gint            th_file_compare_by_name(const ThunarFile *file_a,
                                        const ThunarFile *file_b,
                                        gboolean case_sensitive)
                                        G_GNUC_PURE;

ThunarFile*     th_file_cache_lookup(const GFile *file);
gchar*          th_file_cached_display_name(const GFile *file);


GList*          th_file_list_get_applications(GList *file_list);
GList*          th_file_list_to_thunar_g_file_list(GList *file_list);

#define th_file_is_root(file) (e_file_is_root(th_file_get_file(file)))
#define th_file_has_parent(file) (!th_file_is_root(THUNAR_FILE((file))))
#define th_file_has_uri_scheme(file, uri_scheme) (g_file_has_uri_scheme(th_file_get_file(file),(uri_scheme)))
#define th_file_dup_uri(file) (g_file_get_uri(th_file_get_file(file)))

// Emits the ::changed signal on @file. This function is meant to be called
// by derived classes whenever they notice changes to the @file.
#define th_file_changed(file) \
G_STMT_START{ \
    fileinfo_changed(FILEINFO((file))); \
}G_STMT_END

G_END_DECLS

#endif // __THUNAR_FILE_H__


