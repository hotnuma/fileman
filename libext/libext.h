#ifndef LIBEXT_H
#define LIBEXT_H

#include <stdbool.h>
#include <gtk/gtk.h>
#include <exo/exo.h>

#define E_PARAM_READABLE  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_WRITABLE  (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)

void e_noop();
gpointer e_noop_null() G_GNUC_PURE;
gboolean e_noop_true() G_GNUC_PURE;

gboolean e_str_looks_like_an_uri(const gchar *str);
gchar* e_str_replace(const gchar *str, const gchar *pattern, const gchar *replacement);
gchar* e_strdup_strftime(const gchar *format, const struct tm *tm);

#endif // LIBEXT_H


