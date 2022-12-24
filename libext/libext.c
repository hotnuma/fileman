#include "libext.h"

#include <math.h>

// noop -----------------------------------------------------------------------

void e_noop()
{
}

gpointer e_noop_null()
{
  return NULL;
}

gboolean e_noop_true()
{
  return true;
}

// string functions -----------------------------------------------------------

gboolean e_str_looks_like_an_uri(const gchar *str)
{
  const gchar *s = str;

  if (G_UNLIKELY (str == NULL))
    return FALSE;

  /* <scheme> starts with an alpha character */
  if (g_ascii_isalpha (*s))
    {
      /* <scheme> continues with (alpha | digit | "+" | "-" | ".")* */
      for (++s; g_ascii_isalnum (*s) || *s == '+' || *s == '-' || *s == '.'; ++s);

      /* <scheme> must be followed by ":" */
      return (*s == ':' && *(s+1) != '\0');
    }

  return FALSE;
}

gchar* e_str_replace (const gchar *str, const gchar *pattern, const gchar *replacement)
{
  const gchar *s, *p;
  GString     *result;

  /* an empty string or pattern is useless, so just
   * return a copy of str */
  if (G_UNLIKELY(!str || !*str || !pattern || !*pattern))
    return g_strdup(str);

  /* allocate the result string */
  result = g_string_sized_new (strlen (str));

  /* process the input string */
  while (*str != '\0')
    {
      if (G_UNLIKELY (*str == *pattern))
        {
          /* compare the pattern to the current string */
          for (p = pattern + 1, s = str + 1; *p == *s; ++s, ++p)
            if (*p == '\0' || *s == '\0')
              break;

          /* check if the pattern fully matched */
          if (G_LIKELY (*p == '\0'))
            {
              if (G_LIKELY (replacement != NULL && *replacement != '\0'))
                g_string_append (result, replacement);
              str = s;
              continue;
            }
        }

      g_string_append_c (result, *str++);
    }

  return g_string_free (result, FALSE);
}

gchar* e_strdup_strftime(const gchar *format, const struct tm *tm)
{
  static const gchar C_STANDARD_STRFTIME_CHARACTERS[] = "aAbBcCdeFgGhHIjklmMnprRsStTuUVwWxXyYzZ";
  static const gchar C_STANDARD_NUMERIC_STRFTIME_CHARACTERS[] = "CdegGHIjklmMsSuUVwWyY";
  static const gchar SUS_EXTENDED_STRFTIME_MODIFIERS[] = "EO";
  const gchar       *remainder;
  const gchar       *percent;
  gboolean           strip_leading_zeros;
  gboolean           turn_leading_zeros_to_spaces;
  GString           *string;
  gsize              string_length;
  gchar              code[4];
  gchar              buffer[512];
  gchar             *piece;
  gchar             *result;
  gchar             *converted;
  gchar              modifier = 0;
  gint               i;

  /* Format could be translated, and contain UTF-8 chars,
   * so convert to locale encoding which strftime uses.
   */
  converted = g_locale_from_utf8 (format, -1, NULL, NULL, NULL);
  if (G_UNLIKELY (converted == NULL))
    return NULL;

  /* start processing the format */
  string = g_string_new ("");
  remainder = converted;

  /* walk from % character to % character */
  for (;;)
    {
      /* look out for the next % character */
      percent = strchr (remainder, '%');
      if (percent == NULL)
        {
          /* we're done with the format */
          g_string_append (string, remainder);
          break;
        }

      /* append the characters in between */
      g_string_append_len (string, remainder, percent - remainder);

      /* handle the "%" character */
      remainder = percent + 1;
      switch (*remainder)
        {
        case '-':
          strip_leading_zeros = TRUE;
          turn_leading_zeros_to_spaces = FALSE;
          remainder++;
          break;

        case '_':
          strip_leading_zeros = FALSE;
          turn_leading_zeros_to_spaces = TRUE;
          remainder++;
          break;

        case '%':
          g_string_append_c (string, '%');
          remainder++;
          continue;

        case '\0':
          g_warning ("Trailing %% passed to e_strdup_strftime");
          g_string_append_c (string, '%');
          continue;

        default:
          strip_leading_zeros = FALSE;
          turn_leading_zeros_to_spaces = FALSE;
          break;
        }

      if (strchr (SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL)
        {
          modifier = *remainder++;
          if (*remainder == 0)
            {
              g_warning ("Unfinished %%%c modifier passed to e_strdup_strftime", modifier);
              break;
            }
        }

      if (strchr (C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL)
        g_warning ("e_strdup_strftime does not support non-standard escape code %%%c", *remainder);

      /* convert code to strftime format. We have a fixed
       * limit here that each code can expand to a maximum
       * of 512 bytes, which is probably OK. There's no
       * limit on the total size of the result string.
       */
      i = 0;
      code[i++] = '%';

#ifdef HAVE_STRFTIME_EXTENSION
      if (modifier != 0)
        code[i++] = modifier;
#endif

      code[i++] = *remainder;
      code[i++] = '\0';
      string_length = strftime (buffer, sizeof (buffer), code, tm);
      if (string_length == 0)
        {
          /* we could put a warning here, but there's no
           * way to tell a successful conversion to
           * empty string from a failure
           */
          buffer[0] = '\0';
        }

      /* strip leading zeros if requested */
      piece = buffer;
      if (strip_leading_zeros || turn_leading_zeros_to_spaces)
        {
          if (strchr (C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL)
            g_warning ("e_strdup_strftime does not support modifier for non-numeric escape code %%%c%c", remainder[-1], *remainder);

          if (*piece == '0')
            {
              while (*++piece == '\0');
              if (!g_ascii_isdigit (*piece))
                --piece;
            }

          if (turn_leading_zeros_to_spaces)
            {
              memset (buffer, ' ', piece - buffer);
              piece = buffer;
            }
        }

      /* advance */
      ++remainder;

      /* add this piece */
      g_string_append (string, piece);
    }

  /* Convert the string back into UTF-8 */
  result = g_locale_to_utf8 (string->str, -1, NULL, NULL, NULL);

  /* cleanup */
  g_string_free (string, TRUE);
  g_free (converted);

  return result;
}

// GdkPixbuf ------------------------------------------------------------------

//static inline void
//draw_frame_row (const GdkPixbuf *frame_image,
//                gint             target_width,
//                gint             source_width,
//                gint             source_v_position,
//                gint             dest_v_position,
//                GdkPixbuf       *result_pixbuf,
//                gint             left_offset,
//                gint             height)
//{
//  gint remaining_width;
//  gint slab_width;
//  gint h_offset;

//  for (h_offset = 0, remaining_width = target_width; remaining_width > 0; h_offset += slab_width, remaining_width -= slab_width)
//    {
//      slab_width = (remaining_width > source_width) ? source_width : remaining_width;
//      gdk_pixbuf_copy_area (frame_image, left_offset, source_v_position, slab_width, height, result_pixbuf, left_offset + h_offset, dest_v_position);
//    }
//}

//static inline void
//draw_frame_column (const GdkPixbuf *frame_image,
//                   gint             target_height,
//                   gint             source_height,
//                   gint             source_h_position,
//                   gint             dest_h_position,
//                   GdkPixbuf       *result_pixbuf,
//                   gint             top_offset,
//                   gint             width)
//{
//  gint remaining_height;
//  gint slab_height;
//  gint v_offset;

//  for (v_offset = 0, remaining_height = target_height; remaining_height > 0; v_offset += slab_height, remaining_height -= slab_height)
//    {
//      slab_height = (remaining_height > source_height) ? source_height : remaining_height;
//      gdk_pixbuf_copy_area (frame_image, source_h_position, top_offset, width, slab_height, result_pixbuf, dest_h_position, top_offset + v_offset);
//    }
//}

//GdkPixbuf* egdk_pixbuf_frame(const GdkPixbuf *source,
//                                const GdkPixbuf *frame,
//                                gint            left_offset,
//                                gint            top_offset,
//                                gint            right_offset,
//                                gint            bottom_offset)
//{
//  GdkPixbuf *dst;
//  gint       dst_width;
//  gint       dst_height;
//  gint       frame_width;
//  gint       frame_height;
//  gint       src_width;
//  gint       src_height;

//  g_return_val_if_fail (GDK_IS_PIXBUF (frame), NULL);
//  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);

//  src_width = gdk_pixbuf_get_width (source);
//  src_height = gdk_pixbuf_get_height (source);

//  frame_width = gdk_pixbuf_get_width (frame);
//  frame_height = gdk_pixbuf_get_height (frame);

//  dst_width = src_width + left_offset + right_offset;
//  dst_height = src_height + top_offset + bottom_offset;

//  /* allocate the resulting pixbuf */
//  dst = gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, dst_width, dst_height);

//  /* fill the destination if the source has an alpha channel */
//  if (G_UNLIKELY (gdk_pixbuf_get_has_alpha (source)))
//    gdk_pixbuf_fill (dst, 0xffffffff);

//  /* draw the left top cornder and top row */
//  gdk_pixbuf_copy_area (frame, 0, 0, left_offset, top_offset, dst, 0, 0);
//  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, 0, 0, dst, left_offset, top_offset);

//  /* draw the right top corner and left column */
//  gdk_pixbuf_copy_area (frame, frame_width - right_offset, 0, right_offset, top_offset, dst, dst_width - right_offset, 0);
//  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, 0, 0, dst, top_offset, left_offset);

//  /* draw the bottom right corner and bottom row */
//  gdk_pixbuf_copy_area (frame, frame_width - right_offset, frame_height - bottom_offset, right_offset,
//                        bottom_offset, dst, dst_width - right_offset, dst_height - bottom_offset);
//  draw_frame_row (frame, src_width, frame_width - left_offset - right_offset, frame_height - bottom_offset,
//                  dst_height - bottom_offset, dst, left_offset, bottom_offset);

//  /* draw the bottom left corner and the right column */
//  gdk_pixbuf_copy_area (frame, 0, frame_height - bottom_offset, left_offset, bottom_offset, dst, 0, dst_height - bottom_offset);
//  draw_frame_column (frame, src_height, frame_height - top_offset - bottom_offset, frame_width - right_offset,
//                     dst_width - right_offset, dst, top_offset, right_offset);

//  /* copy the source pixbuf into the framed area */
//  gdk_pixbuf_copy_area (source, 0, 0, src_width, src_height, dst, left_offset, top_offset);

//  return dst;
//}

//GdkPixbuf* egdk_pixbuf_scale_down(GdkPixbuf *source,
//                                  gboolean   preserve_aspect_ratio,
//                                  gint       dest_width,
//                                  gint       dest_height)
//{
//  gdouble wratio;
//  gdouble hratio;
//  gint    source_width;
//  gint    source_height;

//  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
//  g_return_val_if_fail (dest_width > 0, NULL);
//  g_return_val_if_fail (dest_height > 0, NULL);

//  source_width = gdk_pixbuf_get_width (source);
//  source_height = gdk_pixbuf_get_height (source);

//  /* check if we need to scale */
//  if (G_UNLIKELY (source_width <= dest_width && source_height <= dest_height))
//    return GDK_PIXBUF (g_object_ref (G_OBJECT (source)));

//  /* check if aspect ratio should be preserved */
//  if (G_LIKELY (preserve_aspect_ratio))
//    {
//      /* calculate the new dimensions */
//      wratio = (gdouble) source_width  / (gdouble) dest_width;
//      hratio = (gdouble) source_height / (gdouble) dest_height;

//      if (hratio > wratio)
//        dest_width  = rint (source_width / hratio);
//      else
//        dest_height = rint (source_height / wratio);
//    }

//  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);
//}

//GdkPixbuf* egdk_pixbuf_scale_ratio(GdkPixbuf *source, gint dest_size)
//{
//  gdouble wratio;
//  gdouble hratio;
//  gint    source_width;
//  gint    source_height;
//  gint    dest_width;
//  gint    dest_height;

//  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
//  g_return_val_if_fail (dest_size > 0, NULL);

//  source_width  = gdk_pixbuf_get_width  (source);
//  source_height = gdk_pixbuf_get_height (source);

//  wratio = (gdouble) source_width  / (gdouble) dest_size;
//  hratio = (gdouble) source_height / (gdouble) dest_size;

//  if (hratio > wratio)
//    {
//      dest_width  = rint (source_width / hratio);
//      dest_height = dest_size;
//    }
//  else
//    {
//      dest_width  = dest_size;
//      dest_height = rint (source_height / wratio);
//    }

//  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);
//}


