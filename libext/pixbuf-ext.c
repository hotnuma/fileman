/*-
 * Copyright(c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
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

#include <config.h>

#include <pixbuf-ext.h>
#include <math.h>

static inline guchar _lighten_channel(guchar cur_value);

/**
 * egdk_pixbuf_colorize:
 * @source : the source #GdkPixbuf.
 * @color  : the new color.
 *
 * Creates a new #GdkPixbuf based on @source, which is
 * colorized to @color.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Returns: the colorized #GdkPixbuf.
 *
 * Since: 0.3.1.3
 **/
GdkPixbuf* pixbuf_colorize(const GdkPixbuf *source, const GdkColor  *color)
{
    GdkPixbuf *dst;
    gboolean   has_alpha;
    gint       dst_row_stride;
    gint       src_row_stride;
    gint       width;
    gint       height;
    gint       i;

    // determine source parameters
    width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(source);
    has_alpha = gdk_pixbuf_get_has_alpha(source);

    // allocate the destination pixbuf
    dst = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(source),
                         has_alpha,
                         gdk_pixbuf_get_bits_per_sample(source),
                         width,
                         height);

    // determine row strides on src/dst
    dst_row_stride = gdk_pixbuf_get_rowstride(dst);
    src_row_stride = gdk_pixbuf_get_rowstride(source);

#if defined(__GNUC__) && defined(__NO_MMX__)
    // check if there's a good reason to use MMX
    if (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0)
    {
        __m64 *pixdst =(__m64 *) gdk_pixbuf_get_pixels(dst);
        __m64 *pixsrc =(__m64 *) gdk_pixbuf_get_pixels(source);
        __m64  alpha_mask = _mm_set_pi8(0xff, 0, 0, 0, 0xff, 0, 0, 0);
        __m64  color_factor = _mm_set_pi16(0, color->blue, color->green, color->red);
        __m64  zero = _mm_setzero_si64();
        __m64  src, alpha, hi, lo;

        // divide color components by 256
        color_factor = _mm_srli_pi16(color_factor, 8);

        for(i =(width * height) >> 1; i > 0; --i)
        {
            // read the source pixel
            src = *pixsrc;

            // remember the two alpha values
            alpha = _mm_and_si64(alpha_mask, src);

            // extract the hi pixel
            hi = _mm_unpackhi_pi8(src, zero);
            hi = _mm_mullo_pi16(hi, color_factor);

            // extract the lo pixel
            lo = _mm_unpacklo_pi8(src, zero);
            lo = _mm_mullo_pi16(lo, color_factor);

            // prefetch the next two pixels
            __builtin_prefetch(++pixsrc, 0, 1);

            // divide by 256
            hi = _mm_srli_pi16(hi, 8);
            lo = _mm_srli_pi16(lo, 8);

            // combine the 2 pixels again
            src = _mm_packs_pu16(lo, hi);

            // write back the calculated color together with the alpha
            *pixdst = _mm_or_si64(alpha, src);

            // advance the dest pointer
            ++pixdst;
        }

        _mm_empty();
    }
    else
#endif
    {
        guchar *dst_pixels = gdk_pixbuf_get_pixels(dst);
        guchar *src_pixels = gdk_pixbuf_get_pixels(source);
        guchar *pixdst;
        guchar *pixsrc;
        gint    red_value = color->red / 255.0;
        gint    green_value = color->green / 255.0;
        gint    blue_value = color->blue / 255.0;
        gint    j;

        for(i = height; --i >= 0; )
        {
            pixdst = dst_pixels + i * dst_row_stride;
            pixsrc = src_pixels + i * src_row_stride;

            for(j = width; j > 0; --j)
            {
                *pixdst++ =(*pixsrc++ * red_value) >> 8;
                *pixdst++ =(*pixsrc++ * green_value) >> 8;
                *pixdst++ =(*pixsrc++ * blue_value) >> 8;

                if(has_alpha)
                    *pixdst++ = *pixsrc++;
            }
        }
    }

    return dst;
}

/**
 * egdk_pixbuf_scale_down:
 * @source                : the source #GdkPixbuf.
 * @preserve_aspect_ratio : %TRUE to preserve aspect ratio.
 * @dest_width            : the max width for the result.
 * @dest_height           : the max height for the result.
 *
 * Scales down the @source to fit into the given @width and
 * @height. If @aspect_ratio is %TRUE then the aspect ratio
 * of @source will be preserved.
 *
 * If @width is larger than the width of @source and @height
 * is larger than the height of @source, a reference to
 * @source will be returned, as it's unneccesary then to
 * scale down.
 *
 * The caller is responsible to free the returned #GdkPixbuf
 * using g_object_unref() when no longer needed.
 *
 * Returns: the resulting #GdkPixbuf.
 *
 * Since: 0.3.1.1
 **/
GdkPixbuf* pixbuf_scale_down(GdkPixbuf *source, gboolean preserve_ratio,
                             gint dest_width, gint dest_height)
{
    gdouble wratio;
    gdouble hratio;
    gint    source_width;
    gint    source_height;

    g_return_val_if_fail(GDK_IS_PIXBUF(source), NULL);
    g_return_val_if_fail(dest_width > 0, NULL);
    g_return_val_if_fail(dest_height > 0, NULL);

    source_width = gdk_pixbuf_get_width(source);
    source_height = gdk_pixbuf_get_height(source);

    // check if we need to scale
    if (source_width <= dest_width && source_height <= dest_height)
        return GDK_PIXBUF(g_object_ref(G_OBJECT(source)));

    // check if aspect ratio should be preserved
    if (preserve_ratio)
    {
        // calculate the new dimensions
        wratio =(gdouble) source_width  /(gdouble) dest_width;
        hratio =(gdouble) source_height /(gdouble) dest_height;

        if(hratio > wratio)
            dest_width  = rint(source_width / hratio);
        else
            dest_height = rint(source_height / wratio);
    }

    return gdk_pixbuf_scale_simple(source, MAX(dest_width, 1), MAX(dest_height, 1), GDK_INTERP_BILINEAR);
}

/**
 * egdk_pixbuf_spotlight:
 * @source : the source #GdkPixbuf.
 *
 * Creates a lightened version of @source, suitable for
 * prelit state display of icons.
 *
 * The caller is responsible to free the returned
 * pixbuf using #g_object_unref().
 *
 * Returns: the lightened version of @source.
 *
 * Since: 0.3.1.3
 **/
GdkPixbuf* pixbuf_spotlight(const GdkPixbuf *source)
{
    GdkPixbuf *dst;
    gboolean   has_alpha;
    gint       dst_row_stride;
    gint       src_row_stride;
    gint       width;
    gint       height;
    gint       i;

    // determine source parameters
    width = gdk_pixbuf_get_width(source);
    height = gdk_pixbuf_get_height(source);
    has_alpha = gdk_pixbuf_get_has_alpha(source);

    // allocate the destination pixbuf
    dst = gdk_pixbuf_new(gdk_pixbuf_get_colorspace(source), has_alpha, gdk_pixbuf_get_bits_per_sample(source), width, height);

    // determine src/dst row strides
    dst_row_stride = gdk_pixbuf_get_rowstride(dst);
    src_row_stride = gdk_pixbuf_get_rowstride(source);

#if defined(__GNUC__) && defined(__NO_MMX__)
    // check if there's a good reason to use MMX
    if (has_alpha && dst_row_stride == width * 4 && src_row_stride == width * 4 && (width * height) % 2 == 0)
    {
        __m64 *pixdst =(__m64 *) gdk_pixbuf_get_pixels(dst);
        __m64 *pixsrc =(__m64 *) gdk_pixbuf_get_pixels(source);
        __m64  alpha_mask = _mm_set_pi8(0xff, 0, 0, 0, 0xff, 0, 0, 0);
        __m64  twentyfour = _mm_set_pi8(0, 24, 24, 24, 0, 24, 24, 24);
        __m64  zero = _mm_setzero_si64();

        for(i =(width * height) >> 1; i > 0; --i)
        {
            // read the source pixel
            __m64 src = *pixsrc;

            // remember the two alpha values
            __m64 alpha = _mm_and_si64(alpha_mask, src);

            // extract the hi pixel
            __m64 hi = _mm_unpackhi_pi8(src, zero);

            // extract the lo pixel
            __m64 lo = _mm_unpacklo_pi8(src, zero);

            // add(x >> 3) to x
            hi = _mm_adds_pu16(hi, _mm_srli_pi16(hi, 3));
            lo = _mm_adds_pu16(lo, _mm_srli_pi16(lo, 3));

            // prefetch next value
            __builtin_prefetch(++pixsrc, 0, 1);

            // combine the two pixels again
            src = _mm_packs_pu16(lo, hi);

            // add 24(with saturation)
            src = _mm_adds_pu8(src, twentyfour);

            // drop the alpha channel from the temp color
            src = _mm_andnot_si64(alpha_mask, src);

            // write back the calculated color
            *pixdst = _mm_or_si64(alpha, src);

            // advance the dest pointer
            ++pixdst;
        }

        _mm_empty();
    }
    else
#endif
    {
        guchar *dst_pixels = gdk_pixbuf_get_pixels(dst);
        guchar *src_pixels = gdk_pixbuf_get_pixels(source);
        guchar *pixdst;
        guchar *pixsrc;
        gint    j;

        for(i = height; --i >= 0; )
        {
            pixdst = dst_pixels + i * dst_row_stride;
            pixsrc = src_pixels + i * src_row_stride;

            for(j = width; j > 0; --j)
            {
                *pixdst++ = _lighten_channel(*pixsrc++);
                *pixdst++ = _lighten_channel(*pixsrc++);
                *pixdst++ = _lighten_channel(*pixsrc++);

                if (has_alpha)
                    *pixdst++ = *pixsrc++;
            }
        }
    }

    return dst;
}

static inline guchar _lighten_channel(guchar cur_value)
{
    gint new_value = cur_value;

    new_value += 24 +(new_value >> 3);
    if (new_value > 255)
        new_value = 255;

    return(guchar) new_value;
}


