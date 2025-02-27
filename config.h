#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "libmacros.h"
#include "debug.h"
#include <libxfce4util/libxfce4util.h>

#if defined(GETTEXT_PACKAGE)
#include <glib/gi18n-lib.h>
#else
#include <glib/gi18n.h>
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


