/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
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

#ifndef __ICONRENDERER_H__
#define __ICONRENDERER_H__

#include <enumtypes.h>
#include <th_file.h>

G_BEGIN_DECLS

typedef struct _IconRendererClass IconRendererClass;
typedef struct _IconRenderer      IconRenderer;

#define TYPE_ICONRENDERER (irenderer_get_type())
#define ICONRENDERER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_ICONRENDERER, IconRenderer))
#define ICONRENDERER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_ICONRENDERER, IconRendererClass))
#define ICONRENDERER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_ICONRENDERER, IconRendererClass))
#define IS_ICONRENDERER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_ICONRENDERER))
#define IS_ICONRENDERER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_ICONRENDERER))

struct _IconRendererClass
{
    GtkCellRendererClass __parent__;
};

struct _IconRenderer
{
    GtkCellRenderer __parent__;

    ThunarFile      *drop_file;
    ThunarFile      *file;
    gboolean        emblems;
    gboolean        follow_state;
    ThunarIconSize  size;
};

GType irenderer_get_type() G_GNUC_CONST;

GtkCellRenderer* irenderer_new() G_GNUC_MALLOC;

G_END_DECLS

#endif // __ICONRENDERER_H__


