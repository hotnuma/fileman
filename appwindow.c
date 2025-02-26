/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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
#include <appwindow.h>
#include <marshal.h>

#include <clipboard.h>
#include <devmonitor.h>
#include <browser.h>
#include <component.h>
#include <navigator.h>

#include <locationbar.h>
#include <locationentry.h>
#include <sidepane.h>
#include <treepane.h>
#include <baseview.h>
#include <standardview.h>
#include <detailview.h>
#include <statusbar.h>
#include <dialogs.h>
#include <preferences.h>

#include <syslog.h>

// AppWindow ------------------------------------------------------------------

static void window_dispose(GObject *object);
static void window_finalize(GObject *object);
static void window_realize(GtkWidget *widget);
static void window_unrealize(GtkWidget *widget);
static gboolean window_reload(AppWindow *window, gboolean reload_info);
static void window_get_property(GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec);
static void window_set_property(GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec);
static ThunarFile* window_get_current_directory(AppWindow *window);
// window_set_current_directory
static void _window_current_directory_changed(ThunarFile *current_directory,
                                              AppWindow *window);
static void _window_history_changed(AppWindow *window);
static void _window_update_window_icon(AppWindow *window);
static void _window_set_zoom_level(AppWindow *window,
                                   ThunarZoomLevel zoom_level);

// Window Init ----------------------------------------------------------------

static void _window_screen_changed(GtkWidget *widget, GdkScreen *old_screen,
                                   gpointer userdata);
static gboolean _window_delete(AppWindow *window,
                               GdkEvent *event, gpointer data);
static void _window_device_pre_unmount(DeviceMonitor *device_monitor,
                                       ThunarDevice *device,
                                       GFile *root_file,
                                       AppWindow *window);
static void _window_device_changed(DeviceMonitor *device_monitor,
                                   ThunarDevice *device, AppWindow *window);
static gboolean _window_propagate_key_event(GtkWindow *window,
                                            GdkEvent *key_event,
                                            gpointer user_data);
static void _window_select_files(AppWindow *window, GList *path_list);
// toolbar
static gboolean _window_history_clicked(GtkWidget *button,
                                        GdkEventButton *event,
                                        GtkWidget *window);
static gboolean _window_button_press_event(GtkWidget *view,
                                           GdkEventButton *event,
                                           AppWindow *window);
static void _window_handle_reload_request(AppWindow *window);
static void _window_update_location_bar_visible(AppWindow *window);
static gboolean _window_save_paned(AppWindow *window);

// Create Views ---------------------------------------------------------------

static void _window_create_sidepane(AppWindow *window);
static void _window_create_detailview(AppWindow *window);
static void _window_notify_loading(BaseView *view, GParamSpec *pspec,
                                   AppWindow *window);
static void _window_start_open_location(AppWindow *window,
                                        const gchar *initial_text);
static void _window_binding_create(AppWindow *window, gpointer src_object,
                                   const gchar *src_prop, gpointer dst_object,
                                   const gchar *dst_prop, GBindingFlags flags);
static void _window_binding_destroyed(gpointer data, GObject *binding);

// Actions --------------------------------------------------------------------

static void _window_action_back(AppWindow *window);
static void _window_action_forward(AppWindow *window);
static void _window_action_go_up(AppWindow *window);
static void _window_action_open_home(AppWindow *window);
static void _window_action_key_rename(AppWindow *window);
static void _window_action_key_reload(AppWindow *window, GtkWidget *menu_item);
static void _window_action_key_show_hidden(AppWindow *window);
static void _window_action_key_trash(AppWindow *window);
static void _window_action_debug(AppWindow *window, GtkWidget *menu_item);

// Public ---------------------------------------------------------------------

// window_redirect_tooltips
static void _window_redirect_tooltips_r(GtkWidget *menu_item,
                                        AppWindow *window);
static void _window_menu_item_selected(AppWindow *window,
                                       GtkWidget *menu_item);
static void _window_menu_item_deselected(AppWindow *window,
                                         GtkWidget *menu_item);
static GtkWidget* _window_get_focused_tree_view(AppWindow *window);

// Actions --------------------------------------------------------------------

static XfceGtkActionEntry _window_actions[] =
{
    {WINDOW_ACTION_BACK,
     "<Actions>/StandardView/back",
     "<Alt>Left",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Back"),
     N_("Go to the previous visited folder"),
     "go-previous-symbolic",
     G_CALLBACK(_window_action_back)},

    {WINDOW_ACTION_FORWARD,
     "<Actions>/StandardView/forward",
     "<Alt>Right",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Forward"),
     N_("Go to the next visited folder"),
     "go-next-symbolic",
     G_CALLBACK(_window_action_forward)},

    {WINDOW_ACTION_OPEN_PARENT,
     "<Actions>/AppWindow/open-parent",
     "<Alt>Up",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Open _Parent"),
     N_("Open the parent folder"),
     "go-up-symbolic",
     G_CALLBACK(_window_action_go_up)},

    {WINDOW_ACTION_OPEN_HOME,
     "<Actions>/AppWindow/open-home",
     "<Alt>Home",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Home"),
     N_("Go to the home folder"),
     "go-home-symbolic",
     G_CALLBACK(_window_action_open_home)},

    {WINDOW_ACTION_KEY_BACK,
     "<Actions>/StandardView/key-back",
     "BackSpace",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_back)},

    {WINDOW_ACTION_KEY_RENAME,
     "<Actions>/StandardView/key-rename",
     "F2",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_key_rename)},

    {WINDOW_ACTION_KEY_RELOAD,
     "<Actions>/AppWindow/key-reload",
     "F5",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_key_reload)},

    {WINDOW_ACTION_KEY_TRASH,
     "<Actions>/StandardView/key-trash",
     "Delete",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_key_trash)},

    {WINDOW_ACTION_KEY_TRASH,
     "<Actions>/StandardView/key-trash-2",
     "KP_Delete",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_key_trash)},

    {WINDOW_ACTION_KEY_SHOW_HIDDEN,
     "<Actions>/AppWindow/show-hidden",
     "<Primary>h",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_key_show_hidden)},

    {WINDOW_ACTION_DEBUG,
     "<Actions>/AppWindow/debug",
     "<Primary>d",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_window_action_debug)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_window_actions, \
    G_N_ELEMENTS(_window_actions), \
    id)

// Allocation -----------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_ZOOM_LEVEL,
};

enum
{
    RELOAD,
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_RESET,
    LAST_SIGNAL,
};

static guint _window_signals[LAST_SIGNAL];

struct _AppWindowClass
{
    GtkWindowClass __parent__;

    // internal action signals
    gboolean (*reload) (AppWindow *window, gboolean reload_info);
    gboolean (*zoom_in) (AppWindow *window);
    gboolean (*zoom_out) (AppWindow *window);
    gboolean (*zoom_reset) (AppWindow *window);
};

struct _AppWindow
{
    GtkWindow       __parent__;

    ClipboardManager *clipboard;
    IconFactory     *icon_factory;
    // to be able to change folder on "device-pre-unmount" if required
    DeviceMonitor   *device_monitor;

    GtkWidget       *grid;
    GtkWidget       *toolbar;
    GtkWidget       *toolbar_item_back;
    GtkWidget       *toolbar_item_forward;
    GtkWidget       *toolbar_item_parent;
    GtkWidget       *location_bar;
    GtkWidget       *paned;
    GtkWidget       *sidepane;
    GtkWidget       *view_grid;
    GtkWidget       *view;
    GtkWidget       *statusbar;

    ThunarFile      *current_directory;
    gboolean        show_hidden;
    ThunarZoomLevel zoom_level;

    ThunarLauncher  *launcher;

    GtkAccelGroup   *accel_group;
    GSList          *view_bindings;
    gulong          history_changed_id;

    // Takes care to select a file after e.g. rename/create
    GClosure        *select_files_closure;

    // unused
    //GType           toggle_sidepane_type;
    //guint           save_geometry_timer_id;
};

G_DEFINE_TYPE_WITH_CODE(AppWindow, window, GTK_TYPE_WINDOW,
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARBROWSER, NULL))

static void window_class_init(AppWindowClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = window_dispose;
    gobject_class->finalize = window_finalize;
    gobject_class->get_property = window_get_property;
    gobject_class->set_property = window_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->realize = window_realize;
    gtkwidget_class->unrealize = window_unrealize;

    klass->reload = window_reload;
    klass->zoom_in = NULL;
    klass->zoom_out = NULL;
    klass->zoom_reset = NULL;

    xfce_gtk_translate_action_entries(_window_actions,
                                      G_N_ELEMENTS(_window_actions));

    g_object_class_install_property(gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    g_param_spec_object("current-directory",
                                             "current-directory",
                                             "current-directory",
                                             TYPE_THUNARFILE,
                                             E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_ZOOM_LEVEL,
                                    g_param_spec_enum("zoom-level",
                                             "zoom-level",
                                             "zoom-level",
                                             THUNAR_TYPE_ZOOM_LEVEL,
                                             THUNAR_ZOOM_LEVEL_100_PERCENT,
                                             E_PARAM_READWRITE));

    _window_signals[RELOAD] =
        g_signal_new(I_("reload"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(AppWindowClass, reload),
                     g_signal_accumulator_true_handled, NULL,
                     _thunar_marshal_BOOLEAN__BOOLEAN,
                     G_TYPE_BOOLEAN, 1,
                     G_TYPE_BOOLEAN);

    _window_signals[ZOOM_IN] =
        g_signal_new(I_("zoom-in"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(AppWindowClass, zoom_in),
                     g_signal_accumulator_true_handled, NULL,
                     _thunar_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0);

    _window_signals[ZOOM_OUT] =
        g_signal_new(I_("zoom-out"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(AppWindowClass, zoom_out),
                     g_signal_accumulator_true_handled, NULL,
                     _thunar_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0);

    _window_signals[ZOOM_RESET] =
        g_signal_new(I_("zoom-reset"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                     G_STRUCT_OFFSET(AppWindowClass, zoom_reset),
                     g_signal_accumulator_true_handled, NULL,
                     _thunar_marshal_BOOLEAN__VOID,
                     G_TYPE_BOOLEAN, 0);
}

static void window_init(AppWindow *window)
{
    Preferences *prefs = get_preferences();

    window->accel_group = gtk_accel_group_new();
    xfce_gtk_accel_map_add_entries(_window_actions,
                                   G_N_ELEMENTS(_window_actions));

    xfce_gtk_accel_group_connect_action_entries(window->accel_group,
                                                _window_actions,
                                                G_N_ELEMENTS(_window_actions),
                                                window);

    gtk_window_add_accel_group(GTK_WINDOW(window), window->accel_group);

    window->show_hidden = false;

    // update the visual on screen_changed events
    g_signal_connect(window, "screen-changed",
                     G_CALLBACK(_window_screen_changed), NULL);

    // invoke the window_screen_changed function to initially set
    // the best possible visual.
    _window_screen_changed(GTK_WIDGET(window), NULL, NULL);

    // set up a handler to confirm exit when there are multiple tabs open 
    g_signal_connect(window, "delete-event",
                     G_CALLBACK(_window_delete), NULL);

    // connect to the volume monitor
    window->device_monitor = devmon_get();

    g_signal_connect(window->device_monitor, "device-pre-unmount",
                     G_CALLBACK(_window_device_pre_unmount), window);

    g_signal_connect(window->device_monitor, "device-removed",
                     G_CALLBACK(_window_device_changed), window);

    g_signal_connect(window->device_monitor, "device-changed",
                     G_CALLBACK(_window_device_changed), window);

    window->icon_factory = iconfact_get_default();

    // Catch key events before accelerators get processed
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(_window_propagate_key_event), NULL);

    g_signal_connect(window, "key-release-event",
                     G_CALLBACK(_window_propagate_key_event), NULL);

    window->select_files_closure = g_cclosure_new_swap(
                                G_CALLBACK(_window_select_files),
                                window,
                                NULL);
    g_closure_ref(window->select_files_closure);
    g_closure_sink(window->select_files_closure);

    window->launcher = g_object_new(THUNAR_TYPE_LAUNCHER,
                                    "widget",
                                    GTK_WIDGET(window),
                                    "select-files-closure",
                                    window->select_files_closure,
                                    NULL);

    g_object_bind_property(G_OBJECT(window),
                           "current-directory",
                           G_OBJECT(window->launcher),
                           "current-directory",
                           G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped(G_OBJECT(window->launcher),
                             "change-directory",
                             G_CALLBACK(window_set_current_directory),
                             window);

    launcher_append_accelerators(window->launcher, window->accel_group);

    gtk_window_set_default_size(GTK_WINDOW(window),
                                prefs->window_width,
                                prefs->window_height);

    if (prefs->window_maximized)
        gtk_window_maximize(GTK_WINDOW(window));

    // add thunar style class for easier theming
    GtkStyleContext *context =
            gtk_widget_get_style_context(GTK_WIDGET(window));
    gtk_style_context_add_class(context, "thunar");

    // Main Grid --------------------------------------------------------------

    window->grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), window->grid);
    gtk_widget_show(window->grid);

    // Toolbar ----------------------------------------------------------------

    window->toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(window->toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(GTK_TOOLBAR(window->toolbar),
                              GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_hexpand(window->toolbar, TRUE);
    gtk_grid_attach(GTK_GRID(window->grid), window->toolbar, 0, 0, 1, 1);

    // back
    window->toolbar_item_back = xfce_gtk_tool_button_new_from_action_entry(
                get_action_entry(WINDOW_ACTION_BACK),
                G_OBJECT(window),
                GTK_TOOLBAR(window->toolbar));
    g_signal_connect(G_OBJECT(window->toolbar_item_back),
                     "button-press-event",
                     G_CALLBACK(_window_history_clicked),
                     G_OBJECT(window));

    // forward
    window->toolbar_item_forward = xfce_gtk_tool_button_new_from_action_entry(
                get_action_entry(WINDOW_ACTION_FORWARD),
                G_OBJECT(window),
                GTK_TOOLBAR(window->toolbar));
    g_signal_connect(G_OBJECT(window->toolbar_item_forward),
                     "button-press-event",
                     G_CALLBACK(_window_history_clicked),
                     G_OBJECT(window));

    // parent
    window->toolbar_item_parent = xfce_gtk_tool_button_new_from_action_entry(
                get_action_entry(WINDOW_ACTION_OPEN_PARENT),
                G_OBJECT(window),
                GTK_TOOLBAR(window->toolbar));

    // home
    xfce_gtk_tool_button_new_from_action_entry(
                get_action_entry(WINDOW_ACTION_OPEN_HOME),
                G_OBJECT(window),
                GTK_TOOLBAR(window->toolbar));

    g_signal_connect(G_OBJECT(window),
                     "button-press-event",
                     G_CALLBACK(_window_button_press_event),
                     G_OBJECT(window));

    window->history_changed_id = 0;

    #if 0
    /* The UCA shortcuts need to be checked 'by hand',
     *  since we dont want to permanently keep menu items for them */
    g_signal_connect(window, "key-press-event",
                     G_CALLBACK(window_check_uca_key_activation), NULL);
    #endif

    // Location bar

    GtkToolItem *tool_item = gtk_tool_item_new();
    gtk_tool_item_set_expand(tool_item, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(window->toolbar), tool_item, -1);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(window->toolbar), FALSE);

    window->location_bar = locbar_new();

    g_object_bind_property(G_OBJECT(window),
                           "current-directory",
                           G_OBJECT(window->location_bar),
                           "current-directory",
                           G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped(G_OBJECT(window->location_bar),
                             "change-directory",
                             G_CALLBACK(window_set_current_directory),
                             window);

    g_signal_connect_swapped(G_OBJECT(window->location_bar),
                             "reload-requested",
                             G_CALLBACK(_window_handle_reload_request),
                             window);

    g_signal_connect_swapped(G_OBJECT(window->location_bar),
                             "entry-done",
                             G_CALLBACK(_window_update_location_bar_visible),
                             window);

    gtk_container_add(GTK_CONTAINER(tool_item), window->location_bar);

    gtk_widget_show_all(window->toolbar);
    _window_update_location_bar_visible(window);

    // Paned ------------------------------------------------------------------

    window->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(window->paned), 0);
    gtk_widget_set_hexpand(window->paned, TRUE);
    gtk_widget_set_vexpand(window->paned, TRUE);
    gtk_grid_attach(GTK_GRID(window->grid), window->paned, 0, 1, 1, 1);
    gtk_widget_show(window->paned);

    // determine the last separator position and apply it to the paned view
    gtk_paned_set_position(GTK_PANED(window->paned),
                           prefs->separator_position);

    g_signal_connect_swapped(window->paned, "accept-position",
                             G_CALLBACK(_window_save_paned), window);

    g_signal_connect_swapped(window->paned, "button-release-event",
                             G_CALLBACK(_window_save_paned), window);

    // Side Treeview
    _window_create_sidepane(window);

    // Right Grid
    window->view_grid = gtk_grid_new();
    gtk_paned_pack2(GTK_PANED(window->paned), window->view_grid, TRUE, FALSE);
    gtk_widget_show(window->view_grid);

    // Detail View
    _window_create_detailview(window);

    // setup a new statusbar
    window->statusbar = statusbar_new();
    gtk_widget_set_hexpand(window->statusbar, TRUE);
    //gtk_grid_attach(GTK_GRID(window->view_grid),
    //  window->statusbar, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(window->grid), window->statusbar, 0, 2, 1, 1);
    gtk_widget_show(window->statusbar);

    _window_binding_create(window,
                           window->view, "selected-files",
                           window->sidepane, "selected-files",
                           G_BINDING_SYNC_CREATE);

    _window_binding_create(window,
                           window->view, "statusbar-text",
                           window->statusbar, "text",
                           G_BINDING_SYNC_CREATE);

    // ensure that all the view types are registered
    g_type_ensure(TYPE_DETAILVIEW);
}

static void window_realize(GtkWidget *widget)
{
    AppWindow *window = APP_WINDOW(widget);

    // let the GtkWidget class perform the realize operation
    GTK_WIDGET_CLASS(window_parent_class)->realize(widget);

    /* connect to the clipboard manager of the new display and be sure to
     * redraw the window whenever the clipboard contents change to make sure
     * we always display up2date state. */
    window->clipboard = clipman_get_for_display(gtk_widget_get_display(widget));

    g_signal_connect_swapped(G_OBJECT(window->clipboard), "changed",
                             G_CALLBACK(gtk_widget_queue_draw), widget);
}

static void window_unrealize(GtkWidget *widget)
{
    AppWindow *window = APP_WINDOW(widget);

    // disconnect from the clipboard manager
    g_signal_handlers_disconnect_by_func(G_OBJECT(window->clipboard),
                                         gtk_widget_queue_draw,
                                         widget);

    // let the GtkWidget class unrealize the window
    GTK_WIDGET_CLASS(window_parent_class)->unrealize(widget);

    /* drop the reference on the clipboard manager, we do this after letting
     * the GtkWidget class unrealise the window to prevent the clipboard
     * being disposed during the unrealize  */
    g_object_unref(G_OBJECT(window->clipboard));
}

static void window_dispose(GObject *object)
{
    AppWindow  *window = APP_WINDOW(object);

    // indicate that history items are out of use
    window->toolbar_item_back = NULL;
    window->toolbar_item_forward = NULL;

    if (window->accel_group != NULL)
    {
        gtk_accel_group_disconnect(window->accel_group, NULL);
        gtk_window_remove_accel_group(GTK_WINDOW(window), window->accel_group);
        g_object_unref(window->accel_group);
        window->accel_group = NULL;
    }

    // disconnect from the current-directory
    window_set_current_directory(window, NULL);

    G_OBJECT_CLASS(window_parent_class)->dispose(object);
}

static void window_finalize(GObject *object)
{
    AppWindow *window = APP_WINDOW(object);

    // disconnect from the volume monitor
    g_signal_handlers_disconnect_matched(window->device_monitor,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL,
                                         window);

    g_object_unref(window->device_monitor);

    g_object_unref(window->icon_factory);
    g_object_unref(window->launcher);

    g_closure_invalidate(window->select_files_closure);
    g_closure_unref(window->select_files_closure);

    G_OBJECT_CLASS(window_parent_class)->finalize(object);
}

static gboolean window_reload(AppWindow *window, gboolean reload_info)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), FALSE);

    // force the view to reload
    if (window->view == NULL)
        return FALSE;

    baseview_reload(BASEVIEW(window->view), reload_info);

    return TRUE;
}


// properties -----------------------------------------------------------------

static void window_get_property(GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    AppWindow *window = APP_WINDOW(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, window_get_current_directory(window));
        break;

    case PROP_ZOOM_LEVEL:
        g_value_set_enum(value, window->zoom_level);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void window_set_property(GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
    (void) pspec;
    AppWindow *window = APP_WINDOW(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        window_set_current_directory(window, g_value_get_object(value));
        break;

    case PROP_ZOOM_LEVEL:
        _window_set_zoom_level(window, g_value_get_enum(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static ThunarFile* window_get_current_directory(AppWindow *window)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), NULL);

    return window->current_directory;
}

void window_set_current_directory(AppWindow *window, ThunarFile *current_directory)
{
    e_return_if_fail(APP_IS_WINDOW(window));
    e_return_if_fail(current_directory == NULL
                     || IS_THUNARFILE(current_directory));

    // check if we already display the requested directory
    if (window->current_directory == current_directory)
        return;

    #if 0
    if (!current_directory)
    {
        DPRINT("window_set_current_directory: (null)\n");
    }
    else
    {
        gchar *current_uri = th_file_get_uri(current_directory);
        DPRINT("window_set_current_directory: %s\n", current_uri);
        g_free(current_uri);
    }
    #endif

    // disconnect from the previously active directory
    if (window->current_directory != NULL)
    {
        // disconnect signals and release reference
        g_signal_handlers_disconnect_by_func(G_OBJECT(window->current_directory),
                                             _window_current_directory_changed,
                                             window);

        g_object_unref(G_OBJECT(window->current_directory));
        window->current_directory = NULL;
    }

    // connect to the new directory
    if (current_directory != NULL)
    {
        // take a reference on the file
        g_object_ref(G_OBJECT(current_directory));

        window->current_directory = current_directory;

        // connect the "changed"/"destroy" signals
        g_signal_connect(G_OBJECT(current_directory), "changed",
                         G_CALLBACK(_window_current_directory_changed),
                         window);

        // update window icon and title
        _window_current_directory_changed(current_directory, window);

        if (window->view != NULL)
        {
            // grab the focus to the main view
            gtk_widget_grab_focus(window->view);
        }

        _window_history_changed(window);

        gtk_widget_set_sensitive(
                        window->toolbar_item_parent,
                        !e_file_is_root(th_file_get_file(current_directory)));
    }

    /* tell everybody that we have a new "current-directory",
     * we do this first so other widgets display the new
     * state already while the folder view is loading. */

    g_object_notify(G_OBJECT(window), "current-directory");
}

static void _window_current_directory_changed(ThunarFile *current_directory,
                                              AppWindow *window)
{
    e_return_if_fail(APP_IS_WINDOW(window));
    e_return_if_fail(IS_THUNARFILE(current_directory));
    e_return_if_fail(window->current_directory == current_directory);

    gboolean show_full_path = false;

    gchar *parse_name = NULL;
    const gchar *name;

    if (show_full_path)
    {
        parse_name = g_file_get_parse_name(th_file_get_file(current_directory));
        name = parse_name;
    }
    else
    {
        name = th_file_get_display_name(current_directory);
    }

    // set window title
    gtk_window_set_title(GTK_WINDOW(window), name);
    g_free(parse_name);

    // set window icon
    _window_update_window_icon(window);
}

static void _window_history_changed(AppWindow *window)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    if (window->view == NULL)
        return;

    ThunarHistory *history;
    history = standardview_get_history(STANDARD_VIEW(window->view));
    if (history == NULL)
        return;

    if (window->toolbar_item_back != NULL)
        gtk_widget_set_sensitive(window->toolbar_item_back,
                                 history_has_back(history));

    if (window->toolbar_item_forward != NULL)
        gtk_widget_set_sensitive(window->toolbar_item_forward,
                                 history_has_forward(history));
}

static void _window_update_window_icon(AppWindow *window)
{
    const gchar *icon_name = "folder";

    gboolean change_window_icon = true;

    if (change_window_icon)
    {
        GtkIconTheme *icon_theme =
            gtk_icon_theme_get_for_screen(
                gtk_window_get_screen(GTK_WINDOW(window)));

        icon_name = th_file_get_icon_name(window->current_directory,
                                          FILE_ICON_STATE_DEFAULT,
                                          icon_theme);
    }

    gtk_window_set_icon_name(GTK_WINDOW(window), icon_name);
}

static void _window_set_zoom_level(AppWindow *window, ThunarZoomLevel zoom_level)
{
    e_return_if_fail(APP_IS_WINDOW(window));
    e_return_if_fail(zoom_level < THUNAR_ZOOM_N_LEVELS);

    // check if we have a new zoom level
    if (window->zoom_level == zoom_level)
        return;

    // remember the new zoom level
    window->zoom_level = zoom_level;

    // notify listeners
    g_object_notify(G_OBJECT(window), "zoom-level");
}

// Events ---------------------------------------------------------------------

static void _window_screen_changed(GtkWidget *widget, GdkScreen *old_screen,
                                   gpointer userdata)
{
    (void) old_screen;
    (void) userdata;

    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    if (visual == NULL || !gdk_screen_is_composited(screen))
        visual = gdk_screen_get_system_visual(screen);

    gtk_widget_set_visual(GTK_WIDGET(widget), visual);
}

static gboolean _window_delete(AppWindow *window, GdkEvent *event, gpointer data)
{
    (void) event;
    (void) data;

    GtkWidget *widget = GTK_WIDGET(window);

    if (gtk_widget_get_visible(widget) == false)
        return false;

    GdkWindowState state = gdk_window_get_state(gtk_widget_get_window(widget));

    Preferences *prefs = get_preferences();
    prefs->window_maximized = ((state & (GDK_WINDOW_STATE_MAXIMIZED
                                         | GDK_WINDOW_STATE_FULLSCREEN)) != 0);

    if (!prefs->window_maximized)
    {
        gtk_window_get_size(GTK_WINDOW(window),
                            &prefs->window_width,
                            &prefs->window_height);
    }

    return false;
}

// Monitor
static void _window_device_pre_unmount(DeviceMonitor *device_monitor,
                                       ThunarDevice  *device,
                                       GFile         *root_file,
                                       AppWindow     *window)
{
    e_return_if_fail(IS_DEVICE_MONITOR(device_monitor));
    e_return_if_fail(window->device_monitor == device_monitor);
    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(G_IS_FILE(root_file));
    e_return_if_fail(APP_IS_WINDOW(window));

    // nothing to do if we don't have a current directory
    if (window->current_directory == NULL)
        return;

    // check if the file is the current directory or an ancestor of the current directory
    if (g_file_equal(th_file_get_file(window->current_directory), root_file)
            || th_file_is_gfile_ancestor(window->current_directory, root_file))
    {
        // change to the home folder
        _window_action_open_home(window);
    }
}

static void _window_device_changed(DeviceMonitor *device_monitor,
                                   ThunarDevice *device,
                                   AppWindow *window)
{
    e_return_if_fail(IS_DEVICE_MONITOR(device_monitor));
    e_return_if_fail(window->device_monitor == device_monitor);
    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(APP_IS_WINDOW(window));

    if (th_device_is_mounted(device))
        return;

    GFile *root_file = th_device_get_root(device);

    if (root_file == NULL)
        return;

    _window_device_pre_unmount(device_monitor, device, root_file, window);
    g_object_unref(root_file);
}

static gboolean _window_propagate_key_event(GtkWindow* window, GdkEvent *key_event,
                                            gpointer user_data)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), GDK_EVENT_PROPAGATE);

    (void) user_data;

    GtkWidget *focused_widget = gtk_window_get_focus(window);

    /* Turn the accelerator priority around globally,
     * so that the focused widget always gets the accels first.
     * Implementing this cleanly while maintaining some wanted accels
     * (like Ctrl+N and exo accels) is a lot of work. So we resort to
     * only priorize GtkEditable, because that is the easiest way to
     * fix the right-ahead problem. */
    if (focused_widget != NULL && GTK_IS_EDITABLE(focused_widget))
        return gtk_window_propagate_key_event(window,(GdkEventKey *) key_event);

    return GDK_EVENT_PROPAGATE;
}

static void _window_select_files(AppWindow *window, GList *files_to_selected)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    /* If possible, reload the current directory to make sure new files
     * got added to the view */

    ThunarFolder *thunar_folder = th_folder_get_for_thfile(window->current_directory);

    if (thunar_folder != NULL)
    {
        th_folder_load(thunar_folder, FALSE);
        g_object_unref(thunar_folder);
    }

    GList *thunar_files = NULL;

    for (GList *lp = files_to_selected; lp != NULL; lp = lp->next)
    {
        thunar_files = g_list_append(thunar_files, th_file_get(G_FILE(lp->data), NULL));
    }

    baseview_set_selected_files(BASEVIEW(window->view), thunar_files);
    g_list_free_full(thunar_files, g_object_unref);
}

static gboolean _window_history_clicked(GtkWidget *button, GdkEventButton *event,
                                        GtkWidget *data)
{
    e_return_val_if_fail(APP_IS_WINDOW(data), FALSE);

    AppWindow *window = APP_WINDOW(data);

    if (event->button == 3)
    {
        ThunarHistory *history = standardview_get_history(STANDARD_VIEW(window->view));

        if (button == window->toolbar_item_back)
            history_show_menu(history, THUNARHISTORY_MENU_BACK, button);

        else if (button == window->toolbar_item_forward)
            history_show_menu(history, THUNARHISTORY_MENU_FORWARD, button);

        else
            g_warning("This button is not able to spawn a history menu");
    }

    return FALSE;
}

static gboolean _window_button_press_event(GtkWidget *view, GdkEventButton *event,
                                           AppWindow *window)
{
    (void) view;

    e_return_val_if_fail(APP_IS_WINDOW(window), FALSE);

    if (event->type != GDK_BUTTON_PRESS)
        return GDK_EVENT_PROPAGATE;

    const XfceGtkActionEntry* action_entry;

    if (event->button == 8)
    {
        action_entry = get_action_entry(WINDOW_ACTION_BACK);

        ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW(window));

        return GDK_EVENT_STOP;
    }

    else if (event->button == 9)
    {
        action_entry = get_action_entry(WINDOW_ACTION_FORWARD);

        ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW(window));

        return GDK_EVENT_STOP;
    }

    return GDK_EVENT_PROPAGATE;
}

static void _window_handle_reload_request(AppWindow *window)
{
    gboolean result;

    // force the view to reload
    g_signal_emit(G_OBJECT(window), _window_signals[RELOAD], 0, TRUE, &result);

}

static void _window_update_location_bar_visible(AppWindow *window)
{
    gtk_widget_show(window->toolbar);
}

static gboolean _window_save_paned(AppWindow *window)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), FALSE);

    Preferences *prefs = get_preferences();
    prefs->separator_position = gtk_paned_get_position(GTK_PANED(window->paned));

    // for button release event
    return false;
}

// Create Views ---------------------------------------------------------------

static void _window_create_sidepane(AppWindow *window)
{
    e_return_if_fail(APP_IS_WINDOW(window));
    e_return_if_fail(window->sidepane == NULL);

    // allocate the new side pane widget
    window->sidepane = g_object_new(TYPE_TREEPANE, NULL);

    gtk_widget_set_size_request(window->sidepane, 0, -1);

    g_object_bind_property(G_OBJECT(window), "current-directory",
                           G_OBJECT(window->sidepane), "current-directory",
                           G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped(G_OBJECT(window->sidepane), "change-directory",
                             G_CALLBACK(window_set_current_directory), window);

    GtkStyleContext *context = gtk_widget_get_style_context(window->sidepane);
    gtk_style_context_add_class(context, "sidebar");

    gtk_paned_pack1(GTK_PANED(window->paned), window->sidepane, FALSE, FALSE);
    gtk_widget_show(window->sidepane);

    // connect the side pane widget to the view (if any)
    //if (window->view != NULL))
    //{
    //    _window_binding_create(window,
    //                           window->view, "selected-files",
    //                           window->sidepane, "selected-files",
    //                           G_BINDING_SYNC_CREATE);
    //}

    // apply show_hidden config to tree pane
    sidepane_set_show_hidden(SIDEPANE(window->sidepane),
                             window->show_hidden);
}

// Detail View ----------------------------------------------------------------

static void _window_create_detailview(AppWindow *window)
{
    e_return_if_fail(window->view == NULL);
    e_return_if_fail(window->current_directory == NULL);

    GtkWidget *detail_view = g_object_new(TYPE_DETAILVIEW,
                                          "fixed-columns", TRUE,
                                          NULL);
    gtk_widget_set_hexpand(detail_view, TRUE);
    gtk_widget_set_vexpand(detail_view, TRUE);
    gtk_grid_attach(GTK_GRID(window->view_grid), detail_view, 0, 0, 1, 1);

    baseview_set_show_hidden(BASEVIEW(detail_view), window->show_hidden);
    gtk_widget_show(detail_view);

    g_signal_connect(G_OBJECT(detail_view), "notify::loading",
                     G_CALLBACK(_window_notify_loading), window);

    g_signal_connect_swapped(G_OBJECT(detail_view), "start-open-location",
                             G_CALLBACK(_window_start_open_location), window);

    g_signal_connect_swapped(G_OBJECT(detail_view), "change-directory",
                             G_CALLBACK(window_set_current_directory), window);

    g_object_set(G_OBJECT(detail_view), "accel-group", window->accel_group, NULL);

    //if (window->view != NULL))
    //{
    //    // disconnect from previous history
    //    if (window->signal_handler_id_history_changed != 0)
    //    {
    //        history = standardview_get_history(STANDARD_VIEW(window->view));
    //        g_signal_handler_disconnect(history, window->signal_handler_id_history_changed);
    //        window->signal_handler_id_history_changed = 0;
    //    }

    //    // unset view during switch
    //    window->view = NULL;
    //}

    //GSList *view_bindings = window->view_bindings;

    // disconnect existing bindings
    //if (window->view_bindings)
    //{
    //    DPRINT("*** disconnect existing bindings\n");

    //    g_slist_free_full(window->view_bindings, g_object_unref);
    //    window->view_bindings = NULL;
    //}

    // add stock bindings
    _window_binding_create(window,
                           window, "current-directory",
                           detail_view, "current-directory",
                           G_BINDING_DEFAULT);

    _window_binding_create(window,
                           detail_view, "selected-files",
                           window->launcher, "selected-files",
                           G_BINDING_SYNC_CREATE);

    _window_binding_create(window,
                           detail_view, "zoom-level",
                           window, "zoom-level",
                           G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    // connect to the statusbar(if any)
    //if (window->statusbar != NULL))
    //{
    //    _window_binding_create(window,
    //                           detail_view, "statusbar-text",
    //                           window->statusbar, "text",
    //                           G_BINDING_SYNC_CREATE);
    //}

    // activate new view
    window->view = detail_view;

    // connect to the new history
    ThunarHistory *history;
    history = standardview_get_history(STANDARD_VIEW(window->view));

    if (history != NULL)
    {
        if (window->history_changed_id == 0)
        {
            window->history_changed_id =
                g_signal_connect_swapped(G_OBJECT(history), "history-changed",
                                         G_CALLBACK(_window_history_changed),
                                         window);
        }

        _window_history_changed(window);
    }

    // update the selection
    standardview_selection_changed(STANDARD_VIEW(detail_view));

    // ------------------------------------------------------------------------

    // take focus on the new view
    gtk_widget_grab_focus(detail_view);

    //ThunarFile *file = NULL;
    //GList *selected_files = NULL;

    // scroll to the previously visible file in the old view
    //if (file != NULL))
    //    baseview_scroll_to_file(BASEVIEW(detail_view),
    //                            file, FALSE, TRUE, 0.0f, 0.0f);

    // restore the file selection
    //component_set_selected_files(THUNARCOMPONENT(detail_view), selected_files);
    //e_list_free(selected_files);

    // release the file references
    //if (file != NULL))
    //    g_object_unref(G_OBJECT(file));

    // connect to the new history if this is the active view
    //history = standardview_get_history(STANDARD_VIEW(detail_view));

    //window->signal_handler_id_history_changed =
    //    g_signal_connect_swapped(G_OBJECT(history),
    //                             "history-changed",
    //                             G_CALLBACK(_window_history_changed),
    //                             window);
}

static void _window_notify_loading(BaseView *view, GParamSpec *pspec,
                                   AppWindow *window)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));
    e_return_if_fail(APP_IS_WINDOW(window));

    (void) pspec;

    GdkCursor *cursor;

    if (gtk_widget_get_realized(GTK_WIDGET(window))
        && window->view == GTK_WIDGET(view))
    {
        // setup the proper cursor
        if (baseview_get_loading(view))
        {
            cursor = gdk_cursor_new_for_display(gtk_widget_get_display(GTK_WIDGET(view)), GDK_WATCH);
            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window)), cursor);
            g_object_unref(cursor);
        }
        else
        {
            gdk_window_set_cursor(gtk_widget_get_window(GTK_WIDGET(window)), NULL);
        }
    }
}

static void _window_start_open_location(AppWindow *window,
                                        const gchar *initial_text)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    // temporary show the location toolbar, even if it is normally hidden
    gtk_widget_show(window->toolbar);
    locbar_request_entry(LOCATIONBAR(window->location_bar), initial_text);
}

static void _window_binding_create(AppWindow *window, gpointer src_object,
                                   const gchar *src_prop, gpointer dst_object,
                                   const gchar *dst_prop, GBindingFlags flags)
{
    e_return_if_fail(G_IS_OBJECT(src_object));
    e_return_if_fail(G_IS_OBJECT(dst_object));

    GBinding *binding = g_object_bind_property(
                                    G_OBJECT(src_object), src_prop,
                                    G_OBJECT(dst_object), dst_prop,
                                    flags);

    g_object_weak_ref(G_OBJECT(binding), _window_binding_destroyed, window);

    window->view_bindings = g_slist_prepend(window->view_bindings, binding);
}

static void _window_binding_destroyed(gpointer data, GObject *binding)
{
    AppWindow *window = APP_WINDOW(data);

    if (window->view_bindings != NULL)
        window->view_bindings = g_slist_remove(window->view_bindings, binding);
}

// Public ---------------------------------------------------------------------

static GtkWidget* _window_get_focused_tree_view(AppWindow *window)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), NULL);

    GtkWidget *tree_view = GTK_WIDGET(treepane_get_view(
                                     TREEPANE(window->sidepane)));

    GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(window));

    return (tree_view == focused) ? tree_view : NULL;
}

// Actions --------------------------------------------------------------------

static void _window_action_back(AppWindow *window)
{
    ThunarHistory *history;

    e_return_if_fail(APP_IS_WINDOW(window));

    history = standardview_get_history(STANDARD_VIEW(window->view));
    history_action_back(history);
}

static void _window_action_forward(AppWindow *window)
{
    ThunarHistory *history;

    e_return_if_fail(APP_IS_WINDOW(window));

    history = standardview_get_history(STANDARD_VIEW(window->view));
    history_action_forward(history);
}

static void _window_action_go_up(AppWindow *window)
{
    GError *error = NULL;

    ThunarFile *parent = th_file_get_parent(window->current_directory, &error);

    if (parent != NULL)
    {
        window_set_current_directory(window, parent);

        g_object_unref(G_OBJECT(parent));
    }
    else
    {
        // the root folder '/' has no parent. In this special case we do not need a dialog
        if (error->code != G_FILE_ERROR_NOENT)
        {
            dialog_error(GTK_WIDGET(window), error, _("Failed to open parent folder"));
        }

        g_error_free(error);
    }
}

static void _window_action_open_home(AppWindow *window)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    // determine the path to the home directory
    GFile *home = e_file_new_for_home();

    // determine the file for the home directory
    GError *error = NULL;

    ThunarFile *home_file = th_file_get(home, &error);

    if (home_file == NULL)
    {
        // display an error to the user
        dialog_error(GTK_WIDGET(window),
                     error,
                     _("Failed to open the home folder"));
        g_error_free(error);
        g_object_unref(home);

        return;
    }

    // open the home folder
    window_set_current_directory(window, home_file);
    g_object_unref(G_OBJECT(home_file));

    // release our reference on the home path
    g_object_unref(home);
}

static void _window_action_key_reload(AppWindow *window, GtkWidget *menu_item)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    (void) menu_item;

    gboolean result;

    // force the view to reload
    g_signal_emit(G_OBJECT(window), _window_signals[RELOAD], 0, TRUE, &result);

    // update the location bar to show the current directory
    if (window->location_bar != NULL)
        g_object_notify(G_OBJECT(window->location_bar), "current-directory");
}

static void _window_action_key_rename(AppWindow *window)
{
    GtkWidget *tree_view = _window_get_focused_tree_view(window);

    if (tree_view)
    {
        treeview_rename_selected(TREEVIEW(tree_view));
        return;
    }

    launcher_action_rename(window->launcher);
}

static void _window_action_key_show_hidden(AppWindow *window)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    window->show_hidden = !window->show_hidden;

    baseview_set_show_hidden(BASEVIEW(window->view), window->show_hidden);

    if (window->sidepane != NULL)
        sidepane_set_show_hidden(SIDEPANE(window->sidepane), window->show_hidden);
}

static void _window_action_key_trash(AppWindow *window)
{
    GtkWidget *tree_view = _window_get_focused_tree_view(window);

    if (tree_view)
    {
        if (dialog_folder_trash(GTK_WINDOW(window)) == FALSE)
            return;

        treeview_delete_selected(TREEVIEW(tree_view));
        return;
    }

    launcher_action_trash_delete(window->launcher);
}

static void _window_action_debug(AppWindow *window, GtkWidget *menu_item)
{
    (void) window;
    (void) menu_item;

    th_file_cache_dump(NULL);

    //GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(window));
    //const gchar *name = gtk_widget_get_name(focused);
    //syslog(LOG_INFO, "focused widget = %s\n", name);
}

// Public ---------------------------------------------------------------------

ThunarLauncher* window_get_launcher(AppWindow *window)
{
    e_return_val_if_fail(APP_IS_WINDOW(window), NULL);

    return window->launcher;
}

/**
 * window_redirect_menu_tooltips_to_statusbar:
 * @window : a #AppWindow instance.
 * @menu   : #GtkMenu for which all tooltips should be shown in the statusbar
 *
 * All tooltips of the provided #GtkMenu and any submenu will not be shown directly any more.
 * Instead they will be shown in the status bar of the passed #AppWindow
 **/
void window_redirect_tooltips(AppWindow *window,
                                                GtkMenu *menu)
{
    e_return_if_fail(APP_IS_WINDOW(window));
    e_return_if_fail(GTK_IS_MENU(menu));

    gtk_container_foreach(GTK_CONTAINER(menu),
                          (GtkCallback) _window_redirect_tooltips_r,
                          window);
}

static void _window_redirect_tooltips_r(GtkWidget *menu_item,AppWindow *window)
{
    if (GTK_IS_MENU_ITEM(menu_item) == false)
        return;

    GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));

    if (submenu != NULL)
    {
        gtk_container_foreach(GTK_CONTAINER(submenu),
                              (GtkCallback) _window_redirect_tooltips_r,
                              window);
    }

    // this disables to show the tooltip on hover
    gtk_widget_set_has_tooltip(menu_item, FALSE);

    // These method will put the tooltip on the statusbar
    g_signal_connect_swapped(G_OBJECT(menu_item), "select",
                             G_CALLBACK(_window_menu_item_selected), window);
    g_signal_connect_swapped(G_OBJECT(menu_item), "deselect",
                             G_CALLBACK(_window_menu_item_deselected), window);
}

static void _window_menu_item_selected(AppWindow *window, GtkWidget *menu_item)
{
    e_return_if_fail(APP_IS_WINDOW(window));

    // we can only display tooltips if we have a statusbar
    if (window->statusbar == NULL)
        return;

    gchar *tooltip = gtk_widget_get_tooltip_text(menu_item);

    if (tooltip == NULL)
        return;

    // push to the statusbar
    gint id = gtk_statusbar_get_context_id(GTK_STATUSBAR(window->statusbar),
                                           "Menu tooltip");
    gtk_statusbar_push(GTK_STATUSBAR(window->statusbar), id, tooltip);

    g_free(tooltip);
}

static void _window_menu_item_deselected(AppWindow *window, GtkWidget *menu_item)
{
    (void) menu_item;

    e_return_if_fail(APP_IS_WINDOW(window));

    // we can only undisplay tooltips if we have a statusbar
    if (window->statusbar == NULL)
        return;

    // drop the last tooltip from the statusbar
    gint id = gtk_statusbar_get_context_id(GTK_STATUSBAR(window->statusbar),
                                           "Menu tooltip");

    gtk_statusbar_pop(GTK_STATUSBAR(window->statusbar), id);
}


