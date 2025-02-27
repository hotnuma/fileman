/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include "config.h"
#include "th-folder.h"

#include "filemonitor.h"
#include "io-jobs.h"
#include "gio-ext.h"

#define DEBUG_FILE_CHANGES FALSE

// ThunarFolder ---------------------------------------------------------------

static void th_folder_constructed(GObject *object);
static void th_folder_dispose(GObject *object);
static void th_folder_finalize(GObject *object);
static void th_folder_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec);
static void th_folder_set_property(GObject *object, guint prop_uid,
                                   const GValue *value, GParamSpec *pspec);
static void th_folder_real_destroy(ThunarFolder *folder);
static void _th_folder_move_monitor(ThunarFolder *folder);

// Public ---------------------------------------------------------------------

// th_folder_load
static void _th_folder_error(ExoJob *job, GError *error, ThunarFolder *folder);
static void _th_folder_finished(ExoJob *job, ThunarFolder *folder);
static gboolean _th_folder_files_ready(ThunarJob *job, GList *files,
                                       ThunarFolder *folder);
// type loader
static void _th_folder_content_type_loader(ThunarFolder *folder);
static gboolean _th_folder_content_type_loader_idle(gpointer data);
static void _th_folder_content_type_loader_idle_destroyed(gpointer data);

// Monitor --------------------------------------------------------------------

static void _gfmonitor_on_changed(GFileMonitor *monitor,
                                  GFile *file,
                                  GFile *other_file,
                                  GFileMonitorEvent event_type,
                                  gpointer user_data);

static void _monitor_on_changed(FileMonitor *file_monitor,
                                ThunarFile *file,
                                ThunarFolder *folder);
static void _monitor_on_destroyed(FileMonitor *file_monitor,
                                  ThunarFile *file,
                                  ThunarFolder *folder);

// ThunarFolder ---------------------------------------------------------------

static GQuark _th_folder_quark;

enum
{
    PROP_0,
    PROP_CORRESPONDING_FILE,
    PROP_LOADING,
};

// signal identifiers
enum
{
    DESTROY,
    ERROR,
    FILES_ADDED,
    FILES_REMOVED,
    LAST_SIGNAL,
};

static guint _th_folder_signals[LAST_SIGNAL];

struct _ThunarFolderClass
{
    GObjectClass __parent__;

    // signals
    void (*destroy)       (ThunarFolder *folder);
    void (*error)         (ThunarFolder *folder, const GError *error);
    void (*files_added)   (ThunarFolder *folder, GList *files);
    void (*files_removed) (ThunarFolder *folder, GList *files);
};

struct _ThunarFolder
{
    GObject     __parent__;

    ThunarFile  *thunar_file;
    gboolean    reload_info;
    guint       in_destruction : 1;

    // io_list_directory
    ThunarJob   *job;
    GList       *new_files;
    GList       *files;

    // content type list
    guint       content_type_idle_id;
    GList       *type_list;

    gchar       *monitor_uri;
    GFileMonitor *gfilemonitor;
    FileMonitor *monitor;
};

G_DEFINE_TYPE(ThunarFolder, th_folder, G_TYPE_OBJECT)

static void th_folder_class_init(ThunarFolderClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->constructed = th_folder_constructed;
    gobject_class->dispose = th_folder_dispose;
    gobject_class->finalize = th_folder_finalize;
    gobject_class->get_property = th_folder_get_property;
    gobject_class->set_property = th_folder_set_property;

    klass->destroy = th_folder_real_destroy;

    g_object_class_install_property(gobject_class,
                                    PROP_CORRESPONDING_FILE,
                                    g_param_spec_object(
                                        "corresponding-file",
                                        "corresponding-file",
                                        "corresponding-file",
                                        TYPE_THUNARFILE,
                                        G_PARAM_READABLE
                                        | G_PARAM_WRITABLE
                                        | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class,
                                    PROP_LOADING,
                                    g_param_spec_boolean(
                                        "loading",
                                        "loading",
                                        "loading",
                                        FALSE,
                                        E_PARAM_READABLE));

    _th_folder_signals[DESTROY] =
        g_signal_new(I_("destroy"),
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_CLEANUP | G_SIGNAL_NO_RECURSE | G_SIGNAL_NO_HOOKS,
                     G_STRUCT_OFFSET(ThunarFolderClass, destroy),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);

    _th_folder_signals[ERROR] =
        g_signal_new(I_("error"),
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(ThunarFolderClass, error),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    _th_folder_signals[FILES_ADDED] =
        g_signal_new(I_("files-added"),
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(ThunarFolderClass, files_added),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);

    _th_folder_signals[FILES_REMOVED] =
        g_signal_new(I_("files-removed"),
                     G_TYPE_FROM_CLASS(gobject_class),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(ThunarFolderClass, files_removed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE, 1, G_TYPE_POINTER);
}

static void th_folder_init(ThunarFolder *folder)
{
    // connect to the FileMonitor instance
    folder->monitor = filemon_get_default();

    g_signal_connect(G_OBJECT(folder->monitor), "file-changed",
                     G_CALLBACK(_monitor_on_changed), folder);

    g_signal_connect(G_OBJECT(folder->monitor), "file-destroyed",
                     G_CALLBACK(_monitor_on_destroyed), folder);

    folder->monitor_uri = NULL;
    folder->gfilemonitor = NULL;
    folder->reload_info = FALSE;
}

// GObjectClass ---------------------------------------------------------------

static void th_folder_constructed(GObject *object)
{
    ThunarFolder *folder = THUNARFOLDER(object);

    GError *error = NULL;

    folder->monitor_uri = th_file_get_uri(folder->thunar_file);
    folder->gfilemonitor = g_file_monitor_directory(
                                th_file_get_file(folder->thunar_file),
                                G_FILE_MONITOR_WATCH_MOVES,
                                NULL,
                                &error);

    if (folder->gfilemonitor != NULL)
    {
        g_signal_connect(folder->gfilemonitor, "changed",
                         G_CALLBACK(_gfmonitor_on_changed), folder);
    }
    else
    {
        g_debug("Could not create folder monitor: %s", error->message);
        g_error_free(error);
    }

    G_OBJECT_CLASS(th_folder_parent_class)->constructed(object);
}

static void th_folder_dispose(GObject *object)
{
    ThunarFolder *folder = THUNARFOLDER(object);

    if (!folder->in_destruction)
    {
        folder->in_destruction = TRUE;
        g_signal_emit(G_OBJECT(folder), _th_folder_signals[DESTROY], 0);
        folder->in_destruction = FALSE;
    }

    G_OBJECT_CLASS(th_folder_parent_class)->dispose(object);
}

static void th_folder_finalize(GObject *object)
{
    ThunarFolder *folder = THUNARFOLDER(object);

    if (folder->thunar_file)
        th_file_unwatch(folder->thunar_file);

    // disconnect from the FileMonitor instance
    g_signal_handlers_disconnect_matched(folder->monitor,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, folder);
    g_object_unref(folder->monitor);

    // disconnect from the file alteration monitor
    if (folder->gfilemonitor != NULL)
    {
        g_signal_handlers_disconnect_matched(folder->gfilemonitor,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, folder);
        g_file_monitor_cancel(folder->gfilemonitor);
        g_object_unref(folder->gfilemonitor);
    }

    if (folder->monitor_uri)
        g_free(folder->monitor_uri);

    // cancel the pending job(if any)
    if (folder->job != NULL)
    {
        g_signal_handlers_disconnect_matched(folder->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, folder);
        g_object_unref(folder->job);
        folder->job = NULL;
    }

    // disconnect from the corresponding file
    if (folder->thunar_file != NULL)
    {
        // drop the reference
        g_object_set_qdata(G_OBJECT(folder->thunar_file),
                           _th_folder_quark,
                           NULL);
        g_object_unref(G_OBJECT(folder->thunar_file));
    }

    // stop metadata collector
    if (folder->content_type_idle_id != 0)
        g_source_remove(folder->content_type_idle_id);

    // release references to the new files
    e_list_free(folder->new_files);

    // release references to the current files
    e_list_free(folder->files);

    G_OBJECT_CLASS(th_folder_parent_class)->finalize(object);
}

static void th_folder_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarFolder *folder = THUNARFOLDER(object);

    switch (prop_id)
    {
    case PROP_CORRESPONDING_FILE:
        g_value_set_object(value, folder->thunar_file);
        break;

    case PROP_LOADING:
        g_value_set_boolean(value, th_folder_get_loading(folder));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void th_folder_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarFolder *folder = THUNARFOLDER(object);

    switch (prop_id)
    {
    case PROP_CORRESPONDING_FILE:
        folder->thunar_file = g_value_dup_object(value);
        if (folder->thunar_file)
            th_file_watch(folder->thunar_file);
        break;

    case PROP_LOADING:
        e_assert_not_reached();
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

gboolean th_folder_get_loading(const ThunarFolder *folder)
{
    e_return_val_if_fail(IS_THUNARFOLDER(folder), FALSE);

    return (folder->job != NULL);
}

// ThunarFolderClass ----------------------------------------------------------

static void th_folder_real_destroy(ThunarFolder *folder)
{
    g_signal_handlers_destroy(G_OBJECT(folder));
}

// Load / Reload --------------------------------------------------------------

void th_folder_load(ThunarFolder *folder, gboolean reload_info)
{
    e_return_if_fail(IS_THUNARFOLDER(folder));

    // reload file info too?
    folder->reload_info = reload_info;

    // stop metadata collector
    if (folder->content_type_idle_id != 0)
        g_source_remove(folder->content_type_idle_id);

    // check if we are currently connect to a job
    if (folder->job != NULL)
    {
        // disconnect from the job
        g_signal_handlers_disconnect_matched(folder->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             folder);
        g_object_unref(folder->job);

        folder->job = NULL;
    }

    // reset the new_files list
    e_list_free(folder->new_files);

    folder->new_files = NULL;

    // start a new job
    folder->job = io_list_directory(th_file_get_file(folder->thunar_file));

    g_signal_connect(folder->job, "error",
                     G_CALLBACK(_th_folder_error), folder);

    g_signal_connect(folder->job, "finished",
                     G_CALLBACK(_th_folder_finished), folder);

    g_signal_connect(folder->job, "files-ready",
                     G_CALLBACK(_th_folder_files_ready), folder);

    // tell all consumers that we're loading
    g_object_notify(G_OBJECT(folder), "loading");
}

static void _th_folder_error(ExoJob *job, GError *error, ThunarFolder *folder)
{
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(THUNAR_IS_JOB(job));

    // tell the consumer about the problem
    g_signal_emit(G_OBJECT(folder), _th_folder_signals[ERROR], 0, error);
}

static void _th_folder_finished(ExoJob *job, ThunarFolder *folder)
{
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(IS_THUNARFILE(folder->thunar_file));
    e_return_if_fail(folder->content_type_idle_id == 0);

    GList *files;
    GList *lp;

    // check if we need to merge new files with existing files
    if (folder->files != NULL)
    {
        // determine all added files (files on new_files, but not on files)
        for (files = NULL, lp = folder->new_files; lp != NULL; lp = lp->next)
        {
            if (g_list_find(folder->files, lp->data) == NULL)
            {
                // put the file on the added list
                files = g_list_prepend(files, lp->data);

                // add to the internal files list
                folder->files = g_list_prepend(folder->files, lp->data);
                g_object_ref(G_OBJECT(lp->data));
            }
        }

        // check if any files were added
        if (files != NULL)
        {
            // emit a "files-added" signal for the added files
            g_signal_emit(G_OBJECT(folder), _th_folder_signals[FILES_ADDED], 0, files);

            // release the added files list
            g_list_free(files);
        }

        // determine all removed files(files on files, but not on new_files)
        for (files = NULL, lp = folder->files; lp != NULL; )
        {
            // determine the file
            ThunarFile *file = THUNARFILE(lp->data);

            // determine the next list item
            lp = lp->next;

            // check if the file is not on new_files
            if (g_list_find(folder->new_files, file) == NULL)
            {
                // put the file on the removed list(owns the reference now)
                files = g_list_prepend(files, file);

                // remove from the internal files list
                folder->files = g_list_remove(folder->files, file);
            }
        }

        // check if any files were removed
        if (files != NULL)
        {
            // emit a "files-removed" signal for the removed files
            g_signal_emit(G_OBJECT(folder), _th_folder_signals[FILES_REMOVED], 0, files);

            // release the removed files list
            e_list_free(files);
        }

        // drop the temporary new_files list
        e_list_free(folder->new_files);
        folder->new_files = NULL;
    }
    else
    {
        // just use the new files for the files list
        folder->files = folder->new_files;
        folder->new_files = NULL;

        if (folder->files != NULL)
        {
            // emit a "files-added" signal for the new files
            g_signal_emit(G_OBJECT(folder),
                          _th_folder_signals[FILES_ADDED],
                          0, folder->files);
        }
    }

    // schedule a reload of the file information of all files if requested
    if (folder->reload_info)
    {
        for (lp = folder->files; lp != NULL; lp = lp->next)
        {
            th_file_reload(lp->data);
        }

        // reload folder information too
        th_file_reload(folder->thunar_file);

        folder->reload_info = FALSE;
    }

    // we did it, the folder is loaded
    if (folder->job != NULL)
    {
        g_signal_handlers_disconnect_matched(folder->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             folder);
        g_object_unref(folder->job);
        folder->job = NULL;
    }

    // restart the content type loader
    _th_folder_content_type_loader(folder);

    // tell the consumers that we have loaded the directory
    g_object_notify(G_OBJECT(folder), "loading");
}

static gboolean _th_folder_files_ready(ThunarJob *job, GList *files,
                                       ThunarFolder *folder)
{
    e_return_val_if_fail(IS_THUNARFOLDER(folder), FALSE);
    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);

    // merge the list with the existing list of new files
    folder->new_files = g_list_concat(folder->new_files, files);

    // indicate that we took over ownership of the file list
    return TRUE;
}

// Content Type Loader --------------------------------------------------------

static void _th_folder_content_type_loader(ThunarFolder *folder)
{
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(folder->content_type_idle_id == 0);

    // set the pointer to the start of the list
    folder->type_list = folder->files;

    // schedule idle
    folder->content_type_idle_id =
            g_idle_add_full(G_PRIORITY_LOW,
                            _th_folder_content_type_loader_idle,
                            folder,
                            _th_folder_content_type_loader_idle_destroyed);
}

static gboolean _th_folder_content_type_loader_idle(gpointer data)
{
    e_return_val_if_fail(IS_THUNARFOLDER(data), FALSE);

    ThunarFolder *folder = THUNARFOLDER(data);

    for (GList *lp = folder->type_list; lp != NULL; lp = lp->next)
    {
        if (th_file_load_content_type(lp->data))
        {
            // if this was the last file, abort
            if (lp->next == NULL)
                break;

            // set pointer to next file for the next iteration
            folder->type_list = lp->next;

            return TRUE;
        }
    }

    // all content types loaded
    return FALSE;
}

static void _th_folder_content_type_loader_idle_destroyed(gpointer data)
{
    e_return_if_fail(IS_THUNARFOLDER(data));

    THUNARFOLDER(data)->content_type_idle_id = 0;
}

// Public ---------------------------------------------------------------------

ThunarFolder* th_folder_get_for_thfile(ThunarFile *file)
{
    e_return_val_if_fail(IS_THUNARFILE(file), NULL);

    // make sure the file is loaded
    if (!th_file_check_loaded(file))
        return NULL;

    e_return_val_if_fail(th_file_is_directory(file), NULL);

    // load if the file is not a folder
    if (!th_file_is_directory(file))
        return NULL;

    // determine the "thunar-folder" quark on-demand
    if (_th_folder_quark == 0)
        _th_folder_quark = g_quark_from_static_string("thunar-folder");

    // check if we already know that folder
    ThunarFolder *folder = g_object_get_qdata(G_OBJECT(file),
                                              _th_folder_quark);
    if (folder != NULL)
    {
        g_object_ref(G_OBJECT(folder));

        return folder;
    }

    // allocate the new instance
    folder = g_object_new(TYPE_THUNARFOLDER,
                          "corresponding-file", file,
                          NULL);

    // connect the folder to the file
    g_object_set_qdata(G_OBJECT(file), _th_folder_quark, folder);

    // schedule the loading of the folder
    th_folder_load(folder, FALSE);

    return folder;
}

ThunarFile* th_folder_get_thfile(const ThunarFolder *folder)
{
    e_return_val_if_fail(IS_THUNARFOLDER(folder), NULL);

    // call g_object_ref() to get a persistent reference

    return folder->thunar_file;
}

gboolean th_folder_has_folder_monitor(const ThunarFolder *folder)
{
    e_return_val_if_fail(IS_THUNARFOLDER(folder), FALSE);

    return (folder->gfilemonitor != NULL);
}

GList* th_folder_get_files(const ThunarFolder *folder)
{
    e_return_val_if_fail(IS_THUNARFOLDER(folder), NULL);

    // the returned list is owned by folder and must not be freed

    return folder->files;
}

static void _th_folder_move_monitor(ThunarFolder *folder)
{
    if (!folder->gfilemonitor)
        return;

    gchar *file_uri = th_file_get_uri(folder->thunar_file);

    if (file_uri && folder->monitor_uri
        && g_strcmp0(folder->monitor_uri, file_uri) != 0)
    {
        //printf("_monitor_on_changed : %s\n", file_uri);

        g_signal_handlers_disconnect_matched(folder->gfilemonitor,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, folder);
        g_file_monitor_cancel(folder->gfilemonitor);
        g_object_unref(folder->gfilemonitor);

        if (folder->monitor_uri)
            g_free(folder->monitor_uri);

        folder->monitor_uri = file_uri;
        file_uri = NULL;
        folder->gfilemonitor = g_file_monitor_directory(
                                    th_file_get_file(folder->thunar_file),
                                    G_FILE_MONITOR_WATCH_MOVES,
                                    NULL,
                                    NULL);

        if (folder->gfilemonitor)
        {
            g_signal_connect(folder->gfilemonitor, "changed",
                             G_CALLBACK(_gfmonitor_on_changed), folder);
        }
    }

    if (file_uri)
        g_free(file_uri);
}

// Monitor --------------------------------------------------------------------

// constructed
static void _gfmonitor_on_changed(GFileMonitor      *monitor,
                                  GFile             *event_file,
                                  GFile             *other_file,
                                  GFileMonitorEvent event_type,
                                  gpointer          user_data)
{
    ThunarFolder *folder = THUNARFOLDER(user_data);
    e_return_if_fail(G_IS_FILE_MONITOR(monitor));
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(folder->gfilemonitor == monitor);
    e_return_if_fail(IS_THUNARFILE(folder->thunar_file));
    e_return_if_fail(G_IS_FILE(event_file));

    //monitor_print_event(monitor, event_file, other_file,
    //                    event_type,
    //                    folder->thunar_file);

    if (g_file_equal(event_file, th_file_get_file(folder->thunar_file)))
    {
        // directory event
        if (event_type == G_FILE_MONITOR_EVENT_DELETED)
        {
            //printf("G_FILE_MONITOR_EVENT_DELETED\n");

            if (!th_file_exists(folder->thunar_file))
            {
                //printf("g_file_equal: th_file_destroy\n");

                th_file_destroy(folder->thunar_file);
            }
        }
        else
        {
            th_file_reload(folder->thunar_file);
        }

        return;
    }

    GList *lp;

    // check if we already ship the file
    for (lp = folder->files; lp != NULL; lp = lp->next)
    {
        if (g_file_equal(event_file, th_file_get_file(lp->data)))
            break;
    }

    gboolean restart = FALSE;

    // stop the content type collector
    if (folder->content_type_idle_id != 0)
        restart = g_source_remove(folder->content_type_idle_id);

    ThunarFile *file;
    GList list;

    // file added
    if (lp == NULL && event_type != G_FILE_MONITOR_EVENT_DELETED)
    {
        // allocate a file for the path
        file = th_file_get(event_file, NULL);

        if (file != NULL)
        {
            // prepend it to our internal list
            folder->files = g_list_prepend(folder->files, file);

            // tell others about the new file
            list.data = file;
            list.next = list.prev = NULL;
            g_signal_emit(G_OBJECT(folder),
                          _th_folder_signals[FILES_ADDED], 0, &list);

            // load the new file
            th_file_reload(file);
        }
    }

    //
    else if (lp != NULL)
    {
        if (event_type == G_FILE_MONITOR_EVENT_DELETED)
        {
            // destroy the file
            th_file_destroy(lp->data);

            // if the file has not been destroyed by now, reload it to invalidate it
            ThunarFile *destroyed = th_file_cache_lookup(event_file);
            if (destroyed != NULL)
            {
                th_file_reload(destroyed);
                g_object_unref(destroyed);
            }
        }
        else if (event_type == G_FILE_MONITOR_EVENT_RENAMED
                 || event_type == G_FILE_MONITOR_EVENT_MOVED_IN
                 || event_type == G_FILE_MONITOR_EVENT_MOVED_OUT)
        {
            // destroy the old file and update the new one
            th_file_destroy(lp->data);

            // rename
            if (other_file != NULL)
            {
                file = th_file_get(other_file, NULL);

                if (file != NULL && IS_THUNARFILE(file))
                {
                    th_file_reload(file);

                    /* if source and target folders are different, also tell
                       the target folder to reload for the changes */
                    if (th_file_has_parent(file))
                    {
                        // rename a directory
                        ThunarFile *other_parent;
                        other_parent = th_file_get_parent(file, NULL);
                        if (other_parent &&
                            !g_file_equal(th_file_get_file(folder->thunar_file),
                                          th_file_get_file(other_parent)))
                        {
                            th_file_reload(other_parent);
                            g_object_unref(other_parent);
                        }
                    }

                    // drop reference on the other file
                    g_object_unref(file);
                }
            }
        }
        else
        {

#if DEBUG_FILE_CHANGES
            _th_file_infos_equal(lp->data, event_file);
#endif

            th_file_reload(lp->data);
        }
    }

    // check if we need to restart the type loader
    if (restart)
        _th_folder_content_type_loader(folder);
}

// init
static void _monitor_on_changed(FileMonitor *file_monitor,
                                ThunarFile *file,
                                ThunarFolder *folder)
{
    e_return_if_fail(IS_THUNARFILE(file));
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(IS_FILEMONITOR(file_monitor));

    if (folder->thunar_file != file)
        return;

    // folder has changed

    //printf("_monitor_on_changed: %s\n", th_file_get_display_name(file));

    _th_folder_move_monitor(folder);

    th_folder_load(folder, FALSE);
}

static void _monitor_on_destroyed(FileMonitor *file_monitor,
                                  ThunarFile *file, ThunarFolder *folder)
{
    e_return_if_fail(IS_THUNARFILE(file));
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(IS_FILEMONITOR(file_monitor));

    // the directory was destroyed
    if (folder->thunar_file == file)
    {
        // the folder is useless now
        if (!folder->in_destruction)
            g_object_run_dispose(G_OBJECT(folder));

        return;
    }

    // check if we have that file
    GList *lp = g_list_find(folder->files, file);

    if (lp == NULL)
        return;

    gboolean restart = FALSE;

    if (folder->content_type_idle_id != 0)
        restart = g_source_remove(folder->content_type_idle_id);

    // remove the file from our list
    folder->files = g_list_delete_link(folder->files, lp);

    // tell everybody that the file is gone
    GList files;
    files.data = file;
    files.next = files.prev = NULL;
    g_signal_emit(G_OBJECT(folder),
                  _th_folder_signals[FILES_REMOVED], 0, &files);

    // drop our reference to the file
    g_object_unref(G_OBJECT(file));

    // continue collecting the content type
    if (restart)
        _th_folder_content_type_loader(folder);
}

// Debug ----------------------------------------------------------------------

#if DEBUG_FILE_CHANGES
static void _th_file_infos_equal(ThunarFile *file, GFile *event_file)
{
    gchar     **attrs;
    GFileInfo  *info1 = G_FILE_INFO(file->info);
    GFileInfo  *info2;
    guint       i;
    gchar      *attr1, *attr2;
    gboolean    printed = FALSE;
    gchar      *bname;

    attrs = g_file_info_list_attributes(info1, NULL);
    info2 = g_file_query_info(event_file, FILEINFO_NAMESPACE,
                               G_FILE_QUERY_INFO_NONE, NULL, NULL);

    if (info1 != NULL && info2 != NULL)
    {
        for(i = 0; attrs[i] != NULL; i++)
        {
            if (g_file_info_has_attribute(info2, attrs[i]))
            {
                attr1 = g_file_info_get_attribute_as_string(info1, attrs[i]);
                attr2 = g_file_info_get_attribute_as_string(info2, attrs[i]);

                if (g_strcmp0(attr1, attr2) != 0)
                {
                    if (!printed)
                    {
                        bname = g_file_get_basename(event_file);
                        g_print("%s\n", bname);
                        g_free(bname);

                        printed = TRUE;
                    }

                    g_print("  %s: %s -> %s\n", attrs[i], attr1, attr2);
                }

                g_free(attr1);
                g_free(attr2);
            }
        }

        g_object_unref(info2);
    }

    if (printed)
        g_print("\n");

    g_free(attrs);
}
#endif


