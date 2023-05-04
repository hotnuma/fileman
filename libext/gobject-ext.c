/*-
 * Copyright (c) 2004-2006 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <exo.h>

#include <gobject-ext.h>

/**
 * thunar_g_strescape
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
gchar* e_strescape(const gchar *source)
{
    gchar*       g_escaped;
    gchar*       result;
    unsigned int j = 0;
    unsigned int new_size = 0;

    /* First apply the default escaping .. will escape everything, expect SPACE */
    g_escaped = g_strescape(source, NULL);

    /* calc required new size */
    for (unsigned int i = 0; i < strlen(g_escaped); i++)
    {
        if (g_escaped[i] == ' ')
            new_size++;
        new_size++;
    }

    /* strlen() does not include the \0 character, add an extra slot for it */
    new_size++;
    result = malloc(new_size * sizeof (gchar));

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


