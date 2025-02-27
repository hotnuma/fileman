/*-
 * Copyright(c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
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

#include "config.h"
#include "utils.h"

#include <glib/gstdio.h>
#include <pwd.h>

static inline gchar *_util_strrchr_offset(const gchar *str,
                                          const gchar *offset, gchar c);

// string functions  ----------------------------------------------------------

/**
 * util_str_get_extension
 * @filename : an UTF-8 filename
 *
 * Returns a pointer to the extension in @filename.
 *
 * This is an improved version of g_utf8_strrchr with
 * improvements to recognize compound extensions like
 * ".tar.gz" and ".desktop.in.in".
 *
 * Return value: pointer to the extension in @filename
 *               or NULL.
**/
gchar* util_str_get_extension(const gchar *filename)
{
    static const gchar *compressed[] =
                        {"gz", "bz2", "lzma", "lrz", "rpm", "lzo", "xz", "z"};
    gchar *dot;
    gchar *ext;
    guint i;
    gchar *dot2;
    gsize len;
    gboolean is_in;

    // check if there is an possible extension part in the name
    dot = strrchr(filename, '.');
    if (dot == NULL || dot == filename || dot[1] == '\0')
        return NULL;

    // skip the .
    ext = dot + 1;

    // check if this looks like a compression mime-type
    for (i = 0; i < G_N_ELEMENTS(compressed); i++)
    {
        if (strcasecmp(ext, compressed[i]) == 0)
        {
            // look for a possible container part(tar, psd, epsf)
            dot2 = _util_strrchr_offset(filename, dot - 1, '.');
            if (dot2 != NULL && dot2 != filename)
            {
                // check the 2nd part range, keep it between 2 and 5 chars
                len = dot - dot2 - 1;
                if (len >= 2 && len <= 5)
                    dot = dot2;
            }

            // that's it for compression types
            return dot;
        }
    }

    /* for coders, .in are quite common, so check for those too
     * with a max of 3 rounds(2x .in and the possibly final extension) */
    if (strcasecmp(ext, "in") == 0)
    {
        for (i = 0, is_in = TRUE; is_in && i < 3; i++)
        {
            dot2 = _util_strrchr_offset(filename, dot - 1, '.');
            // the extension before .in could be long. check that it's at least 2 chars
            len = dot - dot2 - 1;
            if (dot2 == NULL || dot2 == filename || len < 2)
                break;

            // continue if another .in was found
            is_in = dot - dot2 == 3 && strncasecmp(dot2, ".in", 3) == 0;

            dot = dot2;
        }
    }

    return dot;
}

/**
 * _util_strrchr_offset:
 * @str:    haystack
 * @offset: pointer offset in @str
 * @c:      search needle
 *
 * Return the last occurrence of the character @c in
 * the string @str starting at @offset.
 *
 * There are also Glib functions for this like g_strrstr_len
 * and g_utf8_strrchr, but these work internally the same
 * as this function(tho, less efficient).
 *
 * Return value: pointer in @str or NULL.
 **/
static inline gchar* _util_strrchr_offset(const gchar *str, const gchar *offset,
                                          gchar c)
{
    const gchar *p;

    for (p = offset; p > str; --p)
    {
        if (*p == c)
            return (gchar *)p;
    }

    return NULL;
}

/**
 * util_str_escape
 * @source  : The string to escape
 *
 * Similar to g_strescape, but as well escapes SPACE
 *
 * Escapes the special characters '\b', '\f', '\n', '\r', '\t', '\v', '\' ' ' and '"' in the string source by inserting a '\' before them.
 * Additionally all characters in the range 0x01-0x1F (SPACE and everything below)
 * and in the range 0x7F-0xFF (all non-ASCII chars) are replaced with a '\' followed by their octal representation.
 *
 * Return value: (transfer full): The new string. Has to be freed with g_free after usage.
 **/
gchar* util_str_escape(const gchar *source)
{
    gchar *g_escaped;
    gchar *result;
    unsigned int j = 0;
    unsigned int new_size = 0;

    // First apply the default escaping .. will escape everything, expect SPACE
    g_escaped = g_strescape(source, NULL);

    // calc required new size
    for (unsigned int i = 0; i < strlen(g_escaped); i++)
    {
        if (g_escaped[i] == ' ')
            new_size++;
        new_size++;
    }

    // strlen() does not include the \0 character, add an extra slot for it
    new_size++;
    result = malloc(new_size * sizeof(gchar));

    for (unsigned int i = 0; i < strlen(g_escaped); i++)
    {
        if (g_escaped[i] == ' ')
        {
            result[j] = '\\';
            j++;
        }
        result[j] = g_escaped[i];
        j++;
    }
    result[j] = '\0';
    g_free(g_escaped);
    return result;
}

gchar* util_str_replace(const gchar *str, const gchar *pattern,
                        const gchar *replace)
{
    const gchar *s, *p;
    GString *result;

    /* an empty string or pattern is useless, so just
     * return a copy of str */
    if (!str || !*str || !pattern || !*pattern)
        return g_strdup(str);

    // allocate the result string
    result = g_string_sized_new(strlen(str));

    // process the input string
    while (*str != '\0')
    {
        if (*str == *pattern)
        {
            // compare the pattern to the current string
            for (p = pattern + 1, s = str + 1; *p == *s; ++s, ++p)
                if (*p == '\0' || *s == '\0')
                    break;

            // check if the pattern fully matched
            if (*p == '\0')
            {
                if (replace != NULL && *replace != '\0')
                    g_string_append(result, replace);
                str = s;
                continue;
            }
        }

        g_string_append_c(result, *str++);
    }

    return g_string_free(result, FALSE);
}

/**
 * util_expand_filename:
 * @filename          : a local filename.
 * @working_directory : #GFile of the current working directory.
 * @error             : return location for errors or %NULL.
 *
 * Takes a user-typed @filename and expands a tilde at the
 * beginning of the @filename. It also resolves paths prefixed with
 * '.' using the current working directory.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the expanded @filename or %NULL on error.
 **/
gchar* util_expand_filename(const gchar *filename, GFile *working_directory,
                            GError **error)
{
    struct passwd *passwd;
    const gchar *replacement;
    const gchar *remainder;
    const gchar *slash;
    gchar *username;
    gchar *pwd;
    gchar *variable;
    gchar *result = NULL;

    g_return_val_if_fail(filename != NULL, NULL);

    // check if we have a valid(non-empty!) filename
    if (*filename == '\0')
    {
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid path"));
        return NULL;
    }

    // check if we start with a '~'
    if (*filename == '~')
    {
        // examine the remainder of the filename
        remainder = filename + 1;

        // if we have only the slash, then we want the home dir
        if (*remainder == '\0')
            return g_strdup(g_get_home_dir());

        // lookup the slash
        for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash)
            ;

        // check if a username was given after the '~'
        if (slash == remainder)
        {
            // replace the tilde with the home dir
            replacement = g_get_home_dir();
        }
        else
        {
            // lookup the pwd entry for the username
            username = g_strndup(remainder, slash - remainder);
            passwd = getpwnam(username);
            g_free(username);

            // check if we have a valid entry
            if (passwd == NULL)
            {
                username = g_strndup(remainder, slash - remainder);
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Unknown user \"%s\""), username);
                g_free(username);
                return NULL;
            }

            // use the homedir of the specified user
            replacement = passwd->pw_dir;
        }

        // generate the filename
        return g_build_filename(replacement, slash, NULL);
    }
    else if (*filename == '$')
    {
        // examine the remainder of the variable and filename
        remainder = filename + 1;

        // lookup the slash at the end of the variable
        for (slash = remainder; *slash != '\0' && *slash != G_DIR_SEPARATOR; ++slash)
            ;

        // get the variable for replacement
        variable = g_strndup(remainder, slash - remainder);
        replacement = g_getenv(variable);
        g_free(variable);

        if (replacement == NULL)
            return NULL;

        // generate the filename
        return g_build_filename(replacement, slash, NULL);
    }
    else if (*filename == '.')
    {
        // examine the remainder of the filename
        remainder = filename + 1;

        // transform working directory into a filename string
        if (working_directory != NULL)
        {
            pwd = g_file_get_path(working_directory);

            // if we only have the slash then we want the working directory only
            if (*remainder == '\0')
                return pwd;

            // concatenate working directory and remainder
            result = g_build_filename(pwd, remainder, G_DIR_SEPARATOR_S, NULL);

            // free the working directory string
            g_free(pwd);
        }
        else
            result = g_strdup(filename);

        // return the resulting path string
        return result;
    }

    return g_strdup(filename);
}

// Desktop Entry --------------------------------------------------------------

gchar* util_expand_field_codes(const gchar *command, GSList *uri_list,
                               const gchar *icon, const gchar *name,
                               const gchar *uri, gboolean requires_terminal)
{
    if (command == NULL)
        return NULL;

    GString *string;
    string = g_string_sized_new(strlen(command));

    if (requires_terminal)
        g_string_append(string, "exo-open --launch TerminalEmulator ");

    const gchar *p;
    GSList *li;
    GFile *file;
    gchar *filename;

    for (p = command; *p != '\0'; ++p)
    {
        if (p[0] == '%' && p[1] != '\0')
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

                    file = g_file_new_for_uri(li->data);
                    filename = g_file_get_path(file);
                    if (filename != NULL)
                        util_append_quoted(string, filename);

                    g_object_unref(file);
                    g_free(filename);

                    if (*p == 'f')
                        break;
                    if (li->next != NULL)
                        g_string_append_c(string, ' ');
                }
                break;

            case 'u':
            case 'U':
                for (li = uri_list; li != NULL; li = li->next)
                {
                    util_append_quoted(string, li->data);

                    if (*p == 'u')
                        break;
                    if (li->next != NULL)
                        g_string_append_c(string, ' ');
                }
                break;

            case 'i':
                if (icon != NULL && *icon != '\0')
                {
                    g_string_append(string, "--icon ");
                    util_append_quoted(string, icon);
                }
                break;

            case 'c':
                if (name != NULL && *name != '\0')
                    util_append_quoted(string, name);
                break;

            case 'k':
                if (uri != NULL && *uri != '\0')
                    util_append_quoted(string, uri);
                break;

            case '%':
                g_string_append_c(string, '%');
                break;
            }
        }
        else
        {
            g_string_append_c(string, *p);
        }
    }

    return g_string_free(string, FALSE);
}

// chdir ----------------------------------------------------------------------

gchar* util_change_working_directory(const gchar *new_directory)
{
    gchar *old_directory;

    e_return_val_if_fail(new_directory != NULL && *new_directory != '\0', NULL);

    // try to determine the current working directory
    old_directory = g_get_current_dir();

    // try switching to the new working directory
    if (g_chdir(new_directory) != 0)
    {
        // switching failed, we don't need to return the old directory
        g_free(old_directory);
        old_directory = NULL;
    }

    return old_directory;
}

// GString --------------------------------------------------------------------

void util_append_quoted(GString *string, const gchar *unquoted)
{
    gchar *quoted = g_shell_quote(unquoted);
    g_string_append(string, quoted);
    g_free(quoted);
}

// set display env ------------------------------------------------------------

void util_set_display_env(gpointer data)
{
    g_setenv("DISPLAY", (char*) data, TRUE);
}

// parent screen --------------------------------------------------------------

/* Determines the screen for the parent and returns that GdkScreen.
 * If window_return is not NULL, the pointer to the GtkWindow is
 * placed into it, or NULL if the window could not be determined. */

GdkScreen* util_parse_parent(gpointer parent, GtkWindow **window_return)
{
    e_return_val_if_fail(parent == NULL
                         || GDK_IS_SCREEN(parent)
                         || GTK_IS_WIDGET(parent), NULL);

    GdkScreen *screen;
    GtkWidget *window = NULL;

    // determine the proper parent
    if (parent == NULL)
    {
        // just use the default screen then
        screen = gdk_screen_get_default();
    }
    else if (GDK_IS_SCREEN(parent))
    {
        // yep, that's a screen
        screen = GDK_SCREEN(parent);
    }
    else
    {
        // parent is a widget, so let's determine the toplevel window
        window = gtk_widget_get_toplevel(GTK_WIDGET(parent));
        if (window != NULL && gtk_widget_is_toplevel(window))
        {
            // make sure the toplevel window is shown
            gtk_widget_show_now(window);
        }
        else
        {
            // no toplevel, not usable then
            window = NULL;
        }

        // determine the screen for the widget
        screen = gtk_widget_get_screen(GTK_WIDGET(parent));
    }

    // check if we should return the window
    if (window_return != NULL)
        *window_return = (GtkWindow *)window;

    return screen;
}

// time -----------------------------------------------------------------------

/**
 * util_time_from_rfc3339:
 * @date_string : an RFC 3339 encoded date string.
 *
 * Decodes the @date_string, which must be in the special RFC 3339
 * format <literal>YYYY-MM-DDThh:mm:ss</literal>. This method is
 * used to decode deletion dates of files in the trash. See the
 * Trash Specification for details.
 *
 * Return value: the time value matching the @date_string or
 *               %0 if the @date_string could not be parsed.
 **/
time_t util_time_from_rfc3339(const gchar *date_string)
{
    struct tm tm;

#ifdef HAVE_STRPTIME
    // using strptime() its easy to parse the date string
    if (strptime(date_string, "%FT%T", &tm) == NULL)
        return 0;
#else
    gulong val;

    // be sure to start with a clean tm
    memset(&tm, 0, sizeof(tm));

    // parsing by hand is also doable for RFC 3339 dates
    val = strtoul(date_string, (gchar **)&date_string, 10);
    if (*date_string != '-')
        return 0;

    // YYYY-MM-DD
    tm.tm_year = val - 1900;
    date_string++;
    tm.tm_mon = strtoul(date_string, (gchar **)&date_string, 10) - 1;

    if (*date_string++ != '-')
        return 0;

    tm.tm_mday = strtoul(date_string, (gchar **)&date_string, 10);

    if (*date_string++ != 'T')
        return 0;

    val = strtoul(date_string, (gchar **)&date_string, 10);
    if (*date_string != ':')
        return 0;

    // hh:mm:ss
    tm.tm_hour = val;
    date_string++;
    tm.tm_min = strtoul(date_string, (gchar **)&date_string, 10);

    if (*date_string++ != ':')
        return 0;

    tm.tm_sec = strtoul(date_string, (gchar **)&date_string, 10);
#endif // !HAVE_STRPTIME

    // translate tm to time_t
    return mktime(&tm);
}

/**
 * util_humanize_file_time:
 * @file_time         : a #guint64 timestamp.
 * @date_style        : the #ThunarDateFormat used to humanize the @file_time.
 * @date_custom_style : custom style to apply, if @date_style is set to custom
 *
 * Returns a human readable date representation of the specified
 * @file_time. The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * Return value: a human readable date representation of @file_time
 *               according to the @date_format.
 **/
gchar* util_humanize_file_time(guint64 file_time, ThunarDateStyle date_style,
                               const gchar *date_custom_style)
{
    const gchar *date_format;
    struct tm tfile;
    time_t ftime;
    GDate dfile;
    GDate dnow;
    gint diff;

    // check if the file_time is valid
    if (file_time != 0)
    {
        ftime = (time_t)file_time;

        // take a copy of the local file time
        tfile = *localtime(&ftime);

        // check which style to use to format the time
        if (date_style == THUNAR_DATE_STYLE_SIMPLE || date_style == THUNAR_DATE_STYLE_SHORT)
        {
            // setup the dates for the time values
            g_date_set_time_t(&dfile, (time_t)ftime);
            g_date_set_time_t(&dnow, time(NULL));

            // determine the difference in days
            diff = g_date_get_julian(&dnow) - g_date_get_julian(&dfile);
            if (diff == 0)
            {
                if (date_style == THUNAR_DATE_STYLE_SIMPLE)
                {
                    // TRANSLATORS: file was modified less than one day ago
                    return g_strdup(_("Today"));
                }
                else // if (date_style == THUNAR_DATE_STYLE_SHORT)
                {
                    // TRANSLATORS: file was modified less than one day ago
                    return util_strdup_strftime(_("Today at %X"), &tfile);
                }
            }
            else if (diff == 1)
            {
                if (date_style == THUNAR_DATE_STYLE_SIMPLE)
                {
                    // TRANSLATORS: file was modified less than two days ago
                    return g_strdup(_("Yesterday"));
                }
                else // if (date_style == THUNAR_DATE_STYLE_SHORT)
                {
                    // TRANSLATORS: file was modified less than two days ago
                    return util_strdup_strftime(_("Yesterday at %X"), &tfile);
                }
            }
            else
            {
                if (diff > 1 && diff < 7)
                {
                    // Days from last week
                    date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%A" : _("%A at %X");
                }
                else
                {
                    // Any other date
                    date_format = (date_style == THUNAR_DATE_STYLE_SIMPLE) ? "%x" : _("%x at %X");
                }

                // format the date string accordingly
                return util_strdup_strftime(date_format, &tfile);
            }
        }
        else if (date_style == THUNAR_DATE_STYLE_LONG)
        {
            // use long, date(1)-like format string
            return util_strdup_strftime("%c", &tfile);
        }
        else if (date_style == THUNAR_DATE_STYLE_YYYYMMDD)
        {
            return util_strdup_strftime("%Y-%m-%d %H:%M:%S", &tfile);
        }
        else if (date_style == THUNAR_DATE_STYLE_MMDDYYYY)
        {
            return util_strdup_strftime("%m-%d-%Y %H:%M:%S", &tfile);
        }
        else if (date_style == THUNAR_DATE_STYLE_DDMMYYYY)
        {
            return util_strdup_strftime("%d-%m-%Y %H:%M:%S", &tfile);
        }
        else // if (date_style == THUNAR_DATE_STYLE_CUSTOM)
        {
            if (date_custom_style == NULL)
                return g_strdup("");

            // use custom date formatting
            return util_strdup_strftime(date_custom_style, &tfile);
        }
    }

    // the file_time is invalid
    return g_strdup(_("Unknown"));
}

gchar* util_strdup_strftime(const gchar *format, const struct tm *tm)
{
    static const gchar C_STANDARD_STRFTIME_CHARACTERS[] = "aAbBcCdeFgGhHIjklmMnprRsStTuUVwWxXyYzZ";
    static const gchar C_STANDARD_NUMERIC_STRFTIME_CHARACTERS[] = "CdegGHIjklmMsSuUVwWyY";
    static const gchar SUS_EXTENDED_STRFTIME_MODIFIERS[] = "EO";
    const gchar *remainder;
    const gchar *percent;
    gboolean strip_leading_zeros;
    gboolean turn_leading_zeros_to_spaces;
    GString *string;
    gsize string_length;
    gchar code[4];
    gchar buffer[512];
    gchar *piece;
    gchar *result;
    gchar *converted;
    gchar modifier = 0;
    gint i;

    /* Format could be translated, and contain UTF-8 chars,
     * so convert to locale encoding which strftime uses.
     */
    converted = g_locale_from_utf8(format, -1, NULL, NULL, NULL);
    if (converted == NULL)
        return NULL;

    // start processing the format
    string = g_string_new("");
    remainder = converted;

    // walk from % character to % character
    for (;;)
    {
        // look out for the next % character
        percent = strchr(remainder, '%');
        if (percent == NULL)
        {
            // we're done with the format
            g_string_append(string, remainder);
            break;
        }

        // append the characters in between
        g_string_append_len(string, remainder, percent - remainder);

        // handle the "%" character
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
            g_string_append_c(string, '%');
            remainder++;
            continue;

        case '\0':
            g_warning("Trailing %% passed to e_strdup_strftime");
            g_string_append_c(string, '%');
            continue;

        default:
            strip_leading_zeros = FALSE;
            turn_leading_zeros_to_spaces = FALSE;
            break;
        }

        if (strchr(SUS_EXTENDED_STRFTIME_MODIFIERS, *remainder) != NULL)
        {
            modifier = *remainder++;
            if (*remainder == 0)
            {
                g_warning("Unfinished %%%c modifier passed to e_strdup_strftime", modifier);
                break;
            }
        }

        if (strchr(C_STANDARD_STRFTIME_CHARACTERS, *remainder) == NULL)
            g_warning("e_strdup_strftime does not support non-standard escape code %%%c", *remainder);

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
        string_length = strftime(buffer, sizeof(buffer), code, tm);
        if (string_length == 0)
        {
            /* we could put a warning here, but there's no
           * way to tell a successful conversion to
           * empty string from a failure
           */
            buffer[0] = '\0';
        }

        // strip leading zeros if requested
        piece = buffer;
        if (strip_leading_zeros || turn_leading_zeros_to_spaces)
        {
            if (strchr(C_STANDARD_NUMERIC_STRFTIME_CHARACTERS, *remainder) == NULL)
                g_warning("e_strdup_strftime does not support modifier for non-numeric escape code %%%c%c", remainder[-1], *remainder);

            if (*piece == '0')
            {
                while (*++piece == '\0')
                    ;
                if (!g_ascii_isdigit(*piece))
                    --piece;
            }

            if (turn_leading_zeros_to_spaces)
            {
                memset(buffer, ' ', piece - buffer);
                piece = buffer;
            }
        }

        // advance
        ++remainder;

        // add this piece
        g_string_append(string, piece);
    }

    // Convert the string back into UTF-8
    result = g_locale_to_utf8(string->str, -1, NULL, NULL, NULL);

    // cleanup
    g_string_free(string, TRUE);
    g_free(converted);

    return result;
}


