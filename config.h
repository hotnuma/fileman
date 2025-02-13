#ifndef CONFIG_H
#define CONFIG_H

#include <glib-object.h>
#include <libmacros.h>
#include <stdbool.h>
#include <debug.h>

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
#define G_OBJECT_WARN_INVALID_PROPERTY_ID(object, property_id, pspec) G_STMT_START{ (void)0; }G_STMT_END
#endif

#define APP_NAME "fileman"
#define APP_DISPLAY_NAME "Fileman"
#define PACKAGE_NAME APP_NAME
#define PACKAGE_STRING APP_NAME

#define BINDIR "/usr/bin"
#define PATH_BSHELL "/bin/sh"

#include <libxfce4util/libxfce4util.h>
// Deprecated: xfce 4.18
//#define I_(string) (g_intern_static_string((string)))

#define E_PARAM_READABLE  (G_PARAM_READABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_WRITABLE  (G_PARAM_WRITABLE | G_PARAM_STATIC_STRINGS)
#define E_PARAM_READWRITE (G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS)

#define HAVE_STRPTIME

#define HAVE_LINUX 1
#define HAVE_CTYPE_H 1
#define HAVE_ERRNO_H 1
#define HAVE_FCNTL_H 1
#define HAVE_GRP_H 1
#define HAVE_LIMITS_H 1
#define HAVE_LOCALE_H 1
#define HAVE_MEMORY_H 1
#define HAVE_PATHS_H 1
#define HAVE_PWD_H 1
#define HAVE_SCHED_H 1
#define HAVE_SIGNAL_H 1
#define HAVE_STDARG_H 1
#define HAVE_STDLIB_H 1
#define HAVE_STRING_H 1
#define HAVE_SYS_MMAN_H 1
#define HAVE_SYS_PARAM_H 1
#define HAVE_SYS_STAT_H 1 1
#define HAVE_SYS_TIME_H 1
#define HAVE_SYS_TYPES_H 1
#define HAVE_SYS_UIO_H 1
#define HAVE_SYS_WAIT_H 1
#define HAVE_TIME_H 1
#define HAVE_GIO_UNIX 1
#define HAVE_LIBNOTIFY 1
#define HAVE_UNISTD_H 1
#define HAVE_LOCALECONV
#define HAVE_MKDTEMP
#define HAVE_PREAD
#define HAVE_PWRITE
#define HAVE_SCHED_YIELD
#define HAVE_STRCOLL
#define HAVE_SYMLINK
//#define HAVE_STRLCPY
//#define HAVE_SETGROUPENT
//#define HAVE_SETPASSENT

#endif // CONFIG_H


