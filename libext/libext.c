#include <config.h>
#include <libext.h>

#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <unistd.h>

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

// XFCE Utils -----------------------------------------------------------------

void eg_string_append_quoted(GString *string, const gchar *unquoted)
{
  gchar *quoted;

  quoted = g_shell_quote (unquoted);
  g_string_append (string, quoted);
  g_free (quoted);
}

gchar* e_expand_desktop_entry_field_codes(const gchar *command,
                                          GSList      *uri_list,
                                          const gchar *icon,
                                          const gchar *name,
                                          const gchar *uri,
                                          gboolean     requires_terminal)
{
  const gchar *p;
  gchar       *filename;
  GString     *string;
  GSList      *li;
  GFile       *file;

  if (G_UNLIKELY (command == NULL))
    return NULL;

  string = g_string_sized_new (strlen (command));

  if (requires_terminal)
    g_string_append (string, "exo-open --launch TerminalEmulator ");

  for (p = command; *p != '\0'; ++p)
    {
      if (G_UNLIKELY (p[0] == '%' && p[1] != '\0'))
        {
          switch (*++p)
            {
            case 'f':
            case 'F':
              for (li = uri_list; li != NULL; li = li->next)
                {
                  /* passing through a GFile seems necessary to properly handle
                   * all URI schemes, in particular g_filename_from_uri() is not
                   * able to do so */
                  file = g_file_new_for_uri (li->data);
                  filename = g_file_get_path (file);
                  if (G_LIKELY (filename != NULL))
                    eg_string_append_quoted (string, filename);

                  g_object_unref (file);
                  g_free (filename);

                  if (*p == 'f')
                    break;
                  if (li->next != NULL)
                    g_string_append_c (string, ' ');
                }
              break;

            case 'u':
            case 'U':
              for (li = uri_list; li != NULL; li = li->next)
                {
                  eg_string_append_quoted (string, li->data);

                  if (*p == 'u')
                    break;
                  if (li->next != NULL)
                    g_string_append_c (string, ' ');
                }
              break;

            case 'i':
              if (icon != NULL && *icon != '\0')
                {
                  g_string_append (string, "--icon ");
                  eg_string_append_quoted (string, icon);
                }
              break;

            case 'c':
              if (name != NULL && *name != '\0')
                eg_string_append_quoted (string, name);
              break;

            case 'k':
              if (uri != NULL && *uri != '\0')
                eg_string_append_quoted (string, uri);
              break;

            case '%':
              g_string_append_c (string, '%');
              break;
            }
        }
      else
        {
          g_string_append_c (string, *p);
        }
    }

  return g_string_free (string, FALSE);
}

// ---------------------------------


