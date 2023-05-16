/*-
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __GIO_EXT_H__
#define __GIO_EXT_H__

#include <gio/gio.h>

G_BEGIN_DECLS

// GAppInfo -------------------------------------------------------------------

gboolean e_app_info_launch(GAppInfo *info, GFile *working_directory,
                           GList *path_list, GAppLaunchContext *context,
                           GError **error);
gboolean e_app_info_should_show(GAppInfo *info);

// GFile ----------------------------------------------------------------------

GFile* e_file_new_for_home();
GFile* e_file_new_for_root();
GFile* e_file_new_for_trash();

gchar* e_file_get_display_name(GFile *file);
gchar* e_file_get_display_name_remote(GFile *file);
gboolean e_file_get_free_space(GFile *file, guint64 *fs_free_return,
                               guint64 *fs_size_return);
gchar* e_file_get_free_space_string(GFile *file, gboolean file_size_binary);
gchar* e_file_get_location(GFile *file);

gboolean e_file_is_computer(GFile *file);
gboolean e_file_is_home(GFile *file);
gboolean e_file_is_network(GFile *file);
gboolean e_file_is_root(GFile *file);
gboolean e_file_is_trash(GFile *file);
gboolean e_file_is_trashed(GFile *file);

gboolean e_file_move(GFile                  *source,
                     GFile                  *destination,
                     GFileCopyFlags         flags,
                     GCancellable           *cancellable,
                     GFileProgressCallback  progress_callback,
                     gpointer               progress_callback_data,
                     GError                 **error);

// GKeyFile -------------------------------------------------------------------

GKeyFile* e_file_query_key_file(GFile *file, GCancellable *cancellable,
                                GError **error);

// GList ------------------------------------------------------------------

#define TYPE_EFILELIST (e_list_get_type())
GType e_list_get_type();

GList* e_list_new_from_string(const gchar *string);
gchar** e_list_to_stringv(GList *list);
GList* e_list_get_parents(GList *list);

GList* e_list_copy(GList *list);
void e_list_free(GList *list);

// deep copy jobs for GLists
#define e_list_append_ref(list, object) \
    g_list_append(list, g_object_ref(G_OBJECT(object)))
#define e_list_prepend_ref(list, object) \
    g_list_prepend(list, g_object_ref(G_OBJECT(object)))

// VFS ------------------------------------------------------------------------

gboolean e_vfs_is_uri_scheme_supported(const gchar *scheme);

G_END_DECLS

#endif // __GIO_EXT_H__


