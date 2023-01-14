/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA 02110-1301 USA
 */

#ifndef __PIXBUF_EXT_H__
#define __PIXBUF_EXT_H__

#include <gdk/gdk.h>

G_BEGIN_DECLS

GdkPixbuf* pixbuf_colorize(const GdkPixbuf *source, const GdkColor *color)
                           G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf* pixbuf_spotlight(const GdkPixbuf *source)
                            G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf* pixbuf_scale_down(GdkPixbuf *source,
                             gboolean  preserve_aspect_ratio,
                             gint      dest_width,
                             gint      dest_height)
                             G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif // __PIXBUF_EXT_H__


