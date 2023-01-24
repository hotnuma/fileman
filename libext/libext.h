#ifndef LIBEXT_H
#define LIBEXT_H

#include <glib.h>

void        e_noop();
gpointer    e_noop_null() G_GNUC_PURE;
gboolean    e_noop_true() G_GNUC_PURE;

// deprecated replaced with g_uri_is_valid
gboolean    e_str_looks_like_an_uri(const gchar *str);

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

#endif // LIBEXT_H


