/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __TREEVIEW_H__
#define __TREEVIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _TreeView      TreeView;
typedef struct _TreeViewClass TreeViewClass;

#define TYPE_TREEVIEW (treeview_get_type())

#define TREEVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_TREEVIEW, TreeView))
#define TREEVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_TREEVIEW, TreeViewClass))
#define IS_TREEVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_TREEVIEW))
#define IS_TREEVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_TREEVIEW))
#define TREEVIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_TREEVIEW, TreeViewClass))

GType treeview_get_type() G_GNUC_CONST;

GtkWidget* treeview_new() G_GNUC_MALLOC;

gboolean treeview_delete_selected(TreeView *view);
void treeview_rename_selected(TreeView *view);

G_END_DECLS

#endif // __TREEVIEW_H__


