/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2005      Jeff Franks <jcfranks@xfce.org>
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
#include <application.h>

#include <browser.h>
#include <preferences.h>

#include <dialogs.h>
#include <progress_dlg.h>
#include <io_jobs.h>
#include <utils.h>

#ifdef ENABLE_ACCEL_MAP
#define ACCEL_MAP_PATH "Thunar/accels.scm"
#endif

/* option entries */
static const GOptionEntry _option_entries[] =
{
    {"daemon", 0, 0, G_OPTION_ARG_NONE, NULL, N_("Run in daemon mode"), NULL},
    {"quit", 'q', 0, G_OPTION_ARG_NONE, NULL, N_("Quit a running Thunar instance"), NULL},
    {G_OPTION_REMAINING, 0, 0, G_OPTION_ARG_FILENAME_ARRAY, NULL, NULL, NULL},
    {NULL, 0, 0, 0, NULL, NULL, NULL},
};

/* Prototype for the Thunar job launchers */
typedef ThunarJob* (*Launcher) (GList *source_path_list, GList *target_path_list);

/* Property identifiers */
enum
{
    PROP_0,
    PROP_DAEMON,
};

static void application_finalize(GObject *object);
static void application_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec);
static void application_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec);

static void application_startup(GApplication *application);

static void _application_accel_map_changed(Application *application);
static gboolean _application_accel_map_save(gpointer user_data);
static void _application_load_css();

static void application_shutdown(GApplication *application);
static void application_activate(GApplication *application);
static int application_command_line(GApplication *application,
                                    GApplicationCommandLine *command_line);

static void _application_collect_and_launch(Application *application,
                                            gpointer parent,
                                            const gchar *icon_name,
                                            const gchar *title,
                                            Launcher launcher,
                                            GList *source_file_list,
                                            GFile *target_file,
                                            gboolean update_source_folders,
                                            gboolean update_target_folders,
                                            GClosure *new_files_closure);
static void _application_launch_finished(ThunarJob *job,
                                         GList *containing_folders);
static void _application_launch(Application *application,
                                gpointer parent,
                                const gchar *icon_name,
                                const gchar *title,
                                Launcher launcher,
                                GList *source_path_list,
                                GList *target_path_list,
                                gboolean update_source_folders,
                                gboolean update_target_folders,
                                GClosure *new_files_closure);

static gboolean _application_show_dialogs(gpointer user_data);
static void _application_show_dialogs_destroy(gpointer user_data);
static GtkWidget* _application_get_progress_dialog(Application *application);

static void _application_process_files(Application *application);

struct _ApplicationClass
{
    GtkApplicationClass __parent__;
};

struct _Application
{
    GtkApplication      __parent__;

    gboolean            daemon;

    GtkWidget           *progress_dialog;
    guint               show_dialogs_timer_id;
    GList               *files_to_launch;

    guint               accel_map_save_id;
    GtkAccelMap         *accel_map;
};

static GQuark _app_screen_quark;
static GQuark _app_startup_id_quark;
static GQuark _app_file_quark;

G_DEFINE_TYPE_EXTENDED(Application,
                       application,
                       GTK_TYPE_APPLICATION,
                       0,
                       G_IMPLEMENT_INTERFACE(THUNAR_TYPE_BROWSER, NULL))

static void application_class_init(ApplicationClass *klass)
{
    /* pre-allocate the required quarks */
    _app_screen_quark = g_quark_from_static_string("application-screen");
    _app_startup_id_quark = g_quark_from_static_string("application-startup-id");
    _app_file_quark = g_quark_from_static_string("application-file");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = application_finalize;
    gobject_class->get_property = application_get_property;
    gobject_class->set_property = application_set_property;

    GApplicationClass *gapplication_class = G_APPLICATION_CLASS(klass);
    gapplication_class->startup = application_startup;
    gapplication_class->activate = application_activate;
    gapplication_class->shutdown = application_shutdown;
    gapplication_class->command_line = application_command_line;

    g_object_class_install_property(gobject_class,
                                    PROP_DAEMON,
                                    g_param_spec_boolean(
                                        "daemon",
                                        "daemon",
                                        "daemon",
                                        FALSE,
                                        E_PARAM_READWRITE));
}

static void application_init(Application *application)
{
    /* we do most initialization in GApplication::startup since it is only needed
     * in the primary instance anyways */

    application->files_to_launch = NULL;
    application->progress_dialog = NULL;

    g_application_set_flags(G_APPLICATION(application),
                            G_APPLICATION_HANDLES_COMMAND_LINE);

    g_application_add_main_option_entries(G_APPLICATION(application),
                                          _option_entries);
}

static void application_startup(GApplication *gapplication)
{
    Application *application = APPLICATION(gapplication);

    prefs_file_read();

    G_APPLICATION_CLASS(application_parent_class)->startup(gapplication);

#ifdef ENABLE_ACCEL_MAP
    /* check if we have a saved accel map */
    gchar *path = xfce_resource_lookup(XFCE_RESOURCE_CONFIG, ACCEL_MAP_PATH);

    if (G_LIKELY(path != NULL))
    {
        /* load the accel map */
        gtk_accel_map_load(path);
        g_free(path);
    }
#endif

    /* watch for changes */
    application->accel_map = gtk_accel_map_get();
    g_signal_connect_swapped(G_OBJECT(application->accel_map), "changed",
                             G_CALLBACK(_application_accel_map_changed),
                             application);

    _application_load_css();
}

static void _application_accel_map_changed(Application *application)
{
    e_return_if_fail(IS_APPLICATION(application));

    /* stop pending save */
    if (application->accel_map_save_id != 0)
    {
        g_source_remove(application->accel_map_save_id);
        application->accel_map_save_id = 0;
    }

    /* schedule new save */
    application->accel_map_save_id =
        g_timeout_add_seconds(10, _application_accel_map_save, application);
}

static gboolean _application_accel_map_save(gpointer user_data)
{
    Application *application = APPLICATION(user_data);

    e_return_val_if_fail(IS_APPLICATION(application), FALSE);

    application->accel_map_save_id = 0;

#ifdef ENABLE_ACCEL_MAP
    /* save the current accel map */
    gchar *path = xfce_resource_save_location(XFCE_RESOURCE_CONFIG,
                                              ACCEL_MAP_PATH,
                                              TRUE);
    if (G_LIKELY(path != NULL))
    {
        /* save the accel map */
        gtk_accel_map_save(path);
        g_free(path);
    }
#endif

    return FALSE;
}

static void _application_load_css()
{
    GtkCssProvider *css_provider = gtk_css_provider_new();

    /* for the pathbar-buttons any margin looks ugly*/
    /* remove extra border between side pane and view */
    /* add missing top border to side pane */
    /* make border thicker during DnD */

    gtk_css_provider_load_from_data(
        css_provider,
        ".path-bar-button { margin-right: 0; }"
        ".shortcuts-pane { border-right-width: 0px; }"
        ".shortcuts-pane { border-top-style: solid; }"
        ".standard-view { border-left-width: 0px; border-right-width: 0px; }"
        ".standard-view:drop(active) { border-width: 2px; }",
        -1,
        NULL);

    GdkScreen *screen = gdk_screen_get_default();
    gtk_style_context_add_provider_for_screen(
                                    screen,
                                    GTK_STYLE_PROVIDER(css_provider),
                                    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

    g_object_unref(css_provider);
}

static void application_shutdown(GApplication *gapplication)
{
    Application *application = APPLICATION(gapplication);

    prefs_write();
    prefs_cleanup();

    /* unqueue all files waiting to be processed */
    e_list_free(application->files_to_launch);

    /* save the current accel map */
    if (G_UNLIKELY(application->accel_map_save_id != 0))
    {
        g_source_remove(application->accel_map_save_id);
        _application_accel_map_save(application);
    }

    if (application->accel_map != NULL)
        g_object_unref(G_OBJECT(application->accel_map));

    /* drop any running "show dialogs" timer */
    if (G_UNLIKELY(application->show_dialogs_timer_id != 0))
        g_source_remove(application->show_dialogs_timer_id);

    G_APPLICATION_CLASS(application_parent_class)->shutdown(gapplication);
}

static void application_finalize(GObject *object)
{
    /* rule of thumb: what gets initialized in GApplication::startup is cleaned up
     * in GApplication::shutdown. Therefore, this method doesn't do very much */

    G_OBJECT_CLASS(application_parent_class)->finalize(object);
}

static int application_command_line(GApplication *gapplication,
                                    GApplicationCommandLine *command_line)
{
    Application *application  = APPLICATION(gapplication);

    gboolean           daemon       = FALSE;
    gboolean           quit         = FALSE;
    GStrv              filenames    = NULL;
    const char        *cwd          = g_application_command_line_get_cwd(command_line);
    GVariantDict      *options_dict = g_application_command_line_get_options_dict(command_line);
    GError            *error        = NULL;
    gchar             *cwd_list[]   = {(gchar *)".", NULL };

    /* retrieve arguments */
    g_variant_dict_lookup(options_dict, "quit", "b", &quit);
    g_variant_dict_lookup(options_dict, "daemon", "b", &daemon);
    g_variant_dict_lookup(options_dict, G_OPTION_REMAINING, "^aay", &filenames);

    /* FIXME: --quit should be named --suicide */
    if (G_UNLIKELY(quit))
    {
        g_debug("quitting");
        g_application_quit(gapplication);
        goto out;
    }

    /* check whether we should daemonize */
    if (G_UNLIKELY(daemon))
    {
        application_set_daemon(application, TRUE);
    }

    if (filenames != NULL)
    {
        if (!application_process_filenames(application, cwd, filenames, NULL, NULL, &error))
        {
            /* we failed to process the filenames or the bulk rename failed */
            g_application_command_line_printerr(command_line, "Thunar: %s\n", error->message);
        }
    }
    else if (!daemon)
    {
        if (!application_process_filenames(application, cwd, cwd_list, NULL, NULL, &error))
        {
            /* we failed to process the filenames or the bulk rename failed */
            g_application_command_line_printerr(command_line, "Thunar: %s\n", error->message);
        }
    }

out:
    /* cleanup */
    g_strfreev(filenames);

    if (error)
    {
        g_error_free(error);
        return EXIT_FAILURE;
    }
    else
        return EXIT_SUCCESS;
}

static void application_activate(GApplication *gapp)
{
    /* TODO */

    G_APPLICATION_CLASS(application_parent_class)->activate(gapp);
}

static void application_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    Application *application = APPLICATION(object);

    switch (prop_id)
    {
    case PROP_DAEMON:
        g_value_set_boolean(value, application_get_daemon(application));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void application_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    Application *application = APPLICATION(object);

    switch (prop_id)
    {
    case PROP_DAEMON:
        application_set_daemon(application, g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void _application_collect_and_launch(
                                        Application *application,
                                        gpointer    parent,
                                        const gchar *icon_name,
                                        const gchar *title,
                                        Launcher    launcher,
                                        GList       *source_file_list,
                                        GFile       *target_file,
                                        gboolean    update_source_folders,
                                        gboolean    update_target_folders,
                                        GClosure    *new_files_closure)
{
    GFile  *file;
    GError *err = NULL;
    GList  *target_file_list = NULL;
    GList  *lp;
    gchar  *base_name;

    /* check if we have anything to operate on */
    if (G_UNLIKELY(source_file_list == NULL))
        return;

    /* generate the target path list */
    for (lp = g_list_last(source_file_list); err == NULL && lp != NULL; lp = lp->prev)
    {
        /* verify that we're not trying to collect a root node */
        if (G_UNLIKELY(e_file_is_root(lp->data)))
        {
            /* tell the user that we cannot perform the requested operation */
            g_set_error(&err, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", g_strerror(EINVAL));
        }
        else
        {
            base_name = g_file_get_basename(lp->data);
            file = g_file_resolve_relative_path(target_file, base_name);
            g_free(base_name);

            /* add to the target file list */
            target_file_list = e_list_prepend_ref(target_file_list, file);
            g_object_unref(file);
        }
    }

    /* check if we failed */
    if (G_UNLIKELY(err != NULL))
    {
        /* display an error message to the user */
        dialog_error(parent, err, _("Failed to launch operation"));

        /* release the error */
        g_error_free(err);
    }
    else
    {
        /* launch the operation */
        _application_launch(application,
                            parent,
                            icon_name,
                            title,
                            launcher,
                            source_file_list, target_file_list,
                            update_source_folders, update_target_folders,
                            new_files_closure);
    }

    /* release the target path list */
    e_list_free(target_file_list);
}

static void _application_launch_finished(ThunarJob *job, GList *containing_folders)
{
    GList        *lp;
    ThunarFile   *file;
    ThunarFolder *folder;

    e_return_if_fail(THUNAR_IS_JOB(job));

    for(lp = containing_folders; lp != NULL; lp = lp->next)
    {
        if (lp->data == NULL)
            continue;
        file = th_file_get(lp->data, NULL);
        if (file != NULL)
        {
            if (th_file_is_directory(file))
            {
                folder = th_folder_get_for_file(file);
                if (folder != NULL)
                {
                    /* If the folder is connected to a folder monitor, we dont need to trigger the reload manually */
                    if (!th_folder_has_folder_monitor(folder))
                    {
                        th_folder_load(folder, FALSE);
                    }
                    g_object_unref(folder);
                }
            }
            g_object_unref(file);
        }
        /* Unref all containing_folders(refs obtained by g_file_get_parent in thunar_g_file_list_get_parents ) */
        g_object_unref(lp->data);
    }
}

static void _application_launch(Application *application,
                                gpointer           parent,
                                const gchar       *icon_name,
                                const gchar       *title,
                                Launcher           launcher,
                                GList             *source_file_list,
                                GList             *target_file_list,
                                gboolean           update_source_folders,
                                gboolean           update_target_folders,
                                GClosure          *new_files_closure)
{
    GtkWidget *dialog;
    GdkScreen *screen;
    ThunarJob *job;
    GList     *parent_folder_list = NULL;
    gboolean   has_jobs;

    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));

    /* parse the parent pointer */
    screen = util_parse_parent(parent, NULL);

    /* try to allocate a new job for the operation */
    job =(*launcher)(source_file_list, target_file_list);

    if (update_source_folders)
        parent_folder_list = g_list_concat(parent_folder_list, e_file_list_get_parents(source_file_list));
    if (update_target_folders)
        parent_folder_list = g_list_concat(parent_folder_list, e_file_list_get_parents(target_file_list));

    /* connect a callback to instantly refresh the parent folders after the operation finished */
    g_signal_connect(G_OBJECT(job), "finished",
                     G_CALLBACK(_application_launch_finished),
                     parent_folder_list);

    /* connect the "new-files" closure(if any) */
    if (G_LIKELY(new_files_closure != NULL))
        g_signal_connect_closure(job, "new-files", new_files_closure, FALSE);

    /* get the shared progress dialog */
    dialog = _application_get_progress_dialog(application);

    /* place the dialog on the given screen */
    if (screen != NULL)
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    has_jobs = thunar_progress_dialog_has_jobs(THUNAR_PROGRESS_DIALOG(dialog));

    /* add the job to the dialog */
    thunar_progress_dialog_add_job(THUNAR_PROGRESS_DIALOG(dialog),
                                    job, icon_name, title);

    if (has_jobs)
    {
        /* show the dialog immediately */
        _application_show_dialogs(application);
    }
    else
    {
        /* Set up a timer to show the dialog, to make sure we don't
         * just popup and destroy a dialog for a very short job.
         */
        if (G_LIKELY(application->show_dialogs_timer_id == 0))
        {
            application->show_dialogs_timer_id =
                gdk_threads_add_timeout_full(G_PRIORITY_DEFAULT,
                                             750,
                                             _application_show_dialogs,
                                             application,
                                             _application_show_dialogs_destroy);
        }
    }

    /* drop our reference on the job */
    g_object_unref(job);
}

static gboolean _application_show_dialogs(gpointer user_data)
{
    Application *application = APPLICATION(user_data);

    /* show the progress dialog */
    if (application->progress_dialog != NULL)
        gtk_window_present(GTK_WINDOW(application->progress_dialog));

    return FALSE;
}

static void _application_show_dialogs_destroy(gpointer user_data)
{
    APPLICATION(user_data)->show_dialogs_timer_id = 0;
}

/**
 * application_get:
 *
 * Returns the global shared #Application instance.
 * This method takes a reference on the global instance
 * for the caller, so you must call g_object_unref()
 * on it when done.
 *
 * Return value: the shared #Application instance.
 **/
Application* application_get()
{
    GApplication *default_app = g_application_get_default();

    if (default_app)
        return APPLICATION(g_object_ref(default_app));
    else
        return g_object_ref_sink(g_object_new(TYPE_APPLICATION,
                                              "application-id",
                                              "org.hotnuma.Fileman",
                                              NULL));
}

/**
 * application_get_daemon:
 * @application : a #Application.
 *
 * Returns %TRUE if @application is in daemon mode.
 *
 * Return value: %TRUE if @application is in daemon mode.
 **/
gboolean application_get_daemon(Application *application)
{
    e_return_val_if_fail(IS_APPLICATION(application), FALSE);
    return application->daemon;
}

/**
 * application_set_daemon:
 * @application : a #Application.
 * @daemonize   : %TRUE to set @application into daemon mode.
 *
 * If @daemon is %TRUE, @application will be set into daemon mode.
 **/
void application_set_daemon(Application *application,
                                   gboolean           daemonize)
{
    e_return_if_fail(IS_APPLICATION(application));

    if (application->daemon != daemonize)
    {
        application->daemon = daemonize;
        g_object_notify(G_OBJECT(application), "daemon");

        if (daemonize)
            g_application_hold(G_APPLICATION(application));
        else
            g_application_release(G_APPLICATION(application));
    }
}

/**
 * application_take_window:
 * @application : a #Application.
 * @window      : a #GtkWindow.
 *
 * Lets @application take over control of the specified @window.
 * @application will not exit until the last controlled #GtkWindow
 * is closed by the user.
 *
 * If the @window has no transient window, it will also create a
 * new #GtkWindowGroup for this window. This will make different
 * windows work independant(think gtk_dialog_run).
 **/
void application_take_window(Application *application,
                                    GtkWindow         *window)
{
    GtkWindowGroup *group;

    e_return_if_fail(GTK_IS_WINDOW(window));
    e_return_if_fail(IS_APPLICATION(application));

    /* only windows without a parent get a new window group */
    if (gtk_window_get_transient_for(window) == NULL && !gtk_window_has_group(window))
    {
        group = gtk_window_group_new();
        gtk_window_group_add_window(group, window);
        g_object_weak_ref(G_OBJECT(window),(GWeakNotify)(void(*)(void)) g_object_unref, group);
    }

    /* add the ourselves to the window */
    gtk_window_set_application(window, GTK_APPLICATION(application));
}

/**
 * application_open_window:
 * @application      : a #Application.
 * @directory        : the directory to open.
 * @screen           : the #GdkScreen on which to open the window or %NULL
 *                     to open on the default screen.
 * @startup_id       : startup id from startup notification passed along
 *                     with dbus to make focus stealing work properly.
 * @force_new_window : set this flag to force a new window, even if misc-open-new-window-as-tab is set
 *
 * Opens a new #ThunarWindow(or tab if preferred) for @application, displaying the
 * given @directory.
 *
 * Return value: the newly allocated #ThunarWindow.
 **/
GtkWidget* application_open_window(Application *application,
                                          ThunarFile        *directory,
                                          GdkScreen         *screen,
                                          const gchar       *startup_id,
                                          gboolean           force_new_window)
{
    (void) force_new_window;

    //DPRINT("application_open_window\n");

    GtkWidget *window;
    gchar     *role;

    e_return_val_if_fail(IS_APPLICATION(application), NULL);
    e_return_val_if_fail(directory == NULL || THUNAR_IS_FILE(directory), NULL);
    e_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), NULL);

    if (G_UNLIKELY(screen == NULL))
        screen = gdk_screen_get_default();

    /* generate a unique role for the new window(for session management) */
    role = g_strdup_printf("Thunar-%u-%u",(guint) time(NULL),(guint) g_random_int());

    /* allocate the window */
    window = g_object_new(THUNAR_TYPE_WINDOW,
                           "role", role,
                           "screen", screen,
                           NULL);

    /* cleanup */
    g_free(role);

    /* set the startup id */
    if (startup_id != NULL)
        gtk_window_set_startup_id(GTK_WINDOW(window), startup_id);

    /* hook up the window */
    application_take_window(application, GTK_WINDOW(window));

    /* show the new window */
    gtk_widget_show(window);

    /* change the directory */
    if (directory != NULL)
        window_set_current_directory(THUNAR_WINDOW(window), directory);

    return window;
}

static GtkWidget* _application_get_progress_dialog(
                                            Application *application)
{
    e_return_val_if_fail(IS_APPLICATION(application), NULL);

    if (application->progress_dialog == NULL)
    {
        application->progress_dialog = thunar_progress_dialog_new();

        g_object_add_weak_pointer(G_OBJECT(application->progress_dialog),
                                  (gpointer) &application->progress_dialog);

        application_take_window(application,
                                        GTK_WINDOW(application->progress_dialog));
    }

    return application->progress_dialog;
}

static void _application_process_files_finish(ThunarBrowser  *browser,
                                                    ThunarFile     *file,
                                                    ThunarFile     *target_file,
                                                    GError         *error,
                                                    gpointer       unused)
{
    (void) unused;

    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(THUNAR_IS_FILE(file));

    Application *application = APPLICATION(browser);
    e_return_if_fail(IS_APPLICATION(application));

    /* determine and reset the screen of the file */
    GdkScreen *screen = g_object_get_qdata(G_OBJECT(file), _app_screen_quark);
    g_object_set_qdata(G_OBJECT(file), _app_screen_quark, NULL);

    /* determine and the startup id of the file */
    const gchar *startup_id;
    startup_id = g_object_get_qdata(G_OBJECT(file), _app_startup_id_quark);

    /* check if resolving/mounting failed */
    if (error != NULL)
    {
        /* don't display cancel errors */
        if (error->domain != G_IO_ERROR || error->code != G_IO_ERROR_CANCELLED)
        {
            /* tell the user that we were unable to launch the file specified */
            dialog_error(screen, error, _("Failed to open \"%s\""),
                                       th_file_get_display_name(file));
        }

        /* stop processing files */
        e_list_free(application->files_to_launch);
        application->files_to_launch = NULL;
    }
    else
    {
        /* try to open the file or directory */
        th_file_launch(target_file, screen, startup_id, &error);

        /* remove the file from the list */
        application->files_to_launch =
            g_list_delete_link(application->files_to_launch,
                                application->files_to_launch);

        /* release the file */
        g_object_unref(file);

        /* check if we have more files to process */
        if (application->files_to_launch != NULL)
        {
            /* continue processing the next file */
            _application_process_files(application);
        }
    }

    /* unset the startup id */
    if (startup_id != NULL)
        g_object_set_qdata(G_OBJECT(file), _app_startup_id_quark, NULL);

    /* release the application */
    g_application_release(G_APPLICATION(application));
}

static void _application_process_files(Application *application)
{
    e_return_if_fail(IS_APPLICATION(application));

    /* don't do anything if no files are to be processed */
    if (application->files_to_launch == NULL)
        return;

    /* take the next file from the queue */
    ThunarFile *file = THUNAR_FILE(application->files_to_launch->data);

    /* retrieve the screen we need to launch the file on */
    GdkScreen *screen = g_object_get_qdata(G_OBJECT(file), _app_screen_quark);

    /* make sure to hold a reference to the application while file processing is going on */
    g_application_hold(G_APPLICATION(application));

    /* resolve the file and/or mount its enclosing volume
     * before handling it in the callback */
    browser_poke_file(THUNAR_BROWSER(application),
                      file,
                      screen,
                      _application_process_files_finish,
                      NULL);
}

/**
 * application_process_filenames:
 * @application       : a #Application.
 * @working_directory : the working directory relative to which the @filenames should
 *                      be interpreted.
 * @filenames         : a list of supported URIs or filenames. If a filename is specified
 *                      it can be either an absolute path or a path relative to the
 *                      @working_directory.
 * @screen            : the #GdkScreen on which to process the @filenames, or %NULL to
 *                      use the default screen.
 * @startup_id        : startup id to finish startup notification and properly focus the
 *                      window when focus stealing is enabled or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Tells @application to process the given @filenames and launch them appropriately.
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean application_process_filenames(Application *application,
                                              const gchar       *working_directory,
                                              gchar            **filenames,
                                              GdkScreen         *screen,
                                              const gchar       *startup_id,
                                              GError           **error)
{
    e_return_val_if_fail(IS_APPLICATION(application), FALSE);
    e_return_val_if_fail(working_directory != NULL, FALSE);
    e_return_val_if_fail(filenames != NULL, FALSE);
    e_return_val_if_fail(*filenames != NULL, FALSE);
    e_return_val_if_fail(screen == NULL || GDK_IS_SCREEN(screen), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GList *file_list = NULL;
    GError *derror = NULL;

    /* try to process all filenames and convert them to the appropriate file objects */
    for (gint n = 0; filenames[n] != NULL; ++n)
    {
        ThunarFile *file;
        gchar      *filename;

        /* check if the filename is an absolute path or looks like an URI */
        if (g_path_is_absolute(filenames[n])
            || g_uri_is_valid(filenames[n], G_URI_FLAGS_NONE, NULL))
        {
            /* determine the file for the filename directly */
            file = th_file_get_for_uri(filenames[n], &derror);
        }
        else
        {
            /* translate the filename into an absolute path first */
            filename = g_build_filename(working_directory, filenames[n], NULL);
            file = th_file_get_for_uri(filename, &derror);
            g_free(filename);
        }

        /* verify that we have a valid file */
        if (G_LIKELY(file != NULL))
        {
            file_list = g_list_append(file_list, file);
        }
        else
        {
            /* tell the user that we were unable to launch the file specified */
            dialog_error(screen, derror, _("Failed to open \"%s\""),
                                       filenames[n]);

            g_set_error(error, derror->domain, derror->code,
                         _("Failed to open \"%s\": %s"), filenames[n], derror->message);
            g_error_free(derror);

            e_list_free(file_list);

            return FALSE;
        }
    }

    /* loop over all files */
    for (GList *lp = file_list; lp != NULL; lp = lp->next)
    {
        /* remember the screen to launch the file on */
        g_object_set_qdata(G_OBJECT(lp->data), _app_screen_quark, screen);

        /* remember the startup id to set on the window */
        if (G_LIKELY(startup_id != NULL && *startup_id != '\0'))
            g_object_set_qdata_full(G_OBJECT(lp->data), _app_startup_id_quark,
                                    g_strdup(startup_id),(GDestroyNotify) g_free);

        /* append the file to the list of files we need to launch */
        application->files_to_launch = g_list_append(
                                            application->files_to_launch,
                                            lp->data);
    }

    /* start processing files if we have any to launch */
    if (application->files_to_launch != NULL)
        _application_process_files(application);

    /* free the file list */
    g_list_free(file_list);

    return TRUE;
}

/**
 * application_copy_into:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be copied.
 * @target_file       : the #GFile to the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Copies all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user interaction.
 **/
void application_copy_into(Application *application,
                           gpointer    parent,
                           GList       *source_file_list,
                           GFile       *target_file,
                           GClosure    *new_files_closure)
{
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(G_IS_FILE(target_file));

    /* generate a title for the progress dialog */
    gchar *display_name = th_file_cached_display_name(target_file);
    gchar *title = g_strdup_printf(_("Copying files to \"%s\"..."),
                                   display_name);
    g_free(display_name);

    /* collect the target files and launch the job */
    _application_collect_and_launch(application,
                                    parent,
                                    "edit-copy",
                                    title,
                                    io_copy_files,
                                    source_file_list,
                                    target_file,
                                    FALSE,
                                    TRUE,
                                    new_files_closure);

    /* free the title */
    g_free(title);
}

/**
 * application_link_into:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be symlinked.
 * @target_file       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Symlinks all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void application_link_into(Application *application,
                           gpointer           parent,
                           GList             *source_file_list,
                           GFile             *target_file,
                           GClosure          *new_files_closure)
{
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(G_IS_FILE(target_file));

    /* generate a title for the progress dialog */
    gchar *display_name = th_file_cached_display_name(target_file);
    gchar *title;
    title = g_strdup_printf(_("Creating symbolic links in \"%s\"..."), display_name);
    g_free(display_name);

    /* collect the target files and launch the job */
    _application_collect_and_launch(application, parent, "insert-link",
                                           title, io_link_files,
                                           source_file_list, target_file,
                                           FALSE, TRUE,
                                           new_files_closure);

    /* free the title */
    g_free(title);
}

/**
 * application_move_into:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @source_file_list  : the list of #GFile<!---->s that should be moved.
 * @target_file       : the target directory.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Moves all files referenced by the @source_file_list to the directory
 * referenced by @target_file. This method takes care of all user
 * interaction.
 **/
void application_move_into(Application *application,
                           gpointer           parent,
                           GList             *source_file_list,
                           GFile             *target_file,
                           GClosure          *new_files_closure)
{
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(target_file != NULL);

    /* launch the appropriate operation depending on the target file */
    if (e_file_is_trashed(target_file))
    {
        application_trash(application, parent, source_file_list);
    }
    else
    {
        /* generate a title for the progress dialog */
        gchar *display_name = th_file_cached_display_name(target_file);
        gchar *title = g_strdup_printf(_("Moving files into \"%s\"..."),
                                       display_name);
        g_free(display_name);

        /* collect the target files and launch the job */
        _application_collect_and_launch(application,
                                        parent,
                                        "stock_folder-move",
                                        title,
                                        io_move_files,
                                        source_file_list,
                                        target_file,
                                        TRUE,
                                        TRUE,
                                        new_files_closure);

        /* free the title */
        g_free(title);
    }
}

static ThunarJob* unlink_stub(GList *source_path_list, GList *target_path_list)
{
    (void) target_path_list;

    return io_unlink_files(source_path_list);
}

/**
 * application_unlink_files:
 * @application : a #Application.
 * @parent      : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list   : the list of #ThunarFile<!---->s that should be deleted.
 * @permanently : whether to unlink the files permanently.
 *
 * Deletes all files in the @file_list and takes care of all user interaction.
 *
 * If the user pressed the shift key while triggering the delete action,
 * the files will be deleted permanently(after confirming the action),
 * otherwise the files will be moved to the trash.
 **/
void application_unlink_files(Application *application,
                              gpointer    parent,
                              GList       *file_list,
                              gboolean    permanently)
{
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));

    GList     *lp;
    guint      n_path_list = 0;
    GList     *path_list = NULL;

    /* determine the paths for the files */
    for (lp = g_list_last(file_list); lp != NULL; lp = lp->prev, ++n_path_list)
    {
        /* prepend the path to the path list */
        path_list = e_list_prepend_ref(path_list, th_file_get_file(lp->data));

        /* permanently delete if at least one of the file is not a local
         * file(e.g. resides in the trash) or cannot be trashed */
        if (!th_file_is_local(lp->data)
            || !th_file_can_be_trashed(lp->data))
            permanently = TRUE;
    }

    /* nothing to do if we don't have any paths */
    if (G_UNLIKELY(n_path_list == 0))
        return;

    /* ask the user to confirm if deleting permanently */
    if (G_UNLIKELY(permanently))
    {
        /* parse the parent pointer */
        GtkWindow *window;
        GdkScreen *screen = util_parse_parent(parent, &window);

        gchar     *message;

        /* generate the question to confirm the delete operation */
        if (G_LIKELY(n_path_list == 1))
        {
            message = g_strdup_printf(
            _("Are you sure that you want to\npermanently delete \"%s\"?"),
            th_file_get_display_name(THUNAR_FILE(file_list->data)));
        }
        else
        {
            message = g_strdup_printf(ngettext("Are you sure that you want to permanently\ndelete the selected file?",
                                                 "Are you sure that you want to permanently\ndelete the %u selected files?",
                                                 n_path_list),
                                       n_path_list);
        }

        /* ask the user to confirm the delete operation */
        GtkWidget *dialog = gtk_message_dialog_new(window,
                                         GTK_DIALOG_MODAL
                                         | GTK_DIALOG_DESTROY_WITH_PARENT,
                                         GTK_MESSAGE_QUESTION,
                                         GTK_BUTTONS_NONE,
                                         "%s", message);

        if (G_UNLIKELY(window == NULL && screen != NULL))
            gtk_window_set_screen(GTK_WINDOW(dialog), screen);

        gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                                _("_Cancel"), GTK_RESPONSE_CANCEL,
                                _("_Delete"), GTK_RESPONSE_YES,
                                NULL);

        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                            _("If you delete a file, it is permanently lost."));

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        gtk_widget_destroy(dialog);
        g_free(message);

        /* perform the delete operation */
        if (G_LIKELY(response == GTK_RESPONSE_YES))
        {
            /* launch the "Delete" operation */
            _application_launch(application,
                                parent,
                                "edit-delete",
                                _("Deleting files..."),
                                unlink_stub,
                                path_list,
                                path_list,
                                TRUE,
                                FALSE,
                                NULL);
        }
    }
    else
    {
        /* launch the "Move to Trash" operation */
        application_trash(application, parent, path_list);
    }

    /* release the path list */
    e_list_free(path_list);
}

static ThunarJob* trash_stub(GList *source_file_list, GList *target_file_list)
{
    (void) target_file_list;

    return io_trash_files(source_file_list);
}

void application_trash(Application *application, gpointer parent,
                       GList *file_list)
{
    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent)
                     || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(file_list != NULL);

    _application_launch(application,
                        parent,
                        "user-trash-full",
                        _("Moving files into the trash..."),
                        trash_stub,
                        file_list,
                        NULL,
                        TRUE,
                        FALSE,
                        NULL);
}

static ThunarJob* creat_stub(GList *template_file, GList *target_path_list)
{
    e_return_val_if_fail(template_file->data == NULL
                         || G_IS_FILE(template_file->data), NULL);

    return io_create_files(target_path_list, template_file->data);
}

/**
 * application_creat:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of files to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates empty files for all #GFile<!---->s listed in @file_list. This
 * method takes care of all user interaction.
 **/
void application_creat(Application *application,
                       gpointer           parent,
                       GList             *file_list,
                       GFile             *template_file,
                       GClosure          *new_files_closure)
{
    GList template_list;

    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent)
                     || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));

    template_list.next = template_list.prev = NULL;
    template_list.data = template_file;

    /* launch the operation */
    _application_launch(application,
                        parent,
                        "document-new",
                        _("Creating files..."),
                        creat_stub,
                        &template_list,
                        file_list,
                        FALSE,
                        TRUE,
                        new_files_closure);
}

static ThunarJob* mkdir_stub(GList *source_path_list, GList *target_path_list)
{
    (void) target_path_list;

    return io_make_directories(source_path_list);
}

/**
 * application_mkdir:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @file_list         : the list of directories to create.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Creates all directories referenced by the @file_list. This method takes care of all user
 * interaction.
 **/
void application_mkdir(Application *application,
                              gpointer           parent,
                              GList             *file_list,
                              GClosure          *new_files_closure)
{
    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent)
                     || GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_APPLICATION(application));

    /* launch the operation */
    _application_launch(application,
                        parent,
                        "folder-new",
                        _("Creating directories..."),
                        mkdir_stub,
                        file_list,
                        file_list,
                        TRUE,
                        FALSE,
                        new_files_closure);
}

/**
 * application_empty_trash:
 * @application : a #Application.
 * @parent      : the parent, which can either be the associated
 *                #GtkWidget, the screen on which display the
 *                progress and the confirmation, or %NULL.
 *
 * Deletes all files and folders in the Trash after asking
 * the user to confirm the operation.
 **/
void application_empty_trash(Application *application, gpointer parent,
                             const gchar *startup_id)
{
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));

    /* parse the parent pointer */
    GtkWindow *window;
    GdkScreen *screen = util_parse_parent(parent, &window);

    /* ask the user to confirm the operation */
    GtkWidget *dialog = gtk_message_dialog_new(
                        window,
                        GTK_DIALOG_MODAL
                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_QUESTION,
                        GTK_BUTTONS_NONE,
                        _("Remove all files and folders from the Trash?"));

    if (G_UNLIKELY(window == NULL && screen != NULL))
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    gtk_window_set_startup_id(GTK_WINDOW(dialog), startup_id);

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           _("_Cancel"), GTK_RESPONSE_CANCEL,
                           _("_Empty Trash"), GTK_RESPONSE_YES,
                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
    gtk_message_dialog_format_secondary_text(
                GTK_MESSAGE_DIALOG(dialog),
            _("If you choose to empty the Trash, all items in it will be permanently lost. "
              "Please note that you can also delete them separately."));

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    gtk_widget_destroy(dialog);

    GList file_list;

    /* check if the user confirmed */
    if (G_LIKELY(response == GTK_RESPONSE_YES))
    {
        /* fake a path list with only the trash root(the root
         * folder itself will never be unlinked, so this is safe)
         */
        file_list.data = e_file_new_for_trash();
        file_list.next = NULL;
        file_list.prev = NULL;

        /* launch the operation */
        _application_launch(application,
                            parent,
                            "user-trash",
                            _("Emptying the Trash..."),
                            unlink_stub,
                            &file_list,
                            NULL,
                            TRUE,
                            FALSE,
                            NULL);

        /* cleanup */
        g_object_unref(file_list.data);
    }
}

/**
 * application_restore_files:
 * @application       : a #Application.
 * @parent            : a #GdkScreen, a #GtkWidget or %NULL.
 * @trash_file_list   : a #GList of #ThunarFile<!---->s in the trash.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Restores all #ThunarFile<!---->s in the @trash_file_list to their original
 * location.
 **/
void application_restore_files(Application *application,
                               gpointer    parent,
                               GList       *trash_file_list,
                               GClosure    *new_files_closure)
{
    e_return_if_fail(IS_APPLICATION(application));
    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent)
                     || GTK_IS_WIDGET(parent));

    GList *lp;
    GError *err = NULL;
    GList *source_path_list = NULL;
    GList *target_path_list = NULL;

    for (lp = trash_file_list; lp != NULL; lp = lp->next)
    {
        const gchar *original_uri;
        GFile       *target_path;
        original_uri = th_file_get_original_path(lp->data);
        if (G_UNLIKELY(original_uri == NULL))
        {
            /* no OriginalPath, impossible to continue */
            g_set_error(&err, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                         _("Failed to determine the original path for \"%s\""),
                         th_file_get_display_name(lp->data));
            break;
        }

        /* TODO we might have to distinguish between URIs and paths here */
        target_path = g_file_new_for_commandline_arg(original_uri);

        source_path_list = e_list_append_ref(source_path_list, th_file_get_file(lp->data));
        target_path_list = e_list_append_ref(target_path_list, target_path);

        g_object_unref(target_path);
    }

    if (G_UNLIKELY(err != NULL))
    {
        /* display an error dialog */
        dialog_error(parent,
                     err,
                     _("Could not restore \"%s\""),
                     th_file_get_display_name(lp->data));

        g_error_free(err);
    }
    else
    {
        /* launch the operation */
        _application_launch(application,
                            parent,
                            "stock_folder-move",
                            _("Restoring files..."),
                            io_restore_files,
                            source_path_list,
                            target_path_list,
                            TRUE,
                            TRUE,
                            new_files_closure);
    }

    /* free path lists */
    e_list_free(source_path_list);
    e_list_free(target_path_list);
}


