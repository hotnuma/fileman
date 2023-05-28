/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
#include <standardview.h>

#include <navigator.h>
#include <component.h>
#include <baseview.h>

#include <appwindow.h>
#include <appmenu.h>
#include <iconrender.h>
#include <dnd.h>
#include <dialogs.h>

#include <gio_ext.h>
#include <gtk_ext.h>
#include <pango_ext.h>
#include <utils.h>

// for desktop file edit
#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif

// Allocation -----------------------------------------------------------------

static void standardview_navigator_init(ThunarNavigatorIface *iface);
static void standardview_component_init(ThunarComponentIface *iface);
static void standardview_baseview_init(BaseViewIface *iface);

// Object Class ---------------------------------------------------------------

static GObject* standardview_constructor(GType type, guint n_props,
                                         GObjectConstructParam *props);
static void standardview_dispose(GObject *object);
static void standardview_finalize(GObject *object);
static void standardview_get_property(GObject *object, guint prop_id,
                                      GValue *value, GParamSpec *pspec);
static void standardview_set_property(GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec);

// Widget Class ---------------------------------------------------------------

static void standardview_realize(GtkWidget *widget);
static void standardview_unrealize(GtkWidget *widget);
static void standardview_grab_focus(GtkWidget *widget);
static gboolean standardview_draw(GtkWidget *widget, cairo_t *cr);

// Accelerators ---------------------------------------------------------------

static void _standardview_connect_accelerators(StandardView *view);
static void _standardview_disconnect_accelerators(StandardView *view);

// Navigator Interface --------------------------------------------------------

static ThunarFile* standardview_get_current_directory(ThunarNavigator *navigator);
static void standardview_set_current_directory(ThunarNavigator *navigator,
                                               ThunarFile *current_directory);
static void _standardview_scroll_position_save(StandardView *view);
static void _standardview_current_directory_destroy(ThunarFile *current_directory,
                                                    StandardView *view);
static ThunarFile* _standardview_get_fallback_directory(ThunarFile *directory,
                                                        GError *error);
static void _standardview_current_directory_changed(ThunarFile *current_directory,
                                                    StandardView *view);
static void _standardview_loading_unbound(gpointer user_data);
static void _standardview_restore_selection_from_history(StandardView *view);

// Component Interface --------------------------------------------------------

static GList* standardview_get_selected_files_component(ThunarComponent *component);
static void standardview_set_selected_files_component(ThunarComponent *component,
                                                      GList *selected_files);

// BaseView Interface -------------------------------------------------------------

static GList* standardview_get_selected_files_view(BaseView *baseview);
static void standardview_set_selected_files_view(BaseView *baseview,
                                                 GList *selected_files);
static gboolean standardview_get_show_hidden(BaseView *baseview);
static void standardview_set_show_hidden(BaseView *baseview, gboolean show_hidden);
static ThunarZoomLevel standardview_get_zoom_level(BaseView *baseview);
static void standardview_set_zoom_level(BaseView *baseview,
                                        ThunarZoomLevel zoom_level);
static gboolean standardview_get_loading(BaseView *baseview);
// StandardView Property
static void standardview_set_loading(StandardView *view, gboolean loading);
static void _standardview_new_files(StandardView *view, GList *path_list);
static void standardview_reload(BaseView *baseview, gboolean reload_info);
static gboolean standardview_get_visible_range(BaseView *baseview,
                                               ThunarFile **start_file,
                                               ThunarFile **end_file);
static void standardview_scroll_to_file(BaseView *baseview, ThunarFile *file,
                                        gboolean select_file, gboolean use_align,
                                        gfloat row_align, gfloat col_align);
static const gchar* standardview_get_statusbar_text(BaseView *baseview);

// standardview_constructor ---------------------------------------------------

static void _standardview_sort_column_changed(GtkTreeSortable *tree_sortable,
                                              StandardView *view);
static gboolean _standardview_scroll_event(GtkWidget *widget, GdkEventScroll *event,
                                           StandardView *view);
static gboolean _standardview_key_press_event(GtkWidget *widget, GdkEventKey *event,
                                              StandardView *view);

// standardview_init ----------------------------------------------------------

static void _standardview_select_after_row_deleted(ListModel *model, GtkTreePath *path,
                                                   StandardView *view);
static void _standardview_row_changed(ListModel *model, GtkTreePath *path,
                                      GtkTreeIter *iter, StandardView *view);
static void _standardview_rows_reordered(ListModel *model, GtkTreePath *path,
                                         GtkTreeIter *iter, gpointer new_order,
                                         StandardView *view);
static gboolean _standardview_restore_selection_idle(StandardView *view);
static void _standardview_error(ListModel *model, const GError *error,
                                StandardView *view);
static void _standardview_update_statusbar_text(StandardView *view);
static gboolean _standardview_update_statusbar_text_idle(gpointer data);

// Public Functions -----------------------------------------------------------

// standardview_context_menu
static void _standardview_append_menu_items(StandardView *view, GtkMenu *menu,
                                            GtkAccelGroup *accel_group);
// standardview_queue_popup
static gboolean _popup_timer(gpointer user_data);
static void _popup_timer_destroy(gpointer user_data);
static gboolean _on_button_release_event(GtkWidget *widget,
                                         GdkEventButton *event,
                                         StandardView *view);

// Actions --------------------------------------------------------------------

static void _standardview_select_all_files(BaseView *baseview);
static void _standardview_select_by_pattern(BaseView *baseview);
static void _standardview_selection_invert(BaseView *baseview);

// DnD Source -----------------------------------------------------------------

static void _on_drag_begin(GtkWidget *widget, GdkDragContext *context,
                           StandardView *view);
static void _on_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                              GtkSelectionData *seldata,
                              guint info, guint timestamp,
                              StandardView *view);
static void _on_drag_data_delete(GtkWidget *widget, GdkDragContext *context,
                                 StandardView *view);
static void _on_drag_end(GtkWidget *widget, GdkDragContext *context,
                         StandardView *view);

// DnD Target -----------------------------------------------------------------

static gboolean _on_drag_motion(GtkWidget *widget, GdkDragContext *context,
                                gint x, gint y, guint timestamp,
                                StandardView *view);
static ThunarFile* _standardview_get_drop_file(StandardView *view,
                                               gint x,
                                               gint y,
                                               GtkTreePath **path_return);
static GdkDragAction _standardview_get_dest_actions(StandardView *view,
                                                    GdkDragContext *context,
                                                    gint x,
                                                    gint y,
                                                    guint timestamp,
                                                    ThunarFile **file_return);
static gboolean _standardview_drag_scroll_timer(gpointer user_data);
static void _standardview_drag_scroll_timer_destroy(gpointer user_data);

static gboolean _on_drag_drop(GtkWidget *widget, GdkDragContext *context,
                              gint x, gint y, guint timestamp,
                              StandardView *view);

static void _on_drag_data_received(GtkWidget *widget, GdkDragContext *context,
                                   gint x, gint y,
                                   GtkSelectionData *seldata,
                                   guint info, guint timestamp,
                                   StandardView *view);
static bool _received_text_uri_list(GdkDragContext *context,
                                    gint x, gint y, guint timestamp,
                                    StandardView *view);
static GClosure* _standardview_new_files_closure(StandardView *view,
                                                 GtkWidget *source_view);
static bool _received_netscape_url(GtkWidget *widget,
                                   gint x, gint y,
                                   GtkSelectionData *seldata,
                                   StandardView *view);
static void _reload_directory(GPid pid, gint status, gpointer user_data);

static void _on_drag_leave(GtkWidget *widget, GdkDragContext *context,
                           guint timestamp, StandardView *view);

// Actions --------------------------------------------------------------------

typedef enum
{
    STANDARDVIEW_ACTION_SELECT_ALL_FILES,
    STANDARDVIEW_ACTION_SELECT_BY_PATTERN,
    STANDARDVIEW_ACTION_INVERT_SELECTION,

} StandardViewAction;

static XfceGtkActionEntry _standardview_actions[] =
{
    {STANDARDVIEW_ACTION_SELECT_ALL_FILES,
     "<Actions>/StandardView/select-all-files",
     "<Primary>a",
     XFCE_GTK_MENU_ITEM,
     N_("Select _all Files"),
     N_("Select all files in this window"),
     NULL,
     G_CALLBACK(_standardview_select_all_files)},

    {STANDARDVIEW_ACTION_SELECT_BY_PATTERN,
     "<Actions>/StandardView/select-by-pattern",
     "<Primary>s",
     XFCE_GTK_MENU_ITEM,
     N_("Select _by Pattern..."),
     N_("Select all files that match a certain pattern"),
     NULL,
     G_CALLBACK(_standardview_select_by_pattern)},

    {STANDARDVIEW_ACTION_INVERT_SELECTION,
     "<Actions>/StandardView/invert-selection",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Invert Selection"),
     N_("Select all files but not those currently selected"),
     NULL,
     G_CALLBACK(_standardview_selection_invert)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_standardview_actions, \
                                    G_N_ELEMENTS(_standardview_actions), \
                                    id)

// DnD target -----------------------------------------------------------------

enum
{
    TARGET_TEXT_URI_LIST,
    TARGET_NETSCAPE_URL,
};

// Target types for dragging from the view
static const GtkTargetEntry _drag_entries[] =
{
    {"text/uri-list",   0, TARGET_TEXT_URI_LIST},
};

// Target types for dropping to the view
static const GtkTargetEntry _drop_entries[] =
{
    {"text/uri-list",   0, TARGET_TEXT_URI_LIST},
    {"_NETSCAPE_URL",   0, TARGET_NETSCAPE_URL},
};

// StandardView ---------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_LOADING,
    PROP_DISPLAY_NAME,
    PROP_TOOLTIP_TEXT,
    PROP_SELECTED_FILES,
    PROP_SHOW_HIDDEN,
    PROP_STATUSBAR_TEXT,
    PROP_ZOOM_LEVEL,
    PROP_DIRECTORY_SPECIFIC_SETTINGS,
    PROP_ACCEL_GROUP,
    N_PROPERTIES
};
static GParamSpec *_stdv_props[N_PROPERTIES] = {NULL,};

enum
{
    START_OPEN_LOCATION,
    LAST_SIGNAL,
};
static guint _stdv_signals[LAST_SIGNAL];

struct _StandardViewPrivate
{
    ThunarFile  *current_directory;

    ThunarHistory *history;
    ThunarZoomLevel zoom_level;
    GHashTable  *scroll_to_files;

    gchar       *statusbar_text;
    guint       statusbar_text_idle_id;

    // popup with timer
    guint       popup_timer_id;
    GdkEvent    *drag_timer_event;

    // drag support
    GList       *drag_g_file_list;
    guint       drag_scroll_timer_id;

    // drop site support
    guint       drop_data_ready : 1;    // whether the drop data was received already
    guint       drop_highlight : 1;
    guint       drop_occurred : 1;      // whether the data was dropped
    GList       *drop_file_list;        // the list of URIs that are contained in the drop data */

    /* the "new-files" closure, which is used to select files whenever
     * new files are created by a ThunarJob associated with this view */
    GClosure    *new_files_closure;

    /* the "new-files" path list that was remembered in the closure callback
     * if the view is currently being loaded and as such the folder may
     * not have all "new-files" at hand. This list is used when the
     * folder tells that it's ready loading and the view will try again
     * to select exactly these files. */
    GList       *new_files_path_list;

    // scroll-to-file support
    ThunarFile  *scroll_to_file;
    guint       scroll_to_select : 1;
    guint       scroll_to_use_align : 1;
    gfloat      scroll_to_row_align;
    gfloat      scroll_to_col_align;

    // selected_files support
    GList       *selected_files;
    guint       restore_selection_idle_id;

    // file insert signal
    gulong      row_changed_id;
};

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(
                    StandardView, standardview, GTK_TYPE_SCROLLED_WINDOW,
                    G_IMPLEMENT_INTERFACE(TYPE_THUNARNAVIGATOR,
                                          standardview_navigator_init)
                    G_IMPLEMENT_INTERFACE(TYPE_THUNARCOMPONENT,
                                          standardview_component_init)
                    G_IMPLEMENT_INTERFACE(TYPE_BASEVIEW,
                                          standardview_baseview_init)
                    G_ADD_PRIVATE(StandardView))

static void standardview_class_init(StandardViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructor = standardview_constructor;
    gobject_class->dispose = standardview_dispose;
    gobject_class->finalize = standardview_finalize;
    gobject_class->get_property = standardview_get_property;
    gobject_class->set_property = standardview_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->realize = standardview_realize;
    gtkwidget_class->unrealize = standardview_unrealize;
    gtkwidget_class->grab_focus = standardview_grab_focus;
    gtkwidget_class->draw = standardview_draw;

    xfce_gtk_translate_action_entries(_standardview_actions,
                                      G_N_ELEMENTS(_standardview_actions));

    // Override property to set it as writable
    _stdv_props[PROP_LOADING] =
        g_param_spec_override("loading",
                              g_param_spec_boolean("loading",
                                                   "loading",
                                                   "loading",
                                                   false,
                                                   E_PARAM_READWRITE));

    // Display name of the current directory, for label text
    _stdv_props[PROP_DISPLAY_NAME] =
        g_param_spec_string("display-name",
                            "display-name",
                            "display-name",
                            NULL,
                            E_PARAM_READABLE);

    // Full parsed name of the current directory, for label tooltip
    _stdv_props[PROP_TOOLTIP_TEXT] =
        g_param_spec_string("tooltip-text",
                            "tooltip-text",
                            "tooltip-text",
                            NULL,
                            E_PARAM_READABLE);

    // Whether to use directory specific settings.
    _stdv_props[PROP_DIRECTORY_SPECIFIC_SETTINGS] =
        g_param_spec_boolean("directory-specific-settings",
                             "directory-specific-settings",
                             "directory-specific-settings",
                             false,
                             E_PARAM_READWRITE);

    // override ThunarComponent's properties
    gpointer g_iface = g_type_default_interface_peek(TYPE_THUNARCOMPONENT);
    _stdv_props[PROP_SELECTED_FILES] =
        g_param_spec_override("selected-files",
                              g_object_interface_find_property(g_iface,
                                                               "selected-files"));

    // override ThunarNavigator's properties
    g_iface = g_type_default_interface_peek(TYPE_THUNARNAVIGATOR);
    _stdv_props[PROP_CURRENT_DIRECTORY] =
        g_param_spec_override("current-directory",
                              g_object_interface_find_property(g_iface,
                                                               "current-directory"));

    // override BaseView's properties
    g_iface = g_type_default_interface_peek(TYPE_BASEVIEW);
    _stdv_props[PROP_STATUSBAR_TEXT] =
        g_param_spec_override("statusbar-text",
                              g_object_interface_find_property(g_iface,
                                                               "statusbar-text"));

    _stdv_props[PROP_SHOW_HIDDEN] =
        g_param_spec_override("show-hidden",
                              g_object_interface_find_property(g_iface,
                                                               "show-hidden"));

    _stdv_props[PROP_ZOOM_LEVEL] =
        g_param_spec_override("zoom-level",
                              g_object_interface_find_property(g_iface,
                                                               "zoom-level"));

    _stdv_props[PROP_ACCEL_GROUP] =
        g_param_spec_object("accel-group",
                            "accel-group",
                            "accel-group",
                            GTK_TYPE_ACCEL_GROUP,
                            G_PARAM_WRITABLE);

    // install all properties
    g_object_class_install_properties(gobject_class,
                                      N_PROPERTIES,
                                      _stdv_props);

    /**
     * StandardView::start-opn-location:
     * @standard_view : a #StandardView.
     * @initial_text  : the inital location text.
     *
     * Emitted by @standard_view, whenever the user requests to
     * select a custom location(using either the "Open Location"
     * dialog or the location entry widget) by specifying an
     * @initial_text(i.e. if the user types "/" into the
     * view).
     **/
    _stdv_signals[START_OPEN_LOCATION] =
        g_signal_new(I_("start-open-location"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(StandardViewClass,
                                     start_open_location),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__STRING,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_STRING);
}

static void standardview_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = standardview_get_current_directory;
    iface->set_current_directory = standardview_set_current_directory;
}

static void standardview_component_init(ThunarComponentIface *iface)
{
    iface->get_selected_files = standardview_get_selected_files_component;
    iface->set_selected_files = standardview_set_selected_files_component;
}

static void standardview_baseview_init(BaseViewIface *iface)
{
    iface->get_selected_files = standardview_get_selected_files_view;
    iface->set_selected_files = standardview_set_selected_files_view;
    iface->get_show_hidden = standardview_get_show_hidden;
    iface->set_show_hidden = standardview_set_show_hidden;
    iface->get_zoom_level = standardview_get_zoom_level;
    iface->set_zoom_level = standardview_set_zoom_level;
    iface->get_loading = standardview_get_loading;

    iface->reload = standardview_reload;
    iface->get_visible_range = standardview_get_visible_range;
    iface->scroll_to_file = standardview_scroll_to_file;
    iface->get_statusbar_text = standardview_get_statusbar_text;
}

// Standard View Init ---------------------------------------------------------

static void standardview_init(StandardView *view)
{
    view->priv = standardview_get_instance_private(view);

    /* allocate the scroll_to_files mapping
     * (directory GFile -> first visible child GFile) */
    view->priv->scroll_to_files = g_hash_table_new_full(
                                                    g_file_hash,
                                                    (GEqualFunc) g_file_equal,
                                                    g_object_unref,
                                                    g_object_unref);

    // initialize the scrolled window
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(view),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(view), NULL);
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(view), NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(view),
                                        GTK_SHADOW_IN);

    // setup the history support
    view->priv->history = g_object_new(TYPE_THUNARHISTORY, NULL);
    g_signal_connect_swapped(G_OBJECT(view->priv->history),
                             "change-directory",
                             G_CALLBACK(navigator_change_directory),
                             view);

    // setup the list model
    view->model = listmodel_new();
    g_signal_connect_after(G_OBJECT(view->model), "row-deleted",
                           G_CALLBACK(_standardview_select_after_row_deleted),
                           view);

    view->priv->row_changed_id =
        g_signal_connect(G_OBJECT(view->model), "row-changed",
                         G_CALLBACK(_standardview_row_changed), view);

    g_signal_connect(G_OBJECT(view->model), "rows-reordered",
                     G_CALLBACK(_standardview_rows_reordered), view);

    g_signal_connect(G_OBJECT(view->model), "error",
                     G_CALLBACK(_standardview_error), view);

    // setup the icon renderer
    view->icon_renderer = iconrender_new();
    g_object_ref_sink(G_OBJECT(view->icon_renderer));

    g_object_bind_property(G_OBJECT(view), "zoom-level",
                           G_OBJECT(view->icon_renderer), "size",
                           G_BINDING_SYNC_CREATE);

    // setup the name renderer
    view->name_renderer =
        g_object_new(GTK_TYPE_CELL_RENDERER_TEXT,
#if PANGO_VERSION_CHECK(1, 44, 0)
                     "attributes",
                     e_pango_attr_disable_hyphens(),
#endif
                     "alignment",
                     PANGO_ALIGN_CENTER,
                     "xalign",
                     0.5,
                     NULL);

    g_object_ref_sink(G_OBJECT(view->name_renderer));

    // be sure to update the selection whenever the folder changes
    g_signal_connect_swapped(G_OBJECT(view->model),
                             "notify::folder",
                             G_CALLBACK(standardview_selection_changed),
                             view);

    /* be sure to update the statusbar text whenever the number of
     * files in our model changes.
     */
    g_signal_connect_swapped(G_OBJECT(view->model),
                             "notify::num-files",
                             G_CALLBACK(_standardview_update_statusbar_text),
                             view);

    // be sure to update the statusbar text whenever the file-size-binary property changes
    g_signal_connect_swapped(G_OBJECT(view->model),
                             "notify::file-size-binary",
                             G_CALLBACK(_standardview_update_statusbar_text),
                             view);

    // add widget to css class
    gtk_style_context_add_class(
                gtk_widget_get_style_context(GTK_WIDGET(view)),
                "standard-view");

    view->accel_group = NULL;
}

// Object Class ---------------------------------------------------------------

static GObject* standardview_constructor(GType type, guint n_props,
                                         GObjectConstructParam *props)
{
    // let the GObject constructor create the instance
    GObject *object;
    object = G_OBJECT_CLASS(standardview_parent_class)->constructor(type,
                                                                    n_props,
                                                                    props);
    // cast to standard_view for convenience
    StandardView *view = STANDARD_VIEW(object);

    ThunarZoomLevel zoom_level = THUNAR_ZOOM_LEVEL_25_PERCENT;
    baseview_set_zoom_level(BASEVIEW(view), zoom_level);

    // determine the real view widget(treeview or iconview)
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(object));

    /* apply our list model to the real view(the child of the scrolled window),
     * we therefore assume that all real views have the "model" property. */
    g_object_set(G_OBJECT(child), "model", view->model, NULL);

    ThunarColumn sort_column = THUNAR_COLUMN_NAME;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(view->model),
                                         sort_column, sort_order);

    // stay informed about changes to the sort column/order
    g_signal_connect(G_OBJECT(view->model), "sort-column-changed",
                     G_CALLBACK(_standardview_sort_column_changed), view);

    // setup support to navigate using a horizontal mouse wheel
    // and the back and forward buttons
    g_signal_connect(G_OBJECT(child), "scroll-event",
                     G_CALLBACK(_standardview_scroll_event), object);

    // need to catch certain keys for the internal view widget
    g_signal_connect(G_OBJECT(child), "key-press-event",
                     G_CALLBACK(_standardview_key_press_event), object);

    // setup the real view as drag source
    gtk_drag_source_set(child,
                        GDK_BUTTON1_MASK,
                        _drag_entries,
                        G_N_ELEMENTS(_drag_entries),
                        GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);

    g_signal_connect(G_OBJECT(child), "drag-begin",
                     G_CALLBACK(_on_drag_begin), object);

    g_signal_connect(G_OBJECT(child), "drag-data-get",
                     G_CALLBACK(_on_drag_data_get), object);

    g_signal_connect(G_OBJECT(child), "drag-data-delete",
                     G_CALLBACK(_on_drag_data_delete), object);

    g_signal_connect(G_OBJECT(child), "drag-end",
                     G_CALLBACK(_on_drag_end), object);

    // setup the real view as drop site
    gtk_drag_dest_set(child,
                      0,
                      _drop_entries,
                      G_N_ELEMENTS(_drop_entries),
                      GDK_ACTION_ASK
                      | GDK_ACTION_COPY
                      | GDK_ACTION_LINK
                      | GDK_ACTION_MOVE);

    g_signal_connect(G_OBJECT(child), "drag-motion",
                     G_CALLBACK(_on_drag_motion), object);

    g_signal_connect(G_OBJECT(child), "drag-drop",
                     G_CALLBACK(_on_drag_drop), object);

    g_signal_connect(G_OBJECT(child), "drag-data-received",
                     G_CALLBACK(_on_drag_data_received), object);

    g_signal_connect(G_OBJECT(child), "drag-leave",
                     G_CALLBACK(_on_drag_leave), object);

    // done, we have a working object
    return object;
}

static void standardview_dispose(GObject *object)
{
    StandardView *view = STANDARD_VIEW(object);

    if (view->loading_binding != NULL)
    {
        g_object_unref(view->loading_binding);
        view->loading_binding = NULL;
    }

    // be sure to cancel any pending drag autoscroll timer
    if (view->priv->drag_scroll_timer_id != 0)
        g_source_remove(view->priv->drag_scroll_timer_id);

    // be sure to cancel any pending drag timer
    if (view->priv->popup_timer_id)
        g_source_remove(view->priv->popup_timer_id);

    // be sure to free any pending drag timer event
    if (view->priv->drag_timer_event != NULL)
    {
        gdk_event_free(view->priv->drag_timer_event);
        view->priv->drag_timer_event = NULL;
    }

    // disconnect from file
    if (view->priv->current_directory != NULL)
    {
        g_signal_handlers_disconnect_matched(view->priv->current_directory,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, view);
        g_object_unref(view->priv->current_directory);
        view->priv->current_directory = NULL;
    }

    G_OBJECT_CLASS(standardview_parent_class)->dispose(object);
}

static void standardview_finalize(GObject *object)
{
    StandardView *view = STANDARD_VIEW(object);

    // some safety checks
    e_assert(view->loading_binding == NULL);
    e_assert(view->icon_factory == NULL);

    // disconnect accelerators
    _standardview_disconnect_accelerators(view);

    // release the scroll_to_file reference(if any)
    if (view->priv->scroll_to_file != NULL)
        g_object_unref(G_OBJECT(view->priv->scroll_to_file));

    // release the selected_files list(if any)
    e_list_free(view->priv->selected_files);

    // release the drag path list(just in case the drag-end wasn't fired before)
    e_list_free(view->priv->drag_g_file_list);

    // release the drop path list(just in case the drag-leave wasn't fired before)
    e_list_free(view->priv->drop_file_list);

    // release the history
    g_object_unref(view->priv->history);

    // release the reference on the name renderer
    g_object_unref(G_OBJECT(view->name_renderer));

    // release the reference on the icon renderer
    g_object_unref(G_OBJECT(view->icon_renderer));

    // drop any existing "new-files" closure
    if (view->priv->new_files_closure != NULL)
    {
        g_closure_invalidate(view->priv->new_files_closure);
        g_closure_unref(view->priv->new_files_closure);
        view->priv->new_files_closure = NULL;
    }

    // drop any remaining "new-files" paths
    e_list_free(view->priv->new_files_path_list);

    // disconnect from the list model
    g_signal_handlers_disconnect_matched(G_OBJECT(view->model),
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, view);
    g_object_unref(G_OBJECT(view->model));

    // remove selection restore timeout
    if (view->priv->restore_selection_idle_id != 0)
        g_source_remove(view->priv->restore_selection_idle_id);

    // free the statusbar text(if any)
    if (view->priv->statusbar_text_idle_id != 0)
        g_source_remove(view->priv->statusbar_text_idle_id);

    g_free(view->priv->statusbar_text);

    // release the scroll_to_files hash table
    g_hash_table_destroy(view->priv->scroll_to_files);

    G_OBJECT_CLASS(standardview_parent_class)->finalize(object);
}

static void standardview_get_property(GObject *object, guint prop_id,
                                      GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarFile *current_directory;

    switch (prop_id)
    {

    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value,
                           navigator_get_current_directory(
                                            THUNARNAVIGATOR(object)));
        break;

    case PROP_LOADING:
        g_value_set_boolean(value, baseview_get_loading(BASEVIEW(object)));
        break;

    case PROP_DISPLAY_NAME:
        current_directory = navigator_get_current_directory(
                                            THUNARNAVIGATOR(object));
        if (current_directory != NULL)
        {
            g_value_set_static_string(value,
                                      th_file_get_display_name(current_directory));
        }
        break;

    case PROP_TOOLTIP_TEXT:
        current_directory = navigator_get_current_directory(THUNARNAVIGATOR(object));
        if (current_directory != NULL)
        {
            g_value_take_string(
                    value,
                    g_file_get_parse_name(th_file_get_file(current_directory)));
        }
        break;

    case PROP_SELECTED_FILES:
        g_value_set_boxed(value, component_get_selected_files(THUNARCOMPONENT(object)));
        break;

    case PROP_SHOW_HIDDEN:
        g_value_set_boolean(value, baseview_get_show_hidden(BASEVIEW(object)));
        break;

    case PROP_STATUSBAR_TEXT:
        g_value_set_static_string(value, baseview_get_statusbar_text(BASEVIEW(object)));
        break;

    case PROP_ZOOM_LEVEL:
        g_value_set_enum(value, baseview_get_zoom_level(BASEVIEW(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void standardview_set_property(GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    StandardView *standard_view = STANDARD_VIEW(object);

    switch (prop_id)
    {

    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNARNAVIGATOR(object),
                                        g_value_get_object(value));
        break;

    case PROP_LOADING:
        standardview_set_loading(standard_view, g_value_get_boolean(value));
        break;

    case PROP_SELECTED_FILES:
        component_set_selected_files(THUNARCOMPONENT(object),
                                     g_value_get_boxed(value));
        break;

    case PROP_SHOW_HIDDEN:
        baseview_set_show_hidden(BASEVIEW(object), g_value_get_boolean(value));
        break;

    case PROP_ZOOM_LEVEL:
        baseview_set_zoom_level(BASEVIEW(object), g_value_get_enum(value));
        break;

    case PROP_ACCEL_GROUP:
        _standardview_disconnect_accelerators(standard_view);
        standard_view->accel_group = g_value_dup_object(value);
        _standardview_connect_accelerators(standard_view);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Widget Class ---------------------------------------------------------------

static void standardview_realize(GtkWidget *widget)
{
    // let the GtkWidget do its work
    GTK_WIDGET_CLASS(standardview_parent_class)->realize(widget);

    // determine the icon factory for the screen on which we are realized
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(
                                                gtk_widget_get_screen(widget));

    StandardView *view = STANDARD_VIEW(widget);
    view->icon_factory = iconfact_get_for_icon_theme(icon_theme);
}

static void standardview_unrealize(GtkWidget *widget)
{
    StandardView *view = STANDARD_VIEW(widget);

    // drop the reference on the icon factory
    g_signal_handlers_disconnect_by_func(G_OBJECT(view->icon_factory),
                                         gtk_widget_queue_draw, view);

    g_object_unref(G_OBJECT(view->icon_factory));
    view->icon_factory = NULL;

    // let the GtkWidget do its work
    GTK_WIDGET_CLASS(standardview_parent_class)->unrealize(widget);
}

static void standardview_grab_focus(GtkWidget *widget)
{
    // forward the focus grab to the real view
    gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(widget)));
}

static gboolean standardview_draw(GtkWidget *widget, cairo_t *cr)
{
    // let the scrolled window do it's work
    cairo_save(cr);
    gboolean result = GTK_WIDGET_CLASS(standardview_parent_class)->draw(widget, cr);
    cairo_restore(cr);

    // render the folder drop shadow
    if (STANDARD_VIEW(widget)->priv->drop_highlight)
    {
        GtkAllocation a;
        gtk_widget_get_allocation(widget, &a);

        GtkStyleContext *context;
        context = gtk_widget_get_style_context(widget);

        gtk_style_context_save(context);
        gtk_style_context_set_state(context, GTK_STATE_FLAG_DROP_ACTIVE);
        gtk_render_frame(context, cr, 0, 0, a.width, a.height);
        gtk_style_context_restore(context);
    }

    return result;
}

// Accelerators ---------------------------------------------------------------

static void _standardview_connect_accelerators(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    if (view->accel_group == NULL)
        return;

    xfce_gtk_accel_map_add_entries(_standardview_actions,
                                   G_N_ELEMENTS(_standardview_actions));

    xfce_gtk_accel_group_connect_action_entries(
                                    view->accel_group,
                                    _standardview_actions,
                                    G_N_ELEMENTS(_standardview_actions),
                                    view);

    // as well append accelerators of derived widgets
    STANDARD_VIEW_GET_CLASS(view)->connect_accelerators(view, view->accel_group);
}

static void _standardview_disconnect_accelerators(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    if (view->accel_group == NULL)
        return;

    // Dont listen to the accel keys defined by the action entries any more
    xfce_gtk_accel_group_disconnect_action_entries(
                                        view->accel_group,
                                        _standardview_actions,
                                        G_N_ELEMENTS(_standardview_actions));

    // as well disconnect accelerators of derived widgets
    STANDARD_VIEW_GET_CLASS(view)->disconnect_accelerators(
                                                view,
                                                view->accel_group);

    // and release the accel group
    g_object_unref(view->accel_group);
    view->accel_group = NULL;
}

// Navigator Interface --------------------------------------------------------

static ThunarFile* standardview_get_current_directory(ThunarNavigator *navigator)
{
    StandardView *view = STANDARD_VIEW(navigator);
    e_return_val_if_fail(IS_STANDARD_VIEW(view), NULL);

    return view->priv->current_directory;
}

static void standardview_set_current_directory(ThunarNavigator *navigator,
                                               ThunarFile *current_directory)
{
    StandardView *view = STANDARD_VIEW(navigator);

    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(current_directory == NULL || IS_THUNARFILE(current_directory));

    // get the current directory
    if (view->priv->current_directory == current_directory)
        return;

    // disconnect any previous "loading" binding

    if (view->loading_binding != NULL)
    {
        g_object_unref(view->loading_binding);
        view->loading_binding = NULL;
    }

    // store the current scroll position
    if (current_directory != NULL)
        _standardview_scroll_position_save(view);

    // release previous directory
    if (view->priv->current_directory != NULL)
    {
        g_signal_handlers_disconnect_matched(view->priv->current_directory,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, view);
        g_object_unref(view->priv->current_directory);
    }

    // check if we want to reset the directory
    if (current_directory == NULL)
    {
        // unset
        view->priv->current_directory = NULL;

        /* resetting the folder for the model can take some time if the view has
         * to update the selection every time(i.e. closing a window with a lot of
         * selected files), so we temporarily disconnect the model from the view.
         */
        g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(view))), "model", NULL, NULL);

        // reset the folder for the model
        listmodel_set_folder(view->model, NULL);

        // reconnect the model to the view
        g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(view))),
                     "model", view->model,
                     NULL);

        // and we're done
        return;
    }

    // take ref on new directory
    view->priv->current_directory = g_object_ref(current_directory);

    g_signal_connect(G_OBJECT(current_directory), "destroy",
                     G_CALLBACK(_standardview_current_directory_destroy), view);
    g_signal_connect(G_OBJECT(current_directory), "changed",
                     G_CALLBACK(_standardview_current_directory_changed), view);

    // scroll to top-left when changing folder
    gtk_adjustment_set_value(
                gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(view)),
                0.0);
    gtk_adjustment_set_value(
                gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view)),
                0.0);

    // store the directory in the history
    navigator_set_current_directory(THUNARNAVIGATOR(view->priv->history),
                                    current_directory);

    /* We drop the model from the view as a simple optimization to speed up
     * the process of disconnecting the model data from the view.
     */
    g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(view))),
                 "model", NULL, NULL);

    // open the new directory as folder
    ThunarFolder *folder = th_folder_get_for_thfile(current_directory);

    view->loading_binding =
        g_object_bind_property_full(folder,
                                    "loading",
                                    view,
                                    "loading",
                                    G_BINDING_SYNC_CREATE,
                                    NULL,
                                    NULL,
                                    view,
                                    _standardview_loading_unbound);

    // apply the new folder
    listmodel_set_folder(view->model, folder);
    g_object_unref(G_OBJECT(folder));

    // reconnect our model to the view
    g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(view))),
                 "model", view->model,
                 NULL);

    // notify all listeners about the new/old current directory
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_CURRENT_DIRECTORY]);

    // update tab label and tooltip
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_DISPLAY_NAME]);
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_TOOLTIP_TEXT]);

    // restore the selection from the history
    _standardview_restore_selection_from_history(view);
}

static void _standardview_scroll_position_save(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // store the previous directory in the scroll hash table
    if (view->priv->current_directory == NULL)
        return;

    // only stop the first file is the scroll bar is actually moved
    GtkAdjustment *vadjustment = gtk_scrolled_window_get_vadjustment(
                                                GTK_SCROLLED_WINDOW(view));
    GtkAdjustment *hadjustment = gtk_scrolled_window_get_hadjustment(
                                                GTK_SCROLLED_WINDOW(view));
    GFile *gfile = th_file_get_file(view->priv->current_directory);

    ThunarFile *first_file;

    if (gtk_adjustment_get_value(vadjustment) == 0.0
        && gtk_adjustment_get_value(hadjustment) == 0.0)
    {
        // remove from the hash table, we already scroll to 0,0
        g_hash_table_remove(view->priv->scroll_to_files, gfile);
    }
    else if (baseview_get_visible_range(BASEVIEW(view), &first_file, NULL))
    {
        // add the file to our internal mapping of directories to scroll files
        g_hash_table_replace(view->priv->scroll_to_files,
                             g_object_ref(gfile),
                             g_object_ref(th_file_get_file(first_file)));
        g_object_unref(first_file);
    }
}

static void _standardview_current_directory_destroy(ThunarFile *current_directory,
                                                    StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(IS_THUNARFILE(current_directory));
    e_return_if_fail(view->priv->current_directory == current_directory);

    DPRINT("*** _standardview_current_directory_destroy\n");

    GError *error = NULL;

    // get a fallback directory (parents or home) we can navigate to
    ThunarFile *new_directory =
            _standardview_get_fallback_directory(current_directory, error);

    if (new_directory == NULL)
    {
        // display an error to the user
        dialog_error(GTK_WIDGET(view),
                     error, _("Failed to open the home folder"));

        g_error_free(error);

        return;
    }

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));
    window_set_current_directory(APPWINDOW(window), new_directory);

    // let the parent window update all active and inactive views (tabs)
    //window_update_directories(APPWINDOW(window), current_directory,
    //                          new_directory);

    // release the reference to the new directory
    g_object_unref(new_directory);
}

/*
 * Find a fallback directory we can navigate to if the directory gets
 * deleted. It first tries the parent folders, and finally if none can
 * be found, the home folder. If the home folder cannot be accessed,
 * the error will be stored for use by the caller.
 */
static ThunarFile* _standardview_get_fallback_directory(ThunarFile *directory,
                                                        GError     *error)
{
    e_return_val_if_fail(IS_THUNARFILE(directory), NULL);

    // determine the path of the directory
    GFile *path = g_object_ref(th_file_get_file(directory));

    ThunarFile *new_directory = NULL;

    // try to find a parent directory that still exists
    while (new_directory == NULL)
    {
        // check whether the directory exists
        if (g_file_query_exists(path, NULL))
        {
            // it does, try to load the file
            new_directory = th_file_get(path, NULL);

            // fall back to $HOME if loading the file failed
            if (new_directory == NULL)
                break;
        }
        else
        {
            // determine the parent of the directory
            GFile *tmp;
            tmp = g_file_get_parent(path);

            /* if there's no parent this means that we've found no parent
             * that still exists at all. Fall back to $HOME then */
            if (tmp == NULL)
                break;

            // free the old directory
            g_object_unref(path);

            // check the parent next
            path = tmp;
        }
    }

    // release last ref
    if (path != NULL)
        g_object_unref(path);

    if (new_directory == NULL)
    {
        // fall-back to the home directory
        path = e_file_new_for_home();
        new_directory = th_file_get(path, &error);
        g_object_unref(path);
    }

    return new_directory;
}

static void _standardview_current_directory_changed(ThunarFile *current_directory,
                                                    StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(IS_THUNARFILE(current_directory));
    e_return_if_fail(view->priv->current_directory == current_directory);

    // update tab label and tooltip
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_DISPLAY_NAME]);
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_TOOLTIP_TEXT]);
}

static void _standardview_loading_unbound(gpointer user_data)
{
    StandardView *view = STANDARD_VIEW(user_data);

    // we don't have any binding now
    view->loading_binding = NULL;

    // reset the "loading" property
    if (view->loading)
    {
        view->loading = false;
        g_object_freeze_notify(G_OBJECT(view));
        g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_LOADING]);
        _standardview_update_statusbar_text(view);
        g_object_thaw_notify(G_OBJECT(view));
    }
}

static void _standardview_restore_selection_from_history(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(IS_THUNARFILE(view->priv->current_directory));

    // reset the selected files list
    GList selected_files;
    selected_files.data = NULL;
    selected_files.prev = NULL;
    selected_files.next = NULL;

    // determine the next file in the history
    ThunarFile *selected_file;
    selected_file = history_peek_forward(view->priv->history);
    if (selected_file != NULL)
    {
        /* mark the file from history for selection if it is inside the new
         * directory */
        if (th_file_is_parent(view->priv->current_directory, selected_file))
            selected_files.data = selected_file;
        else
            g_object_unref(selected_file);
    }

    // do the same with the previous file in the history
    if (selected_files.data == NULL)
    {
        selected_file = history_peek_back(view->priv->history);
        if (selected_file != NULL)
        {
            /* mark the file from history for selection if it is inside the
             * new directory */
            if (th_file_is_parent(view->priv->current_directory, selected_file))
                selected_files.data = selected_file;
            else
                g_object_unref(selected_file);
        }
    }

    /* select the previous or next file from the history if it is inside the
     * new current directory */
    if (selected_files.data != NULL)
    {
        component_set_selected_files(THUNARCOMPONENT(view), &selected_files);
        g_object_unref(G_OBJECT(selected_files.data));
    }
}

// Component Interface --------------------------------------------------------

static GList* standardview_get_selected_files_component(ThunarComponent *component)
{
    return STANDARD_VIEW(component)->priv->selected_files;
}

static void standardview_set_selected_files_component(ThunarComponent *component,
                                                      GList *selected_files)
{
    StandardView *view = STANDARD_VIEW(component);

    // release the previous selected files list(if any)
    if (view->priv->selected_files != NULL)
    {
        e_list_free(view->priv->selected_files);
        view->priv->selected_files = NULL;
    }

    // check if we're still loading
    if (baseview_get_loading(BASEVIEW(view)))
    {
        // remember a copy of the list for later
        view->priv->selected_files = e_list_copy(selected_files);
        return;
    }

    // verify that we have a valid model
    if (view->model == NULL)
        return;

    // unselect all previously selected files
    STANDARD_VIEW_GET_CLASS(view)->unselect_all(view);

    GtkTreePath *first_path = NULL;
    GList *lp;

    // determine the tree paths for the given files
    GList *paths;
    paths = listmodel_get_paths_for_files(view->model, selected_files);
    if (paths == NULL)
        return;

    // determine the first path
    for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
    {
        // check if this path is located before the current first_path
        if (gtk_tree_path_compare(lp->data, first_path) < 0)
            first_path = lp->data;
    }

    // place the cursor on the first selected path(must be first for GtkTreeView)
    STANDARD_VIEW_GET_CLASS(view)->set_cursor(view, first_path, false);

    // select the given tree paths paths
    for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
    {
        // select the path
        STANDARD_VIEW_GET_CLASS(view)->select_path(view, lp->data);
    }

    // scroll to the first path(previously determined)
    STANDARD_VIEW_GET_CLASS(view)->scroll_to_path(view, first_path,
                                                  false, 0.0f, 0.0f);

    // release the tree paths
    g_list_free_full(paths,(GDestroyNotify) gtk_tree_path_free);
}

// BaseView Interface -------------------------------------------------------------

static GList* standardview_get_selected_files_view(BaseView *baseview)
{
    return STANDARD_VIEW(baseview)->priv->selected_files;
}

static void standardview_set_selected_files_view(BaseView *baseview,
                                                 GList    *selected_files)
{
    standardview_set_selected_files_component(THUNARCOMPONENT(baseview),
                                              selected_files);
}

static gboolean standardview_get_show_hidden(BaseView *baseview)
{
    return listmodel_get_show_hidden(STANDARD_VIEW(baseview)->model);
}

static void standardview_set_show_hidden(BaseView *baseview, gboolean show_hidden)
{
    listmodel_set_show_hidden(STANDARD_VIEW(baseview)->model, show_hidden);
}

static ThunarZoomLevel standardview_get_zoom_level(BaseView *baseview)
{
    return STANDARD_VIEW(baseview)->priv->zoom_level;
}

static void standardview_set_zoom_level(BaseView *baseview,
                                        ThunarZoomLevel zoom_level)
{
    StandardView *view = STANDARD_VIEW(baseview);

    if (view->priv->zoom_level == zoom_level)
        return;

    view->priv->zoom_level = zoom_level;

    g_object_notify_by_pspec(G_OBJECT(view),
                             _stdv_props[PROP_ZOOM_LEVEL]);
}

static gboolean standardview_get_loading(BaseView *view)
{
    return STANDARD_VIEW(view)->loading;
}

// StandardView Property
static void standardview_set_loading(StandardView *view, gboolean loading)
{
    loading = !!loading;

    // check if we're already in that state
    if (view->loading == loading)
        return;

    // apply the new state
    view->loading = loading;

    // block or unblock the insert signal to avoid queueing thumbnail reloads
    if (loading)
        g_signal_handler_block(view->model, view->priv->row_changed_id);
    else
        g_signal_handler_unblock(view->model, view->priv->row_changed_id);

    // check if we're done loading and have a scheduled scroll_to_file
    if (!loading)
    {
        ThunarFile *file;

        if (view->priv->scroll_to_file != NULL)
        {
            // remember and reset the scroll_to_file reference
            file = view->priv->scroll_to_file;
            view->priv->scroll_to_file = NULL;

            // and try again
            baseview_scroll_to_file(BASEVIEW(view), file,
                                    view->priv->scroll_to_select,
                                    view->priv->scroll_to_use_align,
                                    view->priv->scroll_to_row_align,
                                    view->priv->scroll_to_col_align);

            // cleanup
            g_object_unref(G_OBJECT(file));
        }
        else
        {
            // look for a first visible file in the hash table
            ThunarFile *current_directory;
            current_directory = navigator_get_current_directory(THUNARNAVIGATOR(view));

            if (current_directory != NULL)
            {
                GFile      *first_file;
                first_file = g_hash_table_lookup(view->priv->scroll_to_files, th_file_get_file(current_directory));
                if (first_file != NULL)
                {
                    file = th_file_cache_lookup(first_file);
                    if (file != NULL)
                    {
                        baseview_scroll_to_file(BASEVIEW(view), file, false, true, 0.0f, 0.0f);
                        g_object_unref(file);
                    }
                }
            }
        }
    }

    // check if we have a path list from new_files pending
    if (!loading && view->priv->new_files_path_list != NULL)
    {
        // remember and reset the new_files_path_list
        GList *new_files_path_list;
        new_files_path_list = view->priv->new_files_path_list;
        view->priv->new_files_path_list = NULL;

        // and try again
        _standardview_new_files(view, new_files_path_list);

        // cleanup
        e_list_free(new_files_path_list);
    }

    // check if we're done loading
    if (!loading)
    {
        // remember and reset the file list
        GList *selected_files;
        selected_files = view->priv->selected_files;
        view->priv->selected_files = NULL;

        // and try setting the selected files again
        component_set_selected_files(THUNARCOMPONENT(view), selected_files);

        // cleanup
        e_list_free(selected_files);
    }

    // notify listeners
    g_object_freeze_notify(G_OBJECT(view));
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_LOADING]);
    _standardview_update_statusbar_text(view);
    g_object_thaw_notify(G_OBJECT(view));
}

static void _standardview_new_files(StandardView *view, GList *path_list)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // release the previous "new-files" paths(if any)
    if (view->priv->new_files_path_list != NULL)
    {
        e_list_free(view->priv->new_files_path_list);
        view->priv->new_files_path_list = NULL;
    }

    // check if the folder is currently being loaded
    if (view->loading)
    {
        // schedule the "new-files" paths for later processing
        view->priv->new_files_path_list = e_list_copy(path_list);
    }
    else if (path_list != NULL)
    {
        // to check if we should reload
        GFile     *parent_file;
        parent_file = th_file_get_file(view->priv->current_directory);
        gboolean   belongs_here;
        belongs_here = false;

        GList     *file_list = NULL;

        // determine the files for the paths
        GList *lp;
        for (lp = path_list; lp != NULL; lp = lp->next)
        {
            ThunarFile *file = th_file_cache_lookup(lp->data);

            if (file != NULL)
                file_list = g_list_prepend(file_list, file);
            else if (!belongs_here && g_file_has_parent(lp->data, parent_file))
                belongs_here = true;
        }

        // check if we have any new files here
        if (file_list != NULL)
        {
            // select the files
            component_set_selected_files(THUNARCOMPONENT(view), file_list);

            // release the file list
            g_list_free_full(file_list, g_object_unref);

            // grab the focus to the view widget
            gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(view)));
        }
        else if (belongs_here)
        {
            /* thunar files are not created yet, try again later because we know
             * some of them belong in this directory, so eventually they
             * will get a ThunarFile */
            view->priv->new_files_path_list = e_list_copy(path_list);
        }
    }

    // when performing dnd between 2 views, we force a reload on the source as well
    GtkWidget *source_view;
    source_view = g_object_get_data(G_OBJECT(view), I_("source-view"));

    if (THUNAR_IS_VIEW(source_view))
        baseview_reload(BASEVIEW(source_view), false);
}

static void standardview_reload(BaseView *baseview, gboolean reload_info)
{
    StandardView *standard_view = STANDARD_VIEW(baseview);

    // determine the folder for the view model
    ThunarFolder *folder = listmodel_get_folder(standard_view->model);

    if (folder == NULL)
        return;

    ThunarFile *file = th_folder_get_thfile(folder);

    if (th_file_exists(file))
        th_folder_load(folder, reload_info);
    else
        _standardview_current_directory_destroy(file, standard_view);
}

static gboolean standardview_get_visible_range(BaseView *baseview,
                                               ThunarFile **start_file,
                                               ThunarFile **end_file)
{
    StandardView *view = STANDARD_VIEW(baseview);

    GtkTreePath *start_path;
    GtkTreePath *end_path;

    // determine the visible range as tree paths
    if (STANDARD_VIEW_GET_CLASS(view)->get_visible_range(view,
                                                         &start_path,
                                                         &end_path))
    {
        GtkTreeIter iter;

        // determine the file for the start path
        if (start_file != NULL)
        {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, start_path);
            *start_file = listmodel_get_file(view->model, &iter);
        }

        // determine the file for the end path
        if (end_file != NULL)
        {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, end_path);
            *end_file = listmodel_get_file(view->model, &iter);
        }

        // release the tree paths
        gtk_tree_path_free(start_path);
        gtk_tree_path_free(end_path);

        return true;
    }

    return false;
}

static void standardview_scroll_to_file(BaseView   *baseview,
                                        ThunarFile *file,
                                        gboolean   select_file,
                                        gboolean   use_align,
                                        gfloat     row_align,
                                        gfloat     col_align)
{
    StandardView *view = STANDARD_VIEW(baseview);
    GList               files;
    GList              *paths;

    // release the previous scroll_to_file reference(if any)
    if (view->priv->scroll_to_file != NULL)
    {
        g_object_unref(G_OBJECT(view->priv->scroll_to_file));
        view->priv->scroll_to_file = NULL;
    }

    // check if we're still loading
    if (baseview_get_loading(baseview))
    {
        // remember a reference for the new file and settings
        view->priv->scroll_to_file = THUNARFILE(g_object_ref(G_OBJECT(file)));
        view->priv->scroll_to_select = select_file;
        view->priv->scroll_to_use_align = use_align;
        view->priv->scroll_to_row_align = row_align;
        view->priv->scroll_to_col_align = col_align;
    }
    else
    {
        // fake a file list
        files.data = file;
        files.next = NULL;
        files.prev = NULL;

        // determine the path for the file
        paths = listmodel_get_paths_for_files(view->model, &files);
        if (paths != NULL)
        {
            // scroll to the path
            STANDARD_VIEW_GET_CLASS(view)->scroll_to_path(view,
                                                          paths->data,
                                                          use_align,
                                                          row_align, col_align);

            // check if we should also alter the selection
            if (select_file)
            {
                // select only the file in question
                STANDARD_VIEW_GET_CLASS(view)->unselect_all(view);
                STANDARD_VIEW_GET_CLASS(view)->select_path(view, paths->data);
            }

            // cleanup
            g_list_free_full(paths,(GDestroyNotify) gtk_tree_path_free);
        }
    }
}

static const gchar* standardview_get_statusbar_text(BaseView *baseview)
{
    StandardView *view = STANDARD_VIEW(baseview);

    e_return_val_if_fail(IS_STANDARD_VIEW(view), NULL);

    // generate the statusbar text on-demand
    if (view->priv->statusbar_text == NULL)
    {
        // query the selected items(actually a list of GtkTreePath's)
        GList *items = STANDARD_VIEW_GET_CLASS(view)->get_selected_items(view);

        /* we display a loading text if no items are
         * selected and the view is loading
         */
        if (items == NULL && view->loading)
            return _("Loading folder contents...");

        view->priv->statusbar_text = listmodel_get_statusbar_text(view->model, items);
        g_list_free_full(items,(GDestroyNotify) gtk_tree_path_free);
    }

    return view->priv->statusbar_text;
}

// standardview_constructor ---------------------------------------------------

static void _standardview_sort_column_changed(GtkTreeSortable *tree_sortable,
                                              StandardView *view)
{
    e_return_if_fail(GTK_IS_TREE_SORTABLE(tree_sortable));
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // keep the currently selected files selected after the change
    component_restore_selection(THUNARCOMPONENT(view));

    GtkSortType sort_order;
    gint sort_column;

    // determine the new sort column and sort order, and save them
    if (gtk_tree_sortable_get_sort_column_id(tree_sortable, &sort_column, &sort_order))
    {
#if 0
        // remember the new values as default
        g_object_set(G_OBJECT(view->preferences),
                      "last-sort-column", sort_column,
                      "last-sort-order", sort_order,
                      NULL);
#endif
    }
}

static gboolean _standardview_scroll_event(GtkWidget *widget,
                                           GdkEventScroll *event,
                                           StandardView *view)
{
    (void) widget;

    GdkScrollDirection scrolling_direction;

    e_return_val_if_fail(IS_STANDARD_VIEW(view), false);

    if (event->direction != GDK_SCROLL_SMOOTH)
    {
        scrolling_direction = event->direction;
    }
    else if (event->delta_y < 0)
    {
        scrolling_direction = GDK_SCROLL_UP;
    }
    else if (event->delta_y > 0)
    {
        scrolling_direction = GDK_SCROLL_DOWN;
    }
    else if (event->delta_x < 0)
    {
        scrolling_direction = GDK_SCROLL_LEFT;
    }
    else if (event->delta_x > 0)
    {
        scrolling_direction = GDK_SCROLL_RIGHT;
    }
    else
    {
        g_debug("GDK_SCROLL_SMOOTH scrolling event with no delta happened");

        return false;
    }

    // zoom-in/zoom-out on control+mouse wheel
    if ((event->state & GDK_CONTROL_MASK) != 0
        && (scrolling_direction == GDK_SCROLL_UP
            || scrolling_direction == GDK_SCROLL_DOWN))
    {
        baseview_set_zoom_level(
                    BASEVIEW(view),
                    (scrolling_direction == GDK_SCROLL_UP)
                    ? MIN(view->priv->zoom_level + 1,
                          THUNAR_ZOOM_N_LEVELS - 1)
                    : MAX(view->priv->zoom_level, 1) - 1);
        return true;
    }

    // next please...
    return false;
}

static gboolean _standardview_key_press_event(GtkWidget    *widget,
                                              GdkEventKey  *event,
                                              StandardView *view)
{
    (void) widget;

    e_return_val_if_fail(IS_STANDARD_VIEW(view), false);

    // need to catch "/" and "~" first, as the views would otherwise start interactive search
    if ((event->keyval == GDK_KEY_slash
         || event->keyval == GDK_KEY_asciitilde
         || event->keyval == GDK_KEY_dead_tilde)
        && !(event->state &(~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask())))
    {
        // popup the location selector(in whatever way)
        if (event->keyval == GDK_KEY_dead_tilde)
            g_signal_emit(G_OBJECT(view),
                          _stdv_signals[START_OPEN_LOCATION],
                          0,
                          "~");
        else
            g_signal_emit(G_OBJECT(view),
                          _stdv_signals[START_OPEN_LOCATION],
                          0,
                          event->string);

        return true;
    }

    return false;
}

// standardview_init ----------------------------------------------------------

static void _standardview_select_after_row_deleted(ListModel    *model,
                                                   GtkTreePath  *path,
                                                   StandardView *view)
{
    (void) model;

    e_return_if_fail(path != NULL);
    e_return_if_fail(IS_STANDARD_VIEW(view));

    STANDARD_VIEW_GET_CLASS(view)->set_cursor(view, path, false);
}

static void _standardview_row_changed(ListModel *model, GtkTreePath *path,
                                      GtkTreeIter *iter, StandardView *view)
{
    (void) model;
    (void) path;
    (void) iter;
    (void) view;

    return;
}

static void _standardview_rows_reordered(ListModel *model, GtkTreePath *path,
                                         GtkTreeIter *iter, gpointer new_order,
                                         StandardView *view)
{
    (void) path;
    (void) iter;
    (void) new_order;

    e_return_if_fail(IS_LISTMODEL(model));
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(view->model == model);

    /* the order of the paths might have changed, but the selection
     * stayed the same, so restore the selection of the proper files
     * after letting row changes accumulate a bit */
    if (view->priv->restore_selection_idle_id == 0)
    {
        view->priv->restore_selection_idle_id =
            g_timeout_add(50,
                          (GSourceFunc) _standardview_restore_selection_idle,
                          view);
    }
}

static gboolean _standardview_restore_selection_idle(StandardView *view)
{
    e_return_val_if_fail(IS_STANDARD_VIEW(view), false);

    // save the current scroll position and limits
    GtkAdjustment *hadjustment;
    hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(view));

    GtkAdjustment *vadjustment;
    vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view));

    gdouble h;
    gdouble hl;
    gdouble hu;
    g_object_get(G_OBJECT(hadjustment),
                 "value", &h,
                 "lower", &hl,
                 "upper", &hu,
                 NULL);

    gdouble v;
    gdouble vl;
    gdouble vu;
    g_object_get(G_OBJECT(vadjustment),
                 "value", &v,
                 "lower", &vl,
                 "upper", &vu,
                 NULL);

    // keep the current scroll position by setting the limits to the current value
    g_object_set(G_OBJECT(hadjustment),
                 "lower", h,
                 "upper", h,
                 NULL);

    g_object_set(G_OBJECT(vadjustment),
                 "lower", v,
                 "upper", v,
                 NULL);

    // restore the selection
    component_restore_selection(THUNARCOMPONENT(view));
    view->priv->restore_selection_idle_id = 0;

    // unfreeze the scroll position
    g_object_set(G_OBJECT(hadjustment),
                 "value", h,
                 "lower", hl,
                 "upper", hu,
                 NULL);

    g_object_set(G_OBJECT(vadjustment),
                 "value", v,
                 "lower", vl,
                 "upper", vu,
                 NULL);

    return false;
}

static void _standardview_error(ListModel *model, const GError *error,
                                StandardView *view)
{
    e_return_if_fail(IS_LISTMODEL(model));
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(view->model == model);

    // determine the ThunarFile for the current directory
    ThunarFile *file = navigator_get_current_directory(THUNARNAVIGATOR(view));
    if (file == NULL)
        return;

    // inform the user about the problem
    dialog_error(GTK_WIDGET(view),
                 error,
                 _("Failed to open directory \"%s\""),
                 th_file_get_display_name(file));
}

static void _standardview_update_statusbar_text(StandardView *view)
{
    // stop pending timeout
    if (view->priv->statusbar_text_idle_id != 0)
        g_source_remove(view->priv->statusbar_text_idle_id);

    /* restart a new one, this way we avoid multiple update when
     * the user is pressing a key to scroll */
    view->priv->statusbar_text_idle_id =
        g_timeout_add_full(G_PRIORITY_LOW,
                           50,
                           _standardview_update_statusbar_text_idle,
                           view,
                           NULL);
}

static gboolean _standardview_update_statusbar_text_idle(gpointer data)
{
    StandardView *view = STANDARD_VIEW(data);
    e_return_val_if_fail(IS_STANDARD_VIEW(view), false);

    UTIL_THREADS_ENTER

    // clear the current status text(will be recalculated on-demand)
    g_free(view->priv->statusbar_text);

    view->priv->statusbar_text = NULL;
    view->priv->statusbar_text_idle_id = 0;

    // tell everybody that the statusbar text may have changed
    g_object_notify_by_pspec(G_OBJECT(view),
                             _stdv_props[PROP_STATUSBAR_TEXT]);

    UTIL_THREADS_LEAVE

    return false;
}

// Public Functions -----------------------------------------------------------

ThunarHistory* standardview_get_history(StandardView *view)
{
    return view->priv->history;
}

void standardview_set_history(StandardView *view, ThunarHistory *history)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(history == NULL || IS_THUNARHISTORY(history));

    // set the new history
    g_object_unref(view->priv->history);
    view->priv->history = history;

    // connect callback
    g_signal_connect_swapped(G_OBJECT(history), "change-directory",
                             G_CALLBACK(navigator_change_directory), view);
}

void standardview_selection_changed(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // drop any existing "new-files" closure
    if (view->priv->new_files_closure != NULL)
    {
        g_closure_invalidate(view->priv->new_files_closure);
        g_closure_unref(view->priv->new_files_closure);
        view->priv->new_files_closure = NULL;
    }

    // release the previously selected files
    e_list_free(view->priv->selected_files);

    // determine the new list of selected files(replacing GtkTreePath's with ThunarFile's)
    GList *selected_files;
    selected_files =(*STANDARD_VIEW_GET_CLASS(view)->get_selected_items)(view);

    GtkTreeIter iter;
    GList *lp;
    for (lp = selected_files; lp != NULL; lp = lp->next)
    {
        // determine the iterator for the path
        gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, lp->data);

        // release the tree path...
        gtk_tree_path_free(lp->data);

        // ...and replace it with the file
        lp->data = listmodel_get_file(view->model, &iter);
    }

    // and setup the new selected files list
    view->priv->selected_files = selected_files;

    // update the statusbar text
    _standardview_update_statusbar_text(view);

    // emit notification for "selected-files"
    g_object_notify_by_pspec(G_OBJECT(view), _stdv_props[PROP_SELECTED_FILES]);
}

/* Schedules a context menu popup in response to a right-click button event.
 * Right-click events need to be handled in a special way, as the user may
 * also start a drag using the right mouse button and therefore this function
 * schedules a timer, which - once expired - opens the context menu.
 * If the user moves the mouse prior to expiration, a right-click drag
 * with GDK_ACTION_ASK, will be started instead. */
void standardview_queue_popup(StandardView *view, GdkEventButton *event)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));
    e_return_if_fail(event != NULL);

    // check if we have already scheduled a drag timer
    if (view->priv->popup_timer_id)
        return;

    // figure out the real view
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(view));

    /* we use the menu popup delay here, note that we only use this to
     * allow higher values! see bug #3549 */

    GtkSettings *settings;
    settings = gtk_settings_get_for_screen(gtk_widget_get_screen(child));

    gint delay;
    g_object_get(G_OBJECT(settings), "gtk-menu-popup-delay", &delay, NULL);

    // schedule the timer
    view->priv->popup_timer_id =
        g_timeout_add_full(G_PRIORITY_LOW,
                           MAX(225, delay),
                           _popup_timer,
                           view,
                           _popup_timer_destroy);

    // store current event data
    view->priv->drag_timer_event = gtk_get_current_event();

    // register the motion notify and the button release events on the real view
    g_signal_connect(G_OBJECT(child), "button-release-event",
                     G_CALLBACK(_on_button_release_event), view);
}

static gboolean _popup_timer(gpointer user_data)
{
    StandardView *view = STANDARD_VIEW(user_data);

    // fire up the context menu
    UTIL_THREADS_ENTER;

    standardview_context_menu(view);

    UTIL_THREADS_LEAVE;

    return false;
}

static void _popup_timer_destroy(gpointer user_data)
{
    // unregister event handlers (thread-safe)
    g_signal_handlers_disconnect_by_func(gtk_bin_get_child(GTK_BIN(user_data)),
                                         _on_button_release_event, user_data);

    // reset the drag timer source id
    STANDARD_VIEW(user_data)->priv->popup_timer_id = 0;
}

static gboolean _on_button_release_event(GtkWidget *widget,
                                         GdkEventButton *event,
                                         StandardView *view)
{
    (void) widget;
    (void) event;

    e_return_val_if_fail(IS_STANDARD_VIEW(view), false);
    e_return_val_if_fail(view->priv->popup_timer_id != 0, false);

    // cancel the pending drag timer
    g_source_remove(view->priv->popup_timer_id);

    standardview_context_menu(view);

    return true;
}

void standardview_context_menu(StandardView *view)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // grab an additional reference on the view
    g_object_ref(G_OBJECT(view));

    GList *selected_items;
    selected_items = STANDARD_VIEW_GET_CLASS(view)->get_selected_items(view);

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    AppMenu *context_menu;
    context_menu = g_object_new(TYPE_APPMENU,
                                "menu-type", MENU_TYPE_CONTEXT_STANDARD_VIEW,
                                "launcher", window_get_launcher(APPWINDOW(window)),
                                NULL);

    if (selected_items != NULL)
    {
        appmenu_add_sections(context_menu,
                             MENU_SECTION_OPEN
                             | MENU_SECTION_CUT
                             | MENU_SECTION_COPY_PASTE
                             | MENU_SECTION_TRASH_DELETE
                             | MENU_SECTION_EMPTY_TRASH
                             | MENU_SECTION_RESTORE
                             | MENU_SECTION_RENAME
                             | MENU_SECTION_TERMINAL
                             | MENU_SECTION_EXTRACT
                             | MENU_SECTION_PROPERTIES);
    }

    // right click on some empty space
    else
    {
        appmenu_add_sections(context_menu,
                             MENU_SECTION_CREATE_NEW_FILES
                             | MENU_SECTION_COPY_PASTE
                             | MENU_SECTION_EMPTY_TRASH
                             | MENU_SECTION_TERMINAL);

        _standardview_append_menu_items(view, GTK_MENU(context_menu), NULL);
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(context_menu));
        appmenu_add_sections(context_menu, MENU_SECTION_PROPERTIES);
    }

    appmenu_hide_accel_labels(context_menu);
    gtk_widget_show_all(GTK_WIDGET(context_menu));
    window_redirect_tooltips(APPWINDOW(window), GTK_MENU(context_menu));

    // if there is a drag_timer_event (long press), we use it
    if (view->priv->drag_timer_event != NULL)
    {
        etk_menu_run_at_event(GTK_MENU(context_menu),
                              view->priv->drag_timer_event);

        gdk_event_free(view->priv->drag_timer_event);
        view->priv->drag_timer_event = NULL;
    }
    else
    {
        etk_menu_run(GTK_MENU(context_menu));
    }

    g_list_free_full(selected_items,(GDestroyNotify) gtk_tree_path_free);

    // release the additional reference on the view
    g_object_unref(G_OBJECT(view));
}

static void _standardview_append_menu_items(StandardView *view,
                                            GtkMenu *menu,
                                            GtkAccelGroup *accel_group)
{
    e_return_if_fail(IS_STANDARD_VIEW(view));

    STANDARD_VIEW_GET_CLASS(view)->append_menu_items(view, menu, accel_group);
}

// Actions --------------------------------------------------------------------

static void _standardview_select_all_files(BaseView *baseview)
{
    StandardView *view = STANDARD_VIEW(baseview);
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // grab the focus to the view
    gtk_widget_grab_focus(GTK_WIDGET(view));

    // select all files in the real view
    STANDARD_VIEW_GET_CLASS(view)->select_all(view);
}

static void _standardview_select_by_pattern(BaseView *baseview)
{
    StandardView *view = STANDARD_VIEW(baseview);
    e_return_if_fail(IS_STANDARD_VIEW(view));

    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    GtkWidget *dialog = gtk_dialog_new_with_buttons(
                                        _("Select by Pattern"),
                                        GTK_WINDOW(window),
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        _("_Cancel"),
                                        GTK_RESPONSE_CANCEL,
                                        _("_Select"),
                                        GTK_RESPONSE_OK,
                                        NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 290, -1);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       vbox, true, true, 0);
    gtk_widget_show(vbox);

    GtkWidget *hbox = g_object_new(GTK_TYPE_BOX,
                                   "orientation", GTK_ORIENTATION_HORIZONTAL,
                                   "border-width", 6,
                                   "spacing", 10,
                                   NULL);

    gtk_box_pack_start(GTK_BOX(vbox), hbox, true, true, 0);
    gtk_widget_show(hbox);

    GtkWidget *label = gtk_label_new_with_mnemonic(_("_Pattern:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, false, false, 0);
    gtk_widget_show(label);

    GtkWidget *entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), true);
    gtk_box_pack_start(GTK_BOX(hbox), entry, true, true, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_widget_show(entry);

    hbox = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_HORIZONTAL, "margin-right", 6, "margin-bottom", 6, "spacing", 0, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, true, true, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new(NULL);
    gchar *example_pattern = g_strdup_printf(
                                    "<b>%s</b> %s ",
                                    _("Examples:"),
                                    "*.png, file\?\?.txt, pict*.\?\?\?");
    gtk_label_set_markup(GTK_LABEL(label), example_pattern);
    g_free(example_pattern);
    gtk_box_pack_end(GTK_BOX(hbox), label, false, false, 0);
    gtk_widget_show(label);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_OK)
    {
        // get entered pattern
        const gchar *pattern = gtk_entry_get_text(GTK_ENTRY(entry));

        gchar *pattern_extended = NULL;

        if (pattern != NULL
            && strchr(pattern, '*') == NULL
            && strchr(pattern, '?') == NULL)
        {
            // make file matching pattern
            pattern_extended = g_strdup_printf("*%s*", pattern);
            pattern = pattern_extended;
        }

        // select all files that match pattern
        GList *paths = listmodel_get_paths_for_pattern(view->model, pattern);

        STANDARD_VIEW_GET_CLASS(view)->unselect_all(view);

        // set the cursor and scroll to the first selected item
        if (paths != NULL)
            STANDARD_VIEW_GET_CLASS(view)->set_cursor(view, g_list_last(paths)->data, false);

        for (GList *lp = paths; lp != NULL; lp = lp->next)
        {
            STANDARD_VIEW_GET_CLASS(view)->select_path(view, lp->data);
            gtk_tree_path_free(lp->data);
        }

        g_list_free(paths);
        g_free(pattern_extended);
    }

    gtk_widget_destroy(dialog);
}

static void _standardview_selection_invert(BaseView *baseview)
{
    StandardView *view = STANDARD_VIEW(baseview);

    e_return_if_fail(IS_STANDARD_VIEW(view));

    // grab the focus to the view
    gtk_widget_grab_focus(GTK_WIDGET(view));

    // invert all files in the real view
    STANDARD_VIEW_GET_CLASS(view)->selection_invert(view);
}

// DnD Source -----------------------------------------------------------------

static void _on_drag_begin(GtkWidget *widget, GdkDragContext *context,
                           StandardView *view)
{
    (void) widget;

    // release in case the drag-end wasn't fired before
    e_list_free(view->priv->drag_g_file_list);

    view->priv->drag_g_file_list =
                th_filelist_to_thunar_g_file_list(view->priv->selected_files);

    if (view->priv->drag_g_file_list == NULL)
        return;

    // generate an icon from the first selected file

    ThunarFile *file = th_file_get(view->priv->drag_g_file_list->data, NULL);

    if (file == NULL)
        return;

    gint size;
    g_object_get(G_OBJECT(view->icon_renderer), "size", &size, NULL);

    GdkPixbuf *icon = iconfact_load_file_icon(view->icon_factory,
                                              file,
                                              FILE_ICON_STATE_DEFAULT,
                                              size);

    gtk_drag_set_icon_pixbuf(context, icon, 0, 0);

    g_object_unref(icon);
    g_object_unref(file);
}

static void _on_drag_data_get(GtkWidget *widget, GdkDragContext *context,
                              GtkSelectionData *seldata,
                              guint info, guint timestamp,
                              StandardView *view)
{
    (void) widget;
    (void) context;
    (void) info;
    (void) timestamp;

    // set the URI list for the drag selection
    if (view->priv->drag_g_file_list == NULL)
        return;

    gchar **uris = e_filelist_to_stringv(view->priv->drag_g_file_list);
    gtk_selection_data_set_uris(seldata, uris);
    g_strfreev(uris);
}

static void _on_drag_data_delete(GtkWidget *widget, GdkDragContext *context,
                                 StandardView *view)
{
    (void) context;
    (void) view;

    // make sure the default handler of ExoIconView/GtkTreeView is never run

    g_signal_stop_emission_by_name(G_OBJECT(widget), "drag-data-delete");
}

static void _on_drag_end(GtkWidget *widget, GdkDragContext *context,
                         StandardView *view)
{
    (void) widget;
    (void) context;

    // stop any running drag autoscroll timer
    if (view->priv->drag_scroll_timer_id != 0)
        g_source_remove(view->priv->drag_scroll_timer_id);

    // release the list of dragged URIs
    e_list_free(view->priv->drag_g_file_list);
    view->priv->drag_g_file_list = NULL;
}

// DnD Target -----------------------------------------------------------------

static gboolean _on_drag_motion(GtkWidget *widget, GdkDragContext *context,
                                gint x, gint y, guint timestamp,
                                StandardView *view)
{
    // request the drop data on-demand (if we don't have it already)
    if (!view->priv->drop_data_ready)
    {
        GdkDragAction action = 0;

        // check if we can handle that drag data(yet?)
        GdkAtom target = gtk_drag_dest_find_target(widget, context, NULL);

        if (target == gdk_atom_intern_static_string("_NETSCAPE_URL"))
        {
            // determine the file for the given coordinates
            GtkTreePath *treepath;
            ThunarFile *file = _standardview_get_drop_file(view, x, y, &treepath);

            // check if we can save here
            if (file
                && th_file_is_local(file)
                && th_file_is_directory(file)
                && th_file_is_writable(file))
            {
                action = gdk_drag_context_get_suggested_action(context);
            }

            // reset path if we cannot drop
            if (action == 0 && treepath != NULL)
            {
                gtk_tree_path_free(treepath);
                treepath = NULL;
            }

            // do the view highlighting
            if (view->priv->drop_highlight != (treepath == NULL && action != 0))
            {
                view->priv->drop_highlight = (treepath == NULL && action != 0);
                gtk_widget_queue_draw(GTK_WIDGET(view));
            }

            // setup drop-file for the icon renderer to highlight the target
            g_object_set(G_OBJECT(view->icon_renderer),
                         "drop-file", (action != 0) ? file : NULL,
                         NULL);

            // do the item highlighting
            STANDARD_VIEW_GET_CLASS(view)->highlight_path(view, treepath);

            if (file)
                g_object_unref(G_OBJECT(file));

            if (treepath)
                gtk_tree_path_free(treepath);
        }
        else
        {
            // request the drag data from the source
            if (target != GDK_NONE)
                gtk_drag_get_data(widget, context, target, timestamp);
        }

        // tell Gdk whether we can drop here
        gdk_drag_status(context, action, timestamp);
    }
    else
    {
        // check whether we can drop at (x,y)
        _standardview_get_dest_actions(view, context, x, y, timestamp, NULL);
    }

    // start the drag autoscroll timer if not already running
    if (view->priv->drag_scroll_timer_id != 0)
        return true;

    // schedule the drag autoscroll timer
    view->priv->drag_scroll_timer_id =
                g_timeout_add_full(G_PRIORITY_LOW,
                                   50,
                                   _standardview_drag_scroll_timer,
                                   view,
                                   _standardview_drag_scroll_timer_destroy);

    return true;
}

static ThunarFile* _standardview_get_drop_file(StandardView *view,
                                               gint x, gint y,
                                               GtkTreePath **path_return)
{
    // determine the path for the given coordinates
    GtkTreePath *treepath;
    treepath = STANDARD_VIEW_GET_CLASS(view)->get_path_at_pos(view, x, y);

    ThunarFile *file = NULL;

    if (treepath)
    {
        // determine the file for the path

        GtkTreeIter iter;
        gtk_tree_model_get_iter(GTK_TREE_MODEL(view->model), &iter, treepath);
        file = listmodel_get_file(view->model, &iter);

        // we can only drop to directories and executable files
        if (!th_file_is_directory(file) && !th_file_is_executable(file))
        {
            // drop to the folder instead
            g_object_unref(file);
            gtk_tree_path_free(treepath);
            treepath = NULL;
        }
    }

    // if we don't have a path yet, we'll drop to the folder instead
    if (!treepath)
    {
        // determine the current directory
        file = navigator_get_current_directory(THUNARNAVIGATOR(view));

        if (file)
            g_object_ref(file);
    }

    // return the path(if any)
    if (path_return)
        *path_return = treepath;

    else if (treepath)
        gtk_tree_path_free(treepath);

    return file;
}

static GdkDragAction _standardview_get_dest_actions(StandardView *view,
                                                    GdkDragContext *context,
                                                    gint x, gint y,
                                                    guint timestamp,
                                                    ThunarFile **file_return)
{
    // determine the file and path for the given coordinates
    GtkTreePath *treepath;
    ThunarFile *file = _standardview_get_drop_file(view, x, y, &treepath);

    GdkDragAction actions = 0;
    GdkDragAction action = 0;

    // check if we can drop there
    if (file)
    {
        // determine the possible drop actions for the file(and the suggested action if any)
        actions = th_file_accepts_drop(file,
                                       view->priv->drop_file_list,
                                       context, &action);
        if (actions != 0)
        {
            // tell the caller about the file(if it's interested)
            if (file_return != NULL)
                *file_return = THUNARFILE(g_object_ref(file));
        }
    }

    // reset path if we cannot drop
    if (action == 0 && treepath != NULL)
    {
        gtk_tree_path_free(treepath);
        treepath = NULL;
    }

    /* setup the drop-file for the icon renderer, so the user
     * gets good visual feedback for the drop target.
     */
    g_object_set(G_OBJECT(view->icon_renderer),
                 "drop-file", (action != 0) ? file : NULL,
                 NULL);

    // do the view highlighting
    if (view->priv->drop_highlight !=(treepath == NULL && action != 0))
    {
        view->priv->drop_highlight =(treepath == NULL && action != 0);
        gtk_widget_queue_draw(GTK_WIDGET(view));
    }

    // do the item highlighting
    STANDARD_VIEW_GET_CLASS(view)->highlight_path(view, treepath);

    // tell Gdk whether we can drop here
    gdk_drag_status(context, action, timestamp);

    // clean up
    if (file != NULL)
        g_object_unref(G_OBJECT(file));

    if (treepath != NULL)
        gtk_tree_path_free(treepath);

    return actions;
}

static gboolean _standardview_drag_scroll_timer(gpointer user_data)
{
    StandardView *view = STANDARD_VIEW(user_data);

    UTIL_THREADS_ENTER

    // verify that we are realized
    if (gtk_widget_get_realized(GTK_WIDGET(view)))
    {
        // determine pointer location and window geometry
        GdkWindow *window = gtk_widget_get_window(
                                gtk_bin_get_child(GTK_BIN(view)));

        GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
        GdkDevice *pointer = gdk_seat_get_pointer(seat);

        gint x;
        gint y;
        gdk_window_get_device_position(window, pointer, &x, &y, NULL);

        gint w;
        gint h;
        gdk_window_get_geometry(window, NULL, NULL, &w, &h);

        // check if we are near the edge (vertical)
        gint offset = y - (2 * 20);

        if (offset > 0)
            offset = MAX(y - (h - 2 * 20), 0);

        GtkAdjustment *adjustment;
        gfloat value;

        // change the vertical adjustment appropriately
        if (offset != 0)
        {
            // determine the vertical adjustment
            adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view));

            // determine the new value
            value = CLAMP(gtk_adjustment_get_value(adjustment) + 2 * offset,
                          gtk_adjustment_get_lower(adjustment),
                          gtk_adjustment_get_upper(adjustment)
                              - gtk_adjustment_get_page_size(adjustment));

            // apply the new value
            gtk_adjustment_set_value(adjustment, value);
        }

        // check if we are near the edge (horizontal)
        offset = x - (2 * 20);

        if (offset > 0)
            offset = MAX(x -(w - 2 * 20), 0);

        // change the horizontal adjustment appropriately
        if (offset != 0)
        {
            // determine the vertical adjustment
            adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(view));

            // determine the new value
            value = CLAMP(gtk_adjustment_get_value(adjustment) + 2 * offset, gtk_adjustment_get_lower(adjustment), gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));

            // apply the new value
            gtk_adjustment_set_value(adjustment, value);
        }
    }

    UTIL_THREADS_LEAVE

    return true;
}

static void _standardview_drag_scroll_timer_destroy(gpointer user_data)
{
    STANDARD_VIEW(user_data)->priv->drag_scroll_timer_id = 0;
}

static gboolean _on_drag_drop(GtkWidget *widget, GdkDragContext *context,
                              gint x, gint y, guint timestamp,
                              StandardView *view)
{
    (void) x;
    (void) y;

    GdkAtom target = gtk_drag_dest_find_target(widget, context, NULL);

    if (target == GDK_NONE)
        return false;

    /* set state so the drag-data-received knows that
     * this is really a drop this time. */
    view->priv->drop_occurred = true;

    // request the drag data from the source
    gtk_drag_get_data(widget, context, target, timestamp);

    // we'll call gtk_drag_finish() later

    return true;
}

static void _on_drag_data_received(GtkWidget *widget, GdkDragContext *context,
                                   gint x, gint y,
                                   GtkSelectionData *seldata,
                                   guint info, guint timestamp,
                                   StandardView *view)
{
    // check if we don't already know the drop data
    if (!view->priv->drop_data_ready)
    {
        // extract the URI list from the selection data(if valid)
        if (info == TARGET_TEXT_URI_LIST
            && gtk_selection_data_get_format(seldata) == 8
            && gtk_selection_data_get_length(seldata) > 0)
        {
            const guchar *selstr = gtk_selection_data_get_data(seldata);
            view->priv->drop_file_list =
                e_filelist_new_from_string((gchar*) selstr);
        }

        // reset the state
        view->priv->drop_data_ready = true;
    }

    if (view->priv->drop_occurred == false)
        return;

    view->priv->drop_occurred = false;

    //ThunarFile *file = NULL;
    gboolean succeed = false;

    if (info == TARGET_TEXT_URI_LIST)
    {
        succeed = _received_text_uri_list(context, x, y, timestamp, view);
    }
    else if (info == TARGET_NETSCAPE_URL)
    {
        succeed = _received_netscape_url(widget, x, y, seldata, view);
    }

    // tell the peer that we handled the drop
    gtk_drag_finish(context, succeed, false, timestamp);

    // disable the highlighting and release the drag data
    _on_drag_leave(widget, context, timestamp, view);
}

static bool _received_text_uri_list(GdkDragContext *context,
                                    gint x, gint y, guint timestamp,
                                    StandardView *view)
{
    ThunarFile *file = NULL;
    gboolean succeed = false;

    // determine the drop position
    GdkDragAction actions =
        _standardview_get_dest_actions(view, context, x, y, timestamp, &file);

    if ((actions & (GDK_ACTION_COPY
                    | GDK_ACTION_MOVE
                    | GDK_ACTION_LINK)) != 0)
    {
        // ask the user what to do with the drop data
        GdkDragAction action =
            (gdk_drag_context_get_selected_action(context) == GDK_ACTION_ASK)
            ? dnd_ask(GTK_WIDGET(view), file,
                      view->priv->drop_file_list, actions)
            : gdk_drag_context_get_selected_action(context);

        // perform the requested action
        if (action != 0)
        {
            // look if we can find the drag source widget
            GtkWidget *source_widget = gtk_drag_get_source_widget(context);

            GtkWidget *source_view = NULL;

            if (source_widget != NULL)
            {
                /* if this is a source view, attach it to the view receiving
                 * the data, see thunar_standardview_new_files */
                source_view = gtk_widget_get_parent(source_widget);

                if (!THUNAR_IS_VIEW(source_view))
                    source_view = NULL;
            }

            succeed = dnd_perform(
                        GTK_WIDGET(view),
                        file,
                        view->priv->drop_file_list,
                        action,
                        _standardview_new_files_closure(view, source_view));
        }
    }

    // release the file reference
    if (file)
        g_object_unref(file);

    return succeed;
}

static GClosure* _standardview_new_files_closure(StandardView *view,
                                                 GtkWidget *source_view)
{
    e_return_val_if_fail(source_view == NULL || THUNAR_IS_VIEW(source_view), NULL);

    // drop any previous "new-files" closure
    if (view->priv->new_files_closure != NULL)
    {
        g_closure_invalidate(view->priv->new_files_closure);
        g_closure_unref(view->priv->new_files_closure);
    }

    // set the remove view data we possibly need to reload
    g_object_set_data(G_OBJECT(view), I_("source-view"), source_view);

    // allocate a new "new-files" closure
    view->priv->new_files_closure =
        g_cclosure_new_swap(G_CALLBACK(_standardview_new_files), view, NULL);

    g_closure_ref(view->priv->new_files_closure);
    g_closure_sink(view->priv->new_files_closure);

    // and return our new closure
    return view->priv->new_files_closure;
}

static bool _received_netscape_url(GtkWidget *widget,
                                   gint x, gint y,
                                   GtkSelectionData *seldata,
                                   StandardView *view)
{
    // check if the format is valid and we have any data
    if (gtk_selection_data_get_format(seldata) != 8
        || gtk_selection_data_get_length(seldata) < 1)
    {
        return false;
    }

    // _NETSCAPE_URL looks like this: "$URL\n$TITLE"
    gchar **bits =
        g_strsplit((const gchar*) gtk_selection_data_get_data(seldata),
                   "\n", -1);

    if (g_strv_length(bits) != 2)
    {
        g_strfreev(bits);
        return false;
    }

    // determine the file for the drop position
    ThunarFile *file = _standardview_get_drop_file(view, x, y, NULL);

    if (file == NULL)
    {
        g_strfreev(bits);
        return false;
    }

    // determine the absolute path to the target directory
    gchar *working_directory = g_file_get_path(th_file_get_file(file));

    if (working_directory == NULL)
    {
        g_strfreev(bits);
        return false;
    }

    // prepare the basic part of the command
    gchar *argv[11];
    gint n = 0;
    argv[n++] = "exo-desktop-item-edit";
    argv[n++] = "--type=Link";
    argv[n++] = "--url";
    argv[n++] = bits[0];
    argv[n++] = "--name";
    argv[n++] = bits[1];

    // determine the toplevel window
    GtkWidget *toplevel = gtk_widget_get_toplevel(widget);

    if (toplevel != NULL && gtk_widget_is_toplevel(toplevel))
    {

#if defined(GDK_WINDOWING_X11)
        // on X11, we can supply the parent window id here
        argv[n++] = "--xid";
        argv[n++] = g_newa(gchar, 32);

        GdkWindow *gdk_window = gtk_widget_get_window(GTK_WIDGET(toplevel));
        g_snprintf(argv[n - 1],
                   32,
                   "%ld",
                   (glong) GDK_WINDOW_XID(gdk_window));
#endif

    }

    // terminate the parameter list
    argv[n++] = "--create-new";
    argv[n++] = working_directory;
    argv[n++] = NULL;

    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(widget));

    char *display = NULL;

    if (screen != NULL)
        display = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));

    // try to run exo-desktop-item-edit
    gint pid;
    GError *error = NULL;

    gboolean succeed = false;
    succeed = g_spawn_async(working_directory,
                            argv, NULL,
                            G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                            util_set_display_env,
                            display,
                            &pid, &error);

    if (!succeed)
    {
        // display an error dialog to the user
        dialog_error(view,
                     error,
                     _("Failed to create a link for the URL \"%s\""),
                     bits[0]);

        g_free(working_directory);
        g_error_free(error);
    }
    else
    {
        // reload the directory when the command terminates
        g_child_watch_add_full(G_PRIORITY_LOW,
                               pid,
                               _reload_directory,
                               working_directory,
                               g_free);
    }

    // cleanup
    g_free(display);

    g_object_unref(file);
    g_strfreev(bits);

    return succeed;
}

static void _reload_directory(GPid pid, gint status, gpointer user_data)
{
    (void) pid;
    (void) status;

    // determine the path for the directory
    GFile *file = g_file_new_for_uri(user_data);

    // schedule a changed event for the directory
    GFileMonitor *monitor = g_file_monitor(file, G_FILE_MONITOR_NONE, NULL, NULL);

    if (monitor != NULL)
    {
        g_file_monitor_emit_event(monitor, file, NULL, G_FILE_MONITOR_EVENT_CHANGED);
        g_object_unref(monitor);
    }

    g_object_unref(file);
}

static void _on_drag_leave(GtkWidget *widget, GdkDragContext *context,
                           guint timestamp, StandardView *view)
{
    (void) widget;
    (void) context;
    (void) timestamp;

    // reset the drop-file for the icon renderer
    g_object_set(G_OBJECT(view->icon_renderer),
                 "drop-file", NULL,
                 NULL);

    // stop any running drag autoscroll timer
    if (view->priv->drag_scroll_timer_id != 0)
        g_source_remove(view->priv->drag_scroll_timer_id);

    // disable the drop highlighting around the view
    if (view->priv->drop_highlight)
    {
        view->priv->drop_highlight = false;
        gtk_widget_queue_draw(GTK_WIDGET(view));
    }

    // reset the "drop data ready" status and free the URI list
    if (view->priv->drop_data_ready)
    {
        e_list_free(view->priv->drop_file_list);
        view->priv->drop_file_list = NULL;
        view->priv->drop_data_ready = false;
    }

    // disable the highlighting of the items in the view
    STANDARD_VIEW_GET_CLASS(view)->highlight_path(view, NULL);
}


