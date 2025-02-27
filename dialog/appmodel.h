/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __APPCHOOSER_MODEL_H__
#define __APPCHOOSER_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

// AppChooserModel ------------------------------------------------------------

typedef struct _AppChooserModelClass AppChooserModelClass;
typedef struct _AppChooserModel      AppChooserModel;

#define TYPE_APPCHOOSER_MODEL (appmodel_get_type())
#define APPCHOOSER_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_APPCHOOSER_MODEL, AppChooserModel))
#define APPCHOOSER_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_APPCHOOSER_MODEL, AppChooserModelClass))
#define IS_APPCHOOSER_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_APPCHOOSER_MODEL))
#define IS_APPCHOOSER_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_APPCHOOSER_MODEL))
#define APPCHOOSER_MODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_APPCHOOSER_MODEL, AppChooserModelClass))

typedef enum
{
    APPCHOOSER_COLUMN_NAME,
    APPCHOOSER_COLUMN_ICON,
    APPCHOOSER_COLUMN_APPLICATION,
    APPCHOOSER_COLUMN_STYLE,
    APPCHOOSER_COLUMN_WEIGHT,
    APPCHOOSER_N_COLUMNS,

} AppChooserModelColumn;

GType appmodel_get_type() G_GNUC_CONST;

AppChooserModel* appmodel_new(const gchar *content_type) G_GNUC_MALLOC;
gboolean appmodel_remove(AppChooserModel *model, GtkTreeIter *iter, GError **error);

G_END_DECLS

#endif // __APPCHOOSER_MODEL_H__


