/*-
 * Copyright (c) 2004-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __LISTMODEL_H__
#define __LISTMODEL_H__

#include <th-folder.h>

G_BEGIN_DECLS

// Allocation -----------------------------------------------------------------

typedef struct _ListModelClass ListModelClass;
typedef struct _ListModel      ListModel;

#define TYPE_LISTMODEL (listmodel_get_type())
#define LISTMODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_LISTMODEL, ListModel))
#define LISTMODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_LISTMODEL, ListModelClass))
#define IS_LISTMODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_LISTMODEL))
#define IS_LISTMODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_LISTMODEL))
#define LISTMODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_LISTMODEL, ListModelClass))

GType listmodel_get_type() G_GNUC_CONST;

ListModel* listmodel_new();

// Get/set --------------------------------------------------------------------

ThunarFolder* listmodel_get_folder(ListModel *store);
void listmodel_set_folder(ListModel *store, ThunarFolder *folder);

void listmodel_set_folders_first(ListModel *store, gboolean folders_first);

gboolean listmodel_get_show_hidden(ListModel *store);
void listmodel_set_show_hidden(ListModel *store, gboolean show_hidden);

gboolean listmodel_get_file_size_binary(ListModel *store);
void listmodel_set_file_size_binary(ListModel *store, gboolean file_size_binary);

// Public ---------------------------------------------------------------------

ThunarFile* listmodel_get_file(ListModel *store, GtkTreeIter *iter);
GList* listmodel_get_paths_for_files(ListModel *store, GList *files);
GList* listmodel_get_treepaths(ListModel *store, const gchar *pattern);
gchar* listmodel_get_statusbar_text(ListModel *store, GList *selected_items);

G_END_DECLS

#endif // __LISTMODEL_H__


