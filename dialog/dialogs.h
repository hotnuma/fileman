/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
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

#ifndef __THUNAR_DIALOGS_H__
#define __THUNAR_DIALOGS_H__

#include <enum-types.h>
#include <th-file.h>
#include <thunar-job.h>

G_BEGIN_DECLS

gboolean dialog_insecure_program(gpointer parent, const gchar *title,
                                 ThunarFile *file, const gchar *command);

gchar* dialog_create(gpointer parent, const gchar *content_type,
                     const gchar *filename, const gchar *title);
gboolean dialog_folder_trash(GtkWindow *window);
ThunarJob* dialog_rename_file(gpointer parent, ThunarFile *file);

void dialog_error(gpointer parent, const GError *error,
                  const gchar *format, ...) G_GNUC_PRINTF (3, 4);

ThunarJobResponse dialog_job_ask(GtkWindow *parent, const gchar *question,
                                 ThunarJobResponse choices);
ThunarJobResponse dialog_job_ask_replace(GtkWindow *parent,
                                         ThunarFile *src_file,
                                         ThunarFile *dst_file);
void dialog_job_error(GtkWindow *parent, GError *error);

G_END_DECLS

#endif // __THUNAR_DIALOGS_H__


