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

#ifndef __FILE_INFO_H__
#define __FILE_INFO_H__

#include <gio/gio.h>

G_BEGIN_DECLS

/*
 * File information namespaces available in the GFileInfo returned by
 * thunarx_file_info_get_file_info().
 */
#define FILE_INFO_NAMESPACE \
  "access::*," \
  "id::filesystem," \
  "mountable::can-mount,standard::target-uri," \
  "preview::*," \
  "standard::type,standard::is-hidden,standard::is-backup," \
  "standard::is-symlink,standard::name,standard::display-name," \
  "standard::size,standard::symlink-target," \
  "time::*," \
  "trash::*," \
  "unix::gid,unix::uid,unix::mode," \
  "metadata::emblems," \
  "metadata::thunar-view-type," \
  "metadata::thunar-sort-column,metadata::thunar-sort-order"

/*
 * Filesystem information namespaces available in the #GFileInfo
 * returned by thunarx_file_info_get_filesystem_info().
 */
#define FILESYSTEM_INFO_NAMESPACE \
  "filesystem::*"

typedef struct _FileInfoIface FileInfoIface;
typedef struct _FileInfo      FileInfo;

#define TYPE_FILE_INFO (fileinfo_get_type())
#define FILE_INFO(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),    TYPE_FILE_INFO, FileInfo))
#define IS_FILE_INFO(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),    TYPE_FILE_INFO))
#define FILE_INFO_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj), TYPE_FILE_INFO, FileInfoIface))

struct _FileInfoIface
{
    GTypeInterface __parent__;

    // virtual methods
    gchar*      (*get_name) (FileInfo *file_info);

    gchar*      (*get_uri) (FileInfo *file_info);
    gchar*      (*get_parent_uri) (FileInfo *file_info);
    gchar*      (*get_uri_scheme) (FileInfo *file_info);

    gchar*      (*get_mime_type) (FileInfo *file_info);
    gboolean    (*has_mime_type) (FileInfo *file_info,
                                  const gchar     *mime_type);

    gboolean    (*is_directory) (FileInfo *file_info);

    GFileInfo*  (*get_file_info) (FileInfo *file_info);
    GFileInfo*  (*get_filesystem_info) (FileInfo *file_info);
    GFile*      (*get_location) (FileInfo *file_info);

    void (*reserved0) (void);
    void (*reserved1) (void);
    void (*reserved2) (void);
    void (*reserved3) (void);
    void (*reserved4) (void);
    void (*reserved5) (void);
    void (*reserved6) (void);

    // signals
    void (*changed) (FileInfo *file_info);
    void (*renamed) (FileInfo *file_info);

    void (*reserved7) (void);
    void (*reserved8) (void);
    void (*reserved9) (void);
};

GType       fileinfo_get_type() G_GNUC_CONST;

gchar*      fileinfo_get_name(FileInfo *file_info);
gchar*      fileinfo_get_uri(FileInfo *file_info);
gchar*      fileinfo_get_parent_uri(FileInfo *file_info);
gchar*      fileinfo_get_uri_scheme(FileInfo *file_info);

gchar*      fileinfo_get_mime_type(FileInfo *file_info);
gboolean    fileinfo_has_mime_type(FileInfo *file_info,
                                            const gchar *mime_type);

gboolean    fileinfo_is_directory(FileInfo *file_info);

GFileInfo*  fileinfo_get_file_info(FileInfo *file_info);
GFileInfo*  fileinfo_get_filesystem_info(FileInfo *file_info);
GFile*      fileinfo_get_location(FileInfo *file_info);

void        fileinfo_changed(FileInfo *file_info);
void        fileinfo_renamed(FileInfo *file_info);

#define TYPE_FILE_INFO_LIST (fileinfo_list_get_type())

GType       fileinfo_list_get_type() G_GNUC_CONST;

G_END_DECLS

#endif // __FILE_INFO_H__


