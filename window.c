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
#include <unistd.h>
#include <locale.h>

#include <gdk/gdkkeysyms.h>

#include <libext.h>

#include <application.h>
#include <thunar-browser.h>
#include <clipman.h>
#include <thunar-details-view.h>
#include <dialogs.h>
#include <thunar-gio-extensions.h>
#include <thunar-gobject-extensions.h>
#include <thunar-gtk-extensions.h>
#include <thunar-history.h>
#include <launcher.h>
#include <thunar-location-entry.h>
#include <marshal.h>
#include <menu.h>
#include <thunar-pango-extensions.h>
#include <preferences.h>
#include <utils.h>
#include <thunar-statusbar.h>
#include <thunar-tree-pane.h>
#include <window.h>
#include <device-mon.h>

#include <glib.h>
#include <syslog.h>

/* Property identifiers */
enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_ZOOM_LEVEL,
};

/* Signal identifiers */
enum
{
    BACK,
    RELOAD,
    TOGGLE_SIDEPANE,
    TOGGLE_MENUBAR,
    ZOOM_IN,
    ZOOM_OUT,
    ZOOM_RESET,
    TAB_CHANGE,
    LAST_SIGNAL,
};

struct _ThunarBookmark
{
    GFile *g_file;
    gchar *name;
};

typedef struct _ThunarBookmark ThunarBookmark;

static void thunar_window_screen_changed(GtkWidget *widget,
                                         GdkScreen *old_screen,
                                         gpointer userdata);
static void thunar_window_dispose(GObject *object);
static void thunar_window_finalize(GObject *object);
static gboolean thunar_window_delete(GtkWidget *widget,
                                     GdkEvent *event,
                                     gpointer data);
static void thunar_window_get_property(GObject *object,
                                       guint prop_id,
                                       GValue *value,
                                       GParamSpec *pspec);
static void thunar_window_set_property(GObject *object,
                                       guint prop_id,
                                       const GValue *value,
                                       GParamSpec *pspec);
static gboolean thunar_window_reload(ThunarWindow *window,
                                     gboolean reload_info);
static gboolean thunar_window_tab_change(ThunarWindow *window,
                                         gint nth);
static void thunar_window_realize(GtkWidget *widget);
static void thunar_window_unrealize(GtkWidget *widget);
static void thunar_window_notebook_switch_page(GtkWidget *notebook,
                                               GtkWidget *page,
                                               guint page_num,
                                               ThunarWindow *window);
static void thunar_window_notebook_page_added(GtkWidget *notebook,
                                              GtkWidget *page,
                                              guint page_num,
                                              ThunarWindow *window);
static void thunar_window_notebook_page_removed(GtkWidget *notebook,
                                                GtkWidget *page,
                                                guint page_num,
                                                ThunarWindow *window);
static GtkWidget*thunar_window_notebook_insert(ThunarWindow *window,
                                               ThunarFile *directory,
                                               GType view_type,
                                               gint position,
                                               ThunarHistory *history);

static void thunar_window_update_location_bar_visible(ThunarWindow *window);
static void thunar_window_handle_reload_request(ThunarWindow *window);
static void thunar_window_install_sidepane(ThunarWindow *window,
                                           GType type);
static void thunar_window_start_open_location(ThunarWindow *window,
                                              const gchar *initial_text);

static void thunar_window_action_debug(ThunarWindow *window,
                                       GtkWidget *menu_item);
static void thunar_window_action_reload(ThunarWindow *window,
                                        GtkWidget *menu_item);
static void thunar_window_create_view(ThunarWindow *window,
                                      GtkWidget *view,
                                      GType view_type);
static void thunar_window_action_go_up(ThunarWindow *window);
static void thunar_window_action_back(ThunarWindow *window);
static void thunar_window_action_forward(ThunarWindow *window);
static void thunar_window_action_open_home(ThunarWindow *window);
static void thunar_window_action_show_hidden(ThunarWindow *window);
static gboolean thunar_window_propagate_key_event(GtkWindow *window,
                                                  GdkEvent *key_event,
                                                  gpointer user_data);
static void thunar_window_current_directory_changed(ThunarFile *current_directory,
                                                    ThunarWindow *window);
static void thunar_window_menu_item_selected(ThunarWindow *window,
                                             GtkWidget *menu_item);
static void thunar_window_menu_item_deselected(ThunarWindow *window,
                                               GtkWidget *menu_item);
static void thunar_window_notify_loading(ThunarView *view,
                                         GParamSpec *pspec,
                                         ThunarWindow *window);
static void thunar_window_device_pre_unmount(ThunarDeviceMonitor *device_monitor,
                                             ThunarDevice *device,
                                             GFile *root_file,
                                             ThunarWindow *window);
static void thunar_window_device_changed(ThunarDeviceMonitor *device_monitor,
                                         ThunarDevice *device,
                                         ThunarWindow *window);
static gboolean thunar_window_save_paned(ThunarWindow *window);
static void thunar_window_set_zoom_level(ThunarWindow *window,
                                         ThunarZoomLevel zoom_level);
static void thunar_window_update_window_icon(ThunarWindow *window);
static void thunar_window_select_files(ThunarWindow *window,
                                       GList *path_list);
static void thunar_window_binding_create(ThunarWindow *window,
                                         gpointer src_object,
                                         const gchar *src_prop,
                                         gpointer dst_object,
                                         const gchar *dst_prop,
                                         GBindingFlags flags);
static gboolean thunar_window_history_clicked(GtkWidget *button,
                                              GdkEventButton *event,
                                              GtkWidget *window);
static gboolean thunar_window_button_press_event(GtkWidget *view,
                                                 GdkEventButton *event,
                                                 ThunarWindow *window);
static void thunar_window_history_changed(ThunarWindow *window);
//static gboolean thunar_window_check_uca_key_activation(ThunarWindow *window,
//                                                       GdkEventKey *key_event,
//                                                       gpointer user_data);

struct _ThunarWindowClass
{
    GtkWindowClass __parent__;

    /* internal action signals */
    gboolean(*reload)       (ThunarWindow *window, gboolean reload_info);
    gboolean(*zoom_in)      (ThunarWindow *window);
    gboolean(*zoom_out)     (ThunarWindow *window);
    gboolean(*zoom_reset)   (ThunarWindow *window);
    gboolean(*tab_change)   (ThunarWindow *window, gint idx);
};

struct _ThunarWindow
{
    GtkWindow __parent__;

    /* support for custom preferences actions */
    //ThunarxProviderFactory *provider_factory;
    GList                  *thunarx_preferences_providers;

    ThunarClipboardManager *clipboard;

    ThunarIconFactory      *icon_factory;

    /* to be able to change folder on "device-pre-unmount" if required */
    ThunarDeviceMonitor    *device_monitor;

    GtkWidget              *grid;
    GtkWidget              *paned;
    GtkWidget              *sidepane;
    GtkWidget              *view_box;
    GtkWidget              *notebook;
    GtkWidget              *view;
    GtkWidget              *statusbar;

    GSList                 *view_bindings;

    /* we need to maintain pointers to be able to toggle sensitivity */
    GtkWidget              *toolbar;
    GtkWidget              *toolbar_item_back;
    GtkWidget              *toolbar_item_forward;
    GtkWidget              *toolbar_item_parent;
    GtkWidget              *location_bar;

    gulong                  signal_handler_id_history_changed;

    ThunarLauncher         *launcher;

    ThunarFile             *current_directory;
    GtkAccelGroup          *accel_group;

    /* zoom-level support */
    ThunarZoomLevel         zoom_level;
    gboolean                show_hidden;

    /* support to remember window geometry */
    guint                   save_geometry_timer_id;

    /* support to toggle side pane using F9,
     * see the toggle_sidepane() function.
     */
    GType                   toggle_sidepane_type;

    /* Takes care to select a file after e.g. rename/create */
    GClosure               *select_files_closure;
};

static XfceGtkActionEntry _window_actions[] =
{
    {THUNAR_WINDOW_ACTION_BACK,
     "<Actions>/ThunarStandardView/back",
     "<Alt>Left",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Back"),
     N_("Go to the previous visited folder"),
     "go-previous-symbolic",
     G_CALLBACK(thunar_window_action_back)},

    {THUNAR_WINDOW_ACTION_FORWARD,
     "<Actions>/ThunarStandardView/forward",
     "<Alt>Right",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Forward"),
     N_("Go to the next visited folder"),
     "go-next-symbolic",
     G_CALLBACK(thunar_window_action_forward)},

    {THUNAR_WINDOW_ACTION_OPEN_PARENT,
     "<Actions>/ThunarWindow/open-parent",
     "<Alt>Up",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Open _Parent"),
     N_("Open the parent folder"),
     "go-up-symbolic",
     G_CALLBACK(thunar_window_action_go_up)},

    {THUNAR_WINDOW_ACTION_OPEN_HOME,
     "<Actions>/ThunarWindow/open-home",
     "<Alt>Home",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Home"),
     N_("Go to the home folder"),
     "go-home-symbolic",
     G_CALLBACK(thunar_window_action_open_home)},

    {THUNAR_WINDOW_ACTION_BACK_ALT,
     "<Actions>/ThunarStandardView/back-alt",
     "BackSpace",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_window_action_back)},

    {THUNAR_WINDOW_ACTION_RELOAD_ALT,
     "<Actions>/ThunarWindow/reload-alt",
     "F5",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_window_action_reload)},

    {THUNAR_WINDOW_ACTION_SHOW_HIDDEN,
     "<Actions>/ThunarWindow/show-hidden",
     "<Primary>h",
     XFCE_GTK_CHECK_MENU_ITEM,
     N_("Show _Hidden Files"),
     N_("Toggles the display of hidden files in the current window"),
     NULL,
     G_CALLBACK(thunar_window_action_show_hidden)},

    {THUNAR_WINDOW_ACTION_DEBUG,
     "<Actions>/ThunarWindow/debug",
     "<Primary>d",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_window_action_debug)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_window_actions, \
    G_N_ELEMENTS(_window_actions), \
    id)

static guint window_signals[LAST_SIGNAL];

G_DEFINE_TYPE_WITH_CODE(ThunarWindow, thunar_window, GTK_TYPE_WINDOW,
                         G_IMPLEMENT_INTERFACE(THUNAR_TYPE_BROWSER, NULL))

static void thunar_window_class_init(ThunarWindowClass *klass)
{
    GtkWidgetClass *gtkwidget_class;
    GtkBindingSet  *binding_set;
    GObjectClass   *gobject_class;
    guint           i;

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = thunar_window_dispose;
    gobject_class->finalize = thunar_window_finalize;
    gobject_class->get_property = thunar_window_get_property;
    gobject_class->set_property = thunar_window_set_property;

    gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->realize = thunar_window_realize;
    gtkwidget_class->unrealize = thunar_window_unrealize;

    klass->reload = thunar_window_reload;
    klass->zoom_in = NULL;
    klass->zoom_out = NULL;
    klass->zoom_reset = NULL;

    klass->tab_change = thunar_window_tab_change;

    xfce_gtk_translate_action_entries(_window_actions, G_N_ELEMENTS(_window_actions));

    /**
     * ThunarWindow:current-directory:
     *
     * The directory currently displayed within this #ThunarWindow
     * or %NULL.
     **/
    g_object_class_install_property(gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     g_param_spec_object("current-directory",
                                             "current-directory",
                                             "current-directory",
                                             THUNAR_TYPE_FILE,
                                             E_PARAM_READWRITE));

    /**
     * ThunarWindow:zoom-level:
     *
     * The #ThunarZoomLevel applied to the #ThunarView currently
     * shown within this window.
     **/
    g_object_class_install_property(gobject_class,
                                     PROP_ZOOM_LEVEL,
                                     g_param_spec_enum("zoom-level",
                                             "zoom-level",
                                             "zoom-level",
                                             THUNAR_TYPE_ZOOM_LEVEL,
                                             THUNAR_ZOOM_LEVEL_100_PERCENT,
                                             E_PARAM_READWRITE));

    /**
     * ThunarWindow::reload:
     * @window : a #ThunarWindow instance.
     *
     * Emitted whenever the user requests to reload the contents
     * of the currently displayed folder.
     **/
    window_signals[RELOAD] =
        g_signal_new(I_("reload"),
                      G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET(ThunarWindowClass, reload),
                      g_signal_accumulator_true_handled, NULL,
                      _thunar_marshal_BOOLEAN__BOOLEAN,
                      G_TYPE_BOOLEAN, 1,
                      G_TYPE_BOOLEAN);

    /**
     * ThunarWindow::zoom-in:
     * @window : a #ThunarWindow instance.
     *
     * Emitted whenever the user requests to zoom in. This
     * is an internal signal used to bind the action to keys.
     **/
    window_signals[ZOOM_IN] =
        g_signal_new(I_("zoom-in"),
                      G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET(ThunarWindowClass, zoom_in),
                      g_signal_accumulator_true_handled, NULL,
                      _thunar_marshal_BOOLEAN__VOID,
                      G_TYPE_BOOLEAN, 0);

    /**
     * ThunarWindow::zoom-out:
     * @window : a #ThunarWindow instance.
     *
     * Emitted whenever the user requests to zoom out. This
     * is an internal signal used to bind the action to keys.
     **/
    window_signals[ZOOM_OUT] =
        g_signal_new(I_("zoom-out"),
                      G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET(ThunarWindowClass, zoom_out),
                      g_signal_accumulator_true_handled, NULL,
                      _thunar_marshal_BOOLEAN__VOID,
                      G_TYPE_BOOLEAN, 0);

    /**
     * ThunarWindow::zoom-reset:
     * @window : a #ThunarWindow instance.
     *
     * Emitted whenever the user requests reset the zoom level.
     * This is an internal signal used to bind the action to keys.
     **/
    window_signals[ZOOM_RESET] =
        g_signal_new(I_("zoom-reset"),
                      G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET(ThunarWindowClass, zoom_reset),
                      g_signal_accumulator_true_handled, NULL,
                      _thunar_marshal_BOOLEAN__VOID,
                      G_TYPE_BOOLEAN, 0);

    /**
     * ThunarWindow::tab-change:
     * @window : a #ThunarWindow instance.
     * @idx    : tab index,
     *
     * Emitted whenever the user uses a Alt+N combination to
     * switch tabs.
     **/
    window_signals[TAB_CHANGE] =
        g_signal_new(I_("tab-change"),
                      G_TYPE_FROM_CLASS(klass),
                      G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                      G_STRUCT_OFFSET(ThunarWindowClass, tab_change),
                      g_signal_accumulator_true_handled, NULL,
                      _thunar_marshal_BOOLEAN__INT,
                      G_TYPE_BOOLEAN, 1,
                      G_TYPE_INT);

    /* setup the key bindings for the windows */
    binding_set = gtk_binding_set_by_class(klass);

    /* setup the key bindings for Alt+N */
    for(i = 0; i < 10; i++)
    {
        gtk_binding_entry_add_signal(binding_set, GDK_KEY_0 + i, GDK_MOD1_MASK,
                                      "tab-change", 1, G_TYPE_UINT, i - 1);
    }
}

static void thunar_window_init(ThunarWindow *window)
{
    GType           type;
    GtkToolItem     *tool_item;
    GtkStyleContext *context;

    Preferences *prefs = get_preferences();

    /* grab a reference on the provider factory and load the providers*/
//    window->provider_factory = thunarx_provider_factory_get_default();
//    window->thunarx_preferences_providers = thunarx_provider_factory_list_providers(window->provider_factory, THUNARX_TYPE_PREFERENCES_PROVIDER);

    window->accel_group = gtk_accel_group_new();
    xfce_gtk_accel_map_add_entries(_window_actions, G_N_ELEMENTS(_window_actions));
    xfce_gtk_accel_group_connect_action_entries(window->accel_group,
            _window_actions,
            G_N_ELEMENTS(_window_actions),
            window);

    gtk_window_add_accel_group(GTK_WINDOW(window), window->accel_group);

    window->show_hidden = false;

    /* update the visual on screen_changed events */
    g_signal_connect(window, "screen-changed", G_CALLBACK(thunar_window_screen_changed), NULL);

    /* invoke the thunar_window_screen_changed function to initially set the best possible visual.*/
    thunar_window_screen_changed(GTK_WIDGET(window), NULL, NULL);

    /* set up a handler to confirm exit when there are multiple tabs open  */
    g_signal_connect(window, "delete-event", G_CALLBACK(thunar_window_delete), NULL);

    /* connect to the volume monitor */
    window->device_monitor = thunar_device_monitor_get();
    g_signal_connect(window->device_monitor, "device-pre-unmount", G_CALLBACK(thunar_window_device_pre_unmount), window);
    g_signal_connect(window->device_monitor, "device-removed", G_CALLBACK(thunar_window_device_changed), window);
    g_signal_connect(window->device_monitor, "device-changed", G_CALLBACK(thunar_window_device_changed), window);

    window->icon_factory = thunar_icon_factory_get_default();

    /* Catch key events before accelerators get processed */
    g_signal_connect(window, "key-press-event", G_CALLBACK(thunar_window_propagate_key_event), NULL);
    g_signal_connect(window, "key-release-event", G_CALLBACK(thunar_window_propagate_key_event), NULL);

    window->select_files_closure = g_cclosure_new_swap(G_CALLBACK(thunar_window_select_files), window, NULL);
    g_closure_ref(window->select_files_closure);
    g_closure_sink(window->select_files_closure);
    window->launcher = g_object_new(THUNAR_TYPE_LAUNCHER,
                                     "widget",
                                     GTK_WIDGET(window),
                                     "select-files-closure",
                                     window->select_files_closure,
                                     NULL);

    g_object_bind_property(G_OBJECT(window), "current-directory", G_OBJECT(window->launcher), "current-directory", G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(window->launcher), "change-directory", G_CALLBACK(thunar_window_set_current_directory), window);
    launcher_append_accelerators(window->launcher, window->accel_group);

    gtk_window_set_default_size(GTK_WINDOW(window),
                                prefs->window_width,
                                prefs->window_height);

    if (G_UNLIKELY(prefs->window_maximized))
        gtk_window_maximize(GTK_WINDOW(window));

    /* add thunar style class for easier theming */
    context = gtk_widget_get_style_context(GTK_WIDGET(window));
    gtk_style_context_add_class(context, "thunar");


    // Create widgets
    window->grid = gtk_grid_new();
    gtk_container_add(GTK_CONTAINER(window), window->grid);
    gtk_widget_show(window->grid);


    // Toolbar
    window->toolbar = gtk_toolbar_new();
    gtk_toolbar_set_style(GTK_TOOLBAR(window->toolbar), GTK_TOOLBAR_ICONS);
    gtk_toolbar_set_icon_size(
        GTK_TOOLBAR(window->toolbar), GTK_ICON_SIZE_SMALL_TOOLBAR);
    gtk_widget_set_hexpand(window->toolbar, TRUE);
    gtk_grid_attach(GTK_GRID(window->grid), window->toolbar, 0, 0, 1, 1);

    window->toolbar_item_back = xfce_gtk_tool_button_new_from_action_entry(get_action_entry(THUNAR_WINDOW_ACTION_BACK), G_OBJECT(window), GTK_TOOLBAR(window->toolbar));
    window->toolbar_item_forward = xfce_gtk_tool_button_new_from_action_entry(get_action_entry(THUNAR_WINDOW_ACTION_FORWARD), G_OBJECT(window), GTK_TOOLBAR(window->toolbar));
    window->toolbar_item_parent = xfce_gtk_tool_button_new_from_action_entry(get_action_entry(THUNAR_WINDOW_ACTION_OPEN_PARENT), G_OBJECT(window), GTK_TOOLBAR(window->toolbar));
    xfce_gtk_tool_button_new_from_action_entry(get_action_entry(THUNAR_WINDOW_ACTION_OPEN_HOME), G_OBJECT(window), GTK_TOOLBAR(window->toolbar));

    g_signal_connect(G_OBJECT(window->toolbar_item_back), "button-press-event", G_CALLBACK(thunar_window_history_clicked), G_OBJECT(window));
    g_signal_connect(G_OBJECT(window->toolbar_item_forward), "button-press-event", G_CALLBACK(thunar_window_history_clicked), G_OBJECT(window));
    g_signal_connect(G_OBJECT(window), "button-press-event", G_CALLBACK(thunar_window_button_press_event), G_OBJECT(window));
    window->signal_handler_id_history_changed = 0;

    /* The UCA shortcuts need to be checked 'by hand',
     *  since we dont want to permanently keep menu items for them */
//    g_signal_connect(window, "key-press-event",
//                     G_CALLBACK(thunar_window_check_uca_key_activation), NULL);

    /* add the location bar to the toolbar */
    tool_item = gtk_tool_item_new();
    gtk_tool_item_set_expand(tool_item, TRUE);
    gtk_toolbar_insert(GTK_TOOLBAR(window->toolbar), tool_item, -1);
    gtk_toolbar_set_show_arrow(GTK_TOOLBAR(window->toolbar), FALSE);

    /* allocate the new location bar widget */
    window->location_bar = thunar_location_bar_new();
    g_object_bind_property(G_OBJECT(window), "current-directory", G_OBJECT(window->location_bar), "current-directory", G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(window->location_bar), "change-directory", G_CALLBACK(thunar_window_set_current_directory), window);
    g_signal_connect_swapped(G_OBJECT(window->location_bar), "reload-requested", G_CALLBACK(thunar_window_handle_reload_request), window);
    g_signal_connect_swapped(G_OBJECT(window->location_bar), "entry-done", G_CALLBACK(thunar_window_update_location_bar_visible), window);

    /* add the location bar itself */
    gtk_container_add(GTK_CONTAINER(tool_item), window->location_bar);

    /* display the toolbar */
    gtk_widget_show_all(window->toolbar);

    thunar_window_update_location_bar_visible(window);

    // Paned
    window->paned = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_container_set_border_width(GTK_CONTAINER(window->paned), 0);
    gtk_widget_set_hexpand(window->paned, TRUE);
    gtk_widget_set_vexpand(window->paned, TRUE);
    gtk_grid_attach(GTK_GRID(window->grid), window->paned, 0, 1, 1, 1);
    gtk_widget_show(window->paned);

    /* determine the last separator position and apply it to the paned view */
    gtk_paned_set_position(GTK_PANED(window->paned), prefs->separator_position);
    g_signal_connect_swapped(window->paned, "accept-position", G_CALLBACK(thunar_window_save_paned), window);
    g_signal_connect_swapped(window->paned, "button-release-event", G_CALLBACK(thunar_window_save_paned), window);


    // Left pane : Treeview
    type = THUNAR_TYPE_TREE_PANE;
    thunar_window_install_sidepane(window, type);


    // Right pane : GtkGrid
    window->view_box = gtk_grid_new();
    gtk_paned_pack2(GTK_PANED(window->paned), window->view_box, TRUE, FALSE);
    gtk_widget_show(window->view_box);

    /* tabs */
    window->notebook = gtk_notebook_new();
    gtk_widget_set_hexpand(window->notebook, TRUE);
    gtk_widget_set_vexpand(window->notebook, TRUE);
    gtk_grid_attach(GTK_GRID(window->view_box), window->notebook, 0, 1, 1, 1);
    g_signal_connect(G_OBJECT(window->notebook), "switch-page", G_CALLBACK(thunar_window_notebook_switch_page), window);
    g_signal_connect(G_OBJECT(window->notebook), "page-added", G_CALLBACK(thunar_window_notebook_page_added), window);
    g_signal_connect(G_OBJECT(window->notebook), "page-removed", G_CALLBACK(thunar_window_notebook_page_removed), window);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(window->notebook), FALSE);
    gtk_notebook_set_scrollable(GTK_NOTEBOOK(window->notebook), TRUE);
    gtk_container_set_border_width(GTK_CONTAINER(window->notebook), 0);
    gtk_notebook_set_group_name(GTK_NOTEBOOK(window->notebook), "thunar-tabs");
    gtk_widget_show(window->notebook);

    gtk_widget_set_can_focus(window->notebook, FALSE);

    /* setup a new statusbar */
    window->statusbar = thunar_statusbar_new();
    gtk_widget_set_hexpand(window->statusbar, TRUE);
    gtk_grid_attach(GTK_GRID(window->view_box), window->statusbar, 0, 2, 1, 1);

    gtk_widget_show(window->statusbar);

    if (G_LIKELY(window->view != NULL))
        thunar_window_binding_create(window, window->view, "statusbar-text", window->statusbar, "text", G_BINDING_SYNC_CREATE);

    /* ensure that all the view types are registered */
    g_type_ensure(THUNAR_TYPE_DETAILS_VIEW);
}

static void thunar_window_screen_changed(GtkWidget *widget,
                                         GdkScreen *old_screen,
                                         gpointer   userdata)
{
    UNUSED(old_screen);
    UNUSED(userdata);
    GdkScreen *screen = gdk_screen_get_default();
    GdkVisual *visual = gdk_screen_get_rgba_visual(screen);

    if (visual == NULL || !gdk_screen_is_composited(screen))
        visual = gdk_screen_get_system_visual(screen);

    gtk_widget_set_visual(GTK_WIDGET(widget), visual);
}

/**
 * thunar_window_select_files:
 * @window            : a #ThunarWindow instance.
 * @files_to_selected : a list of #GFile<!---->s
 *
 * Visually selects the files, given by the list
 **/
static void thunar_window_select_files(ThunarWindow *window,
                                       GList        *files_to_selected)
{
    GList        *thunar_files = NULL;
    ThunarFolder *thunar_folder;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* If possible, reload the current directory to make sure new files got added to the view */
    thunar_folder = thunar_folder_get_for_file(window->current_directory);
    if (thunar_folder != NULL)
    {
        thunar_folder_reload(thunar_folder, FALSE);
        g_object_unref(thunar_folder);
    }

    for(GList *lp = files_to_selected; lp != NULL; lp = lp->next)
        thunar_files = g_list_append(thunar_files, th_file_get(G_FILE(lp->data), NULL));
    thunar_view_set_selected_files(THUNAR_VIEW(window->view), thunar_files);
    g_list_free_full(thunar_files, g_object_unref);
}

static void thunar_window_dispose(GObject *object)
{
    ThunarWindow  *window = THUNAR_WINDOW(object);

    /* indicate that history items are out of use */
    window->toolbar_item_back = NULL;
    window->toolbar_item_forward = NULL;

    if (window->accel_group != NULL)
    {
        gtk_accel_group_disconnect(window->accel_group, NULL);
        gtk_window_remove_accel_group(GTK_WINDOW(window), window->accel_group);
        g_object_unref(window->accel_group);
        window->accel_group = NULL;
    }

    /* destroy the save geometry timer source */
    if (G_UNLIKELY(window->save_geometry_timer_id != 0))
        g_source_remove(window->save_geometry_timer_id);

    /* disconnect from the current-directory */
    thunar_window_set_current_directory(window, NULL);

   (*G_OBJECT_CLASS(thunar_window_parent_class)->dispose)(object);
}

static void thunar_window_finalize(GObject *object)
{
    ThunarWindow *window = THUNAR_WINDOW(object);

    /* disconnect from the volume monitor */
    g_signal_handlers_disconnect_matched(window->device_monitor, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);
    g_object_unref(window->device_monitor);

    g_object_unref(window->icon_factory);
    g_object_unref(window->launcher);

    /* release our reference on the provider factory */
    //g_object_unref(window->provider_factory);

    g_closure_invalidate(window->select_files_closure);
    g_closure_unref(window->select_files_closure);

   (*G_OBJECT_CLASS(thunar_window_parent_class)->finalize)(object);
}

static gboolean thunar_window_delete(GtkWidget *widget,
                                     GdkEvent  *event,
                                     gpointer   data )
{
    UNUSED(widget);
    UNUSED(event);
    UNUSED(data);

    Preferences *prefs = get_preferences();
    GtkWindow *window = GTK_WINDOW(widget);

    if (gtk_widget_get_visible(GTK_WIDGET(widget)))
    {
        GdkWindowState state = gdk_window_get_state(
                                            gtk_widget_get_window(widget));

        prefs->window_maximized = ((state & (GDK_WINDOW_STATE_MAXIMIZED
                                             | GDK_WINDOW_STATE_FULLSCREEN))
                                    != 0);

        if (!prefs->window_maximized)
        {
            gtk_window_get_size(window,
                                &prefs->window_width,
                                &prefs->window_height);
        }
    }

    return false;
}

static void thunar_window_get_property(GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
    UNUSED(pspec);

    ThunarWindow *window = THUNAR_WINDOW(object);

    switch(prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, thunar_window_get_current_directory(window));
        break;

    case PROP_ZOOM_LEVEL:
        g_value_set_enum(value, window->zoom_level);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void thunar_window_set_property(GObject            *object,
                                       guint               prop_id,
                                       const GValue       *value,
                                       GParamSpec         *pspec)
{
    UNUSED(pspec);
    ThunarWindow *window = THUNAR_WINDOW(object);

    switch(prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        thunar_window_set_current_directory(window, g_value_get_object(value));
        break;

    case PROP_ZOOM_LEVEL:
        thunar_window_set_zoom_level(window, g_value_get_enum(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean thunar_window_reload(ThunarWindow *window,
                                     gboolean      reload_info)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);

    /* force the view to reload */
    if (G_LIKELY(window->view != NULL))
    {
        thunar_view_reload(THUNAR_VIEW(window->view), reload_info);
        return TRUE;
    }

    return FALSE;
}

/**
 * thunar_window_has_shortcut_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value: True, if this window is running a shortcut sidepane
 **/
gboolean thunar_window_has_shortcut_sidepane(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);

    return FALSE;
}

/**
 * thunar_window_get_sidepane:
 * @window : a #ThunarWindow instance.
 *
 * Return value:(transfer none): The #ThunarSidePane of this window, or NULL if not available
 **/
GtkWidget* thunar_window_get_sidepane(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);
    return GTK_WIDGET(window->sidepane);
}

static gboolean thunar_window_tab_change(ThunarWindow *window,
                                         gint          nth)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);

    /* Alt+0 is 10th tab */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook),
                                   nth == -1 ? 9 : nth);

    return TRUE;
}

static void thunar_window_realize(GtkWidget *widget)
{
    ThunarWindow *window = THUNAR_WINDOW(widget);

    /* let the GtkWidget class perform the realize operation */
   (*GTK_WIDGET_CLASS(thunar_window_parent_class)->realize)(widget);

    /* connect to the clipboard manager of the new display and be sure to redraw the window
     * whenever the clipboard contents change to make sure we always display up2date state.
     */
    window->clipboard = clipman_get_for_display(gtk_widget_get_display(widget));
    g_signal_connect_swapped(G_OBJECT(window->clipboard), "changed",
                              G_CALLBACK(gtk_widget_queue_draw), widget);
}

static void thunar_window_unrealize(GtkWidget *widget)
{
    ThunarWindow *window = THUNAR_WINDOW(widget);

    /* disconnect from the clipboard manager */
    g_signal_handlers_disconnect_by_func(G_OBJECT(window->clipboard), gtk_widget_queue_draw, widget);

    /* let the GtkWidget class unrealize the window */
   (*GTK_WIDGET_CLASS(thunar_window_parent_class)->unrealize)(widget);

    /* drop the reference on the clipboard manager, we do this after letting the GtkWidget class
     * unrealise the window to prevent the clipboard being disposed during the unrealize  */
    g_object_unref(G_OBJECT(window->clipboard));
}

static void thunar_window_binding_destroyed(gpointer data,
                                            GObject  *binding)
{
    ThunarWindow *window = THUNAR_WINDOW(data);

    if (window->view_bindings != NULL)
        window->view_bindings = g_slist_remove(window->view_bindings, binding);
}

static void thunar_window_binding_create(ThunarWindow *window,
                                         gpointer src_object,
                                         const gchar *src_prop,
                                         gpointer dst_object,
                                         const gchar *dst_prop,
                                         GBindingFlags flags)
{
    GBinding *binding;

    thunar_return_if_fail(G_IS_OBJECT(src_object));
    thunar_return_if_fail(G_IS_OBJECT(dst_object));

    binding = g_object_bind_property(G_OBJECT(src_object), src_prop,
                                      G_OBJECT(dst_object), dst_prop,
                                      flags);

    g_object_weak_ref(G_OBJECT(binding), thunar_window_binding_destroyed, window);
    window->view_bindings = g_slist_prepend(window->view_bindings, binding);
}


/******************************************************************************
 *
 * Notebook functions
 *
 **/
static void thunar_window_notebook_switch_page(GtkWidget   *notebook,
                                               GtkWidget   *page,
                                               guint       page_num,
                                               ThunarWindow *window)
{
    thunar_return_if_fail(window->notebook == notebook);
    thunar_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    thunar_return_if_fail(THUNAR_IS_VIEW(page));
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    UNUSED(page_num);

    GSList        *view_bindings;
    ThunarFile    *current_directory;
    ThunarHistory *history;

    /* leave if nothing changed */
    if (window->view == page)
        return;

//  DPRINT("enter : thunar_window_notebook_switch_page\n");

    /* Use accelerators only on the current active tab */
    if (window->view != NULL)
        g_object_set(G_OBJECT(window->view), "accel-group", NULL, NULL);

    g_object_set(G_OBJECT(page), "accel-group", window->accel_group, NULL);

    if (G_LIKELY(window->view != NULL))
    {
        /* disconnect from previous history */
        if (window->signal_handler_id_history_changed != 0)
        {
            history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));
            g_signal_handler_disconnect(history, window->signal_handler_id_history_changed);
            window->signal_handler_id_history_changed = 0;
        }

        /* unset view during switch */
        window->view = NULL;
    }

    /* disconnect existing bindings */
    view_bindings = window->view_bindings;
    window->view_bindings = NULL;
    g_slist_free_full(view_bindings, g_object_unref);

    /* update the directory of the current window */
    current_directory = thunar_navigator_get_current_directory(THUNAR_NAVIGATOR(page));
    thunar_window_set_current_directory(window, current_directory);

    /* add stock bindings */
    thunar_window_binding_create(window,
                                  window, "current-directory",
                                  page, "current-directory",
                                  G_BINDING_DEFAULT);
    thunar_window_binding_create(window,
                                  page, "selected-files",
                                  window->launcher, "selected-files",
                                  G_BINDING_SYNC_CREATE);
    thunar_window_binding_create(window,
                                  page, "zoom-level",
                                  window, "zoom-level",
                                  G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    /* connect to the sidepane(if any) */
    if (G_LIKELY(window->sidepane != NULL))
    {
        thunar_window_binding_create(window,
                                      page, "selected-files",
                                      window->sidepane, "selected-files",
                                      G_BINDING_SYNC_CREATE);
    }

    /* connect to the statusbar(if any) */
    if (G_LIKELY(window->statusbar != NULL))
    {
        thunar_window_binding_create(window, page, "statusbar-text",
                                      window->statusbar, "text",
                                      G_BINDING_SYNC_CREATE);
    }

    /* activate new view */
    window->view = page;

    /* connect to the new history */
    history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));
    if (history != NULL)
    {
        window->signal_handler_id_history_changed = g_signal_connect_swapped(G_OBJECT(history), "history-changed", G_CALLBACK(thunar_window_history_changed), window);
        thunar_window_history_changed(window);
    }

    /* update the selection */
    standard_view_selection_changed(THUNAR_STANDARD_VIEW(page));

    gtk_widget_grab_focus(page);
}

static void thunar_window_history_changed(ThunarWindow *window)
{
    ThunarHistory *history;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    if (window->view == NULL)
        return;

    history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));
    if (history == NULL)
        return;

    if (window->toolbar_item_back != NULL)
        gtk_widget_set_sensitive(window->toolbar_item_back, thunar_history_has_back(history));

    if (window->toolbar_item_forward != NULL)
        gtk_widget_set_sensitive(window->toolbar_item_forward, thunar_history_has_forward(history));
}

static void thunar_window_notebook_page_added(GtkWidget    *notebook,
                                              GtkWidget    *page,
                                              guint         page_num,
                                              ThunarWindow *window)
{
    UNUSED(page_num);
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    thunar_return_if_fail(THUNAR_IS_VIEW(page));
    thunar_return_if_fail(window->notebook == notebook);

//  DPRINT("enter : thunar_window_notebook_page_added\n");

    /* connect signals */
    g_signal_connect(G_OBJECT(page), "notify::loading", G_CALLBACK(thunar_window_notify_loading), window);
    g_signal_connect_swapped(G_OBJECT(page), "start-open-location", G_CALLBACK(thunar_window_start_open_location), window);
    g_signal_connect_swapped(G_OBJECT(page), "change-directory", G_CALLBACK(thunar_window_set_current_directory), window);

    /* update tab visibility */
    //thunar_window_notebook_show_tabs(window);
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), FALSE);
}

static void thunar_window_notebook_page_removed(GtkWidget    *notebook,
                                                GtkWidget    *page,
                                                guint         page_num,
                                                ThunarWindow *window)
{
    UNUSED(page_num);
    gint n_pages;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    thunar_return_if_fail(THUNAR_IS_VIEW(page));
    thunar_return_if_fail(window->notebook == notebook);

//  DPRINT("enter : thunar_window_notebook_page_removed\n");

    /* drop connected signals */
    g_signal_handlers_disconnect_matched(page, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, window);

    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    if (n_pages == 0)
    {
        /* destroy the window */
        gtk_widget_destroy(GTK_WIDGET(window));
    }
}

static GtkWidget* thunar_window_notebook_insert(ThunarWindow  *window,
                                                ThunarFile    *directory,
                                                GType          view_type,
                                                gint           position,
                                                ThunarHistory *history)
{
//  DPRINT("enter : thunar_window_notebook_insert\n");

    GtkWidget      *view;
    GtkWidget      *label;
    GtkWidget      *label_box;
    GtkWidget      *button;
    GtkWidget      *icon;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), NULL);
    thunar_return_val_if_fail(THUNAR_IS_FILE(directory), NULL);
    thunar_return_val_if_fail(view_type != G_TYPE_NONE, NULL);
    thunar_return_val_if_fail(history == NULL || THUNAR_IS_HISTORY(history), NULL);

    /* allocate and setup a new view */
    view = g_object_new(view_type, "current-directory", directory, NULL);
    thunar_view_set_show_hidden(THUNAR_VIEW(view), window->show_hidden);
    gtk_widget_show(view);

    /* set the history of the view if a history is provided */
    if (history != NULL)
        standard_view_set_history(THUNAR_STANDARD_VIEW(view), history);

    label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);

    label = gtk_label_new(NULL);
    g_object_bind_property(G_OBJECT(view), "display-name", G_OBJECT(label), "label", G_BINDING_SYNC_CREATE);
    g_object_bind_property(G_OBJECT(view), "tooltip-text", G_OBJECT(label), "tooltip-text", G_BINDING_SYNC_CREATE);
    gtk_widget_set_has_tooltip(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_end(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_top(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_bottom(GTK_WIDGET(label), 3);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(label_box), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(label_box), button, FALSE, FALSE, 0);
    gtk_widget_set_can_default(button, FALSE);
    gtk_widget_set_focus_on_click(button, FALSE);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(button, _("Close tab"));
    g_signal_connect_swapped(G_OBJECT(button), "clicked", G_CALLBACK(gtk_widget_destroy), view);
    gtk_widget_show(button);

    icon = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(button), icon);
    gtk_widget_show(icon);

    /* insert the new page */
    gtk_notebook_insert_page(GTK_NOTEBOOK(window->notebook), view, label_box, position);

    /* set tab child properties */
    gtk_container_child_set(GTK_CONTAINER(window->notebook), view, "tab-expand", TRUE, NULL);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(window->notebook), view, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(window->notebook), view, TRUE);

    return view;
}

void thunar_window_update_directories(ThunarWindow *window,
                                      ThunarFile   *old_directory,
                                      ThunarFile   *new_directory)
{
    GtkWidget  *view;
    ThunarFile *directory;
    gint        n;
    gint        n_pages;
    gint        active_page;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(THUNAR_IS_FILE(old_directory));
    thunar_return_if_fail(THUNAR_IS_FILE(new_directory));

    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    if (G_UNLIKELY(n_pages == 0))
        return;

    active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));

    for(n = 0; n < n_pages; n++)
    {
        /* get the view */
        view = gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), n);
        if (! THUNAR_IS_NAVIGATOR(view))
            continue;

        /* get the directory of the view */
        directory = thunar_navigator_get_current_directory(THUNAR_NAVIGATOR(view));
        if (! THUNAR_IS_FILE(directory))
            continue;

        /* if it matches the old directory, change to the new one */
        if (directory == old_directory)
        {
            if (n == active_page)
                thunar_navigator_change_directory(THUNAR_NAVIGATOR(view), new_directory);
            else
                thunar_navigator_set_current_directory(THUNAR_NAVIGATOR(view), new_directory);
        }
    }
}

static void thunar_window_update_location_bar_visible(ThunarWindow *window)
{
    gtk_widget_show(window->toolbar);
}

static void thunar_window_update_window_icon(ThunarWindow *window)
{
    const gchar *icon_name = "folder";

    gboolean change_window_icon = true;

    if (change_window_icon)
    {
        GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(gtk_window_get_screen(GTK_WINDOW(window)));
        icon_name = th_file_get_icon_name(window->current_directory,
                                               THUNAR_FILE_ICON_STATE_DEFAULT,
                                               icon_theme);
    }

    gtk_window_set_icon_name(GTK_WINDOW(window), icon_name);
}

static void thunar_window_handle_reload_request(ThunarWindow *window)
{
    gboolean result;

    /* force the view to reload */
    g_signal_emit(G_OBJECT(window), window_signals[RELOAD], 0, TRUE, &result);
}

static void thunar_window_install_sidepane(ThunarWindow *window,
                                           GType         type)
{
    GtkStyleContext *context;

    thunar_return_if_fail(type == G_TYPE_NONE || g_type_is_a(type, THUNAR_TYPE_SIDE_PANE));
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* drop the previous side pane(if any) */
    if (G_UNLIKELY(window->sidepane != NULL))
    {
        gtk_widget_destroy(window->sidepane);
        window->sidepane = NULL;
    }

    /* check if we have a new sidepane widget */
    if (G_LIKELY(type != G_TYPE_NONE))
    {
        /* allocate the new side pane widget */
        window->sidepane = g_object_new(type, NULL);
        gtk_widget_set_size_request(window->sidepane, 0, -1);
        g_object_bind_property(G_OBJECT(window), "current-directory", G_OBJECT(window->sidepane), "current-directory", G_BINDING_SYNC_CREATE);
        g_signal_connect_swapped(G_OBJECT(window->sidepane), "change-directory", G_CALLBACK(thunar_window_set_current_directory), window);
        context = gtk_widget_get_style_context(window->sidepane);
        gtk_style_context_add_class(context, "sidebar");
        gtk_paned_pack1(GTK_PANED(window->paned), window->sidepane, FALSE, FALSE);
        gtk_widget_show(window->sidepane);

        /* connect the side pane widget to the view(if any) */
        if (G_LIKELY(window->view != NULL))
            thunar_window_binding_create(window, window->view, "selected-files", window->sidepane, "selected-files", G_BINDING_SYNC_CREATE);

        /* apply show_hidden config to tree pane */
        if (type == THUNAR_TYPE_TREE_PANE)
            thunar_side_pane_set_show_hidden(THUNAR_SIDE_PANE(window->sidepane), window->show_hidden);
    }
}

static void thunar_window_start_open_location(ThunarWindow *window,
                                              const gchar  *initial_text)
{
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* temporary show the location toolbar, even if it is normally hidden */
    gtk_widget_show(window->toolbar);
    thunar_location_bar_request_entry(THUNAR_LOCATION_BAR(window->location_bar), initial_text);
}

static void thunar_window_action_debug(ThunarWindow *window,
                                       GtkWidget *menu_item)
{
    UNUSED(window);
    UNUSED(menu_item);

    GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(window));
    const gchar *name = gtk_widget_get_name(focused);

    DPRINT("focused widget = %s\n", name);

    openlog("Fileman", LOG_PID, LOG_USER);
    syslog(LOG_INFO,"focused widget = %s\n", name);
    closelog();
}

static void thunar_window_action_reload(ThunarWindow *window,
                                        GtkWidget    *menu_item)
{
    UNUSED(menu_item);
    gboolean result;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* force the view to reload */
    g_signal_emit(G_OBJECT(window), window_signals[RELOAD], 0, TRUE, &result);

    /* update the location bar to show the current directory */
    if (window->location_bar != NULL)
        g_object_notify(G_OBJECT(window->location_bar), "current-directory");
}

static void thunar_window_create_view(ThunarWindow *window,
                                      GtkWidget    *view,
                                      GType         view_type)
{
    ThunarFile     *file = NULL;
    ThunarFile     *current_directory = NULL;
    GtkWidget      *new_view;
    ThunarHistory  *history = NULL;
    GList          *selected_files = NULL;
    gint            page_num;

    thunar_return_if_fail(view_type != G_TYPE_NONE);

    thunar_assert(view == NULL);

    if (view != NULL)
        return;

    //DPRINT("enter : thunar_window_create_view\n");

    /* if we have not got a current directory from the old view, use the window's current directory */
    if (current_directory == NULL && window->current_directory != NULL)
        current_directory = g_object_ref(window->current_directory);

    thunar_assert(current_directory != NULL);

    page_num = -1;

    /* insert the new view */
    new_view = thunar_window_notebook_insert(window, current_directory, view_type, page_num + 1, history);

    /* switch to the new view */
    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(window->notebook), new_view);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), page_num);

    /* take focus on the new view */
    gtk_widget_grab_focus(new_view);

    /* scroll to the previously visible file in the old view */
    if (G_UNLIKELY(file != NULL))
        thunar_view_scroll_to_file(THUNAR_VIEW(new_view), file, FALSE, TRUE, 0.0f, 0.0f);

    /* restore the file selection */
    thunar_component_set_selected_files(THUNAR_COMPONENT(new_view), selected_files);
    thunar_g_file_list_free(selected_files);

    /* release the file references */
    if (G_UNLIKELY(file != NULL))
        g_object_unref(G_OBJECT(file));

    if (G_UNLIKELY(current_directory != NULL))
        g_object_unref(G_OBJECT(current_directory));

    /* connect to the new history if this is the active view */
    history = standard_view_get_history(THUNAR_STANDARD_VIEW(new_view));
    window->signal_handler_id_history_changed = g_signal_connect_swapped(G_OBJECT(history),
            "history-changed",
            G_CALLBACK(thunar_window_history_changed),
            window);
}

static void thunar_window_action_go_up(ThunarWindow *window)
{
    ThunarFile *parent;
    GError     *error = NULL;

    parent = th_file_get_parent(window->current_directory, &error);
    if (G_LIKELY(parent != NULL))
    {
        thunar_window_set_current_directory(window, parent);
        g_object_unref(G_OBJECT(parent));
    }
    else
    {
        /* the root folder '/' has no parent. In this special case we do not need a dialog */
        if (error->code != G_FILE_ERROR_NOENT)
        {
            dialog_error(GTK_WIDGET(window), error, _("Failed to open parent folder"));
        }
        g_error_free(error);
    }
}

static void thunar_window_action_back(ThunarWindow *window)
{
    ThunarHistory *history;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));
    thunar_history_action_back(history);
}

static void thunar_window_action_forward(ThunarWindow *window)
{
    ThunarHistory *history;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));
    thunar_history_action_forward(history);
}

static void thunar_window_action_open_home(ThunarWindow *window)
{
    GFile         *home;
    ThunarFile    *home_file;
    GError        *error = NULL;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* determine the path to the home directory */
    home = thunar_g_file_new_for_home();

    /* determine the file for the home directory */
    home_file = th_file_get(home, &error);
    if (G_UNLIKELY(home_file == NULL))
    {
        /* display an error to the user */
        dialog_error(GTK_WIDGET(window), error, _("Failed to open the home folder"));
        g_error_free(error);
    }
    else
    {
        /* open the home folder */
        thunar_window_set_current_directory(window, home_file);
        g_object_unref(G_OBJECT(home_file));
    }

    /* release our reference on the home path */
    g_object_unref(home);
}

//static gboolean thunar_window_check_uca_key_activation(ThunarWindow *window,
//                                                       GdkEventKey  *key_event,
//                                                       gpointer      user_data)
//{
//    UNUSED(user_data);

//    if (thunar_launcher_check_uca_key_activation(window->launcher, key_event))
//        return GDK_EVENT_STOP;

//    return GDK_EVENT_PROPAGATE;
//}

static gboolean thunar_window_propagate_key_event(GtkWindow* window,
                                                  GdkEvent  *key_event,
                                                  gpointer   user_data)
{
    UNUSED(user_data);

    GtkWidget* focused_widget;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), GDK_EVENT_PROPAGATE);

    focused_widget = gtk_window_get_focus(window);

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

static void thunar_window_action_show_hidden(ThunarWindow *window)
{
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    window->show_hidden = !window->show_hidden;
    gtk_container_foreach(GTK_CONTAINER(window->notebook),(GtkCallback)(void(*)(void)) thunar_view_set_show_hidden, GINT_TO_POINTER(window->show_hidden));

    if (G_LIKELY(window->sidepane != NULL))
        thunar_side_pane_set_show_hidden(THUNAR_SIDE_PANE(window->sidepane), window->show_hidden);
}

static void thunar_window_current_directory_changed(ThunarFile *current_directory,
                                                    ThunarWindow *window)
{
    gboolean      show_full_path = false;
    gchar        *parse_name = NULL;
    const gchar  *name;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(THUNAR_IS_FILE(current_directory));
    thunar_return_if_fail(window->current_directory == current_directory);

    if (G_UNLIKELY(show_full_path))
        name = parse_name = g_file_get_parse_name(th_file_get_file(current_directory));
    else
        name = th_file_get_display_name(current_directory);

    /* set window title */
    gtk_window_set_title(GTK_WINDOW(window), name);
    g_free(parse_name);

    /* set window icon */
    thunar_window_update_window_icon(window);
}

static void thunar_window_menu_item_selected(ThunarWindow *window,
                                             GtkWidget    *menu_item)
{
    gchar *tooltip;
    gint   id;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* we can only display tooltips if we have a statusbar */
    if (G_LIKELY(window->statusbar != NULL))
    {
        tooltip = gtk_widget_get_tooltip_text(menu_item);
        if (G_LIKELY(tooltip != NULL))
        {
            /* push to the statusbar */
            id = gtk_statusbar_get_context_id(GTK_STATUSBAR(window->statusbar), "Menu tooltip");
            gtk_statusbar_push(GTK_STATUSBAR(window->statusbar), id, tooltip);
            g_free(tooltip);
        }
    }
}

static void thunar_window_menu_item_deselected(ThunarWindow *window,
                                               GtkWidget    *menu_item)
{
    UNUSED(menu_item);
    gint id;

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* we can only undisplay tooltips if we have a statusbar */
    if (G_LIKELY(window->statusbar != NULL))
    {
        /* drop the last tooltip from the statusbar */
        id = gtk_statusbar_get_context_id(GTK_STATUSBAR(window->statusbar), "Menu tooltip");
        gtk_statusbar_pop(GTK_STATUSBAR(window->statusbar), id);
    }
}

static void thunar_window_notify_loading(ThunarView   *view,
                                         GParamSpec   *pspec,
                                         ThunarWindow *window)
{
    UNUSED(pspec);

    GdkCursor *cursor;

    thunar_return_if_fail(THUNAR_IS_VIEW(view));
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    if (gtk_widget_get_realized(GTK_WIDGET(window))
            && window->view == GTK_WIDGET(view))
    {
        /* setup the proper cursor */
        if (thunar_view_get_loading(view))
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

static void thunar_window_device_pre_unmount(ThunarDeviceMonitor *device_monitor,
                                             ThunarDevice        *device,
                                             GFile               *root_file,
                                             ThunarWindow        *window)
{
    thunar_return_if_fail(THUNAR_IS_DEVICE_MONITOR(device_monitor));
    thunar_return_if_fail(window->device_monitor == device_monitor);
    thunar_return_if_fail(THUNAR_IS_DEVICE(device));
    thunar_return_if_fail(G_IS_FILE(root_file));
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    /* nothing to do if we don't have a current directory */
    if (G_UNLIKELY(window->current_directory == NULL))
        return;

    /* check if the file is the current directory or an ancestor of the current directory */
    if (g_file_equal(th_file_get_file(window->current_directory), root_file)
            || th_file_is_gfile_ancestor(window->current_directory, root_file))
    {
        /* change to the home folder */
        thunar_window_action_open_home(window);
    }
}

static void thunar_window_device_changed(ThunarDeviceMonitor *device_monitor,
                                         ThunarDevice        *device,
                                         ThunarWindow        *window)
{
    GFile *root_file;

    thunar_return_if_fail(THUNAR_IS_DEVICE_MONITOR(device_monitor));
    thunar_return_if_fail(window->device_monitor == device_monitor);
    thunar_return_if_fail(THUNAR_IS_DEVICE(device));
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));

    if (thunar_device_is_mounted(device))
        return;

    root_file = thunar_device_get_root(device);
    if (root_file != NULL)
    {
        thunar_window_device_pre_unmount(device_monitor, device, root_file, window);
        g_object_unref(root_file);
    }
}

static gboolean thunar_window_save_paned(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);

    Preferences *prefs = get_preferences();
    prefs->separator_position = gtk_paned_get_position(GTK_PANED(window->paned));

    /* for button release event */
    return false;
}

/**
 * thunar_window_set_zoom_level:
 * @window     : a #ThunarWindow instance.
 * @zoom_level : the new zoom level for @window.
 *
 * Sets the zoom level for @window to @zoom_level.
 **/
void thunar_window_set_zoom_level(ThunarWindow   *window,
                                  ThunarZoomLevel zoom_level)
{
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(zoom_level < THUNAR_ZOOM_N_LEVELS);

    /* check if we have a new zoom level */
    if (G_LIKELY(window->zoom_level != zoom_level))
    {
        /* remember the new zoom level */
        window->zoom_level = zoom_level;

        /* notify listeners */
        g_object_notify(G_OBJECT(window), "zoom-level");
    }
}

/**
 * thunar_window_get_current_directory:
 * @window : a #ThunarWindow instance.
 *
 * Queries the #ThunarFile instance, which represents the directory
 * currently displayed within @window. %NULL is returned if @window
 * is not currently associated with any directory.
 *
 * Return value: the directory currently displayed within @window or %NULL.
 **/
ThunarFile* thunar_window_get_current_directory(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), NULL);
    return window->current_directory;
}

/**
 * thunar_window_set_current_directory:
 * @window            : a #ThunarWindow instance.
 * @current_directory : the new directory or %NULL.
 **/
void thunar_window_set_current_directory(ThunarWindow *window,
                                         ThunarFile   *current_directory)
{
//  DPRINT("enter : thunar_window_set_current_directory\n");

    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(current_directory == NULL || THUNAR_IS_FILE(current_directory));

    /* check if we already display the requested directory */
    if (G_UNLIKELY(window->current_directory == current_directory))
        return;

    /* disconnect from the previously active directory */
    if (G_LIKELY(window->current_directory != NULL))
    {
        /* disconnect signals and release reference */
        g_signal_handlers_disconnect_by_func(G_OBJECT(window->current_directory), thunar_window_current_directory_changed, window);
        g_object_unref(G_OBJECT(window->current_directory));
    }

    /* connect to the new directory */
    if (G_LIKELY(current_directory != NULL))
    {
        /* take a reference on the file */
        g_object_ref(G_OBJECT(current_directory));

        window->current_directory = current_directory;

        /* create a new view if the window is new */
        if (window->view == NULL /*num_pages == 0*/)
        {
            thunar_window_create_view(window, window->view, THUNAR_TYPE_DETAILS_VIEW);
        }

        /* connect the "changed"/"destroy" signals */
        g_signal_connect(G_OBJECT(current_directory),
                          "changed",
                          G_CALLBACK(thunar_window_current_directory_changed),
                          window);

        /* update window icon and title */
        thunar_window_current_directory_changed(current_directory, window);

        if (G_LIKELY(window->view != NULL))
        {
            /* grab the focus to the main view */
            gtk_widget_grab_focus(window->view);
        }

        thunar_window_history_changed(window);
        gtk_widget_set_sensitive(window->toolbar_item_parent, !thunar_g_file_is_root(th_file_get_file(current_directory)));
    }

    /* tell everybody that we have a new "current-directory",
     * we do this first so other widgets display the new
     * state already while the folder view is loading.
     */
    g_object_notify(G_OBJECT(window), "current-directory");
}

/**
 * thunar_window_scroll_to_file:
 * @window      : a #ThunarWindow instance.
 * @file        : a #ThunarFile.
 * @select_file : if %TRUE the @file will also be selected.
 * @use_align   : %TRUE to use the alignment arguments.
 * @row_align   : the vertical alignment.
 * @col_align   : the horizontal alignment.
 *
 * Tells the @window to scroll to the @file
 * in the current view.
 **/
void thunar_window_scroll_to_file(ThunarWindow *window,
                                  ThunarFile   *file,
                                  gboolean      select_file,
                                  gboolean      use_align,
                                  gfloat        row_align,
                                  gfloat        col_align)
{
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    /* verify that we have a valid view */
    if (G_LIKELY(window->view != NULL))
        thunar_view_scroll_to_file(THUNAR_VIEW(window->view), file, select_file, use_align, row_align, col_align);
}

gchar** thunar_window_get_directories(ThunarWindow *window,
                                      gint         *active_page)
{
    gint         n;
    gint         n_pages;
    gchar      **uris;
    GtkWidget   *view;
    ThunarFile  *directory;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), NULL);

    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));
    if (G_UNLIKELY(n_pages == 0))
        return NULL;

    /* create array of uris */
    uris = g_new0(gchar *, n_pages + 1);
    for(n = 0; n < n_pages; n++)
    {
        /* get the view */
        view = gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), n);
        thunar_return_val_if_fail(THUNAR_IS_NAVIGATOR(view), FALSE);

        /* get the directory of the view */
        directory = thunar_navigator_get_current_directory(THUNAR_NAVIGATOR(view));
        thunar_return_val_if_fail(THUNAR_IS_FILE(directory), FALSE);

        /* add to array */
        uris[n] = th_file_dup_uri(directory);
    }

    /* selected tab */
    if (active_page != NULL)
        *active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));

    return uris;
}

gboolean thunar_window_set_directories(ThunarWindow   *window,
                                       gchar         **uris,
                                       gint            active_page)
{
    ThunarFile *directory;
    guint       n;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);
    thunar_return_val_if_fail(uris != NULL, FALSE);

    for(n = 0; uris[n] != NULL; n++)
    {
        /* check if the string looks like an uri */
        if (!e_str_looks_like_an_uri(uris[n]))
            continue;

        /* get the file for the uri */
        directory = th_file_get_for_uri(uris[n], NULL);
        if (G_UNLIKELY(directory == NULL))
            continue;

        /* open the directory in a new notebook */
        if (th_file_is_directory(directory))
        {
            if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook)) == 0)
                thunar_window_set_current_directory(window, directory);
            //else
            //    DPRINT("new tab oops");
        }

        g_object_unref(G_OBJECT(directory));
    }

    /* select the page */
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), active_page);

    /* we succeeded if new pages have been opened */
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook)) > 0;
}

const XfceGtkActionEntry* thunar_window_get_action_entry(ThunarWindow *window,
                                                     ThunarWindowAction action)
{
    UNUSED(window);

    return get_action_entry(action);
}

ThunarLauncher* thunar_window_get_launcher(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), NULL);

    return window->launcher;
}

static void thunar_window_redirect_menu_tooltips_to_statusbar_recursive(GtkWidget *menu_item,
                                                                        ThunarWindow *window)
{
    if (GTK_IS_MENU_ITEM(menu_item))
    {
        GtkWidget *submenu = gtk_menu_item_get_submenu(GTK_MENU_ITEM(menu_item));
        if (submenu != NULL)
            gtk_container_foreach(GTK_CONTAINER(submenu),(GtkCallback)(void(*)(void)) thunar_window_redirect_menu_tooltips_to_statusbar_recursive, window);

        /* this disables to show the tooltip on hover */
        gtk_widget_set_has_tooltip(menu_item, FALSE);

        /* These method will put the tooltip on the statusbar */
        g_signal_connect_swapped(G_OBJECT(menu_item), "select", G_CALLBACK(thunar_window_menu_item_selected), window);
        g_signal_connect_swapped(G_OBJECT(menu_item), "deselect", G_CALLBACK(thunar_window_menu_item_deselected), window);
    }
}

/**
 * thunar_window_redirect_menu_tooltips_to_statusbar:
 * @window : a #ThunarWindow instance.
 * @menu   : #GtkMenu for which all tooltips should be shown in the statusbar
 *
 * All tooltips of the provided #GtkMenu and any submenu will not be shown directly any more.
 * Instead they will be shown in the status bar of the passed #ThunarWindow
 **/
void thunar_window_redirect_menu_tooltips_to_statusbar(ThunarWindow *window,
                                                       GtkMenu *menu)
{
    thunar_return_if_fail(THUNAR_IS_WINDOW(window));
    thunar_return_if_fail(GTK_IS_MENU(menu));

    gtk_container_foreach(GTK_CONTAINER(menu),(GtkCallback)(void(*)(void)) thunar_window_redirect_menu_tooltips_to_statusbar_recursive, window);
}

static gboolean thunar_window_button_press_event(GtkWidget      *view,
                                                 GdkEventButton *event,
                                                 ThunarWindow   *window)
{
    UNUSED(view);

    const XfceGtkActionEntry* action_entry;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), FALSE);

    if (event->type == GDK_BUTTON_PRESS)
    {
        if (G_UNLIKELY(event->button == 8))
        {
            action_entry = get_action_entry(THUNAR_WINDOW_ACTION_BACK);
           ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW(window));
            return GDK_EVENT_STOP;
        }
        if (G_UNLIKELY(event->button == 9))
        {
            action_entry = get_action_entry(THUNAR_WINDOW_ACTION_FORWARD);
           ((void(*)(GtkWindow*))action_entry->callback)(GTK_WINDOW(window));
            return GDK_EVENT_STOP;
        }
    }

    return GDK_EVENT_PROPAGATE;
}

static gboolean thunar_window_history_clicked(GtkWidget      *button,
                                              GdkEventButton *event,
                                              GtkWidget      *data)
{
    ThunarHistory *history;
    ThunarWindow  *window;

    thunar_return_val_if_fail(THUNAR_IS_WINDOW(data), FALSE);

    window = THUNAR_WINDOW(data);

    if (event->button == 3)
    {
        history = standard_view_get_history(THUNAR_STANDARD_VIEW(window->view));

        if (button == window->toolbar_item_back)
            thunar_history_show_menu(history, THUNAR_HISTORY_MENU_BACK, button);
        else if (button == window->toolbar_item_forward)
            thunar_history_show_menu(history, THUNAR_HISTORY_MENU_FORWARD, button);
        else
            g_warning("This button is not able to spawn a history menu");
    }

    return FALSE;
}

GtkWidget* thunar_window_get_focused_tree_view(ThunarWindow *window)
{
    thunar_return_val_if_fail(THUNAR_IS_WINDOW(window), NULL);

    GtkWidget *tree_view = GTK_WIDGET(thunar_tree_pane_get_view(
                                     THUNAR_TREE_PANE(window->sidepane)));

    GtkWidget *focused = gtk_window_get_focus(GTK_WINDOW(window));

    return (tree_view == focused) ? tree_view : NULL;
}


