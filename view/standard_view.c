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
#include <standard_view.h>

#include <memory.h>
#include <string.h>
#include <gdk/gdkkeysyms.h>

#if defined(GDK_WINDOWING_X11)
#include <gdk/gdkx.h>
#endif

#include <utils.h>

#include <application.h>
#include <appmenu.h>
#include <dialogs.h>
#include <dnd.h>
#include <enum_types.h>
#include <gio_ext.h>
#include <gtk_ext.h>
#include <history.h>
#include <icon_render.h>
#include <launcher.h>
#include <marshal.h>
#include <pango_ext.h>
#include <propsdlg.h>
#include <simple_job.h>

// Property identifiers
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

// Signal identifiers
enum
{
    START_OPEN_LOCATION,
    LAST_SIGNAL,
};

// Identifiers for DnD target types
enum
{
    TARGET_TEXT_URI_LIST,
    TARGET_XDND_DIRECT_SAVE0,
    TARGET_NETSCAPE_URL,
};

// Allocation -----------------------------------------------------------------

static void standard_view_navigator_init(ThunarNavigatorIface *iface);
static void standard_view_component_init(ThunarComponentIface *iface);
static void standard_view_view_init(BaseViewIface *iface);

// Object Class ---------------------------------------------------------------

static GObject* standard_view_constructor(
                                    GType type,
                                    guint n_construct_properties,
                                    GObjectConstructParam *construct_properties);
static void standard_view_dispose(GObject *object);
static void standard_view_finalize(GObject *object);
static void standard_view_get_property(GObject *object, guint prop_id,
                                       GValue *value, GParamSpec *pspec);
static void standard_view_set_property(GObject *object, guint prop_id,
                                       const GValue *value, GParamSpec *pspec);

// Widget Class ---------------------------------------------------------------

static void standard_view_realize(GtkWidget *widget);
static void standard_view_unrealize(GtkWidget *widget);
static void standard_view_grab_focus(GtkWidget *widget);
static gboolean standard_view_draw(GtkWidget *widget, cairo_t *cr);

// Accelerators ---------------------------------------------------------------

static void _standard_view_connect_accelerators(StandardView *standard_view);
static void _standard_view_disconnect_accelerators(StandardView *standard_view);

// Navigator Interface --------------------------------------------------------

static ThunarFile* standard_view_get_current_directory(ThunarNavigator *navigator);
static void standard_view_set_current_directory(ThunarNavigator *navigator,
                                                ThunarFile *current_directory);
static void _standard_view_loading_unbound(gpointer user_data);
static void _standard_view_scroll_position_save(
                                        StandardView *standard_view);
static void _standard_view_restore_selection_from_history(
                                                StandardView *standard_view);

// Component Interface --------------------------------------------------------

static GList* standard_view_get_selected_files_component(ThunarComponent *component);
static void standard_view_set_selected_files_component(ThunarComponent *component,
                                                       GList *selected_files);

// View Interface -------------------------------------------------------------

static GList* standard_view_get_selected_files_view(BaseView *view);
static void standard_view_set_selected_files_view(BaseView *view,
                                                  GList *selected_files);

static gboolean standard_view_get_show_hidden(BaseView *view);
static void standard_view_set_show_hidden(BaseView *view, gboolean show_hidden);

static ThunarZoomLevel standard_view_get_zoom_level(BaseView *view);
static void standard_view_set_zoom_level(BaseView *view,
                                         ThunarZoomLevel zoom_level);

static gboolean standard_view_get_loading(BaseView *view);
static void standard_view_set_loading(StandardView *standard_view,
                                      gboolean loading);
static void _standard_view_new_files(StandardView *standard_view,
                                     GList *path_list);
static GClosure *_standard_view_new_files_closure(
                                            StandardView *standard_view,
                                            GtkWidget *source_view);

static void standard_view_reload(BaseView *view, gboolean reload_info);
static gboolean standard_view_get_visible_range(BaseView *view,
                                                ThunarFile **start_file,
                                                ThunarFile **end_file);
static void standard_view_scroll_to_file(BaseView *view,
                                         ThunarFile *file,
                                         gboolean select_file,
                                         gboolean use_align,
                                         gfloat row_align,
                                         gfloat col_align);
static const gchar* standard_view_get_statusbar_text(BaseView *view);

// Public Functions -----------------------------------------------------------

static void _standard_view_append_menu_items(StandardView *standard_view,
                                             GtkMenu *menu,
                                             GtkAccelGroup *accel_group);
static gboolean _standard_view_drag_timer(gpointer user_data);
static void _standard_view_drag_timer_destroy(gpointer user_data);

// ----------------------------------------------------------------------------

static void _standard_view_sort_column_changed(
                                            GtkTreeSortable *tree_sortable,
                                            StandardView *standard_view);

static gboolean _standard_view_scroll_event(GtkWidget *view,
                                            GdkEventScroll *event,
                                            StandardView *standard_view);

static gboolean _standard_view_key_press_event(
                                            GtkWidget *view,
                                            GdkEventKey *event,
                                            StandardView *standard_view);

static void _standard_view_scrolled(GtkAdjustment *adjustment,
                                    StandardView *standard_view);

// ----------------------------------------------------------------------------

static void _standard_view_select_after_row_deleted(
                                            ListModel *model,
                                            GtkTreePath *path,
                                            StandardView *standard_view);

static void _standard_view_row_changed(ListModel *model,
                                       GtkTreePath *path,
                                       GtkTreeIter *iter,
                                       StandardView *standard_view);

static void _standard_view_rows_reordered(ListModel *model,
                                          GtkTreePath *path,
                                          GtkTreeIter *iter,
                                          gpointer new_order,
                                          StandardView *standard_view);
static gboolean _standard_view_restore_selection_idle(
                                            StandardView *standard_view);

static void _standard_view_error(ListModel *model,
                                 const GError *error,
                                 StandardView *standard_view);

static void _standard_view_update_statusbar_text(StandardView *standard_view);

static void _standard_view_size_allocate(StandardView *standard_view,
                                         GtkAllocation *allocation);

// ----------------------------------------------------------------------------

static void _standard_view_current_directory_destroy(
                                        ThunarFile *current_directory,
                                        StandardView *standard_view);
static void _standard_view_current_directory_changed(
                                        ThunarFile *current_directory,
                                        StandardView *standard_view);
static ThunarFile* _standard_view_get_fallback_directory(
                                                        ThunarFile *directory,
                                                        GError     *error);

// ----------------------------------------------------------------------------

static gboolean _standard_view_button_release_event(
                                            GtkWidget *view,
                                            GdkEventButton *event,
                                            StandardView *standard_view);

static gboolean _standard_view_motion_notify_event(
                                            GtkWidget *view,
                                            GdkEventMotion *event,
                                            StandardView *standard_view);

// Actions --------------------------------------------------------------------

static void _standard_view_select_all_files(BaseView *view);
static void _standard_view_select_by_pattern(BaseView *view);
static void _standard_view_selection_invert(BaseView *view);

// DnD Source -----------------------------------------------------------------

static void _standard_view_drag_begin(GtkWidget *view,
                                      GdkDragContext *context,
                                      StandardView *standard_view);
static void _standard_view_drag_data_get(GtkWidget *view,
                                         GdkDragContext *context,
                                         GtkSelectionData *selection_data,
                                         guint info,
                                         guint timestamp,
                                         StandardView *standard_view);
static void _standard_view_drag_data_delete(GtkWidget *view,
                                            GdkDragContext *context,
                                            StandardView *standard_view);
static void _standard_view_drag_end(GtkWidget *view,
                                    GdkDragContext *context,
                                    StandardView *standard_view);

// DnD Target -----------------------------------------------------------------

static void _standard_view_drag_leave(GtkWidget *view,
                                      GdkDragContext *context,
                                      guint timestamp,
                                      StandardView *standard_view);
static gboolean _standard_view_drag_motion(GtkWidget *view,
                                           GdkDragContext *context,
                                           gint x,
                                           gint y,
                                           guint timestamp,
                                           StandardView *standard_view);
static gboolean _standard_view_drag_drop(GtkWidget *view,
                                         GdkDragContext *context,
                                         gint x,
                                         gint y,
                                         guint timestamp,
                                         StandardView *standard_view);
static void _standard_view_drag_data_received(GtkWidget *view,
                                              GdkDragContext *context,
                                              gint x,
                                              gint y,
                                              GtkSelectionData *selection_data,
                                              guint info,
                                              guint timestamp,
                                              StandardView *standard_view);

static ThunarFile* _standard_view_get_drop_file(
                                            StandardView *standard_view,
                                            gint x,
                                            gint y,
                                            GtkTreePath **path_return);
static GdkDragAction _standard_view_get_dest_actions(
                                            StandardView *standard_view,
                                            GdkDragContext *context,
                                            gint x,
                                            gint y,
                                            guint timestamp,
                                            ThunarFile **file_return);

static void _standard_view_reload_directory(GPid pid, gint status,
                                            gpointer user_data);

static gboolean _standard_view_drag_scroll_timer(gpointer user_data);
static void _standard_view_drag_scroll_timer_destroy(gpointer user_data);


// Actions --------------------------------------------------------------------

static XfceGtkActionEntry _standard_view_actions[] =
{
    {STANDARD_VIEW_ACTION_SELECT_ALL_FILES,
     "<Actions>/StandardView/select-all-files",
     "<Primary>a",
     XFCE_GTK_MENU_ITEM,
     N_("Select _all Files"),
     N_("Select all files in this window"),
     NULL,
     G_CALLBACK(_standard_view_select_all_files)},

    {STANDARD_VIEW_ACTION_SELECT_BY_PATTERN,
     "<Actions>/StandardView/select-by-pattern",
     "<Primary>s",
     XFCE_GTK_MENU_ITEM,
     N_("Select _by Pattern..."),
     N_("Select all files that match a certain pattern"),
     NULL,
     G_CALLBACK(_standard_view_select_by_pattern)},

    {STANDARD_VIEW_ACTION_INVERT_SELECTION,
     "<Actions>/StandardView/invert-selection",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Invert Selection"),
     N_("Select all files but not those currently selected"),
     NULL,
     G_CALLBACK(_standard_view_selection_invert)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_standard_view_actions, \
                                    G_N_ELEMENTS(_standard_view_actions), \
                                    id)

// Target types for dragging from the view
static const GtkTargetEntry _drag_targets[] =
{
    {"text/uri-list",   0, TARGET_TEXT_URI_LIST},
};

// Target types for dropping to the view
static const GtkTargetEntry _drop_targets[] =
{
    {"text/uri-list",   0, TARGET_TEXT_URI_LIST},
    {"XdndDirectSave0", 0, TARGET_XDND_DIRECT_SAVE0},
    {"_NETSCAPE_URL",   0, TARGET_NETSCAPE_URL},
};


// Allocation -----------------------------------------------------------------

static guint _standard_view_signals[LAST_SIGNAL];
static GParamSpec *_standard_view_props[N_PROPERTIES] = { NULL, };

struct _StandardViewPrivate
{
    // current directory of the view
    ThunarFile      *current_directory;

    // history of the current view
    ThunarHistory   *history;

    // zoom-level support
    ThunarZoomLevel zoom_level;

    // scroll_to_file support
    GHashTable  *scroll_to_files;

    // statusbar
    gchar       *statusbar_text;
    guint       statusbar_text_idle_id;

    // right-click drag/popup support
    GList       *drag_g_file_list;
    guint       drag_scroll_timer_id;
    guint       drag_timer_id;
    GdkEvent    *drag_timer_event;
    gint        drag_x;
    gint        drag_y;

    // drop site support
    guint       drop_data_ready : 1;    // whether the drop data was received already
    guint       drop_highlight : 1;
    guint       drop_occurred : 1;      // whether the data was dropped
    GList       *drop_file_list;        // the list of URIs that are contained in the drop data */

    /* the "new-files" closure, which is used to select files whenever
     * new files are created by a ThunarJob associated with this view
     */
    GClosure    *new_files_closure;

    /* the "new-files" path list that was remembered in the closure callback
     * if the view is currently being loaded and as such the folder may
     * not have all "new-files" at hand. This list is used when the
     * folder tells that it's ready loading and the view will try again
     * to select exactly these files.
     */
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

G_DEFINE_ABSTRACT_TYPE_WITH_CODE(StandardView,
                                 standard_view,
                                 GTK_TYPE_SCROLLED_WINDOW,
                                 G_IMPLEMENT_INTERFACE(
                                     THUNAR_TYPE_NAVIGATOR,
                                     standard_view_navigator_init)
                                 G_IMPLEMENT_INTERFACE(
                                     THUNAR_TYPE_COMPONENT,
                                     standard_view_component_init)
                                 G_IMPLEMENT_INTERFACE(
                                     TYPE_BASEVIEW,
                                     standard_view_view_init)
                                 G_ADD_PRIVATE(StandardView))

static void standard_view_class_init(StandardViewClass *klass)
{
    GtkWidgetClass *gtkwidget_class;
    GObjectClass   *gobject_class;
    gpointer       g_iface;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructor = standard_view_constructor;
    gobject_class->dispose = standard_view_dispose;
    gobject_class->finalize = standard_view_finalize;
    gobject_class->get_property = standard_view_get_property;
    gobject_class->set_property = standard_view_set_property;

    gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->realize = standard_view_realize;
    gtkwidget_class->unrealize = standard_view_unrealize;
    gtkwidget_class->grab_focus = standard_view_grab_focus;
    gtkwidget_class->draw = standard_view_draw;

    xfce_gtk_translate_action_entries(_standard_view_actions,
                                      G_N_ELEMENTS(_standard_view_actions));

    /**
     * StandardView:loading:
     *
     * Whether the folder associated with this view is
     * currently being loaded from the underlying media.
     *
     * Override property to set the property as writable
     * for the binding.
     **/
    _standard_view_props[PROP_LOADING] =
        g_param_spec_override("loading",
                              g_param_spec_boolean("loading",
                                                   "loading",
                                                   "loading",
                                                   FALSE,
                                                   E_PARAM_READWRITE));

    // Display name of the current directory, for label text
    _standard_view_props[PROP_DISPLAY_NAME] =
        g_param_spec_string("display-name",
                            "display-name",
                            "display-name",
                            NULL,
                            E_PARAM_READABLE);

    // Full parsed name of the current directory, for label tooltip
    _standard_view_props[PROP_TOOLTIP_TEXT] =
        g_param_spec_string("tooltip-text",
                            "tooltip-text",
                            "tooltip-text",
                            NULL,
                            E_PARAM_READABLE);

    // Whether to use directory specific settings.
    _standard_view_props[PROP_DIRECTORY_SPECIFIC_SETTINGS] =
        g_param_spec_boolean("directory-specific-settings",
                             "directory-specific-settings",
                             "directory-specific-settings",
                             FALSE,
                             E_PARAM_READWRITE);

    // override ThunarComponent's properties
    g_iface = g_type_default_interface_peek(THUNAR_TYPE_COMPONENT);
    _standard_view_props[PROP_SELECTED_FILES] =
        g_param_spec_override("selected-files",
                              g_object_interface_find_property(g_iface,
                                                               "selected-files"));

    // override ThunarNavigator's properties
    g_iface = g_type_default_interface_peek(THUNAR_TYPE_NAVIGATOR);
    _standard_view_props[PROP_CURRENT_DIRECTORY] =
        g_param_spec_override("current-directory",
                              g_object_interface_find_property(g_iface,
                                                               "current-directory"));

    // override BaseView's properties
    g_iface = g_type_default_interface_peek(TYPE_BASEVIEW);
    _standard_view_props[PROP_STATUSBAR_TEXT] =
        g_param_spec_override("statusbar-text",
                              g_object_interface_find_property(g_iface,
                                                               "statusbar-text"));

    _standard_view_props[PROP_SHOW_HIDDEN] =
        g_param_spec_override("show-hidden",
                              g_object_interface_find_property(g_iface,
                                                               "show-hidden"));

    _standard_view_props[PROP_ZOOM_LEVEL] =
        g_param_spec_override("zoom-level",
                              g_object_interface_find_property(g_iface,
                                                               "zoom-level"));

    _standard_view_props[PROP_ACCEL_GROUP] =
        g_param_spec_object("accel-group",
                            "accel-group",
                            "accel-group",
                            GTK_TYPE_ACCEL_GROUP,
                            G_PARAM_WRITABLE);

    // install all properties
    g_object_class_install_properties(gobject_class,
                                      N_PROPERTIES,
                                      _standard_view_props);

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
    _standard_view_signals[START_OPEN_LOCATION] =
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

static void standard_view_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = standard_view_get_current_directory;
    iface->set_current_directory = standard_view_set_current_directory;
}

static void standard_view_component_init(ThunarComponentIface *iface)
{
    iface->get_selected_files = standard_view_get_selected_files_component;
    iface->set_selected_files = standard_view_set_selected_files_component;
}

static void standard_view_view_init(BaseViewIface *iface)
{
    iface->get_selected_files = standard_view_get_selected_files_view;
    iface->set_selected_files = standard_view_set_selected_files_view;
    iface->get_show_hidden = standard_view_get_show_hidden;
    iface->set_show_hidden = standard_view_set_show_hidden;
    iface->get_zoom_level = standard_view_get_zoom_level;
    iface->set_zoom_level = standard_view_set_zoom_level;
    iface->get_loading = standard_view_get_loading;

    iface->reload = standard_view_reload;
    iface->get_visible_range = standard_view_get_visible_range;
    iface->scroll_to_file = standard_view_scroll_to_file;
    iface->get_statusbar_text = standard_view_get_statusbar_text;
}


// Object Class ---------------------------------------------------------------

static GObject* standard_view_constructor(
                                    GType type,
                                    guint n_construct_properties,
                                    GObjectConstructParam *construct_properties)
{
    // let the GObject constructor create the instance
    GObject *object;
    object = G_OBJECT_CLASS(standard_view_parent_class)->constructor(
                                                        type,
                                                        n_construct_properties,
                                                        construct_properties);

    // cast to standard_view for convenience
    StandardView *standard_view = STANDARD_VIEW(object);

    ThunarZoomLevel zoom_level = THUNAR_ZOOM_LEVEL_25_PERCENT;
    baseview_set_zoom_level(BASEVIEW(standard_view), zoom_level);

    // determine the real view widget(treeview or iconview)
    GtkWidget *view = gtk_bin_get_child(GTK_BIN(object));

    /* apply our list model to the real view(the child of the scrolled window),
     * we therefore assume that all real views have the "model" property.
     */
    g_object_set(G_OBJECT(view), "model", standard_view->model, NULL);

    ThunarColumn sort_column = THUNAR_COLUMN_NAME;
    GtkSortType sort_order = GTK_SORT_ASCENDING;
    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(standard_view->model), sort_column, sort_order);

    // stay informed about changes to the sort column/order
    g_signal_connect(G_OBJECT(standard_view->model), "sort-column-changed",
                     G_CALLBACK(_standard_view_sort_column_changed), standard_view);

    // setup support to navigate using a horizontal mouse wheel and the back and forward buttons
    g_signal_connect(G_OBJECT(view), "scroll-event",
                     G_CALLBACK(_standard_view_scroll_event), object);

    // need to catch certain keys for the internal view widget
    g_signal_connect(G_OBJECT(view), "key-press-event",
                     G_CALLBACK(_standard_view_key_press_event), object);

    // setup the real view as drag source
    gtk_drag_source_set(view,
                        GDK_BUTTON1_MASK,
                        _drag_targets,
                        G_N_ELEMENTS(_drag_targets),
                        GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK);
    g_signal_connect(G_OBJECT(view), "drag-begin",
                     G_CALLBACK(_standard_view_drag_begin), object);
    g_signal_connect(G_OBJECT(view), "drag-data-get",
                     G_CALLBACK(_standard_view_drag_data_get), object);
    g_signal_connect(G_OBJECT(view), "drag-data-delete",
                     G_CALLBACK(_standard_view_drag_data_delete), object);
    g_signal_connect(G_OBJECT(view), "drag-end",
                     G_CALLBACK(_standard_view_drag_end), object);

    // setup the real view as drop site
    gtk_drag_dest_set(view,
                      0,
                      _drop_targets,
                      G_N_ELEMENTS(_drop_targets),
                      GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
    g_signal_connect(G_OBJECT(view), "drag-leave",
                     G_CALLBACK(_standard_view_drag_leave), object);
    g_signal_connect(G_OBJECT(view), "drag-motion",
                     G_CALLBACK(_standard_view_drag_motion), object);
    g_signal_connect(G_OBJECT(view), "drag-drop",
                     G_CALLBACK(_standard_view_drag_drop), object);
    g_signal_connect(G_OBJECT(view), "drag-data-received",
                     G_CALLBACK(_standard_view_drag_data_received), object);

    // connect to scroll events for generating thumbnail requests
    GtkAdjustment *adjustment;
    adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(standard_view));
    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(_standard_view_scrolled), object);
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(standard_view));
    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(_standard_view_scrolled), object);

    // done, we have a working object
    return object;
}

static void standard_view_dispose(GObject *object)
{
    StandardView *standard_view = STANDARD_VIEW(object);

    if (G_UNLIKELY(standard_view->loading_binding != NULL))
    {
        g_object_unref(standard_view->loading_binding);
        standard_view->loading_binding = NULL;
    }

    // be sure to cancel any pending drag autoscroll timer
    if (G_UNLIKELY(standard_view->priv->drag_scroll_timer_id != 0))
        g_source_remove(standard_view->priv->drag_scroll_timer_id);

    // be sure to cancel any pending drag timer
    if (G_UNLIKELY(standard_view->priv->drag_timer_id != 0))
        g_source_remove(standard_view->priv->drag_timer_id);

    // be sure to free any pending drag timer event
    if (G_UNLIKELY(standard_view->priv->drag_timer_event != NULL))
    {
        gdk_event_free(standard_view->priv->drag_timer_event);
        standard_view->priv->drag_timer_event = NULL;
    }

    // disconnect from file
    if (standard_view->priv->current_directory != NULL)
    {
        g_signal_handlers_disconnect_matched(standard_view->priv->current_directory,
                                              G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
        g_object_unref(standard_view->priv->current_directory);
        standard_view->priv->current_directory = NULL;
    }

    G_OBJECT_CLASS(standard_view_parent_class)->dispose(object);
}

static void standard_view_finalize(GObject *object)
{
    StandardView *standard_view = STANDARD_VIEW(object);

    // some safety checks
    e_assert(standard_view->loading_binding == NULL);
    e_assert(standard_view->icon_factory == NULL);

    // disconnect accelerators
    _standard_view_disconnect_accelerators(standard_view);

    // release the scroll_to_file reference(if any)
    if (G_UNLIKELY(standard_view->priv->scroll_to_file != NULL))
        g_object_unref(G_OBJECT(standard_view->priv->scroll_to_file));

    // release the selected_files list(if any)
    e_list_free(standard_view->priv->selected_files);

    // release the drag path list(just in case the drag-end wasn't fired before)
    e_list_free(standard_view->priv->drag_g_file_list);

    // release the drop path list(just in case the drag-leave wasn't fired before)
    e_list_free(standard_view->priv->drop_file_list);

    // release the history
    g_object_unref(standard_view->priv->history);

    // release the reference on the name renderer
    g_object_unref(G_OBJECT(standard_view->name_renderer));

    // release the reference on the icon renderer
    g_object_unref(G_OBJECT(standard_view->icon_renderer));

    // drop any existing "new-files" closure
    if (G_UNLIKELY(standard_view->priv->new_files_closure != NULL))
    {
        g_closure_invalidate(standard_view->priv->new_files_closure);
        g_closure_unref(standard_view->priv->new_files_closure);
        standard_view->priv->new_files_closure = NULL;
    }

    // drop any remaining "new-files" paths
    e_list_free(standard_view->priv->new_files_path_list);

    // disconnect from the list model
    g_signal_handlers_disconnect_matched(G_OBJECT(standard_view->model), G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
    g_object_unref(G_OBJECT(standard_view->model));

    // remove selection restore timeout
    if (standard_view->priv->restore_selection_idle_id != 0)
        g_source_remove(standard_view->priv->restore_selection_idle_id);

    // free the statusbar text(if any)
    if (standard_view->priv->statusbar_text_idle_id != 0)
        g_source_remove(standard_view->priv->statusbar_text_idle_id);

    g_free(standard_view->priv->statusbar_text);

    // release the scroll_to_files hash table
    g_hash_table_destroy(standard_view->priv->scroll_to_files);

    G_OBJECT_CLASS(standard_view_parent_class)->finalize(object);
}

static void standard_view_get_property(GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    (void) pspec;
    ThunarFile *current_directory;

    switch (prop_id)
    {

    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, navigator_get_current_directory(THUNAR_NAVIGATOR(object)));
        break;

    case PROP_LOADING:
        g_value_set_boolean(value, baseview_get_loading(BASEVIEW(object)));
        break;

    case PROP_DISPLAY_NAME:
        current_directory = navigator_get_current_directory(THUNAR_NAVIGATOR(object));
        if (current_directory != NULL)
            g_value_set_static_string(value, th_file_get_display_name(current_directory));
        break;

    case PROP_TOOLTIP_TEXT:
        current_directory = navigator_get_current_directory(THUNAR_NAVIGATOR(object));
        if (current_directory != NULL)
            g_value_take_string(value, g_file_get_parse_name(th_file_get_file(current_directory)));
        break;

    case PROP_SELECTED_FILES:
        g_value_set_boxed(value, component_get_selected_files(THUNAR_COMPONENT(object)));
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

static void standard_view_set_property(GObject      *object,
                                              guint         prop_id,
                                              const GValue *value,
                                              GParamSpec   *pspec)
{
    (void) pspec;
    StandardView *standard_view = STANDARD_VIEW(object);

    switch (prop_id)
    {

    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNAR_NAVIGATOR(object), g_value_get_object(value));
        break;

    case PROP_LOADING:
        standard_view_set_loading(standard_view, g_value_get_boolean(value));
        break;

    case PROP_SELECTED_FILES:
        component_set_selected_files(THUNAR_COMPONENT(object), g_value_get_boxed(value));
        break;

    case PROP_SHOW_HIDDEN:
        baseview_set_show_hidden(BASEVIEW(object), g_value_get_boolean(value));
        break;

    case PROP_ZOOM_LEVEL:
        baseview_set_zoom_level(BASEVIEW(object), g_value_get_enum(value));
        break;

    case PROP_ACCEL_GROUP:
        _standard_view_disconnect_accelerators(standard_view);
        standard_view->accel_group = g_value_dup_object(value);
        _standard_view_connect_accelerators(standard_view);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


// Widget Class ---------------------------------------------------------------

static void standard_view_realize(GtkWidget *widget)
{
    StandardView *standard_view = STANDARD_VIEW(widget);
    GtkIconTheme       *icon_theme;

    // let the GtkWidget do its work
    GTK_WIDGET_CLASS(standard_view_parent_class)->realize(widget);

    // determine the icon factory for the screen on which we are realized
    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
    standard_view->icon_factory = ifactory_get_for_icon_theme(icon_theme);
    g_object_bind_property(G_OBJECT(standard_view->icon_renderer), "size", G_OBJECT(standard_view->icon_factory), "thumbnail-size", G_BINDING_SYNC_CREATE);
}

static void standard_view_unrealize(GtkWidget *widget)
{
    StandardView *standard_view = STANDARD_VIEW(widget);

    // drop the reference on the icon factory
    g_signal_handlers_disconnect_by_func(G_OBJECT(standard_view->icon_factory), gtk_widget_queue_draw, standard_view);
    g_object_unref(G_OBJECT(standard_view->icon_factory));
    standard_view->icon_factory = NULL;

    // let the GtkWidget do its work
    GTK_WIDGET_CLASS(standard_view_parent_class)->unrealize(widget);
}

static void standard_view_grab_focus(GtkWidget *widget)
{
    // forward the focus grab to the real view
    gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(widget)));
}

static gboolean standard_view_draw(GtkWidget *widget, cairo_t *cr)
{
    // let the scrolled window do it's work
    cairo_save(cr);
    gboolean result = GTK_WIDGET_CLASS(standard_view_parent_class)->draw(widget, cr);
    cairo_restore(cr);

    // render the folder drop shadow
    if (G_UNLIKELY(STANDARD_VIEW(widget)->priv->drop_highlight))
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

static void _standard_view_connect_accelerators(StandardView *standard_view)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    if (standard_view->accel_group == NULL)
        return;

    xfce_gtk_accel_map_add_entries(_standard_view_actions, G_N_ELEMENTS(_standard_view_actions));
    xfce_gtk_accel_group_connect_action_entries(standard_view->accel_group,
            _standard_view_actions,
            G_N_ELEMENTS(_standard_view_actions),
            standard_view);

    // as well append accelerators of derived widgets
    STANDARD_VIEW_GET_CLASS(standard_view)->connect_accelerators(standard_view, standard_view->accel_group);
}

static void _standard_view_disconnect_accelerators(StandardView *standard_view)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    if (standard_view->accel_group == NULL)
        return;

    // Dont listen to the accel keys defined by the action entries any more
    xfce_gtk_accel_group_disconnect_action_entries(standard_view->accel_group,
            _standard_view_actions,
            G_N_ELEMENTS(_standard_view_actions));

    // as well disconnect accelerators of derived widgets
    STANDARD_VIEW_GET_CLASS(standard_view)->disconnect_accelerators(standard_view, standard_view->accel_group);

    // and release the accel group
    g_object_unref(standard_view->accel_group);
    standard_view->accel_group = NULL;
}


// Standard View Init ---------------------------------------------------------

static void standard_view_init(StandardView *standard_view)
{
    standard_view->priv = standard_view_get_instance_private(standard_view);

    // allocate the scroll_to_files mapping(directory GFile -> first visible child GFile)
    standard_view->priv->scroll_to_files = g_hash_table_new_full(g_file_hash,
                                                                 (GEqualFunc) g_file_equal, g_object_unref, g_object_unref);

    // initialize the scrolled window
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(standard_view),
                                   GTK_POLICY_AUTOMATIC,
                                   GTK_POLICY_AUTOMATIC);

    gtk_scrolled_window_set_hadjustment(GTK_SCROLLED_WINDOW(standard_view), NULL);
    gtk_scrolled_window_set_vadjustment(GTK_SCROLLED_WINDOW(standard_view), NULL);
    gtk_scrolled_window_set_shadow_type(GTK_SCROLLED_WINDOW(standard_view),
                                        GTK_SHADOW_IN);

    // setup the history support
    standard_view->priv->history = g_object_new(THUNAR_TYPE_HISTORY, NULL);
    g_signal_connect_swapped(G_OBJECT(standard_view->priv->history),
                             "change-directory",
                             G_CALLBACK(navigator_change_directory),
                             standard_view);

    // setup the list model
    standard_view->model = listmodel_new();
    g_signal_connect_after(G_OBJECT(standard_view->model),
                           "row-deleted",
                           G_CALLBACK(_standard_view_select_after_row_deleted),
                           standard_view);
    standard_view->priv->row_changed_id = g_signal_connect(G_OBJECT(standard_view->model), "row-changed", G_CALLBACK(_standard_view_row_changed), standard_view);
    g_signal_connect(G_OBJECT(standard_view->model),
                     "rows-reordered",
                     G_CALLBACK(_standard_view_rows_reordered),
                     standard_view);
    g_signal_connect(G_OBJECT(standard_view->model),
                     "error",
                     G_CALLBACK(_standard_view_error),
                     standard_view);

    // setup the icon renderer
    standard_view->icon_renderer = irenderer_new();
    g_object_ref_sink(G_OBJECT(standard_view->icon_renderer));

    g_object_bind_property(G_OBJECT(standard_view),
                           "zoom-level",
                           G_OBJECT(standard_view->icon_renderer),
                           "size",
                           G_BINDING_SYNC_CREATE);

    // setup the name renderer
    standard_view->name_renderer =
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

    g_object_ref_sink(G_OBJECT(standard_view->name_renderer));

    // be sure to update the selection whenever the folder changes
    g_signal_connect_swapped(G_OBJECT(standard_view->model),
                             "notify::folder",
                             G_CALLBACK(standard_view_selection_changed),
                             standard_view);

    /* be sure to update the statusbar text whenever the number of
     * files in our model changes.
     */
    g_signal_connect_swapped(G_OBJECT(standard_view->model),
                             "notify::num-files",
                             G_CALLBACK(_standard_view_update_statusbar_text),
                             standard_view);

    // be sure to update the statusbar text whenever the file-size-binary property changes
    g_signal_connect_swapped(G_OBJECT(standard_view->model),
                             "notify::file-size-binary",
                             G_CALLBACK(_standard_view_update_statusbar_text),
                             standard_view);

    // connect to size allocation signals for generating thumbnail requests
    g_signal_connect_after(G_OBJECT(standard_view),
                           "size-allocate",
                           G_CALLBACK(_standard_view_size_allocate),
                           NULL);

    // add widget to css class
    gtk_style_context_add_class(
                gtk_widget_get_style_context(GTK_WIDGET(standard_view)),
                "standard-view");

    standard_view->accel_group = NULL;
}


// Navigator Interface --------------------------------------------------------

static ThunarFile* standard_view_get_current_directory(ThunarNavigator *navigator)
{
    StandardView *standard_view = STANDARD_VIEW(navigator);
    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), NULL);
    return standard_view->priv->current_directory;
}

static void standard_view_set_current_directory(ThunarNavigator *navigator,
                                                ThunarFile *current_directory)
{
    StandardView *standard_view = STANDARD_VIEW(navigator);
    ThunarFolder       *folder;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(current_directory == NULL || THUNAR_IS_FILE(current_directory));

    // get the current directory
    if (standard_view->priv->current_directory == current_directory)
        return;

    // disconnect any previous "loading" binding

    if (G_LIKELY(standard_view->loading_binding != NULL))
    {
        g_object_unref(standard_view->loading_binding);
        standard_view->loading_binding = NULL;
    }

    // store the current scroll position
    if (current_directory != NULL)
        _standard_view_scroll_position_save(standard_view);

    // release previous directory
    if (standard_view->priv->current_directory != NULL)
    {
        g_signal_handlers_disconnect_matched(standard_view->priv->current_directory,
                                              G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, standard_view);
        g_object_unref(standard_view->priv->current_directory);
    }

    // check if we want to reset the directory
    if (G_UNLIKELY(current_directory == NULL))
    {
        // unset
        standard_view->priv->current_directory = NULL;

        /* resetting the folder for the model can take some time if the view has
         * to update the selection every time(i.e. closing a window with a lot of
         * selected files), so we temporarily disconnect the model from the view.
         */
        g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(standard_view))), "model", NULL, NULL);

        // reset the folder for the model
        listmodel_set_folder(standard_view->model, NULL);

        // reconnect the model to the view
        g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(standard_view))), "model", standard_view->model, NULL);

        // and we're done
        return;
    }

    // take ref on new directory
    standard_view->priv->current_directory = g_object_ref(current_directory);
    g_signal_connect(G_OBJECT(current_directory), "destroy", G_CALLBACK(_standard_view_current_directory_destroy), standard_view);
    g_signal_connect(G_OBJECT(current_directory), "changed", G_CALLBACK(_standard_view_current_directory_changed), standard_view);

    // scroll to top-left when changing folder
    gtk_adjustment_set_value(gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(standard_view)), 0.0);
    gtk_adjustment_set_value(gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(standard_view)), 0.0);

    // store the directory in the history
    navigator_set_current_directory(THUNAR_NAVIGATOR(standard_view->priv->history), current_directory);

    /* We drop the model from the view as a simple optimization to speed up
     * the process of disconnecting the model data from the view.
     */
    g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(standard_view))), "model", NULL, NULL);

    // open the new directory as folder
    folder = th_folder_get_for_file(current_directory);

    standard_view->loading_binding =
        g_object_bind_property_full(folder,
                                    "loading",
                                    standard_view,
                                    "loading",
                                    G_BINDING_SYNC_CREATE,
                                    NULL,
                                    NULL,
                                    standard_view,
                                    _standard_view_loading_unbound);

    // apply the new folder
    listmodel_set_folder(standard_view->model, folder);
    g_object_unref(G_OBJECT(folder));

    // reconnect our model to the view
    g_object_set(G_OBJECT(gtk_bin_get_child(GTK_BIN(standard_view))), "model", standard_view->model, NULL);

    // notify all listeners about the new/old current directory
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_CURRENT_DIRECTORY]);

    // update tab label and tooltip
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_DISPLAY_NAME]);
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_TOOLTIP_TEXT]);

    // restore the selection from the history
    _standard_view_restore_selection_from_history(standard_view);
}

static void _standard_view_loading_unbound(gpointer user_data)
{
    StandardView *standard_view = STANDARD_VIEW(user_data);

    // we don't have any binding now
    standard_view->loading_binding = NULL;

    // reset the "loading" property
    if (G_UNLIKELY(standard_view->loading))
    {
        standard_view->loading = FALSE;
        g_object_freeze_notify(G_OBJECT(standard_view));
        g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_LOADING]);
        _standard_view_update_statusbar_text(standard_view);
        g_object_thaw_notify(G_OBJECT(standard_view));
    }
}

static void _standard_view_scroll_position_save(StandardView *standard_view)
{
    ThunarFile    *first_file;
    GtkAdjustment *vadjustment;
    GtkAdjustment *hadjustment;
    GFile         *gfile;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // store the previous directory in the scroll hash table
    if (standard_view->priv->current_directory != NULL)
    {
        // only stop the first file is the scroll bar is actually moved
        vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(standard_view));
        hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(standard_view));
        gfile = th_file_get_file(standard_view->priv->current_directory);

        if (gtk_adjustment_get_value(vadjustment) == 0.0
                && gtk_adjustment_get_value(hadjustment) == 0.0)
        {
            // remove from the hash table, we already scroll to 0,0
            g_hash_table_remove(standard_view->priv->scroll_to_files, gfile);
        }
        else if (baseview_get_visible_range(BASEVIEW(standard_view), &first_file, NULL))
        {
            // add the file to our internal mapping of directories to scroll files
            g_hash_table_replace(standard_view->priv->scroll_to_files,
                                  g_object_ref(gfile),
                                  g_object_ref(th_file_get_file(first_file)));
            g_object_unref(first_file);
        }
    }
}

static void _standard_view_restore_selection_from_history(StandardView *standard_view)
{
    GList       selected_files;
    ThunarFile *selected_file;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(THUNAR_IS_FILE(standard_view->priv->current_directory));

    // reset the selected files list
    selected_files.data = NULL;
    selected_files.prev = NULL;
    selected_files.next = NULL;

    // determine the next file in the history
    selected_file = history_peek_forward(standard_view->priv->history);
    if (selected_file != NULL)
    {
        /* mark the file from history for selection if it is inside the new
         * directory */
        if (th_file_is_parent(standard_view->priv->current_directory, selected_file))
            selected_files.data = selected_file;
        else
            g_object_unref(selected_file);
    }

    // do the same with the previous file in the history
    if (selected_files.data == NULL)
    {
        selected_file = history_peek_back(standard_view->priv->history);
        if (selected_file != NULL)
        {
            /* mark the file from history for selection if it is inside the
             * new directory */
            if (th_file_is_parent(standard_view->priv->current_directory, selected_file))
                selected_files.data = selected_file;
            else
                g_object_unref(selected_file);
        }
    }

    /* select the previous or next file from the history if it is inside the
     * new current directory */
    if (selected_files.data != NULL)
    {
        component_set_selected_files(THUNAR_COMPONENT(standard_view), &selected_files);
        g_object_unref(G_OBJECT(selected_files.data));
    }
}


// Component Interface --------------------------------------------------------

static GList* standard_view_get_selected_files_component(ThunarComponent *component)
{
    return STANDARD_VIEW(component)->priv->selected_files;
}

static void standard_view_set_selected_files_component(ThunarComponent *component,
        GList           *selected_files)
{
    StandardView *standard_view = STANDARD_VIEW(component);
    GtkTreePath        *first_path = NULL;
    GList              *paths;
    GList              *lp;

    // release the previous selected files list(if any)
    if (G_UNLIKELY(standard_view->priv->selected_files != NULL))
    {
        e_list_free(standard_view->priv->selected_files);
        standard_view->priv->selected_files = NULL;
    }

    // check if we're still loading
    if (baseview_get_loading(BASEVIEW(standard_view)))
    {
        // remember a copy of the list for later
        standard_view->priv->selected_files = e_list_copy(selected_files);
    }
    else
    {
        // verify that we have a valid model
        if (G_UNLIKELY(standard_view->model == NULL))
            return;

        // unselect all previously selected files
        STANDARD_VIEW_GET_CLASS(standard_view)->unselect_all(standard_view);

        // determine the tree paths for the given files
        paths = listmodel_get_paths_for_files(standard_view->model, selected_files);
        if (G_LIKELY(paths != NULL))
        {
            // determine the first path
            for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
                // check if this path is located before the current first_path
                if (gtk_tree_path_compare(lp->data, first_path) < 0)
                    first_path = lp->data;
            }

            // place the cursor on the first selected path(must be first for GtkTreeView)
            STANDARD_VIEW_GET_CLASS(standard_view)->set_cursor(standard_view, first_path, FALSE);

            // select the given tree paths paths
            for (first_path = paths->data, lp = paths; lp != NULL; lp = lp->next)
            {
                // select the path
                STANDARD_VIEW_GET_CLASS(standard_view)->select_path(standard_view, lp->data);
            }

            // scroll to the first path(previously determined)
            STANDARD_VIEW_GET_CLASS(standard_view)->scroll_to_path(standard_view, first_path, FALSE, 0.0f, 0.0f);

            // release the tree paths
            g_list_free_full(paths,(GDestroyNotify) gtk_tree_path_free);
        }
    }
}


// View Interface -------------------------------------------------------------

static GList* standard_view_get_selected_files_view(BaseView *view)
{
    return STANDARD_VIEW(view)->priv->selected_files;
}

static void standard_view_set_selected_files_view(BaseView *view,
                                                  GList      *selected_files)
{
    standard_view_set_selected_files_component(THUNAR_COMPONENT(view), selected_files);
}

static gboolean standard_view_get_show_hidden(BaseView *view)
{
    return listmodel_get_show_hidden(STANDARD_VIEW(view)->model);
}

static void standard_view_set_show_hidden(BaseView *view, gboolean show_hidden)
{
    listmodel_set_show_hidden(STANDARD_VIEW(view)->model, show_hidden);
}

static ThunarZoomLevel standard_view_get_zoom_level(BaseView *view)
{
    return STANDARD_VIEW(view)->priv->zoom_level;
}

static void standard_view_set_zoom_level(BaseView     *view,
                                         ThunarZoomLevel zoom_level)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    gboolean newThumbnailSize = FALSE;

    // check if we have a new zoom-level here
    if (G_LIKELY(standard_view->priv->zoom_level != zoom_level))
    {
        if (thunar_zoom_level_to_thumbnail_size(zoom_level) != thunar_zoom_level_to_thumbnail_size(standard_view->priv->zoom_level))
            newThumbnailSize = TRUE;

        standard_view->priv->zoom_level = zoom_level;

        g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_ZOOM_LEVEL]);

        if (newThumbnailSize)
            standard_view_reload(view, TRUE);
    }
}

static gboolean standard_view_get_loading(BaseView *view)
{
    return STANDARD_VIEW(view)->loading;
}

static void standard_view_set_loading(StandardView *standard_view,
                                      gboolean           loading)
{
    ThunarFile *file;
    GList      *new_files_path_list;
    GList      *selected_files;
    GFile      *first_file;
    ThunarFile *current_directory;

    loading = !!loading;

    // check if we're already in that state
    if (G_UNLIKELY(standard_view->loading == loading))
        return;

    // apply the new state
    standard_view->loading = loading;

    // block or unblock the insert signal to avoid queueing thumbnail reloads
    if (loading)
        g_signal_handler_block(standard_view->model, standard_view->priv->row_changed_id);
    else
        g_signal_handler_unblock(standard_view->model, standard_view->priv->row_changed_id);

    // check if we're done loading and have a scheduled scroll_to_file
    if (G_UNLIKELY(!loading))
    {
        if (standard_view->priv->scroll_to_file != NULL)
        {
            // remember and reset the scroll_to_file reference
            file = standard_view->priv->scroll_to_file;
            standard_view->priv->scroll_to_file = NULL;

            // and try again
            baseview_scroll_to_file(BASEVIEW(standard_view), file,
                                        standard_view->priv->scroll_to_select,
                                        standard_view->priv->scroll_to_use_align,
                                        standard_view->priv->scroll_to_row_align,
                                        standard_view->priv->scroll_to_col_align);

            // cleanup
            g_object_unref(G_OBJECT(file));
        }
        else
        {
            // look for a first visible file in the hash table
            current_directory = navigator_get_current_directory(THUNAR_NAVIGATOR(standard_view));
            if (G_LIKELY(current_directory != NULL))
            {
                first_file = g_hash_table_lookup(standard_view->priv->scroll_to_files, th_file_get_file(current_directory));
                if (G_LIKELY(first_file != NULL))
                {
                    file = th_file_cache_lookup(first_file);
                    if (G_LIKELY(file != NULL))
                    {
                        baseview_scroll_to_file(BASEVIEW(standard_view), file, FALSE, TRUE, 0.0f, 0.0f);
                        g_object_unref(file);
                    }
                }
            }
        }
    }

    // check if we have a path list from new_files pending
    if (G_UNLIKELY(!loading && standard_view->priv->new_files_path_list != NULL))
    {
        // remember and reset the new_files_path_list
        new_files_path_list = standard_view->priv->new_files_path_list;
        standard_view->priv->new_files_path_list = NULL;

        // and try again
        _standard_view_new_files(standard_view, new_files_path_list);

        // cleanup
        e_list_free(new_files_path_list);
    }

    // check if we're done loading
    if (!loading)
    {
        // remember and reset the file list
        selected_files = standard_view->priv->selected_files;
        standard_view->priv->selected_files = NULL;

        // and try setting the selected files again
        component_set_selected_files(THUNAR_COMPONENT(standard_view), selected_files);

        // cleanup
        e_list_free(selected_files);
    }

    // notify listeners
    g_object_freeze_notify(G_OBJECT(standard_view));
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_LOADING]);
    _standard_view_update_statusbar_text(standard_view);
    g_object_thaw_notify(G_OBJECT(standard_view));
}

static void _standard_view_new_files(StandardView *standard_view,
                                           GList              *path_list)
{
    ThunarFile*file;
    GList     *file_list = NULL;
    GList     *lp;
    GtkWidget *source_view;
    GFile     *parent_file;
    gboolean   belongs_here;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // release the previous "new-files" paths(if any)
    if (G_UNLIKELY(standard_view->priv->new_files_path_list != NULL))
    {
        e_list_free(standard_view->priv->new_files_path_list);
        standard_view->priv->new_files_path_list = NULL;
    }

    // check if the folder is currently being loaded
    if (G_UNLIKELY(standard_view->loading))
    {
        // schedule the "new-files" paths for later processing
        standard_view->priv->new_files_path_list = e_list_copy(path_list);
    }
    else if (G_LIKELY(path_list != NULL))
    {
        // to check if we should reload
        parent_file = th_file_get_file(standard_view->priv->current_directory);
        belongs_here = FALSE;

        // determine the files for the paths
        for(lp = path_list; lp != NULL; lp = lp->next)
        {
            file = th_file_cache_lookup(lp->data);
            if (G_LIKELY(file != NULL))
                file_list = g_list_prepend(file_list, file);
            else if (!belongs_here && g_file_has_parent(lp->data, parent_file))
                belongs_here = TRUE;
        }

        // check if we have any new files here
        if (G_LIKELY(file_list != NULL))
        {
            // select the files
            component_set_selected_files(THUNAR_COMPONENT(standard_view), file_list);

            // release the file list
            g_list_free_full(file_list, g_object_unref);

            // grab the focus to the view widget
            gtk_widget_grab_focus(gtk_bin_get_child(GTK_BIN(standard_view)));
        }
        else if (belongs_here)
        {
            /* thunar files are not created yet, try again later because we know
             * some of them belong in this directory, so eventually they
             * will get a ThunarFile */
            standard_view->priv->new_files_path_list = e_list_copy(path_list);
        }
    }

    // when performing dnd between 2 views, we force a reload on the source as well
    source_view = g_object_get_data(G_OBJECT(standard_view), I_("source-view"));
    if (THUNAR_IS_VIEW(source_view))
        baseview_reload(BASEVIEW(source_view), FALSE);
}

static GClosure* _standard_view_new_files_closure(StandardView *standard_view,
                                       GtkWidget          *source_view)
{
    e_return_val_if_fail(source_view == NULL || THUNAR_IS_VIEW(source_view), NULL);

    // drop any previous "new-files" closure
    if (G_UNLIKELY(standard_view->priv->new_files_closure != NULL))
    {
        g_closure_invalidate(standard_view->priv->new_files_closure);
        g_closure_unref(standard_view->priv->new_files_closure);
    }

    // set the remove view data we possibly need to reload
    g_object_set_data(G_OBJECT(standard_view), I_("source-view"), source_view);

    // allocate a new "new-files" closure
    standard_view->priv->new_files_closure = g_cclosure_new_swap(G_CALLBACK(_standard_view_new_files), standard_view, NULL);
    g_closure_ref(standard_view->priv->new_files_closure);
    g_closure_sink(standard_view->priv->new_files_closure);

    // and return our new closure
    return standard_view->priv->new_files_closure;
}

static void standard_view_reload(BaseView *view, gboolean reload_info)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    ThunarFolder       *folder;
    ThunarFile         *file;

    // determine the folder for the view model
    folder = listmodel_get_folder(standard_view->model);

    if (G_LIKELY(folder != NULL))
    {
        file = th_folder_get_corresponding_file(folder);

        if (th_file_exists(file))
            th_folder_load(folder, reload_info);
        else
            _standard_view_current_directory_destroy(file, standard_view);
    }
}

static gboolean standard_view_get_visible_range(BaseView  *view,
                                                ThunarFile **start_file,
                                                ThunarFile **end_file)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    GtkTreePath        *start_path;
    GtkTreePath        *end_path;
    GtkTreeIter         iter;

    // determine the visible range as tree paths
    if ((*STANDARD_VIEW_GET_CLASS(standard_view)->get_visible_range)(standard_view, &start_path, &end_path))
    {
        // determine the file for the start path
        if (G_LIKELY(start_file != NULL))
        {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, start_path);
            *start_file = listmodel_get_file(standard_view->model, &iter);
        }

        // determine the file for the end path
        if (G_LIKELY(end_file != NULL))
        {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, end_path);
            *end_file = listmodel_get_file(standard_view->model, &iter);
        }

        // release the tree paths
        gtk_tree_path_free(start_path);
        gtk_tree_path_free(end_path);

        return TRUE;
    }

    return FALSE;
}

static void standard_view_scroll_to_file(BaseView *view,
                                         ThunarFile *file,
                                         gboolean    select_file,
                                         gboolean    use_align,
                                         gfloat      row_align,
                                         gfloat      col_align)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    GList               files;
    GList              *paths;

    // release the previous scroll_to_file reference(if any)
    if (G_UNLIKELY(standard_view->priv->scroll_to_file != NULL))
    {
        g_object_unref(G_OBJECT(standard_view->priv->scroll_to_file));
        standard_view->priv->scroll_to_file = NULL;
    }

    // check if we're still loading
    if (baseview_get_loading(view))
    {
        // remember a reference for the new file and settings
        standard_view->priv->scroll_to_file = THUNAR_FILE(g_object_ref(G_OBJECT(file)));
        standard_view->priv->scroll_to_select = select_file;
        standard_view->priv->scroll_to_use_align = use_align;
        standard_view->priv->scroll_to_row_align = row_align;
        standard_view->priv->scroll_to_col_align = col_align;
    }
    else
    {
        // fake a file list
        files.data = file;
        files.next = NULL;
        files.prev = NULL;

        // determine the path for the file
        paths = listmodel_get_paths_for_files(standard_view->model, &files);
        if (G_LIKELY(paths != NULL))
        {
            // scroll to the path
           (*STANDARD_VIEW_GET_CLASS(standard_view)->scroll_to_path)(standard_view, paths->data, use_align, row_align, col_align);

            // check if we should also alter the selection
            if (G_UNLIKELY(select_file))
            {
                // select only the file in question
               (*STANDARD_VIEW_GET_CLASS(standard_view)->unselect_all)(standard_view);
               (*STANDARD_VIEW_GET_CLASS(standard_view)->select_path)(standard_view, paths->data);
            }

            // cleanup
            g_list_free_full(paths,(GDestroyNotify) gtk_tree_path_free);
        }
    }
}

static const gchar* standard_view_get_statusbar_text(BaseView *view)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    GList              *items;

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), NULL);

    // generate the statusbar text on-demand
    if (standard_view->priv->statusbar_text == NULL)
    {
        // query the selected items(actually a list of GtkTreePath's)
        items = STANDARD_VIEW_GET_CLASS(standard_view)->get_selected_items(standard_view);

        /* we display a loading text if no items are
         * selected and the view is loading
         */
        if (items == NULL && standard_view->loading)
            return _("Loading folder contents...");

        standard_view->priv->statusbar_text = listmodel_get_statusbar_text(standard_view->model, items);
        g_list_free_full(items,(GDestroyNotify) gtk_tree_path_free);
    }

    return standard_view->priv->statusbar_text;
}


// Public Functions -----------------------------------------------------------

void standard_view_context_menu(StandardView *standard_view)
{
    GtkWidget  *window;
    AppMenu *context_menu;
    GList      *selected_items;

    //static int count;
    //DPRINT("%d : thunar_standard_view_context_menu\n", ++count);

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // grab an additional reference on the view
    g_object_ref(G_OBJECT(standard_view));

    selected_items =(*STANDARD_VIEW_GET_CLASS(standard_view)->get_selected_items)(standard_view);

    window = gtk_widget_get_toplevel(GTK_WIDGET(standard_view));

    context_menu = g_object_new(TYPE_APPMENU, "menu-type", MENU_TYPE_CONTEXT_STANDARD_VIEW,
                                 "launcher", window_get_launcher(THUNAR_WINDOW(window)), NULL);
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
    else // right click on some empty space
    {
        appmenu_add_sections(context_menu,
                                 MENU_SECTION_CREATE_NEW_FILES
                                 | MENU_SECTION_COPY_PASTE
                                 | MENU_SECTION_EMPTY_TRASH
                                 | MENU_SECTION_TERMINAL);

        _standard_view_append_menu_items(standard_view, GTK_MENU(context_menu), NULL);
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(context_menu));
        appmenu_add_sections(context_menu, MENU_SECTION_PROPERTIES);
    }

    appmenu_hide_accel_labels(context_menu);
    gtk_widget_show_all(GTK_WIDGET(context_menu));
    window_redirect_menu_tooltips_to_statusbar(THUNAR_WINDOW(window), GTK_MENU(context_menu));

    // if there is a drag_timer_event(long press), we use it
    if (standard_view->priv->drag_timer_event != NULL)
    {
        etk_menu_run_at_event(GTK_MENU(context_menu), standard_view->priv->drag_timer_event);
        gdk_event_free(standard_view->priv->drag_timer_event);
        standard_view->priv->drag_timer_event = NULL;
    }
    else
    {
        etk_menu_run(GTK_MENU(context_menu));
    }

    g_list_free_full(selected_items,(GDestroyNotify) gtk_tree_path_free);

    // release the additional reference on the view
    g_object_unref(G_OBJECT(standard_view));
}

static void _standard_view_append_menu_items(StandardView *standard_view,
                                             GtkMenu            *menu,
                                             GtkAccelGroup      *accel_group)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    STANDARD_VIEW_GET_CLASS(standard_view)->append_menu_items(standard_view, menu,
                                                                     accel_group);
}

/*
 * Schedules a context menu popup in response to a right-click button event.
 * Right-click events need to be handled in a special way, as the user may
 * also start a drag using the right mouse button and therefore this function
 * schedules a timer, which - once expired - opens the context menu.
 * If the user moves the mouse prior to expiration, a right-click drag
 * with GDK_ACTION_ASK, will be started instead.
 */
void standard_view_queue_popup(StandardView *standard_view,
                               GdkEventButton     *event)
{
    GtkSettings *settings;
    GtkWidget   *view;
    gint        delay;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(event != NULL);

    // check if we have already scheduled a drag timer
    if (G_LIKELY(standard_view->priv->drag_timer_id == 0))
    {
        // remember the new coordinates
        standard_view->priv->drag_x = event->x;
        standard_view->priv->drag_y = event->y;

        // figure out the real view
        view = gtk_bin_get_child(GTK_BIN(standard_view));

        /* we use the menu popup delay here, note that we only use this to
         * allow higher values! see bug #3549 */
        settings = gtk_settings_get_for_screen(gtk_widget_get_screen(view));
        g_object_get(G_OBJECT(settings), "gtk-menu-popup-delay", &delay, NULL);

        // schedule the timer
        standard_view->priv->drag_timer_id =
            g_timeout_add_full(G_PRIORITY_LOW,
                               MAX(225, delay),
                               _standard_view_drag_timer,
                               standard_view,
                               _standard_view_drag_timer_destroy);

        // store current event data
        standard_view->priv->drag_timer_event = gtk_get_current_event();

        // register the motion notify and the button release events on the real view
        g_signal_connect(G_OBJECT(view), "button-release-event",
                         G_CALLBACK(_standard_view_button_release_event), standard_view);

        g_signal_connect(G_OBJECT(view), "motion-notify-event",
                         G_CALLBACK(_standard_view_motion_notify_event), standard_view);
    }
}

static gboolean _standard_view_drag_timer(gpointer user_data)
{
    StandardView *standard_view = STANDARD_VIEW(user_data);

    // fire up the context menu
    THUNAR_THREADS_ENTER;

    //DPRINT("drag timer\n");
    standard_view_context_menu(standard_view);

    THUNAR_THREADS_LEAVE;

    return FALSE;
}

static void _standard_view_drag_timer_destroy(gpointer user_data)
{
    // unregister the motion notify and button release event handlers(thread-safe)
    g_signal_handlers_disconnect_by_func(gtk_bin_get_child(GTK_BIN(user_data)), _standard_view_button_release_event, user_data);
    g_signal_handlers_disconnect_by_func(gtk_bin_get_child(GTK_BIN(user_data)), _standard_view_motion_notify_event, user_data);

    // reset the drag timer source id
    STANDARD_VIEW(user_data)->priv->drag_timer_id = 0;
}

void standard_view_selection_changed(StandardView *standard_view)
{
    GtkTreeIter iter;
    GList      *lp, *selected_files;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // drop any existing "new-files" closure
    if (G_UNLIKELY(standard_view->priv->new_files_closure != NULL))
    {
        g_closure_invalidate(standard_view->priv->new_files_closure);
        g_closure_unref(standard_view->priv->new_files_closure);
        standard_view->priv->new_files_closure = NULL;
    }

    // release the previously selected files
    e_list_free(standard_view->priv->selected_files);

    // determine the new list of selected files(replacing GtkTreePath's with ThunarFile's)
    selected_files =(*STANDARD_VIEW_GET_CLASS(standard_view)->get_selected_items)(standard_view);
    for(lp = selected_files; lp != NULL; lp = lp->next)
    {
        // determine the iterator for the path
        gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, lp->data);

        // release the tree path...
        gtk_tree_path_free(lp->data);

        // ...and replace it with the file
        lp->data = listmodel_get_file(standard_view->model, &iter);
    }

    // and setup the new selected files list
    standard_view->priv->selected_files = selected_files;

    // update the statusbar text
    _standard_view_update_statusbar_text(standard_view);

    // emit notification for "selected-files"
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_SELECTED_FILES]);
}

void standard_view_set_history(StandardView *standard_view,
                               ThunarHistory      *history)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(history == NULL || THUNAR_IS_HISTORY(history));

    // set the new history
    g_object_unref(standard_view->priv->history);
    standard_view->priv->history = history;

    // connect callback
    g_signal_connect_swapped(G_OBJECT(history), "change-directory", G_CALLBACK(navigator_change_directory), standard_view);
}

ThunarHistory* standard_view_get_history(StandardView *standard_view)
{
    return standard_view->priv->history;
}


// ----------------------------------------------------------------------------

static void _standard_view_sort_column_changed(
                                            GtkTreeSortable    *tree_sortable,
                                            StandardView *standard_view)
{

    e_return_if_fail(GTK_IS_TREE_SORTABLE(tree_sortable));
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // keep the currently selected files selected after the change
    component_restore_selection(THUNAR_COMPONENT(standard_view));

    GtkSortType      sort_order;
    gint             sort_column;

    // determine the new sort column and sort order, and save them
    if (gtk_tree_sortable_get_sort_column_id(tree_sortable, &sort_column, &sort_order))
    {
#if 0
        // remember the new values as default
        g_object_set(G_OBJECT(standard_view->preferences),
                      "last-sort-column", sort_column,
                      "last-sort-order", sort_order,
                      NULL);
#endif
    }
}

static gboolean _standard_view_scroll_event(
                                            GtkWidget          *view,
                                            GdkEventScroll     *event,
                                            StandardView *standard_view)
{
    (void) view;
    GdkScrollDirection scrolling_direction;

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);

    if (event->direction != GDK_SCROLL_SMOOTH)
        scrolling_direction = event->direction;
    else if (event->delta_y < 0)
        scrolling_direction = GDK_SCROLL_UP;
    else if (event->delta_y > 0)
        scrolling_direction = GDK_SCROLL_DOWN;
    else if (event->delta_x < 0)
        scrolling_direction = GDK_SCROLL_LEFT;
    else if (event->delta_x > 0)
        scrolling_direction = GDK_SCROLL_RIGHT;
    else
    {
        g_debug("GDK_SCROLL_SMOOTH scrolling event with no delta happened");
        return FALSE;
    }

    // zoom-in/zoom-out on control+mouse wheel
    if ((event->state & GDK_CONTROL_MASK) != 0
        && (scrolling_direction == GDK_SCROLL_UP
            || scrolling_direction == GDK_SCROLL_DOWN))
    {
        baseview_set_zoom_level(BASEVIEW(standard_view),
                                   (scrolling_direction == GDK_SCROLL_UP)
                                    ? MIN(standard_view->priv->zoom_level + 1, THUNAR_ZOOM_N_LEVELS - 1)
                                    : MAX(standard_view->priv->zoom_level, 1) - 1);
        return TRUE;
    }

    // next please...
    return FALSE;
}

static gboolean _standard_view_key_press_event(
                                            GtkWidget          *view,
                                            GdkEventKey        *event,
                                            StandardView *standard_view)
{
    (void) view;
    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);

    // need to catch "/" and "~" first, as the views would otherwise start interactive search
    if ((event->keyval == GDK_KEY_slash || event->keyval == GDK_KEY_asciitilde || event->keyval == GDK_KEY_dead_tilde) && !(event->state &(~GDK_SHIFT_MASK & gtk_accelerator_get_default_mod_mask())))
    {
        // popup the location selector(in whatever way)
        if (event->keyval == GDK_KEY_dead_tilde)
            g_signal_emit(G_OBJECT(standard_view), _standard_view_signals[START_OPEN_LOCATION], 0, "~");
        else
            g_signal_emit(G_OBJECT(standard_view), _standard_view_signals[START_OPEN_LOCATION], 0, event->string);

        return TRUE;
    }

    return FALSE;
}

static void _standard_view_scrolled(GtkAdjustment      *adjustment,
                                          StandardView *standard_view)
{
    e_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // ignore adjustment changes when the view is still loading
    if (baseview_get_loading(BASEVIEW(standard_view)))
        return;
}


// ----------------------------------------------------------------------------

static void _standard_view_select_after_row_deleted(
                                            ListModel    *model,
                                            GtkTreePath        *path,
                                            StandardView *standard_view)
{
    (void) model;

    e_return_if_fail(path != NULL);
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

   (*STANDARD_VIEW_GET_CLASS(standard_view)->set_cursor)(standard_view, path, FALSE);
}

static void _standard_view_row_changed(ListModel    *model,
                                             GtkTreePath        *path,
                                             GtkTreeIter        *iter,
                                             StandardView *standard_view)
{
    (void) model;
    (void) path;
    (void) iter;
    (void) standard_view;
    return;
}

static void _standard_view_rows_reordered(
                                            ListModel    *model,
                                            GtkTreePath        *path,
                                            GtkTreeIter        *iter,
                                            gpointer            new_order,
                                            StandardView *standard_view)
{
    (void) path;
    (void) iter;
    (void) new_order;

    e_return_if_fail(IS_LISTMODEL(model));
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(standard_view->model == model);

    /* the order of the paths might have changed, but the selection
     * stayed the same, so restore the selection of the proper files
     * after letting row changes accumulate a bit */
    if (standard_view->priv->restore_selection_idle_id == 0)
        standard_view->priv->restore_selection_idle_id =
            g_timeout_add(50,
                          (GSourceFunc) _standard_view_restore_selection_idle,
                           standard_view);
}

static gboolean _standard_view_restore_selection_idle(StandardView *standard_view)
{
    GtkAdjustment *hadjustment;
    GtkAdjustment *vadjustment;
    gdouble        h, v, hl, hu, vl, vu;

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);

    // save the current scroll position and limits
    hadjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(standard_view));
    vadjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(standard_view));
    g_object_get(G_OBJECT(hadjustment), "value", &h, "lower", &hl, "upper", &hu, NULL);
    g_object_get(G_OBJECT(vadjustment), "value", &v, "lower", &vl, "upper", &vu, NULL);

    // keep the current scroll position by setting the limits to the current value
    g_object_set(G_OBJECT(hadjustment), "lower", h, "upper", h, NULL);
    g_object_set(G_OBJECT(vadjustment), "lower", v, "upper", v, NULL);

    // restore the selection
    component_restore_selection(THUNAR_COMPONENT(standard_view));
    standard_view->priv->restore_selection_idle_id = 0;

    // unfreeze the scroll position
    g_object_set(G_OBJECT(hadjustment), "value", h, "lower", hl, "upper", hu, NULL);
    g_object_set(G_OBJECT(vadjustment), "value", v, "lower", vl, "upper", vu, NULL);

    return FALSE;
}

static void _standard_view_error(ListModel    *model,
                                       const GError       *error,
                                       StandardView *standard_view)
{
    ThunarFile *file;

    e_return_if_fail(IS_LISTMODEL(model));
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(standard_view->model == model);

    // determine the ThunarFile for the current directory
    file = navigator_get_current_directory(THUNAR_NAVIGATOR(standard_view));
    if (G_UNLIKELY(file == NULL))
        return;

    // inform the user about the problem
    dialog_error(GTK_WIDGET(standard_view), error,
                               _("Failed to open directory \"%s\""),
                               th_file_get_display_name(file));
}

static gboolean _standard_view_update_statusbar_text_idle(gpointer data)
{
    StandardView *standard_view = STANDARD_VIEW(data);

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);

    THUNAR_THREADS_ENTER

    // clear the current status text(will be recalculated on-demand)
    g_free(standard_view->priv->statusbar_text);
    standard_view->priv->statusbar_text = NULL;

    standard_view->priv->statusbar_text_idle_id = 0;

    // tell everybody that the statusbar text may have changed
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_STATUSBAR_TEXT]);

    THUNAR_THREADS_LEAVE

    return FALSE;
}

static void _standard_view_update_statusbar_text(StandardView *standard_view)
{
    // stop pending timeout
    if (standard_view->priv->statusbar_text_idle_id != 0)
        g_source_remove(standard_view->priv->statusbar_text_idle_id);

    /* restart a new one, this way we avoid multiple update when
     * the user is pressing a key to scroll */
    standard_view->priv->statusbar_text_idle_id =
        g_timeout_add_full(G_PRIORITY_LOW,
                           50,
                           _standard_view_update_statusbar_text_idle,
                           standard_view,
                           NULL);
}

static void _standard_view_size_allocate(StandardView *standard_view,
                                               GtkAllocation      *allocation)
{
    (void) allocation;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // ignore size changes when the view is still loading
    if (baseview_get_loading(BASEVIEW(standard_view)))
        return;
}


// ----------------------------------------------------------------------------

static void _standard_view_current_directory_destroy(ThunarFile *current_directory,
                                               StandardView *standard_view)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(THUNAR_IS_FILE(current_directory));
    e_return_if_fail(standard_view->priv->current_directory == current_directory);

    GError     *error = NULL;

    // get a fallback directory(parents or home) we can navigate to
    ThunarFile *new_directory = NULL;
    new_directory = _standard_view_get_fallback_directory(current_directory, error);

    if (G_UNLIKELY(new_directory == NULL))
    {
        // display an error to the user
        dialog_error(GTK_WIDGET(standard_view), error, _("Failed to open the home folder"));
        g_error_free(error);
        return;
    }

    // let the parent window update all active and inactive views(tabs)
    GtkWidget  *window;
    window = gtk_widget_get_toplevel(GTK_WIDGET(standard_view));
    window_update_directories(THUNAR_WINDOW(window),
                                      current_directory,
                                      new_directory);

    // release the reference to the new directory
    g_object_unref(new_directory);
}

static void _standard_view_current_directory_changed(ThunarFile *current_directory,
                                               StandardView *standard_view)
{
    e_return_if_fail(IS_STANDARD_VIEW(standard_view));
    e_return_if_fail(THUNAR_IS_FILE(current_directory));
    e_return_if_fail(standard_view->priv->current_directory == current_directory);

    // update tab label and tooltip
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_DISPLAY_NAME]);
    g_object_notify_by_pspec(G_OBJECT(standard_view), _standard_view_props[PROP_TOOLTIP_TEXT]);
}

/*
 * Find a fallback directory we can navigate to if the directory gets
 * deleted. It first tries the parent folders, and finally if none can
 * be found, the home folder. If the home folder cannot be accessed,
 * the error will be stored for use by the caller.
 */
static ThunarFile* _standard_view_get_fallback_directory(
                                                        ThunarFile *directory,
                                                        GError     *error)
{
    ThunarFile *new_directory = NULL;
    GFile      *path;
    GFile      *tmp;

    e_return_val_if_fail(THUNAR_IS_FILE(directory), NULL);

    // determine the path of the directory
    path = g_object_ref(th_file_get_file(directory));

    // try to find a parent directory that still exists
    while(new_directory == NULL)
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


// ----------------------------------------------------------------------------

static gboolean _standard_view_button_release_event(
                                            GtkWidget      *view,
                                            GdkEventButton *event,
                                            StandardView   *standard_view)
{
    (void) view;
    (void) event;

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);
    e_return_val_if_fail(standard_view->priv->drag_timer_id != 0, FALSE);

    // cancel the pending drag timer
    g_source_remove(standard_view->priv->drag_timer_id);

    standard_view_context_menu(standard_view);

    return TRUE;
}

static gboolean _standard_view_motion_notify_event(
                                                GtkWidget          *view,
                                                GdkEventMotion     *event,
                                                StandardView *standard_view)
{
    GtkTargetList  *target_list;

    e_return_val_if_fail(IS_STANDARD_VIEW(standard_view), FALSE);
    e_return_val_if_fail(standard_view->priv->drag_timer_id != 0, FALSE);

    // check if we passed the DnD threshold
    if (gtk_drag_check_threshold(view, standard_view->priv->drag_x, standard_view->priv->drag_y, event->x, event->y))
    {
        // cancel the drag timer, as we won't popup the menu anymore
        g_source_remove(standard_view->priv->drag_timer_id);
        gdk_event_free(standard_view->priv->drag_timer_event);
        standard_view->priv->drag_timer_event = NULL;

        // allocate the drag context
        target_list = gtk_target_list_new(_drag_targets, G_N_ELEMENTS(_drag_targets));
        gtk_drag_begin_with_coordinates(view, target_list,
                                         GDK_ACTION_COPY |
                                         GDK_ACTION_MOVE |
                                         GDK_ACTION_LINK |
                                         GDK_ACTION_ASK,
                                         3,(GdkEvent *) event, -1, -1);
        gtk_target_list_unref(target_list);

        return TRUE;
    }

    return FALSE;
}


// Actions --------------------------------------------------------------------

static void _standard_view_select_all_files(BaseView *view)
{
    StandardView *standard_view = STANDARD_VIEW(view);

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // grab the focus to the view
    gtk_widget_grab_focus(GTK_WIDGET(standard_view));

    // select all files in the real view
   (*STANDARD_VIEW_GET_CLASS(standard_view)->select_all)(standard_view);
}

static void _standard_view_select_by_pattern(BaseView *view)
{
    StandardView *standard_view = STANDARD_VIEW(view);
    GtkWidget          *window;
    GtkWidget          *dialog;
    GtkWidget          *vbox;
    GtkWidget          *hbox;
    GtkWidget          *label;
    GtkWidget          *entry;
    GList              *paths;
    GList              *lp;
    gint                response;
    gchar              *example_pattern;
    const gchar        *pattern;
    gchar              *pattern_extended = NULL;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    window = gtk_widget_get_toplevel(GTK_WIDGET(standard_view));
    dialog = gtk_dialog_new_with_buttons(_("Select by Pattern"),
                                          GTK_WINDOW(window),
                                          GTK_DIALOG_MODAL
                                          | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_Select"), GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 290, -1);

    vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), vbox, TRUE, TRUE, 0);
    gtk_widget_show(vbox);

    hbox = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_HORIZONTAL, "border-width", 6, "spacing", 10, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new_with_mnemonic(_("_Pattern:"));
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    entry = gtk_entry_new();
    gtk_entry_set_activates_default(GTK_ENTRY(entry), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), entry, TRUE, TRUE, 0);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), entry);
    gtk_widget_show(entry);

    hbox = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_HORIZONTAL, "margin-right", 6, "margin-bottom", 6, "spacing", 0, NULL);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    label = gtk_label_new(NULL);
    example_pattern = g_strdup_printf("<b>%s</b> %s ",
                                       _("Examples:"),
                                       "*.png, file\?\?.txt, pict*.\?\?\?");
    gtk_label_set_markup(GTK_LABEL(label), example_pattern);
    g_free(example_pattern);
    gtk_box_pack_end(GTK_BOX(hbox), label, FALSE, FALSE, 0);
    gtk_widget_show(label);

    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (response == GTK_RESPONSE_OK)
    {
        // get entered pattern
        pattern = gtk_entry_get_text(GTK_ENTRY(entry));
        if (pattern != NULL
                && strchr(pattern, '*') == NULL
                && strchr(pattern, '?') == NULL)
        {
            // make file matching pattern
            pattern_extended = g_strdup_printf("*%s*", pattern);
            pattern = pattern_extended;
        }

        // select all files that match pattern
        paths = listmodel_get_paths_for_pattern(standard_view->model, pattern);
        STANDARD_VIEW_GET_CLASS(standard_view)->unselect_all(standard_view);

        // set the cursor and scroll to the first selected item
        if (paths != NULL)
            STANDARD_VIEW_GET_CLASS(standard_view)->set_cursor(standard_view, g_list_last(paths)->data, FALSE);

        for(lp = paths; lp != NULL; lp = lp->next)
        {
            STANDARD_VIEW_GET_CLASS(standard_view)->select_path(standard_view, lp->data);
            gtk_tree_path_free(lp->data);
        }
        g_list_free(paths);
        g_free(pattern_extended);
    }

    gtk_widget_destroy(dialog);
}

static void _standard_view_selection_invert(BaseView *view)
{
    StandardView *standard_view = STANDARD_VIEW(view);

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    // grab the focus to the view
    gtk_widget_grab_focus(GTK_WIDGET(standard_view));

    // invert all files in the real view
   (*STANDARD_VIEW_GET_CLASS(standard_view)->selection_invert)(standard_view);
}


// DnD Source -----------------------------------------------------------------

static void _standard_view_drag_begin(GtkWidget *view, GdkDragContext *context,
                                      StandardView *standard_view)
{
    (void) view;
    ThunarFile *file;
    GdkPixbuf  *icon;
    gint        size;

    // release the drag path list(just in case the drag-end wasn't fired before)
    e_list_free(standard_view->priv->drag_g_file_list);

    // query the list of selected URIs
    standard_view->priv->drag_g_file_list = th_file_list_to_thunar_g_file_list(standard_view->priv->selected_files);
    if (G_LIKELY(standard_view->priv->drag_g_file_list != NULL))
    {
        // determine the first selected file
        file = th_file_get(standard_view->priv->drag_g_file_list->data, NULL);
        if (G_LIKELY(file != NULL))
        {
            // generate an icon based on that file
            g_object_get(G_OBJECT(standard_view->icon_renderer), "size", &size, NULL);
            icon = ifactory_load_file_icon(standard_view->icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, size);
            gtk_drag_set_icon_pixbuf(context, icon, 0, 0);
            g_object_unref(G_OBJECT(icon));

            // release the file
            g_object_unref(G_OBJECT(file));
        }
    }
}

static void _standard_view_drag_data_get(GtkWidget          *view,
                                         GdkDragContext     *context,
                                         GtkSelectionData   *selection_data,
                                         guint              info,
                                         guint              timestamp,
                                         StandardView *standard_view)
{
    (void) view;
    (void) context;
    (void) info;
    (void) timestamp;

    gchar **uris;

    // set the URI list for the drag selection
    if (standard_view->priv->drag_g_file_list != NULL)
    {
        uris = e_file_list_to_stringv(standard_view->priv->drag_g_file_list);
        gtk_selection_data_set_uris(selection_data, uris);
        g_strfreev(uris);
    }
}

static void _standard_view_drag_data_delete(GtkWidget          *view,
                                            GdkDragContext     *context,
                                            StandardView *standard_view)
{
    (void) context;
    (void) standard_view;
    // make sure the default handler of ExoIconView/GtkTreeView is never run
    g_signal_stop_emission_by_name(G_OBJECT(view), "drag-data-delete");
}

static void _standard_view_drag_end(GtkWidget          *view,
                                    GdkDragContext     *context,
                                    StandardView *standard_view)
{
    (void) view;
    (void) context;

    // stop any running drag autoscroll timer
    if (G_UNLIKELY(standard_view->priv->drag_scroll_timer_id != 0))
        g_source_remove(standard_view->priv->drag_scroll_timer_id);

    // release the list of dragged URIs
    e_list_free(standard_view->priv->drag_g_file_list);
    standard_view->priv->drag_g_file_list = NULL;
}


// DnD Target -----------------------------------------------------------------

static void _standard_view_drag_leave(GtkWidget          *widget,
                                      GdkDragContext     *context,
                                      guint              timestamp,
                                      StandardView *standard_view)
{
    (void) widget;
    (void) context;
    (void) timestamp;

    // reset the drop-file for the icon renderer
    g_object_set(G_OBJECT(standard_view->icon_renderer), "drop-file", NULL, NULL);

    // stop any running drag autoscroll timer
    if (G_UNLIKELY(standard_view->priv->drag_scroll_timer_id != 0))
        g_source_remove(standard_view->priv->drag_scroll_timer_id);

    // disable the drop highlighting around the view
    if (G_LIKELY(standard_view->priv->drop_highlight))
    {
        standard_view->priv->drop_highlight = FALSE;
        gtk_widget_queue_draw(GTK_WIDGET(standard_view));
    }

    // reset the "drop data ready" status and free the URI list
    if (G_LIKELY(standard_view->priv->drop_data_ready))
    {
        e_list_free(standard_view->priv->drop_file_list);
        standard_view->priv->drop_file_list = NULL;
        standard_view->priv->drop_data_ready = FALSE;
    }

    // disable the highlighting of the items in the view
    STANDARD_VIEW_GET_CLASS(standard_view)->highlight_path(standard_view, NULL);
}

static gboolean _standard_view_drag_motion(GtkWidget          *view,
                                           GdkDragContext     *context,
                                           gint                x,
                                           gint                y,
                                           guint               timestamp,
                                           StandardView *standard_view)
{
    GdkDragAction action = 0;
    GtkTreePath  *path;
    ThunarFile   *file = NULL;
    GdkAtom       target;

    // request the drop data on-demand(if we don't have it already)
    if (G_UNLIKELY(!standard_view->priv->drop_data_ready))
    {
        // check if we can handle that drag data(yet?)
        target = gtk_drag_dest_find_target(view, context, NULL);

        if ((target == gdk_atom_intern_static_string("XdndDirectSave0")) ||(target == gdk_atom_intern_static_string("_NETSCAPE_URL")))
        {
            // determine the file for the given coordinates
            file = _standard_view_get_drop_file(standard_view, x, y, &path);

            // check if we can save here
            if (G_LIKELY(file != NULL
                          && th_file_is_local(file)
                          && th_file_is_directory(file)
                          && th_file_is_writable(file)))
            {
                action = gdk_drag_context_get_suggested_action(context);
            }

            // reset path if we cannot drop
            if (G_UNLIKELY(action == 0 && path != NULL))
            {
                gtk_tree_path_free(path);
                path = NULL;
            }

            // do the view highlighting
            if (standard_view->priv->drop_highlight !=(path == NULL && action != 0))
            {
                standard_view->priv->drop_highlight =(path == NULL && action != 0);
                gtk_widget_queue_draw(GTK_WIDGET(standard_view));
            }

            // setup drop-file for the icon renderer to highlight the target
            g_object_set(G_OBJECT(standard_view->icon_renderer), "drop-file",(action != 0) ? file : NULL, NULL);

            // do the item highlighting
           (*STANDARD_VIEW_GET_CLASS(standard_view)->highlight_path)(standard_view, path);

            // cleanup
            if (G_LIKELY(file != NULL))
                g_object_unref(G_OBJECT(file));

            if (G_LIKELY(path != NULL))
                gtk_tree_path_free(path);
        }
        else
        {
            // request the drag data from the source
            if (target != GDK_NONE)
                gtk_drag_get_data(view, context, target, timestamp);
        }

        // tell Gdk whether we can drop here
        gdk_drag_status(context, action, timestamp);
    }
    else
    {
        // check whether we can drop at(x,y)
        _standard_view_get_dest_actions(standard_view, context, x, y, timestamp, NULL);
    }

    // start the drag autoscroll timer if not already running
    if (G_UNLIKELY(standard_view->priv->drag_scroll_timer_id == 0))
    {
        // schedule the drag autoscroll timer
        standard_view->priv->drag_scroll_timer_id = g_timeout_add_full(
                                        G_PRIORITY_LOW,
                                        50,
                                        _standard_view_drag_scroll_timer,
                                        standard_view,
                                        _standard_view_drag_scroll_timer_destroy);
    }

    return TRUE;
}

static gboolean _standard_view_drag_drop(GtkWidget          *view,
                                         GdkDragContext     *context,
                                         gint               x,
                                         gint               y,
                                         guint              timestamp,
                                         StandardView *standard_view)
{
    ThunarFile *file = NULL;
    GdkAtom     target;
    guchar     *prop_text;
    GFile      *path;
    gchar      *uri = NULL;
    gint        prop_len;

    target = gtk_drag_dest_find_target(view, context, NULL);
    if (G_UNLIKELY(target == GDK_NONE))
    {
        // we cannot handle the drag data
        return FALSE;
    }
    else if (G_UNLIKELY(target == gdk_atom_intern_static_string("XdndDirectSave0")))
    {
        // determine the file for the drop position
        file = _standard_view_get_drop_file(standard_view, x, y, NULL);
        if (G_LIKELY(file != NULL))
        {
            // determine the file name from the DnD source window
            if (gdk_property_get(gdk_drag_context_get_source_window(context),
                                  gdk_atom_intern_static_string("XdndDirectSave0"),
                                  gdk_atom_intern_static_string("text/plain"), 0, 1024, FALSE, NULL, NULL,
                                  &prop_len, &prop_text) && prop_text != NULL)
            {
                // zero-terminate the string
                prop_text = g_realloc(prop_text, prop_len + 1);
                prop_text[prop_len] = '\0';

                // verify that the file name provided by the source is valid
                if (G_LIKELY(*prop_text != '\0' && strchr((const gchar *) prop_text, G_DIR_SEPARATOR) == NULL))
                {
                    // allocate the relative path for the target
                    path = g_file_resolve_relative_path(th_file_get_file(file),
                                                        (const gchar *)prop_text);

                    // determine the new URI
                    uri = g_file_get_uri(path);

                    // setup the property
                    gdk_property_change(gdk_drag_context_get_source_window(context),
                                         gdk_atom_intern_static_string("XdndDirectSave0"),
                                         gdk_atom_intern_static_string("text/plain"), 8,
                                         GDK_PROP_MODE_REPLACE,(const guchar *) uri,
                                         strlen(uri));

                    // cleanup
                    g_object_unref(path);
                    g_free(uri);
                }
                else
                {
                    // tell the user that the file name provided by the X Direct Save source is invalid
                    dialog_error(GTK_WIDGET(standard_view), NULL, _("Invalid filename provided by XDS drag site"));
                }

                // cleanup
                g_free(prop_text);
            }

            // release the file reference
            g_object_unref(G_OBJECT(file));
        }

        // if uri == NULL, we didn't set the property
        if (G_UNLIKELY(uri == NULL))
            return FALSE;
    }

    /* set state so the drag-data-received knows that
     * this is really a drop this time.
     */
    standard_view->priv->drop_occurred = TRUE;

    /* request the drag data from the source(initiates
     * saving in case of XdndDirectSave).
     */
    gtk_drag_get_data(view, context, target, timestamp);

    // we'll call gtk_drag_finish() later
    return TRUE;
}

static void _standard_view_drag_data_received(GtkWidget          *view,
                                              GdkDragContext     *context,
                                              gint               x,
                                              gint               y,
                                              GtkSelectionData   *selection_data,
                                              guint              info,
                                              guint              timestamp,
                                              StandardView *standard_view)
{
    GdkDragAction actions;
    GdkDragAction action;
    ThunarFolder *folder;
    ThunarFile   *file = NULL;
    GtkWidget    *toplevel;
    gboolean      succeed = FALSE;
    GError       *error = NULL;
    gchar        *working_directory;
    gchar        *argv[11];
    gchar       **bits;
    gint          pid;
    gint          n = 0;
    GtkWidget    *source_widget;
    GtkWidget    *source_view = NULL;
    GdkScreen    *screen;
    char         *display = NULL;

    // check if we don't already know the drop data
    if (G_LIKELY(!standard_view->priv->drop_data_ready))
    {
        // extract the URI list from the selection data(if valid)
        if (info == TARGET_TEXT_URI_LIST && gtk_selection_data_get_format(selection_data) == 8 && gtk_selection_data_get_length(selection_data) > 0)
            standard_view->priv->drop_file_list = e_file_list_new_from_string((gchar *) gtk_selection_data_get_data(selection_data));

        // reset the state
        standard_view->priv->drop_data_ready = TRUE;
    }

    // check if the data was dropped
    if (G_UNLIKELY(standard_view->priv->drop_occurred))
    {
        // reset the state
        standard_view->priv->drop_occurred = FALSE;

        // check if we're doing XdndDirectSave
        if (G_UNLIKELY(info == TARGET_XDND_DIRECT_SAVE0))
        {
            // we don't handle XdndDirectSave stage(3), result "F" yet
            if (G_UNLIKELY(gtk_selection_data_get_format(selection_data) == 8 && gtk_selection_data_get_length(selection_data) == 1 && gtk_selection_data_get_data(selection_data)[0] == 'F'))
            {
                // indicate that we don't provide "F" fallback
                gdk_property_change(gdk_drag_context_get_source_window(context),
                                     gdk_atom_intern_static_string("XdndDirectSave0"),
                                     gdk_atom_intern_static_string("text/plain"), 8,
                                     GDK_PROP_MODE_REPLACE,(const guchar *) "", 0);
            }
            else if (G_LIKELY(gtk_selection_data_get_format(selection_data) == 8 && gtk_selection_data_get_length(selection_data) == 1 && gtk_selection_data_get_data(selection_data)[0] == 'S'))
            {
                // XDS was successfull, so determine the file for the drop position
                file = _standard_view_get_drop_file(standard_view, x, y, NULL);
                if (G_LIKELY(file != NULL))
                {
                    // verify that we have a directory here
                    if (th_file_is_directory(file))
                    {
                        // reload the folder corresponding to the file
                        folder = th_folder_get_for_file(file);
                        th_folder_load(folder, FALSE);
                        g_object_unref(G_OBJECT(folder));
                    }

                    // cleanup
                    g_object_unref(G_OBJECT(file));
                }
            }

            // in either case, we succeed!
            succeed = TRUE;
        }
        else if (G_UNLIKELY(info == TARGET_NETSCAPE_URL))
        {
            // check if the format is valid and we have any data
            if (G_LIKELY(gtk_selection_data_get_format(selection_data) == 8 && gtk_selection_data_get_length(selection_data) > 0))
            {
                // _NETSCAPE_URL looks like this: "$URL\n$TITLE"
                bits = g_strsplit((const gchar *) gtk_selection_data_get_data(selection_data), "\n", -1);
                if (G_LIKELY(g_strv_length(bits) == 2))
                {
                    // determine the file for the drop position
                    file = _standard_view_get_drop_file(standard_view, x, y, NULL);
                    if (G_LIKELY(file != NULL))
                    {
                        // determine the absolute path to the target directory
                        working_directory = g_file_get_path(th_file_get_file(file));
                        if (G_LIKELY(working_directory != NULL))
                        {
                            // prepare the basic part of the command
                            argv[n++] = "exo-desktop-item-edit";
                            argv[n++] = "--type=Link";
                            argv[n++] = "--url";
                            argv[n++] = bits[0];
                            argv[n++] = "--name";
                            argv[n++] = bits[1];

                            // determine the toplevel window
                            toplevel = gtk_widget_get_toplevel(view);
                            if (toplevel != NULL && gtk_widget_is_toplevel(toplevel))
                            {

#if defined(GDK_WINDOWING_X11)
                                // on X11, we can supply the parent window id here
                                argv[n++] = "--xid";
                                argv[n++] = g_newa(gchar, 32);
                                g_snprintf(argv[n - 1], 32, "%ld",(glong) GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(toplevel))));
#endif

                            }

                            // terminate the parameter list
                            argv[n++] = "--create-new";
                            argv[n++] = working_directory;
                            argv[n++] = NULL;

                            screen = gtk_widget_get_screen(GTK_WIDGET(view));

                            if (screen != NULL)
                                display = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));

                            // try to run exo-desktop-item-edit
                            succeed = g_spawn_async(working_directory, argv, NULL,
                                                     G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                                     util_setup_display_cb, display, &pid, &error);

                            if (G_UNLIKELY(!succeed))
                            {
                                // display an error dialog to the user
                                dialog_error(standard_view, error, _("Failed to create a link for the URL \"%s\""), bits[0]);
                                g_free(working_directory);
                                g_error_free(error);
                            }
                            else
                            {
                                // reload the directory when the command terminates
                                g_child_watch_add_full(
                                                G_PRIORITY_LOW,
                                                pid,
                                                _standard_view_reload_directory,
                                                working_directory,
                                                g_free);
                            }

                            // cleanup
                            g_free(display);
                        }

                        g_object_unref(G_OBJECT(file));
                    }
                }

                // cleanup
                g_strfreev(bits);
            }
        }
        else if (G_LIKELY(info == TARGET_TEXT_URI_LIST))
        {
            // determine the drop position
            actions = _standard_view_get_dest_actions(standard_view, context, x, y, timestamp, &file);

            if (G_LIKELY((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
            {
                // ask the user what to do with the drop data
                action = (gdk_drag_context_get_selected_action(context) == GDK_ACTION_ASK)
                         ? dnd_ask(GTK_WIDGET(standard_view), file, standard_view->priv->drop_file_list, actions)
                         : gdk_drag_context_get_selected_action(context);

                // perform the requested action
                if (G_LIKELY(action != 0))
                {
                    // look if we can find the drag source widget
                    source_widget = gtk_drag_get_source_widget(context);
                    if (source_widget != NULL)
                    {
                        /* if this is a source view, attach it to the view receiving
                         * the data, see thunar_standard_view_new_files */
                        source_view = gtk_widget_get_parent(source_widget);
                        if (!THUNAR_IS_VIEW(source_view))
                            source_view = NULL;
                    }

                    succeed = dnd_perform(GTK_WIDGET(standard_view), file, standard_view->priv->drop_file_list,
                                                  action, _standard_view_new_files_closure(standard_view, source_view));
                }
            }

            // release the file reference
            if (G_LIKELY(file != NULL))
                g_object_unref(G_OBJECT(file));
        }

        // tell the peer that we handled the drop
        gtk_drag_finish(context, succeed, FALSE, timestamp);

        // disable the highlighting and release the drag data
        _standard_view_drag_leave(view, context, timestamp, standard_view);
    }
}

static ThunarFile* _standard_view_get_drop_file(
                                        StandardView *standard_view,
                                        gint               x,
                                        gint               y,
                                        GtkTreePath        **path_return)
{
    GtkTreePath *path = NULL;
    GtkTreeIter  iter;
    ThunarFile  *file = NULL;

    // determine the path for the given coordinates
    path =(*STANDARD_VIEW_GET_CLASS(standard_view)->get_path_at_pos)(standard_view, x, y);
    if (G_LIKELY(path != NULL))
    {
        // determine the file for the path
        gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, path);
        file = listmodel_get_file(standard_view->model, &iter);

        // we can only drop to directories and executable files
        if (!th_file_is_directory(file) && !th_file_is_executable(file))
        {
            // drop to the folder instead
            g_object_unref(G_OBJECT(file));
            gtk_tree_path_free(path);
            path = NULL;
        }
    }

    // if we don't have a path yet, we'll drop to the folder instead
    if (G_UNLIKELY(path == NULL))
    {
        // determine the current directory
        file = navigator_get_current_directory(THUNAR_NAVIGATOR(standard_view));
        if (G_LIKELY(file != NULL))
            g_object_ref(G_OBJECT(file));
    }

    // return the path(if any)
    if (G_LIKELY(path_return != NULL))
        *path_return = path;
    else if (G_LIKELY(path != NULL))
        gtk_tree_path_free(path);

    return file;
}

static GdkDragAction _standard_view_get_dest_actions(StandardView *standard_view,
                                      GdkDragContext     *context,
                                      gint                x,
                                      gint                y,
                                      guint               timestamp,
                                      ThunarFile        **file_return)
{
    GdkDragAction actions = 0;
    GdkDragAction action = 0;
    GtkTreePath  *path;
    ThunarFile   *file;

    // determine the file and path for the given coordinates
    file = _standard_view_get_drop_file(standard_view, x, y, &path);

    // check if we can drop there
    if (G_LIKELY(file != NULL))
    {
        // determine the possible drop actions for the file(and the suggested action if any)
        actions = th_file_accepts_drop(file, standard_view->priv->drop_file_list, context, &action);
        if (G_LIKELY(actions != 0))
        {
            // tell the caller about the file(if it's interested)
            if (G_UNLIKELY(file_return != NULL))
                *file_return = THUNAR_FILE(g_object_ref(G_OBJECT(file)));
        }
    }

    // reset path if we cannot drop
    if (G_UNLIKELY(action == 0 && path != NULL))
    {
        gtk_tree_path_free(path);
        path = NULL;
    }

    /* setup the drop-file for the icon renderer, so the user
     * gets good visual feedback for the drop target.
     */
    g_object_set(G_OBJECT(standard_view->icon_renderer), "drop-file",(action != 0) ? file : NULL, NULL);

    // do the view highlighting
    if (standard_view->priv->drop_highlight !=(path == NULL && action != 0))
    {
        standard_view->priv->drop_highlight =(path == NULL && action != 0);
        gtk_widget_queue_draw(GTK_WIDGET(standard_view));
    }

    // do the item highlighting
   (*STANDARD_VIEW_GET_CLASS(standard_view)->highlight_path)(standard_view, path);

    // tell Gdk whether we can drop here
    gdk_drag_status(context, action, timestamp);

    // clean up
    if (G_LIKELY(file != NULL))
        g_object_unref(G_OBJECT(file));
    if (G_LIKELY(path != NULL))
        gtk_tree_path_free(path);

    return actions;
}

static gboolean _standard_view_drag_scroll_timer(gpointer user_data)
{
    StandardView *standard_view = STANDARD_VIEW(user_data);
    gint        y, x;
    gint        w, h;

    THUNAR_THREADS_ENTER

    // verify that we are realized
    if (G_LIKELY(gtk_widget_get_realized(GTK_WIDGET(standard_view))))
    {
        // determine pointer location and window geometry
        GdkWindow *window = gtk_widget_get_window(
                                gtk_bin_get_child(GTK_BIN(standard_view)));

        GdkSeat *seat = gdk_display_get_default_seat(gdk_display_get_default());
        GdkDevice *pointer = gdk_seat_get_pointer(seat);

        gdk_window_get_device_position(window, pointer, &x, &y, NULL);
        gdk_window_get_geometry(window, NULL, NULL, &w, &h);

        // check if we are near the edge(vertical)
        gint offset = y -(2 * 20);

        if (G_UNLIKELY(offset > 0))
            offset = MAX(y -(h - 2 * 20), 0);

        GtkAdjustment *adjustment;
        gfloat value;

        // change the vertical adjustment appropriately
        if (G_UNLIKELY(offset != 0))
        {
            // determine the vertical adjustment
            adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(standard_view));

            // determine the new value
            value = CLAMP(gtk_adjustment_get_value(adjustment) + 2 * offset, gtk_adjustment_get_lower(adjustment), gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));

            // apply the new value
            gtk_adjustment_set_value(adjustment, value);
        }

        // check if we are near the edge(horizontal)
        offset = x -(2 * 20);

        if (G_UNLIKELY(offset > 0))
            offset = MAX(x -(w - 2 * 20), 0);

        // change the horizontal adjustment appropriately
        if (G_UNLIKELY(offset != 0))
        {
            // determine the vertical adjustment
            adjustment = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(standard_view));

            // determine the new value
            value = CLAMP(gtk_adjustment_get_value(adjustment) + 2 * offset, gtk_adjustment_get_lower(adjustment), gtk_adjustment_get_upper(adjustment) - gtk_adjustment_get_page_size(adjustment));

            // apply the new value
            gtk_adjustment_set_value(adjustment, value);
        }
    }

    THUNAR_THREADS_LEAVE

    return TRUE;
}

static void _standard_view_drag_scroll_timer_destroy(gpointer user_data)
{
    STANDARD_VIEW(user_data)->priv->drag_scroll_timer_id = 0;
}

static void _standard_view_reload_directory(GPid pid, gint status,
                                            gpointer user_data)
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


