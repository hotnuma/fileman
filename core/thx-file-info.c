/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include <config.h>
#include <thx-file-info.h>

#include <gio-ext.h>

/* Signal identifiers */
enum
{
    CHANGED,
    RENAMED,
    LAST_SIGNAL,
};

static guint _fi_signals[LAST_SIGNAL];

GType fileinfo_get_type()
{
    static volatile gsize type__volatile = 0;
    GType                 type;

    if (g_once_init_enter((gsize*) &type__volatile))
    {
        type = g_type_register_static_simple(G_TYPE_INTERFACE,
                                              I_("FileInfo"),
                                              sizeof(FileInfoIface),
                                              NULL,
                                              0,
                                              NULL,
                                              0);

        g_type_interface_add_prerequisite(type, G_TYPE_OBJECT);

        /*
         * Thunar plugins should use this signal to stay informed about
         * changes to a @file_info for which they currently display
         * information(i.e. in a #ThunarxPropertyPage), and update
         * it's user interface whenever a change is noticed on @file_info.
         */
        _fi_signals[CHANGED] =
            g_signal_new(I_("changed"),
                          type,
                          G_SIGNAL_RUN_FIRST,
                          G_STRUCT_OFFSET(FileInfoIface, changed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        /*
         * For example, within Thunar, #ThunarFolder uses this
         * signal to reregister it's VFS directory monitor, after
         * the corresponding file was renamed.
         */
        _fi_signals[RENAMED] =
            g_signal_new(I_("renamed"),
                          type,
                          G_SIGNAL_RUN_FIRST,
                          G_STRUCT_OFFSET(FileInfoIface, renamed),
                          NULL, NULL,
                          g_cclosure_marshal_VOID__VOID,
                          G_TYPE_NONE, 0);

        g_once_init_leave(&type__volatile, type);
    }

    return type__volatile;
}

gchar* thunarx_file_info_get_name(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_name(file_info);
}

gchar* thunarx_file_info_get_uri(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_uri(file_info);
}

/*
 * Note that the parent URI
 * may be of a different type than the
 * URI of @file_info. For example, the
 * parent of "file:///" is "computer:///".
 */
gchar* thunarx_file_info_get_parent_uri(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_parent_uri(file_info);
}

gchar* thunarx_file_info_get_uri_scheme(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_uri_scheme(file_info);
}

gchar* thunarx_file_info_get_mime_type(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_mime_type(file_info);
}



/**
 * thunarx_file_info_has_mime_type:
 * @file_info : a #FileInfo.
 * @mime_type : a MIME-type(e.g. "text/plain").
 *
 * Checks whether @file_info is of the given @mime_type
 * or whether the MIME-type of @file_info is a subclass
 * of @mime_type.
 *
 * This is the preferred way for most extensions to check
 * whether they support a given file or not, and you should
 * consider using this method rather than
 * thunarx_file_info_get_mime_type(). A simple example would
 * be a menu extension that performs a certain action on
 * text files. In this case you want to check whether a given
 * #FileInfo refers to any kind of text file, not only
 * to "text/plain"(e.g. this also includes "text/xml" and
 * "application/x-desktop").
 *
 * But you should be aware that this method may take some
 * time to test whether @mime_type is valid for @file_info,
 * so don't call it too often.
 *
 * Return value: %TRUE if @mime_type is valid for @file_info,
 *               else %FALSE.
 **/
gboolean thunarx_file_info_has_mime_type(FileInfo *file_info,
                                         const gchar     *mime_type)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), FALSE);
    g_return_val_if_fail(mime_type != NULL, FALSE);

    return FILE_INFO_GET_IFACE(file_info)->has_mime_type(file_info, mime_type);
}



/**
 * thunarx_file_info_is_directory:
 * @file_info : a #FileInfo.
 *
 * Checks whether @file_info refers to a directory.
 *
 * Return value: %TRUE if @file_info is a directory.
 **/
gboolean thunarx_file_info_is_directory(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), FALSE);

    return FILE_INFO_GET_IFACE(file_info)->is_directory(file_info);
}



/**
 * thunarx_file_info_get_file_info:
 * @file_info : a #FileInfo.
 *
 * Returns the #GFileInfo associated with @file_info,
 * which includes additional information about the @file_info
 * as queried from GIO earlier. The caller is responsible to free the
 * returned #GFileInfo object using g_object_unref() when
 * no longer needed.
 *
 * Returns:(transfer full): the #GFileInfo object associated with @file_info,
 *          which MUST be freed using g_object_unref().
 **/
GFileInfo* thunarx_file_info_get_file_info(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_file_info(file_info);
}



/**
 * thunarx_file_info_get_filesystem_info:
 * @file_info : a #FileInfo.
 *
 * Returns the #GFileInfo which includes additional information about
 * the filesystem @file_info resides on. The caller is responsible to
 * free the returned #GFileInfo object using g_object_unref() when
 * no longer needed.
 *
 * Returns:(transfer full): the #GFileInfo containing information about the
 *          filesystem of @file_info or %NULL if no filesystem information is
 *          available. It MUST be released using g_object_unref().
 **/
GFileInfo* thunarx_file_info_get_filesystem_info(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);
    return FILE_INFO_GET_IFACE(file_info)->get_filesystem_info(file_info);
}



/**
 * thunarx_file_info_get_location:
 * @file_info : a #FileInfo.
 *
 * Returns the #GFile @file_info points to. The #GFile is a more
 * powerful tool than just the URI or the path. The caller
 * is responsible to release the returned #GFile using g_object_unref()
 * when no longer needed.
 *
 * Returns:(transfer full): the #GFile to which @file_info points. It MUST be
 *          released using g_object_unref().
 **/
GFile* thunarx_file_info_get_location(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILE_INFO(file_info), NULL);

    return FILE_INFO_GET_IFACE(file_info)->get_location(file_info);
}



/**
 * thunarx_file_info_changed:
 * @file_info : a #FileInfo.
 *
 * Emits the ::changed signal on @file_info. This method should not
 * be invoked by Thunar plugins, instead the file manager itself
 * will use this method to emit ::changed whenever it notices a
 * change on @file_info.
 **/
void thunarx_file_info_changed(FileInfo *file_info)
{
    g_return_if_fail(IS_FILE_INFO(file_info));

    g_signal_emit(G_OBJECT(file_info), _fi_signals[CHANGED], 0);
}



/**
 * thunarx_file_info_renamed:
 * @file_info : a #FileInfo.
 *
 * Emits the ::renamed signal on @file_info. This method should
 * not be invoked by Thunar plugins, instead the file manager
 * will emit this signal whenever the user renamed the @file_info.
 *
 * The plugins should instead connect to the ::renamed signal
 * and update it's internal state and it's user interface
 * after the file manager renamed a file.
 **/
void thunarx_file_info_renamed(FileInfo *file_info)
{
    g_return_if_fail(IS_FILE_INFO(file_info));
    g_signal_emit(G_OBJECT(file_info), _fi_signals[RENAMED], 0);
}



GType thunarx_file_info_list_get_type()
{
    static GType type = G_TYPE_INVALID;

    if (G_UNLIKELY(type == G_TYPE_INVALID))
    {
        type = g_boxed_type_register_static(I_("FileInfoList"),
                                            (GBoxedCopyFunc) eg_list_copy,
                                            (GBoxedFreeFunc) eg_list_free);
    }

    return type;
}

#if 0
GList* thunarx_file_info_list_copy(GList *file_infos)
{
    return g_list_copy_deep(file_infos,(GCopyFunc)(void(*)(void)) g_object_ref, NULL);
}

void thunarx_file_info_list_free(GList *file_infos)
{
    g_list_free_full(file_infos, g_object_unref);
}

#endif


