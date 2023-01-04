/*-
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __THUNAR_IO_JOBS_H__
#define __THUNAR_IO_JOBS_H__

#include <thunar-job.h>
#include <enum-types.h>

G_BEGIN_DECLS

ThunarJob* io_list_directory(GFile *directory) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarJob* io_make_directories(GList *file_list) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarJob* io_create_files(GList *file_list, GFile *template_file)
                           G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_unlink_files(GList *file_list) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_move_files(GList *source_file_list, GList *target_file_list)
                         G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_copy_files(GList *source_file_list, GList *target_file_list)
                         G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_link_files(GList *source_file_list, GList *target_file_list)
                         G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_trash_files(GList *file_list) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_restore_files(GList *source_file_list, GList *target_file_list)
                            G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarJob* io_rename_file(ThunarFile *file, const gchar *display_name)
                          G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

ThunarJob* io_change_group(GList *files, guint32 gid, gboolean recursive)
                           G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarJob* io_change_mode(GList *files, ThunarFileMode dir_mask, ThunarFileMode dir_mode,
                          ThunarFileMode file_mask, ThunarFileMode file_mode,
                          gboolean recursive) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif // __THUNAR_IO_JOBS_H__


