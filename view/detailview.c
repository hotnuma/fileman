/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
#include <detailview.h>

#include <appwindow.h>
#include <standardview.h>
#include <exotreeview.h>
#include <columnmodel.h>
#include <launcher.h>

// DetailView -----------------------------------------------------------------

static void detailview_finalize(GObject *object);
static void detailview_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);
static void detailview_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);
static gboolean _detailview_get_fixed_columns(DetailView *details_view);
static void _detailview_set_fixed_columns(DetailView *details_view,
                                          gboolean fixed_columns);

static AtkObject* detailview_get_accessible(GtkWidget *widget);

static GList* detailview_get_selected_items(StandardView *standard_view);
static void detailview_select_all(StandardView *standard_view);
static void detailview_unselect_all(StandardView *standard_view);
static void detailview_selection_invert(StandardView *standard_view);
static void _detailview_selection_invert_foreach(GtkTreeModel *model,
                                                 GtkTreePath  *path,
                                                 GtkTreeIter  *iter,
                                                 gpointer      data);
static void detailview_select_path(StandardView *standard_view,
                                   GtkTreePath *path);
static void detailview_set_cursor(StandardView *standard_view,
                                  GtkTreePath *path,
                                  gboolean start_editing);
static void detailview_scroll_to_path(StandardView *standard_view,
                                      GtkTreePath *path,
                                      gboolean use_align,
                                      gfloat row_align,
                                      gfloat col_align);
static GtkTreePath* detailview_get_path_at_pos(StandardView *standard_view,
                                               gint x,
                                               gint y);
static gboolean detailview_get_visible_range(StandardView *standard_view,
                                             GtkTreePath **start_path,
                                             GtkTreePath **end_path);
static void detailview_highlight_path(StandardView *standard_view,
                                      GtkTreePath *path);
static void detailview_append_menu_items(StandardView *standard_view,
                                         GtkMenu *menu,
                                         GtkAccelGroup *accel_group);
static void detailview_connect_accelerators(StandardView *standard_view,
                                            GtkAccelGroup *accel_group);
static void detailview_disconnect_accelerators(StandardView *standard_view,
                                               GtkAccelGroup *accel_group);

// Events ---------------------------------------------------------------------

static void _detailview_zoom_level_changed(DetailView *details_view);
static gboolean _detailview_zoom_level_changed_reload_fixed_columns(gpointer data);
static void _detailview_notify_model(GtkTreeView *tree_view,
                                     GParamSpec *pspec,
                                     DetailView *details_view);
static gboolean _detailview_button_press_event(GtkTreeView *tree_view,
                                               GdkEventButton *event,
                                               DetailView *details_view);
static gboolean _detailview_key_press_event(GtkTreeView *tree_view,
                                            GdkEventKey *event,
                                            DetailView *details_view);
static void _detailview_row_activated(GtkTreeView *tree_view,
                                      GtkTreePath *path,
                                      GtkTreeViewColumn *column,
                                      DetailView *details_view);
static gboolean _detailview_select_cursor_row(GtkTreeView *tree_view,
                                              gboolean editing,
                                              DetailView *details_view);
static void _detailview_columns_changed(ColumnModel *column_model,
                                        DetailView *details_view);
static void _detailview_row_changed(GtkTreeView *tree_view,
                                    GtkTreePath *path,
                                    GtkTreeViewColumn *column,
                                    DetailView *details_view);
static void _detailview_notify_width(GtkTreeViewColumn *tree_view_column,
                                     GParamSpec *pspec,
                                     DetailView *details_view);

// Column Editor Dialog -------------------------------------------------------

static void _detailview_show_column_editor(gpointer parent);

// DetailView -----------------------------------------------------------------

static XfceGtkActionEntry _detailview_actions[] =
{
    {DETAILVIEW_ACTION_CONFIGURE_COLUMNS,
     "<Actions>/StandardView/configure-columns",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Configure _Columns..."),
     N_("Configure the columns in the detailed list view"),
     NULL,
     G_CALLBACK(_detailview_show_column_editor)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_detailview_actions, \
                                    G_N_ELEMENTS(_detailview_actions), \
                                    id)
enum
{
    PROP_0,
    PROP_FIXED_COLUMNS,
};

struct _DetailViewClass
{
    StandardViewClass __parent__;
};

struct _DetailView
{
    StandardView    __parent__;

    GtkTreeViewColumn *columns[THUNAR_N_VISIBLE_COLUMNS];
    ColumnModel     *column_model;
    gboolean        fixed_columns;

    // whether the most recent item activation used a mouse button press
    gboolean        button_pressed;

    // event source id for thunar_details_view_zoom_level_changed_reload_fixed_columns
    guint           idle_id;
};

G_DEFINE_TYPE(DetailView, detailview, TYPE_STANDARD_VIEW)

static void detailview_class_init(DetailViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = detailview_finalize;
    gobject_class->get_property = detailview_get_property;
    gobject_class->set_property = detailview_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->get_accessible = detailview_get_accessible;

    StandardViewClass *standardview_class = STANDARD_VIEW_CLASS(klass);
    standardview_class->get_selected_items = detailview_get_selected_items;
    standardview_class->select_all = detailview_select_all;
    standardview_class->unselect_all = detailview_unselect_all;
    standardview_class->selection_invert = detailview_selection_invert;
    standardview_class->select_path = detailview_select_path;
    standardview_class->set_cursor = detailview_set_cursor;
    standardview_class->scroll_to_path = detailview_scroll_to_path;
    standardview_class->get_path_at_pos = detailview_get_path_at_pos;
    standardview_class->get_visible_range = detailview_get_visible_range;
    standardview_class->highlight_path = detailview_highlight_path;
    standardview_class->append_menu_items = detailview_append_menu_items;
    standardview_class->connect_accelerators = detailview_connect_accelerators;
    standardview_class->disconnect_accelerators = detailview_disconnect_accelerators;
    standardview_class->zoom_level_property_name = "last-details-view-zoom-level";

    xfce_gtk_translate_action_entries(_detailview_actions,
                                      G_N_ELEMENTS(_detailview_actions));

    // fixed-columns
    g_object_class_install_property(gobject_class,
                                    PROP_FIXED_COLUMNS,
                                    g_param_spec_boolean(
                                        "fixed-columns",
                                        "fixed-columns",
                                        "fixed-columns",
                                        TRUE,
                                        E_PARAM_READWRITE));
}

static void detailview_init(DetailView *details_view)
{
    /* we need to force the GtkTreeView to recalculate column sizes
       whenever the zoom-level changes, so we connect a handler here. */
    g_signal_connect(G_OBJECT(details_view), "notify::zoom-level",
                     G_CALLBACK(_detailview_zoom_level_changed), NULL);

    // create the tree view to embed
    GtkWidget *tree_view = exo_treeview_new();

    g_signal_connect(G_OBJECT(tree_view), "notify::model",
                     G_CALLBACK(_detailview_notify_model), details_view);
    g_signal_connect(G_OBJECT(tree_view), "button-press-event",
                     G_CALLBACK(_detailview_button_press_event), details_view);
    g_signal_connect(G_OBJECT(tree_view), "key-press-event",
                     G_CALLBACK(_detailview_key_press_event), details_view);
    g_signal_connect(G_OBJECT(tree_view), "row-activated",
                     G_CALLBACK(_detailview_row_activated), details_view);
    g_signal_connect(G_OBJECT(tree_view), "select-cursor-row",
                     G_CALLBACK(_detailview_select_cursor_row), details_view);

    gtk_container_add(GTK_CONTAINER(details_view), tree_view);
    gtk_widget_show(tree_view);

    // configure general aspects of the details view
    gtk_tree_view_set_enable_search(GTK_TREE_VIEW(tree_view), TRUE);

    // enable rubberbanding(if supported)
    gtk_tree_view_set_rubber_banding(GTK_TREE_VIEW(tree_view), TRUE);

    // connect to the default column model
    details_view->column_model = colmodel_get_default();
    g_signal_connect(G_OBJECT(details_view->column_model), "columns-changed",
                     G_CALLBACK(_detailview_columns_changed), details_view);
    g_signal_connect_after(G_OBJECT(STANDARD_VIEW(details_view)->model), "row-changed",
                            G_CALLBACK(_detailview_row_changed), details_view);

    // allocate the shared right-aligned text renderer
    GtkCellRenderer *right_aligned_renderer = g_object_new(GTK_TYPE_CELL_RENDERER_TEXT,
                                                           "xalign", 1.0f,
                                                           NULL);
    g_object_ref_sink(G_OBJECT(right_aligned_renderer));

    // allocate the shared left-aligned text renderer
    GtkCellRenderer *left_aligned_renderer = g_object_new(GTK_TYPE_CELL_RENDERER_TEXT,
                                                          "xalign", 0.0f,
                                                          NULL);
    g_object_ref_sink(G_OBJECT(left_aligned_renderer));

    // allocate the tree view columns
    for (ThunarColumn column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
        // allocate the tree view column
        GtkTreeViewColumn *tvcol = gtk_tree_view_column_new();
        details_view->columns[column] = tvcol;
        g_object_ref_sink(G_OBJECT(tvcol));

        gtk_tree_view_column_set_min_width(tvcol, COL_MIN_WIDTH);
        gtk_tree_view_column_set_resizable(tvcol, TRUE);
        //gtk_tree_view_column_set_sizing(tvcol, GTK_TREE_VIEW_COLUMN_FIXED);
        gtk_tree_view_column_set_sort_column_id(tvcol, column);
        gtk_tree_view_column_set_title(tvcol, colmodel_get_column_name(details_view->column_model, column));

        // stay informed whenever the width of the column is changed
        g_signal_connect(G_OBJECT(details_view->columns[column]), "notify::width",
                         G_CALLBACK(_detailview_notify_width), details_view);

        // name column gets special renderers
        if (column == THUNAR_COLUMN_NAME)
        {
            // add the icon renderer
            gtk_tree_view_column_pack_start(
                        details_view->columns[column],
                        STANDARD_VIEW(details_view)->icon_renderer, FALSE);
            gtk_tree_view_column_set_attributes(
                        details_view->columns[column],
                        STANDARD_VIEW(details_view)->icon_renderer,
                        "file", THUNAR_COLUMN_FILE,
                        NULL);

            // add the name renderer
            g_object_set(G_OBJECT(STANDARD_VIEW(details_view)->name_renderer),
                          "xalign", 0.0, "ellipsize", PANGO_ELLIPSIZE_END, "width-chars", 30, NULL);
            gtk_tree_view_column_pack_start(details_view->columns[column], STANDARD_VIEW(details_view)->name_renderer, TRUE);
            gtk_tree_view_column_set_attributes(details_view->columns[column], STANDARD_VIEW(details_view)->name_renderer,
                                                 "text", THUNAR_COLUMN_NAME,
                                                 NULL);

            // add some spacing between the icon and the name
            gtk_tree_view_column_set_spacing(details_view->columns[column], 2);
            gtk_tree_view_column_set_expand(details_view->columns[column], TRUE);
        }
        else
        {
            // size is right aligned, everything else is left aligned
            GtkCellRenderer *renderer =
                (column == THUNAR_COLUMN_SIZE
                 || column == THUNAR_COLUMN_SIZE_IN_BYTES)
                        ? right_aligned_renderer : left_aligned_renderer;

            // add the renderer
            gtk_tree_view_column_pack_start(details_view->columns[column],
                                            renderer, TRUE);
            gtk_tree_view_column_set_attributes(details_view->columns[column],
                                                renderer, "text", column, NULL);
        }

        // append the tree view column to the tree view
        gtk_tree_view_append_column(GTK_TREE_VIEW(tree_view), details_view->columns[column]);
    }

    // configure the tree selection

    GtkTreeSelection *selection;
    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(tree_view));
    gtk_tree_selection_set_mode(selection, GTK_SELECTION_MULTIPLE);

    g_signal_connect_swapped(G_OBJECT(selection), "changed",
                             G_CALLBACK(standardview_selection_changed), details_view);

    // apply the initial column order and visibility from the column model
    _detailview_columns_changed(details_view->column_model, details_view);

    // apply fixed column widths if we start in fixed column mode
    if (details_view->fixed_columns)
    {
        // apply to all columns
        for (ThunarColumn column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
        {
            gtk_tree_view_column_set_fixed_width(
                details_view->columns[column],
                colmodel_get_column_width(details_view->column_model,
                                              column));
        }
    }

    // release the shared text renderers
    g_object_unref(G_OBJECT(right_aligned_renderer));
    g_object_unref(G_OBJECT(left_aligned_renderer));
}

static void detailview_finalize(GObject *object)
{
    DetailView *details_view = DETAILVIEW(object);
    ThunarColumn       column;

    // release the tree view columns array
    for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
        g_object_unref(G_OBJECT(details_view->columns[column]));

    // disconnect from the default column model
    g_signal_handlers_disconnect_by_func(G_OBJECT(details_view->column_model), _detailview_columns_changed, details_view);
    g_object_unref(G_OBJECT(details_view->column_model));

    if (details_view->idle_id)
        g_source_remove(details_view->idle_id);

    G_OBJECT_CLASS(detailview_parent_class)->finalize(object);
}

static void detailview_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    DetailView *details_view = DETAILVIEW(object);

    switch (prop_id)
    {
    case PROP_FIXED_COLUMNS:
        g_value_set_boolean(value, _detailview_get_fixed_columns(details_view));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void detailview_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    DetailView *details_view = DETAILVIEW(object);

    switch (prop_id)
    {
    case PROP_FIXED_COLUMNS:
        _detailview_set_fixed_columns(details_view, g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean _detailview_get_fixed_columns(DetailView *details_view)
{
    e_return_val_if_fail(IS_DETAILVIEW(details_view), FALSE);

    return details_view->fixed_columns;
}

static void _detailview_set_fixed_columns(DetailView *details_view,
                                          gboolean fixed_columns)
{
    e_return_if_fail(IS_DETAILVIEW(details_view));

    // normalize the value
    fixed_columns = !!fixed_columns;

    // check if we have a new value
    if (details_view->fixed_columns == fixed_columns)
        return;

        // apply the new value
    details_view->fixed_columns = fixed_columns;

    // disable in reverse order, otherwise graphical glitches can appear
    if (!fixed_columns)
        gtk_tree_view_set_fixed_height_mode(
                    GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(details_view))),
                    FALSE);

    // apply the new setting to all columns
    for (ThunarColumn column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
        // apply the new column mode
        if (fixed_columns)
        {
            // apply "width" as "fixed-width" for fixed columns mode
            gint width;
            width = gtk_tree_view_column_get_width(details_view->columns[column]);
            if (width <= 0)
                width = colmodel_get_column_width(details_view->column_model, column);

            gtk_tree_view_column_set_fixed_width(details_view->columns[column], MAX(width, 1));

            // set column to fixed width
            gtk_tree_view_column_set_sizing(details_view->columns[column],
                                            GTK_TREE_VIEW_COLUMN_FIXED);
        }
        else
        {
            // reset column to grow-only mode
            gtk_tree_view_column_set_sizing(details_view->columns[column],
                                            GTK_TREE_VIEW_COLUMN_GROW_ONLY);
            //gtk_tree_view_column_set_sizing(details_view->columns[column],
            //                                GTK_TREE_VIEW_COLUMN_FIXED);
        }
    }

    /* for fixed columns mode, we can enable the fixed height
     * mode to improve the performance of the GtkTreeVeiw. */
    if (fixed_columns)
        gtk_tree_view_set_fixed_height_mode(
                    GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(details_view))),
                    TRUE);

    // notify listeners
    g_object_notify(G_OBJECT(details_view), "fixed-columns");
}

static AtkObject* detailview_get_accessible(GtkWidget *widget)
{
    // query the atk object for the tree view class
    AtkObject *object = GTK_WIDGET_CLASS(detailview_parent_class)->get_accessible(widget);

    // set custom Atk properties for the details view
    if (object != NULL)
    {
        atk_object_set_description(object, _("Detailed directory listing"));
        atk_object_set_name(object, _("Details view"));
        atk_object_set_role(object, ATK_ROLE_DIRECTORY_PANE);
    }

    return object;
}

static GList* detailview_get_selected_items(StandardView *standard_view)
{
    GtkTreeSelection *selection;

    e_return_val_if_fail(IS_DETAILVIEW(standard_view), NULL);

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))));
    return gtk_tree_selection_get_selected_rows(selection, NULL);
}

static void detailview_select_all(StandardView *standard_view)
{
    GtkTreeSelection *selection;

    e_return_if_fail(IS_DETAILVIEW(standard_view));

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))));
    gtk_tree_selection_select_all(selection);
}

static void detailview_unselect_all(StandardView *standard_view)
{
    GtkTreeSelection *selection;

    e_return_if_fail(IS_DETAILVIEW(standard_view));

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))));
    gtk_tree_selection_unselect_all(selection);
}

static void detailview_selection_invert(StandardView *standard_view)
{
    GtkTreeSelection *selection;
    GList            *selected_paths = NULL;
    GList            *lp;
    GtkTreePath      *path;

    e_return_if_fail(IS_DETAILVIEW(standard_view));

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))));

    // block updates
    g_signal_handlers_block_by_func(selection, standardview_selection_changed, standard_view);

    // get paths of selected files
    gtk_tree_selection_selected_foreach(selection, _detailview_selection_invert_foreach, &selected_paths);

    gtk_tree_selection_select_all(selection);

    for(lp = selected_paths; lp != NULL; lp = lp->next)
    {
        path = lp->data;
        gtk_tree_selection_unselect_path(selection, path);
        gtk_tree_path_free(path);
    }

    g_list_free(selected_paths);

    // unblock updates
    g_signal_handlers_unblock_by_func(selection, standardview_selection_changed, standard_view);

    standardview_selection_changed(STANDARD_VIEW(standard_view));
}

static void _detailview_selection_invert_foreach(GtkTreeModel *model,
                                                 GtkTreePath  *path,
                                                 GtkTreeIter  *iter,
                                                 gpointer      data)
{
    (void) model;
    (void) iter;

    GList **list = data;

    *list = g_list_prepend(*list, gtk_tree_path_copy(path));
}

static void detailview_select_path(StandardView *standard_view, GtkTreePath *path)
{
    e_return_if_fail(IS_DETAILVIEW(standard_view));

    GtkTreeSelection *selection;

    selection = gtk_tree_view_get_selection(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))));
    gtk_tree_selection_select_path(selection, path);
}

static void detailview_set_cursor(StandardView *standard_view, GtkTreePath *path,
                                  gboolean start_editing)
{
    GtkCellRendererMode mode;
    GtkTreeViewColumn  *column;

    e_return_if_fail(IS_DETAILVIEW(standard_view));

    // make sure the name renderer is editable
    g_object_get( G_OBJECT(standard_view->name_renderer), "mode", &mode, NULL);
    g_object_set( G_OBJECT(standard_view->name_renderer), "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);

    // tell the tree view to start editing the given row
    column = gtk_tree_view_get_column(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), 0);
    gtk_tree_view_set_cursor_on_cell(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))),
                                      path, column, standard_view->name_renderer,
                                      start_editing);

    // reset the name renderer mode
    g_object_set(G_OBJECT(standard_view->name_renderer), "mode", mode, NULL);
}

static void detailview_scroll_to_path(StandardView *standard_view,
                                      GtkTreePath        *path,
                                      gboolean            use_align,
                                      gfloat              row_align,
                                      gfloat              col_align)
{
    GtkTreeViewColumn *column;

    e_return_if_fail(IS_DETAILVIEW(standard_view));

    // tell the tree view to scroll to the given row
    column = gtk_tree_view_get_column(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), 0);
    gtk_tree_view_scroll_to_cell(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), path, column, use_align, row_align, col_align);
}

static GtkTreePath* detailview_get_path_at_pos(StandardView *standard_view,
                                               gint    x,
                                               gint    y)
{
    GtkTreePath *path;

    e_return_val_if_fail(IS_DETAILVIEW(standard_view), NULL);

    if (gtk_tree_view_get_dest_row_at_pos(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), x, y, &path, NULL))
        return path;

    return NULL;
}

static gboolean detailview_get_visible_range(StandardView *standard_view,
                                             GtkTreePath   **start_path,
                                             GtkTreePath   **end_path)
{
    e_return_val_if_fail(IS_DETAILVIEW(standard_view), FALSE);

    return gtk_tree_view_get_visible_range(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), start_path, end_path);
}

static void detailview_highlight_path(StandardView *standard_view,
                                      GtkTreePath        *path)
{
    e_return_if_fail(IS_DETAILVIEW(standard_view));
    gtk_tree_view_set_drag_dest_row(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(standard_view))), path, GTK_TREE_VIEW_DROP_INTO_OR_AFTER);
}

static void detailview_append_menu_items(StandardView  *standard_view,
                                         GtkMenu       *menu,
                                         GtkAccelGroup *accel_group)
{
    (void) standard_view;
    (void) menu;
    (void) accel_group;

    #ifdef WITH_CONFIGURE_COLUMNS
    DetailView *details_view = DETAILVIEW(standard_view);
    e_return_if_fail(IS_DETAILVIEW(details_view));

    xfce_gtk_menu_item_new_from_action_entry(
                get_action_entry(DETAILVIEW_ACTION_CONFIGURE_COLUMNS),
                G_OBJECT(details_view),
                GTK_MENU_SHELL(menu));
    #endif
}

/**
 * thunar_details_view_connect_accelerators:
 * @standard_view : a #StandardView.
 * @accel_group   : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 * The concrete implementation depends on the concrete widget which is implementing this view
 **/
static void detailview_connect_accelerators(StandardView  *standard_view,
                                            GtkAccelGroup *accel_group)
{
    DetailView *details_view = DETAILVIEW(standard_view);

    e_return_if_fail(IS_DETAILVIEW(details_view));

    xfce_gtk_accel_map_add_entries(_detailview_actions,
                                   G_N_ELEMENTS(_detailview_actions));

    xfce_gtk_accel_group_connect_action_entries(accel_group,
                                                _detailview_actions,
                                                G_N_ELEMENTS(_detailview_actions),
                                                standard_view);
}

static void detailview_disconnect_accelerators(StandardView *standard_view,
                                               GtkAccelGroup *accel_group)
{
    (void) standard_view;

    // Dont listen to the accel keys defined by the action entries any more
    xfce_gtk_accel_group_disconnect_action_entries(accel_group,
                                                   _detailview_actions,
                                                   G_N_ELEMENTS(_detailview_actions));
}

// Events ---------------------------------------------------------------------

static void _detailview_zoom_level_changed(DetailView *details_view)
{
    ThunarColumn column;
    gboolean     fixed_columns_used = FALSE;

    e_return_if_fail(IS_DETAILVIEW(details_view));

    if (details_view->fixed_columns == TRUE)
        fixed_columns_used = TRUE;

    // Disable fixed column mode during resize, since it can generate graphical glitches
    if (fixed_columns_used)
        _detailview_set_fixed_columns(details_view, FALSE);

    // determine the list of tree view columns
    for(column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
        // just queue a resize on this column
        gtk_tree_view_column_queue_resize(details_view->columns[column]);
    }

    if (fixed_columns_used)
    {
        // Call when idle to ensure that gtk_tree_view_column_queue_resize got finished
        details_view->idle_id = gdk_threads_add_idle(_detailview_zoom_level_changed_reload_fixed_columns, details_view);
    }
}

static gboolean _detailview_zoom_level_changed_reload_fixed_columns(gpointer data)
{
    DetailView *details_view = data;
    e_return_val_if_fail(IS_DETAILVIEW(details_view), FALSE);

    _detailview_set_fixed_columns(details_view, TRUE);
    details_view->idle_id = 0;
    return FALSE;
}

static void _detailview_notify_model(GtkTreeView *tree_view, GParamSpec *pspec,
                                     DetailView *details_view)
{
    (void) pspec;
    (void) details_view;
    /* We need to set the search column here, as GtkTreeView resets it
     * whenever a new model is set.
     */
    gtk_tree_view_set_search_column(tree_view, THUNAR_COLUMN_NAME);
}

static gboolean _detailview_button_press_event(GtkTreeView *tree_view,
                                               GdkEventButton *event,
                                               DetailView *details_view)
{
    // check if the event is for the bin window
    if (event->window != gtk_tree_view_get_bin_window(tree_view))
        return FALSE;

    // get the current selection
    GtkTreeSelection  *selection;
    selection = gtk_tree_view_get_selection(tree_view);

    // get the column showing the filenames
    GtkTreeViewColumn *name_column;
    name_column = details_view->columns[THUNAR_COLUMN_NAME];

    GtkTreePath *path = NULL;
    GtkTreeViewColumn *column;

    /* unselect all selected items if the user clicks on an empty area
     * of the treeview and no modifier key is active */
    if ((event->state & gtk_accelerator_get_default_mod_mask()) == 0
        && !gtk_tree_view_get_path_at_pos(tree_view, event->x, event->y,
                                          &path, &column, NULL, NULL))
    {
        gtk_tree_selection_unselect_all(selection);
    }

    // make sure that rubber banding is enabled
    gtk_tree_view_set_rubber_banding(tree_view, TRUE);

    // left click
    if (path != NULL && event->type == GDK_BUTTON_PRESS && event->button == 1)
    {
        details_view->button_pressed = TRUE;

        // grab the tree view
        gtk_widget_grab_focus(GTK_WIDGET(tree_view));

        GtkTreePath *cursor_path;

        gtk_tree_view_get_cursor(tree_view, &cursor_path, NULL);

        if (cursor_path != NULL)
        {
            gtk_tree_path_free(cursor_path);

            if (column != name_column)
                gtk_tree_selection_unselect_all(selection);

            // do not start rubber banding from the name column
            if (!gtk_tree_selection_path_is_selected(selection, path)
                    && column == name_column)
            {
                /* the clicked row does not have the focus and is not
                 * selected, so make it the new selection(start) */
                gtk_tree_selection_unselect_all(selection);
                gtk_tree_selection_select_path(selection, path);

                gtk_tree_path_free(path);

                // return FALSE to not abort dragging
                return FALSE;
            }

            gtk_tree_path_free(path);
        }
    }

    // right click
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        // FOCUS
        gtk_widget_grab_focus(GTK_WIDGET(tree_view));

        if (path == NULL)
        {
            // create the context menu
            standardview_context_menu(STANDARD_VIEW(details_view));
        }
        else
        {
            if (column != name_column)
            {
                // if the clicked path is not selected, unselect all other paths
                if (!gtk_tree_selection_path_is_selected(selection, path))
                    gtk_tree_selection_unselect_all(selection);

                // queue the menu popup
                standardview_popup_timer(STANDARD_VIEW(details_view), event);
            }
            else
            {
                // select the clicked path if necessary
                if (!gtk_tree_selection_path_is_selected(selection, path))
                {
                    gtk_tree_selection_unselect_all(selection);
                    gtk_tree_selection_select_path(selection, path);
                }

                // show the context menu
                standardview_context_menu(STANDARD_VIEW(details_view));
            }

            gtk_tree_path_free(path);
        }

        return TRUE;
    }

    return FALSE;
}

static gboolean _detailview_key_press_event(GtkTreeView *tree_view,
                                            GdkEventKey *event,
                                            DetailView *details_view)
{
    (void) tree_view;

    details_view->button_pressed = FALSE;

    // popup context menu if "Menu" or "<Shift>F10" is pressed
    if (event->keyval == GDK_KEY_Menu
        || ((event->state & GDK_SHIFT_MASK) != 0
            && event->keyval == GDK_KEY_F10))
    {
        standardview_context_menu(STANDARD_VIEW(details_view));

        return TRUE;
    }

    return FALSE;
}

static void _detailview_row_activated(GtkTreeView *tree_view,
                                      GtkTreePath *path,
                                      GtkTreeViewColumn *column,
                                      DetailView *details_view)
{
    (void) column;

    e_return_if_fail(IS_DETAILVIEW(details_view));

    // be sure to have only the clicked item selected
    if (details_view->button_pressed)
    {
        GtkTreeSelection *selection;

        selection = gtk_tree_view_get_selection(tree_view);
        gtk_tree_selection_unselect_all(selection);
        gtk_tree_selection_select_path(selection, path);
    }

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(details_view));
    ThunarLauncher *launcher = window_get_launcher(APPWINDOW(window));
    launcher_activate_selected_files(launcher, LAUNCHER_CHANGE_DIRECTORY, NULL);

    // FOCUS
    gtk_widget_grab_focus(GTK_WIDGET(tree_view));
}

static gboolean _detailview_select_cursor_row(GtkTreeView *tree_view,
                                              gboolean editing,
                                              DetailView *details_view)
{
    /* This function is a work-around to fix bug #2487. The default gtk handler for
     * the "select-cursor-row" signal changes the selection to just the cursor row,
     * which prevents multiple file selections being opened. Thus we bypass the gtk
     * signal handler with g_signal_stop_emission_by_name, and emit the "open" action
     * directly. A better long-term solution would be to fix exo to avoid using the
     * default gtk signal handler there.
     */

    (void) editing;
    ThunarLauncher *launcher;
    GtkWidget      *window;

    e_return_val_if_fail(IS_DETAILVIEW(details_view), FALSE);

    g_signal_stop_emission_by_name(tree_view,"select-cursor-row");

    window = gtk_widget_get_toplevel(GTK_WIDGET(details_view));
    launcher = window_get_launcher(APPWINDOW(window));
    launcher_activate_selected_files(launcher, LAUNCHER_CHANGE_DIRECTORY, NULL);

    return TRUE;
}

static void _detailview_columns_changed(ColumnModel *column_model,
                                        DetailView *details_view)
{
    const ThunarColumn *column_order;
    ThunarColumn        column;

    e_return_if_fail(IS_DETAILVIEW(details_view));
    e_return_if_fail(IS_COLUMN_MODEL(column_model));
    e_return_if_fail(details_view->column_model == column_model);

    // determine the new column order
    column_order = colmodel_get_column_order(column_model);

    // apply new order and visibility
    for(column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
        // apply the new visibility for the tree view column
        gtk_tree_view_column_set_visible(details_view->columns[column], colmodel_get_column_visible(column_model, column));

        // change the order of the column relative to its predecessor
        if (column > 0)
        {
            gtk_tree_view_move_column_after(GTK_TREE_VIEW(gtk_bin_get_child(GTK_BIN(details_view))),
                                             details_view->columns[column_order[column]],
                                             details_view->columns[column_order[column - 1]]);
        }
    }
}

static void _detailview_row_changed(GtkTreeView *tree_view, GtkTreePath *path,
                                    GtkTreeViewColumn *column,
                                    DetailView *details_view)
{
    (void) tree_view;
    (void) path;
    (void) column;

    e_return_if_fail(IS_DETAILVIEW(details_view));

    gtk_widget_queue_draw(GTK_WIDGET(details_view));
}

static void _detailview_notify_width(GtkTreeViewColumn *tree_view_column,
                                     GParamSpec *pspec,
                                     DetailView *details_view)
{
    (void) pspec;
    ThunarColumn column;

    e_return_if_fail(GTK_IS_TREE_VIEW_COLUMN(tree_view_column));
    e_return_if_fail(IS_DETAILVIEW(details_view));

    // lookup the column no for the given tree view column
    for (column = 0; column < THUNAR_N_VISIBLE_COLUMNS; ++column)
    {
        if (details_view->columns[column] == tree_view_column)
        {
            // save the new width as default fixed width
            colmodel_set_column_width(details_view->column_model,
                                                 column,
                                                 gtk_tree_view_column_get_width(
                                                     tree_view_column));
            break;
        }
    }
}

// Popup action ---------------------------------------------------------------

static void _detailview_show_column_editor(gpointer parent)
{
    (void) parent;

    return;
}


