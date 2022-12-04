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

#ifndef __THUNAR_TREE_VIEW_H__
#define __THUNAR_TREE_VIEW_H__

#include <thunar-navigator.h>

G_BEGIN_DECLS;

typedef struct _ThunarTreeViewClass ThunarTreeViewClass;
typedef struct _ThunarTreeView      ThunarTreeView;

#define THUNAR_TYPE_TREE_VIEW               (thunar_tree_view_get_type ())
#define THUNAR_TREE_VIEW(obj)               (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TREE_VIEW, ThunarTreeView))
#define THUNAR_TREE_VIEW_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TREE_VIEW, ThunarTreeViewClass))
#define THUNAR_IS_TREE_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TREE_VIEW))
#define THUNAR_IS_TREE_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TREE_VIEW))
#define THUNAR_TREE_VIEW_GET_CLASS(obj)     (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TREE_VIEW, ThunarTreeViewClass))

GType thunar_tree_view_get_type() G_GNUC_CONST;

GtkWidget *thunar_tree_view_new() G_GNUC_MALLOC;
gboolean thunar_tree_view_delete_selected_files(ThunarTreeView *view);

G_END_DECLS;

#endif /* !__THUNAR_TREE_VIEW_H__ */


