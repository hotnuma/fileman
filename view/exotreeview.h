/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __EXO_TREEVIEW_H__
#define __EXO_TREEVIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ExoTreeViewPrivate  ExoTreeViewPrivate;
typedef struct _ExoTreeViewClass    ExoTreeViewClass;
typedef struct _ExoTreeView         ExoTreeView;

#define EXO_TYPE_TREE_VIEW (exo_treeview_get_type())
#define EXO_TREE_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  EXO_TYPE_TREE_VIEW, ExoTreeView))
#define EXO_TREE_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   EXO_TYPE_TREE_VIEW, ExoTreeViewClass))
#define EXO_IS_TREE_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  EXO_TYPE_TREE_VIEW))
#define EXO_IS_TREE_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   EXO_TYPE_TREE_VIEW))
#define EXO_TREE_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   EXO_TYPE_TREE_VIEW, ExoTreeViewClass))

struct _ExoTreeViewClass
{
    //< private >
    GtkTreeViewClass __parent__;

    //< private >
    void (*reserved1) ();
    void (*reserved2) ();
    void (*reserved3) ();
    void (*reserved4) ();
    void (*reserved5) ();
    void (*reserved6) ();
    void (*reserved7) ();
    void (*reserved8) ();
};

struct _ExoTreeView
{
    GtkTreeView __parent__;

    ExoTreeViewPrivate *priv;
};

GType exo_treeview_get_type() G_GNUC_CONST;

GtkWidget *exo_treeview_new() G_GNUC_MALLOC;

gboolean exo_treeview_get_single_click(const ExoTreeView *tree_view);
void exo_treeview_set_single_click(ExoTreeView *tree_view, gboolean single_click);

guint exo_treeview_get_single_click_timeout(const ExoTreeView *tree_view);
void exo_treeview_set_single_click_timeout(ExoTreeView *tree_view,
                                           guint single_click_timeout);

G_END_DECLS

#endif // __EXO_TREEVIEW_H__


