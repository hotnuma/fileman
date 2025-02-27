/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __SIZELABEL_H__
#define __SIZELABEL_H__

#include "th-file.h"

G_BEGIN_DECLS

// SizeLabel ------------------------------------------------------------------

typedef struct _SizeLabelClass SizeLabelClass;
typedef struct _SizeLabel      SizeLabel;

#define TYPE_SIZELABEL (szlabel_get_type())
#define SIZELABEL(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_SIZELABEL, SizeLabel))
#define SIZELABEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_SIZELABEL, SizeLabelClass))
#define IS_SIZELABEL(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_SIZELABEL))
#define IS_SIZELABEL_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_SIZELABEL))
#define SIZELABEL_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_SIZELABEL, SizeLabelClass))

GType szlabel_get_type() G_GNUC_CONST;

GtkWidget* szlabel_new() G_GNUC_MALLOC;

G_END_DECLS

#endif // __SIZELABEL_H__


