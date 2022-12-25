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
#include <thunar-launcher.h>

#include <libext.h>
#include <thunar-application.h>
#include <thunar-browser.h>
#include <thunar-chooser-dialog.h>
#include <thunar-clipboard-manager.h>
#include <thunar-dialogs.h>
#include <thunar-gio-extensions.h>
#include <thunar-gobject-extensions.h>
#include <thunar-gtk-extensions.h>
#include <thunar-icon-factory.h>
#include <thunar-io-scan-directory.h>
#include <thunar-debug.h>
#include <thunar-properties-dialog.h>
#include <thunar-simple-job.h>
#include <thunar-device-monitor.h>
#include <thunar-tree-view.h>
#include <thunar-util.h>
#include <thunar-window.h>

#include <libxfce4ui/libxfce4ui.h>
#include <cstring.h>

#include <memory.h>
#include <string.h>

typedef struct _ThunarLauncherPokeData ThunarLauncherPokeData;

/* Property identifiers */
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

static void thunar_launcher_component_init(ThunarComponentIface *iface);
static void thunar_launcher_navigator_init(ThunarNavigatorIface *iface);
static void thunar_launcher_dispose(GObject *object);
static void thunar_launcher_finalize(GObject *object);

static void thunar_launcher_get_property(GObject *object,
                                         guint prop_id,
                                         GValue *value,
                                         GParamSpec *pspec);
static void thunar_launcher_set_property(GObject *object,
                                         guint prop_id,
                                         const GValue *value,
                                         GParamSpec *pspec);

static ThunarFile* thunar_launcher_get_current_directory(ThunarNavigator *navigator);
static void thunar_launcher_set_current_directory(ThunarNavigator *navigator,
                                                   ThunarFile *current_directory);

static void thunar_launcher_set_selected_files(ThunarComponent *component,
                                                GList *selected_files);

static void _thunar_launcher_execute_files(ThunarLauncher *launcher,
                                           GList *files);
static void _thunar_launcher_open_file(ThunarLauncher *launcher,
                                       ThunarFile *file,
                                       GAppInfo *application_to_use);
static void _thunar_launcher_open_files(ThunarLauncher *launcher,
                                        GList *files,
                                        GAppInfo *application_to_use);
static void _thunar_launcher_open_paths(GAppInfo *app_info,
                                        GList *file_list,
                                        ThunarLauncher *launcher);
static void _thunar_launcher_open_windows(ThunarLauncher *launcher,
                                          GList *directories);

static void thunar_launcher_menu_item_activated(ThunarLauncher *launcher,
                                                GtkWidget *menu_item);


// Poke files.
static ThunarLauncherPokeData* _thunar_launcher_poke_data_new(
                                    GList *files_to_poke,
                                    GAppInfo *application_to_use,
                                    ThunarLauncherFolderOpenAction folder_open_action);
static void _thunar_launcher_poke_data_free(ThunarLauncherPokeData *data);
static void _thunar_launcher_poke(ThunarLauncher *launcher,
                                  GAppInfo *application_to_use,
                                  ThunarLauncherFolderOpenAction folder_open_action);

// Actions
static void _thunar_launcher_action_open(ThunarLauncher *launcher);
static void thunar_launcher_action_open_in_new_windows(ThunarLauncher *launcher);
static void thunar_launcher_action_open_with_other(ThunarLauncher *launcher);

static void thunar_launcher_action_create_folder(ThunarLauncher *launcher);
static void thunar_launcher_action_create_document(ThunarLauncher *launcher,
                                                   GtkWidget *menu_item);
static GtkWidget* thunar_launcher_create_document_submenu_new(ThunarLauncher *launcher);

static void thunar_launcher_action_cut(ThunarLauncher *launcher);
static void thunar_launcher_action_copy(ThunarLauncher *launcher);
static void thunar_launcher_action_paste(ThunarLauncher *launcher);
static void thunar_launcher_action_paste_into_folder(ThunarLauncher *launcher);

static void thunar_launcher_action_trash_delete(ThunarLauncher *launcher);
static void thunar_launcher_action_key_trash_delete(ThunarLauncher *launcher);
static void thunar_launcher_action_move_to_trash(ThunarLauncher *launcher);
static void thunar_launcher_action_delete(ThunarLauncher *launcher);
static void thunar_launcher_action_empty_trash(ThunarLauncher *launcher);
static void thunar_launcher_action_restore(ThunarLauncher *launcher);

static void thunar_launcher_action_duplicate(ThunarLauncher *launcher);
static void thunar_launcher_action_make_link(ThunarLauncher *launcher);
static void thunar_launcher_action_key_rename(ThunarLauncher *launcher);

static void _thunar_launcher_action_terminal(ThunarLauncher *launcher);

static void thunar_launcher_action_properties(ThunarLauncher *launcher);

struct _ThunarLauncherClass
{
    GObjectClass __parent__;
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

    /* Parent widget which holds the instance of the launcher */
    GtkWidget       *widget;
};

static GQuark thunar_launcher_appinfo_quark;
static GQuark thunar_launcher_device_quark;
static GQuark thunar_launcher_file_quark;

struct _ThunarLauncherPokeData
{
    GList       *files_to_poke;
    GList       *files_poked;
    GAppInfo    *application_to_use;
    ThunarLauncherFolderOpenAction folder_open_action;
};

static GParamSpec* launcher_props[N_PROPERTIES] = { NULL, };

static XfceGtkActionEntry _launcher_actions[] =
{

    {THUNAR_LAUNCHER_ACTION_OPEN,
     "<Actions>/ThunarLauncher/open",
     "<Primary>O",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     "document-open",
     G_CALLBACK(_thunar_launcher_action_open)},

    {THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW,
     "<Actions>/ThunarLauncher/open-in-new-window",
     "<Primary><shift>O",
     XFCE_GTK_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_open_in_new_windows)},

    {THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER,
     "<Actions>/ThunarLauncher/open-with-other",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Open With Other _Application..."),
     N_("Choose another application with which to open the selected file"),
     NULL, G_CALLBACK(thunar_launcher_action_open_with_other)},


    {THUNAR_LAUNCHER_ACTION_EXECUTE,
     "<Actions>/ThunarLauncher/execute",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     "system-run",
     G_CALLBACK(_thunar_launcher_action_open)},


    {THUNAR_LAUNCHER_ACTION_CREATE_FOLDER,
     "<Actions>/ThunarStandardView/create-folder",
     "<Primary><shift>N",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Create _Folder..."),
     N_("Create an empty folder within the current folder"),
     "folder-new",
     G_CALLBACK(thunar_launcher_action_create_folder)},

    {THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT,
     "<Actions>/ThunarStandardView/create-document",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Create _Document"),
     N_("Create a new document from a template"),
     "document-new",
     G_CALLBACK(NULL)},


    {THUNAR_LAUNCHER_ACTION_CUT,
     "<Actions>/ThunarLauncher/cut",
     "<Primary>X",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Cu_t"),
     N_("Prepare the selected files to be moved with a Paste command"),
     "edit-cut",
     G_CALLBACK(thunar_launcher_action_cut)},

    {THUNAR_LAUNCHER_ACTION_COPY,
     "<Actions>/ThunarLauncher/copy",
     "<Primary>C",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Copy"),
     N_("Prepare the selected files to be copied with a Paste command"),
     "edit-copy",
     G_CALLBACK(thunar_launcher_action_copy)},

    {THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER,
     NULL,
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Paste Into Folder"),
     N_("Move or copy files previously selected by a Cut or Copy command into the selected folder"),
     "edit-paste",
     G_CALLBACK(thunar_launcher_action_paste_into_folder)},

    {THUNAR_LAUNCHER_ACTION_PASTE,
     "<Actions>/ThunarLauncher/paste",
     "<Primary>V",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Paste"),
     N_("Move or copy files previously selected by a Cut or Copy command"),
     "edit-paste",
     G_CALLBACK(thunar_launcher_action_paste)},


    {THUNAR_LAUNCHER_ACTION_TRASH_DELETE,
     "<Actions>/ThunarLauncher/trash-delete",
     "Delete",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_key_trash_delete)},

    {THUNAR_LAUNCHER_ACTION_TRASH_DELETE,
     "<Actions>/ThunarLauncher/trash-delete-2",
     "KP_Delete",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_key_trash_delete)},

    {THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH,
     "<Actions>/ThunarLauncher/move-to-trash",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("Mo_ve to Trash"),
     NULL,
     "user-trash",
     G_CALLBACK(thunar_launcher_action_trash_delete)},

    {THUNAR_LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Delete"),
     NULL,
     "edit-delete",
     G_CALLBACK(thunar_launcher_action_delete)},

    {THUNAR_LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete-2",
     "<Shift>Delete",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_delete)},

    {THUNAR_LAUNCHER_ACTION_DELETE,
     "<Actions>/ThunarLauncher/delete-3",
     "<Shift>KP_Delete",
     XFCE_GTK_IMAGE_MENU_ITEM,
     NULL,
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_delete)},

    {THUNAR_LAUNCHER_ACTION_EMPTY_TRASH,
     "<Actions>/ThunarWindow/empty-trash",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Empty Trash"),
     N_("Delete all files and folders in the Trash"),
     NULL,
     G_CALLBACK(thunar_launcher_action_empty_trash)},

    {THUNAR_LAUNCHER_ACTION_RESTORE,
     "<Actions>/ThunarLauncher/restore",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Restore"),
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_restore)},


    {THUNAR_LAUNCHER_ACTION_DUPLICATE,
     "<Actions>/ThunarStandardView/duplicate",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Du_plicate"),
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_duplicate)},

    {THUNAR_LAUNCHER_ACTION_MAKE_LINK,
     "<Actions>/ThunarStandardView/make-link",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("Ma_ke Link"),
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_make_link)},

    {THUNAR_LAUNCHER_ACTION_RENAME,
     "<Actions>/ThunarStandardView/rename",
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Rename..."),
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_rename)},

    {THUNAR_LAUNCHER_ACTION_RENAME,
     "<Actions>/ThunarStandardView/rename-2",
     "F2",
     XFCE_GTK_MENU_ITEM,
     N_("_Rename..."),
     NULL,
     NULL,
     G_CALLBACK(thunar_launcher_action_key_rename)},


    {THUNAR_LAUNCHER_ACTION_TERMINAL,
     "<Actions>/ThunarStandardView/terminal",
     "",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("MyTerminal..."),
     N_("Open in terminal"),
     "utilities-terminal",
     G_CALLBACK(_thunar_launcher_action_terminal)},


    {THUNAR_LAUNCHER_ACTION_MOUNT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Mount"),
     N_("Mount the selected device"),
     NULL,
     G_CALLBACK(_thunar_launcher_action_open)},

    {THUNAR_LAUNCHER_ACTION_UNMOUNT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Unmount"),
     N_("Unmount the selected device"),
     NULL,
     G_CALLBACK(thunar_launcher_action_unmount)},

    {THUNAR_LAUNCHER_ACTION_EJECT,
     NULL,
     "",
     XFCE_GTK_MENU_ITEM,
     N_("_Eject"),
     N_("Eject the selected device"),
     NULL,
     G_CALLBACK(thunar_launcher_action_eject)},


    {THUNAR_LAUNCHER_ACTION_PROPERTIES,
     "<Actions>/ThunarStandardView/properties",
     "<Alt>Return",
     XFCE_GTK_IMAGE_MENU_ITEM,
     N_("_Properties..."),
     N_("View the properties of the selected file"),
     "document-properties",
     G_CALLBACK(thunar_launcher_action_properties)},

};

#define get_action_entry(id) \
    xfce_gtk_get_action_entry_by_id(_launcher_actions, \
                                    G_N_ELEMENTS(_launcher_actions), \
                                    id)

G_DEFINE_TYPE_WITH_CODE(ThunarLauncher,
                        thunar_launcher,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_BROWSER,
                                              NULL)
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_NAVIGATOR,
                                              thunar_launcher_navigator_init)
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_COMPONENT,
                                              thunar_launcher_component_init))

static void thunar_launcher_class_init(ThunarLauncherClass *klass)
{
    GObjectClass *gobject_class;
    gpointer     g_iface;

    /* determine all used quarks */
    thunar_launcher_appinfo_quark = g_quark_from_static_string("thunar-launcher-appinfo");
    thunar_launcher_device_quark = g_quark_from_static_string("thunar-launcher-device");
    thunar_launcher_file_quark = g_quark_from_static_string("thunar-launcher-file");

    xfce_gtk_translate_action_entries(_launcher_actions, G_N_ELEMENTS(_launcher_actions));

    gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = thunar_launcher_dispose;
    gobject_class->finalize = thunar_launcher_finalize;
    gobject_class->get_property = thunar_launcher_get_property;
    gobject_class->set_property = thunar_launcher_set_property;

    /**
     * ThunarLauncher:widget:
     *
     * The #GtkWidget with which this launcher is associated.
     **/
    launcher_props[PROP_WIDGET] =
        g_param_spec_object("widget",
                            "widget",
                            "widget",
                            GTK_TYPE_WIDGET,
                            E_PARAM_WRITABLE);

    /**
     * ThunarLauncher:select-files-closure:
     *
     * The #GClosure which will be called if the selected file should be updated after a launcher operation
     **/
    launcher_props[PROP_SELECT_FILES_CLOSURE] =
        g_param_spec_pointer("select-files-closure",
                             "select-files-closure",
                             "select-files-closure",
                             G_PARAM_WRITABLE
                             | G_PARAM_CONSTRUCT_ONLY);

    /**
     * ThunarLauncher:select-device:
     *
     * The #ThunarDevice which currently is selected(or NULL if no #ThunarDevice is selected)
     **/
    launcher_props[PROP_SELECTED_DEVICE] =
        g_param_spec_pointer("selected-device",
                              "selected-device",
                              "selected-device",
                              G_PARAM_WRITABLE);

    /* Override ThunarNavigator's properties */
    g_iface = g_type_default_interface_peek(THUNAR_TYPE_NAVIGATOR);
    launcher_props[PROP_CURRENT_DIRECTORY] =
        g_param_spec_override("current-directory",
                               g_object_interface_find_property(g_iface, "current-directory"));

    /* Override ThunarComponent's properties */
    g_iface = g_type_default_interface_peek(THUNAR_TYPE_COMPONENT);
    launcher_props[PROP_SELECTED_FILES] =
        g_param_spec_override("selected-files",
                               g_object_interface_find_property(g_iface, "selected-files"));

    /* install properties */
    g_object_class_install_properties(gobject_class, N_PROPERTIES, launcher_props);
}

static void thunar_launcher_component_init(ThunarComponentIface *iface)
{
    iface->get_selected_files = (gpointer) e_noop_null;
    iface->set_selected_files = thunar_launcher_set_selected_files;
}

static void thunar_launcher_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = thunar_launcher_get_current_directory;
    iface->set_current_directory = thunar_launcher_set_current_directory;
}

static void thunar_launcher_init(ThunarLauncher *launcher)
{
    launcher->files_to_process = NULL;
    launcher->select_files_closure = NULL;
    launcher->device_to_process = NULL;
}

static void thunar_launcher_dispose(GObject *object)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

    /* reset our properties */
    thunar_navigator_set_current_directory(THUNAR_NAVIGATOR(launcher), NULL);
    thunar_launcher_set_widget(THUNAR_LAUNCHER(launcher), NULL);

    /* disconnect from the currently selected files */
    thunar_g_file_list_free(launcher->files_to_process);
    launcher->files_to_process = NULL;

    /* unref parent, if any */
    if (launcher->parent_folder != NULL)
        g_object_unref(launcher->parent_folder);

   (*G_OBJECT_CLASS(thunar_launcher_parent_class)->dispose)(object);
}

static void thunar_launcher_finalize(GObject *object)
{
    //ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

   (*G_OBJECT_CLASS(thunar_launcher_parent_class)->finalize)(object);
}

// Properties -----------------------------------------------------------------

static void thunar_launcher_get_property(GObject    *object,
                                         guint       prop_id,
                                         GValue     *value,
                                         GParamSpec *pspec)
{
    UNUSED(pspec);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, thunar_navigator_get_current_directory(THUNAR_NAVIGATOR(object)));
        break;

    case PROP_SELECTED_FILES:
        g_value_set_boxed(value, thunar_component_get_selected_files(THUNAR_COMPONENT(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void thunar_launcher_set_property(GObject      *object,
                                         guint         prop_id,
                                         const GValue *value,
                                         GParamSpec   *pspec)
{
    UNUSED(pspec);

    ThunarLauncher *launcher = THUNAR_LAUNCHER(object);

    switch(prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        thunar_navigator_set_current_directory(THUNAR_NAVIGATOR(object),
                                               g_value_get_object(value));
        break;

    case PROP_SELECTED_FILES:
        thunar_component_set_selected_files(THUNAR_COMPONENT(object),
                                            g_value_get_boxed(value));
        break;

    case PROP_WIDGET:
        thunar_launcher_set_widget(launcher, g_value_get_object(value));
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

// Directory ------------------------------------------------------------------

static ThunarFile* thunar_launcher_get_current_directory(
                                                ThunarNavigator *navigator)
{
    return THUNAR_LAUNCHER(navigator)->current_directory;
}

static void thunar_launcher_set_current_directory(ThunarNavigator *navigator,
                                                  ThunarFile *current_directory)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(navigator);

    /* disconnect from the previous directory */
    if (G_LIKELY(launcher->current_directory != NULL))
        g_object_unref(G_OBJECT(launcher->current_directory));

    /* activate the new directory */
    launcher->current_directory = current_directory;

    /* connect to the new directory */
    if (G_LIKELY(current_directory != NULL))
    {
        g_object_ref(G_OBJECT(current_directory));

        /* update files_to_process if not initialized yet */
        if (launcher->files_to_process == NULL)
            thunar_launcher_set_selected_files(THUNAR_COMPONENT(navigator), NULL);
    }

    /* notify listeners */
    g_object_notify_by_pspec(G_OBJECT(launcher), launcher_props[PROP_CURRENT_DIRECTORY]);
}

static void thunar_launcher_set_selected_files(ThunarComponent *component,
                                                GList           *selected_files)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(component);
    GList          *lp;

    /* That happens at startup for some reason */
    if (launcher->current_directory == NULL)
        return;

    /* disconnect from the previous files to process */
    if (launcher->files_to_process != NULL)
        thunar_g_file_list_free(launcher->files_to_process);

    launcher->files_to_process = NULL;

    /* notify listeners */
    g_object_notify_by_pspec(G_OBJECT(launcher), launcher_props[PROP_SELECTED_FILES]);

    /* unref previous parent, if any */
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

    /* if nothing is selected, the current directory is the folder to use for all menus */
    if (launcher->files_are_selected)
        launcher->files_to_process = thunar_g_file_list_copy(selected_files);
    else
        launcher->files_to_process = g_list_append(launcher->files_to_process,
                                                   launcher->current_directory);

    /* determine the number of files/directories/executables */
    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next, ++launcher->n_files_to_process)
    {
        /* Keep a reference on all selected files */
        g_object_ref(lp->data);

        if (thunar_file_is_directory(lp->data)
            || thunar_file_is_shortcut(lp->data)
            || thunar_file_is_mountable(lp->data))
        {
            ++launcher->n_directories_to_process;
        }
        else
        {
            if (thunar_file_is_executable(lp->data))
                ++launcher->n_executables_to_process;

            ++launcher->n_regulars_to_process;
        }

        if (!thunar_file_can_be_trashed(lp->data))
            launcher->files_to_process_trashable = FALSE;
    }

    launcher->single_directory_to_process =(launcher->n_directories_to_process == 1 && launcher->n_files_to_process == 1);

    if (launcher->single_directory_to_process)
    {
        /* grab the folder of the first selected item */
        launcher->single_folder = THUNAR_FILE(launcher->files_to_process->data);
    }

    if (launcher->files_to_process != NULL)
    {
        /* just grab the folder of the first selected item */
        launcher->parent_folder = thunar_file_get_parent(THUNAR_FILE(launcher->files_to_process->data), NULL);
    }

#ifdef DEBUG_SELECTION
    static gint count;
    DPRINT("%d: thunar_launcher_set_selected_files\n", count++);
    //DPRINT("%d dirs selected\n", launcher->n_directories_to_process);
    //DPRINT("%d files selected\n", launcher->n_regulars_to_process);

    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    {
        ThunarFile *file =(ThunarFile*) lp->data;

        DPRINT("%s\n", thunar_file_get_basename(file));
    }
#endif

}

// Widget ---------------------------------------------------------------------

static void _thunar_launcher_widget_destroyed(ThunarLauncher *launcher,
                                              GtkWidget      *widget)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));
    thunar_return_if_fail(launcher->widget == widget);
    thunar_return_if_fail(GTK_IS_WIDGET(widget));

    /* just reset the widget property for the launcher */
    thunar_launcher_set_widget(launcher, NULL);
}

GtkWidget* thunar_launcher_get_widget(ThunarLauncher *launcher)
{
    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);
    return launcher->widget;
}

void thunar_launcher_set_widget(ThunarLauncher *launcher,
                                GtkWidget      *widget)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));
    thunar_return_if_fail(widget == NULL || GTK_IS_WIDGET(widget));

    /* disconnect from the previous widget */
    if (G_UNLIKELY(launcher->widget != NULL))
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(launcher->widget),
                                             _thunar_launcher_widget_destroyed, launcher);
        g_object_unref(G_OBJECT(launcher->widget));
    }

    launcher->widget = widget;

    /* connect to the new widget */
    if (G_LIKELY(widget != NULL))
    {
        g_object_ref(G_OBJECT(widget));
        g_signal_connect_swapped(G_OBJECT(widget), "destroy",
                                 G_CALLBACK(_thunar_launcher_widget_destroyed), launcher);
    }

    /* notify listeners */
    g_object_notify_by_pspec(G_OBJECT(launcher), launcher_props[PROP_WIDGET]);
}



/******************************************************************************
 *
 *
 *
 */
static void thunar_launcher_menu_item_activated(ThunarLauncher *launcher,
                                                GtkWidget      *menu_item)
{
    GAppInfo *app_info;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    /* if we have a mime handler associated with the menu_item, we pass it to the launcher(g_object_get_qdata will return NULL otherwise)*/
    app_info = g_object_get_qdata(G_OBJECT(menu_item), thunar_launcher_appinfo_quark);
    thunar_launcher_activate_selected_files(launcher, THUNAR_LAUNCHER_CHANGE_DIRECTORY, app_info);
}

/**
 * thunar_launcher_activate_selected_files:
 * @launcher : a #ThunarLauncher instance
 * @action   : the #ThunarLauncherFolderOpenAction to use,
 *  if there are folders among the selected files
 * @app_info : a #GAppInfo instance
 *
 * Will try to open all selected files with the provided #GAppInfo
 **/
void thunar_launcher_activate_selected_files(ThunarLauncher  *launcher,
                                             ThunarLauncherFolderOpenAction action,
                                             GAppInfo        *app_info)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    _thunar_launcher_poke(launcher, app_info, action);
}

static void _thunar_launcher_execute_files(ThunarLauncher *launcher,
                                          GList          *files)
{
    GError *error = NULL;
    GFile  *working_directory;
    GList  *lp;

    /* execute all selected files */
    for (lp = files; lp != NULL; lp = lp->next)
    {
        working_directory = thunar_file_get_file(launcher->current_directory);

        if (!thunar_file_execute(lp->data, working_directory, launcher->widget, NULL, NULL, &error))
        {
            /* display an error message to the user */
            thunar_dialogs_show_error(launcher->widget,
                                      error,
                                      _("Failed to execute file \"%s\""),
                                      thunar_file_get_display_name(lp->data));
            g_error_free(error);
            break;
        }
    }
}

static guint thunar_launcher_g_app_info_hash(gconstpointer app_info)
{
    UNUSED(app_info);

    return 0;
}

static void _thunar_launcher_open_file(ThunarLauncher *launcher,
                                      ThunarFile     *file,
                                      GAppInfo       *application_to_use)
{
    GList *files = NULL;

    files = g_list_append(files, file);
    _thunar_launcher_open_files(launcher, files, application_to_use);
    g_list_free(files);
}

static void _thunar_launcher_open_files(ThunarLauncher *launcher,
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
    applications = g_hash_table_new_full(thunar_launcher_g_app_info_hash,
                                         (GEqualFunc) g_app_info_equal,
                                         (GDestroyNotify) g_object_unref,
                                         (GDestroyNotify) thunar_g_file_list_free);

    for (lp = files; lp != NULL; lp = lp->next)
    {
        /* Because we created the hash_table with g_hash_table_new_full
         * g_object_unref on each hash_table key and value will be called by g_hash_table_destroy */
        if (application_to_use)
        {
            app_info = g_app_info_dup(application_to_use);

            /* Foreign data needs to be set explicitly to the new app_info */
            if (g_object_get_data(G_OBJECT(application_to_use), "skip-app-info-update") != NULL)
                g_object_set_data(G_OBJECT(app_info), "skip-app-info-update", GUINT_TO_POINTER(1));
        }
        else
        {
            /* determine the default application for the MIME type */
            app_info = thunar_file_get_default_handler(lp->data);
        }

        /* check if we have an application here */
        if (G_LIKELY(app_info != NULL))
        {
            /* check if we have that application already */
            file_list = g_hash_table_lookup(applications, app_info);
            if (G_LIKELY(file_list != NULL))
            {
                /* take a copy of the list as the old one will be dropped by the insert */
                file_list = thunar_g_file_list_copy(file_list);
            }

            /* append our new URI to the list */
            file_list = thunar_g_file_list_append(file_list, thunar_file_get_file(lp->data));

            /*(re)insert the URI list for the application */
            g_hash_table_insert(applications, app_info, file_list);
        }
        else
        {
            /* display a chooser dialog for the file and stop */
            thunar_show_chooser_dialog(launcher->widget, lp->data, TRUE);
            break;
        }
    }

    /* run all collected applications */
    g_hash_table_foreach(applications,(GHFunc) _thunar_launcher_open_paths, launcher);

    /* drop the applications hash table */
    g_hash_table_destroy(applications);
}

static void _thunar_launcher_open_paths(GAppInfo       *app_info,
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

    /* determine the screen on which to launch the application */
    screen = gtk_widget_get_screen(launcher->widget);

    /* create launch context */
    context = gdk_display_get_app_launch_context(gdk_screen_get_display(screen));
    gdk_app_launch_context_set_screen(context, screen);
    gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());
    gdk_app_launch_context_set_icon(context, g_app_info_get_icon(app_info));

    /* determine the working directory */
    if (launcher->current_directory != NULL)
        working_directory = thunar_file_get_file(launcher->current_directory);

    /* try to execute the application with the given URIs */
    if (!thunar_g_app_info_launch(app_info, working_directory, path_list, G_APP_LAUNCH_CONTEXT(context), &error))
    {
        /* figure out the appropriate error message */
        n = g_list_length(path_list);
        if (G_LIKELY(n == 1))
        {
            /* we can give a precise error message here */
            name = g_filename_display_name(g_file_get_basename(path_list->data));
            message = g_strdup_printf(_("Failed to open file \"%s\""), name);
            g_free(name);
        }
        else
        {
            /* we can just tell that n files failed to open */
            message = g_strdup_printf(ngettext("Failed to open %d file", "Failed to open %d files", n), n);
        }

        /* display an error dialog to the user */
        thunar_dialogs_show_error(launcher->widget, error, "%s", message);
        g_error_free(error);
        g_free(message);
    }

    /* destroy the launch context */
    g_object_unref(context);
}

static void _thunar_launcher_open_windows(ThunarLauncher *launcher,
                                         GList          *directories)
{
    ThunarApplication *application;
    GtkWidget         *dialog;
    GtkWidget         *window;
    GdkScreen         *screen;
    gchar             *label;
    GList             *lp;
    gint               response = GTK_RESPONSE_YES;
    gint               n;

    /* ask the user if we would open more than one new window */
    n = g_list_length(directories);
    if (G_UNLIKELY(n > 1))
    {
        /* open a message dialog */
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

    /* open n new windows if the user approved it */
    if (G_LIKELY(response == GTK_RESPONSE_YES))
    {
        /* query the application object */
        application = thunar_application_get();

        /* determine the screen on which to open the new windows */
        screen = gtk_widget_get_screen(launcher->widget);

        /* open all requested windows */
        for (lp = directories; lp != NULL; lp = lp->next)
            thunar_application_open_window(application, lp->data, screen, NULL, TRUE);

        /* release the application object */
        g_object_unref(G_OBJECT(application));
    }
}

// Poke files -----------------------------------------------------------------

static ThunarLauncherPokeData* _thunar_launcher_poke_data_new(
                                    GList     *files_to_poke,
                                    GAppInfo  *application_to_use,
                                    ThunarLauncherFolderOpenAction folder_open_action)
{
    ThunarLauncherPokeData *data = g_slice_new0(ThunarLauncherPokeData);

    data->files_to_poke = thunar_g_file_list_copy(files_to_poke);
    data->files_poked = NULL;
    data->application_to_use = application_to_use;

    /* keep a reference on the appdata */
    if (application_to_use != NULL)
        g_object_ref(application_to_use);

    data->folder_open_action = folder_open_action;

    return data;
}

static void _thunar_launcher_poke_data_free(ThunarLauncherPokeData *data)
{
    thunar_return_if_fail(data != NULL);

    thunar_g_file_list_free(data->files_to_poke);
    thunar_g_file_list_free(data->files_poked);

    if (data->application_to_use != NULL)
        g_object_unref(data->application_to_use);

    g_slice_free(ThunarLauncherPokeData, data);
}

static void thunar_launcher_poke_device_finish(ThunarBrowser *browser,
                                               ThunarDevice  *volume,
                                               ThunarFile    *mount_point,
                                               GError        *error,
                                               gpointer       user_data,
                                               gboolean       cancelled)
{
    ThunarLauncherPokeData *poke_data = user_data;
    gchar                  *device_name;

    if (error != NULL)
    {
        device_name = thunar_device_get_name(volume);
        thunar_dialogs_show_error(GTK_WIDGET(THUNAR_LAUNCHER(browser)->widget), error, _("Failed to mount \"%s\""), device_name);
        g_free(device_name);
    }

    if (cancelled == TRUE || error != NULL || mount_point == NULL)
    {
        _thunar_launcher_poke_data_free(poke_data);
        return;
    }

    if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW)
    {
        GList *directories = NULL;
        directories = g_list_append(directories, mount_point);
        _thunar_launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
        g_list_free(directories);
    }
    else if (poke_data->folder_open_action == THUNAR_LAUNCHER_CHANGE_DIRECTORY)
    {
        thunar_navigator_change_directory(THUNAR_NAVIGATOR(browser), mount_point);
    }

    _thunar_launcher_poke_data_free(poke_data);
}

static void thunar_launcher_poke_files_finish(ThunarBrowser *browser,
                                              ThunarFile    *file,
                                              ThunarFile    *target_file,
                                              GError        *error,
                                              gpointer       user_data)
{
    ThunarLauncherPokeData *poke_data = user_data;
    gboolean                executable = TRUE;
    GList                  *directories = NULL;
    GList                  *files = NULL;
    GList                  *lp;

    thunar_return_if_fail(THUNAR_IS_BROWSER(browser));
    thunar_return_if_fail(THUNAR_IS_FILE(file));
    thunar_return_if_fail(poke_data != NULL);
    thunar_return_if_fail(poke_data->files_to_poke != NULL);

    /* check if poking succeeded */
    if (error == NULL)
    {
        /* add the resolved file to the list of file to be opened/executed later */
        poke_data->files_poked = g_list_prepend(poke_data->files_poked,g_object_ref(target_file));
    }

    /* release and remove the just poked file from the list */
    g_object_unref(poke_data->files_to_poke->data);
    poke_data->files_to_poke = g_list_delete_link(poke_data->files_to_poke,
                                                  poke_data->files_to_poke);

    if (poke_data->files_to_poke == NULL)
    {
        /* separate files and directories in the selected files list */
        for (lp = poke_data->files_poked; lp != NULL; lp = lp->next)
        {
            if (thunar_file_is_directory(lp->data))
            {
                /* add to our directory list */
                directories = g_list_prepend(directories, lp->data);
            }
            else
            {
                /* add to our file list */
                files = g_list_prepend(files, lp->data);

                /* check if the file is executable */
                executable = (executable && thunar_file_is_executable(lp->data));
            }
        }

        /* check if we have any directories to process */
        if (G_LIKELY(directories != NULL))
        {
            if (poke_data->application_to_use != NULL)
            {
                /* open them separately, using some specific application */
                for (lp = directories; lp != NULL; lp = lp->next)
                    _thunar_launcher_open_file(THUNAR_LAUNCHER(browser),
                                              lp->data, poke_data->application_to_use);
            }

            else if (poke_data->folder_open_action == THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW)
            {
                _thunar_launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
            }

            else if (poke_data->folder_open_action == THUNAR_LAUNCHER_CHANGE_DIRECTORY)
            {
                /* If multiple directories are passed, we assume that we should open them all */
                if (directories->next == NULL)
                    thunar_navigator_change_directory(THUNAR_NAVIGATOR(browser), directories->data);
                else
                {
                    _thunar_launcher_open_windows(THUNAR_LAUNCHER(browser), directories);
                }
            }
            else if (poke_data->folder_open_action == THUNAR_LAUNCHER_NO_ACTION)
            {
                // nothing to do
            }
            else
                g_warning("'folder_open_action' was not defined");

            g_list_free(directories);
        }

        /* check if we have any files to process */
        if (G_LIKELY(files != NULL))
        {
            /* if all files are executable, we just run them here */
            if (G_UNLIKELY(executable) && poke_data->application_to_use == NULL)
            {
                /* try to execute all given files */
                _thunar_launcher_execute_files(THUNAR_LAUNCHER(browser), files);
            }
            else
            {
                /* try to open all files */
                _thunar_launcher_open_files(THUNAR_LAUNCHER(browser), files, poke_data->application_to_use);
            }

            /* cleanup */
            g_list_free(files);
        }

        /* free the poke data */
        _thunar_launcher_poke_data_free(poke_data);
    }
    else
    {
        /* we need to continue this until all files have been resolved */
        // We will only poke one file at a time, in order to dont use all available CPU's
        // TODO: Check if that could cause slowness
        thunar_browser_poke_file(browser,
                                 poke_data->files_to_poke->data,
                                 (THUNAR_LAUNCHER(browser))->widget,
                                 thunar_launcher_poke_files_finish,
                                 poke_data);
    }
}

static void _thunar_launcher_poke(ThunarLauncher *launcher,
                                 GAppInfo       *application_to_use,
                                 ThunarLauncherFolderOpenAction folder_open_action)
{

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL)
    {
        g_warning("No files to process. thunar_launcher_poke aborted.");
        return;
    }

    ThunarLauncherPokeData *poke_data = _thunar_launcher_poke_data_new(
                                                        launcher->files_to_process,
                                                        application_to_use,
                                                        folder_open_action);

    if (launcher->device_to_process != NULL)
    {
        thunar_browser_poke_device(THUNAR_BROWSER(launcher),
                                   launcher->device_to_process,
                                   launcher->widget,
                                   thunar_launcher_poke_device_finish,
                                   poke_data);
    }
    else
    {
        // We will only poke one file at a time, in order to dont use all available CPU's
        // TODO: Check if that could cause slowness
        thunar_browser_poke_file(THUNAR_BROWSER(launcher),
                                 poke_data->files_to_poke->data,
                                 launcher->widget,
                                 thunar_launcher_poke_files_finish,
                                 poke_data);
    }
}


/**
 * thunar_launcher_open_selected_folders:
 * @launcher : a #ThunarLauncher instance
 * @open_in_tabs : TRUE to open each folder in a new tab, FALSE to open each folder in a new window
 *
 * Will open each selected folder in a new tab/window
 **/
void thunar_launcher_open_selected_folders(ThunarLauncher *launcher)
{
    GList *lp;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
        thunar_return_if_fail(thunar_file_is_directory(THUNAR_FILE(lp->data)));

    _thunar_launcher_poke(launcher, NULL, THUNAR_LAUNCHER_OPEN_AS_NEW_WINDOW);
}





















/**
 * thunar_launcher_append_accelerators:
 * @launcher    : a #ThunarLauncher.
 * @accel_group : a #GtkAccelGroup to be used used for new menu items
 *
 * Connects all accelerators and corresponding default keys of this widget to the global accelerator list
 **/
void thunar_launcher_append_accelerators(ThunarLauncher *launcher,
                                         GtkAccelGroup  *accel_group)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    xfce_gtk_accel_map_add_entries(_launcher_actions,
                                   G_N_ELEMENTS(_launcher_actions));

    xfce_gtk_accel_group_connect_action_entries(accel_group,
                                                _launcher_actions,
                                                G_N_ELEMENTS(_launcher_actions),
                                                launcher);
}

static gboolean thunar_launcher_show_trash(ThunarLauncher *launcher)
{
    if (launcher->parent_folder == NULL)
        return FALSE;

    /* If the folder is read only, always show trash insensitive */
    /* If we are outside waste basket, the selection is trashable
     * and we support trash, show trash */

    return !thunar_file_is_writable(launcher->parent_folder)
            || (!thunar_file_is_trashed(launcher->parent_folder)
            && launcher->files_to_process_trashable
            && thunar_g_vfs_is_uri_scheme_supported("trash"));
}

GtkWidget* thunar_launcher_append_menu_item(ThunarLauncher  *launcher,
                                            GtkMenuShell    *menu,
                                            ThunarLauncherAction action,
                                            gboolean        force)
{
    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    const XfceGtkActionEntry *action_entry = get_action_entry(action);

    thunar_return_val_if_fail(action_entry != NULL, NULL);

    GtkWidget                *item = NULL;
    GtkWidget                *submenu;
    GtkWidget                *focused_widget;
    gchar                    *label_text;
    gchar                    *tooltip_text;
    gboolean                  show_item;
    ThunarClipboardManager   *clipboard;
    ThunarFile               *parent;
    gint                      n;


    /* This may occur when the thunar-window is build */
    if (G_UNLIKELY(launcher->files_to_process == NULL) && launcher->device_to_process == NULL)
        return NULL;

    gboolean show_delete_item = false;

    switch (action)
    {

    case THUNAR_LAUNCHER_ACTION_OPEN:
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

    case THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW:
        n = launcher->n_files_to_process > 0 ? launcher->n_files_to_process : 1;
        label_text = g_strdup_printf(ngettext("Open in New _Window", "Open in %d New _Windows", n),
                                     n);
        tooltip_text = g_strdup_printf(ngettext("Open the selected directory in new window",
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

    case THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER:
        return xfce_gtk_menu_item_new(action_entry->menu_item_label_text,
                                      action_entry->menu_item_tooltip_text,
                                      action_entry->accel_path,
                                      action_entry->callback,
                                      G_OBJECT(launcher),
                                      menu);

    case THUNAR_LAUNCHER_ACTION_EXECUTE:
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

    case THUNAR_LAUNCHER_ACTION_CREATE_FOLDER:
        if (THUNAR_IS_TREE_VIEW(launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
            parent = launcher->single_folder;
        else
            parent = launcher->current_directory;
        if (thunar_file_is_trashed(parent))
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, thunar_file_is_writable(parent));
        return item;

    case THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT:
        if (THUNAR_IS_TREE_VIEW(launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
            parent = launcher->single_folder;
        else
            parent = launcher->current_directory;
        if (thunar_file_is_trashed(parent))
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
        submenu = thunar_launcher_create_document_submenu_new(launcher);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);
        gtk_widget_set_sensitive(item, thunar_file_is_writable(parent));
        return item;

    case THUNAR_LAUNCHER_ACTION_CUT:
        focused_widget = thunar_gtk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Cut the selection"),
                       action_entry->accel_path, G_CALLBACK(gtk_editable_cut_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, thunar_gtk_editable_can_cut(GTK_EDITABLE(focused_widget)));
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
            gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable(launcher->parent_folder));
        }
        return item;

    case THUNAR_LAUNCHER_ACTION_COPY:
        focused_widget = thunar_gtk_get_focused_widget();
        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Copy the selection"),
                       action_entry->accel_path,G_CALLBACK(gtk_editable_copy_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, thunar_gtk_editable_can_copy(GTK_EDITABLE(focused_widget)));
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

    case THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER:
        if (!launcher->single_directory_to_process)
            return NULL;
        clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, thunar_clipboard_manager_get_can_paste(clipboard) && thunar_file_is_writable(launcher->single_folder));
        g_object_unref(clipboard);
        return item;

    case THUNAR_LAUNCHER_ACTION_PASTE:

        focused_widget = thunar_gtk_get_focused_widget();

        if (focused_widget && GTK_IS_EDITABLE(focused_widget))
        {
            item = xfce_gtk_image_menu_item_new_from_icon_name(
                       action_entry->menu_item_label_text,
                       N_("Paste the clipboard"),
                       action_entry->accel_path,G_CALLBACK(gtk_editable_paste_clipboard),
                       G_OBJECT(focused_widget), action_entry->menu_item_icon_name, menu);
            gtk_widget_set_sensitive(item, thunar_gtk_editable_can_paste(GTK_EDITABLE(focused_widget)));
        }
        else
        {
            if (launcher->single_directory_to_process && launcher->files_are_selected)
                return thunar_launcher_append_menu_item(
                                                    launcher,
                                                    menu,
                                                    THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER,
                                                    force);
            clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
            item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                            G_OBJECT(launcher),
                                                            GTK_MENU_SHELL(menu));
            gtk_widget_set_sensitive(item, thunar_clipboard_manager_get_can_paste(clipboard) && thunar_file_is_writable(launcher->current_directory));
            g_object_unref(clipboard);
        }
        return item;

    case THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH:
        if (!thunar_launcher_show_trash(launcher))
            return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
            return NULL;

        tooltip_text = ngettext("Move the selected file to the Trash",
                                 "Move the selected files to the Trash", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable(launcher->parent_folder));
        return item;


    case THUNAR_LAUNCHER_ACTION_DELETE:
        if (thunar_launcher_show_trash(launcher) && !show_delete_item)
            return NULL;

        show_item = launcher->files_are_selected;
        if (!show_item && !force)
            return NULL;

        tooltip_text = ngettext("Permanently delete the selected file",
                                 "Permanently delete the selected files", launcher->n_files_to_process);
        item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable(launcher->parent_folder));
        return item;

    case THUNAR_LAUNCHER_ACTION_EMPTY_TRASH:
        if (launcher->single_directory_to_process == TRUE)
        {
            if (thunar_file_is_root(launcher->single_folder) && thunar_file_is_trashed(launcher->single_folder))
            {
                item = xfce_gtk_image_menu_item_new_from_icon_name(action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text, action_entry->accel_path,
                        action_entry->callback, G_OBJECT(launcher), action_entry->menu_item_icon_name, menu);
                gtk_widget_set_sensitive(item, thunar_file_get_item_count(launcher->single_folder) > 0);
                return item;
            }
        }
        return NULL;

    case THUNAR_LAUNCHER_ACTION_RESTORE:
        if (launcher->files_are_selected && thunar_file_is_trashed(launcher->current_directory))
        {
            tooltip_text = ngettext("Restore the selected file to its original location",
                                     "Restore the selected files to its original location", launcher->n_files_to_process);
            item = xfce_gtk_menu_item_new(action_entry->menu_item_label_text, tooltip_text, action_entry->accel_path,
                                           action_entry->callback, G_OBJECT(launcher), menu);
            gtk_widget_set_sensitive(item, thunar_file_is_writable(launcher->current_directory));
            return item;
        }
        return NULL;

    case THUNAR_LAUNCHER_ACTION_DUPLICATE:
        show_item = thunar_file_is_writable(launcher->current_directory) &&
                    launcher->files_are_selected &&
                    thunar_file_is_trashed(launcher->current_directory) == FALSE;
        if (!show_item && !force)
            return NULL;
        item = xfce_gtk_menu_item_new(action_entry->menu_item_label_text, action_entry->menu_item_tooltip_text,
                                       action_entry->accel_path, action_entry->callback, G_OBJECT(launcher), menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable(launcher->parent_folder));
        return item;

    case THUNAR_LAUNCHER_ACTION_MAKE_LINK:
        show_item = thunar_file_is_writable(launcher->current_directory)
                &&
                    launcher->files_are_selected
                &&
                    thunar_file_is_trashed(launcher->current_directory) == FALSE;
        if (!show_item && !force)
            return NULL;

        label_text = ngettext("Ma_ke Link", "Ma_ke Links", launcher->n_files_to_process);
        tooltip_text = ngettext("Create a symbolic link for the selected file",
                                 "Create a symbolic link for each selected file", launcher->n_files_to_process);
        item = xfce_gtk_menu_item_new(label_text, tooltip_text, action_entry->accel_path, action_entry->callback,
                                       G_OBJECT(launcher), menu);
        gtk_widget_set_sensitive(item, show_item && launcher->parent_folder != NULL && thunar_file_is_writable(launcher->parent_folder));
        return item;

    case THUNAR_LAUNCHER_ACTION_RENAME:
        show_item = thunar_file_is_writable(launcher->current_directory)
                    && launcher->files_are_selected
                    && thunar_file_is_trashed(launcher->current_directory) == FALSE;
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
                                 && thunar_file_is_writable(launcher->parent_folder));
        return item;

    case THUNAR_LAUNCHER_ACTION_TERMINAL:
        if (!launcher->single_directory_to_process)
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        return item;

    case THUNAR_LAUNCHER_ACTION_MOUNT:
        if (launcher->device_to_process == NULL
            || thunar_device_is_mounted(launcher->device_to_process) == TRUE)
            return NULL;
        return xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));

    case THUNAR_LAUNCHER_ACTION_UNMOUNT:
        if (launcher->device_to_process == NULL
            || thunar_device_is_mounted(launcher->device_to_process) == FALSE)
            return NULL;
        return xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));

    case THUNAR_LAUNCHER_ACTION_EJECT:
        if (launcher->device_to_process == NULL
            || thunar_device_get_kind(launcher->device_to_process) != THUNAR_DEVICE_KIND_VOLUME)
            return NULL;
        item = xfce_gtk_menu_item_new_from_action_entry(action_entry,
                                                        G_OBJECT(launcher),
                                                        GTK_MENU_SHELL(menu));
        gtk_widget_set_sensitive(item, thunar_device_can_eject(launcher->device_to_process));
        return item;

    default:
        return xfce_gtk_menu_item_new_from_action_entry(action_entry, G_OBJECT(launcher), GTK_MENU_SHELL(menu));
    }

    return NULL;
}




// Actions ====================================================================

static void _thunar_launcher_action_open(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    thunar_launcher_activate_selected_files(launcher, THUNAR_LAUNCHER_CHANGE_DIRECTORY, NULL);
}

static void thunar_launcher_action_open_in_new_windows(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return;

    thunar_launcher_open_selected_folders(launcher);
}

static void thunar_launcher_action_open_with_other(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->n_files_to_process == 1)
        thunar_show_chooser_dialog(launcher->widget, launcher->files_to_process->data, TRUE);
}

static void thunar_launcher_action_properties(ThunarLauncher *launcher)
{
    GtkWidget *toplevel;
    GtkWidget *dialog;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    /* popup the files dialog */
    toplevel = gtk_widget_get_toplevel(launcher->widget);

    if (G_LIKELY(toplevel != NULL))
    {
        dialog = thunar_properties_dialog_new(GTK_WINDOW(toplevel));

        /* check if no files are currently selected */
        if (launcher->files_to_process == NULL)
        {
            /* if we don't have any files selected, we just popup the properties dialog for the current folder. */
            thunar_properties_dialog_set_file(THUNAR_PROPERTIES_DIALOG(dialog), launcher->current_directory);
        }
        else
        {
            /* popup the properties dialog for all file(s) */
            thunar_properties_dialog_set_files(THUNAR_PROPERTIES_DIALOG(dialog), launcher->files_to_process);
        }
        gtk_widget_show(dialog);
    }
}

static void thunar_launcher_action_make_link(ThunarLauncher *launcher)
{
    ThunarApplication *application;
    GList             *g_files = NULL;
    GList             *lp;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->current_directory == NULL))
        return;
    if (launcher->files_are_selected == FALSE || thunar_file_is_trashed(launcher->current_directory))
        return;

    for (lp = launcher->files_to_process; lp != NULL; lp = lp->next)
    {
        g_files = g_list_append(g_files, thunar_file_get_file(lp->data));
    }
    /* link the selected files into the current directory, which effectively
     * creates new unique links for the files.
     */
    application = thunar_application_get();
    thunar_application_link_into(application, launcher->widget, g_files,
                                  thunar_file_get_file(launcher->current_directory), launcher->select_files_closure);
    g_object_unref(G_OBJECT(application));
    g_list_free(g_files);
}

static void thunar_launcher_action_duplicate(ThunarLauncher *launcher)
{
    ThunarApplication *application;
    GList             *files_to_process;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_UNLIKELY(launcher->current_directory == NULL))
        return;
    if (launcher->files_are_selected == FALSE || thunar_file_is_trashed(launcher->current_directory))
        return;

    /* determine the selected files for the view */
    files_to_process = thunar_file_list_to_thunar_g_file_list(launcher->files_to_process);
    if (G_LIKELY(files_to_process != NULL))
    {
        /* copy the selected files into the current directory, which effectively
         * creates duplicates of the files.
         */
        application = thunar_application_get();
        thunar_application_copy_into(application, launcher->widget, files_to_process,
                                      thunar_file_get_file(launcher->current_directory), launcher->select_files_closure);
        g_object_unref(G_OBJECT(application));

        /* clean up */
        thunar_g_file_list_free(files_to_process);
    }
}

static void thunar_launcher_action_key_rename(ThunarLauncher *launcher)
{
    ThunarWindow *window = THUNAR_WINDOW(launcher->widget);
    GtkWidget *tree_view = thunar_window_get_focused_tree_view(window);

    if (!tree_view)
    {
        thunar_launcher_action_rename(launcher);
        return;
    }

    thunar_tree_view_rename_selected(THUNAR_TREE_VIEW(tree_view));
}

static void _thunar_launcher_rename_error(ExoJob    *job,
                                         GError    *error,
                                         GtkWidget *widget)
{
    GArray     *param_values;
    ThunarFile *file;

    thunar_return_if_fail(EXO_IS_JOB(job));
    thunar_return_if_fail(error != NULL);

    param_values = thunar_simple_job_get_param_values(THUNAR_SIMPLE_JOB(job));
    file = g_value_get_object(&g_array_index(param_values, GValue, 0));

    thunar_dialogs_show_error(GTK_WIDGET(widget), error,
                               _("Failed to rename \"%s\""),
                               thunar_file_get_display_name(file));
    g_object_unref(file);
}


static void _thunar_launcher_rename_finished(ExoJob    *job,
                                             GtkWidget *widget)
{
    thunar_return_if_fail(EXO_IS_JOB(job));

    /* destroy the job */
    g_signal_handlers_disconnect_matched(job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, widget);
    g_object_unref(job);
}

void thunar_launcher_action_rename(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL
        || g_list_length(launcher->files_to_process) == 0)
        return;

    if (launcher->files_are_selected == FALSE
        || thunar_file_is_trashed(launcher->current_directory))
        return;

    /* get the window */
    GtkWidget *window = gtk_widget_get_toplevel(launcher->widget);

    /* start renaming if we have exactly one selected file */
    if (g_list_length(launcher->files_to_process) == 1)
    {
        /* run the rename dialog */
        ThunarJob *job = thunar_dialogs_show_rename_file(
                            GTK_WINDOW(window),
                            THUNAR_FILE(launcher->files_to_process->data));

        if (G_LIKELY(job != NULL))
        {
            g_signal_connect(job,
                             "error",
                             G_CALLBACK(_thunar_launcher_rename_error),
                             launcher->widget);
            g_signal_connect(job,
                             "finished",
                             G_CALLBACK(_thunar_launcher_rename_finished),
                             launcher->widget);
        }
    }
}

static void _thunar_launcher_action_terminal(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_to_process == NULL
        || g_list_length(launcher->files_to_process) == 0
        || thunar_file_is_trashed(launcher->current_directory))
        return;

    if (g_list_length(launcher->files_to_process) == 1)
    {
        ThunarFile *file = THUNAR_FILE(launcher->files_to_process->data);
        GFile *gfile = thunar_file_get_file(file);
        gchar *filepath = g_file_get_path(gfile);

        //const gchar *filename = thunar_file_get_display_name(file);


        CStringAuto *cmd = cstr_new_size(32);
        cstr_fmt(cmd,
                 "exo-open --working-directory \"%s\" --launch TerminalEmulator",
                 filepath);

        //g_print("execute : %s\n", c_str(cmd));

        g_spawn_command_line_async(c_str(cmd), NULL);

        g_free(filepath);
    }
}




static void thunar_launcher_action_key_trash_delete(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));
    thunar_return_if_fail(THUNAR_IS_WINDOW(launcher->widget));

    ThunarWindow *window = THUNAR_WINDOW(launcher->widget);
    GtkWidget *tree_view = thunar_window_get_focused_tree_view(window);

    if (!tree_view)
    {
        thunar_launcher_action_trash_delete(launcher);
        return;
    }

    if (thunar_dialogs_show_folder_trash(GTK_WINDOW(window)) == FALSE)
        return;

    thunar_tree_view_delete_selected_files(THUNAR_TREE_VIEW(tree_view));

}

static void thunar_launcher_action_trash_delete(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (!thunar_g_vfs_is_uri_scheme_supported("trash"))
        return;

    GdkModifierType event_state;

    /* when shift modifier is pressed, we delete(as well via context menu) */
    if (gtk_get_current_event_state(&event_state)
        &&(event_state & GDK_SHIFT_MASK) != 0)
    {
        thunar_launcher_action_delete(launcher);
        return;
    }

    thunar_launcher_action_move_to_trash(launcher);
}

static void thunar_launcher_action_move_to_trash(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->parent_folder == NULL
        || launcher->files_are_selected == FALSE)
        return;

    ThunarApplication *application = thunar_application_get();

    thunar_application_unlink_files(application,
                                    launcher->widget,
                                    launcher->files_to_process,
                                    FALSE);

    g_object_unref(G_OBJECT(application));
}

static void thunar_launcher_action_delete(ThunarLauncher *launcher)
{
    ThunarApplication *application;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->parent_folder == NULL
        || launcher->files_are_selected == FALSE)
        return;

    application = thunar_application_get();

    thunar_application_unlink_files(application,
                                     launcher->widget,
                                     launcher->files_to_process,
                                     TRUE);

    g_object_unref(G_OBJECT(application));
}

static void thunar_launcher_action_empty_trash(ThunarLauncher *launcher)
{
    ThunarApplication *application;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->single_directory_to_process == FALSE)
        return;
    if (!thunar_file_is_root(launcher->single_folder)
            || !thunar_file_is_trashed(launcher->single_folder))
        return;

    application = thunar_application_get();
    thunar_application_empty_trash(application, launcher->widget, NULL);
    g_object_unref(G_OBJECT(application));
}

static void thunar_launcher_action_restore(ThunarLauncher *launcher)
{
    ThunarApplication *application;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE
            || !thunar_file_is_trashed(launcher->current_directory))
        return;

    /* restore the selected files */
    application = thunar_application_get();
    thunar_application_restore_files(application, launcher->widget, launcher->files_to_process, NULL);
    g_object_unref(G_OBJECT(application));
}

static void thunar_launcher_action_create_folder(ThunarLauncher *launcher)
{
    ThunarApplication *application;
    GList              path_list;
    gchar             *name;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    //DPRINT("thunar_launcher_action_create_folder\n");

    if (thunar_file_is_trashed(launcher->current_directory))
        return;

    /* ask the user to enter a name for the new folder */
    name = thunar_dialogs_show_create(launcher->widget,
                                       "inode/directory",
                                       _("New Folder"),
                                       _("Create New Folder"));
    if (G_LIKELY(name != NULL))
    {
        /* fake the path list */
        if (THUNAR_IS_TREE_VIEW(launcher->widget) && launcher->files_are_selected && launcher->single_directory_to_process)
            path_list.data = g_file_resolve_relative_path(thunar_file_get_file(launcher->single_folder), name);
        else
            path_list.data = g_file_resolve_relative_path(thunar_file_get_file(launcher->current_directory), name);
        path_list.next = path_list.prev = NULL;

        /* launch the operation */
        application = thunar_application_get();
        thunar_application_mkdir(application, launcher->widget, &path_list, launcher->select_files_closure);
        g_object_unref(G_OBJECT(application));

        /* release the path */
        g_object_unref(path_list.data);

        /* release the file name */
        g_free(name);
    }
}

static void thunar_launcher_action_create_document(ThunarLauncher   *launcher,
                                                   GtkWidget        *menu_item)
{
    ThunarApplication *application;
    GList              target_path_list;
    gchar             *name;
    gchar             *title;
    ThunarFile        *template_file;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (thunar_file_is_trashed(launcher->current_directory))
        return;

    template_file = g_object_get_qdata(G_OBJECT(menu_item), thunar_launcher_file_quark);

    if (template_file != NULL)
    {
        /* generate a title for the create dialog */
        title = g_strdup_printf(_("Create Document from template \"%s\""),
                                 thunar_file_get_display_name(template_file));

        /* ask the user to enter a name for the new document */
        name = thunar_dialogs_show_create(launcher->widget,
                                           thunar_file_get_content_type(THUNAR_FILE(template_file)),
                                           thunar_file_get_display_name(template_file),
                                           title);
        /* cleanup */
        g_free(title);
    }
    else
    {
        /* ask the user to enter a name for the new empty file */
        name = thunar_dialogs_show_create(launcher->widget,
                                          "text/plain",
                                          _("New Empty File"),
                                          _("New Empty File..."));
    }

    if (G_LIKELY(name != NULL))
    {
        if (G_LIKELY(launcher->parent_folder != NULL))
        {
            /* fake the target path list */
            if (THUNAR_IS_TREE_VIEW(launcher->widget)
                && launcher->files_are_selected
                && launcher->single_directory_to_process)
            {
                target_path_list.data =
                            g_file_get_child(
                                thunar_file_get_file(launcher->single_folder),
                                name);
            }
            else
            {
                target_path_list.data =
                            g_file_get_child(
                                thunar_file_get_file(launcher->current_directory),
                                name);
            }

            target_path_list.next = NULL;
            target_path_list.prev = NULL;

            /* launch the operation */
            application = thunar_application_get();
            thunar_application_creat(application,
                                     launcher->widget,
                                     &target_path_list,
                                     template_file != NULL
                                        ? thunar_file_get_file(template_file)
                                        : NULL,
                                     launcher->select_files_closure);
            g_object_unref(G_OBJECT(application));

            /* release the target path */
            g_object_unref(target_path_list.data);
        }

        /* release the file name */
        g_free(name);
    }
}

/* helper method in order to find the parent menu for a menu item */
static GtkWidget*
thunar_launcher_create_document_submenu_templates_find_parent_menu(
                                                        ThunarFile *file,
                                                        GList      *dirs,
                                                        GList      *items)
{
    GtkWidget *parent_menu = NULL;
    GFile     *parent;
    GList     *lp;
    GList     *ip;

    /* determine the parent of the file */
    parent = g_file_get_parent(thunar_file_get_file(file));

    /* check if the file has a parent at all */
    if (parent == NULL)
        return NULL;

    /* iterate over all dirs and menu items */
    for (lp = g_list_first(dirs), ip = g_list_first(items);
            parent_menu == NULL && lp != NULL && ip != NULL;
            lp = lp->next, ip = ip->next)
    {
        /* check if the current dir/item is the parent of our file */
        if (g_file_equal(parent, thunar_file_get_file(lp->data)))
        {
            /* we want to insert an item for the file in this menu */
            parent_menu = gtk_menu_item_get_submenu(ip->data);
        }
    }

    /* destroy the parent GFile */
    g_object_unref(parent);

    return parent_menu;
}

/* recursive helper method in order to create menu items for all available templates */
static gboolean thunar_launcher_create_document_submenu_templates(
                                            ThunarLauncher *launcher,
                                            GtkWidget   *create_file_submenu,
                                            GList       *files)
{
    ThunarIconFactory *icon_factory;
    ThunarFile        *file;
    GdkPixbuf         *icon;
    GtkWidget         *parent_menu;
    GtkWidget         *submenu;
    GtkWidget         *image;
    GtkWidget         *item;
    GList             *lp;
    GList             *dirs = NULL;
    GList             *items = NULL;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);

    /* do nothing if there is no menu */
    if (create_file_submenu == NULL)
        return FALSE;

    /* get the icon factory */
    icon_factory = thunar_icon_factory_get_default();

    /* sort items so that directories come before files and ancestors come
     * before descendants */
    files = g_list_sort(files,(GCompareFunc)(void(*)(void)) thunar_file_compare_by_type);

    for (lp = g_list_first(files); lp != NULL; lp = lp->next)
    {
        file = lp->data;

        /* determine the parent menu for this file/directory */
        parent_menu = thunar_launcher_create_document_submenu_templates_find_parent_menu(file, dirs, items);
        if (parent_menu == NULL)
            parent_menu = create_file_submenu;

        /* determine the icon for this file/directory */
        icon = thunar_icon_factory_load_file_icon(icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 16);

        /* allocate an image based on the icon */
        image = gtk_image_new_from_pixbuf(icon);

        item = xfce_gtk_image_menu_item_new(thunar_file_get_display_name(file), NULL, NULL, NULL, NULL, image, GTK_MENU_SHELL(parent_menu));
        if (thunar_file_is_directory(file))
        {
            /* allocate a new submenu for the directory */
            submenu = gtk_menu_new();

            /* allocate a new menu item for the directory */
            gtk_menu_item_set_submenu(GTK_MENU_ITEM(item), submenu);

            /* prepend the directory, its item and the parent menu it should
             * later be added to to the respective lists */
            dirs = g_list_prepend(dirs, file);
            items = g_list_prepend(items, item);
        }
        else
        {
            g_signal_connect_swapped(G_OBJECT(item), "activate", G_CALLBACK(thunar_launcher_action_create_document), launcher);
            g_object_set_qdata_full(G_OBJECT(item), thunar_launcher_file_quark, g_object_ref(file), g_object_unref);
        }

        /* release the icon reference */
        g_object_unref(icon);
    }

    /* destroy lists */
    g_list_free(dirs);
    g_list_free(items);

    /* release the icon factory */
    g_object_unref(icon_factory);

    /* let the job destroy the file list */
    return FALSE;
}

/**
 * thunar_launcher_create_document_submenu_new:
 * @launcher : a #ThunarLauncher instance
 *
 * Will create a complete 'create_document* #GtkMenu and return it
 *
 * Return value:(transfer full): the created #GtkMenu
 **/
static GtkWidget* thunar_launcher_create_document_submenu_new(
                                                    ThunarLauncher *launcher)
{
    GList           *files = NULL;
    GFile           *home_dir;
    GFile           *templates_dir = NULL;
    const gchar     *path;
    gchar           *template_path;
    gchar           *label_text;
    GtkWidget       *submenu;
    GtkWidget       *item;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    home_dir = thunar_g_file_new_for_home();
    path = g_get_user_special_dir(G_USER_DIRECTORY_TEMPLATES);

    if (G_LIKELY(path != NULL))
        templates_dir = g_file_new_for_path(path);

    /* If G_USER_DIRECTORY_TEMPLATES not found, set "~/Templates" directory as default */
    if (G_UNLIKELY(path == NULL) || G_UNLIKELY(g_file_equal(templates_dir, home_dir)))
    {
        if (templates_dir != NULL)
            g_object_unref(templates_dir);
        templates_dir = g_file_resolve_relative_path(home_dir, "Templates");
    }

    if (G_LIKELY(templates_dir != NULL))
    {
        /* load the ThunarFiles */
        files = thunar_io_scan_directory(NULL, templates_dir, G_FILE_QUERY_INFO_NONE, TRUE, FALSE, TRUE, NULL);
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
        thunar_launcher_create_document_submenu_templates(launcher, submenu, files);
        thunar_g_file_list_free(files);
    }

    xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(submenu));
    xfce_gtk_image_menu_item_new_from_icon_name(_("_Empty File"), NULL, NULL, G_CALLBACK(thunar_launcher_action_create_document),
            G_OBJECT(launcher), "text-x-generic", GTK_MENU_SHELL(submenu));


    g_object_unref(templates_dir);
    g_object_unref(home_dir);

    return submenu;
}

static void thunar_launcher_action_cut(ThunarLauncher *launcher)
{
    ThunarClipboardManager *clipboard;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE || launcher->parent_folder == NULL)
        return;

    clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
    thunar_clipboard_manager_cut_files(clipboard, launcher->files_to_process);
    g_object_unref(G_OBJECT(clipboard));
}

static void thunar_launcher_action_copy(ThunarLauncher *launcher)
{
    ThunarClipboardManager *clipboard;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (launcher->files_are_selected == FALSE)
        return;

    clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
    thunar_clipboard_manager_copy_files(clipboard, launcher->files_to_process);
    g_object_unref(G_OBJECT(clipboard));
}

static void thunar_launcher_action_paste(ThunarLauncher *launcher)
{
    ThunarClipboardManager *clipboard;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
    thunar_clipboard_manager_paste_files(clipboard, thunar_file_get_file(launcher->current_directory), launcher->widget, launcher->select_files_closure);
    g_object_unref(G_OBJECT(clipboard));
}

static void thunar_launcher_action_paste_into_folder(ThunarLauncher *launcher)
{
    ThunarClipboardManager *clipboard;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (!launcher->single_directory_to_process)
        return;

    clipboard = thunar_clipboard_manager_get_for_display(gtk_widget_get_display(launcher->widget));
    thunar_clipboard_manager_paste_files(clipboard, thunar_file_get_file(launcher->single_folder), launcher->widget, launcher->select_files_closure);
    g_object_unref(G_OBJECT(clipboard));
}

void thunar_launcher_action_mount(ThunarLauncher *launcher)
{
    _thunar_launcher_poke(launcher, NULL,THUNAR_LAUNCHER_NO_ACTION);
}

static void thunar_launcher_action_eject_finish(ThunarDevice  *device,
                                                const GError *error,
                                                gpointer      user_data)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(user_data);
    gchar          *device_name;

    thunar_return_if_fail(THUNAR_IS_DEVICE(device));
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    /* check if there was an error */
    if (error != NULL)
    {
        /* display an error dialog to inform the user */
        device_name = thunar_device_get_name(device);
        thunar_dialogs_show_error(GTK_WIDGET(launcher->widget), error, _("Failed to eject \"%s\""), device_name);
        g_free(device_name);
    }
    else
    {
        launcher->device_to_process = NULL;
    }

    g_object_unref(launcher);
}

void thunar_launcher_action_eject(ThunarLauncher *launcher)
{
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_LIKELY(launcher->device_to_process != NULL))
    {
        /* prepare a mount operation */
        GMountOperation *mount_operation;
        mount_operation = thunar_gtk_mount_operation_new(GTK_WIDGET(launcher->widget));

        /* eject */
        thunar_device_eject(launcher->device_to_process,
                             mount_operation,
                             NULL,
                             thunar_launcher_action_eject_finish,
                             g_object_ref(launcher));

        g_object_unref(mount_operation);
    }
}

static void thunar_launcher_action_unmount_finish(ThunarDevice *device,
                                                  const GError *error,
                                                  gpointer      user_data)
{
    ThunarLauncher *launcher = THUNAR_LAUNCHER(user_data);
    gchar          *device_name;

    thunar_return_if_fail(THUNAR_IS_DEVICE(device));
    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    /* check if there was an error */
    if (error != NULL)
    {
        /* display an error dialog to inform the user */
        device_name = thunar_device_get_name(device);
        thunar_dialogs_show_error(GTK_WIDGET(launcher->widget), error, _("Failed to unmount \"%s\""), device_name);
        g_free(device_name);
    }
    else
        launcher->device_to_process = NULL;

    g_object_unref(launcher);
}

void thunar_launcher_action_unmount(ThunarLauncher *launcher)
{
    GMountOperation *mount_operation;

    thunar_return_if_fail(THUNAR_IS_LAUNCHER(launcher));

    if (G_LIKELY(launcher->device_to_process != NULL))
    {
        /* prepare a mount operation */
        mount_operation = thunar_gtk_mount_operation_new(GTK_WIDGET(launcher->widget));

        /* eject */
        thunar_device_unmount(launcher->device_to_process,
                               mount_operation,
                               NULL,
                               thunar_launcher_action_unmount_finish,
                               g_object_ref(launcher));

        /* release the device */
        g_object_unref(mount_operation);
    }
}

static GtkWidget* thunar_launcher_build_application_submenu(
                                                ThunarLauncher *launcher,
                                                GList          *applications)
{
    GList     *lp;
    GtkWidget *submenu;
    GtkWidget *image;
    GtkWidget *item;
    gchar     *label_text;
    gchar     *tooltip_text;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), NULL);

    submenu = gtk_menu_new();

    /* add open with subitem per application */
    for (lp = applications; lp != NULL; lp = lp->next)
    {
        label_text = g_strdup_printf(_("Open With \"%s\""), g_app_info_get_name(lp->data));
        tooltip_text = g_strdup_printf(ngettext("Use \"%s\" to open the selected file",
                                        "Use \"%s\" to open the selected files",
                                        launcher->n_files_to_process), g_app_info_get_name(lp->data));
        image = gtk_image_new_from_gicon(g_app_info_get_icon(lp->data), GTK_ICON_SIZE_MENU);
        item = xfce_gtk_image_menu_item_new(label_text, tooltip_text, NULL, G_CALLBACK(thunar_launcher_menu_item_activated), G_OBJECT(launcher), image, GTK_MENU_SHELL(submenu));
        g_object_set_qdata_full(G_OBJECT(item), thunar_launcher_appinfo_quark, g_object_ref(lp->data), g_object_unref);
        g_free(tooltip_text);
        g_free(label_text);
    }

    if (launcher->n_files_to_process == 1)
    {
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(submenu));
        thunar_launcher_append_menu_item(launcher, GTK_MENU_SHELL(submenu), THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
    }

    return submenu;
}

gboolean thunar_launcher_append_open_section(ThunarLauncher *launcher,
                                             GtkMenuShell   *menu,
                                             gboolean        support_tabs,
                                             gboolean        support_change_directory,
                                             gboolean        force)
{
    UNUSED(support_tabs);
    GList     *applications;
    gchar     *label_text;
    gchar     *tooltip_text;
    GtkWidget *image;
    GtkWidget *menu_item;
    GtkWidget *submenu;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);

    /* Usually it is not required to open the current directory */
    if (launcher->files_are_selected == FALSE && !force)
        return FALSE;

    /* determine the set of applications that work for all selected files */
    applications = thunar_file_list_get_applications(launcher->files_to_process);

    /* Execute OR Open OR OpenWith */
    if (G_UNLIKELY(launcher->n_executables_to_process == launcher->n_files_to_process))
        thunar_launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), THUNAR_LAUNCHER_ACTION_EXECUTE, FALSE);
    else if (G_LIKELY(launcher->n_directories_to_process >= 1))
    {
        if (support_change_directory)
            thunar_launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), THUNAR_LAUNCHER_ACTION_OPEN, FALSE);
    }
    else if (G_LIKELY(applications != NULL))
    {
        label_text = g_strdup_printf(_("_Open With \"%s\""), g_app_info_get_name(applications->data));
        tooltip_text = g_strdup_printf(ngettext("Use \"%s\" to open the selected file",
                                        "Use \"%s\" to open the selected files",
                                        launcher->n_files_to_process), g_app_info_get_name(applications->data));

        image = gtk_image_new_from_gicon(g_app_info_get_icon(applications->data), GTK_ICON_SIZE_MENU);
        menu_item = xfce_gtk_image_menu_item_new(label_text, tooltip_text, NULL, G_CALLBACK(thunar_launcher_menu_item_activated),
                    G_OBJECT(launcher), image, menu);

        /* remember the default application for the "Open" action as quark */
        g_object_set_qdata_full(G_OBJECT(menu_item), thunar_launcher_appinfo_quark, applications->data, g_object_unref);
        g_free(tooltip_text);
        g_free(label_text);

        /* drop the default application from the list */
        applications = g_list_delete_link(applications, applications);
    }
    else
    {
        /* we can only show a generic "Open" action */
        label_text = g_strdup_printf(_("_Open With Default Applications"));
        tooltip_text = g_strdup_printf(ngettext("Open the selected file with the default application",
                                        "Open the selected files with the default applications", launcher->n_files_to_process));
        xfce_gtk_menu_item_new(label_text, tooltip_text, NULL, G_CALLBACK(thunar_launcher_menu_item_activated), G_OBJECT(launcher), menu);
        g_free(tooltip_text);
        g_free(label_text);
    }

    if (launcher->n_files_to_process == launcher->n_directories_to_process
            && launcher->n_directories_to_process >= 1)
    {
        thunar_launcher_append_menu_item(launcher,
                                         GTK_MENU_SHELL(menu),
                                         THUNAR_LAUNCHER_ACTION_OPEN_IN_WINDOW,
                                         FALSE);
    }

    if (G_LIKELY(applications != NULL))
    {
        menu_item = xfce_gtk_menu_item_new(_("Open With"),
                                            _("Choose another application with which to open the selected file"),
                                            NULL, NULL, NULL, menu);
        submenu = thunar_launcher_build_application_submenu(launcher, applications);
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(menu_item), submenu);
    }
    else
    {
        if (launcher->n_files_to_process == 1)
            thunar_launcher_append_menu_item(launcher, GTK_MENU_SHELL(menu), THUNAR_LAUNCHER_ACTION_OPEN_WITH_OTHER, FALSE);
    }

    g_list_free_full(applications, g_object_unref);

    return TRUE;
}





// ============================================================================
#if 0
gboolean thunar_launcher_append_custom_actions(ThunarLauncher *launcher,
                                               GtkMenuShell   *menu)
{
    gboolean                uca_added = FALSE;
    GtkWidget              *window;
    GtkWidget              *gtk_menu_item;
    ThunarxProviderFactory *provider_factory;
    GList                  *providers;
    GList                  *thunarx_menu_items = NULL;
    GList                  *lp_provider;
    GList                  *lp_item;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);
    thunar_return_val_if_fail(GTK_IS_MENU(menu), FALSE);

    /* determine the toplevel window we belong to */
    window = gtk_widget_get_toplevel(launcher->widget);

    /* load the menu providers from the provider factory */
    provider_factory = thunarx_provider_factory_get_default();
    providers = thunarx_provider_factory_list_providers(provider_factory, THUNARX_TYPE_MENU_PROVIDER);
    g_object_unref(provider_factory);

    if (G_UNLIKELY(providers == NULL))
        return FALSE;

    /* This may occur when the thunar-window is build */
    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return FALSE;

    /* load the menu items offered by the menu providers */
    for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
        if (launcher->files_are_selected == FALSE)
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, THUNARX_FILE_INFO(launcher->current_directory));
        else
            thunarx_menu_items = thunarx_menu_provider_get_file_menu_items(lp_provider->data, window, launcher->files_to_process);

        for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
            gtk_menu_item = thunar_gtk_menu_thunarx_menu_item_new(lp_item->data, menu);

            /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item*/
            g_signal_connect_swapped(G_OBJECT(gtk_menu_item), "destroy", G_CALLBACK(g_object_unref), lp_item->data);
            uca_added = TRUE;
        }
        g_list_free(thunarx_menu_items);
    }

    g_list_free_full(providers, g_object_unref);

    return uca_added;
}

gboolean thunar_launcher_check_uca_key_activation(ThunarLauncher *launcher,
                                                  GdkEventKey    *key_event)
{
    GtkWidget              *window;
    ThunarxProviderFactory *provider_factory;
    GList                  *providers;
    GList                  *thunarx_menu_items = NULL;
    GList                  *lp_provider;
    GList                  *lp_item;
    GtkAccelKey             uca_key;
    gchar                  *name, *accel_path;
    gboolean                uca_activated = FALSE;

    /* determine the toplevel window we belong to */
    window = gtk_widget_get_toplevel(launcher->widget);

    /* load the menu providers from the provider factory */
    provider_factory = thunarx_provider_factory_get_default();
    providers = thunarx_provider_factory_list_providers(provider_factory, THUNARX_TYPE_MENU_PROVIDER);
    g_object_unref(provider_factory);

    if (G_UNLIKELY(providers == NULL))
        return uca_activated;

    /* load the menu items offered by the menu providers */
    for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
        if (launcher->files_are_selected == FALSE)
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, THUNARX_FILE_INFO(launcher->current_directory));
        else
            thunarx_menu_items = thunarx_menu_provider_get_file_menu_items(lp_provider->data, window, launcher->files_to_process);

        for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
            g_object_get(G_OBJECT(lp_item->data), "name", &name, NULL);

            accel_path = g_strconcat("<Actions>/ThunarActions/", name, NULL);
            if (gtk_accel_map_lookup_entry(accel_path, &uca_key) == TRUE)
            {
                if (g_ascii_tolower(key_event->keyval) == g_ascii_tolower(uca_key.accel_key))
                {
                    if ((key_event->state & gtk_accelerator_get_default_mod_mask()) == uca_key.accel_mods)
                    {
                        thunarx_menu_item_activate(lp_item->data);
                        uca_activated = TRUE;
                    }
                }
            }
            g_free(name);
            g_free(accel_path);
            g_object_unref(lp_item->data);
        }
        g_list_free(thunarx_menu_items);
    }
    g_list_free_full(providers, g_object_unref);
    return uca_activated;
}
#endif


