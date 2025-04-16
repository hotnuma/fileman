#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "libmacros.h"
#include "debug.h"

#include <glib.h>

#define ngetstr(Str1, Str2, num) ((num) == 1 ? (Str1) : (Str2))

#undef _
#define _(String) (String)

#ifndef I_
#define I_(String) (g_intern_static_string ((String)))
#endif

#ifndef N_
#define N_(String) (String)
#endif

/* We don't need to implement all the G_OBJECT_WARN_INVALID_PROPERTY_ID()
 * macros for regular builds, as all properties are only accessible from
 * within Thunar and they will be tested during debug builds. Besides that
 * GObject already performs the required checks prior to calling get_property()
 * and set_property().
 */
#ifndef G_ENABLE_DEBUG
#undef G_OBJECT_WARN_INVALID_PROPERTY_ID
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec) \
    G_STMT_START{ (void)0; }G_STMT_END
#endif

#define APP_NAME "fileman"
#define PACKAGE_NAME APP_NAME
#define BINDIR "/usr/bin"

#define E_PARAM_READABLE  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_WRITABLE  (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)

#endif // __CONFIG_H__


