/*-
 * Copyright(c) 2006      Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
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

#include <config.h>
#include <treeview.h>

#include <utils.h>

#include <application.h>
#include <clipboard.h>
#include <th_device.h>
#include <dialogs.h>
#include <dnd.h>
#include <gio_ext.h>
#include <gtk_ext.h>
#include <marshal.h>
#include <appmenu.h>
#include <propsdlg.h>
#include <shortrender.h>
#include <simplejob.h>
#include <treemodel.h>

#include <gdk/gdkkeysyms.h>

// drag dest expand timeout
#define TREEVIEW_EXPAND_TIMEOUT 750

// Identifiers for DnD target types
enum
{
    TARGET_TEXT_URI_LIST,
};

// Property identifiers
enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_SHOW_HIDDEN,
};

static void treeview_navigator_init(ThunarNavigatorIface *iface);
static void treeview_finalize(GObject *object);
static void treeview_get_property(GObject *object,
                                          guint prop_id,
                                          GValue *value,
                                          GParamSpec *pspec);
static void treeview_set_property(GObject *object,
                                          guint prop_id,
                                          const GValue *value,
                                          GParamSpec *pspec);

static ThunarFile *treeview_get_current_directory(ThunarNavigator *navigator);
static void treeview_set_current_directory(ThunarNavigator *navigator,
                                                   ThunarFile *current_directory);

static void treeview_realize(GtkWidget *widget);
static void treeview_unrealize(GtkWidget *widget);


static void _treeview_action_open(TreeView *view);
static void _treeview_open_selection(TreeView *view);

static void _treeview_select_files(TreeView *view,
                                          GList *files_to_selected);

static gboolean treeview_button_press_event(GtkWidget *widget,
                                                    GdkEventButton *event);
static gboolean treeview_button_release_event(GtkWidget *widget,
                                                      GdkEventButton *event);
static gboolean _treeview_key_press_event(GtkWidget *widget,
                                                 GdkEventKey *event);

static void treeview_drag_data_received(GtkWidget *widget,
                                                GdkDragContext *context,
                                                gint x,
                                                gint y,
                                                GtkSelectionData *selection_data,
                                                guint info,
                                                guint time);
static gboolean treeview_drag_drop(GtkWidget *widget,
                                           GdkDragContext *context,
                                           gint x,
                                           gint y,
                                           guint time);
static gboolean treeview_drag_motion(GtkWidget *widget,
                                             GdkDragContext *context,
                                             gint x,
                                             gint y,
                                             guint time);
static void treeview_drag_leave(GtkWidget *widget,
                                        GdkDragContext *context,
                                        guint time);

static gboolean treeview_popup_menu(GtkWidget *widget);

static void treeview_row_activated(GtkTreeView *tree_view,
                                           GtkTreePath *path,
                                           GtkTreeViewColumn *column);
static gboolean treeview_test_expand_row(GtkTreeView *tree_view,
                                                 GtkTreeIter *iter,
                                                 GtkTreePath *path);
static void treeview_row_collapsed(GtkTreeView *tree_view,
                                           GtkTreeIter *iter,
                                           GtkTreePath *path);

static void _treeview_context_menu(TreeView *view,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter);

static GdkDragAction _treeview_get_dest_actions(TreeView *view,
                                                       GdkDragContext *context,
                                                       gint x,
                                                       gint y,
                                                       guint time,
                                                       ThunarFile **file_return);

static ThunarFile* _treeview_get_selected_file(TreeView *view);
static ThunarDevice* _treeview_get_selected_device(TreeView *view);

static gboolean _treeview_visible_func(TreeModel *model,
                                              ThunarFile *file,
                                              gpointer user_data);
static gboolean _treeview_selection_func(GtkTreeSelection *selection,
                                                GtkTreeModel *model,
                                                GtkTreePath *path,
                                                gboolean path_currently_selected,
                                                gpointer user_data);

static gboolean _treeview_cursor_idle(gpointer user_data);
static void _treeview_cursor_idle_destroy(gpointer user_data);

static gboolean _treeview_drag_scroll_timer(gpointer user_data);
static void _treeview_drag_scroll_timer_destroy(gpointer user_data);

static gboolean _treeview_expand_timer(gpointer user_data);
static void _treeview_expand_timer_destroy(gpointer user_data);

static gboolean _treeview_get_show_hidden(TreeView *view);
static void _treeview_set_show_hidden(TreeView *view,
                                             gboolean show_hidden);

static GtkTreePath* _treeview_get_preferred_toplevel_path(
                                                        TreeView *view,
                                                        ThunarFile *file);

static void _treeview_action_unlink_selected_folder(TreeView *view,
                                                           gboolean permanently);

// Allocation -----------------------------------------------------------------

struct _TreeViewClass
{
    GtkTreeViewClass __parent__;
};

struct _TreeView
{
    GtkTreeView             __parent__;
    ClipboardManager  *clipboard;
    GtkCellRenderer         *icon_renderer;
    ThunarFile              *current_directory;
    TreeModel         *model;

    // whether to display hidden/backup files
    guint                   show_hidden : 1;

    // drop site support
    guint                   drop_data_ready : 1;
    // whether the drop data was received already
    guint                   drop_occurred : 1;
    GList                   *drop_file_list;
    // the list of URIs that are contained in the drop data

    /* the "new-files" closure, which is used to
     * open newly created directories once done.
     */
    GClosure                *new_files_closure;

    /* sometimes we want to keep the cursor on a certain item to allow
     * more intuitive navigation, even though the main view shows another path
     */
    GtkTreePath             *select_path;

    // used to create menu items for the context menu
    ThunarLauncher          *launcher;

    /* the currently pressed mouse button, set in the
     * button-press-event handler if the associated
     * button-release-event should activate.
     */
    gint                    pressed_button;

    // set cursor to current directory idle source
    guint                   cursor_idle_id;

    // autoscroll during drag timer source
    guint                   drag_scroll_timer_id;

    // expand drag dest row timer source
    guint                   expand_timer_id;
};

// Target types for dropping into the tree view
static const GtkTargetEntry drop_targets[] =
{
    {"text/uri-list", 0, TARGET_TEXT_URI_LIST},
};

G_DEFINE_TYPE_WITH_CODE(TreeView,
                        treeview,
                        GTK_TYPE_TREE_VIEW,
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_NAVIGATOR,
                                              treeview_navigator_init))

GtkWidget* treeview_new()
{
    return g_object_new(TYPE_TREEVIEW, NULL);
}

static void treeview_class_init(TreeViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = treeview_finalize;
    gobject_class->get_property = treeview_get_property;
    gobject_class->set_property = treeview_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->realize = treeview_realize;
    gtkwidget_class->unrealize = treeview_unrealize;
    gtkwidget_class->button_press_event = treeview_button_press_event;
    gtkwidget_class->button_release_event = treeview_button_release_event;
    gtkwidget_class->drag_data_received = treeview_drag_data_received;
    gtkwidget_class->drag_drop = treeview_drag_drop;
    gtkwidget_class->drag_motion = treeview_drag_motion;
    gtkwidget_class->drag_leave = treeview_drag_leave;
    gtkwidget_class->popup_menu = treeview_popup_menu;

    GtkTreeViewClass *gtktree_view_class = GTK_TREE_VIEW_CLASS(klass);
    gtktree_view_class->row_activated = treeview_row_activated;
    gtktree_view_class->test_expand_row = treeview_test_expand_row;
    gtktree_view_class->row_collapsed = treeview_row_collapsed;

    // Override ThunarNavigator's properties
    g_object_class_override_property(gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     "current-directory");

    g_object_class_install_property(gobject_class,
                                    PROP_SHOW_HIDDEN,
                                    g_param_spec_boolean("show-hidden",
                                                         "show-hidden",
                                                         "show-hidden",
                                                         FALSE,
                                                         E_PARAM_READWRITE));
}

static void treeview_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = treeview_get_current_directory;
    iface->set_current_directory = treeview_set_current_directory;
}

static void treeview_init(TreeView *view)
{
    GtkTreeViewColumn *column;
    GtkTreeSelection  *selection;
    GtkCellRenderer   *renderer;

    // Create a tree model for this tree view
    view->model = g_object_new(TYPE_TREEMODEL, NULL);

    treemodel_set_visible_func(view->model, _treeview_visible_func, view);
    gtk_tree_view_set_model(GTK_TREE_VIEW(view), GTK_TREE_MODEL(view->model));

    // configure the tree view
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(view), FALSE);
    gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(view), FALSE);

    // allocate a single column for our renderers
    column = g_object_new(GTK_TYPE_TREE_VIEW_COLUMN,
                          "reorderable", FALSE,
                          "resizable", FALSE,
                          "sizing", GTK_TREE_VIEW_COLUMN_AUTOSIZE,
                          "spacing", 2,
                          NULL);
    gtk_tree_view_append_column(GTK_TREE_VIEW(view), column);

    // allocate the special icon renderer
    view->icon_renderer = shrender_new();
    gtk_tree_view_column_pack_start(column, view->icon_renderer, FALSE);
    gtk_tree_view_column_set_attributes(column, view->icon_renderer,
                                        "file", TREEMODEL_COLUMN_FILE,
                                        "device", TREEMODEL_COLUMN_DEVICE,
                                        NULL);

    // allocate the text renderer
    renderer = gtk_cell_renderer_text_new();
    gtk_tree_view_column_pack_start(column, renderer, TRUE);
    gtk_tree_view_column_set_attributes(column, renderer,
                                        "attributes", TREEMODEL_COLUMN_ATTR,
                                        "text", TREEMODEL_COLUMN_NAME,
                                        NULL);

    // setup the tree selection
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_SINGLE);
    gtk_tree_selection_set_select_function(selection, _treeview_selection_func, view, NULL);

    // custom keyboard handler for better navigation
    g_signal_connect(GTK_WIDGET(view), "key_press_event",
                     G_CALLBACK(_treeview_key_press_event), NULL);

    // enable drop support for the tree view
    gtk_drag_dest_set(GTK_WIDGET(view), 0, drop_targets, G_N_ELEMENTS(drop_targets),
                      GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);

    view->new_files_closure = g_cclosure_new_swap(
                                    G_CALLBACK(_treeview_select_files), view, NULL);
    g_closure_ref(view->new_files_closure);
    g_closure_sink(view->new_files_closure);

    view->launcher =  g_object_new(THUNAR_TYPE_LAUNCHER, "widget", GTK_WIDGET(view),
                                   "select-files-closure", view->new_files_closure, NULL);

    g_signal_connect_swapped(G_OBJECT(view->launcher), "change-directory", G_CALLBACK(_treeview_action_open), view);
    g_object_bind_property(G_OBJECT(view), "current-directory", G_OBJECT(view->launcher), "current-directory", G_BINDING_SYNC_CREATE);
}

static void treeview_finalize(GObject *object)
{
    TreeView *view = TREEVIEW(object);

    // release drop path list(if drag_leave wasn't called)
    e_list_free(view->drop_file_list);

    // be sure to cancel the cursor idle source
    if (G_UNLIKELY(view->cursor_idle_id != 0))
        g_source_remove(view->cursor_idle_id);

    // cancel any running autoscroll timer
    if (G_LIKELY(view->drag_scroll_timer_id != 0))
        g_source_remove(view->drag_scroll_timer_id);

    // be sure to cancel the expand timer source
    if (G_UNLIKELY(view->expand_timer_id != 0))
        g_source_remove(view->expand_timer_id);

    // free path remembered for selection
    if (view->select_path != NULL)
        gtk_tree_path_free(view->select_path);

    // reset the current-directory property
    navigator_set_current_directory(THUNAR_NAVIGATOR(view), NULL);

    // free the tree model
    g_object_unref(G_OBJECT(view->model));

    // drop any existing "new-files" closure
    if (G_UNLIKELY(view->new_files_closure != NULL))
    {
        g_closure_invalidate(view->new_files_closure);
        g_closure_unref(view->new_files_closure);
    }

    G_OBJECT_CLASS(treeview_parent_class)->finalize(object);
}

static void treeview_realize(GtkWidget *widget)
{
    TreeView *view = TREEVIEW(widget);
    GdkDisplay     *display;

    // let the parent class realize the widget
    GTK_WIDGET_CLASS(treeview_parent_class)->realize(widget);

    // query the clipboard manager for the display
    display = gtk_widget_get_display(widget);
    view->clipboard = clipman_get_for_display(display);
}

static void treeview_unrealize(GtkWidget *widget)
{
    TreeView *view = TREEVIEW(widget);

    // release the clipboard manager reference
    g_object_unref(G_OBJECT(view->clipboard));
    view->clipboard = NULL;

    // let the parent class unrealize the widget
    GTK_WIDGET_CLASS(treeview_parent_class)->unrealize(widget);
}

// Properties -----------------------------------------------------------------

static void treeview_get_property(GObject   *object,
                                          guint     prop_id,
                                          GValue    *value,
                                          GParamSpec *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, navigator_get_current_directory(THUNAR_NAVIGATOR(object)));
        break;

    case PROP_SHOW_HIDDEN:
        g_value_set_boolean(value, _treeview_get_show_hidden(TREEVIEW(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void treeview_set_property(GObject       *object,
                                          guint         prop_id,
                                          const GValue  *value,
                                          GParamSpec    *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNAR_NAVIGATOR(object), g_value_get_object(value));
        break;

    case PROP_SHOW_HIDDEN:
        _treeview_set_show_hidden(TREEVIEW(object), g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


// Current Directory ----------------------------------------------------------

static ThunarFile* treeview_get_current_directory(
                                                    ThunarNavigator *navigator)
{
    return TREEVIEW(navigator)->current_directory;
}

static void treeview_set_current_directory(ThunarNavigator *navigator,
                                                   ThunarFile *current_directory)
{
    TreeView *view = TREEVIEW(navigator);
    ThunarFile  *file;
    ThunarFile  *file_parent;
    gboolean    needs_refiltering = FALSE;

    // check if we already use that directory
    if (G_UNLIKELY(view->current_directory == current_directory))
        return;

    // check if we have an active directory
    if (G_LIKELY(view->current_directory != NULL))
    {
        // update the filter if the old current directory, or one of it's parents, is hidden
        if (!view->show_hidden)
        {
            // look if the file or one of it's parents is hidden
            for(file = THUNAR_FILE(g_object_ref(G_OBJECT(view->current_directory))); file != NULL; file = file_parent)
            {
                // check if this file is hidden
                if (th_file_is_hidden(file))
                {
                    // schedule an update of the filter after the current directory has been changed
                    needs_refiltering = TRUE;

                    // release the file
                    g_object_unref(G_OBJECT(file));

                    break;
                }

                // get the file parent
                file_parent = th_file_get_parent(file, NULL);

                // release the file
                g_object_unref(G_OBJECT(file));
            }
        }

        // disconnect from the previous current directory
        g_object_unref(G_OBJECT(view->current_directory));
    }

    // activate the new current directory
    view->current_directory = current_directory;

    // connect to the new current directory
    if (G_LIKELY(current_directory != NULL))
    {
        // take a reference on the directory
        g_object_ref(G_OBJECT(current_directory));

        /* update the filter if the new current directory, or one of it's parents, is
         * hidden. we don't have to check this if refiltering needs to be done
         * anyway */
        if (!needs_refiltering && !view->show_hidden)
        {
            // look if the file or one of it's parents is hidden
            for(file = THUNAR_FILE(g_object_ref(G_OBJECT(current_directory))); file != NULL; file = file_parent)
            {
                // check if this file is hidden
                if (th_file_is_hidden(file))
                {
                    // update the filter
                    treemodel_refilter(view->model);

                    // release the file
                    g_object_unref(G_OBJECT(file));

                    break;
                }

                // get the file parent
                file_parent = th_file_get_parent(file, NULL);

                // release the file
                g_object_unref(G_OBJECT(file));
            }
        }

        // schedule an idle source to set the cursor to the current directory
        if (G_LIKELY(view->cursor_idle_id == 0))
            view->cursor_idle_id = g_idle_add_full(G_PRIORITY_LOW, _treeview_cursor_idle, view, _treeview_cursor_idle_destroy);
    }

    // refilter the model if necessary
    if (needs_refiltering)
        treemodel_refilter(view->model);

    // notify listeners
    g_object_notify(G_OBJECT(view), "current-directory");
}


// Show Hidden ----------------------------------------------------------------

static gboolean _treeview_get_show_hidden(TreeView *view)
{
    e_return_val_if_fail(IS_TREEVIEW(view), FALSE);

    return view->show_hidden;
}

static void _treeview_set_show_hidden(TreeView *view,
                                             gboolean        show_hidden)
{
    e_return_if_fail(IS_TREEVIEW(view));
    e_return_if_fail(THUNAR_IS_TREE_MODEL(view->model));

    // normalize the value
    show_hidden = !!show_hidden;

    // check if we have a new setting
    if (view->show_hidden != show_hidden)
    {
        // apply the new setting
        view->show_hidden = show_hidden;

        // update the model
        treemodel_refilter(view->model);

        // notify listeners
        g_object_notify(G_OBJECT(view), "show-hidden");
    }
}

// Events ---------------------------------------------------------------------

static gboolean treeview_button_press_event(GtkWidget      *widget,
                                                    GdkEventButton *event)
{
    TreeView    *view = TREEVIEW(widget);
    ThunarDevice      *device = NULL;
    ThunarFile        *file = NULL;
    GtkTreeViewColumn *column;
    GtkTreePath       *path;
    GtkTreeIter        iter;
    gboolean           result;

    // reset the pressed button state
    view->pressed_button = -1;

    if (event->button == 2)
    {
        // completely ignore double middle clicks
        if (event->type == GDK_2BUTTON_PRESS)
            return TRUE;

        // remember the current selection as we want to restore it later
        gtk_tree_path_free(view->select_path);
        gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &(view->select_path), NULL);
    }

    // let the widget process the event first(handles focussing and scrolling)
    result =
        GTK_WIDGET_CLASS(treeview_parent_class)->button_press_event(widget, event);

    // for the following part, we'll only handle single button presses
    if (event->type != GDK_BUTTON_PRESS)
        return result;

    // resolve the path at the cursor position
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(widget), event->x, event->y, &path, &column, NULL, NULL))
    {
        // check if we should popup the context menu
        if (G_UNLIKELY(event->button == 3))
        {
            // determine the iterator for the path
            if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path))
            {
                // popup the context menu
                _treeview_context_menu(view, GTK_TREE_MODEL(view->model), &iter);

                // we effectively handled the event
                result = TRUE;
            }
        }
        else if (event->button == 1)
        {
            GdkRectangle rect;
            gtk_tree_view_get_cell_area(GTK_TREE_VIEW(widget), path, column, &rect);

            // set cursor only when the user did not click the expander
            if (rect.x <= event->x && event->x <=(rect.x + rect.width))
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(widget), path, NULL, FALSE);

            // remember the button as pressed and handle it in the release handler
            view->pressed_button = event->button;
        }
        else if (event->button == 2)
        {
            // only open the item if it is mounted(otherwise opening and selecting it won't work correctly)
            gtk_tree_path_free(path);
            gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);

            if (path != NULL && gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model),
                                                        &iter, path))
                gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter,
                                    TREEMODEL_COLUMN_FILE, &file,
                                    TREEMODEL_COLUMN_DEVICE, &device, -1);

            if ((device != NULL && th_device_is_mounted(device)) ||
                   (file != NULL && th_file_is_mounted(file)))
            {
                view->pressed_button = event->button;
            }
            else
            {
                gtk_tree_path_free(view->select_path);
                view->select_path = NULL;
            }

            if (device)
                g_object_unref(device);

            if (file)
                g_object_unref(file);
        }

        // release the path
        gtk_tree_path_free(path);
    }

    return result;
}

static gboolean treeview_button_release_event(GtkWidget      *widget,
                                                      GdkEventButton *event)
{
    TreeView *view = TREEVIEW(widget);

    // check if we have an event matching the pressed button state
    if (G_LIKELY(view->pressed_button ==(gint) event->button))
    {
        // check if we should simply open or open in new window
        if (G_LIKELY(event->button == 1))
        {
            _treeview_action_open(view);
        }
        else if (G_UNLIKELY(event->button == 2))
        {
            launcher_open_selected_folders(view->launcher);

            // set the cursor back to the previously selected item
            if (view->select_path != NULL)
            {
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), view->select_path, NULL, FALSE);
                gtk_tree_path_free(view->select_path);
                view->select_path = NULL;
            }
        }
        gtk_widget_grab_focus(widget);
    }

    // reset the pressed button state
    view->pressed_button = -1;

    // call the parent's release event handler
    return GTK_WIDGET_CLASS(treeview_parent_class)->button_release_event(widget, event);
}

static gboolean _treeview_key_press_event(GtkWidget   *widget,
                                                 GdkEventKey *event)
{
    TreeView *view = TREEVIEW(widget);
    ThunarDevice   *device = NULL;
    GtkTreePath    *path;
    GtkTreeIter     iter;
    gboolean        stopPropagation = FALSE;

    // Get path of currently highlighted item
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);

    switch (event->keyval)
    {
    case GDK_KEY_Up:
    case GDK_KEY_KP_Up:
    case GDK_KEY_Down:
    case GDK_KEY_KP_Down:
        // the default actions works good, but we want to update the right pane
        GTK_WIDGET_CLASS(treeview_parent_class)->key_press_event(widget, event);

        // sync with new tree view selection
        gtk_tree_path_free(path);
        gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);
        _treeview_open_selection(view);

        stopPropagation = TRUE;
        break;

    case GDK_KEY_Left:
    case GDK_KEY_KP_Left:
        // if branch is expanded then collapse it
        if (gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
            gtk_tree_view_collapse_row(GTK_TREE_VIEW(view), path);

        else // if the branch is already collapsed
            if (gtk_tree_path_get_depth(path) > 1 && gtk_tree_path_up(path))
            {
                // if this is not a toplevel item then move to parent
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
            }
            else if (gtk_tree_path_get_depth(path) == 1)
            {
                // if this is a toplevel item and a mountable device, unmount it
                if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path))
                    gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter,
                                        TREEMODEL_COLUMN_DEVICE, &device, -1);

                if (device != NULL)
                    if (th_device_is_mounted(device) && th_device_can_unmount(device))
                    {
                        // mark this path for selection after unmounting
                        view->select_path = gtk_tree_path_copy(path);
                        g_object_set(G_OBJECT(view->launcher), "selected-device", device, NULL);
                        g_object_set(G_OBJECT(view->launcher), "selected-files", NULL, "current-directory", NULL, NULL);
                        launcher_action_unmount(view->launcher);
                        g_object_unref(G_OBJECT(device));
                    }
            }
        _treeview_open_selection(view);

        stopPropagation = TRUE;
        break;

    case GDK_KEY_Right:
    case GDK_KEY_KP_Right:
        // if this is a toplevel item and a mountable device, mount it
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path))
            gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter,
                                TREEMODEL_COLUMN_DEVICE, &device, -1);
        if (device != NULL && th_device_is_mounted(device) == FALSE)
        {
            g_object_set(G_OBJECT(view->launcher), "selected-device", device, NULL);
            g_object_set(G_OBJECT(view->launcher), "selected-files", NULL, "current-directory", NULL, NULL);
            launcher_action_mount(view->launcher);
        }
        else
        {
            // if branch is not expanded then expand it
            if (!gtk_tree_view_row_expanded(GTK_TREE_VIEW(view), path))
                gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, FALSE);
            else // if branch is already expanded then move to first child
            {
                gtk_tree_path_down(path);
                gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
                _treeview_action_open(view);
            }
        }
        stopPropagation = TRUE;
        break;

    case GDK_KEY_space:
    case GDK_KEY_Return:
    case GDK_KEY_KP_Enter:
        _treeview_open_selection(view);
        stopPropagation = TRUE;
        break;
    }

    gtk_tree_path_free(path);

    if (stopPropagation)
        gtk_widget_grab_focus(widget);

    return stopPropagation;
}

// Popup Menu -----------------------------------------------------------------

static gboolean treeview_popup_menu(GtkWidget *widget)
{
    GtkTreeSelection *selection;
    TreeView   *view = TREEVIEW(widget);
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    // determine the selected row
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

    if (gtk_tree_selection_get_selected(selection, &model, &iter))
    {
        // popup the context menu
        _treeview_context_menu(view, model, &iter);

        return TRUE;
    }
    else if (GTK_WIDGET_CLASS(treeview_parent_class)->popup_menu != NULL)
    {
        // call the parent's "popup-menu" handler
        return GTK_WIDGET_CLASS(treeview_parent_class)->popup_menu(widget);
    }

    return FALSE;
}

static void _treeview_context_menu(TreeView     *view,
                                   GtkTreeModel *model,
                                   GtkTreeIter  *iter)
{
    // check if we're connected to the clipboard manager
    if (G_UNLIKELY(view->clipboard == NULL))
        return;

    // determine the file and device for the given iter
    ThunarFile *file;
    ThunarDevice *device;
    gtk_tree_model_get(model, iter,
                       TREEMODEL_COLUMN_FILE, &file,
                       TREEMODEL_COLUMN_DEVICE, &device,
                       -1);

    // create the popup menu
    AppMenu *context_menu = g_object_new(
                                TYPE_APPMENU,
                                "menu-type", MENU_TYPE_CONTEXT_TREE_VIEW,
                                "launcher", view->launcher,
                                "force-section-open", TRUE,
                                NULL);

    g_object_set(G_OBJECT(view->launcher),
                 "selected-device", device,
                 NULL);

    gboolean file_is_available = (device == NULL
                                  || th_device_is_mounted(device));

    if (file_is_available)
    {
        // set selected files and current directory

        GList *files = g_list_append(NULL, file);

        g_object_set(G_OBJECT(view->launcher),
                     "selected-files", files,
                     "current-directory", view->current_directory,
                     NULL);

        g_list_free(files);

        // build popup menu

        if (e_file_is_trash(th_file_get_file(file))
            || e_file_is_computer(th_file_get_file(file))
            || e_file_is_network(th_file_get_file(file)))
        {
            launcher_append_menu_item(view->launcher, GTK_MENU_SHELL(context_menu), LAUNCHER_ACTION_OPEN, TRUE);
            launcher_append_menu_item(view->launcher, GTK_MENU_SHELL(context_menu), LAUNCHER_ACTION_OPEN_IN_WINDOW, TRUE);

            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(context_menu));

            appmenu_add_sections(context_menu, MENU_SECTION_EMPTY_TRASH);
        }
        else
        {
            appmenu_add_sections(context_menu, MENU_SECTION_OPEN);

            appmenu_add_sections(context_menu, MENU_SECTION_CREATE_NEW_FILES
                                                   | MENU_SECTION_CUT
                                                   | MENU_SECTION_COPY_PASTE
                                                   | MENU_SECTION_TRASH_DELETE
                                                   | MENU_SECTION_RESTORE
                                                   | MENU_SECTION_RENAME
                                                   | MENU_SECTION_TERMINAL);
        }

        appmenu_add_sections(context_menu, MENU_SECTION_MOUNTABLE);
        appmenu_add_sections(context_menu, MENU_SECTION_PROPERTIES);
    }
    else
    {
        // no files selected.

        g_object_set(G_OBJECT(view->launcher),
                     "selected-files", NULL,
                     "current-directory", NULL,
                     NULL);

        launcher_append_menu_item(view->launcher,
                                  GTK_MENU_SHELL(context_menu),
                                  LAUNCHER_ACTION_OPEN,
                                  TRUE);

        launcher_append_menu_item(view->launcher,
                                  GTK_MENU_SHELL(context_menu),
                                  LAUNCHER_ACTION_OPEN_IN_WINDOW,
                                  TRUE);

        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(context_menu));

        appmenu_add_sections(context_menu, MENU_SECTION_MOUNTABLE);

        if (th_device_is_mounted(device))
            appmenu_add_sections(context_menu, MENU_SECTION_PROPERTIES);
    }

    appmenu_hide_accel_labels(context_menu);

    gtk_widget_show_all(GTK_WIDGET(context_menu));

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    window_redirect_menu_tooltips_to_statusbar(THUNAR_WINDOW(window),
                                               GTK_MENU(context_menu));
    etk_menu_run(GTK_MENU(context_menu));

    if (G_UNLIKELY(device != NULL))
        g_object_unref(G_OBJECT(device));

    if (G_LIKELY(file != NULL))
        g_object_unref(G_OBJECT(file));
}

// Open -----------------------------------------------------------------------

static void _treeview_action_open(TreeView *view)
{
    ThunarDevice *device = _treeview_get_selected_device(view);
    ThunarFile *file = _treeview_get_selected_file(view);

    if (device != NULL)
    {
        if (th_device_is_mounted(device))
            _treeview_open_selection(view);
        else
        {
            g_object_set(G_OBJECT(view->launcher),
                         "selected-device", device,
                         NULL);

            g_object_set(G_OBJECT(view->launcher),
                         "selected-files", NULL,
                         "current-directory", NULL,
                         NULL);

            launcher_action_mount(view->launcher);
        }
    }
    else if (file != NULL)
    {
        _treeview_open_selection(view);
    }

    if (device != NULL)
        g_object_unref(device);

    if (file != NULL)
        g_object_unref(file);
}

static void _treeview_open_selection(TreeView *view)
{
    e_return_if_fail(IS_TREEVIEW(view));

    // determine the selected file
    ThunarFile *file = _treeview_get_selected_file(view);

    if (G_LIKELY(file != NULL))
    {
        // open that folder in the main view
        navigator_change_directory(THUNAR_NAVIGATOR(view), file);
        g_object_unref(file);
    }
}



// ----------------------------------------------------------------------------

static void treeview_row_activated(GtkTreeView       *tree_view,
                                   GtkTreePath       *path,
                                   GtkTreeViewColumn *column)
{
    // call the parent's "row-activated" handler
    if (GTK_TREE_VIEW_CLASS(treeview_parent_class)->row_activated != NULL)
        GTK_TREE_VIEW_CLASS(treeview_parent_class)->row_activated(
                    tree_view,
                    path,
                    column);

    // toggle the expanded state of the activated row...
    if (gtk_tree_view_row_expanded(tree_view, path))
    {
        gtk_tree_view_collapse_row(tree_view, path);
    }
    else
    {
        // expand the row, but open it if mounted
        if (gtk_tree_view_expand_row(tree_view, path, FALSE))
        {
            // ...open the selected folder
            _treeview_action_open(TREEVIEW(tree_view));
        }
    }
}



static gboolean treeview_test_expand_row(GtkTreeView *tree_view,
                                         GtkTreeIter *iter,
                                         GtkTreePath *path)
{
    (void) path;

    TreeView *view = TREEVIEW(tree_view);

    // determine the device for the iterator

    ThunarDevice *device;

    gtk_tree_model_get(GTK_TREE_MODEL(view->model),
                       iter,
                       TREEMODEL_COLUMN_DEVICE, &device,
                       -1);

    gboolean expandable = TRUE;

    // check if we have a device
    if (G_UNLIKELY(device != NULL))
    {
        // check if we need to mount the device first
        if (!th_device_is_mounted(device))
        {
            // we need to mount the device before we can expand the row
            expandable = FALSE;

            g_object_set(G_OBJECT(view->launcher),
                         "selected-device", device,
                         NULL);

            g_object_set(G_OBJECT(view->launcher),
                         "selected-files", NULL,
                         "current-directory", NULL,
                         NULL);

            // The closure will expand the row after the mount operation finished
            launcher_action_mount(view->launcher);
        }

        // release the device
        g_object_unref(G_OBJECT(device));
    }

    // cancel the cursor idle source if not expandable
    if (!expandable && view->cursor_idle_id != 0)
        g_source_remove(view->cursor_idle_id);

    return !expandable;
}

static void treeview_row_collapsed(GtkTreeView *tree_view,
                                           GtkTreeIter *iter,
                                           GtkTreePath *path)
{
    (void) iter;
    (void) path;

    // schedule a cleanup of the tree model
    treemodel_cleanup(TREEVIEW(tree_view)->model);
}




// Selected Actions -----------------------------------------------------------

gboolean treeview_delete_selected_files(TreeView *view)
{
    e_return_val_if_fail(IS_TREEVIEW(view), FALSE);

    if (!e_vfs_is_uri_scheme_supported("trash"))
        return TRUE;

    /* Check if there is a user defined accelerator for the delete action,
     * if there is, skip events from the hard-coded keys which are set in
     * the class of the standard view. See bug #4173. */

    GtkAccelKey key;

    if (gtk_accel_map_lookup_entry(
                        "<Actions>/StandardView/move-to-trash",
                        &key)
        &&(key.accel_key != 0 || key.accel_mods != 0))
    {
        return FALSE;
    }

    // check if we should permanently delete the files(user holds shift or no gvfs available)

    GdkModifierType state;

    if (gtk_get_current_event_state(&state) && (state & GDK_SHIFT_MASK) != 0)
    {
        _treeview_action_unlink_selected_folder(view, TRUE);
    }
    else
    {
        _treeview_action_unlink_selected_folder(view, FALSE);
    }

    // ...and we're done

    return TRUE;
}

static void _treeview_action_unlink_selected_folder(TreeView *view,
                                                    gboolean permanently)
{
    e_return_if_fail(IS_TREEVIEW(view));

    // determine the selected file

    ThunarFile *file = _treeview_get_selected_file(view);

    if (G_UNLIKELY(file == NULL))
        return;

    if (th_file_can_be_trashed(file))
    {
        // fake a file list

        GList file_list;
        file_list.data = file;
        file_list.next = NULL;
        file_list.prev = NULL;

        // delete the file
        Application *application = application_get();

        application_unlink_files(application,
                                        GTK_WIDGET(view),
                                        &file_list,
                                        permanently);

        g_object_unref(G_OBJECT(application));
    }

    // release the file
    g_object_unref(G_OBJECT(file));
}

void treeview_rename_selected(TreeView *view)
{
    ThunarFile *file = _treeview_get_selected_file(view);
    if (!file)
        return;

    g_object_set(G_OBJECT(view->launcher),
                 "selected-device", NULL,
                 NULL);

    GList *files = g_list_append(NULL, file);

    g_object_set(G_OBJECT(view->launcher),
                 "selected-files", files,
                 "current-directory", view->current_directory,
                 NULL);

    g_list_free(files);
    g_object_unref(file);

    launcher_action_rename(view->launcher);

    return;
}






// Selections -----------------------------------------------------------------

static ThunarFile* _treeview_get_selected_file(TreeView *view)
{
    GtkTreeSelection *selection;
    GtkTreeModel     *model;
    GtkTreeIter       iter;
    ThunarFile       *file = NULL;

    // determine file for the selected row
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));

    /* avoid dealing with invalid selections(may occur when the mount_finish()
     * handler is called and the tree view has been hidden already) */
    if (!GTK_IS_TREE_SELECTION(selection))
        return NULL;

    if (gtk_tree_selection_get_selected(selection, &model, &iter))
        gtk_tree_model_get(model, &iter, TREEMODEL_COLUMN_FILE, &file, -1);

    return file;
}

static ThunarDevice* _treeview_get_selected_device(TreeView *view)
{
    GtkTreeSelection *selection;
    ThunarDevice     *device = NULL;
    GtkTreeModel     *model;
    GtkTreeIter       iter;

    // determine file for the selected row
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(view));
    if (gtk_tree_selection_get_selected(selection, &model, &iter))
        gtk_tree_model_get(model, &iter, TREEMODEL_COLUMN_DEVICE, &device, -1);

    return device;
}

static void _treeview_select_files(TreeView *view,
                                          GList          *files_to_selected)
{
    ThunarFile *file = NULL;

    e_return_if_fail(IS_TREEVIEW(view));

    // check if we have exactly one new path
    if (G_UNLIKELY(files_to_selected == NULL || files_to_selected->next != NULL))
        return;

    // determine the file for the first path
    file = th_file_get(G_FILE(files_to_selected->data), NULL);
    if (G_LIKELY(file != NULL))
    {
        if (G_LIKELY(th_file_is_directory(file)))
            navigator_change_directory(THUNAR_NAVIGATOR(view), file);

        g_object_unref(file);
    }
}

static gboolean _treeview_visible_func(TreeModel *model,
                                              ThunarFile      *file,
                                              gpointer         user_data)
{
    TreeView *view;
    gboolean        visible = TRUE;

    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    e_return_val_if_fail(THUNAR_IS_TREE_MODEL(model), FALSE);
    e_return_val_if_fail(IS_TREEVIEW(user_data), FALSE);

    // if show_hidden is TRUE, nothing is filtered
    view = TREEVIEW(user_data);
    if (G_LIKELY(!view->show_hidden))
    {
        // we display all non-hidden file and hidden files that are ancestors of the current directory
        visible = !th_file_is_hidden(file) ||(view->current_directory == file)
                  ||(view->current_directory != NULL && th_file_is_ancestor(view->current_directory, file));
    }

    return visible;
}

static gboolean _treeview_selection_func(GtkTreeSelection *selection,
                                                GtkTreeModel     *model,
                                                GtkTreePath      *path,
                                                gboolean          path_currently_selected,
                                                gpointer          user_data)
{
    (void) selection;
    (void) user_data;

    GtkTreeIter   iter;
    ThunarFile   *file;
    gboolean      result = FALSE;
    ThunarDevice *device;

    // every row may be unselected at any time
    if (path_currently_selected)
        return TRUE;

    // determine the iterator for the path
    if (gtk_tree_model_get_iter(model, &iter, path))
    {
        // determine the file for the iterator
        gtk_tree_model_get(model, &iter, TREEMODEL_COLUMN_FILE, &file, -1);
        if (G_LIKELY(file != NULL))
        {
            // rows with files can be selected
            result = TRUE;

            // release file
            g_object_unref(file);
        }
        else
        {
            // but maybe the row has a device
            gtk_tree_model_get(model, &iter, TREEMODEL_COLUMN_DEVICE, &device, -1);
            if (G_LIKELY(device != NULL))
            {
                // rows with devices can also be selected
                result = TRUE;

                // release device
                g_object_unref(device);
            }
        }
    }

    return result;
}

static gboolean _treeview_cursor_idle(gpointer user_data)
{
    TreeView *view = TREEVIEW(user_data);
    GtkTreePath    *path;
    GtkTreeIter     iter;
    ThunarFile     *file;
    ThunarFolder   *folder;
    GFileInfo      *file_info;
    GtkTreeIter     child_iter;
    ThunarFile     *file_in_tree;
    gboolean        done = FALSE;
    GList          *lp;
    GList          *path_as_list = NULL;

    UTIL_THREADS_ENTER

    // for easier navigation, we sometimes want to force/keep selection of a certain path
    if (view->select_path != NULL)
    {
        gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), view->select_path, NULL, FALSE);
        gtk_tree_path_free(view->select_path);
        view->select_path = NULL;
        return TRUE;
    }

    // verify that we still have a current directory
    if (G_UNLIKELY(view->current_directory == NULL))
        return TRUE;

    // get the preferred toplevel path for the current directory
    path = _treeview_get_preferred_toplevel_path(view, view->current_directory);

    // fallback to a newly created root node
    if (path == NULL)
        path = gtk_tree_path_new_first();

    gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path);
    gtk_tree_path_free(path);

    // collect all ThunarFiles in the path of current_directory in a List. root is on the very left side
    for(file = view->current_directory; file != NULL; file = th_file_get_parent(file, NULL))
        path_as_list = g_list_prepend(path_as_list, file);

    // 1. skip files on path_as_list till we found the beginning of the tree(which e.g. may start at $HOME
    gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter, TREEMODEL_COLUMN_FILE, &file_in_tree, -1);
    for(lp = path_as_list; lp != NULL; lp = lp->next)
    {
        if (THUNAR_FILE(lp->data) == file_in_tree)
            break;
    }
    if (file_in_tree)
        g_object_unref(file_in_tree);

    // 2. loop on the remaining path_as_list
    for (; lp != NULL; lp = lp->next)
    {
        file = THUNAR_FILE(lp->data);

        // 3. Check if the contents of the corresponding folder is still being loaded
        folder = th_folder_get_for_file(file);
        if (folder != NULL && th_folder_get_loading(folder))
        {
            g_object_unref(folder);
            break;
        }
        if (folder)
            g_object_unref(folder);

        // 4. Loop on all items of current tree-level to see if any folder matches the path we search
        while (TRUE)
        {
            gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter, TREEMODEL_COLUMN_FILE, &file_in_tree, -1);
            if (file == file_in_tree)
            {
                g_object_unref(file_in_tree);
                break;
            }
            if (file_in_tree)
                g_object_unref(file_in_tree);

            if (!gtk_tree_model_iter_next(GTK_TREE_MODEL(view->model), &iter))
                break;
        }

        // 5. Did we already find the full path ?
        if (lp->next == NULL)
        {
            path = gtk_tree_model_get_path(GTK_TREE_MODEL(view->model), &iter);
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(view), path, NULL, FALSE);
            gtk_tree_path_free(path);
            done = TRUE;
            break;
        }

        // 6. Get all children of the current tree iter
        // Try to create missing children on the tree if there are none
        if (!gtk_tree_model_iter_children(GTK_TREE_MODEL(view->model), &child_iter, &iter))
        {
            if (file == NULL) // e.g root has no parent .. skip it
                continue;

            file_info = th_file_get_info(file);
            if (file_info != NULL)
            {
                // E.g. folders for which we do not have read permission dont have any child in the tree
                // Make sure that missing read permissions are the problem
                if (!g_file_info_get_attribute_boolean(file_info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
                {
                    // We KNOW that there is a File. Lets just create the required tree-node
                    treemodel_add_child(view->model, iter.user_data,  THUNAR_FILE(lp->next->data));
                }
            }
            break; // we dont have a valid child_iter by now, so we cannot continue.                        
            // Since done is FALSE, the next iteration on thunar_tree_view_cursor_idle will go deeper
        }

        // expand path up to the current tree level
        path = gtk_tree_model_get_path(GTK_TREE_MODEL(view->model), &iter);
        gtk_tree_view_expand_to_path(GTK_TREE_VIEW(view), path);
        gtk_tree_path_free(path);

        iter = child_iter; // next tree level
    }

    // tidy up
    for (lp = path_as_list; lp != NULL; lp = lp->next)
    {
        file = THUNAR_FILE(lp->data);
        if (file != NULL && file != view->current_directory)
            g_object_unref(file);
    }
    g_list_free(path_as_list);

    UTIL_THREADS_LEAVE

    return !done;
}

static void _treeview_cursor_idle_destroy(gpointer user_data)
{
    TREEVIEW(user_data)->cursor_idle_id = 0;
}



static GtkTreePath* _treeview_get_preferred_toplevel_path(TreeView   *view,
                                                          ThunarFile *file)
{
    GtkTreeModel *model = GTK_TREE_MODEL(view->model);
    GtkTreePath  *path = NULL;
    GtkTreeIter   iter;
    ThunarFile   *toplevel_file;
    GFile        *home;
    GFile        *root;
    GFile        *best_match;

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    // get active toplevel path and check if we can use it
    gtk_tree_view_get_cursor(GTK_TREE_VIEW(view), &path, NULL);

    if (path != NULL)
    {
        if (gtk_tree_path_get_depth(path) != 1)
            while(gtk_tree_path_get_depth(path) > 1)
                gtk_tree_path_up(path);

        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path))
        {
            // lookup file for the toplevel item
            gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter, TREEMODEL_COLUMN_FILE, &toplevel_file, -1);
            if (toplevel_file)
            {
                // check if the toplevel file is an ancestor
                if (th_file_is_ancestor(file, toplevel_file))
                {
                    g_object_unref(toplevel_file);
                    return path;
                }

                g_object_unref(toplevel_file);
            }
        }

        gtk_tree_path_free(path);
        path = NULL;
    }

    // check whether the root node is available
    if (!gtk_tree_model_get_iter_first(model, &iter))
        return NULL;

    // get GFiles for special toplevel items
    home = e_file_new_for_home();
    root = e_file_new_for_root();

    // we prefer certain toplevel items to others
    if (th_file_is_gfile_ancestor(file, home))
        best_match = home;
    else if (th_file_is_gfile_ancestor(file, root))
        best_match = root;
    else
        best_match = NULL;

    // loop over all top-level nodes to find the best-matching top-level item
    do
    {
        gtk_tree_model_get(model, &iter, TREEMODEL_COLUMN_FILE, &toplevel_file, -1);
        // this toplevel item has no file, so continue with the next
        if (toplevel_file == NULL)
            continue;

        // if the file matches the toplevel item exactly, we are done
        if (g_file_equal(th_file_get_file(file),
                          th_file_get_file(toplevel_file)))
        {
            gtk_tree_path_free(path);
            path = gtk_tree_model_get_path(model, &iter);
            g_object_unref(toplevel_file);
            break;
        }

        if (th_file_is_ancestor(file, toplevel_file))
        {
            /* the toplevel item could be a mounted device or network
             * and we prefer this to everything else */
            if (!g_file_equal(th_file_get_file(toplevel_file), home) &&
                    !g_file_equal(th_file_get_file(toplevel_file), root))
            {
                gtk_tree_path_free(path);
                g_object_unref(toplevel_file);
                path = gtk_tree_model_get_path(model, &iter);
                break;
            }

            // continue if the toplevel item is already the best match
            if (best_match != NULL &&
                    g_file_equal(th_file_get_file(toplevel_file), best_match))
            {
                gtk_tree_path_free(path);
                g_object_unref(toplevel_file);
                path = gtk_tree_model_get_path(model, &iter);
                continue;
            }

            // remember this ancestor if we do not already have one
            if (path == NULL)
            {
                gtk_tree_path_free(path);
                path = gtk_tree_model_get_path(model, &iter);
            }
        }
        g_object_unref(toplevel_file);
    }
    while (gtk_tree_model_iter_next(model, &iter));

    // cleanup
    g_object_unref(root);
    g_object_unref(home);

    return path;
}



// DnD ------------------------------------------------------------------------

static void treeview_drag_data_received(GtkWidget        *widget,
                                        GdkDragContext   *context,
                                        gint             x,
                                        gint             y,
                                        GtkSelectionData *selection_data,
                                        guint            info,
                                        guint            timestamp)
{
    TreeView *view = TREEVIEW(widget);
    GdkDragAction   actions;
    GdkDragAction   action;
    ThunarFile     *file;
    gboolean        succeed = FALSE;

    // check if don't already know the drop data
    if (G_LIKELY(!view->drop_data_ready))
    {
        // extract the URI list from the selection data(if valid)
        if (info == TARGET_TEXT_URI_LIST && gtk_selection_data_get_format(selection_data) == 8 && gtk_selection_data_get_length(selection_data) > 0)
            view->drop_file_list = e_file_list_new_from_string((const gchar *) gtk_selection_data_get_data(selection_data));

        // reset the state
        view->drop_data_ready = TRUE;
    }

    // check if the data was droppped
    if (G_UNLIKELY(view->drop_occurred))
    {
        // reset the state
        view->drop_occurred = FALSE;

        // determine the drop position
        actions = _treeview_get_dest_actions(view, context, x, y, timestamp, &file);

        if (G_LIKELY((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
        {
            // ask the user what to do with the drop data
            action = (gdk_drag_context_get_selected_action(context) == GDK_ACTION_ASK)
                     ? dnd_ask(GTK_WIDGET(view),
                                      file,
                                      view->drop_file_list,
                                      actions)
                     : gdk_drag_context_get_selected_action(context);

            // perform the requested action
            if (G_LIKELY(action != 0))
                succeed = dnd_perform(GTK_WIDGET(view), file, view->drop_file_list, action, NULL);
        }

        // release the file reference
        if (G_LIKELY(file != NULL))
            g_object_unref(G_OBJECT(file));

        // tell the peer that we handled the drop
        gtk_drag_finish(context, succeed, FALSE, timestamp);

        // disable the highlighting and release the drag data
        treeview_drag_leave(GTK_WIDGET(view), context, timestamp);
    }
}

static gboolean treeview_drag_drop(GtkWidget      *widget,
                                   GdkDragContext *context,
                                   gint           x,
                                   gint           y,
                                   guint          timestamp)
{
    (void) x;
    (void) y;

    TreeView *view = TREEVIEW(widget);
    GdkAtom         target;

    // determine the drop target
    target = gtk_drag_dest_find_target(widget, context, NULL);
    if (G_LIKELY(target == gdk_atom_intern_static_string("text/uri-list")))
    {
        /* set state so the drag-data-received handler
         * knows that this is really a drop this time.
         */
        view->drop_occurred = TRUE;

        // request the drag data from the source.
        gtk_drag_get_data(widget, context, target, timestamp);
    }
    else
    {
        // we cannot handle the drop
        return FALSE;
    }

    return TRUE;
}

static gboolean treeview_drag_motion(GtkWidget      *widget,
                                     GdkDragContext *context,
                                     gint           x,
                                     gint           y,
                                     guint          timestamp)
{
    TreeView *view = TREEVIEW(widget);
    GdkAtom         target;

    // determine the drop target
    target = gtk_drag_dest_find_target(widget, context, NULL);
    if (G_UNLIKELY(target != gdk_atom_intern_static_string("text/uri-list")))
    {
        // reset the "drop-file" of the icon renderer
        g_object_set(G_OBJECT(view->icon_renderer), "drop-file", NULL, NULL);

        // we cannot handle the drop
        return FALSE;
    }

    // request the drop data on-demand(if we don't have it already)
    if (G_UNLIKELY(!view->drop_data_ready))
    {
        // reset the "drop-file" of the icon renderer
        g_object_set(G_OBJECT(view->icon_renderer), "drop-file", NULL, NULL);

        // request the drag data from the source
        gtk_drag_get_data(widget, context, target, timestamp);

        // tell Gdk that we don't know whether we can drop
        gdk_drag_status(context, 0, timestamp);
    }
    else
    {
        // check if we can drop here
        _treeview_get_dest_actions(view, context, x, y, timestamp, NULL);
    }

    // start the drag autoscroll timer if not already running
    if (G_UNLIKELY(view->drag_scroll_timer_id == 0))
    {
        // schedule the drag autoscroll timer
        view->drag_scroll_timer_id = g_timeout_add_full(G_PRIORITY_LOW, 50, _treeview_drag_scroll_timer,
                                     view, _treeview_drag_scroll_timer_destroy);
    }

    return TRUE;
}

static void treeview_drag_leave(GtkWidget      *widget,
                                GdkDragContext *context,
                                guint          timestamp)
{
    TreeView *view = TREEVIEW(widget);

    // cancel any running autoscroll timer
    if (G_LIKELY(view->drag_scroll_timer_id != 0))
        g_source_remove(view->drag_scroll_timer_id);

    // reset the "drop-file" of the icon renderer
    g_object_set(G_OBJECT(view->icon_renderer), "drop-file", NULL, NULL);

    // reset the "drop data ready" status and free the URI list
    if (G_LIKELY(view->drop_data_ready))
    {
        e_list_free(view->drop_file_list);
        view->drop_data_ready = FALSE;
        view->drop_file_list = NULL;
    }

    // call the parent's handler
    GTK_WIDGET_CLASS(treeview_parent_class)->drag_leave(widget, context, timestamp);
}

static GdkDragAction _treeview_get_dest_actions(TreeView       *view,
                                                GdkDragContext *context,
                                                gint           x,
                                                gint           y,
                                                guint          timestamp,
                                                ThunarFile     **file_return)
{
    GdkDragAction actions = 0;
    GdkDragAction action = 0;
    GtkTreePath  *path = NULL;
    GtkTreeIter   iter;
    ThunarFile   *file = NULL;

    // cancel any previously active expand timer
    if (G_LIKELY(view->expand_timer_id != 0))
        g_source_remove(view->expand_timer_id);

    // determine the path for x/y
    if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(view), x, y, &path, NULL, NULL, NULL))
    {
        // determine the iter for the given path
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, path))
        {
            // determine the file for the given path
            gtk_tree_model_get(GTK_TREE_MODEL(view->model), &iter, TREEMODEL_COLUMN_FILE, &file, -1);

            if (G_LIKELY(file != NULL))
            {
                // check if the file accepts the drop
                actions = th_file_accepts_drop(file, view->drop_file_list, context, &action);

                if (G_UNLIKELY(actions == 0))
                {
                    // reset file
                    g_object_unref(G_OBJECT(file));
                    file = NULL;
                }
            }
        }
    }

    // setup the new drag dest row
    gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(view), path, GTK_TREE_VIEW_DROP_INTO_OR_BEFORE);

    // check if we have drag dest row
    if (G_LIKELY(path != NULL))
    {
        // schedule a new expand timer to expand the drag dest row
        view->expand_timer_id = g_timeout_add_full(G_PRIORITY_LOW, TREEVIEW_EXPAND_TIMEOUT,
                                _treeview_expand_timer, view,
                                _treeview_expand_timer_destroy);
    }

    // setup the new "drop-file"
    g_object_set(G_OBJECT(view->icon_renderer), "drop-file", file, NULL);

    // tell Gdk whether we can drop here
    gdk_drag_status(context, action, timestamp);

    // return the file if requested
    if (G_LIKELY(file_return != NULL))
    {
        *file_return = file;
        file = NULL;
    }
    else if (G_UNLIKELY(file != NULL))
    {
        // release the file
        g_object_unref(G_OBJECT(file));
    }

    // clean up
    if (G_LIKELY(path != NULL))
        gtk_tree_path_free(path);

    return actions;
}

static gboolean _treeview_drag_scroll_timer(gpointer user_data)
{
    TreeView *view = TREEVIEW(user_data);
    GtkAdjustment  *vadjustment;
    GtkTreePath    *start_path;
    GtkTreePath    *end_path;
    GtkTreePath    *path;
    GdkWindow      *window;
    GdkSeat        *seat;
    GdkDevice      *pointer;
    gfloat          value;
    gint            offset;
    gint            y, h;

    UTIL_THREADS_ENTER

    // verify that we are realized
    if (gtk_widget_get_realized(GTK_WIDGET(view)))
    {
        // determine pointer location and window geometry
        window = gtk_widget_get_window(GTK_WIDGET(view));
        seat = gdk_display_get_default_seat(gdk_display_get_default());
        pointer = gdk_seat_get_pointer(seat);

        gdk_window_get_device_position(window, pointer, NULL, &y, NULL);
        gdk_window_get_geometry(window, NULL, NULL, NULL, &h);

        // check if we are near the edge
        offset = y -(2 * 20);
        if (G_UNLIKELY(offset > 0))
            offset = MAX(y -(h - 2 * 20), 0);

        // change the vertical adjustment appropriately
        if (G_UNLIKELY(offset != 0))
        {
            // determine the vertical adjustment
            vadjustment = gtk_scrollable_get_vadjustment(GTK_SCROLLABLE(view));

            // determine the new value
            value = CLAMP(gtk_adjustment_get_value(vadjustment) + 2 * offset, gtk_adjustment_get_lower(vadjustment), gtk_adjustment_get_upper(vadjustment) - gtk_adjustment_get_page_size(vadjustment));

            // check if we have a new value
            if (G_UNLIKELY(gtk_adjustment_get_value(vadjustment) != value))
            {
                // apply the new value
                gtk_adjustment_set_value(vadjustment, value);

                /* drop any pending expand timer source, as its confusing
                 * if a path is expanded while scrolling through the view.
                 * reschedule it if the drag dest path is still visible.
                 */
                if (G_UNLIKELY(view->expand_timer_id != 0))
                {
                    // drop the current expand timer source
                    g_source_remove(view->expand_timer_id);

                    // determine the visible range of the tree view
                    if (gtk_tree_view_get_visible_range(GTK_TREE_VIEW(view), &start_path, &end_path))
                    {
                        // determine the drag dest row
                        gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(view), &path, NULL);
                        if (G_LIKELY(path != NULL))
                        {
                            // check if the drag dest row is currently visible
                            if (gtk_tree_path_compare(path, start_path) >= 0 && gtk_tree_path_compare(path, end_path) <= 0)
                            {
                                // schedule a new expand timer to expand the drag dest row
                                view->expand_timer_id = g_timeout_add_full(G_PRIORITY_LOW, TREEVIEW_EXPAND_TIMEOUT,
                                                        _treeview_expand_timer, view,
                                                        _treeview_expand_timer_destroy);
                            }

                            // release the drag dest row
                            gtk_tree_path_free(path);
                        }

                        // release the start/end paths
                        gtk_tree_path_free(start_path);
                        gtk_tree_path_free(end_path);
                    }
                }
            }
        }
    }

    UTIL_THREADS_LEAVE

    return TRUE;
}

static void _treeview_drag_scroll_timer_destroy(gpointer user_data)
{
    TREEVIEW(user_data)->drag_scroll_timer_id = 0;
}

static gboolean _treeview_expand_timer(gpointer user_data)
{
    TreeView *view = TREEVIEW(user_data);
    GtkTreePath    *path;

    UTIL_THREADS_ENTER

    // cancel the drag autoscroll timer when expanding a row
    if (G_UNLIKELY(view->drag_scroll_timer_id != 0))
        g_source_remove(view->drag_scroll_timer_id);

    // determine the drag dest row
    gtk_tree_view_get_drag_dest_row(GTK_TREE_VIEW(view), &path, NULL);
    if (G_LIKELY(path != NULL))
    {
        // expand the drag dest row
        gtk_tree_view_expand_row(GTK_TREE_VIEW(view), path, FALSE);
        gtk_tree_path_free(path);
    }

    UTIL_THREADS_LEAVE

    return FALSE;
}

static void _treeview_expand_timer_destroy(gpointer user_data)
{
    TREEVIEW(user_data)->expand_timer_id = 0;
}


