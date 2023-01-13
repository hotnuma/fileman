/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __PROPERTIES_DIALOG_H__
#define __PROPERTIES_DIALOG_H__

#include <th-file.h>

G_BEGIN_DECLS

typedef struct _PropertiesDialogClass PropertiesDialogClass;
typedef struct _PropertiesDialog      PropertiesDialog;

#define TYPE_PROPERTIES_DIALOG (propsdlg_get_type())
#define PROPERTIES_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PROPERTIES_DIALOG, PropertiesDialog))
#define PROPERTIES_DIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_PROPERTIES_DIALOG, PropertiesDialogClass))
#define IS_PROPERTIES_DIALOG(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PROPERTIES_DIALOG))
#define IS_PROPERTIES_DIALOG_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_PROPERTIES_DIALOG))
#define PROPERTIES_DIALOG_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_PROPERTIES_DIALOG, PropertiesDialog))

GType propsdlg_get_type() G_GNUC_CONST;

GtkWidget* propsdlg_new(GtkWindow *parent);
void propsdlg_set_files(PropertiesDialog *dialog, GList *files);
void propsdlg_set_file(PropertiesDialog *dialog, ThunarFile *file);

G_END_DECLS

#endif // __PROPERTIES_DIALOG_H__

