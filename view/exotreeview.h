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

#ifndef DISABLE_EXOTREEVIEW

#include <gtk/gtk.h>

G_BEGIN_DECLS

// ExoTreeView ----------------------------------------------------------------

typedef struct _ExoTreeViewPrivate  ExoTreeViewPrivate;
typedef struct _ExoTreeViewClass    ExoTreeViewClass;
typedef struct _ExoTreeView         ExoTreeView;

#define TYPE_EXOTREEVIEW (exo_treeview_get_type())
#define EXOTREEVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_EXOTREEVIEW, ExoTreeView))
#define EXOTREEVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_EXOTREEVIEW, ExoTreeViewClass))
#define IS_EXOTREEVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_EXOTREEVIEW))
#define IS_EXOTREEVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_EXOTREEVIEW))
#define EXOTREEVIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_EXOTREEVIEW, ExoTreeViewClass))

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

#endif

#endif // __EXO_TREEVIEW_H__


