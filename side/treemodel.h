/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __TREEMODEL_H__
#define __TREEMODEL_H__

#include "th-file.h"

G_BEGIN_DECLS

// Globals --------------------------------------------------------------------

typedef enum
{
    TREEMODEL_COLUMN_FILE,
    TREEMODEL_COLUMN_NAME,
    TREEMODEL_COLUMN_ATTR,
    TREEMODEL_COLUMN_DEVICE,
    TREEMODEL_N_COLUMNS,

} TreeModelColumn;

// TreeModel ------------------------------------------------------------------

typedef struct _TreeModel      TreeModel;
typedef struct _TreeModelClass TreeModelClass;

typedef gboolean (*TreeModelVisibleFunc) (TreeModel *model, ThunarFile *file,
                                          gpointer data);

#define TYPE_TREEMODEL (treemodel_get_type())

#define TREEMODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_TREEMODEL, TreeModel))
#define TREEMODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_TREEMODEL, TreeModelClass))
#define THUNAR_IS_TREE_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_TREEMODEL))
#define THUNAR_IS_TREE_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_TREEMODEL))
#define TREEMODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_TREEMODEL, TreeModelClass))

GType treemodel_get_type() G_GNUC_CONST;

void treemodel_set_visible_func(TreeModel *model,
                                TreeModelVisibleFunc func, gpointer data);
void treemodel_cleanup(TreeModel *model);
void treemodel_refilter(TreeModel *model);

void treemodel_add_child(TreeModel *model, GNode *node, ThunarFile *file);

G_END_DECLS

#endif // __TREEMODEL_H__


