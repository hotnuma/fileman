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

#include "config.h"
#include "io-scandir.h"

#include "gio-ext.h"

GList* io_scan_directory(ThunarJob *job, GFile *file,
                         GFileQueryInfoFlags flags,
                         gboolean recursively,
                         gboolean unlinking,
                         gboolean return_thunar_files,
                         GError **error)
{
    GFileEnumerator *enumerator;
    GFileInfo       *info;
    GFileType        type;
    GError          *err = NULL;
    GFile           *child_file;
    GList           *child_files = NULL;
    GList           *files = NULL;
    const gchar     *namespace;
    ThunarFile      *thunar_file;
    gboolean         is_mounted;
    GCancellable    *cancellable = NULL;

    e_return_val_if_fail(G_IS_FILE(file), NULL);
    e_return_val_if_fail(error == NULL || *error == NULL, NULL);

    // abort if the job was cancelled
    if (job != NULL && exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return NULL;

    /* don't recurse when we are scanning prior to unlinking and the current
     * file/dir is in the trash. In GVfs, only the top-level directories in
     * the trash can be modified and deleted directly. See
     * https://bugzilla.xfce.org/show_bug.cgi?id=7147
     * for more information */

    if (unlinking && e_file_is_trashed(file) && !e_file_is_root(file))
    {
        return NULL;
    }

    if (job != NULL)
        cancellable = exo_job_get_cancellable(EXOJOB(job));

    // query the file type
    type = g_file_query_file_type(file, flags, cancellable);

    // abort if the job was cancelled
    if (job != NULL && exo_job_set_error_if_cancelled(EXOJOB(job), error))
        return NULL;

    // ignore non-directory nodes
    if (type != G_FILE_TYPE_DIRECTORY)
        return NULL;

    // determine the namespace
    if (return_thunar_files)
        namespace = FILEINFO_NAMESPACE;
    else
        namespace = G_FILE_ATTRIBUTE_STANDARD_TYPE ","
                        G_FILE_ATTRIBUTE_STANDARD_NAME;

    // try to read from the direectory
    enumerator = g_file_enumerate_children(file, namespace,
                                            flags, cancellable, &err);

    // abort if there was an error or the job was cancelled
    if (err != NULL)
    {
        g_propagate_error(error, err);
        return NULL;
    }

    // iterate over children one by one
    while (job == NULL || !exo_job_is_cancelled(EXOJOB(job)))
    {
        // query info of the child
        info = g_file_enumerator_next_file(enumerator, cancellable, &err);

        if (info == NULL)
            break;

        is_mounted = TRUE;
        if (err != NULL)
        {
            if (g_error_matches(err, G_IO_ERROR, G_IO_ERROR_NOT_MOUNTED))
            {
                is_mounted = FALSE;
                g_clear_error(&err);
            }
            else
            {
                // break on errors
                break;
            }
        }

        // create GFile for the child
        child_file = g_file_get_child(file, g_file_info_get_name(info));

        if (return_thunar_files)
        {
            // Prepend the ThunarFile
            thunar_file = th_file_get_with_info(child_file,
                                                info, !is_mounted);
            files = e_list_prepend_ref(files, thunar_file);
            g_object_unref(G_OBJECT(thunar_file));
        }
        else
        {
            // Prepend the GFile
            files = e_list_prepend_ref(files, child_file);
        }

        // if the child is a directory and we need to recurse ... just do so
        if (recursively
            && is_mounted
            && g_file_info_get_file_type(info) == G_FILE_TYPE_DIRECTORY)
        {
            child_files = io_scan_directory(job,
                                            child_file,
                                            flags,
                                            recursively,
                                            unlinking,
                                            return_thunar_files,
                                            &err);

            /* prepend children to the file list to make sure they're
             * processed first(required for unlinking) */
            files = g_list_concat(child_files, files);
        }

        g_object_unref(child_file);
        g_object_unref(info);
    }

    // release the enumerator
    g_object_unref(enumerator);

    if (err != NULL)
    {
        g_propagate_error(error, err);
        e_list_free(files);
        return NULL;
    }
    else if (job != NULL && exo_job_set_error_if_cancelled(EXOJOB(job), &err))
    {
        g_propagate_error(error, err);
        e_list_free(files);
        return NULL;
    }

    return files;
}


