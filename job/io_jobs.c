/*-
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <io_jobs.h>

#include <application.h>
#include <enumtypes.h>
#include <gio_ext.h>
#include <io_scan_directory.h>
#include <job_utils.h>
#include <simplejob.h>
#include <transferjob.h>

#include <errno.h>
#include <gio/gio.h>
#include <glib/gstdio.h>

static gboolean _io_ls(ThunarJob *job, GArray *param_values, GError **error);

static gboolean _io_mkdir(ThunarJob *job, GArray *param_values, GError **error);
static gboolean _io_delete_file(GFile *file, GCancellable *cancellable,
                                GError **error);

static gboolean _io_create(ThunarJob *job, GArray *param_values, GError **error);

static gboolean _io_unlink(ThunarJob *job, GArray *param_values, GError **error);
static GList* _io_collect_nofollow(ThunarJob *job, GList *base_file_list,
                                   gboolean unlinking, GError **error);

static gboolean _io_link(ThunarJob *job, GArray *param_values, GError **error);
static GFile* _io_link_file(ThunarJob *job, GFile *source_file,
                            GFile *target_file, GError **error);

static gboolean _io_trash(ThunarJob *job, GArray *param_values, GError **error);

static gboolean _io_rename(ThunarJob *job, GArray *param_values, GError **error);
static gboolean _io_rename_notify(ThunarFile *file);

static gboolean _io_chown(ThunarJob *job, GArray *param_values, GError **error);
static gboolean _io_chmod(ThunarJob *job, GArray *param_values, GError **error);


ThunarJob* io_list_directory(GFile *directory)
{
    e_return_val_if_fail(G_IS_FILE(directory), NULL);

    return simple_job_launch(_io_ls, 1, G_TYPE_FILE, directory);
}

static gboolean _io_ls(ThunarJob *job, GArray *param_values, GError **error)
{
    GError *err = NULL;
    GFile  *directory;
    GList  *file_list = NULL;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 1, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
        return FALSE;

    // determine the directory to list
    directory = g_value_get_object(&g_array_index(param_values, GValue, 0));

    // make sure the object is valid
    e_assert(G_IS_FILE(directory));

    // collect directory contents(non-recursively)
    file_list = io_scan_directory(job, directory,
                                          G_FILE_QUERY_INFO_NONE,
                                          FALSE, FALSE, TRUE, &err);

    // abort on errors or cancellation
    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }
    else if (exo_job_set_error_if_cancelled(EXO_JOB(job), &err))
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    // check if we have any files to report
    if (G_LIKELY(file_list != NULL))
    {
        // emit the "files-ready" signal
        if (!job_files_ready(THUNAR_JOB(job), file_list))
        {
            /* none of the handlers took over the file list, so it's up to us
             * to destroy it */
            e_list_free(file_list);
        }
    }

    // there should be no errors here
    e_assert(err == NULL);

    // propagate cancellation error
    if (exo_job_set_error_if_cancelled(EXO_JOB(job), &err))
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    return TRUE;
}


ThunarJob* io_make_directories(GList *file_list)
{
    return simple_job_launch(_io_mkdir,
                                    1,
                                    THUNAR_TYPE_G_FILE_LIST,
                                    file_list);
}

static gboolean _io_mkdir(ThunarJob *job, GArray *param_values, GError **error)
{
    ThunarJobResponse response;
    GFileInfo        *info;
    GError           *err = NULL;
    GList            *file_list;
    GList            *lp;
    gchar            *base_name;
    gchar            *display_name;
    guint             n_processed = 0;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 1, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));

    // we know the total list of files to process
    job_set_total_files(THUNAR_JOB(job), file_list);

    for (lp = file_list;
            err == NULL && lp != NULL && !exo_job_is_cancelled(EXO_JOB(job));
            lp = lp->next,  n_processed++)
    {
        g_assert(G_IS_FILE(lp->data));

        // update progress information
        job_processing_file(THUNAR_JOB(job), lp, n_processed);

again:
        // try to create the directory
        if (!g_file_make_directory(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err))
        {
            if (err->code == G_IO_ERROR_EXISTS)
            {
                g_error_free(err);
                err = NULL;

                // abort if the job was cancelled
                if (exo_job_is_cancelled(EXO_JOB(job)))
                    break;

                // the file already exists, query its display name
                info = g_file_query_info(lp->data,
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          exo_job_get_cancellable(EXO_JOB(job)),
                                          NULL);

                // abort if the job was cancelled
                if (exo_job_is_cancelled(EXO_JOB(job)))
                    break;

                // determine the display name, using the basename as a fallback
                if (info != NULL)
                {
                    display_name = g_strdup(g_file_info_get_display_name(info));
                    g_object_unref(info);
                }
                else
                {
                    base_name = g_file_get_basename(lp->data);
                    display_name = g_filename_display_name(base_name);
                    g_free(base_name);
                }

                // ask the user whether he wants to overwrite the existing file
                response = job_ask_overwrite(THUNAR_JOB(job),
                                                     _("The file \"%s\" already exists"),
                                                     display_name);

                // check if we should overwrite it
                if (response == THUNAR_JOB_RESPONSE_REPLACE)
                {
                    // try to remove the file, fail if not possible
                    if (_io_delete_file(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err))
                        goto again;
                }

                // clean up
                g_free(display_name);
            }
            else
            {
                // determine the display name of the file
                base_name = g_file_get_basename(lp->data);
                display_name = g_filename_display_basename(base_name);
                g_free(base_name);

                // ask the user whether to skip/retry this path(cancels the job if not)
                response = job_ask_skip(THUNAR_JOB(job),
                                                _("Failed to create directory \"%s\": %s"),
                                                display_name, err->message);
                g_free(display_name);

                g_error_free(err);
                err = NULL;

                // go back to the beginning if the user wants to retry
                if (response == THUNAR_JOB_RESPONSE_RETRY)
                    goto again;
            }
        }
    }

    // check if we have failed
    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    // check if the job was cancelled
    if (exo_job_is_cancelled(EXO_JOB(job)))
        return FALSE;

    // emit the "new-files" signal with the given file list
    job_new_files(THUNAR_JOB(job), file_list);

    return TRUE;
}

static gboolean _io_delete_file(GFile *file, GCancellable *cancellable,
                                GError **error)
{
    gchar *path;

    if (!g_file_is_native(file))
        return g_file_delete(file, cancellable, error);

    // adapted from g_local_file_delete of gio/glocalfile.c
    path = g_file_get_path(file);

    if (g_remove(path) == 0)
    {
        g_free(path);
        return TRUE;
    }

    g_set_error(error, G_IO_ERROR, g_io_error_from_errno(errno),
                 _("Error removing file: %s"), g_strerror(errno));

    g_free(path);
    return FALSE;
}


ThunarJob* io_create_files(GList *file_list, GFile *template_file)
{
    return simple_job_launch(_io_create, 2,
                                    THUNAR_TYPE_G_FILE_LIST,
                                    file_list,
                                    G_TYPE_FILE,
                                    template_file);
}

static gboolean _io_create(ThunarJob *job, GArray *param_values, GError **error)
{
    GFileOutputStream *stream;
    ThunarJobResponse  response = THUNAR_JOB_RESPONSE_CANCEL;
    GFileInfo         *info;
    GError            *err = NULL;
    GList             *file_list;
    GList             *lp;
    gchar             *base_name;
    gchar             *display_name;
    guint              n_processed = 0;
    GFile             *template_file;
    GFileInputStream  *template_stream = NULL;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 2, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    // get the file list
    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));
    template_file = g_value_get_object(&g_array_index(param_values, GValue, 1));

    // we know the total amount of files to be processed
    job_set_total_files(THUNAR_JOB(job), file_list);

    // check if we need to open the template
    if (template_file != NULL)
    {
        // open read stream to feed in the new files
        template_stream = g_file_read(template_file, exo_job_get_cancellable(EXO_JOB(job)), &err);
        if (G_UNLIKELY(template_stream == NULL))
        {
            g_propagate_error(error, err);
            return FALSE;
        }
    }

    // iterate over all files in the list
    for (lp = file_list;
            err == NULL && lp != NULL && !exo_job_is_cancelled(EXO_JOB(job));
            lp = lp->next, n_processed++)
    {
        g_assert(G_IS_FILE(lp->data));

        // update progress information
        job_processing_file(THUNAR_JOB(job), lp, n_processed);

again:
        // try to create the file
        stream = g_file_create(lp->data,
                                G_FILE_CREATE_NONE,
                                exo_job_get_cancellable(EXO_JOB(job)),
                                &err);

        // abort if the job was cancelled
        if (exo_job_is_cancelled(EXO_JOB(job)))
            break;

        // check if creating failed
        if (stream == NULL)
        {
            if (err->code == G_IO_ERROR_EXISTS)
            {
                g_clear_error(&err);

                // the file already exists, query its display name
                info = g_file_query_info(lp->data,
                                          G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                          G_FILE_QUERY_INFO_NONE,
                                          exo_job_get_cancellable(EXO_JOB(job)),
                                          NULL);

                // abort if the job was cancelled
                if (exo_job_is_cancelled(EXO_JOB(job)))
                    break;

                // determine the display name, using the basename as a fallback
                if (info != NULL)
                {
                    display_name = g_strdup(g_file_info_get_display_name(info));
                    g_object_unref(info);
                }
                else
                {
                    base_name = g_file_get_basename(lp->data);
                    display_name = g_filename_display_name(base_name);
                    g_free(base_name);
                }

                // ask the user whether he wants to overwrite the existing file
                response = job_ask_overwrite(THUNAR_JOB(job),
                                                     _("The file \"%s\" already exists"),
                                                     display_name);

                // check if we should overwrite
                if (response == THUNAR_JOB_RESPONSE_REPLACE)
                {
                    // try to remove the file. fail if not possible
                    if (_io_delete_file(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err))
                        goto again;
                }

                // clean up
                g_free(display_name);
            }
            else
            {
                // determine display name of the file
                base_name = g_file_get_basename(lp->data);
                display_name = g_filename_display_basename(base_name);
                g_free(base_name);

                // ask the user whether to skip/retry this path(cancels the job if not)
                response = job_ask_skip(THUNAR_JOB(job),
                                                _("Failed to create empty file \"%s\": %s"),
                                                display_name, err->message);
                g_free(display_name);

                g_clear_error(&err);

                // go back to the beginning if the user wants to retry
                if (response == THUNAR_JOB_RESPONSE_RETRY)
                    goto again;
            }
        }
        else
        {
            if (template_stream != NULL)
            {
                // write the template into the new file
                g_output_stream_splice(G_OUTPUT_STREAM(stream),
                                        G_INPUT_STREAM(template_stream),
                                        G_OUTPUT_STREAM_SPLICE_CLOSE_TARGET,
                                        exo_job_get_cancellable(EXO_JOB(job)),
                                        NULL);
            }

            g_object_unref(stream);
        }
    }

    if (template_stream != NULL)
        g_object_unref(template_stream);

    // check if we have failed
    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    // check if the job was cancelled
    if (exo_job_is_cancelled(EXO_JOB(job)))
        return FALSE;

    // emit the "new-files" signal with the given file list
    job_new_files(THUNAR_JOB(job), file_list);

    return TRUE;
}


ThunarJob* io_unlink_files(GList *file_list)
{
    return simple_job_launch(_io_unlink, 1,
                                    THUNAR_TYPE_G_FILE_LIST, file_list);
}

static gboolean _io_unlink(ThunarJob *job, GArray *param_values, GError **error)
{
    ThunarJobResponse     response;
    GFileInfo            *info;
    GError               *err = NULL;
    GList                *file_list;
    GList                *lp;
    gchar                *base_name;
    gchar                *display_name;
    guint                 n_processed = 0;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 1, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    // get the file list
    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));

    // tell the user that we're preparing to unlink the files
    exo_job_info_message(EXO_JOB(job), _("Preparing..."));

    // recursively collect files for removal, not following any symlinks
    file_list = _io_collect_nofollow(job, file_list, TRUE, &err);

    // free the file list and fail if there was an error or the job was cancelled
    if (err != NULL || exo_job_is_cancelled(EXO_JOB(job)))
    {
        if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
            g_error_free(err);
        else
            g_propagate_error(error, err);

        e_list_free(file_list);
        return FALSE;
    }

    // we know the total list of files to process
    job_set_total_files(THUNAR_JOB(job), file_list);

    // remove all the files
    for (lp = file_list;
            lp != NULL && !exo_job_is_cancelled(EXO_JOB(job));
            lp = lp->next, n_processed++)
    {
        g_assert(G_IS_FILE(lp->data));

        // skip root folders which cannot be deleted anyway
        if (e_file_is_root(lp->data))
            continue;

        // update progress information
        job_processing_file(THUNAR_JOB(job), lp, n_processed);

again:
        // try to delete the file
        if (_io_delete_file(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err))
        {
            /* notify the thumbnail cache that the corresponding thumbnail can also
             * be deleted now */
            //thunar_thumbnail_cache_delete_file(thumbnail_cache, lp->data);
        }
        else
        {
            // query the file info for the display name
            info = g_file_query_info(lp->data,
                                      G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                      G_FILE_QUERY_INFO_NONE,
                                      exo_job_get_cancellable(EXO_JOB(job)),
                                      NULL);

            // abort if the job was cancelled
            if (exo_job_is_cancelled(EXO_JOB(job)))
            {
                g_clear_error(&err);
                break;
            }

            // determine the display name, using the basename as a fallback
            if (info != NULL)
            {
                display_name = g_strdup(g_file_info_get_display_name(info));
                g_object_unref(info);
            }
            else
            {
                base_name = g_file_get_basename(lp->data);
                display_name = g_filename_display_name(base_name);
                g_free(base_name);
            }

            // ask the user whether he wants to skip this file
            response = job_ask_skip(THUNAR_JOB(job),
                                            _("Could not delete file \"%s\": %s"),
                                            display_name, err->message);
            g_free(display_name);

            // clear the error
            g_clear_error(&err);

            // check whether to retry
            if (response == THUNAR_JOB_RESPONSE_RETRY)
                goto again;
        }
    }

    // release the file list
    e_list_free(file_list);

    if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
        return FALSE;
    else
        return TRUE;
}

static GList* _io_collect_nofollow(ThunarJob *job, GList *base_file_list,
                                   gboolean unlinking, GError **error)
{
    GError *err = NULL;
    GList  *child_file_list = NULL;
    GList  *file_list = NULL;
    GList  *lp;

    // recursively collect the files
    for (lp = base_file_list;
            err == NULL && lp != NULL && !exo_job_is_cancelled(EXO_JOB(job));
            lp = lp->next)
    {
        // try to scan the directory
        child_file_list = io_scan_directory(job, lp->data,
                          G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                          TRUE, unlinking, FALSE, &err);

        // prepend the new files to the existing list
        file_list = e_list_prepend_ref(file_list, lp->data);
        file_list = g_list_concat(child_file_list, file_list);
    }

    // check if we failed
    if (err != NULL || exo_job_is_cancelled(EXO_JOB(job)))
    {
        if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
            g_error_free(err);
        else
            g_propagate_error(error, err);

        // release the collected files
        e_list_free(file_list);

        return NULL;
    }

    return file_list;
}


ThunarJob* io_move_files(GList *source_file_list, GList *target_file_list)
{
    ThunarJob *job;

    e_return_val_if_fail(source_file_list != NULL, NULL);
    e_return_val_if_fail(target_file_list != NULL, NULL);
    e_return_val_if_fail(g_list_length(source_file_list) == g_list_length(target_file_list), NULL);

    job = transfer_job_new(source_file_list, target_file_list,
                                   TRANSFERJOB_MOVE);
    job_set_pausable(job, TRUE);

    return THUNAR_JOB(exo_job_launch(EXO_JOB(job)));
}


ThunarJob* io_copy_files(GList *source_file_list, GList *target_file_list)
{
    ThunarJob *job;

    e_return_val_if_fail(source_file_list != NULL, NULL);
    e_return_val_if_fail(target_file_list != NULL, NULL);
    e_return_val_if_fail(g_list_length(source_file_list) == g_list_length(target_file_list), NULL);

    job = transfer_job_new(source_file_list, target_file_list,
                                   TRANSFERJOB_COPY);
    job_set_pausable(job, TRUE);

    return THUNAR_JOB(exo_job_launch(EXO_JOB(job)));
}


ThunarJob* io_link_files(GList *source_file_list, GList *target_file_list)
{
    e_return_val_if_fail(source_file_list != NULL, NULL);
    e_return_val_if_fail(target_file_list != NULL, NULL);
    e_return_val_if_fail(g_list_length(source_file_list) == g_list_length(target_file_list), NULL);

    return simple_job_launch(_io_link, 2,
                                     THUNAR_TYPE_G_FILE_LIST, source_file_list,
                                     THUNAR_TYPE_G_FILE_LIST, target_file_list);
}

static gboolean _io_link(ThunarJob *job, GArray *param_values, GError **error)
{
    GError               *err = NULL;
    GFile                *real_target_file;
    GList                *new_files_list = NULL;
    GList                *source_file_list;
    GList                *sp;
    GList                *target_file_list;
    GList                *tp;
    guint                 n_processed = 0;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 2, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    source_file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));
    target_file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 1));

    // we know the total list of paths to process
    job_set_total_files(THUNAR_JOB(job), source_file_list);

    // process all files
    for (sp = source_file_list, tp = target_file_list;
            err == NULL && sp != NULL && tp != NULL;
            sp = sp->next, tp = tp->next, n_processed++)
    {
        e_assert(G_IS_FILE(sp->data));
        e_assert(G_IS_FILE(tp->data));

        // update progress information
        job_processing_file(THUNAR_JOB(job), sp, n_processed);

        // try to create the symbolic link
        real_target_file = _io_link_file(job, sp->data, tp->data, &err);
        if (real_target_file != NULL)
        {
            // queue the file for the folder update unless it was skipped
            if (sp->data != real_target_file)
            {
                new_files_list = e_list_prepend_ref(new_files_list,
                                 real_target_file);

            }

            // release the real target file
            g_object_unref(real_target_file);
        }
    }

    if (err != NULL)
    {
        e_list_free(new_files_list);
        g_propagate_error(error, err);
        return FALSE;
    }
    else
    {
        job_new_files(THUNAR_JOB(job), new_files_list);
        e_list_free(new_files_list);
        return TRUE;
    }
}

static GFile* _io_link_file(ThunarJob *job, GFile *source_file,
                            GFile *target_file, GError **error)
{
    ThunarJobResponse response;
    GError           *err = NULL;
    gchar            *base_name;
    gchar            *display_name;
    gchar            *source_path;
    gint              n;

    e_return_val_if_fail(THUNAR_IS_JOB(job), NULL);
    e_return_val_if_fail(G_IS_FILE(source_file), NULL);
    e_return_val_if_fail(G_IS_FILE(target_file), NULL);
    e_return_val_if_fail(error == NULL || *error == NULL, NULL);

    // abort on cancellation
    if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
        return NULL;

    // try to determine the source path
    source_path = g_file_get_path(source_file);
    if (source_path == NULL)
    {
        base_name = g_file_get_basename(source_file);
        display_name = g_filename_display_name(base_name);
        g_set_error(&err, G_IO_ERROR, G_IO_ERROR_NOT_SUPPORTED,
                     _("Could not create symbolic link to \"%s\" "
                       "because it is not a local file"), display_name);
        g_free(display_name);
        g_free(base_name);
    }

    // various attempts to create the symbolic link
    while (err == NULL)
    {
        if (!g_file_equal(source_file, target_file))
        {
            // try to create the symlink
            if (g_file_make_symbolic_link(target_file, source_path,
                                           exo_job_get_cancellable(EXO_JOB(job)),
                                           &err))
            {
                // release the source path
                g_free(source_path);

                // return the real target file
                return g_object_ref(target_file);
            }
        }
        else
        {
            for (n = 1; err == NULL; ++n)
            {
                GFile *duplicate_file = job_util_next_duplicate_file(job,
                                        source_file,
                                        FALSE, n,
                                        &err);

                if (err == NULL)
                {
                    // try to create the symlink
                    if (g_file_make_symbolic_link(duplicate_file, source_path,
                                                   exo_job_get_cancellable(EXO_JOB(job)),
                                                   &err))
                    {
                        // release the source path
                        g_free(source_path);

                        // return the real target file
                        return duplicate_file;
                    }

                    // release the duplicate file, we no longer need it
                    g_object_unref(duplicate_file);
                }

                if (err != NULL && err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
                {
                    // this duplicate already exists => clear the error and try the next alternative
                    g_clear_error(&err);
                }
            }
        }

        // check if we can recover from this error
        if (err->domain == G_IO_ERROR && err->code == G_IO_ERROR_EXISTS)
        {
            // ask the user whether to replace the target file
            response = job_ask_overwrite(job, "%s", err->message);

            // reset the error
            g_clear_error(&err);

            // propagate the cancelled error if the job was aborted
            if (exo_job_set_error_if_cancelled(EXO_JOB(job), &err))
                break;

            // try to delete the file
            if (response == THUNAR_JOB_RESPONSE_REPLACE)
            {
                /* try to remove the target file. if not possible, err will be set and
                 * the while loop will be aborted */
                _io_delete_file(target_file, exo_job_get_cancellable(EXO_JOB(job)), &err);
            }

            /* tell the caller that we skipped this file if the user doesn't want to
             * overwrite it */
            if (response == THUNAR_JOB_RESPONSE_SKIP)
                return g_object_ref(source_file);
        }
    }

    e_assert(err != NULL);

    // free the source path
    g_free(source_path);

    g_propagate_error(error, err);
    return NULL;
}


ThunarJob* io_trash_files(GList *file_list)
{
    e_return_val_if_fail(file_list != NULL, NULL);

    return simple_job_launch(_io_trash, 1,
                                    THUNAR_TYPE_G_FILE_LIST, file_list);
}

static gboolean _io_trash(ThunarJob *job, GArray *param_values, GError **error)
{
    ThunarJobResponse     response;
    GError               *err = NULL;
    GList                *file_list;
    GList                *lp;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 1, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));

    if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
        return FALSE;

    for (lp = file_list; err == NULL && lp != NULL; lp = lp->next)
    {
        e_assert(G_IS_FILE(lp->data));

        // trash the file or folder
        g_file_trash(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err);

        if (err != NULL)
        {
            response = job_ask_delete(job, "%s", err->message);

            g_clear_error(&err);

            if (response == THUNAR_JOB_RESPONSE_CANCEL)
                break;

            if (response == THUNAR_JOB_RESPONSE_YES)
                _io_delete_file(lp->data, exo_job_get_cancellable(EXO_JOB(job)), &err);
        }
    }

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}


ThunarJob* io_restore_files(GList *source_file_list, GList *target_file_list)
{
    ThunarJob *job;

    e_return_val_if_fail(source_file_list != NULL, NULL);
    e_return_val_if_fail(target_file_list != NULL, NULL);
    e_return_val_if_fail(g_list_length(source_file_list) == g_list_length(target_file_list), NULL);

    job = transfer_job_new(source_file_list, target_file_list,
                                  TRANSFERJOB_MOVE);

    return THUNAR_JOB(exo_job_launch(EXO_JOB(job)));
}


ThunarJob* io_rename_file(ThunarFile *file, const gchar *display_name)
{
    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    e_return_val_if_fail(g_utf8_validate(display_name, -1, NULL), NULL);

    return simple_job_launch(_io_rename,
                                    2,
                                    THUNAR_TYPE_FILE, file,
                                    G_TYPE_STRING,
                                    display_name);
}

static gboolean _io_rename(ThunarJob *job, GArray *param_values, GError **error)
{
    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 2, FALSE);
    e_return_val_if_fail(G_VALUE_HOLDS(&g_array_index(param_values, GValue, 0), THUNAR_TYPE_FILE), FALSE);
    e_return_val_if_fail(G_VALUE_HOLDS_STRING(&g_array_index(param_values, GValue, 1)), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    GError *err = NULL;

    if (exo_job_set_error_if_cancelled(EXO_JOB(job), error))
        return FALSE;

    // determine the file and display name
    ThunarFile *file = g_value_get_object(&g_array_index(param_values, GValue, 0));

    const gchar *display_name = g_value_get_string(&g_array_index(param_values,
                                                                  GValue,
                                                                  1));

    // try to rename the file
    if (th_file_rename(file,
                       display_name,
                       exo_job_get_cancellable(EXO_JOB(job)),
                       TRUE,
                       &err))
    {
        exo_job_send_to_mainloop(EXO_JOB(job),
                                 (GSourceFunc) _io_rename_notify,
                                 g_object_ref(file),
                                 g_object_unref);
    }

    // abort on errors or cancellation
    if (err != NULL)
    {
        g_propagate_error(error, err);

        return FALSE;
    }

    return TRUE;
}

static gboolean _io_rename_notify(ThunarFile *file)
{
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    // tell the associated folder that the file was renamed
    fileinfo_renamed(FILEINFO(file));

    // emit the file changed signal
    th_file_changed(file);

    return FALSE;
}

ThunarJob* io_change_group(GList *files, guint32 gid, gboolean recursive)
{
    e_return_val_if_fail(files != NULL, NULL);

    // files are released when the list if destroyed
    g_list_foreach(files, (GFunc)(void(*)(void)) g_object_ref, NULL);

    return simple_job_launch(_io_chown, 4,
                                    THUNAR_TYPE_G_FILE_LIST, files,
                                    G_TYPE_INT, -1,
                                    G_TYPE_INT, (gint) gid,
                                    G_TYPE_BOOLEAN, recursive);
}

static gboolean _io_chown(ThunarJob *job, GArray *param_values, GError **error)
{
    ThunarJobResponse response;
    const gchar      *message;
    GFileInfo        *info;
    gboolean          recursive;
    GError           *err = NULL;
    GList            *file_list;
    GList            *lp;
    gint              uid;
    gint              gid;
    guint             n_processed = 0;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 4, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));
    uid = g_value_get_int(&g_array_index(param_values, GValue, 1));
    gid = g_value_get_int(&g_array_index(param_values, GValue, 2));
    recursive = g_value_get_boolean(&g_array_index(param_values, GValue, 3));

    e_assert((uid >= 0 || gid >= 0) && !(uid >= 0 && gid >= 0));

    // collect the files for the chown operation
    if (recursive)
        file_list = _io_collect_nofollow(job, file_list, FALSE, &err);
    else
        file_list = e_list_copy(file_list);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    // we know the total list of files to process
    job_set_total_files(THUNAR_JOB(job), file_list);

    // change the ownership of all files
    for (lp = file_list; lp != NULL && err == NULL; lp = lp->next, n_processed++)
    {
        // update progress information
        job_processing_file(THUNAR_JOB(job), lp, n_processed);

        // try to query information about the file
        info = g_file_query_info(lp->data,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  exo_job_get_cancellable(EXO_JOB(job)),
                                  &err);

        if (err != NULL)
            break;

retry_chown:
        if (uid >= 0)
        {
            // try to change the owner UID
            g_file_set_attribute_uint32(lp->data,
                                         G_FILE_ATTRIBUTE_UNIX_UID, uid,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         exo_job_get_cancellable(EXO_JOB(job)),
                                         &err);
        }
        else if (gid >= 0)
        {
            // try to change the owner GID
            g_file_set_attribute_uint32(lp->data,
                                         G_FILE_ATTRIBUTE_UNIX_GID, gid,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         exo_job_get_cancellable(EXO_JOB(job)),
                                         &err);
        }

        // check if there was a recoverable error
        if (err != NULL && !exo_job_is_cancelled(EXO_JOB(job)))
        {
            // generate a useful error message
            message = G_LIKELY(uid >= 0) ? _("Failed to change the owner of \"%s\": %s")
                      : _("Failed to change the group of \"%s\": %s");

            // ask the user whether to skip/retry this file
            response = job_ask_skip(THUNAR_JOB(job), message,
                                            g_file_info_get_display_name(info),
                                            err->message);

            // clear the error
            g_clear_error(&err);

            // check whether to retry
            if (response == THUNAR_JOB_RESPONSE_RETRY)
                goto retry_chown;
        }

        // release file information
        g_object_unref(info);
    }

    // release the file list
    e_list_free(file_list);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
}

ThunarJob* io_change_mode(GList          *files,
                          ThunarFileMode dir_mask,
                          ThunarFileMode dir_mode,
                          ThunarFileMode file_mask,
                          ThunarFileMode file_mode,
                          gboolean       recursive)
{
    e_return_val_if_fail(files != NULL, NULL);

    // files are released when the list if destroyed
    g_list_foreach(files, (GFunc)(void(*)(void)) g_object_ref, NULL);

    return simple_job_launch(_io_chmod, 6,
                                    THUNAR_TYPE_G_FILE_LIST, files,
                                    THUNAR_TYPE_FILE_MODE, dir_mask,
                                    THUNAR_TYPE_FILE_MODE, dir_mode,
                                    THUNAR_TYPE_FILE_MODE, file_mask,
                                    THUNAR_TYPE_FILE_MODE, file_mode,
                                    G_TYPE_BOOLEAN, recursive);
}

static gboolean _io_chmod(ThunarJob *job, GArray *param_values, GError **error)
{
    ThunarJobResponse response;
    GFileInfo        *info;
    gboolean          recursive;
    GError           *err = NULL;
    GList            *file_list;
    GList            *lp;
    guint             n_processed = 0;
    ThunarFileMode    dir_mask;
    ThunarFileMode    dir_mode;
    ThunarFileMode    file_mask;
    ThunarFileMode    file_mode;
    ThunarFileMode    mask;
    ThunarFileMode    mode;
    ThunarFileMode    old_mode;
    ThunarFileMode    new_mode;

    e_return_val_if_fail(THUNAR_IS_JOB(job), FALSE);
    e_return_val_if_fail(param_values != NULL, FALSE);
    e_return_val_if_fail(param_values->len == 6, FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    file_list = g_value_get_boxed(&g_array_index(param_values, GValue, 0));
    dir_mask = g_value_get_flags(&g_array_index(param_values, GValue, 1));
    dir_mode = g_value_get_flags(&g_array_index(param_values, GValue, 2));
    file_mask = g_value_get_flags(&g_array_index(param_values, GValue, 3));
    file_mode = g_value_get_flags(&g_array_index(param_values, GValue, 4));
    recursive = g_value_get_boolean(&g_array_index(param_values, GValue, 5));

    // collect the files for the chown operation
    if (recursive)
        file_list = _io_collect_nofollow(job, file_list, FALSE, &err);
    else
        file_list = e_list_copy(file_list);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }

    // we know the total list of files to process
    job_set_total_files(THUNAR_JOB(job), file_list);

    // change the ownership of all files
    for (lp = file_list; lp != NULL && err == NULL; lp = lp->next, n_processed++)
    {
        // update progress information
        job_processing_file(THUNAR_JOB(job), lp, n_processed);

        // try to query information about the file
        info = g_file_query_info(lp->data,
                                  G_FILE_ATTRIBUTE_STANDARD_DISPLAY_NAME ","
                                  G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                                  G_FILE_ATTRIBUTE_UNIX_MODE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  exo_job_get_cancellable(EXO_JOB(job)),
                                  &err);

        if (err != NULL)
            break;

retry_chown:
        // different actions depending on the type of the file
        if (g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
        {
            mask = dir_mask;
            mode = dir_mode;
        }
        else
        {
            mask = file_mask;
            mode = file_mode;
        }

        // determine the current mode
        old_mode = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE);

        /* generate the new mode, taking the old mode(which contains file type
         * information) into account */
        new_mode =((old_mode & ~mask) | mode) & 07777;

        if (old_mode != new_mode)
        {
            // try to change the file mode
            g_file_set_attribute_uint32(lp->data,
                                         G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                         G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                         exo_job_get_cancellable(EXO_JOB(job)),
                                         &err);
        }

        // check if there was a recoverable error
        if (err != NULL && !exo_job_is_cancelled(EXO_JOB(job)))
        {
            // ask the user whether to skip/retry this file
            response = job_ask_skip(job,
                                            _("Failed to change the permissions of \"%s\": %s"),
                                            g_file_info_get_display_name(info),
                                            err->message);

            // clear the error
            g_clear_error(&err);

            // check whether to retry
            if (response == THUNAR_JOB_RESPONSE_RETRY)
                goto retry_chown;
        }

        // release file information
        g_object_unref(info);
    }

    // release the file list
    e_list_free(file_list);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        return FALSE;
    }
    else
    {
        return TRUE;
    }
    return TRUE;
}


