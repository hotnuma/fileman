/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
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

#ifndef __TH_FOLDER_H__
#define __TH_FOLDER_H__

#include "th-file.h"

G_BEGIN_DECLS

// ThunarFolder ---------------------------------------------------------------

typedef struct _ThunarFolderClass ThunarFolderClass;
typedef struct _ThunarFolder      ThunarFolder;

#define TYPE_THUNARFOLDER (th_folder_get_type())
#define THUNARFOLDER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARFOLDER, ThunarFolder))
#define THUNARFOLDER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARFOLDER, ThunarFolderClass))
#define IS_THUNARFOLDER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARFOLDER))
#define IS_THUNARFOLDER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_THUNARFOLDER))
#define THUNARFOLDER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARFOLDER, ThunarFolderClass))

GType th_folder_get_type() G_GNUC_CONST;

// property
gboolean th_folder_get_loading(const ThunarFolder *folder);

void th_folder_load(ThunarFolder *folder, gboolean reload_info);

ThunarFolder* th_folder_get_for_thfile(ThunarFile *file);
ThunarFile* th_folder_get_thfile(const ThunarFolder *folder);

gboolean th_folder_has_folder_monitor(const ThunarFolder *folder);
GList* th_folder_get_files(const ThunarFolder *folder);

G_END_DECLS

#endif // __TH_FOLDER_H__


