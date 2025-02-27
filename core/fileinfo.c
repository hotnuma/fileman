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

#include "config.h"
#include "fileinfo.h"

#include "gio-ext.h"

// FileInfo -------------------------------------------------------------------

enum
{
    CHANGED,
    RENAMED,
    LAST_SIGNAL,
};
static guint _fileinfo_signals[LAST_SIGNAL];

GType fileinfo_get_type()
{
    static volatile gsize type__volatile = 0;
    GType type;

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

        _fileinfo_signals[CHANGED] =
            g_signal_new(I_("changed"),
                         type,
                         G_SIGNAL_RUN_FIRST,
                         G_STRUCT_OFFSET(FileInfoIface, changed),
                         NULL, NULL,
                         g_cclosure_marshal_VOID__VOID,
                         G_TYPE_NONE, 0);

        _fileinfo_signals[RENAMED] =
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

gchar* fileinfo_get_name(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_name(file_info);
}

gchar* fileinfo_get_uri(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_uri(file_info);
}

/*
 * Note that the parent URI
 * may be of a different type than the
 * URI of @file_info. For example, the
 * parent of "file:///" is "computer:///".
 */
gchar* fileinfo_get_parent_uri(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_parent_uri(file_info);
}

gchar* fileinfo_get_uri_scheme(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_uri_scheme(file_info);
}

gchar* fileinfo_get_mime_type(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_mime_type(file_info);
}

gboolean fileinfo_has_mime_type(FileInfo *file_info, const gchar *mime_type)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), FALSE);
    g_return_val_if_fail(mime_type != NULL, FALSE);

    return FILEINFO_GET_IFACE(file_info)->has_mime_type(file_info, mime_type);
}

gboolean fileinfo_is_directory(FileInfo *file_info)
{
    g_return_val_if_fail(IS_FILEINFO(file_info), FALSE);

    return FILEINFO_GET_IFACE(file_info)->is_directory(file_info);
}

GFileInfo* fileinfo_get_file_info(FileInfo *file_info)
{
    // g_object_unref when unneeded

    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_file_info(file_info);
}

GFileInfo* fileinfo_get_filesystem_info(FileInfo *file_info)
{
    // g_object_unref when unneeded

    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_filesystem_info(file_info);
}

GFile* fileinfo_get_location(FileInfo *file_info)
{
    // g_object_unref when unneeded

    g_return_val_if_fail(IS_FILEINFO(file_info), NULL);

    return FILEINFO_GET_IFACE(file_info)->get_location(file_info);
}

// The file manager uses this method to emit the ::changed signal.
void fileinfo_changed(FileInfo *file_info)
{
    g_return_if_fail(IS_FILEINFO(file_info));

    g_signal_emit(G_OBJECT(file_info), _fileinfo_signals[CHANGED], 0);
}

// The file manager emits this signal whenever the user renamed the file_info.
void fileinfo_renamed(FileInfo *file_info)
{
    g_return_if_fail(IS_FILEINFO(file_info));

    g_signal_emit(G_OBJECT(file_info), _fileinfo_signals[RENAMED], 0);
}

// FileInfoList ---------------------------------------------------------------

GType fileinfolist_get_type()
{
    static GType type = G_TYPE_INVALID;

    if (type == G_TYPE_INVALID)
    {
        type = g_boxed_type_register_static(I_("FileInfoList"),
                                            (GBoxedCopyFunc) e_list_copy,
                                            (GBoxedFreeFunc) e_list_free);
    }

    return type;
}


