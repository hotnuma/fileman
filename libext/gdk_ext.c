/*-
 * Copyright(c) 2003-2007 Benedikt Meurer <benny@xfce.org>
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

#include <config.h>
#include <gdk_ext.h>

static const cairo_user_data_key_t _cairo_key;

static cairo_surface_t* _egdk_cairo_create_surface(const GdkPixbuf *pixbuf);

/**
 * thunar_gdk_cairo_set_source_pixbuf:
 * cr       : a Cairo context
 * pixbuf   : a GdkPixbuf
 * pixbuf_x : X coordinate of location to place upper left corner of pixbuf
 * pixbuf_y : Y coordinate of location to place upper left corner of pixbuf
 *
 * Works like gdk_cairo_set_source_pixbuf but we try to cache the surface
 * on the pixbuf, which is efficient within Thunar because we also share
 * the pixbufs using the icon cache.
 **/
void edk_cairo_set_source_pixbuf(cairo_t *cr, GdkPixbuf *pixbuf,
                                 gdouble pixbuf_x, gdouble pixbuf_y)
{
    cairo_surface_t *surface;
    static GQuark    surface_quark = 0;

    if (surface_quark == 0)
        surface_quark = g_quark_from_static_string("thunar-gdk-surface");

    // peek if there is already a surface
    surface = g_object_get_qdata(G_OBJECT(pixbuf), surface_quark);
    if (surface == NULL)
    {
        // create a new surface
        surface = _egdk_cairo_create_surface(pixbuf);

        // store the pixbuf on the pixbuf
        g_object_set_qdata_full(G_OBJECT(pixbuf), surface_quark,
                                 surface,(GDestroyNotify) cairo_surface_destroy);
    }

    // apply
    cairo_set_source_surface(cr, surface, pixbuf_x, pixbuf_y);
}

static cairo_surface_t* _egdk_cairo_create_surface(const GdkPixbuf *pixbuf)
{
    gint             width;
    gint             height;
    guchar          *gdk_pixels;
    gint             gdk_rowstride;
    gint             n_channels;
    gint             cairo_stride;
    guchar          *cairo_pixels;
    cairo_format_t   format;
    cairo_surface_t *surface;
    gint             j;
    guchar          *p, *q;
    guchar          *end;

    e_return_val_if_fail(GDK_IS_PIXBUF(pixbuf), NULL);

    // get pixbuf information
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);
    gdk_pixels = gdk_pixbuf_get_pixels(pixbuf);
    gdk_rowstride = gdk_pixbuf_get_rowstride(pixbuf);
    n_channels = gdk_pixbuf_get_n_channels(pixbuf);

    if (n_channels == 3)
        format = CAIRO_FORMAT_RGB24;
    else
        format = CAIRO_FORMAT_ARGB32;

    // prepare pixel data and surface
    cairo_stride = cairo_format_stride_for_width(format, width);
    cairo_pixels = g_malloc_n(height, cairo_stride);
    surface = cairo_image_surface_create_for_data(cairo_pixels, format,
              width, height, cairo_stride);
    cairo_surface_set_user_data(surface, &_cairo_key, cairo_pixels, g_free);

    // convert format
    if (n_channels == 3)
    {
        for (j = height; j; j--)
        {
            p = gdk_pixels;
            q = cairo_pixels;
            end = p + 3 * width;

            while (p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                q[0] = p[2];
                q[1] = p[1];
                q[2] = p[0];
#else
                q[1] = p[0];
                q[2] = p[1];
                q[3] = p[2];
#endif
                p += 3;
                q += 4;
            }

            gdk_pixels += gdk_rowstride;
            cairo_pixels += cairo_stride;
        }
    }
    else
    {
#define MULT(d,c,a) G_STMT_START { guint t = c * a + 0x7f; d =((t >> 8) + t) >> 8; } G_STMT_END
        for (j = height; j; j--)
        {
            p = gdk_pixels;
            q = cairo_pixels;
            end = p + 4 * width;

            while(p < end)
            {
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
                MULT(q[0], p[2], p[3]);
                MULT(q[1], p[1], p[3]);
                MULT(q[2], p[0], p[3]);
                q[3] = p[3];
#else
                q[0] = p[3];
                MULT(q[1], p[0], p[3]);
                MULT(q[2], p[1], p[3]);
                MULT(q[3], p[2], p[3]);
#endif

                p += 4;
                q += 4;
            }

            gdk_pixels += gdk_rowstride;
            cairo_pixels += cairo_stride;
        }
#undef MULT
    }

    return surface;
}


