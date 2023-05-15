/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __APPCHOOSERDIALOG_H__
#define __APPCHOOSERDIALOG_H__

#include <th_file.h>

G_BEGIN_DECLS

// AppChooserDialog -----------------------------------------------------------

typedef struct _AppChooserDialogClass AppChooserDialogClass;
typedef struct _AppChooserDialog      AppChooserDialog;

#define TYPE_APPCHOOSERDIALOG (appchooser_get_type())
#define APPCHOOSERDIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_APPCHOOSERDIALOG, AppChooserDialog))
#define APPCHOOSERDIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_APPCHOOSERDIALOG, AppChooserDialogClass))
#define IS_APPCHOOSERDIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_APPCHOOSERDIALOG))
#define IS_APPCHOOSERDIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_APPCHOOSERDIALOG))
#define APPCHOOSERDIALOG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_APPCHOOSERDIALOG, AppChooserDialogClass))

GType appchooser_get_type() G_GNUC_CONST;

void appchooser_dialog(gpointer parent, ThunarFile *file, gboolean open);

G_END_DECLS

#endif // __APPCHOOSERDIALOG_H__


