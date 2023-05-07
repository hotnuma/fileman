/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __DETAILVIEW_H__
#define __DETAILVIEW_H__

#include <standardview.h>

G_BEGIN_DECLS

typedef struct _DetailViewClass DetailViewClass;
typedef struct _DetailView      DetailView;

#define TYPE_DETAILVIEW (detailview_get_type ())
#define DETAILVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DETAILVIEW, DetailView))
#define DETAILVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_DETAILVIEW, DetailViewClass))
#define IS_DETAILVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DETAILVIEW))
#define IS_DETAILVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_DETAILVIEW))
#define DETAILVIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_DETAILVIEW, DetailViewClass))

// Action Entrys provided by this widget

typedef enum
{
    DETAILVIEW_ACTION_CONFIGURE_COLUMNS,

} DetailViewAction;

GType detailview_get_type() G_GNUC_CONST;

G_END_DECLS

#endif // __DETAILVIEW_H__


