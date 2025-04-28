/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __TH_IMAGE_H__
#define __TH_IMAGE_H__

#include "th-file.h"

G_BEGIN_DECLS

// ThunarImage ----------------------------------------------------------------

typedef struct _ThunarImage        ThunarImage;
typedef struct _ThunarImageClass   ThunarImageClass;
typedef struct _ThunarImagePrivate ThunarImagePrivate;

#define TYPE_THUNARIMAGE (th_image_get_type())

#define THUNARIMAGE(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARIMAGE, ThunarImage))
#define THUNARIMAGE_CLASS(klass)    \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARIMAGE, ThunarImageClass))
#define IS_THUNARIMAGE(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARIMAGE))
#define IS_THUNARIMAGE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_THUNARIMAGE)
#define THUNARIMAGE_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARIMAGE, ThunarImageClass))

GType th_image_get_type() G_GNUC_CONST;

GtkWidget* th_image_new() G_GNUC_MALLOC;
void th_image_set_file(ThunarImage *image, ThunarFile *file);

G_END_DECLS

#endif // __TH_IMAGE_H__


