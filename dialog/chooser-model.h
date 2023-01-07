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

#ifndef __CHOOSER_MODEL_H__
#define __CHOOSER_MODEL_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ChooserModelClass ChooserModelClass;
typedef struct _ChooserModel      ChooserModel;

#define THUNAR_TYPE_CHOOSER_MODEL (chooser_model_get_type ())
#define THUNAR_CHOOSER_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_CHOOSER_MODEL, ChooserModel))
#define THUNAR_CHOOSER_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_CHOOSER_MODEL, ChooserModelClass))
#define THUNAR_IS_CHOOSER_MODEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_CHOOSER_MODEL))
#define THUNAR_IS_CHOOSER_MODEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_CHOOSER_MODEL))
#define THUNAR_CHOOSER_MODEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_CHOOSER_MODEL, ChooserModelClass))

typedef enum
{
    CHOOSER_MODEL_COLUMN_NAME,
    CHOOSER_MODEL_COLUMN_ICON,
    CHOOSER_MODEL_COLUMN_APPLICATION,
    CHOOSER_MODEL_COLUMN_STYLE,
    CHOOSER_MODEL_COLUMN_WEIGHT,
    CHOOSER_MODEL_N_COLUMNS,

} ChooserModelColumn;

GType chooser_model_get_type() G_GNUC_CONST;

ChooserModel* chooser_model_new(const gchar *content_type)
                                             G_GNUC_MALLOC;
const gchar* chooser_model_get_content_type(ChooserModel *model);
gboolean chooser_model_remove(ChooserModel *model,
                                     GtkTreeIter    *iter,
                                     GError         **error);

G_END_DECLS

#endif // __CHOOSER_MODEL_H__


