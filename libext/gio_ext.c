/*-
 * Copyright(c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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
#include <gio_ext.h>

#include <th_file.h>
#include <utils.h>
#include <gio/gdesktopappinfo.h>
#include <syslog.h>

// GAppInfo -------------------------------------------------------------------

gboolean e_app_info_launch(GAppInfo *info,
                           GFile    *working_directory,
                           GList    *path_list,
                           GAppLaunchContext *context,
                           GError   **error)
{
    ThunarFile   *file;
    GAppInfo     *default_app_info;
    GList        *recommended_app_infos;
    GList        *lp;
    const gchar  *content_type;
    gboolean      result = FALSE;
    gchar        *new_path = NULL;
    gchar        *old_path = NULL;
    gboolean      skip_app_info_update;

    e_return_val_if_fail(G_IS_APP_INFO(info), FALSE);
    e_return_val_if_fail(working_directory == NULL || G_IS_FILE(working_directory), FALSE);
    e_return_val_if_fail(path_list != NULL, FALSE);
    e_return_val_if_fail(G_IS_APP_LAUNCH_CONTEXT(context), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    skip_app_info_update =(g_object_get_data(G_OBJECT(info), "skip-app-info-update") != NULL);

    // check if we want to set the working directory of the spawned app
    if (working_directory != NULL)
    {
        // determine the working directory path
        new_path = g_file_get_path(working_directory);
        if (new_path != NULL)
        {
            // switch to the desired working directory, remember that of Thunar itself
            old_path = util_change_working_directory(new_path);

            // forget about the new working directory path
            g_free(new_path);
        }
    }

    // launch the paths with the specified app info
    result = g_app_info_launch(info, path_list, context, error);

    // if successful, remember the application as last used for the file types
    if (result == TRUE)
    {
        for(lp = path_list; lp != NULL; lp = lp->next)
        {
            gboolean update_app_info = !skip_app_info_update;

            file = th_file_get(lp->data, NULL);
            if (file == NULL)
                continue;

            content_type = th_file_get_content_type(file);

            // determine default application
            default_app_info = th_file_get_default_handler(file);
            if (default_app_info != NULL)
            {
                // check if the application is the default one
                if (g_app_info_equal(info, default_app_info))
                    update_app_info = FALSE;
                g_object_unref(default_app_info);
            }

            if (update_app_info)
            {
                // obtain list of last used applications
                recommended_app_infos = g_app_info_get_recommended_for_type(content_type);
                if (recommended_app_infos != NULL)
                {
                    /* check if the application is already the last used one
                     * by comparing it with the first entry in the list */
                    if (g_app_info_equal(info, recommended_app_infos->data))
                        update_app_info = FALSE;

                    g_list_free(recommended_app_infos);
                }
            }

            // emit "changed" on the file if we successfully changed the last used application
            if (update_app_info && g_app_info_set_as_last_used_for_type(info, content_type, NULL))
                th_file_changed(file);

            g_object_unref(file);
        }
    }

    /* check if we need to reset the working directory to the one Thunar was
     * opened from */
    if (old_path != NULL)
    {
        // switch to Thunar's original working directory
        new_path = util_change_working_directory(old_path);

        // clean up
        g_free(new_path);
        g_free(old_path);
    }

    return result;
}

gboolean e_app_info_should_show(GAppInfo *info)
{
    e_return_val_if_fail(G_IS_APP_INFO(info), FALSE);

    if (G_IS_DESKTOP_APP_INFO(info))
    {
        /* NoDisplay=true files should be visible in the interface,
         * because this key is intent to hide mime-helpers from the
         * application menu. Hidden=true is never returned by GIO. */
        return g_desktop_app_info_get_show_in(G_DESKTOP_APP_INFO(info), NULL);
    }

    return TRUE;
}

// GFile ----------------------------------------------------------------------

GFile* e_file_new_for_home()
{
    return g_file_new_for_path(g_get_home_dir());
}

GFile* e_file_new_for_root()
{
    return g_file_new_for_uri("file:///");
}

GFile* e_file_new_for_trash()
{
    return g_file_new_for_uri("trash:///");
}

gchar* e_file_get_display_name(GFile *file)
{
    gchar *base_name;
    gchar *display_name;

    e_return_val_if_fail(G_IS_FILE(file), NULL);

    base_name = g_file_get_basename(file);
    if (G_LIKELY(base_name != NULL))
    {
        if (strcmp(base_name, "/") == 0)
            display_name = g_strdup(_("File System"));
        else if (e_file_is_trash(file))
            display_name = g_strdup(_("Trash"));
        else if (g_utf8_validate(base_name, -1, NULL))
            display_name = g_strdup(base_name);
        else
            display_name = g_uri_escape_string(base_name, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);

        g_free(base_name);
    }
    else
    {
        display_name = g_strdup("?");
    }

    return display_name;
}

gchar* e_file_get_display_name_remote(GFile *mount_point)
{
    gchar       *scheme;
    gchar       *parse_name;
    const gchar *p;
    const gchar *path;
    gchar       *unescaped;
    gchar       *hostname;
    gchar       *display_name = NULL;
    const gchar *skip;
    const gchar *firstdot;
    const gchar  skip_chars[] = ":@";
    guint        n;

    e_return_val_if_fail(G_IS_FILE(mount_point), NULL);

    // not intended for local mounts
    if (!g_file_is_native(mount_point))
    {
        scheme = g_file_get_uri_scheme(mount_point);
        parse_name = g_file_get_parse_name(mount_point);

        if (g_str_has_prefix(parse_name, scheme))
        {
            // extract the hostname
            p = parse_name + strlen(scheme);
            while(*p == ':' || *p == '/')
                ++p;

            // goto path part
            path = strchr(p, '/');
            firstdot = strchr(p, '.');

            if (firstdot != NULL)
            {
                // skip password or login names in the hostname
                for(n = 0; n < G_N_ELEMENTS(skip_chars) - 1; n++)
                {
                    skip = strchr(p, skip_chars[n]);
                    if (skip != NULL
                            &&(path == NULL || skip < path)
                            &&(skip < firstdot))
                        p = skip + 1;
                }
            }

            // extract the path and hostname from the string
            if (G_LIKELY(path != NULL))
            {
                hostname = g_strndup(p, path - p);
            }
            else
            {
                hostname = g_strdup(p);
                path = "/";
            }

            // unescape the path so that spaces and other characters are shown correctly
            unescaped = g_uri_unescape_string(path, NULL);

            // TRANSLATORS: this will result in "<path> on <hostname>"
            display_name = g_strdup_printf(_("%s on %s"), unescaped, hostname);

            g_free(unescaped);
            g_free(hostname);
        }

        g_free(scheme);
        g_free(parse_name);
    }

    // never return null
    if (display_name == NULL)
        display_name = e_file_get_display_name(mount_point);

    return display_name;
}

/**
 * thunar_g_file_get_free_space:
 * @file           : a #GFile instance.
 * @fs_free_return : return location for the amount of
 *                   free space or %NULL.
 * @fs_size_return : return location for the total volume size.
 *
 * Determines the amount of free space of the volume on
 * which @file resides. Returns %TRUE if the amount of
 * free space was determined successfully and placed into
 * @free_space_return, else %FALSE will be returned.
 *
 * Return value: %TRUE if successfull, else %FALSE.
 **/
gboolean e_file_get_free_space(GFile *file, guint64 *fs_free_return,
                               guint64 *fs_size_return)
{
    e_return_val_if_fail(G_IS_FILE(file), FALSE);

    GFileInfo *filesystem_info;
    gboolean   success = FALSE;

    filesystem_info = g_file_query_filesystem_info(file,
                      FILESYSTEM_INFO_NAMESPACE,
                      NULL, NULL);

    if (filesystem_info != NULL)
    {
        if (fs_free_return != NULL)
        {
            *fs_free_return = g_file_info_get_attribute_uint64(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
            success = g_file_info_has_attribute(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_FREE);
        }

        if (fs_size_return != NULL)
        {
            *fs_size_return = g_file_info_get_attribute_uint64(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
            success = g_file_info_has_attribute(filesystem_info, G_FILE_ATTRIBUTE_FILESYSTEM_SIZE);
        }

        g_object_unref(filesystem_info);
    }

    return success;
}

gchar* e_file_get_free_space_string(GFile *file, gboolean file_size_binary)
{
    gchar             *fs_free_str;
    gchar             *fs_size_str;
    guint64            fs_free;
    guint64            fs_size;
    gchar             *fs_string = NULL;

    e_return_val_if_fail(G_IS_FILE(file), NULL);

    if (e_file_get_free_space(file, &fs_free, &fs_size)
            && fs_size > 0)
    {
        fs_free_str = g_format_size_full(fs_free, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
        fs_size_str = g_format_size_full(fs_size, file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
        // free disk space string
        fs_string = g_strdup_printf(_("%s of %s free(%d%% used)"),
                                     fs_free_str, fs_size_str,
                                    (gint)((fs_size - fs_free) * 100 / fs_size));
        g_free(fs_free_str);
        g_free(fs_size_str);
    }

    return fs_string;
}

gchar* e_file_get_location(GFile *file)
{
    gchar *location;

    e_return_val_if_fail(G_IS_FILE(file), NULL);

    location = g_file_get_path(file);
    if (location == NULL)
        location = g_file_get_uri(file);

    return location;
}

gboolean e_file_is_computer(GFile *file)
{
    char *uri;
    gboolean is_computer = FALSE;

    e_return_val_if_fail(G_IS_FILE(file), FALSE);

    uri = g_file_get_uri(file);
    is_computer = g_strcmp0(uri, "computer:///") == 0;
    g_free(uri);

    return is_computer;
}

gboolean e_file_is_home(GFile *file)
{
    GFile   *home;
    gboolean is_home = FALSE;

    e_return_val_if_fail(G_IS_FILE(file), FALSE);

    home = e_file_new_for_home();
    is_home = g_file_equal(home, file);
    g_object_unref(home);

    return is_home;
}

gboolean e_file_is_network(GFile *file)
{
    char *uri;
    gboolean is_network = FALSE;

    e_return_val_if_fail(G_IS_FILE(file), FALSE);

    uri = g_file_get_uri(file);
    is_network = g_strcmp0(uri, "network:///") == 0;
    g_free(uri);

    return is_network;
}

gboolean e_file_is_root(GFile *file)
{
    GFile   *parent;
    gboolean is_root = TRUE;

    parent = g_file_get_parent(file);
    if (G_UNLIKELY(parent != NULL))
    {
        is_root = FALSE;
        g_object_unref(parent);
    }

    return is_root;
}

gboolean e_file_is_trash(GFile *file)
{
    char *uri;
    gboolean is_trash = FALSE;

    e_return_val_if_fail(G_IS_FILE(file), FALSE);

    uri = g_file_get_uri(file);
    is_trash = g_strcmp0(uri, "trash:///") == 0;
    g_free(uri);

    return is_trash;
}

gboolean e_file_is_trashed(GFile *file)
{
    e_return_val_if_fail(G_IS_FILE(file), FALSE);
    return g_file_has_uri_scheme(file, "trash");
}

gboolean e_file_move(GFile                 *source,
                     GFile                 *destination,
                     GFileCopyFlags        flags,
                     GCancellable          *cancellable,
                     GFileProgressCallback progress_callback,
                     gpointer              progress_callback_data,
                     GError                **error)
{
    gboolean ret = g_file_move(source, destination, flags, cancellable,
                               progress_callback, progress_callback_data,
                               error);

    if (!error)
        return ret;

    GError *err = *error;

    // cancelled
    if (err && err->code == G_IO_ERROR_CANCELLED)
    {
        GFileType source_type = g_file_query_file_type(
                                        source,
                                        G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                        NULL);

        if (source_type == G_FILE_TYPE_REGULAR
            || source_type == G_FILE_TYPE_SYMBOLIC_LINK
            || source_type == G_FILE_TYPE_SPECIAL)
        {
            gchar *target_path = g_file_get_path(destination);
            syslog(LOG_WARNING,
                   "e_file_move: delete truncated file \"%s\"\n",
                   target_path);
            g_free(target_path);

            // remove truncated target file
            g_file_delete(destination, NULL, NULL);
        }
    }

    return ret;
}

// GList ----------------------------------------------------------------------

GList* e_list_copy(GList *list)
{
    return g_list_copy_deep(list,
                            (GCopyFunc) (void(*)(void)) g_object_ref,
                            NULL);
}

void e_list_free(GList *list)
{
    g_list_free_full(list, g_object_unref);
}

// EFileList ------------------------------------------------------------------

GType e_filelist_get_type()
{
    static GType type = G_TYPE_INVALID;

    if (G_UNLIKELY(type == G_TYPE_INVALID))
    {
        type = g_boxed_type_register_static(I_("ThunarGFileList"),
                                            (GBoxedCopyFunc) e_list_copy,
                                            (GBoxedFreeFunc) e_list_free);
    }

    return type;
}

/**
 * thunar_g_file_list_new_from_string:
 * @string : a string representation of an URI list.
 *
 * Splits an URI list conforming to the text/uri-list
 * mime type defined in RFC 2483 into individual URIs,
 * discarding any comments and whitespace. The resulting
 * list will hold one #GFile for each URI.
 *
 * If @string contains no URIs, this function
 * will return %NULL.
 *
 * Return value: the list of #GFile<!---->s or %NULL.
 **/
GList* e_filelist_new_from_string(const gchar *string)
{
    GList  *list = NULL;
    gchar **uris;
    gsize   n;

    uris = g_uri_list_extract_uris(string);

    for(n = 0; uris != NULL && uris[n] != NULL; ++n)
        list = g_list_append(list, g_file_new_for_uri(uris[n]));

    g_strfreev(uris);

    return list;
}

/**
 * thunar_g_file_list_to_stringv:
 * @list : a list of #GFile<!---->s.
 *
 * Free the returned value using g_strfreev() when you
 * are done with it. Useful for gtk_selection_data_set_uris.
 *
 * Return value: and array of uris.
 **/
gchar** e_filelist_to_stringv(GList *list)
{
    gchar **uris;
    guint   n;
    GList  *lp;

    // allocate initial string
    uris = g_new0(gchar *, g_list_length(list) + 1);

    for(lp = list, n = 0; lp != NULL; lp = lp->next)
    {
        // Prefer native paths for interoperability.
        gchar *path = g_file_get_path(G_FILE(lp->data));
        if (path == NULL)
        {
            uris[n++] = g_file_get_uri(G_FILE(lp->data));
        }
        else
        {
            uris[n++] = g_filename_to_uri(path, NULL, NULL);
            g_free(path);
        }
    }

    return uris;
}

/**
 * thunar_g_file_list_get_parents:
 * @list : a list of #GFile<!---->s.
 *
 * Collects all parent folders of the passed files
 * If multiple files share the same parent, the parent will only be added once to the returned list.
 * Each list element of the returned list needs to be freed with g_object_unref() after use.
 *
 * Return value: A list of #GFile<!---->s of all parent folders. Free the returned list with calling g_object_unref() on each element
 **/
GList* e_filelist_get_parents(GList *file_list)
{
    GList    *lp_file_list;
    GList    *lp_parent_folder_list;
    GFile    *parent_folder;
    GList    *parent_folder_list = NULL;
    gboolean  folder_already_added;

    for(lp_file_list = file_list; lp_file_list != NULL; lp_file_list = lp_file_list->next)
    {
        if (!G_IS_FILE(lp_file_list->data))
            continue;
        parent_folder = g_file_get_parent(lp_file_list->data);
        if (parent_folder == NULL)
            continue;
        folder_already_added = FALSE;
        // Check if the folder already is in our list
        for(lp_parent_folder_list = parent_folder_list; lp_parent_folder_list != NULL; lp_parent_folder_list = lp_parent_folder_list->next)
        {
            if (g_file_equal(lp_parent_folder_list->data, parent_folder))
            {
                folder_already_added = TRUE;
                break;
            }
        }
        // Keep the reference for each folder added to parent_folder_list
        if (folder_already_added)
            g_object_unref(parent_folder);
        else
            parent_folder_list = g_list_append(parent_folder_list, parent_folder);
    }
    return parent_folder_list;
}

// GKeyFile -------------------------------------------------------------------

GKeyFile* e_file_query_key_file(GFile *file, GCancellable *cancellable,
                                GError **error)
{
    e_return_val_if_fail(G_IS_FILE(file), NULL);
    e_return_val_if_fail(cancellable == NULL
                         || G_IS_CANCELLABLE(cancellable), NULL);
    e_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GKeyFile *key_file;
    gchar    *contents = NULL;
    gsize     length;

    // try to load the entire file into memory
    if (!g_file_load_contents(file, cancellable, &contents, &length, NULL, error))
        return NULL;

    // allocate a new key file
    key_file = g_key_file_new();

    // try to parse the key file from the contents of the file
    if (G_LIKELY(length == 0
                  || g_key_file_load_from_data(key_file, contents, length,
                          G_KEY_FILE_KEEP_COMMENTS
                          | G_KEY_FILE_KEEP_TRANSLATIONS,
                          error)))
    {
        g_free(contents);
        return key_file;
    }
    else
    {
        g_free(contents);
        g_key_file_free(key_file);
        return NULL;
    }
}

// VFS ------------------------------------------------------------------------

gboolean e_vfs_is_uri_scheme_supported(const gchar *scheme)
{

    e_return_val_if_fail(scheme != NULL && *scheme != '\0', FALSE);

    GVfs *gvfs = g_vfs_get_default();
    const gchar *const *supported_schemes = g_vfs_get_supported_uri_schemes(gvfs);

    if (supported_schemes == NULL)
        return FALSE;

    for(guint n = 0; supported_schemes[n] != NULL; ++n)
    {
        if (g_strcmp0(supported_schemes[n], scheme) == 0)
            return TRUE;
    }

    return FALSE;
}


