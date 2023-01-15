/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
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

#include <config.h>
#include <treepane.h>

#include <libext.h>
#include <treeview.h>

/* Property identifiers */
enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_SELECTED_FILES,
    PROP_SHOW_HIDDEN,
};

static void treepane_component_init(ThunarComponentIface *iface);
static void treepane_navigator_init(ThunarNavigatorIface *iface);
static void treepane_sidepane_init(SidePaneIface *iface);

static void treepane_dispose(GObject *object);
static void treepane_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void treepane_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);

static ThunarFile *treepane_get_current_directory(ThunarNavigator *navigator);
static void treepane_set_current_directory(ThunarNavigator *navigator,
                                                   ThunarFile *current_directory);

static gboolean treepane_get_show_hidden(SidePane *side_pane);
static void treepane_set_show_hidden(SidePane *side_pane,
                                             gboolean show_hidden);

struct _TreePaneClass
{
    GtkScrolledWindowClass __parent__;
};

struct _TreePane
{
    GtkScrolledWindow __parent__;

    ThunarFile  *current_directory;
    GtkWidget   *view;
    gboolean    show_hidden;
};

G_DEFINE_TYPE_WITH_CODE(TreePane,
                        treepane,
                        GTK_TYPE_SCROLLED_WINDOW,
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_NAVIGATOR,
                                              treepane_navigator_init)
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_COMPONENT,
                                              treepane_component_init)
                        G_IMPLEMENT_INTERFACE(TYPE_SIDEPANE,
                                              treepane_sidepane_init))

static void treepane_class_init(TreePaneClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = treepane_dispose;
    gobject_class->get_property = treepane_get_property;
    gobject_class->set_property = treepane_set_property;

    /* override ThunarNavigator's properties */
    g_object_class_override_property(gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     "current-directory");

    /* override ThunarComponent's properties */
    g_object_class_override_property(gobject_class,
                                     PROP_SELECTED_FILES,
                                     "selected-files");

    /* override SidePane's properties */
    g_object_class_override_property(gobject_class,
                                     PROP_SHOW_HIDDEN,
                                     "show-hidden");
}

static void treepane_component_init(ThunarComponentIface *iface)
{
    iface->get_selected_files = (gpointer) e_noop_null;
    iface->set_selected_files = (gpointer) e_noop;
}

static void treepane_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = treepane_get_current_directory;
    iface->set_current_directory = treepane_set_current_directory;
}

static void treepane_sidepane_init(SidePaneIface *iface)
{
    iface->get_show_hidden = treepane_get_show_hidden;
    iface->set_show_hidden = treepane_set_show_hidden;
}

static void treepane_init(TreePane *tree_pane)
{
    /* configure the GtkScrolledWindow */
    gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(tree_pane), NULL);
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(tree_pane), NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(tree_pane), GTK_SHADOW_IN);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(tree_pane), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);

    /* allocate the tree view */
    tree_pane->view = treeview_new();

    g_object_bind_property(G_OBJECT(tree_pane), "show-hidden", G_OBJECT(tree_pane->view), "show-hidden", G_BINDING_SYNC_CREATE);
    g_object_bind_property(G_OBJECT(tree_pane), "current-directory", G_OBJECT(tree_pane->view), "current-directory", G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(tree_pane->view), "change-directory", G_CALLBACK(navigator_change_directory), tree_pane);
    gtk_container_add(GTK_CONTAINER(tree_pane), tree_pane->view);
    gtk_widget_show(tree_pane->view);
}

static void treepane_dispose(GObject *object)
{
    TreePane *tree_pane = TREEPANE(object);

    navigator_set_current_directory(THUNAR_NAVIGATOR(tree_pane), NULL);
    thunar_component_set_selected_files(THUNAR_COMPONENT(tree_pane), NULL);

    G_OBJECT_CLASS(treepane_parent_class)->dispose(object);
}

static void treepane_get_property(GObject    *object,
                                          guint       prop_id,
                                          GValue     *value,
                                          GParamSpec *pspec)
{
    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value,
                           navigator_get_current_directory(THUNAR_NAVIGATOR(object)));
        break;

    case PROP_SELECTED_FILES:
        g_value_set_boxed(value,
                          thunar_component_get_selected_files(THUNAR_COMPONENT(object)));
        break;

    case PROP_SHOW_HIDDEN:
        g_value_set_boolean(value,
                            sidepane_get_show_hidden(SIDEPANE(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void treepane_set_property(GObject      *object,
                                          guint         prop_id,
                                          const GValue *value,
                                          GParamSpec   *pspec)
{
    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNAR_NAVIGATOR(object),
                                               g_value_get_object(value));
        break;

    case PROP_SELECTED_FILES:
        thunar_component_set_selected_files(THUNAR_COMPONENT(object),
                                            g_value_get_boxed(value));
        break;

    case PROP_SHOW_HIDDEN:
        sidepane_set_show_hidden(SIDEPANE(object),
                                         g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static ThunarFile* treepane_get_current_directory(ThunarNavigator *navigator)
{
    return TREEPANE(navigator)->current_directory;
}

static void treepane_set_current_directory(ThunarNavigator  *navigator,
                                                   ThunarFile       *current_directory)
{
    TreePane *tree_pane = TREEPANE(navigator);

    /* disconnect from the previously set current directory */
    if (G_LIKELY(tree_pane->current_directory != NULL))
        g_object_unref(G_OBJECT(tree_pane->current_directory));

    /* activate the new directory */
    tree_pane->current_directory = current_directory;

    /* connect to the new directory */
    if (G_LIKELY(current_directory != NULL))
        g_object_ref(G_OBJECT(current_directory));

    /* notify listeners */
    g_object_notify(G_OBJECT(tree_pane), "current-directory");
}

static gboolean treepane_get_show_hidden(SidePane *side_pane)
{
    return TREEPANE(side_pane)->show_hidden;
}

static void treepane_set_show_hidden(SidePane *side_pane,
                                             gboolean       show_hidden)
{
    TreePane *tree_pane = TREEPANE(side_pane);

    show_hidden = !!show_hidden;

    /* check if we have a new setting */
    if (G_UNLIKELY(tree_pane->show_hidden != show_hidden))
    {
        /* remember the new setting */
        tree_pane->show_hidden = show_hidden;

        /* notify listeners */
        g_object_notify(G_OBJECT(tree_pane), "show-hidden");
    }
}

TreeView* treepane_get_view(TreePane *tree_pane)
{
    return TREEVIEW(tree_pane->view);
}


