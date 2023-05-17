/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright(c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#include <launcher.h>

#include <application.h>
#include <treeview.h>
#include <preferences.h>
#include <component.h>
#include <browser.h>
#include <clipboard.h>
#include <iconfactory.h>

#include <simplejob.h>
#include <io_scandir.h>
#include <gtk_ext.h>
#include <fnmatch.h>

#include <dialogs.h>
#include <appchooser.h>
#include <propsdlg.h>

typedef struct _LauncherPokeData LauncherPokeData;

// Launcher -------------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
    PROP_SELECTED_FILES,
    PROP_WIDGET,
    PROP_SELECT_FILES_CLOSURE,
    PROP_SELECTED_DEVICE,
    N_PROPERTIES
};

static void launcher_navigator_init(ThunarNavigatorIface *iface);
static void launcher_component_init(ThunarComponentIface *iface);
static void launcher_dispose(GObject *object);
static void launcher_finalize(GObject *object);

// Properties -----------------------------------------------------------------

static void launcher_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void launcher_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);

// Navigator ------------------------------------------------------------------

static ThunarFile* launcher_get_current_directory(ThunarNavigator *navigator);
static void launcher_set_current_directory(ThunarNavigator *navigator,
                                           ThunarFile *current_directory);

// Component ------------------------------------------------------------------

static void launcher_set_selected_files(ThunarComponent *component,
                                        GList *selected_files);

// Public ---------------------------------------------------------------------

// launcher_set_widget
static void _launcher_widget_destroyed(ThunarLauncher *launcher, GtkWidget *widget);

// Build Menu -----------------------------------------------------------------

// launcher_append_open_section
static GtkWidget* _launcher_build_application_submenu(
                                                ThunarLauncher *launcher,
                                                GList          *applications);
// launcher_append_menu_item
static GtkWidget* _launcher_create_document_submenu_new(ThunarLauncher *launcher);
static gboolean _launcher_submenu_templates(ThunarLauncher *launcher,
                                            GtkWidget *create_file_submenu,
                                            GList *files);
static GtkWidget* _launcher_find_parent_menu(ThunarFile *file, GList *dirs,
                                             GList *items);
static gboolean _launcher_show_trash(ThunarLauncher *launcher);
static bool _launcher_can_extract(ThunarLauncher *launcher);

// Poke Files -----------------------------------------------------------------

static void _launcher_menu_item_activated(ThunarLauncher *launcher,
                                          GtkWidget *menu_item);

static void _launcher_poke(ThunarLauncher *launcher,
                           GAppInfo *application_to_use,
                           FolderOpenAction folder_open_action);
static LauncherPokeData* _launcher_poke_data_new(
                                        GList *files_to_poke,
                                        GAppInfo *application_to_use,
                                        FolderOpenAction folder_open_action);
static void _launcher_poke_data_free(LauncherPokeData *data);
static void _launcher_poke_device_finish(ThunarBrowser *browser,
                                         ThunarDevice  *volume,
                                         ThunarFile    *mount_point,
                                         GError        *error,
                                         gpointer       user_data,
                                         gboolean       cancelled);
static void _launcher_poke_files_finish(ThunarBrowser *browser,
                                              ThunarFile    *file,
                                              ThunarFile    *target_file,
                                              GError        *error,
                                              gpointer       user_data);

// ----------------------------------------------------------------------------

static void _launcher_execute_files(ThunarLauncher *launcher, GList *files);
static void _launcher_open_file(ThunarLauncher *launcher, ThunarFile *file,
                                GAppInfo *application_to_use);
static void _launcher_open_files(ThunarLauncher *launcher, GList *files,
                                 GAppInfo *application_to_use);
static guint _launcher_g_app_info_hash(gconstpointer app_info);
static void _launcher_open_paths(GAppInfo *app_info, GList *file_list,
                                 ThunarLauncher *launcher);
static void _launcher_open_windows(ThunarLauncher *launcher, GList *directories);

// Actions --------------------------------------------------------------------

static void _launcher_action_open(ThunarLauncher *launcher);
static void _launcher_action_open_in_new_windows(ThunarLauncher *launcher);
static void _launcher_action_open_with_other(ThunarLauncher *launcher);

static void _launcher_action_create_folder(ThunarLauncher *launcher);
static void _launcher_action_create_document(ThunarLauncher *launcher,
                                             GtkWidget *menu_item);
static void _launcher_action_cut(ThunarLauncher *launcher);
static void _launcher_action_copy(ThunarLauncher *launcher);
static void _launcher_action_paste(ThunarLauncher *launcher);
static void _launcher_action_paste_into_folder(ThunarLauncher *launcher);

static void _launcher_action_move_to_trash(ThunarLauncher *launcher);
static void _launcher_action_delete(ThunarLauncher *launcher);
static void _launcher_action_empty_trash(ThunarLauncher *launcher);
static void _launcher_action_restore(ThunarLauncher *launcher);

static void _launcher_action_duplicate(ThunarLauncher *launcher);
static void _launcher_action_make_link(ThunarLauncher *launcher);

static void _launcher_action_extract(ThunarLauncher *launcher);
static void _launcher_action_terminal(ThunarLauncher *launcher);
static void _launcher_action_unmount_finish(ThunarDevice *device,
                                            const GError *error,
                                            gpointer user_data);
static void _launcher_action_properties(ThunarLauncher *launcher);

// ----------------------------------------------------------------------------

static XfceGtkActionEntry _launcher_actions[] =
{
    {LAUNCHER_ACTION_OPEN,
     "<Actions>/ThunarLauncher/open",
     "<Primary>O",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     "document-open",
     G_CALLBACK(_launcher_action_open)},

    {LAUNCHER_ACTION_OPEN_IN_WINDOW,
     "<Actions>/ThunarLauncher/open-in-new-window",
     "<Primary><shift>O",
     XFCE_GTK_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_open_in_new_windows)},

    {LAUNCHER_ACTION_OPEN_WITH_OTHER,
     "<Actions>/ThunarLauncher/open-with-other",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Open With Other _Application..."),
     N_("Choose another application with which to open the selected file"),
     NULL, G_CALLBACK(_launcher_action_open_with_other)},

    {LAUNCHER_ACTION_EXECUTE,
     "<Actions>/ThunarLauncher/execute",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     "system-run",
     G_CALLBACK(_launcher_action_open)},

    {LAUNCHER_ACTION_CREATE_FOLDER,
     "<Actions>/StandardView/create-folder",
     "<Primary><shift>N",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Create _Folder..."),
     N_("Create an empty folder within the current folder"),
     "folder-new",
     G_CALLBACK(_launcher_action_create_folder)},

    {LAUNCHER_ACTION_CREATE_DOCUMENT,
     "<Actions>/StandardView/create-document",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Create _Document"),
     N_("Create a new document from a template"),
     "document-new",
     G_CALLBACK(NULL)},

    {LAUNCHER_ACTION_CUT,
     "<Actions>/ThunarLauncher/cut",
     "<Primary>X",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Cu_t"),
     N_("Prepare the selected files to be moved with a Paste command"),
     "edit-cut",
     G_CALLBACK(_launcher_action_cut)},

    {LAUNCHER_ACTION_COPY,
     "<Actions>/ThunarLauncher/copy",
     "<Primary>C",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Copy"),
     N_("Prepare the selected files to be copied with a Paste command"),
     "edit-copy",
     G_CALLBACK(_launcher_action_copy)},

    {LAUNCHER_ACTION_PASTE_INTO_FOLDER,
     NULL,
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Paste Into Folder"),
     N_("Move or copy files previously selected by a Cut or Copy command"),
     "edit-paste",
     G_CALLBACK(_launcher_action_paste_into_folder)},

    {LAUNCHER_ACTION_PASTE,
     "<Actions>/ThunarLauncher/paste",
     "<Primary>V",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Paste"),
     N_("Move or copy files previously selected by a Cut or Copy command"),
     "edit-paste",
     G_CALLBACK(_launcher_action_paste)},

    {LAUNCHER_ACTION_MOVE_TO_TRASH,
     "<Actions>/ThunarLauncher/move-to-trash",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Mo_ve to Trash"),
     NULL,
     "user-trash",
     G_CALLBACK(launcher_action_trash_delete)},

    {LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Delete"),
     NULL,
     "edit-delete",
     G_CALLBACK(_launcher_action_delete)},

    {LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete-2",
     "<Shift>Delete",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_delete)},

    {LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete-3",
     "<Shift>KP_Delete",
     0,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_delete)},

    {LAUNCHER_ACTION_EMPTY_TRASH,
     "<Actions>/AppWindow/empty-trash",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Empty Trash"),
     N_("Delete all files and folders in the Trash"),
     NULL,
     G_CALLBACK(_launcher_action_empty_trash)},

    {LAUNCHER_ACTION_RESTORE,
     "<Actions>/ThunarLauncher/restore",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Restore"),
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_restore)},

    {LAUNCHER_ACTION_DUPLICATE,
     "<Actions>/StandardView/duplicate",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Du_plicate"),
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_duplicate)},

    {LAUNCHER_ACTION_MAKE_LINK,
     "<Actions>/StandardView/make-link",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Ma_ke Link"),
     NULL,
     NULL,
     G_CALLBACK(_launcher_action_make_link)},

    {LAUNCHER_ACTION_RENAME,
     "<Actions>/StandardView/rename",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Rename..."),
     NULL,
     NULL,
     G_CALLBACK(launcher_action_rename)},

    {LAUNCHER_ACTION_TERMINAL,
     "<Actions>/StandardView/terminal",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("MyTerminal..."),
     N_("Open in terminal"),
     "utilities-terminal",
     G_CALLBACK(_launcher_action_terminal)},

    {LAUNCHER_ACTION_EXTRACT,
     "<Actions>/StandardView/extract",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Extract..."),
     N_("Extract archive"),
     "archive-manager",
     G_CALLBACK(_launcher_action_extract)},

    {LAUNCHER_ACTION_MOUNT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Mount"),
     N_("Mount the selected device"),
     NULL,
     G_CALLBACK(_launcher_action_open)},

    {LAUNCHER_ACTION_UNMOUNT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Unmount"),
     N_("Unmount the selected device"),
     NULL,
     G_CALLBACK(launcher_action_unmount)},

    {LAUNCHER_ACTION_EJECT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Eject"),
     N_("Eject the selected device"),
     NULL,
     G_CALLBACK(launcher_action_eject)},

    {LAUNCHER_ACTION_PROPERTIES,
     "<Actions>/StandardView/properties",
     "<Alt>Return",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Properties..."),
     N_("View the properties of the selected file"),
     "document-properties",
     G_CALLBACK(_launcher_action_properties)},
};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_launcher_actions, \
                                    G_N_ELEMENTS(_launcher_actions), \
                                    id)

// Poke Data ------------------------------------------------------------------

struct _LauncherPokeData
{
    GList       *files_to_poke;
    GList       *files_poked;
    GAppInfo    *application_to_use;

    FolderOpenAction folder_open_action;
};

// Launcher -------------------------------------------------------------------

static GQuark _launcher_appinfo_quark;
static GQuark _launcher_device_quark;
static GQuark _launcher_file_quark;
static GParamSpec* _launcher_props[N_PROPERTIES] = {NULL,};

struct _ThunarLauncherClass
{
    GObjectClass    __parent__;
};

struct _ThunarLauncher
{
    GObject         __parent__;

    ThunarFile      *current_directory;
    GList           *files_to_process;
    ThunarDevice    *device_to_process;

    gint            n_files_to_process;
    gint            n_directories_to_process;
    gint            n_executables_to_process;
    gint            n_regulars_to_process;
    gboolean        files_to_process_trashable;
    gboolean        files_are_selected;
    gboolean        single_directory_to_process;

    ThunarFile      *single_folder;
    ThunarFile      *parent_folder;

    GClosure        *select_files_closure;

    GtkWidget       *widget;
};

G_DEFINE_TYPE_WITH_CODE(ThunarLauncher,
                        launcher,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARBROWSER, NULL)
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARNAVIGATOR,
                                              launcher_navigator_init)
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_COMPONENT,
                                              launcher_component_init))

static void launcher_class_init(ThunarLauncherClass *klass)
{
    // determine all used quarks
    _launcher_appinfo_quark = g_quark_from_static_string("thunar-launcher-appinfo");
    _launcher_device_quark = g_quark_from_static_string("thunar-launcher-device");
    _launcher_file_quark = g_quark_from_static_string("thunar-launcher-file");

    xfce_gtk_translate_action_entries(_launcher_actions,
                                      G_N_ELEMENTS(_launcher_actions));

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = launcher_dispose;
    gobject_class->finalize = launcher_finalize;
    gobject_class->get_property = launcher_get_property;
    gobject_class->set_property = launcher_set_property;

    _launcher_props[PROP_WIDGET] =
        g_param_spec_object("widget",
                            "widget",
                            "widget",
                            GTK_TYPE_WIDGET,
                            E_PARAM_WRITABLE);

    // The GClosure which will be called if the selected file should be updated
    // after a launcher operation
    _launcher_props[PROP_SELECT_FILES_CLOSURE] =
        g_param_spec_pointer("select-files-closure",
                             "select-files-closure",
                             "select-files-closure",
                             G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY);

    _launcher_props[PROP_SELECTED_DEVICE] =
        g_param_spec_pointer("selected-device",
                             "selected-device",
                             "selected-device",
                             G_PARAM_WRITABLE);

    gpointer g_iface;

    // Override ThunarNavigator's properties
    g_iface = g_type_default_interface_peek(TYPE_THUNARNAVIGATOR);
    _launcher_props[PROP_CURRENT_DIRECTORY] =
        g_param_spec_override("current-directory",
                              g_object_interface_find_property(g_iface,
                                                               "current-directory"));

    // Override ThunarComponent's properties
    g_iface = g_type_default_interface_peek(THUNAR_TYPE_COMPONENT);
    _launcher_props[PROP_SELECTED_FILES] =
        g_param_spec_override("selected-files",
                              g_object_interface_find_property(g_iface,
                                                               "selected-files"));

    // install properties
    g_object_class_install_properties(gobject_class, N_PROPERTIES, _launcher_props);
}

static void launcher_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = launcher_get_current_directory;
    iface->set_current_directory = launcher_set_current_directory;
}

static void launcher_component_init(ThunarComponentIface *iface)
{
    iface->get_selected_files = NULL; // (gpointer) e_noop_null;
    iface->set_selected_files = launcher_set_selected_files;
}

static void launcher_init(ThunarLauncher *launcher)
{
    launcher->files_to_process = NULL;
    launcher->select_files_closure = NULL;
    launcher->device_to_process = NULL;
}

// GObject --------------------------------------------------------------------

static void launcher_dispose(GObject *object)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

    // reset our properties
    navigator_set_current_directory(THUNARNAVIGATOR(launcher), NULL);
    launcher_set_widget(THUNAR_LAUNCHER(launcher), NULL);

    // disconnect from the currently selected files
    e_list_free(launcher->files_to_process);
    launcher->files_to_process = NULL;

    // unref parent, if any
    if (launcher->parent_folder != NULL)
        g_object_unref(launcher->parent_folder);

    G_OBJECT_CLASS(launcher_parent_class)->dispose(object);
}

static void launcher_finalize(GObject *object)
{
    //ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

    G_OBJECT_CLASS(launcher_parent_class)->finalize(object);
}

// Properties -----------------------------------------------------------------

static void launcher_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value,
                           navigator_get_current_directory(THUNARNAVIGATOR(object)));
        break;

    case PROP_SELECTED_FILES:
        g_value_set_boxed(value,
                          component_get_selected_files(THUNAR_COMPONENT(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void launcher_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNARNAVIGATOR(object),
                                        g_value_get_object(value));
        break;

    case PROP_SELECTED_FILES:
        component_set_selected_files(THUNAR_COMPONENT(object),
                                     g_value_get_boxed(value));
        break;

    case PROP_WIDGET:
        launcher_set_widget(launcher, g_value_get_object(value));
        break;

    case PROP_SELECT_FILES_CLOSURE:
        launcher->select_files_closure = g_value_get_pointer(value);
        break;

    case PROP_SELECTED_DEVICE:
        launcher->device_to_process = g_value_get_pointer(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Navigator ------------------------------------------------------------------

static ThunarFile* launcher_get_current_directory(ThunarNavigator *navigator)
{
    return THUNAR_LAUNCHER(navigator)->current_directory;
}

static void launcher_set_current_directory(ThunarNavigator *navigator,
                                           ThunarFile *current_directory)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(navigator);

    // disconnect from the previous directory
    if (G_LIKELY(launcher->current_directory != NULL))
        g_object_unref(G_OBJECT(launcher->current_directory));

    // activate the new directory
    launcher->current_directory = current_directory;

    // connect to the new directory
    if (G_LIKELY(current_directory != NULL))
    {
        g_object_ref(G_OBJECT(current_directory));

        // update files_to_process if not initialized yet
        if (launcher->files_to_process == NULL)
            launcher_set_selected_files(THUNAR_COMPONENT(navigator), NULL);
    }

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(launcher), _launcher_props[PROP_CURRENT_DIRECTORY]);
}

// Component ------------------------------------------------------------------

static void launcher_set_selected_files(ThunarComponent *component,
                                        GList *selected_files)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(component);

    // That happens at startup for some reason
    if (launcher->current_directory == NULL)
        return;

    // disconnect from the previous files to process
    if (launcher->files_to_process != NULL)
        e_list_free(launcher->files_to_process);

    launcher->files_to_process = NULL;

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(launcher), _launcher_props[PROP_SELECTED_FILES]);

    // unref previous parent, if any
    if (launcher->parent_folder != NULL)
        g_object_unref(launcher->parent_folder);

    launcher->parent_folder = NULL;
    launcher->files_are_selected = TRUE;

    if (selected_files == NULL || g_list_length(selected_files) == 0)
        launcher->files_are_selected = FALSE;

    launcher->files_to_process_trashable = TRUE;
    launcher->n_files_to_process         = 0;
    launcher->n_directories_to_process   = 0;
    launcher->n_executables_to_process   = 0;
    launcher->n_regulars_to_process      = 0;

    launcher->single_directory_to_process = FALSE;
    launcher->single_folder = NULL;
    launcher->parent_folder = NULL;

    // if nothing is selected, the current directory is the folder to use for all menus
    if (launcher->files_are_selected)
        launcher->files_to_process = e_list_copy(selected_files);
    else
        launcher->files_to_process = g_list_append(launcher->files_to_process,
                                                   launcher->current_directory);

    // determine the number of files/directories/executables
    GList *lp;
    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next, ++launcher->n_files_to_process)
    {
        // Keep a reference on all selected files
        g_object_ref(lp->data);

        if (th_file_is_directory(lp->data)
            || th_file_is_shortcut(lp->data)
            || th_file_is_mountable(lp->data))
        {
            ++launcher->n_directories_to_process;
        }
        else
        {
            if (th_file_is_executable(lp->data))
                ++launcher->n_executables_to_process;

            ++launcher->n_regulars_to_process;
        }

        if (!th_file_can_be_trashed(lp->data))
            launcher->files_to_process_trashable = FALSE;
    }

    launcher->single_directory_to_process =(launcher->n_directories_to_process == 1 && launcher->n_files_to_process == 1);

    if (launcher->single_directory_to_process)
    {
        // grab the folder of the first selected item
        launcher->single_folder = THUNAR_FILE(launcher->files_to_process->data);
    }

    if (launcher->files_to_process != NULL)
    {
        // just grab the folder of the first selected item
        launcher->parent_folder = th_file_get_parent(THUNAR_FILE(launcher->files_to_process->data), NULL);
    }
}

// Public ---------------------------------------------------------------------

GtkWidget* launcher_get_widget(ThunarLauncher *launcher)
{
    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);
    return launcher->widget;
}

void launcher_set_widget(ThunarLauncher *launcher, GtkWidget *widget)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));
    e_return_if_fail(widget == NULL || GTK_IS_WIDGET(widget));

    // disconnect from the previous widget
    if (G_UNLIKELY(launcher->widget != NULL))
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(launcher->widget),
                                             _launcher_widget_destroyed, launcher);
        g_object_unref(G_OBJECT(launcher->widget));
    }

    launcher->widget = widget;

    // connect to the new widget
    if (G_LIKELY(widget != NULL))
    {
        g_object_ref(G_OBJECT(widget));
        g_signal_connect_swapped(G_OBJECT(widget), "destroy",
                                 G_CALLBACK(_launcher_widget_destroyed), launcher);
    }

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(launcher), _launcher_props[PROP_WIDGET]);
}

static void _launcher_widget_destroyed(ThunarLauncher *launcher, GtkWidget *widget)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));
    e_return_if_fail(launcher->widget == widget);
    e_return_if_fail(GTK_IS_WIDGET(widget));

    // just reset the widget property for the launcher
    launcher_set_widget(launcher, NULL);
}

// Build Menu -----------------------------------------------------------------

void launcher_append_accelerators(ThunarLauncher *launcher, GtkAccelGroup *accel_group)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    xfce_gtk_accel_map_add_entries(_launcher_actions,
                                   G_N_ELEMENTS(_launcher_actions));

    xfce_gtk_accel_group_connect_action_entries(accel_group,
                                                _launcher_actions,
                                                G_N_ELEMENTS(_launcher_actions),
                                                launcher);
}

gboolean launcher_append_open_section(ThunarLauncher *launcher,
                                             GtkMenuShell   *menu,
                                             gboolean        support_tabs,
                                             gboolean        support_change_directory,
                                             gboolean        force)
{
    (void) support_tabs;
    GList     *applications;
    gchar     *label_text;
    gchar     *tooltip_text;
    GtkWidget *image;
    GtkWidget *menu_item;
    GtkWidget *submenu;

    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);

    // Usually it is not required to open the current directory
    if (launcher->files_are_selected == FALSE && !force)
        return FALSE;

    // determine the set of applications that work for all selected files
    applications = th_filelist_get_applications(launcher->files_to_process);

    // Execute OR Open OR OpenWith
    if (G_UNLIKELY(launcher->n_executables_to_process == launcher->n_files_to_process))
        launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), LAUNCHER_ACTION_EXECUTE, FALSE);
    else if (G_LIKELY(launcher->n_directories_to_process >= 1))
    {
        if (support_change_directory)
            launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), LAUNCHER_ACTION_OPEN, FALSE);
    }
    else if (G_LIKELY(applications != NULL))
    {
        label_text = g_strdup_printf(_("_Open With \"%s\""), g_app_info_get_name(applications->data));
        tooltip_text = g_strdup_printf(ngettext("Use \"%s\" to open the selected file",
                                        "Use \"%s\" to open the selected files",
                                        launcher->n_files_to_process), g_app_info_get_name(applications->data));

        image = gtk_image_new_from_gicon(g_app_info_get_icon(applications->data), GTK_ICON_SIZE_MENU);
        menu_item = xfce_gtk_image_menu_item_new(label_text, tooltip_text, NULL, G_CALLBACK(_launcher_menu_item_activated),
                    G_OBJECT(launcher), image, menu);

        // remember the default application for the "Open" action as quark
        g_object_set_qdata_full(G_OBJECT(menu_item), _launcher_appinfo_quark, applications->data, g_object_unref);
        g_free(tooltip_text);
        g_free(label_text);

        // drop the default application from the list
        applications = g_list_delete_link(applications, applications);
    }
    else
    {
        // we can only show a generic "Open" action
        label_text = g_strdup_printf(_("_Open With Default Applications"));
        tooltip_text = g_strdup_printf(ngettext("Open the selected file with the default application",
                                        "Open the selected files with the default applications", launcher->n_files_to_process));
        xfce_gtk_menu_item_new(label_text, tooltip_text, NULL, G_CALLBACK(_launcher_menu_item_activated), G_OBJECT(launcher), menu);
        g_free(tooltip_text);
        g_free(label_text);
    }

    if (launcher->n_files_to_process == launcher->n_directories_to_process
            && launcher->n_directories_to_process >= 1)
    {
        launcher_append_menu_item(launcher,
                                         GTK_MENU_SHELL(menu),
                                         LAUNCHER_ACTION_OPEN_IN_WINDOW,
                                         FALSE);
    }

    if (G_LIKELY(applications != NULL))
    {
        menu_item = xfce_gtk_menu_item_new(_("Open With"),
                                            _("Choose another application with which to open the selected file"),
                                            NULL, NULL, NULL, menu);
        submenu = _launcher_build_application_submenu(launcher, applications);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }
    else
    {
        if (launcher->n_files_to_process == 1)
            launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
    }

    g_list_free_full(applications, g_object_unref);

    return TRUE;
}

static GtkWidget* _launcher_build_application_submenu(
                                                ThunarLauncher *launcher,
                                                GList          *applications)
{
    GList     *lp;
    GtkWidget *submenu;
    GtkWidget *image;
    GtkWidget *item;
    gchar     *label_text;
    gchar     *tooltip_text;

    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    submenu = gtk_menu_new();

    // add open with subitem per application
    for (lp = applications; lp != NULL; lp = lp->next)
    {
        label_text = g_strdup_printf(_("Open With \"%s\""), g_app_info_get_name(lp->data));
        tooltip_text = g_strdup_printf(ngettext("Use \"%s\" to open the selected file",
                                        "Use \"%s\" to open the selected files",
                                        launcher->n_files_to_process), g_app_info_get_name(lp->data));
        image = gtk_image_new_from_gicon(g_app_info_get_icon(lp->data), GTK_ICON_SIZE_MENU);
        item = xfce_gtk_image_menu_item_new(label_text,
                                            tooltip_text,
                                            NULL,
                                            G_CALLBACK(_launcher_menu_item_activated),
                                            G_OBJECT(launcher),
                                            image,
                                            GTK_MENU_SHELL(submenu));
        g_object_set_qdata_full(G_OBJECT(item), _launcher_appinfo_quark, g_object_ref(lp->data), g_object_unref);
        g_free(tooltip_text);
        g_free(label_text);
    }

    if (launcher->n_files_to_process == 1)
    {
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(submenu));
        launcher_append_menu_item(launcher, GTK_MENU_SHELL(submenu), LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
    }

    return submenu;
}

GtkWidget* launcher_append_menu_item(ThunarLauncher  *launcher,
                                     GtkMenuShell    *menu,
                                     LauncherAction action,
                                     gboolean        force)
{
    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    const XfceGtkActionEntry *action_entry = get_action_entry(action);

    e_return_val_if_fail(action_entry != NULL, NULL);

    GtkWidget                *item = NULL;
    GtkWidget                *submenu;
    GtkWidget                *focused_widget;
    gchar                    *label_text;
    gchar                    *tooltip_text;
    gboolean                  show_item;
    ClipboardManager   *clipboard;
    ThunarFile               *parent;
    gint                      n;


    // This may occur when the thunar-window is build
    if (G_UNLIKELY(launcher->files_to_process == NULL)
        && launcher->device_to_process == NULL)
        return NULL;

    gboolean show_delete_item = false;

    switch (action)
    {
    case LAUNCHER_ACTION_OPEN:
        return xfce_gtk_image_menu_item_new_from_icon_name(
                                    _("_Open"),
                                    ngettext("Open the selected file",
                                             "Open the selected files",
                                             launcher->n_files_to_process),
                                    action_entry->accel_path,
                                    action_entry->callback,
                                    G_OBJECT(launcher),
                                    action_entry->menu_item_icon_name,
                                    menu);

    case LAUNCHER_ACTION_OPEN_IN_WINDOW:
        n = launcher->n_files_to_process > 0 ? launcher->n_files_to_process : 1;
        label_text = g_strdup_printf(
                    ngettext("Open in New _Window",
                    "Open in %d New _Windows", n),
                    n);
        tooltip_text = g_strdup_printf(
                    ngettext("Open the selected directory in new window",
                    "Open the selected directories in %d new windows", n),
                    n);
        item = xfce_gtk_menu_item_new(label_text,
                                      tooltip_text,
                                      action_entry->accel_path,
                                      action_entry->callback,
                                      G_OBJECT(launcher),
                                      menu);
        g_free(tooltip_text);
        g_free(label_text);
        return item;

    case LAUNCHER_ACTION_OPEN_WITH_OTHER:
        return xfce_gtk_menu_item_new(action_entry->menu_item_label_text,
                                      action_entry->menu_item_tooltip_text,
                                      action_entry->accel_path,
                                      action_entry->callback,
                                      G_OBJECT(launcher),
                                      menu);

    case LAUNCHER_ACTION_EXECUTE:
        return xfce_gtk_image_menu_item_new_from_icon_name(
                                        _("_Execute"),
                                        ngettext("Execute the selected file",
                                                 "Execute the selected files",
                                                 launcher->n_files_to_process),
                                        action_entry->accel_path,
                                        action_entry->callback,
                                        G_OBJECT(launcher),
                                        action_entry->menu_item_icon_name,
                                        menu);

    case LAUNCHER_ACTION_CREATE_FOLDER:
        if (IS_TREEVIEW(launcher->widget)
            && launcher->files_are_selected
            && launcher->single_directory_to_process)
            parent = launcher->single_folder;
        else
            parent = launcher->current_directory;

        if (th_file_is_trashed(parent))
            return NULL;

        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, th_file_is_writable(parent));
        return item;

    case LAUNCHER_ACTION_CREATE_DOCUMENT:
        if (IS_TREEVIEW(launcher->widget)
            && launcher->files_are_selected
            && launcher->single_directory_to_process)
            parent = launcher->single_folder;
        else
            parent = launcher->current_directory;

        if (th_file_is_trashed(parent))
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        submenu = _launcher_create_document_submenu_new(launcher);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        gtk_widget_set_sensitive(item, th_file_is_writable(parent));
        return item;

    case LAUNCHER_ACTION_CUT:
        focused_widget = etk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Cut the selection"),
                       action_entry->accel_path, G_CALLBACK(gtk_editable_cut_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, etk_editable_can_cut(GTK_EDITABLE(focused_widget)));
        }
        else
        {
            show_item = launcher->files_are_selected;
            if (!show_item && !force)
                return NULL;
            tooltip_text = ngettext("Prepare the selected file to be moved with a Paste command",
                                     "Prepare the selected files to be moved with a Paste command", launcher->n_files_to_process);
            item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                    action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && th_file_is_writable(launcher->parent_folder));
        }
        return item;

    case LAUNCHER_ACTION_COPY:
        focused_widget = etk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Copy the selection"),
                       action_entry->accel_path,G_CALLBACK(gtk_editable_copy_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, etk_editable_can_copy(GTK_EDITABLE(focused_widget)));
        }
        else
        {
            show_item = launcher->files_are_selected;
            if (!show_item && !force)
                return NULL;
            tooltip_text = ngettext("Prepare the selected file to be copied with a Paste command",
                                     "Prepare the selected files to be copied with a Paste command", launcher->n_files_to_process);
            item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                    action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, show_item);
        }
        return item;

    case LAUNCHER_ACTION_PASTE_INTO_FOLDER:
        if (!launcher->single_directory_to_process)
            return NULL;
        clipboard = clipman_get_for_display(gtk_widget_get_display(launcher->widget));
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, clipman_can_paste(clipboard) && th_file_is_writable(launcher->single_folder));
        g_object_unref(clipboard);
        return item;

    case LAUNCHER_ACTION_PASTE:

        focused_widget = etk_get_focused_widget();

        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Paste the clipboard"),
                       action_entry->accel_path,G_CALLBACK(gtk_editable_paste_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, etk_editable_can_paste(GTK_EDITABLE(focused_widget)));
        }
        else
        {
            if (launcher->single_directory_to_process && launcher->files_are_selected)
                return launcher_append_menu_item(
                                                    launcher,
                                                    menu,
                                                    LAUNCHER_ACTION_PASTE_INTO_FOLDER,
                                                    force);
            clipboard = clipman_get_for_display(gtk_widget_get_display(launcher->widget));
            item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                            G_OBJECT(launcher),
                                                            GTK_MENU_SHELL(menu));
            gtk_widget_set_sensitive(item, clipman_can_paste(clipboard) && th_file_is_writable(launcher->current_directory));
            g_object_unref(clipboard);
        }
        return item;

    case LAUNCHER_ACTION_MOVE_TO_TRASH:
        if (!_launcher_show_trash(launcher))
            return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
            return NULL;

        tooltip_text = ngettext("Move the selected file to the Trash",
                                 "Move the selected files to the Trash", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && th_file_is_writable(launcher->parent_folder));
        return item;


    case LAUNCHER_ACTION_DELETE:
        if (_launcher_show_trash(launcher) && !show_delete_item)
            return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
            return NULL;

        tooltip_text = ngettext("Permanently delete the selected file",
                                 "Permanently delete the selected files", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && th_file_is_writable(launcher->parent_folder));
        return item;

    case LAUNCHER_ACTION_EMPTY_TRASH:
        if (launcher->single_directory_to_process == TRUE)
        {
            if (th_file_is_root(launcher->single_folder) && th_file_is_trashed(launcher->single_folder))
            {
                item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text, action_entry->accel_path,
                        action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
                gtk_widget_set_sensitive(item, th_file_get_item_count(launcher->single_folder) > 0);
                return item;
            }
        }
        return NULL;

    case LAUNCHER_ACTION_RESTORE:
        if (launcher->files_are_selected && th_file_is_trashed(launcher->current_directory))
        {
            tooltip_text = ngettext("Restore the selected file to its original location",
                                     "Restore the selected files to its original location", launcher->n_files_to_process);
            item = xfce_gtk_menu_item_new(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                           action_entry->callback, G_OBJECT(launcher), menu);
            gtk_widget_set_sensitive(item, th_file_is_writable(launcher->current_directory));
            return item;
        }
        return NULL;

    case LAUNCHER_ACTION_DUPLICATE:
        show_item = th_file_is_writable(launcher->current_directory) &&
                    launcher->files_are_selected &&
                    th_file_is_trashed(launcher->current_directory) == FALSE;
        if (!show_item && !force)
            return NULL;
        item = xfce_gtk_menu_item_new(action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT(launcher), menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && th_file_is_writable(launcher->parent_folder));
        return item;

    case LAUNCHER_ACTION_MAKE_LINK:
        show_item = th_file_is_writable(launcher->current_directory)
                &&
                    launcher->files_are_selected
                &&
                    th_file_is_trashed(launcher->current_directory) == FALSE;
        if (!show_item && !force)
            return NULL;

        label_text = ngettext("Ma_ke Link", "Ma_ke Links", launcher->n_files_to_process);
        tooltip_text = ngettext("Create a symbolic link for the selected file",
                                 "Create a symbolic link for each selected file", launcher->n_files_to_process);
        item = xfce_gtk_menu_item_new(label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                       G_OBJECT(launcher), menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && th_file_is_writable(launcher->parent_folder));
        return item;

    case LAUNCHER_ACTION_RENAME:
        show_item = th_file_is_writable(launcher->current_directory)
                    && launcher->files_are_selected
                    && th_file_is_trashed(launcher->current_directory) == FALSE;
        if (!show_item && !force)
            return NULL;
        tooltip_text = ngettext("Rename the selected file",
                                "Rename the selected files", launcher->n_files_to_process);
        item = xfce_gtk_menu_item_new(action_entry->menu_item_label_text,
                                      tooltip_text,
                                      action_entry->accel_path,
                                      action_entry->callback,
                                      G_OBJECT(launcher),
                                      menu);
        gtk_widget_set_sensitive(item, show_item
                                 && launcher->parent_folder != NULL
                                 && th_file_is_writable(launcher->parent_folder));
        return item;

    case LAUNCHER_ACTION_TERMINAL:
        if (!launcher->single_directory_to_process)
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        return item;

    case LAUNCHER_ACTION_EXTRACT:
        if (!_launcher_can_extract(launcher))
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        return item;

    case LAUNCHER_ACTION_MOUNT:
        if (launcher->device_to_process == NULL
            || th_device_is_mounted(launcher->device_to_process) == TRUE)
            return NULL;
        return xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));

    case LAUNCHER_ACTION_UNMOUNT:
        if (launcher->device_to_process == NULL
            || th_device_is_mounted(launcher->device_to_process) == FALSE)
            return NULL;
        return xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));

    case LAUNCHER_ACTION_EJECT:
        if (launcher->device_to_process == NULL
            || th_device_get_kind(launcher->device_to_process) != THUNARDEVICE_KIND_VOLUME)
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, th_device_can_eject(launcher->device_to_process));
        return item;

    default:
        return xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
    }

    return NULL;
}

static GtkWidget* _launcher_create_document_submenu_new(ThunarLauncher *launcher)
{
    GList           *files = NULL;
    GFile           *home_dir;
    GFile           *templates_dir = NULL;
    const gchar     *path;
    gchar           *template_path;
    gchar           *label_text;
    GtkWidget       *submenu;
    GtkWidget       *item;

    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    home_dir = e_file_new_for_home();
    path = g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES);

    if (G_LIKELY(path != NULL))
        templates_dir = g_file_new_for_path(path);

    // If G_USER_DIRECTORY_TEMPLATES not found, set "~/Templates" directory as default
    if (G_UNLIKELY(path == NULL) || G_UNLIKELY(g_file_equal(templates_dir, home_dir)))
    {
        if (templates_dir != NULL)
            g_object_unref(templates_dir);
        templates_dir = g_file_resolve_relative_path(home_dir, "Templates");
    }

    if (G_LIKELY(templates_dir != NULL))
    {
        // load the ThunarFiles
        files = io_scan_directory(NULL, templates_dir, G_FILE_QUERY_INFO_NONE, TRUE, FALSE, TRUE, NULL);
    }

    submenu = gtk_menu_new();
    if (files == NULL)
    {
        template_path = g_file_get_path(templates_dir);
        label_text = g_strdup_printf(_("No templates installed in \"%s\""), template_path);
        item = xfce_gtk_image_menu_item_new(label_text, NULL, NULL, NULL, NULL, NULL, GTK_MENU_SHELL(submenu));
        gtk_widget_set_sensitive(item, FALSE);
        g_free(template_path);
        g_free(label_text);
    }
    else
    {
        _launcher_submenu_templates(launcher, submenu, files);
        e_list_free(files);
    }

    xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(submenu));
    xfce_gtk_image_menu_item_new_from_icon_name(_("_Empty File"), NULL, NULL,
                                                G_CALLBACK(_launcher_action_create_document),
            G_OBJECT(launcher), "text-x-generic", GTK_MENU_SHELL(submenu));


    g_object_unref(templates_dir);
    g_object_unref(home_dir);

    return submenu;
}

// recursive helper method in order to create menu items for all available templates
static gboolean _launcher_submenu_templates(ThunarLauncher *launcher,
                                            GtkWidget *create_file_submenu,
                                            GList *files)
{
    IconFactory *icon_factory;
    ThunarFile        *file;
    GdkPixbuf         *icon;
    GtkWidget         *parent_menu;
    GtkWidget         *submenu;
    GtkWidget         *image;
    GtkWidget         *item;
    GList             *lp;
    GList             *dirs = NULL;
    GList             *items = NULL;

    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);

    // do nothing if there is no menu
    if (create_file_submenu == NULL)
        return FALSE;

    // get the icon factory
    icon_factory = iconfact_get_default();

    /* sort items so that directories come before files and ancestors come
     * before descendants */
    files = g_list_sort(files,(GCompareFunc)(void(*)(void)) th_file_compare_by_type);

    for (lp = g_list_first(files); lp != NULL; lp = lp->next)
    {
        file = lp->data;

        // determine the parent menu for this file/directory
        parent_menu = _launcher_find_parent_menu(file, dirs, items);
        if (parent_menu == NULL)
            parent_menu = create_file_submenu;

        // determine the icon for this file/directory
        icon = iconfact_load_file_icon(icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 16);

        // allocate an image based on the icon
        image = gtk_image_new_from_pixbuf(icon);

        item = xfce_gtk_image_menu_item_new(th_file_get_display_name(file), NULL, NULL, NULL, NULL, image, GTK_MENU_SHELL(parent_menu));
        if (th_file_is_directory(file))
        {
            // allocate a new submenu for the directory
            submenu = gtk_menu_new();

            // allocate a new menu item for the directory
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

            /* prepend the directory, its item and the parent menu it should
             * later be added to to the respective lists */
            dirs = g_list_prepend(dirs, file);
            items = g_list_prepend(items, item);
        }
        else
        {
            g_signal_connect_swapped(
                        G_OBJECT(item), "activate",
                        G_CALLBACK(_launcher_action_create_document), launcher);

            g_object_set_qdata_full(G_OBJECT(item),
                                    _launcher_file_quark,
                                    g_object_ref(file),
                                    g_object_unref);
        }

        // release the icon reference
        g_object_unref(icon);
    }

    // destroy lists
    g_list_free(dirs);
    g_list_free(items);

    // release the icon factory
    g_object_unref(icon_factory);

    // let the job destroy the file list
    return FALSE;
}

// helper method in order to find the parent menu for a menu item
static GtkWidget* _launcher_find_parent_menu(ThunarFile *file, GList *dirs,
                                             GList *items)
{
    GtkWidget *parent_menu = NULL;
    GFile     *parent;
    GList     *lp;
    GList     *ip;

    // determine the parent of the file
    parent = g_file_get_parent(th_file_get_file(file));

    // check if the file has a parent at all
    if (parent == NULL)
        return NULL;

    // iterate over all dirs and menu items
    for (lp = g_list_first(dirs), ip = g_list_first(items);
            parent_menu == NULL && lp != NULL && ip != NULL;
            lp = lp->next, ip = ip->next)
    {
        // check if the current dir/item is the parent of our file
        if (g_file_equal(parent, th_file_get_file(lp->data)))
        {
            // we want to insert an item for the file in this menu
            parent_menu = gtk_menu_item_get_submenu(ip->data);
        }
    }

    // destroy the parent GFile
    g_object_unref(parent);

    return parent_menu;
}

static gboolean _launcher_show_trash(ThunarLauncher *launcher)
{
    if (launcher->parent_folder == NULL)
        return FALSE;

    // If the folder is read only, always show trash insensitive
    /* If we are outside waste basket, the selection is trashable
     * and we support trash, show trash */

    return !th_file_is_writable(launcher->parent_folder)
            || (!th_file_is_trashed(launcher->parent_folder)
            && launcher->files_to_process_trashable
            && e_vfs_is_uri_scheme_supported("trash"));
}

static bool _launcher_can_extract(ThunarLauncher *launcher)
{
    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), false);

    //DPRINT("can extract\n");

    if (launcher->n_files_to_process != 1 || !launcher->files_are_selected)
        return false;

    ThunarFile *file = THUNAR_FILE(launcher->files_to_process->data);
    const gchar *filename = th_file_get_display_name(file);

    Preferences *prefs = get_preferences();
    CStringList *filters = prefs->extractflt;
    if (filters)
    {
        int size = cstrlist_size(filters);
        for (int i = 0; i < size; ++i)
        {
            CString *str = cstrlist_at(filters, i);

            if (fnmatch(c_str(str), filename, 0) == 0)
                return true;
        }
    }

    return false;
}

// Open -----------------------------------------------------------------------

void launcher_open_selected_folders(ThunarLauncher *launcher)
{
    GList *lp;

    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
        e_return_if_fail(th_file_is_directory(THUNAR_FILE(lp->data)));

    _launcher_poke(launcher, NULL, LAUNCHER_OPEN_AS_NEW_WINDOW);
}

static void _launcher_menu_item_activated(ThunarLauncher *launcher,
                                                GtkWidget      *menu_item)
{
    GAppInfo *app_info;

    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    // if we have a mime handler associated with the menu_item, we pass it to the launcher(g_object_get_qdata will return NULL otherwise)
    app_info = g_object_get_qdata(G_OBJECT(menu_item), _launcher_appinfo_quark);
    launcher_activate_selected_files(launcher, LAUNCHER_CHANGE_DIRECTORY, app_info);
}

void launcher_activate_selected_files(ThunarLauncher  *launcher,
                                             FolderOpenAction action,
                                             GAppInfo        *app_info)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    _launcher_poke(launcher, app_info, action);
}

// Poke files -----------------------------------------------------------------

static void _launcher_poke(ThunarLauncher *launcher,
                           GAppInfo *application_to_use,
                           FolderOpenAction folder_open_action)
{

    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL)
    {
        g_warning("No files to process. _launcher_poke aborted.");
        return;
    }

    LauncherPokeData *poke_data = _launcher_poke_data_new(
                                                launcher->files_to_process,
                                                application_to_use,
                                                folder_open_action);

    if (launcher->device_to_process != NULL)
    {
        browser_poke_device(THUNARBROWSER(launcher),
                                   launcher->device_to_process,
                                   launcher->widget,
                                   _launcher_poke_device_finish,
                                   poke_data);
    }
    else
    {
        // We will only poke one file at a time,
        // in order to dont use all available CPU's
        // TODO: Check if that could cause slowness
        browser_poke_file(THUNARBROWSER(launcher),
                                 poke_data->files_to_poke->data,
                                 launcher->widget,
                                 _launcher_poke_files_finish,
                                 poke_data);
    }
}

static LauncherPokeData* _launcher_poke_data_new(
                                    GList     *files_to_poke,
                                    GAppInfo  *application_to_use,
                                    FolderOpenAction folder_open_action)
{
    LauncherPokeData *data = g_slice_new0(LauncherPokeData);

    data->files_to_poke = e_list_copy(files_to_poke);
    data->files_poked = NULL;
    data->application_to_use = application_to_use;

    // keep a reference on the appdata
    if (application_to_use != NULL)
        g_object_ref(application_to_use);

    data->folder_open_action = folder_open_action;

    return data;
}

static void _launcher_poke_data_free(LauncherPokeData *data)
{
    e_return_if_fail(data != NULL);

    e_list_free(data->files_to_poke);
    e_list_free(data->files_poked);

    if (data->application_to_use != NULL)
        g_object_unref(data->application_to_use);

    g_slice_free(LauncherPokeData, data);
}

static void _launcher_poke_device_finish(ThunarBrowser *browser,
                                               ThunarDevice  *volume,
                                               ThunarFile    *mount_point,
                                               GError        *error,
                                               gpointer       user_data,
                                               gboolean       cancelled)
{
    LauncherPokeData *poke_data = user_data;
    gchar                  *device_name;

    if (error != NULL)
    {
        device_name = th_device_get_name(volume);
        dialog_error(GTK_WIDGET(THUNAR_LAUNCHER(browser)->widget), error, _("Failed to mount \"%s\""), device_name);
        g_free(device_name);
    }

    if (cancelled == TRUE || error != NULL || mount_point == NULL)
    {
        _launcher_poke_data_free(poke_data);
        return;
    }

    if (poke_data->folder_open_action == LAUNCHER_OPEN_AS_NEW_WINDOW)
    {
        GList *directories = NULL;
        directories = g_list_append(directories, mount_point);
        _launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
        g_list_free(directories);
    }
    else if (poke_data->folder_open_action == LAUNCHER_CHANGE_DIRECTORY)
    {
        navigator_change_directory(THUNARNAVIGATOR(browser), mount_point);
    }

    _launcher_poke_data_free(poke_data);
}

static void _launcher_poke_files_finish(ThunarBrowser *browser,
                                              ThunarFile    *file,
                                              ThunarFile    *target_file,
                                              GError        *error,
                                              gpointer       user_data)
{
    LauncherPokeData *poke_data = user_data;
    gboolean                executable = TRUE;
    GList                  *directories = NULL;
    GList                  *files = NULL;
    GList                  *lp;

    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(THUNAR_IS_FILE(file));
    e_return_if_fail(poke_data != NULL);
    e_return_if_fail(poke_data->files_to_poke != NULL);

    // check if poking succeeded
    if (error == NULL)
    {
        // add the resolved file to the list of file to be opened/executed later
        poke_data->files_poked = g_list_prepend(poke_data->files_poked,g_object_ref(target_file));
    }

    // release and remove the just poked file from the list
    g_object_unref(poke_data->files_to_poke->data);
    poke_data->files_to_poke = g_list_delete_link(poke_data->files_to_poke,
                                                  poke_data->files_to_poke);

    if (poke_data->files_to_poke == NULL)
    {
        // separate files and directories in the selected files list
        for (lp = poke_data->files_poked; lp != NULL; lp = lp->next)
        {
            if (th_file_is_directory(lp->data))
            {
                // add to our directory list
                directories = g_list_prepend(directories, lp->data);
            }
            else
            {
                // add to our file list
                files = g_list_prepend(files, lp->data);

                // check if the file is executable
                executable = (executable && th_file_is_executable(lp->data));
            }
        }

        // check if we have any directories to process
        if (G_LIKELY(directories != NULL))
        {
            if (poke_data->application_to_use != NULL)
            {
                // open them separately, using some specific application
                for (lp = directories; lp != NULL; lp = lp->next)
                    _launcher_open_file(THUNAR_LAUNCHER(browser),
                                              lp->data, poke_data->application_to_use);
            }

            else if (poke_data->folder_open_action == LAUNCHER_OPEN_AS_NEW_WINDOW)
            {
                _launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
            }

            else if (poke_data->folder_open_action == LAUNCHER_CHANGE_DIRECTORY)
            {
                // If multiple directories are passed, we assume that we should open them all
                if (directories->next == NULL)
                    navigator_change_directory(THUNARNAVIGATOR(browser), directories->data);
                else
                {
                    _launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
                }
            }
            else if (poke_data->folder_open_action == LAUNCHER_NO_ACTION)
            {
                // nothing to do
            }
            else
                g_warning("'folder_open_action' was not defined");

            g_list_free(directories);
        }

        // check if we have any files to process
        if (G_LIKELY(files != NULL))
        {
            // if all files are executable, we just run them here
            if (G_UNLIKELY(executable) && poke_data->application_to_use == NULL)
            {
                // try to execute all given files
                _launcher_execute_files(THUNAR_LAUNCHER(browser), files);
            }
            else
            {
                // try to open all files
                _launcher_open_files(THUNAR_LAUNCHER(browser), files, poke_data->application_to_use);
            }

            // cleanup
            g_list_free(files);
        }

        // free the poke data
        _launcher_poke_data_free(poke_data);
    }
    else
    {
        // we need to continue this until all files have been resolved
        // We will only poke one file at a time, in order to dont use all available CPU's
        // TODO: Check if that could cause slowness
        browser_poke_file(browser,
                                 poke_data->files_to_poke->data,
                                 (THUNAR_LAUNCHER(browser))->widget,
                                 _launcher_poke_files_finish,
                                 poke_data);
    }
}

static void _launcher_open_windows(ThunarLauncher *launcher, GList *directories)
{
    Application *application;
    GtkWidget         *dialog;
    GtkWidget         *window;
    GdkScreen         *screen;
    gchar             *label;
    GList             *lp;
    gint               response = GTK_RESPONSE_YES;
    gint               n;

    // ask the user if we would open more than one new window
    n = g_list_length(directories);
    if (G_UNLIKELY(n > 1))
    {
        // open a message dialog
        window = gtk_widget_get_toplevel(launcher->widget);
        dialog = gtk_message_dialog_new((GtkWindow *) window,
                                         GTK_DIALOG_DESTROY_WITH_PARENT
                                         | GTK_DIALOG_MODAL,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         _("Are you sure you want to open all folders?"));
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                ngettext("This will open %d separate file manager window.",
                          "This will open %d separate file manager windows.",
                          n), n);
        label = g_strdup_printf(ngettext("Open %d New Window", "Open %d New Windows", n), n);
        gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
        gtk_dialog_add_button(GTK_DIALOG(dialog), label, GTK_RESPONSE_YES);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
        response = gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        g_free(label);
    }

    // open n new windows if the user approved it
    if (G_LIKELY(response == GTK_RESPONSE_YES))
    {
        // query the application object
        application = application_get();

        // determine the screen on which to open the new windows
        screen = gtk_widget_get_screen(launcher->widget);

        // open all requested windows
        for (lp = directories; lp != NULL; lp = lp->next)
            application_open_window(application, lp->data, screen, NULL, TRUE);

        // release the application object
        g_object_unref(G_OBJECT(application));
    }
}

static void _launcher_open_file(ThunarLauncher *launcher,
                                      ThunarFile     *file,
                                      GAppInfo       *application_to_use)
{
    GList *files = NULL;

    files = g_list_append(files, file);
    _launcher_open_files(launcher, files, application_to_use);
    g_list_free(files);
}

static void _launcher_open_files(ThunarLauncher *launcher,
                                       GList          *files,
                                       GAppInfo       *application_to_use)
{
    GHashTable *applications;
    GAppInfo   *app_info;
    GList      *file_list;
    GList      *lp;

    /* allocate a hash table to associate applications to URIs. since GIO allocates
     * new GAppInfo objects every time, g_direct_hash does not work. we therefore use
     * a fake hash function to always hit the collision list of the hash table and
     * avoid storing multiple equal GAppInfos by means of g_app_info_equal(). */
    applications = g_hash_table_new_full(_launcher_g_app_info_hash,
                                         (GEqualFunc) g_app_info_equal,
                                         (GDestroyNotify) g_object_unref,
                                         (GDestroyNotify) e_list_free);

    for (lp = files; lp != NULL; lp = lp->next)
    {
        /* Because we created the hash_table with g_hash_table_new_full
         * g_object_unref on each hash_table key and value will be called by g_hash_table_destroy */
        if (application_to_use)
        {
            app_info = g_app_info_dup(application_to_use);

            // Foreign data needs to be set explicitly to the new app_info
            if (g_object_get_data(G_OBJECT(application_to_use), "skip-app-info-update") != NULL)
                g_object_set_data(G_OBJECT(app_info), "skip-app-info-update", GUINT_TO_POINTER(1));
        }
        else
        {
            // determine the default application for the MIME type
            app_info = th_file_get_default_handler(lp->data);
        }

        // check if we have an application here
        if (G_LIKELY(app_info != NULL))
        {
            // check if we have that application already
            file_list = g_hash_table_lookup(applications, app_info);
            if (G_LIKELY(file_list != NULL))
            {
                // take a copy of the list as the old one will be dropped by the insert
                file_list = e_list_copy(file_list);
            }

            // append our new URI to the list
            file_list = e_list_append_ref(file_list, th_file_get_file(lp->data));

            //(re)insert the URI list for the application
            g_hash_table_insert(applications, app_info, file_list);
        }
        else
        {
            // display a chooser dialog for the file and stop
            appchooser_dialog(launcher->widget, lp->data, TRUE);
            break;
        }
    }

    // run all collected applications
    g_hash_table_foreach(applications,(GHFunc) _launcher_open_paths, launcher);

    // drop the applications hash table
    g_hash_table_destroy(applications);
}

static guint _launcher_g_app_info_hash(gconstpointer app_info)
{
    (void) app_info;

    return 0;
}

static void _launcher_open_paths(GAppInfo       *app_info,
                                       GList          *path_list,
                                       ThunarLauncher *launcher)
{
    GdkAppLaunchContext *context;
    GdkScreen           *screen;
    GError              *error = NULL;
    GFile               *working_directory = NULL;
    gchar               *message;
    gchar               *name;
    guint                n;

    // determine the screen on which to launch the application
    screen = gtk_widget_get_screen(launcher->widget);

    // create launch context
    context = gdk_display_get_app_launch_context(gdk_screen_get_display(screen));
    gdk_app_launch_context_set_screen(context, screen);
    gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());
    gdk_app_launch_context_set_icon(context, g_app_info_get_icon(app_info));

    // determine the working directory
    if (launcher->current_directory != NULL)
        working_directory = th_file_get_file(launcher->current_directory);

    // try to execute the application with the given URIs
    if (!e_app_info_launch(app_info, working_directory, path_list, G_APP_LAUNCH_CONTEXT(context), &error))
    {
        // figure out the appropriate error message
        n = g_list_length(path_list);
        if (G_LIKELY(n == 1))
        {
            // we can give a precise error message here
            name = g_filename_display_name(g_file_get_basename(path_list->data));
            message = g_strdup_printf(_("Failed to open file \"%s\""), name);
            g_free(name);
        }
        else
        {
            // we can just tell that n files failed to open
            message = g_strdup_printf(ngettext("Failed to open %d file", "Failed to open %d files", n), n);
        }

        // display an error dialog to the user
        dialog_error(launcher->widget, error, "%s", message);
        g_error_free(error);
        g_free(message);
    }

    // destroy the launch context
    g_object_unref(context);
}

static void _launcher_execute_files(ThunarLauncher *launcher, GList *files)
{
    GError *error = NULL;
    GFile  *working_directory;
    GList  *lp;

    // execute all selected files
    for (lp = files; lp != NULL; lp = lp->next)
    {
        working_directory = th_file_get_file(launcher->current_directory);

        if (!th_file_execute(lp->data, working_directory, launcher->widget, NULL, NULL, &error))
        {
            // display an error message to the user
            dialog_error(launcher->widget,
                                      error,
                                      _("Failed to execute file \"%s\""),
                                      th_file_get_display_name(lp->data));
            g_error_free(error);
            break;
        }
    }
}

// Actions ====================================================================

static void _launcher_action_open(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    launcher_activate_selected_files(launcher, LAUNCHER_CHANGE_DIRECTORY, NULL);
}

static void _launcher_action_open_in_new_windows(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    launcher_open_selected_folders(launcher);
}

static void _launcher_action_open_with_other(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->n_files_to_process == 1)
        appchooser_dialog(launcher->widget, launcher->files_to_process->data, TRUE);
}

static void _launcher_action_create_folder(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    //DPRINT("_launcher_action_create_folder\n");

    if (th_file_is_trashed(launcher->current_directory))
        return;

    // ask the user to enter a name for the new folder
    gchar *name = dialog_file_create(launcher->widget,
                                     "inode/directory",
                                     _("New Folder"),
                                     _("Create New Folder"));

    if (G_UNLIKELY(name == NULL))
        return;

    GList path_list;

    // fake the path list
    if (IS_TREEVIEW(launcher->widget)
        && launcher->files_are_selected
        && launcher->single_directory_to_process)
        path_list.data = g_file_resolve_relative_path(
                    th_file_get_file(launcher->single_folder),
                    name);
    else
        path_list.data = g_file_resolve_relative_path(
                    th_file_get_file(launcher->current_directory),
                    name);

    path_list.next = path_list.prev = NULL;

    // launch the operation
    Application *application = application_get();
    application_mkdir(application, launcher->widget, &path_list, launcher->select_files_closure);
    g_object_unref(G_OBJECT(application));

    // release the path
    g_object_unref(path_list.data);

    // release the file name
    g_free(name);
}

static void _launcher_action_create_document(ThunarLauncher   *launcher,
                                                   GtkWidget        *menu_item)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (th_file_is_trashed(launcher->current_directory))
        return;

    ThunarFile        *template_file;
    template_file = g_object_get_qdata(G_OBJECT(menu_item), _launcher_file_quark);

    gchar             *name;

    if (template_file != NULL)
    {
        // generate a title for the create dialog
        gchar *title;
        title = g_strdup_printf(_("Create Document from template \"%s\""),
                                th_file_get_display_name(template_file));

        // ask the user to enter a name for the new document
        name = dialog_file_create(
                        launcher->widget,
                        th_file_get_content_type(THUNAR_FILE(template_file)),
                        th_file_get_display_name(template_file),
                        title);
        // cleanup
        g_free(title);
    }
    else
    {
        // ask the user to enter a name for the new empty file
        name = dialog_file_create(launcher->widget,
                                  "text/plain",
                                  _("New Empty File"),
                                  _("New Empty File..."));
    }

    if (G_UNLIKELY(name == NULL))
        return;

    if (G_LIKELY(launcher->parent_folder != NULL))
    {
        GList target_path_list;

        // fake the target path list
        if (IS_TREEVIEW(launcher->widget)
            && launcher->files_are_selected
            && launcher->single_directory_to_process)
        {
            target_path_list.data = g_file_get_child(
                            th_file_get_file(launcher->single_folder),
                            name);
        }
        else
        {
            target_path_list.data = g_file_get_child(
                            th_file_get_file(launcher->current_directory),
                            name);
        }

        target_path_list.next = NULL;
        target_path_list.prev = NULL;

        // launch the operation
        Application *application = application_get();
        application_creat(application,
                          launcher->widget,
                          &target_path_list,
                          template_file != NULL
                            ? th_file_get_file(template_file)
                            : NULL,
                          launcher->select_files_closure);

        g_object_unref(G_OBJECT(application));

        // release the target path
        g_object_unref(target_path_list.data);
    }

    // release the file name
    g_free(name);
}

static void _launcher_action_cut(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE || launcher->parent_folder == NULL)
        return;

    ClipboardManager *clipboard = clipman_get_for_display(
                                        gtk_widget_get_display(launcher->widget));

    clipman_cut_files(clipboard, launcher->files_to_process);

    g_object_unref(G_OBJECT(clipboard));
}

static void _launcher_action_copy(ThunarLauncher *launcher)
{
    ClipboardManager *clipboard;

    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE)
        return;

    clipboard = clipman_get_for_display(gtk_widget_get_display(launcher->widget));
    clipman_copy_files(clipboard, launcher->files_to_process);
    g_object_unref(G_OBJECT(clipboard));
}

static void _launcher_action_paste_into_folder(ThunarLauncher *launcher)
{
    ClipboardManager *clipboard;

    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (!launcher->single_directory_to_process)
        return;

    clipboard = clipman_get_for_display(gtk_widget_get_display(launcher->widget));

    clipman_paste_files(clipboard,
                        th_file_get_file(launcher->single_folder),
                        launcher->widget,
                        launcher->select_files_closure);

    g_object_unref(G_OBJECT(clipboard));
}

static void _launcher_action_paste(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    ClipboardManager *clipboard = clipman_get_for_display(
                        gtk_widget_get_display(launcher->widget));

    clipman_paste_files(clipboard,
                        th_file_get_file(launcher->current_directory),
                        launcher->widget,
                        launcher->select_files_closure);

    g_object_unref(G_OBJECT(clipboard));
}

void launcher_action_trash_delete(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (!e_vfs_is_uri_scheme_supported("trash"))
        return;

    GdkModifierType event_state;

    // when shift modifier is pressed, we delete(as well via context menu)
    if (gtk_get_current_event_state(&event_state)
        &&(event_state & GDK_SHIFT_MASK) != 0)
    {
        _launcher_action_delete(launcher);
        return;
    }

    _launcher_action_move_to_trash(launcher);
}

static void _launcher_action_move_to_trash(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->parent_folder == NULL
        || launcher->files_are_selected == FALSE)
        return;

    Application *application = application_get();

    application_unlink_files(application,
                             launcher->widget,
                             launcher->files_to_process,
                             FALSE);

    g_object_unref(G_OBJECT(application));
}

static void _launcher_action_delete(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->parent_folder == NULL
        || launcher->files_are_selected == FALSE)
        return;

    Application *application = application_get();

    application_unlink_files(application,
                             launcher->widget,
                             launcher->files_to_process,
                             TRUE);

    g_object_unref(G_OBJECT(application));
}

static void _launcher_action_empty_trash(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->single_directory_to_process == FALSE)
        return;

    if (!th_file_is_root(launcher->single_folder)
        || !th_file_is_trashed(launcher->single_folder))
        return;

    Application *application = application_get();

    application_empty_trash(application, launcher->widget, NULL);

    g_object_unref(G_OBJECT(application));
}

static void _launcher_action_restore(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE
        || !th_file_is_trashed(launcher->current_directory))
        return;

    // restore the selected files
    Application *application = application_get();

    application_restore_files(application,
                              launcher->widget,
                              launcher->files_to_process,
                              NULL);

    g_object_unref(G_OBJECT(application));
}

static void _launcher_action_duplicate(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->current_directory == NULL))
        return;

    if (launcher->files_are_selected == FALSE
        || th_file_is_trashed(launcher->current_directory))
        return;

    // determine the selected files for the view
    GList *files_to_process = th_filelist_to_thunar_g_file_list(
                                            launcher->files_to_process);

    if (!files_to_process)
        return;

    /* copy the selected files into the current directory, which effectively
     * creates duplicates of the files */
    Application *application = application_get();

    application_copy_into(application,
                          launcher->widget,
                          files_to_process,
                          th_file_get_file(launcher->current_directory),
                          launcher->select_files_closure);

    g_object_unref(G_OBJECT(application));

    e_list_free(files_to_process);
}

static void _launcher_action_make_link(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->current_directory == NULL))
        return;
    if (launcher->files_are_selected == FALSE
        || th_file_is_trashed(launcher->current_directory))
        return;

    GList *g_files = NULL;

    for (GList *lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    {
        g_files = g_list_append(g_files, th_file_get_file(lp->data));
    }

    /* link the selected files into the current directory, which effectively
     * creates new unique links for the files */
    Application *application = application_get();

    application_link_into(application,
                          launcher->widget,
                          g_files,
                          th_file_get_file(launcher->current_directory),
                          launcher->select_files_closure);

    g_object_unref(G_OBJECT(application));

    g_list_free(g_files);
}

static void _launcher_rename_finished(ExoJob *job, GtkWidget *widget)
{
    e_return_if_fail(IS_EXOJOB(job));

    // destroy the job
    g_signal_handlers_disconnect_matched(job,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL,
                                         widget);

    g_object_unref(job);
}

static void _launcher_rename_error(ExoJob *job, GError *error, GtkWidget *widget)
{
    e_return_if_fail(IS_EXOJOB(job));
    e_return_if_fail(error != NULL);

    GArray *param_values = simplejob_get_param_values(THUNAR_SIMPLE_JOB(job));
    ThunarFile *file = g_value_get_object(&g_array_index(param_values, GValue, 0));

    dialog_error(GTK_WIDGET(widget),
                 error,
                 _("Failed to rename \"%s\""),
                 th_file_get_display_name(file));

    g_object_unref(file);
}

void launcher_action_rename(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL
        || g_list_length(launcher->files_to_process) == 0)
        return;

    if (launcher->files_are_selected == FALSE
        || th_file_is_trashed(launcher->current_directory))
        return;

    // get the window
    GtkWidget *window = gtk_widget_get_toplevel(launcher->widget);

    // start renaming if we have exactly one selected file
    if (g_list_length(launcher->files_to_process) != 1)
        return;

    // run the rename dialog
    ThunarJob *job = dialog_file_rename(
                            GTK_WINDOW(window),
                            THUNAR_FILE(launcher->files_to_process->data));

    if (!job)
        return;

    g_signal_connect(job,
                     "error",
                     G_CALLBACK(_launcher_rename_error),
                     launcher->widget);

    g_signal_connect(job,
                     "finished",
                     G_CALLBACK(_launcher_rename_finished),
                     launcher->widget);
}

static void _launcher_action_terminal(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL
        || g_list_length(launcher->files_to_process) != 1
        || th_file_is_trashed(launcher->current_directory))
        return;

    ThunarFile *file = THUNAR_FILE(launcher->files_to_process->data);
    GFile *gfile = th_file_get_file(file);
    gchar *filepath = g_file_get_path(gfile);

    CStringAuto *cmd = cstr_new_size(128);
    cstr_fmt(cmd,
             "exo-open --working-directory \"%s\" --launch TerminalEmulator",
             filepath);

    g_spawn_command_line_async(c_str(cmd), NULL);

    g_free(filepath);
}

static void _launcher_action_extract(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL
        || g_list_length(launcher->files_to_process) != 1)
        return;

    ThunarFile *file = THUNAR_FILE(launcher->files_to_process->data);
    GFile *gfile = th_file_get_file(file);
    gchar *filepath = g_file_get_path(gfile);

    CStringAuto *cmd = cstr_new_size(128);
    const char *extractpath = "extract.sh";
    cstr_fmt(cmd,
             "xfce4-terminal -e 'bash -c \"%s %s; bash\"'",
             extractpath,
             filepath);

    //g_print("execute : %s\n", c_str(cmd));

    g_spawn_command_line_async(c_str(cmd), NULL);

    g_free(filepath);
}

void launcher_action_mount(ThunarLauncher *launcher)
{
    _launcher_poke(launcher, NULL, LAUNCHER_NO_ACTION);
}

static void _launcher_action_eject_finish(ThunarDevice *device, const GError *error,
                                          gpointer user_data)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(user_data);

    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    // check if there was an error
    if (error != NULL)
    {
        // display an error dialog to inform the user
        gchar *device_name = th_device_get_name(device);

        dialog_error(GTK_WIDGET(launcher->widget),
                     error,
                     _("Failed to eject \"%s\""),
                     device_name);

        g_free(device_name);
    }
    else
    {
        launcher->device_to_process = NULL;
    }

    g_object_unref(launcher);
}

void launcher_action_eject(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_LIKELY(launcher->device_to_process != NULL))
    {
        // prepare a mount operation
        GMountOperation *mount_operation;
        mount_operation = etk_mount_operation_new(GTK_WIDGET(launcher->widget));

        // eject
        th_device_eject(launcher->device_to_process,
                        mount_operation,
                        NULL,
                        _launcher_action_eject_finish,
                        g_object_ref(launcher));

        g_object_unref(mount_operation);
    }
}

void launcher_action_unmount(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_LIKELY(launcher->device_to_process != NULL))
    {
        // prepare a mount operation
        GMountOperation *mount_operation;
        mount_operation = etk_mount_operation_new(GTK_WIDGET(launcher->widget));

        // eject
        th_device_unmount(launcher->device_to_process,
                          mount_operation,
                          NULL,
                          _launcher_action_unmount_finish,
                          g_object_ref(launcher));

        // release the device
        g_object_unref(mount_operation);
    }
}

static void _launcher_action_unmount_finish(ThunarDevice *device,
                                            const GError *error,
                                            gpointer      user_data)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(user_data);

    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    // check if there was an error
    if (error != NULL)
    {
        // display an error dialog to inform the user
        gchar *device_name = th_device_get_name(device);

        dialog_error(GTK_WIDGET(launcher->widget),
                     error,
                     _("Failed to unmount \"%s\""),
                     device_name);

        g_free(device_name);
    }
    else
        launcher->device_to_process = NULL;

    g_object_unref(launcher);
}

static void _launcher_action_properties(ThunarLauncher *launcher)
{
    e_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    // popup the files dialog
    GtkWidget *toplevel = gtk_widget_get_toplevel(launcher->widget);

    if (G_LIKELY(toplevel != NULL))
    {
        GtkWidget *dialog = propsdlg_new(GTK_WINDOW(toplevel));

        // check if no files are currently selected
        if (launcher->files_to_process == NULL)
        {
            // if we don't have any files selected, we just popup the properties dialog for the current folder.
            propsdlg_set_file(PROPERTIESDIALOG(dialog),
                              launcher->current_directory);
        }
        else
        {
            // popup the properties dialog for all file(s)
            propsdlg_set_files(PROPERTIESDIALOG(dialog),
                               launcher->files_to_process);
        }

        gtk_widget_show(dialog);
    }
}


