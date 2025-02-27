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

#include "config.h"
#include "transferjob.h"

#include "jobutils.h"
#include "io-scandir.h"
#include "gio-ext.h"
#include <syslog.h>

// 10 seconds before we show the transfer rate + remaining time
#define MINIMUM_TRANSFER_TIME (10 * G_USEC_PER_SEC)

// TransferNode ---------------------------------------------------------------

typedef struct _TransferNode TransferNode;
static void _transfernode_free(gpointer data);

// TransferJob ----------------------------------------------------------------

static void transferjob_finalize(GObject *object);
static void transferjob_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec);
static void transferjob_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec);

// Job Execute function -------------------------------------------------------

static gboolean transferjob_execute(ExoJob *job, GError **error);
static void _transferjob_check_pause(TransferJob *job);
static gboolean _transferjob_verify_destination(TransferJob *transfer_job,
                                                GError **error);
static gboolean _transferjob_collect_node(TransferJob *job, TransferNode *node,
                                          GError **error);
static void _transferjob_copy_node(TransferJob  *job,
                                   TransferNode *node,
                                   GFile        *target_file,
                                   GFile        *target_parent_file,
                                   GList        **target_file_list_return,
                                   GError       **error);
static void _transferjob_freeze_optional(TransferJob *transfer_job);
static void _transferjob_fill_source_device_info(TransferJob *transfer_job,
                                                 GFile *file);
static void _transferjob_fill_target_device_info(TransferJob *transfer_job,
                                                 GFile *file);
static gboolean _transferjob_is_file_on_local_device(GFile *file);
static void _transferjob_determine_copy_behavior(
                                TransferJob *transfer_job,
                                gboolean *freeze_if_src_busy_p,
                                gboolean *freeze_if_tgt_busy_p,
                                gboolean *always_parallel_copy_p,
                                gboolean *should_freeze_on_any_other_job_p);
static GList* _transferjob_filter_running_jobs(GList *jobs, ThunarJob *own_job);
static gboolean _transferjob_device_id_in_job_list(const char *device_fs_id,
                                                   GList *jobs);

// Actions --------------------------------------------------------------------

static gboolean _transferjob_prepare_untrash_file(ExoJob *job, GFileInfo *info,
                                                  GFile *file, GError **error);
static gboolean _transferjob_move_file(ExoJob *job, GFileInfo *info, GList *sp,
                                       TransferNode *node, GList *tp,
                                       GFileCopyFlags move_flags,
                                       GList **new_files_list_p,
                                       GError **error);
static gboolean _transferjob_move_file_with_rename(ExoJob *job,
                                                   TransferNode *node,
                                                   GList *tp,
                                                   GFileCopyFlags flags,
                                                   GError **error);
static GFile* _transferjob_copy_file(TransferJob *job,
                                     GFile *source_file, GFile *target_file,
                                     gboolean replace_confirmed,
                                     gboolean rename_confirmed, GError **error);
static gboolean _transferjob_copy_file_real(TransferJob *job,
                                            GFile *source_file,
                                            GFile *target_file,
                                            GFileCopyFlags copy_flags,
                                            gboolean merge_directories,
                                            GError **error);
static void _transferjob_progress(goffset current_num_bytes,
                                  goffset total_num_bytes, gpointer user_data);

// TransferNode ---------------------------------------------------------------

struct _TransferNode
{
    TransferNode    *next;
    TransferNode    *children;
    GFile           *source_file;
    gboolean        replace_confirmed;
    gboolean        rename_confirmed;
};

static void _transfernode_free(gpointer data)
{
    TransferNode *node = data;
    TransferNode *next;

    // free all nodes in a row
    while (node != NULL)
    {
        // free all children of this node
        _transfernode_free(node->children);

        // determine the next node
        next = node->next;

        // drop the source file of this node
        g_object_unref(node->source_file);

        // release the resources of this node
        g_slice_free(TransferNode, node);

        // continue with the next node
        node = next;
    }
}

// TransferJob ----------------------------------------------------------------

enum
{
    PROP_0,
    PROP_FILE_SIZE_BINARY,
    PROP_PARALLEL_COPY_MODE,
};

struct _TransferJobClass
{
    ThunarJobClass __parent__;
};

struct _TransferJob
{
    ThunarJob   __parent__;

    TransferJobType type;

    GList       *source_node_list;
    gchar       *source_device_fs_id;
    gboolean    is_source_device_local;
    GList       *target_file_list;
    gchar       *target_device_fs_id;
    gboolean    is_target_device_local;

    gint64      start_time;
    gint64      last_update_time;
    guint64     last_total_progress;

    guint64     total_size;
    guint64     total_progress;
    guint64     file_progress;
    guint64     transfer_rate;

    gboolean    file_size_binary;

    ParallelCopyMode parallel_copy_mode;
};

G_DEFINE_TYPE(TransferJob, transferjob, TYPE_THUNARJOB)

static void transferjob_class_init(TransferJobClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = transferjob_finalize;
    gobject_class->get_property = transferjob_get_property;
    gobject_class->set_property = transferjob_set_property;

    ExoJobClass *exojob_class = EXOJOB_CLASS(klass);
    exojob_class->execute = transferjob_execute;

    g_object_class_install_property(gobject_class,
                                    PROP_FILE_SIZE_BINARY,
                                    g_param_spec_boolean(
                                        "file-size-binary",
                                        "FileSizeBinary",
                                        NULL,
                                        TRUE,
                                        E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_PARALLEL_COPY_MODE,
                                    g_param_spec_enum(
                                        "parallel-copy-mode",
                                        "ParallelCopyMode",
                                        NULL,
                                        TYPE_PARALLEL_COPY_MODE,
                                        PARALLEL_COPY_MODE_ONLY_LOCAL,
                                        E_PARAM_READWRITE));
}

static void transferjob_init(TransferJob *job)
{
    job->type = 0;
    job->source_node_list = NULL;
    job->source_device_fs_id = NULL;
    job->is_source_device_local = FALSE;
    job->target_file_list = NULL;
    job->target_device_fs_id = NULL;
    job->is_target_device_local = FALSE;
    job->total_size = 0;
    job->total_progress = 0;
    job->file_progress = 0;
    job->last_update_time = 0;
    job->last_total_progress = 0;
    job->transfer_rate = 0;
    job->start_time = 0;
}

static void transferjob_finalize(GObject *object)
{
    TransferJob *job = TRANSFERJOB(object);

    g_list_free_full(job->source_node_list, _transfernode_free);

    if (job->source_device_fs_id != NULL)
        g_free(job->source_device_fs_id);

    if (job->target_device_fs_id != NULL)
        g_free(job->target_device_fs_id);

    e_list_free(job->target_file_list);

    G_OBJECT_CLASS(transferjob_parent_class)->finalize(object);
}

static void transferjob_get_property(GObject *object, guint prop_id,
                                     GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    TransferJob *job = TRANSFERJOB(object);

    switch (prop_id)
    {
    case PROP_FILE_SIZE_BINARY:
        g_value_set_boolean(value, job->file_size_binary);
        break;

    case PROP_PARALLEL_COPY_MODE:
        g_value_set_enum(value, job->parallel_copy_mode);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void transferjob_set_property(GObject *object, guint prop_id,
                                     const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    TransferJob *job = TRANSFERJOB(object);

    switch (prop_id)
    {
    case PROP_FILE_SIZE_BINARY:
        job->file_size_binary = g_value_get_boolean(value);
        break;

    case PROP_PARALLEL_COPY_MODE:
        job->parallel_copy_mode = g_value_get_enum(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Job Execute function -------------------------------------------------------

static gboolean transferjob_execute(ExoJob *job, GError **error)
{
    TransferNode   *node;
    TransferJob    *transfer_job = TRANSFERJOB(job);
    GFileInfo            *info;
    GError               *err = NULL;
    GList                *new_files_list = NULL;
    GList                *snext;
    GList                *sp;
    GList                *tnext;
    GList                *tp;

    e_return_val_if_fail(IS_TRANSFERJOB(job), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (exo_job_set_error_if_cancelled(job, error))
        return FALSE;

    exo_job_info_message(job, _("Collecting files..."));

    for(sp = transfer_job->source_node_list, tp = transfer_job->target_file_list;
            sp != NULL && tp != NULL && err == NULL;
            sp = snext, tp = tnext)
    {
        _transferjob_check_pause(transfer_job);

        // determine the next list items
        snext = sp->next;
        tnext = tp->next;

        // determine the current source transfer node
        node = sp->data;

        info = g_file_query_info(node->source_file,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  exo_job_get_cancellable(job),
                                  &err);

        if (info == NULL)
            break;

        // check if we are moving a file out of the trash
        if (transfer_job->type == TRANSFERJOB_MOVE
            && e_file_is_trashed(node->source_file))
        {
            if (!_transferjob_prepare_untrash_file(job, info, tp->data, &err))
                break;

            if (!_transferjob_move_file(job,
                        info,
                        sp,
                        node,
                        tp,
                        G_FILE_COPY_NOFOLLOW_SYMLINKS | G_FILE_COPY_ALL_METADATA,
                        &new_files_list, &err))
                break;
        }

        else if (transfer_job->type == TRANSFERJOB_MOVE)
        {
            if (!_transferjob_move_file(job,
                        info,
                        sp,
                        node,
                        tp,
                        G_FILE_COPY_NOFOLLOW_SYMLINKS
                            | G_FILE_COPY_NO_FALLBACK_FOR_MOVE
                            | G_FILE_COPY_ALL_METADATA,
                        &new_files_list,
                        &err))
                break;
        }

        else if (transfer_job->type == TRANSFERJOB_COPY)
        {
            if (!_transferjob_collect_node(TRANSFERJOB(job), node, &err))
                break;
        }

        g_object_unref(info);
    }

    // continue if there were no errors yet
    if (err == NULL)
    {
        // check destination
        if (!_transferjob_verify_destination(transfer_job, &err))
        {
            if (err != NULL)
            {
                g_propagate_error(error, err);
                return FALSE;
            }
            else
            {
                // pretend nothing happened
                return TRUE;
            }
        }

        _transferjob_freeze_optional(transfer_job);

        // transfer starts now
        transfer_job->start_time = g_get_real_time();

        // perform the copy recursively for all source transfer nodes
        for (sp = transfer_job->source_node_list, tp = transfer_job->target_file_list;
             sp != NULL && tp != NULL && err == NULL;
             sp = sp->next, tp = tp->next)
        {
            _transferjob_copy_node(transfer_job,
                                   sp->data,
                                   tp->data,
                                   NULL,
                                   &new_files_list,
                                   &err);
        }
    }

    // check if we failed
    if (err != NULL)
    {
        g_propagate_error(error, err);

        return FALSE;
    }

    job_new_files(THUNAR_JOB(job), new_files_list);
    e_list_free(new_files_list);

    return TRUE;
}

static void _transferjob_check_pause(TransferJob *job)
{
    e_return_if_fail(IS_TRANSFERJOB(job));

    while (job_is_paused(THUNAR_JOB(job))
           && !exo_job_is_cancelled(EXOJOB(job)))
    {
        g_usleep(500 * 1000); // 500ms pause
    }
}

static gboolean _transferjob_verify_destination(TransferJob *transfer_job,
                                                 GError **error)
{
    (void) error;

    GFileInfo         *filesystem_info;
    guint64            free_space;
    GFile             *dest;
    GFileInfo         *dest_info;
    gchar             *dest_name = NULL;
    gchar             *base_name;
    gboolean           succeed = TRUE;
    gchar             *size_string;

    e_return_val_if_fail(IS_TRANSFERJOB(transfer_job), FALSE);

    // no target file list
    if (transfer_job->target_file_list == NULL)
        return TRUE;

    // total size is nul, should be fine
    if (transfer_job->total_size == 0)
        return TRUE;

    /* for all actions in thunar use the same target directory so
     * although not all files are checked, this should work nicely */
    dest = g_file_get_parent(G_FILE(transfer_job->target_file_list->data));

    // query information about the filesystem
    filesystem_info = g_file_query_filesystem_info(dest, FILESYSTEM_INFO_NAMESPACE,
                      exo_job_get_cancellable(EXOJOB(transfer_job)),
                      NULL);

    // unable to query the info, this could happen on some backends
    if (filesystem_info == NULL)
    {
        g_object_unref(G_OBJECT(dest));
        return TRUE;
    }

    // some info about the file
    dest_info = g_file_query_info(dest, G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME, 0,
                                   exo_job_get_cancellable(EXOJOB(transfer_job)),
                                   NULL);
    if (dest_info != NULL)
    {
        dest_name = g_strdup(g_file_info_get_display_name(dest_info));
        g_object_unref(G_OBJECT(dest_info));
    }

    if (dest_name == NULL)
    {
        base_name = g_file_get_basename(dest);
        dest_name = g_filename_display_name(base_name);
        g_free(base_name);
    }

    if (g_file_info_has_attribute(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE))
    {
        free_space = g_file_info_get_attribute_uint64(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        if (transfer_job->total_size > free_space)
        {
            size_string = g_format_size_full(transfer_job->total_size - free_space,
                                              transfer_job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
            succeed = job_ask_no_size(THUNAR_JOB(transfer_job),
                                              _("Error while copying to \"%s\": %s more space is "
                                                "required to copy to the destination"),
                                              dest_name, size_string);
            g_free(size_string);
        }
    }

    /* We used to check G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE here,
     * but it's not completely reliable, especially on remote file systems.
     * The downside is that operations on read-only files fail with a generic
     * error message.
     * More details: https://bugzilla.xfce.org/show_bug.cgi?id=15367#c16 */

    g_object_unref(filesystem_info);
    g_object_unref(G_OBJECT(dest));
    g_free(dest_name);

    return succeed;
}

static gboolean _transferjob_collect_node(TransferJob *job, TransferNode *node,
                                          GError **error)
{
    e_return_val_if_fail(IS_TRANSFERJOB(job), FALSE);
    e_return_val_if_fail(node != NULL && G_IS_FILE(node->source_file), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return FALSE;

    GError             *err = NULL;
    GFileInfo *info = g_file_query_info(
                                node->source_file,
                                G_FILE_ATTRIBUTE_STANDARD_SIZE ","
                                G_FILE_ATTRIBUTE_STANDARD_TYPE,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                exo_job_get_cancellable(EXOJOB(job)),
                                &err);

    if (info == NULL)
        return FALSE;

    job->total_size += g_file_info_get_size(info);

    // check if we have a directory here
    if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
    {
        // scan the directory for immediate children
        GList *file_list = io_scan_directory(
                                    THUNAR_JOB(job),
                                    node->source_file,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    FALSE, FALSE, FALSE,
                                    &err);

        // add children to the transfer node
        for (GList *lp = file_list; err == NULL && lp != NULL; lp = lp->next)
        {
            _transferjob_check_pause(job);

            // allocate a new transfer node for the child
            TransferNode *child_node = g_slice_new0(TransferNode);

            child_node->source_file = g_object_ref(lp->data);
            child_node->replace_confirmed = node->replace_confirmed;
            child_node->rename_confirmed = FALSE;

            // hook the child node into the child list
            child_node->next = node->children;
            node->children = child_node;

            // collect the child node
            _transferjob_collect_node(job, child_node, &err);
        }

        // release the child files
        e_list_free(file_list);
    }

    // release file info
    g_object_unref(info);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    return TRUE;
}

static void _transferjob_copy_node(TransferJob  *job,
                                    TransferNode *node,
                                    GFile        *target_file,
                                    GFile        *target_parent_file,
                                    GList        **target_file_list_return,
                                    GError       **error)
{
    ThunarJobResponse     response;
    GFileInfo            *info;
    GError               *err = NULL;
    GFile                *real_target_file = NULL;
    gchar                *base_name;

    e_return_if_fail(IS_TRANSFERJOB(job));
    e_return_if_fail(node != NULL && G_IS_FILE(node->source_file));
    e_return_if_fail(target_file == NULL || node->next == NULL);
    e_return_if_fail((target_file == NULL && target_parent_file != NULL) ||(target_file != NULL && target_parent_file == NULL));
    e_return_if_fail(error == NULL || *error == NULL);

    /* The caller can either provide a target_file or a target_parent_file, but not both. The toplevel
     * transfer_nodes(for which next is NULL) should be called with target_file, to get proper behavior
     * wrt restoring files from the trash. Other transfer_nodes will be called with target_parent_file.
     */

    for (; err == NULL && node != NULL; node = node->next)
    {
        // guess the target file for this node(unless already provided)
        if (target_file == NULL)
        {
            base_name = g_file_get_basename(node->source_file);
            target_file = g_file_get_child(target_parent_file, base_name);
            g_free(base_name);
        }
        else
            target_file = g_object_ref(target_file);

        // query file info
        info = g_file_query_info(node->source_file,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  exo_job_get_cancellable(EXOJOB(job)),
                                  &err);

        // abort on error or cancellation
        if (info == NULL)
        {
            g_object_unref(target_file);
            break;
        }

        // update progress information
        exo_job_info_message(EXOJOB(job), "%s", g_file_info_get_display_name(info));

retry_copy:
        _transferjob_check_pause(job);

        // copy the item specified by this node(not recursively)
        real_target_file = _transferjob_copy_file(job, node->source_file,
                           target_file,
                           node->replace_confirmed,
                           node->rename_confirmed,
                           &err);

        if (real_target_file != NULL)
        {
            // node->source_file == real_target_file means to skip the file
            if (node->source_file != real_target_file)
            {
                // check if we have children to copy
                if (node->children != NULL)
                {
                    // copy all children of this node
                    _transferjob_copy_node(job, node->children, NULL, real_target_file, NULL, &err);

                    // free resources allocted for the children
                    _transfernode_free(node->children);
                    node->children = NULL;
                }

                // check if the child copy failed
                if (err != NULL)
                {
                    // outa here, freeing the target paths
                    g_object_unref(real_target_file);
                    g_object_unref(target_file);
                    break;
                }

                // add the real target file to the return list
                if (target_file_list_return != NULL)
                {
                    *target_file_list_return =
                        e_list_prepend_ref(*target_file_list_return,
                                                    real_target_file);
                }

retry_remove:
                _transferjob_check_pause(job);

                // try to remove the source directory if we are on copy+remove fallback for move
                if (job->type == TRANSFERJOB_MOVE)
                {
                    if (!g_file_delete(node->source_file,
                                        exo_job_get_cancellable(EXOJOB(job)),
                                        &err))
                    {
                        // ask the user to retry
                        response = job_ask_skip(THUNAR_JOB(job), "%s",
                                                        err->message);

                        // reset the error
                        g_clear_error(&err);

                        // check whether to retry
                        if (response == THUNAR_JOB_RESPONSE_RETRY)
                            goto retry_remove;
                    }
                }
            }

            g_object_unref(real_target_file);
        }
        else if (err != NULL)
        {
            // we can only skip if there is space left on the device
            if (err->domain != G_IO_ERROR || err->code != G_IO_ERROR_NO_SPACE)
            {
                // ask the user to skip this node and all subnodes
                response = job_ask_skip(THUNAR_JOB(job), "%s", err->message);

                // reset the error
                g_clear_error(&err);

                // check whether to retry
                if (response == THUNAR_JOB_RESPONSE_RETRY)
                    goto retry_copy;
            }
        }

        // release the guessed target file
        g_object_unref(target_file);
        target_file = NULL;

        // release file info
        g_object_unref(info);
    }

    // propagate error if we failed or the job was cancelled
    if (err != NULL)
        g_propagate_error(error, err);
}

/**
 * thunar_transferjob_freeze_optional:
 * @job : a #TransferJob.
 *
 * Based on thunar setting, will block until all running jobs
 * doing IO on the source files or target files devices are completed.
 * The unblocking could be forced by the user in the UI.
 *
 **/
static void _transferjob_freeze_optional(TransferJob *transfer_job)
{
    gboolean            freeze_if_src_busy;
    gboolean            freeze_if_tgt_busy;
    gboolean            always_parallel_copy;
    gboolean            should_freeze_on_any_other_job;
    gboolean            been_frozen;

    e_return_if_fail(IS_TRANSFERJOB(transfer_job));

    // no source node list nor target file list
    if (transfer_job->source_node_list == NULL || transfer_job->target_file_list == NULL)
        return;
    // first source file
    _transferjob_fill_source_device_info(transfer_job,
                                         ((TransferNode*) transfer_job->source_node_list->data)->source_file);
    // first target file
    _transferjob_fill_target_device_info(transfer_job, G_FILE(transfer_job->target_file_list->data));
    _transferjob_determine_copy_behavior(transfer_job,
            &freeze_if_src_busy,
            &freeze_if_tgt_busy,
            &always_parallel_copy,
            &should_freeze_on_any_other_job);
    if (always_parallel_copy)
        return;

    been_frozen = FALSE; // this boolean can only take the TRUE value once.
    while(TRUE)
    {
        GList *jobs = job_ask_jobs(THUNAR_JOB(transfer_job));
        GList *other_jobs = _transferjob_filter_running_jobs(jobs, THUNAR_JOB(transfer_job));
        g_list_free(g_steal_pointer(&jobs));
        if
       (
            // should freeze because another job is running
           (should_freeze_on_any_other_job && other_jobs != NULL) ||
            // should freeze because source is busy and source device id appears in another job
           (freeze_if_src_busy && _transferjob_device_id_in_job_list(transfer_job->source_device_fs_id, other_jobs)) ||
            // should freeze because target is busy and target device id appears in another job
           (freeze_if_tgt_busy && _transferjob_device_id_in_job_list(transfer_job->target_device_fs_id, other_jobs))
        )
            g_list_free(g_steal_pointer(&other_jobs));
        else
        {
            g_list_free(g_steal_pointer(&other_jobs));
            break;
        }
        if (exo_job_is_cancelled(EXOJOB(transfer_job)))
            break;
        if (!job_is_frozen(THUNAR_JOB(transfer_job)))
        {
            if (been_frozen)
                break; // cannot re-freeze. It means that the user force to unfreeze
            else
            {
                been_frozen = TRUE; // first time here. The job needs to change to frozen state
                job_freeze(THUNAR_JOB(transfer_job));
            }
        }
        g_usleep(500 * 1000); // pause for 500ms
    }
    if (job_is_frozen(THUNAR_JOB(transfer_job)))
        job_unfreeze(THUNAR_JOB(transfer_job));
}

static void _transferjob_fill_source_device_info(TransferJob *transfer_job,
                                                 GFile       *file)
{
    /* query device filesystem id(unique string)
     * The source exists and can be queried directly. */
    GFileInfo *file_info = g_file_query_info(file,
                           G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                           G_FILE_QUERY_INFO_NONE,
                           exo_job_get_cancellable(EXOJOB(transfer_job)),
                           NULL);
    if (file_info != NULL)
    {
        transfer_job->source_device_fs_id = g_strdup(g_file_info_get_attribute_string(file_info, G_FILE_ATTRIBUTE_ID_FILESYSTEM));
        g_object_unref(file_info);
    }
    transfer_job->is_source_device_local = _transferjob_is_file_on_local_device(file);
}

static void _transferjob_fill_target_device_info(TransferJob *transfer_job,
                                                  GFile       *file)
{
    /*
     * To query the device id it should be done on an existing file/directory.
     * Usually the target file does not exist yet and so the parent directory
     * will be queried, and so on until reaching root directory if necessary.
     * Normally it will end in the worst case to the mounted filesystem root,
     * because that always exists. */
    GFile     *target_file = g_object_ref(file); // start with target file
    GFile     *target_parent;
    GFileInfo *file_info;
    while(target_file != NULL)
    {
        // query device id
        file_info = g_file_query_info(target_file,
                                       G_FILE_ATTRIBUTE_ID_FILESYSTEM,
                                       G_FILE_QUERY_INFO_NONE,
                                       exo_job_get_cancellable(EXOJOB(transfer_job)),
                                       NULL);
        if (file_info != NULL)
        {
            transfer_job->target_device_fs_id = g_strdup(
                        g_file_info_get_attribute_string(file_info,
                                                         G_FILE_ATTRIBUTE_ID_FILESYSTEM));
            g_object_unref(file_info);
            break;
        }
        else // target file or parent directory does not exist(yet)
        {
            // query the parent directory
            target_parent = g_file_get_parent(target_file);
            g_object_unref(target_file);
            target_file = target_parent;
        }
    }
    g_object_unref(target_file);
    transfer_job->is_target_device_local =
            _transferjob_is_file_on_local_device(file);
}

/**
 * thunar_transferjob_is_file_on_local_device:
 * @file : the source or target #GFile to test.
 *
 * Tries to find if the @file is on a local device or not.
 * Local device if (all conditions should match):
 * - the file has a 'file' uri scheme.
 * - the file is located on devices not handled by the #GVolumeMonitor(GVFS).
 * - the device is handled by #GVolumeMonitor(GVFS) and cannot be unmounted
 *  (USB key/disk, fuse mounts, Samba shares, PTP devices).
 *
 * The target @file may not exist yet when this function is used, so recurse
 * the parent directory, possibly reaching the root mountpoint.
 *
 * This should be enough to determine if a @file is on a local device or not.
 *
 * Return value: %TRUE if #GFile @file is on a so-called local device.
 **/
static gboolean _transferjob_is_file_on_local_device(GFile *file)
{
    gboolean  is_local;
    GFile    *target_file;
    GFile    *target_parent;
    GMount   *file_mount;

    e_return_val_if_fail(file != NULL, TRUE);

    if (g_file_has_uri_scheme(file, "file") == FALSE)
        return FALSE;

    target_file = g_object_ref(file); // start with file
    is_local = FALSE;

    while (target_file != NULL)
    {
        if (g_file_query_exists(target_file, NULL))
        {
            // file_mount will be NULL for local files on local partitions/devices
            file_mount = g_file_find_enclosing_mount(target_file, NULL, NULL);

            if (file_mount == NULL)
                is_local = TRUE;
            else
            {
                /* mountpoints which cannot be unmounted are local devices.
                 * attached devices like USB key/disk, fuse mounts, Samba shares,
                 * PTP devices can always be unmounted and are considered remote/slow. */
                is_local = ! g_mount_can_unmount(file_mount);
                g_object_unref(file_mount);
            }
            break;
        }
        else // file or parent directory does not exist(yet)
        {
            // query the parent directory
            target_parent = g_file_get_parent(target_file);
            g_object_unref(target_file);
            target_file = target_parent;
        }
    }
    g_object_unref(target_file);

    return is_local;
}

static void _transferjob_determine_copy_behavior(
                                    TransferJob *transfer_job,
                                    gboolean *freeze_if_src_busy_p,
                                    gboolean *freeze_if_tgt_busy_p,
                                    gboolean *always_parallel_copy_p,
                                    gboolean *should_freeze_on_any_other_job_p)
{
    *freeze_if_src_busy_p = FALSE;
    *freeze_if_tgt_busy_p = FALSE;
    *should_freeze_on_any_other_job_p = FALSE;
    if (transfer_job->parallel_copy_mode == PARALLEL_COPY_MODE_ALWAYS)
        // never freeze, always parallel copies
        *always_parallel_copy_p = TRUE;
    else if (transfer_job->parallel_copy_mode == PARALLEL_COPY_MODE_ONLY_LOCAL)
    {
        /* always parallel copy if:
         * - source device is local
         * AND
         * - target device is local
         */
        *always_parallel_copy_p = transfer_job->is_source_device_local && transfer_job->is_target_device_local;
        *freeze_if_src_busy_p = ! transfer_job->is_source_device_local;
        *freeze_if_tgt_busy_p = ! transfer_job->is_target_device_local;
    }
    else if (transfer_job->parallel_copy_mode == PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES)
    {
        /* always parallel copy if:
         * - source and target device fs are identical
         * AND
         * - source device is local
         * AND
         * - target device is local
         */
        /* freeze copy if
         * - src device fs ≠ tgt device fs and src or tgt appears in another job
         * OR
         * - src device is not local and src device appears in another job
         * OR
         * - tgt device is not local and tgt device appears in another job
         */
        if (g_strcmp0(transfer_job->source_device_fs_id, transfer_job->target_device_fs_id) != 0)
        {
            *always_parallel_copy_p = FALSE;
            // freeze when either src or tgt device appears on another job
            *freeze_if_src_busy_p = TRUE;
            *freeze_if_tgt_busy_p = TRUE;
        }
        else // same as PARALLEL_COPY_MODE_ONLY_LOCAL
        {
            *always_parallel_copy_p = transfer_job->is_source_device_local && transfer_job->is_target_device_local;
            *freeze_if_src_busy_p = ! transfer_job->is_source_device_local;
            *freeze_if_tgt_busy_p = ! transfer_job->is_target_device_local;
        }
    }
    else // PARALLEL_COPY_MODE_NEVER
    {
        // freeze copy if another transfer job is running
        *always_parallel_copy_p = FALSE;
        *should_freeze_on_any_other_job_p = TRUE;
    }
}

static GList* _transferjob_filter_running_jobs(GList *jobs, ThunarJob *own_job)
{
    ThunarJob *job;
    GList     *run_jobs = NULL;

    e_return_val_if_fail(IS_TRANSFERJOB(own_job), NULL);

    for(GList *ljobs = jobs; ljobs != NULL; ljobs = ljobs->next)
    {
        job = ljobs->data;
        if (job == own_job)
            continue;
        if (!exo_job_is_cancelled(EXOJOB(job)) && !job_is_paused(job) && !job_is_frozen(job))
        {
            run_jobs = g_list_append(run_jobs, job);
        }
    }

    return run_jobs;
}

static gboolean _transferjob_device_id_in_job_list(const char *device_fs_id,
                                                   GList      *jobs)
{
    TransferJob *job;

    for(GList *ljobs = jobs; device_fs_id != NULL && ljobs != NULL; ljobs = ljobs->next)
    {
        if (IS_TRANSFERJOB(ljobs->data))
        {
            job = TRANSFERJOB(ljobs->data);
            if (g_strcmp0(device_fs_id, job->source_device_fs_id) == 0)
                return TRUE;
            if (g_strcmp0(device_fs_id, job->target_device_fs_id) == 0)
                return TRUE;
        }
    }
    return FALSE;
}

// Public ---------------------------------------------------------------------

ThunarJob* transferjob_new(GList *source_node_list, GList *target_file_list,
                           TransferJobType type)
{
    e_return_val_if_fail(source_node_list != NULL, NULL);
    e_return_val_if_fail(target_file_list != NULL, NULL);
    e_return_val_if_fail(g_list_length(source_node_list) == g_list_length(target_file_list), NULL);

    TransferJob *job = g_object_new(TYPE_TRANSFERJOB, NULL);
    job->type = type;

    GList *sp;
    GList *tp;

    // add a transfer node for each source path and a matching target parent path
    for (sp = source_node_list, tp = target_file_list;
            sp != NULL;
            sp = sp->next, tp = tp->next)
    {
        // make sure we don't transfer root directories. this should be prevented in the GUI
        if (e_file_is_root(sp->data) || e_file_is_root(tp->data))
            continue;

        // only process non-equal pairs unless we're copying
        if (type != TRANSFERJOB_MOVE || !g_file_equal(sp->data, tp->data))
        {
            // append transfer node for this source file
            TransferNode *node = g_slice_new0(TransferNode);
            node->source_file = g_object_ref(sp->data);
            node->replace_confirmed = FALSE;
            node->rename_confirmed = FALSE;
            job->source_node_list = g_list_append(job->source_node_list, node);

            // append target file
            job->target_file_list = e_list_append_ref(job->target_file_list, tp->data);
        }
    }

    // make sure we didn't mess things up
    e_assert(g_list_length(job->source_node_list) == g_list_length(job->target_file_list));

    return THUNAR_JOB(job);
}

gchar* transferjob_get_status(TransferJob *job)
{
    gchar             *total_size_str;
    gchar             *total_progress_str;
    gchar             *transfer_rate_str;
    GString           *status;
    gulong             remaining_time;

    e_return_val_if_fail(IS_TRANSFERJOB(job), NULL);

    status = g_string_sized_new(100);

    // transfer status like "22.6MB of 134.1MB"
    total_size_str = g_format_size_full(job->total_size, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
    total_progress_str = g_format_size_full(job->total_progress, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
    g_string_append_printf(status, _("%s of %s"), total_progress_str, total_size_str);
    g_free(total_size_str);
    g_free(total_progress_str);

    // show time and transfer rate after 10 seconds
    if (job->transfer_rate > 0
            &&(job->last_update_time - job->start_time) > MINIMUM_TRANSFER_TIME)
    {
        // remaining time based on the transfer speed
        transfer_rate_str = g_format_size_full(job->transfer_rate, job->file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
        remaining_time =(job->total_size - job->total_progress) / job->transfer_rate;

        if (remaining_time > 0)
        {
            // insert long dash
            g_string_append(status, " \xE2\x80\x94 ");

            if (remaining_time > 60 * 60)
            {
                remaining_time =(gulong)(remaining_time /(60 * 60));
                g_string_append_printf(status, ngettext("%lu hour remaining(%s/sec)",
                                        "%lu hours remaining(%s/sec)",
                                        remaining_time),
                                        remaining_time, transfer_rate_str);
            }
            else if (remaining_time > 60)
            {
                remaining_time =(gulong)(remaining_time / 60);
                g_string_append_printf(status, ngettext("%lu minute remaining(%s/sec)",
                                        "%lu minutes remaining(%s/sec)",
                                        remaining_time),
                                        remaining_time, transfer_rate_str);
            }
            else
            {
                g_string_append_printf(status, ngettext("%lu second remaining(%s/sec)",
                                        "%lu seconds remaining(%s/sec)",
                                        remaining_time),
                                        remaining_time, transfer_rate_str);
            }
        }

        g_free(transfer_rate_str);
    }

    return g_string_free(status, FALSE);
}

// Actions --------------------------------------------------------------------

static gboolean _transferjob_prepare_untrash_file(ExoJob *job, GFileInfo *info,
                                                  GFile *file, GError **error)
{
    ThunarJobResponse  response;
    GFile             *target_parent;
    gboolean           parent_exists;
    gchar             *base_name;
    gchar             *parent_display_name;

    // update progress information
    exo_job_info_message(job, _("Trying to restore \"%s\""),
                          g_file_info_get_display_name(info));

    // determine the parent file
    target_parent = g_file_get_parent(file);
    // check if the parent exists
    if (target_parent != NULL)
        parent_exists = g_file_query_exists(target_parent, exo_job_get_cancellable(job));
    else
        parent_exists = FALSE;

    // abort on cancellation
    if (exo_job_set_error_if_cancelled(job, error))
    {
        g_object_unref(info);
        if (target_parent != NULL)
            g_object_unref(target_parent);
        return FALSE;
    }

    if (target_parent != NULL && !parent_exists)
    {
        // determine the display name of the parent
        base_name = g_file_get_basename(target_parent);
        parent_display_name = g_filename_display_name(base_name);
        g_free(base_name);

        // ask the user whether he wants to create the parent folder because its gone
        response = job_ask_create(THUNAR_JOB(job),
                                          _("The folder \"%s\" does not exist anymore but is "
                                            "required to restore the file \"%s\" from the "
                                            "trash"),
                                          parent_display_name,
                                          g_file_info_get_display_name(info));

        // abort if cancelled
        if (response == THUNAR_JOB_RESPONSE_CANCEL)
        {
            g_free(parent_display_name);
            g_object_unref(info);
            if (target_parent != NULL)
                g_object_unref(target_parent);
            return FALSE;
        }

        // try to create the parent directory
        if (!g_file_make_directory_with_parents(target_parent,
                exo_job_get_cancellable(job),
                error))
        {
            if (!exo_job_is_cancelled(job))
            {
                g_clear_error(error);

                // overwrite the internal GIO error with something more user-friendly
                g_set_error(error, G_IO_ERROR, G_IO_ERROR_FAILED,
                             _("Failed to restore the folder \"%s\""),
                             parent_display_name);
            }

            g_free(parent_display_name);
            g_object_unref(info);
            if (target_parent != NULL)
                g_object_unref(target_parent);
            return FALSE;
        }

        // clean up
        g_free(parent_display_name);
    }

    if (target_parent != NULL)
        g_object_unref(target_parent);
    return TRUE;
}

static gboolean _transferjob_move_file(ExoJob         *job,
                                       GFileInfo      *info,
                                       GList          *sp,
                                       TransferNode   *node,
                                       GList          *tp,
                                       GFileCopyFlags move_flags,
                                       GList          **new_files_list_p,
                                       GError         **error)
{
    TransferJob *transfer_job = TRANSFERJOB(job);
    ThunarJobResponse  response;
    gboolean           move_successful;

    // update progress information
    exo_job_info_message(job, _("Trying to move \"%s\""),
                          g_file_info_get_display_name(info));

    move_successful = e_file_move(node->source_file,
                                   tp->data,
                                   move_flags,
                                   exo_job_get_cancellable(job),
                                   NULL, NULL, error);

    // if the file already exists, ask the user if they want to overwrite, rename or skip it
    if (!move_successful && (*error)->code == G_IO_ERROR_EXISTS)
    {
        g_clear_error(error);

        response = job_ask_replace(THUNAR_JOB(job), node->source_file, tp->data, NULL);

        // if the user chose to overwrite then try to do so
        if (response == THUNAR_JOB_RESPONSE_REPLACE)
        {
            node->replace_confirmed = TRUE;
            move_successful = e_file_move(node->source_file,
                                          tp->data,
                                          move_flags | G_FILE_COPY_OVERWRITE,
                                          exo_job_get_cancellable(job),
                                          NULL, NULL,
                                          error);
        }

        // if the user chose to rename then try to do so
        else if (response == THUNAR_JOB_RESPONSE_RENAME)
        {
            move_successful = _transferjob_move_file_with_rename(job,
                                                                 node,
                                                                 tp,
                                                                 move_flags,
                                                                 error);
        }

        // if the user chose to cancel then abort all remaining file moves
        else if (response == THUNAR_JOB_RESPONSE_CANCEL)
        {
            // release all the remaining source and target files, and free the lists
            g_list_free_full(transfer_job->source_node_list, _transfernode_free);
            transfer_job->source_node_list = NULL;

            g_list_free_full(transfer_job->target_file_list, g_object_unref);
            transfer_job->target_file_list= NULL;

            return FALSE;
        }

        /* if the user chose not to replace nor rename the file, so that response == THUNAR_JOB_RESPONSE_SKIP,
         * then *error will be NULL but move_successful will be FALSE, so that the source and target
         * files will be released and the matching list items will be dropped below
         */
    }
    if (*error == NULL)
    {
        if (move_successful)
        {
            // add the target file to the new files list
            *new_files_list_p = e_list_prepend_ref(*new_files_list_p, tp->data);
        }

        // release source and target files
        _transfernode_free(node);
        g_object_unref(tp->data);

        // drop the matching list items
        transfer_job->source_node_list = g_list_delete_link(transfer_job->source_node_list, sp);
        transfer_job->target_file_list = g_list_delete_link(transfer_job->target_file_list, tp);
    }

    // prepare for the fallback copy and delete if appropriate
    else if (!exo_job_is_cancelled(job)
             && (((*error)->code == G_IO_ERROR_NOT_SUPPORTED)
                 || ((*error)->code == G_IO_ERROR_WOULD_MERGE)
                 || ((*error)->code == G_IO_ERROR_WOULD_RECURSE))
            )
    {
        g_clear_error(error);

        // update progress information
        exo_job_info_message(job,
                             _("Could not move \"%s\" directly. "
                             "Collecting files for copying..."),
                             g_file_info_get_display_name(info));

        // if this call fails to collect the node, err will be non-NULL and the loop will exit
        _transferjob_collect_node(transfer_job, node, error);
    }

    return TRUE;
}

static gboolean _transferjob_move_file_with_rename(ExoJob         *job,
                                                   TransferNode   *node,
                                                   GList          *tp,
                                                   GFileCopyFlags flags,
                                                   GError         **error)
{
    gboolean  move_rename_successful = FALSE;
    gint      n_rename = 1;

    node->rename_confirmed = TRUE;

    while (TRUE)
    {
        g_clear_error(error);

        GFile    *renamed_file;
        renamed_file = jobutil_next_renamed_file(THUNAR_JOB(job),
                       node->source_file,
                       tp->data,
                       n_rename++, error);

        if (renamed_file == NULL)
            return FALSE;

        /* Try to move it again to the new renamed file.
         * Directly try to move, because it is racy to first check for
         * file existence and then execute something based on the
         * outcome of that. */
        move_rename_successful = e_file_move(node->source_file,
                                              renamed_file,
                                              flags,
                                              exo_job_get_cancellable(job),
                                              NULL, NULL, error);

        if (!move_rename_successful && !exo_job_is_cancelled(job) &&((*error)->code == G_IO_ERROR_EXISTS))
            continue;

        return move_rename_successful;
    }
}

/**
 * _transferjob_copy_file:
 * @job                : a #TransferJob.
 * @source_file        : the source #GFile to copy.
 * @target_file        : the destination #GFile to copy to.
 * @replace_confirmed  : whether the user has already confirmed that this file should replace an existing one
 * @rename_confirmed   : whether the user has already confirmed that this file should be renamed to a new unique file name
 * @error              : return location for errors or %NULL.
 *
 * Tries to copy @source_file to @target_file. The real destination is the
 * return value and may differ from @target_file(e.g. if you try to copy
 * the file "/foo/bar" into the same directory you'll end up with something
 * like "/foo/copy of bar" instead of "/foo/bar"). If an existing file would
 * be replaced, the user is asked to confirm replace or rename it unless
 * @replace_confirmed or @rename_confirmed is TRUE.
 *
 * The return value is guaranteed to be %NULL on errors and @error will
 * always be set in those cases. If the file is skipped, the return value
 * will be @source_file.
 *
 * Return value: the destination #GFile to which @source_file was copied
 *               or linked. The caller is reposible to release it with
 *               g_object_unref() if no longer needed. It points to
 *               @source_file if the file was skipped and will be %NULL
 *               on error or cancellation.
 **/
static GFile* _transferjob_copy_file(TransferJob *job,
                                      GFile       *source_file,
                                      GFile       *target_file,
                                      gboolean    replace_confirmed,
                                      gboolean    rename_confirmed,
                                      GError      **error)
{
    ThunarJobResponse response;
    GFile            *dest_file = target_file;
    GFileCopyFlags    copy_flags = G_FILE_COPY_NOFOLLOW_SYMLINKS;
    GError           *err = NULL;
    gint              n;
    gint              n_rename = 0;

    e_return_val_if_fail(IS_TRANSFERJOB(job), NULL);
    e_return_val_if_fail(G_IS_FILE(source_file), NULL);
    e_return_val_if_fail(G_IS_FILE(target_file), NULL);
    e_return_val_if_fail(error == NULL || *error == NULL, NULL);

    // abort on cancellation
    if (exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return NULL;

    // various attempts to copy the file
    while (err == NULL)
    {
        _transferjob_check_pause(job);

        if (!g_file_equal(source_file, dest_file))
        {
            // try to copy the file from source_file to the dest_file
            if (_transferjob_copy_file_real(job,
                                        source_file,
                                        dest_file,
                                        copy_flags,
                                        TRUE,
                                        &err))
            {
                // return the real target file
                return g_object_ref(dest_file);
            }
        }
        else
        {
            for (n = 1; err == NULL; ++n)
            {
                GFile *duplicate_file = jobutil_next_duplicate_file(THUNAR_JOB(job),
                                        source_file,
                                        TRUE, n,
                                        &err);

                if (err == NULL)
                {
                    // try to copy the file from source file to the duplicate file
                    if (_transferjob_copy_file_real(job,
                                                source_file,
                                                duplicate_file,
                                                copy_flags,
                                                FALSE,
                                                &err))
                    {
                        // return the real target file
                        return duplicate_file;
                    }

                    g_object_unref(duplicate_file);
                }

                if (err != NULL && err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                    // this duplicate already exists => clear the error to try the next alternative
                    g_clear_error(&err);
                }
            }
        }

        // check if we can recover from this error
        if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
        {
            // reset the error
            g_clear_error(&err);

            // if necessary, ask the user whether to replace or rename the target file
            if (replace_confirmed)
                response = THUNAR_JOB_RESPONSE_REPLACE;
            else if (rename_confirmed)
                response = THUNAR_JOB_RESPONSE_RENAME;
            else
                response = job_ask_replace(THUNAR_JOB(job), source_file,
                                                   dest_file, &err);

            if (err != NULL)
                break;

            // add overwrite flag and retry if we should overwrite
            if (response == THUNAR_JOB_RESPONSE_REPLACE)
            {
                copy_flags |= G_FILE_COPY_OVERWRITE;
                continue;
            }
            else if (response == THUNAR_JOB_RESPONSE_RENAME)
            {
                GFile *renamed_file;
                renamed_file = jobutil_next_renamed_file(THUNAR_JOB(job),
                               source_file,
                               dest_file,
                               ++n_rename, &err);
                if (renamed_file != NULL)
                {
                    if (err != NULL)
                        g_object_unref(renamed_file);
                    else
                    {
                        dest_file = renamed_file;
                        rename_confirmed = TRUE;
                    }
                }
            }

            /* tell the caller we skipped the file if the user
             * doesn't want to retry/overwrite */
            if (response == THUNAR_JOB_RESPONSE_SKIP)
                return g_object_ref(source_file);
        }
    }

    e_assert(err != NULL);

    g_propagate_error(error, err);
    return NULL;
}

static gboolean _transferjob_copy_file_real(TransferJob    *job,
                                            GFile          *source_file,
                                            GFile          *target_file,
                                            GFileCopyFlags copy_flags,
                                            gboolean       merge_directories,
                                            GError         **error)
{
    e_return_val_if_fail(IS_TRANSFERJOB(job), FALSE);
    e_return_val_if_fail(G_IS_FILE(source_file), FALSE);
    e_return_val_if_fail(G_IS_FILE(target_file), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    // reset the file progress
    job->file_progress = 0;

    // source type
    if (exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return FALSE;

    _transferjob_check_pause(job);

    GFileType source_type = g_file_query_file_type(
                                    source_file,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    exo_job_get_cancellable(EXOJOB(job)));

    // target type
    if (exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return FALSE;

    _transferjob_check_pause(job);

    GFileType target_type = g_file_query_file_type(
                                    target_file,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                    exo_job_get_cancellable(EXOJOB(job)));

    if (exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return FALSE;

    _transferjob_check_pause(job);

    GError *err = NULL;

    // check if the target is a symlink and we are in overwrite mode
    if (target_type == G_FILE_TYPE_SYMBOLIC_LINK
        && (copy_flags & G_FILE_COPY_OVERWRITE) != 0)
    {
        if (!g_file_delete(target_file,
                           exo_job_get_cancellable(EXOJOB(job)),
                           &err))
        {
            g_propagate_error(error, err);
            return FALSE;
        }
    }

    g_file_copy(source_file,
                target_file,
                copy_flags,
                exo_job_get_cancellable(EXOJOB(job)),
                _transferjob_progress,
                job,
                &err);

    if (err != NULL && err->domain == G_IO_ERROR)
    {
        // cancelled
        if (err->code == G_IO_ERROR_CANCELLED
            && (source_type == G_FILE_TYPE_REGULAR
                || source_type == G_FILE_TYPE_SYMBOLIC_LINK
                || source_type == G_FILE_TYPE_SPECIAL))
        {
            gchar *target_path = g_file_get_path(target_file);
            syslog(LOG_WARNING,
                   "copy_file_real: delete truncated file \"%s\"\n",
                   target_path);
            g_free(target_path);

            // remove truncated target file
            g_file_delete(target_file, NULL, NULL);
        }

        // merge
        else if (err->code == G_IO_ERROR_WOULD_MERGE
                 || (err->code == G_IO_ERROR_EXISTS
                     && source_type == G_FILE_TYPE_DIRECTORY
                     && target_type == G_FILE_TYPE_DIRECTORY))
        {
            /* we tried to overwrite a directory with a directory.
             * this normally results in a merge.
             * ignore the error if we actually *want* to merge */

            if (merge_directories)
                g_clear_error(&err);
        }

        // recurse
        else if (err->code == G_IO_ERROR_WOULD_RECURSE)
        {
            g_clear_error(&err);

            /*   we tried to copy a directory and either
             *
             * - the target did not exist which means we simple have to
             *   create the target directory
             *
             * or
             *
             * - the target is not a directory and we tried to overwrite it in
             *   which case we have to delete it first and then create the target
             *   directory */

            // check if the target file exists

            gboolean target_exists = g_file_query_exists(
                                        target_file,
                                        exo_job_get_cancellable(EXOJOB(job)));

            // abort on cancellation, continue otherwise
            if (!exo_job_set_error_if_cancelled(EXOJOB(job), &err))
            {
                if (target_exists)
                {
                    /* the target still exists and thus is not a directory.
                     *  try to remove it */
                    g_file_delete(target_file,
                                  exo_job_get_cancellable(EXOJOB(job)),
                                  &err);
                }

                // abort on error or cancellation, continue otherwise
                if (err == NULL)
                {
                    // now try to create the directory
                    g_file_make_directory(target_file,
                                          exo_job_get_cancellable(EXOJOB(job)),
                                          &err);
                }
            }
        }
    }

    if (err != NULL)
    {
        g_propagate_error(error, err);

        return FALSE;
    }

    return TRUE;
}

static void _transferjob_progress(goffset current_num_bytes,
                                  goffset total_num_bytes,
                                  gpointer user_data)
{
    (void) total_num_bytes;

    TransferJob *job = user_data;
    guint64            new_percentage;
    gint64             current_time;
    gint64             expired_time;
    guint64            transfer_rate;

    e_return_if_fail(IS_TRANSFERJOB(job));

    _transferjob_check_pause(job);

    if (job->total_size > 0)
    {
        // update total progress
        job->total_progress +=(current_num_bytes - job->file_progress);

        // update file progress
        job->file_progress = current_num_bytes;

        // compute the new percentage after the progress we've made
        new_percentage =(job->total_progress * 100.0) / job->total_size;

        // get current time
        current_time = g_get_real_time();
        expired_time = current_time - job->last_update_time;

        // notify callers not more then every 500ms
        if (expired_time >(500 * 1000))
        {
            // calculate the transfer rate in the last expired time
            transfer_rate =(job->total_progress - job->last_total_progress) /((gfloat) expired_time / G_USEC_PER_SEC);

            // take the average of the last 10 rates(5 sec), so the output is less jumpy
            if (job->transfer_rate > 0)
                job->transfer_rate =((job->transfer_rate * 10) + transfer_rate) / 11;
            else
                job->transfer_rate = transfer_rate;

            // emit the percent signal
            exo_job_percent(EXOJOB(job), new_percentage);

            // update internals
            job->last_update_time = current_time;
            job->last_total_progress = job->total_progress;
        }
    }
}


