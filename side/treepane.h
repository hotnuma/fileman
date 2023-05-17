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

#ifndef __TREEPANE_H__
#define __TREEPANE_H__

#include <treeview.h>

G_BEGIN_DECLS

// TreePane -------------------------------------------------------------------

typedef struct _TreePaneClass TreePaneClass;
typedef struct _TreePane      TreePane;

#define TYPE_TREEPANE (treepane_get_type())
#define TREEPANE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_TREEPANE, TreePane))
#define TREEPANE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_TREEPANE, TreePaneClass))
#define IS_TREEPANE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_TREEPANE))
#define IS_TREEPANE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_TREEPANE))
#define TREEPANE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_TREEPANE, TreePaneClass))

GType treepane_get_type() G_GNUC_CONST;

TreeView* treepane_get_view(TreePane *tree_pane);

G_END_DECLS

#endif // __TREEPANE_H__


