/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __PROGRESSDIALOG_H__
#define __PROGRESSDIALOG_H__

#include <gtk/gtk.h>
#include <job.h>

G_BEGIN_DECLS

// ProgressDialog -------------------------------------------------------------

typedef struct _ProgressDialogClass ProgressDialogClass;
typedef struct _ProgressDialog      ProgressDialog;

#define TYPE_PROGRESSDIALOG (progressdlg_get_type())
#define PROGRESSDIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_PROGRESSDIALOG, ProgressDialog))
#define PROGRESSDIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_PROGRESSDIALOG, ProgressDialogClass))
#define IS_PROGRESSDIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_PROGRESSDIALOG))
#define IS_PROGRESSDIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_PROGRESSDIALOG))
#define PROGRESSDIALOG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_PROGRESSDIALOG, ProgressDialogClass))

GType progressdlg_get_type() G_GNUC_CONST;

GtkWidget* progressdlg_new();
gboolean progressdlg_has_jobs(ProgressDialog *dialog);
GList* progressdlg_list_jobs(ProgressDialog *dialog);
void progressdlg_add_job(ProgressDialog *dialog, ThunarJob *job,
                         const gchar *icon_name, const gchar *title);

G_END_DECLS

#endif // __PROGRESSDIALOG_H__


