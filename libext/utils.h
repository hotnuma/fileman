/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
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

#ifndef __UTILS_H__
#define __UTILS_H__

#include <enumtypes.h>
#include <gtk/gtk.h>

#define UTIL_THREADS_ENTER           \
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
    gdk_threads_enter();             \
    G_GNUC_END_IGNORE_DEPRECATIONS

#define UTIL_THREADS_LEAVE           \
    G_GNUC_BEGIN_IGNORE_DEPRECATIONS \
    gdk_threads_leave();             \
    G_GNUC_END_IGNORE_DEPRECATIONS

G_BEGIN_DECLS

// str
gchar* util_str_get_extension(const gchar *name) G_GNUC_WARN_UNUSED_RESULT;
gchar* util_str_escape(const gchar *source);
gchar* util_str_replace(const gchar *str, const gchar *pattern,
                        const gchar *replace);

gchar* util_expand_filename(const gchar *filename, GFile *working_directory,
                            GError **error)
                            G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* util_expand_field_codes(const gchar *command, GSList *uri_list,
                               const gchar *icon, const gchar *name,
                               const gchar *uri, gboolean requires_terminal);
// chdir
gchar* util_change_working_directory(const gchar *new_directory)
                                     G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
// GString
void util_append_quoted(GString *string, const gchar *unquoted);

// set display (used by g_spaw_async exo-desktop-item-edit)
void util_set_display_env(gpointer data);

// parent screen
GdkScreen* util_parse_parent(gpointer parent, GtkWindow **window_return)
                             G_GNUC_WARN_UNUSED_RESULT;
// time
time_t util_time_from_rfc3339(const gchar *date_string)
                              G_GNUC_WARN_UNUSED_RESULT;
gchar* util_humanize_file_time(guint64 file_time,
                               ThunarDateStyle date_style,
                               const gchar *date_custom_style)
                               G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
gchar* util_strdup_strftime(const gchar *format, const struct tm *tm);

G_END_DECLS

#endif // __UTILS_H__


