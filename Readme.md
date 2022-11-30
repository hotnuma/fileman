
This is an experimental fork of Thunar File Manager from XFCE desktop

TODO : preferences options cleanup

menus:
thunar_tree_view_context_menu
thunar_standard_view_context_menu

#### Widget focus

* search
    
    ```
    $ cgrep gtk_widget_grab_focus
    
    dialog/thunar-chooser-dialog.c:971:     gtk_widget_grab_focus (dialog->tree_view);
    dialog/thunar-properties-dialog.c:972:  gtk_widget_grab_focus (GTK_WIDGET (xfce_filename_input_get_entry (dialog->name_entry)));
    dialog/thunar-dialogs.c:1115:           gtk_widget_grab_focus (entry);
    view/thunar-column-editor.c:283:        gtk_widget_grab_focus (column_editor->tree_view);
    
    widget/thunar-location-entry.c:308:     gtk_widget_grab_focus (location_entry->path_entry);
    
    side/thunar-tree-view.c:790:            gtk_widget_grab_focus (widget);
    side/thunar-tree-view.c:904:            gtk_widget_grab_focus (widget);
    
    view/thunar-standard-view.c:944:        gtk_widget_grab_focus (gtk_bin_get_child (GTK_BIN (widget)));
    view/thunar-standard-view.c:1959:       gtk_widget_grab_focus (GTK_WIDGET (standard_view));
    view/thunar-standard-view.c:2072:       gtk_widget_grab_focus (GTK_WIDGET (standard_view));
    view/thunar-standard-view.c:2159:       gtk_widget_grab_focus (gtk_bin_get_child (GTK_BIN (standard_view)));
    
    view/thunar-details-view.c:684:         gtk_widget_grab_focus (GTK_WIDGET (tree_view));
    
    thunar-window.c:1109:                   gtk_widget_grab_focus (page);
    thunar-window.c:1346:                   gtk_widget_grab_focus (window->view);
    thunar-window.c:1523:                   gtk_widget_grab_focus (new_view);
    thunar-window.c:2052:                   gtk_widget_grab_focus (window->view);
    ```

* Widget focus
    
    gtk_widget_grab_focus
    
    focus-out-event, focus-in-event, gtk_window_get_focus
    
    ```
    thunar/thunar-standard-view.c:394:
    gtkwidget_class->grab_focus = thunar_standard_view_grab_focus;
    ```
    

<!--
metadata

gboolean directory_specific_settings;
thunar_file_get_metadata_setting()

xfce_get_homedir

#'-DGDK_DISABLE_DEPRECATED',
#'-DGTK_DISABLE_DEPRECATED',
#    '-D__USE_XOPEN_EXTENDED',

* New macros
    
#ifdef ENABLE_GETTEXT
#ifdef ENABLE_DBUS
#ifdef ENABLE_LIBSM

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


