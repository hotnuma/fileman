#ifndef EXO_H
#define EXO_H

#include <glib.h>

gchar*      e_str_replace(const gchar *str, const gchar *pattern,
                          const gchar *replacement);
gchar*      e_strdup_strftime(const gchar *format, const struct tm *tm);

gchar*      e_expand_desktop_entry_field_codes(const gchar *command,
                                               GSList      *uri_list,
                                               const gchar *icon,
                                               const gchar *name,
                                               const gchar *uri,
                                               gboolean    requires_terminal);

void        e_string_append_quoted(GString *string, const gchar *unquoted);

#endif // EXO_H


