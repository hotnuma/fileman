/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __COLUMNMODEL_H__
#define __COLUMNMODEL_H__

#include "enumtypes.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

#define COL_MIN_WIDTH 50

typedef enum
{
    COLUMN_MODEL_COLUMN_NAME,
    COLUMN_MODEL_COLUMN_MUTABLE,
    COLUMN_MODEL_COLUMN_VISIBLE,
    COLUMN_MODEL_N_COLUMNS,

} ColumnModelColumn;

// ColumnModel ----------------------------------------------------------------

typedef struct _ColumnModelClass ColumnModelClass;
typedef struct _ColumnModel      ColumnModel;

#define TYPE_COLUMN_MODEL (colmodel_get_type())

#define COLUMN_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_COLUMN_MODEL, ColumnModel))
#define COLUMN_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_COLUMN_MODEL, ColumnModelClass))
#define IS_COLUMN_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_COLUMN_MODEL))
#define IS_COLUMN_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_COLUMN_MODEL))
#define COLUMN_MODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_COLUMN_MODEL, ColumnModelClass))

GType colmodel_get_type() G_GNUC_CONST;

ColumnModel* colmodel_get_default();

void colmodel_exchange(ColumnModel *column_model,
                           GtkTreeIter *iter1, GtkTreeIter *iter2);
ThunarColumn colmodel_get_column_for_iter(ColumnModel *column_model,
                                              GtkTreeIter *iter);
const ThunarColumn* colmodel_get_column_order(ColumnModel *column_model);
const gchar* colmodel_get_column_name(ColumnModel *column_model,
                                          ThunarColumn column);
gboolean colmodel_get_column_visible(ColumnModel *column_model,
                                         ThunarColumn column);
void colmodel_set_column_visible(ColumnModel *column_model,
                                     ThunarColumn column, gboolean visible);
gint colmodel_get_column_width(ColumnModel *column_model,
                                   ThunarColumn column);
void colmodel_set_column_width(ColumnModel *column_model,
                                   ThunarColumn column, gint width);

G_END_DECLS

#endif // __COLUMNMODEL_H__


