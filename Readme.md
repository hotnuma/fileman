
This is an experimental fork of Thunar File Manager from XFCE desktop

<!--

metadata

gboolean directory_specific_settings;
thunar_file_get_metadata_setting()



enter : thunar_window_notebook_insert
enter : thunar_window_notebook_page_added
enter : thunar_window_notebook_show_tabs
enter : thunar_window_notebook_switch_page
enter : thunar_window_notebook_page_removed

xfce_get_homedir

#'-DGDK_DISABLE_DEPRECATED',
#'-DGTK_DISABLE_DEPRECATED',

* New macros
    
    #ifdef ENABLE_GETTEXT
    #ifdef ENABLE_DBUS
    
    

HAVE_LINUX

CTYPE_H
ERRNO_H
FCNTL_H
GRP_H
LIMITS_H
LOCALE_H
MEMORY_H
PATHS_H
PWD_H
SCHED_H
SIGNAL_H
STDARG_H
STDLIB_H
STRING_H
SYS_MMAN_H
SYS_PARAM_H
SYS_STAT_H
SYS_TIME_H
SYS_TYPES_H
SYS_UIO_H
SYS_WAIT_H
TIME_H


AC_FUNC_MMAP()

AC_CHECK_FUNCS()
localeconv
mkdtemp
pread
pwrite
sched_yield
setgroupent
setpassent
strcoll
strlcpy
strptime
symlink
atexit

XDT_I18N([@LINGUAS@])

XDT_CHECK_LIBX11_REQUIRE()

GTK_DOC_CHECK([1.9])

XDT_CHECK_PACKAGE([EXO], [exo-2], [4.15.3])
XDT_CHECK_PACKAGE([GLIB], [glib-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GIO], [gio-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTHREAD], [gthread-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GMODULE], [gmodule-2.0], [2.50.0])
XDT_CHECK_PACKAGE([GTK], [gtk+-3.0], [3.22.0])
XDT_CHECK_PACKAGE([GDK_PIXBUF], [gdk-pixbuf-2.0], [2.14.0])
XDT_CHECK_PACKAGE([LIBXFCE4UTIL], [libxfce4util-1.0], [4.15.2])
XDT_CHECK_PACKAGE([LIBXFCE4UI], [libxfce4ui-2], [4.15.3])
XDT_CHECK_PACKAGE([LIBXFCE4KBD_PRIVATE], [libxfce4kbd-private-3], [4.12.0])
XDT_CHECK_PACKAGE([XFCONF], [libxfconf-0], [4.12.0])
XDT_CHECK_PACKAGE([PANGO], [pango], [1.38.0])

GOBJECT_INTROSPECTION_CHECK([1.30.0])

XDT_CHECK_LIBSM()

XDT_CHECK_OPTIONAL_PACKAGE([GIO_UNIX], [gio-unix-2.0],
                           [2.30.0], [gio-unix], [GIO UNIX features])

XDT_CHECK_OPTIONAL_PACKAGE([GUDEV], [gudev-1.0], [145], [gudev],
                           [GUDev (required for thunar-volman)])

XDT_CHECK_OPTIONAL_PACKAGE([LIBNOTIFY], [libnotify], [0.4.0], [notifications],
                           [Mount notification support], [yes])

thunarx/thunarx-3.pc
thunarx/thunarx-config.h

-->


