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

#ifndef __THUNAR_LIST_MODEL_H__
#define __THUNAR_LIST_MODEL_H__

#include <th-folder.h>

G_BEGIN_DECLS

typedef struct _ListModelClass ListModelClass;
typedef struct _ListModel      ListModel;

#define THUNAR_TYPE_LIST_MODEL            (listmodel_get_type())
#define THUNAR_LIST_MODEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_LIST_MODEL, ListModel))
#define THUNAR_LIST_MODEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_LIST_MODEL, ListModelClass))
#define THUNAR_IS_LIST_MODEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_LIST_MODEL))
#define THUNAR_IS_LIST_MODEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_LIST_MODEL))
#define THUNAR_LIST_MODEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_LIST_MODEL, ListModelClass))

GType listmodel_get_type() G_GNUC_CONST;

ListModel* list_model_new();

ThunarFolder* list_model_get_folder(ListModel *store);
void list_model_set_folder(ListModel *store, ThunarFolder *folder);

void list_model_set_folders_first(ListModel *store, gboolean folders_first);

gboolean list_model_get_show_hidden(ListModel *store);
void list_model_set_show_hidden(ListModel *store, gboolean show_hidden);

gboolean list_model_get_file_size_binary(ListModel *store);
void list_model_set_file_size_binary(ListModel *store, gboolean file_size_binary);

ThunarFile* list_model_get_file(ListModel *store, GtkTreeIter *iter);

GList* list_model_get_paths_for_files(ListModel *store, GList *files);
GList* list_model_get_paths_for_pattern(ListModel *store, const gchar *pattern);

gchar* list_model_get_statusbar_text(ListModel *store, GList *selected_items);

G_END_DECLS

#endif // __THUNAR_LIST_MODEL_H__

