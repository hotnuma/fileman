TEMPLATE = app
TARGET = fileman
CONFIG = c11 link_pkgconfig
DEFINES = GTK HAVE_CONFIG_H

PKGCONFIG = \
    gtk+-3.0 \
    tinyc \
    gudev-1.0 \
    libnotify \
    libxfce4ui-2 \
    thunarx-3 \

INCLUDEPATH = core dialog job libext menu misc side view widget

HEADERS = \
    core/th-file.h \
    core/thunar-clipboard-manager.h \
    core/thunar-device.h \
    core/thunar-device-monitor.h \
    core/thunar-dnd.h \
    core/thunar-file-monitor.h \
    core/thunar-folder.h \
    core/thunar-user.h \
    core/thunar-util.h \
    core/thx-file-info.h \
    dialog/thunar-chooser-button.h \
    dialog/thunar-chooser-dialog.h \
    dialog/thunar-chooser-model.h \
    dialog/thunar-dialogs.h \
    dialog/thunar-permissions-chooser.h \
    dialog/thunar-progress-dialog.h \
    dialog/thunar-progress-view.h \
    dialog/thunar-properties-dialog.h \
    job/io-jobs.h \
    job/io-scan-directory.h \
    job/thunar-deep-count-job.h \
    job/thunar-io-jobs-util.h \
    job/thunar-job.h \
    job/thunar-simple-job.h \
    job/thunar-transfer-job.h \
    libext/exo-cell-renderer-icon.h \
    libext/exo-gdk-pixbuf-extensions.h \
    libext/exo-job.h \
    libext/exo-tree-view.h \
    libext/libext.h \
    menu/thunar-launcher.h \
    menu/thunar-menu.h \
    misc/enum-types.h \
    misc/thunar-browser.h \
    misc/thunar-component.h \
    misc/thunar-gdk-extensions.h \
    misc/thunar-gio-extensions.h \
    misc/thunar-gobject-extensions.h \
    misc/thunar-gtk-extensions.h \
    misc/thunar-history.h \
    misc/thunar-navigator.h \
    misc/thunar-notify.h \
    misc/thunar-pango-extensions.h \
    side/thunar-side-pane.h \
    side/thunar-tree-model.h \
    side/thunar-tree-pane.h \
    side/thunar-tree-view.h \
    thunar-debug.h \
    view/list-model.h \
    view/standard-view.h \
    view/thunar-column-model.h \
    view/thunar-details-view.h \
    view/thunar-view.h \
    widget/thunar-icon-factory.h \
    widget/thunar-icon-renderer.h \
    widget/thunar-image.h \
    widget/thunar-location-bar.h \
    widget/thunar-location-entry.h \
    widget/thunar-path-entry.h \
    widget/thunar-shortcuts-icon-renderer.h \
    widget/thunar-size-label.h \
    widget/thunar-statusbar.h \
    config.h \
    preferences.h \
    thunar-application.h \
    thunar-marshal.h \
    thunar-window.h \

SOURCES = \
    0Temp.c \
    core/th-file.c \
    core/thunar-clipboard-manager.c \
    core/thunar-device.c \
    core/thunar-device-monitor.c \
    core/thunar-dnd.c \
    core/thunar-file-monitor.c \
    core/thunar-folder.c \
    core/thunar-user.c \
    core/thunar-util.c \
    core/thx-file-info.c \
    dialog/thunar-chooser-button.c \
    dialog/thunar-chooser-dialog.c \
    dialog/thunar-chooser-model.c \
    dialog/thunar-dialogs.c \
    dialog/thunar-permissions-chooser.c \
    dialog/thunar-progress-dialog.c \
    dialog/thunar-progress-view.c \
    dialog/thunar-properties-dialog.c \
    job/io-jobs.c \
    job/io-scan-directory.c \
    libext/exo-cell-renderer-icon.c \
    libext/exo-gdk-pixbuf-extensions.c \
    libext/exo-job.c \
    libext/exo-tree-view.c \
    libext/libext.c \
    misc/enum-types.c \
    misc/thunar-browser.c \
    misc/thunar-component.c \
    misc/thunar-gdk-extensions.c \
    misc/thunar-gio-extensions.c \
    misc/thunar-gobject-extensions.c \
    misc/thunar-gtk-extensions.c \
    misc/thunar-history.c \
    misc/thunar-navigator.c \
    misc/thunar-notify.c \
    misc/thunar-pango-extensions.c \
    job/thunar-deep-count-job.c \
    job/thunar-io-jobs-util.c \
    job/thunar-job.c \
    job/thunar-simple-job.c \
    job/thunar-transfer-job.c \
    menu/thunar-launcher.c \
    menu/thunar-menu.c \
    side/thunar-side-pane.c \
    side/thunar-tree-model.c \
    side/thunar-tree-pane.c \
    side/thunar-tree-view.c \
    view/list-model.c \
    view/standard-view.c \
    view/thunar-column-model.c \
    view/thunar-details-view.c \
    view/thunar-view.c \
    widget/thunar-icon-factory.c \
    widget/thunar-icon-renderer.c \
    widget/thunar-image.c \
    widget/thunar-location-bar.c \
    widget/thunar-location-entry.c \
    widget/thunar-path-entry.c \
    widget/thunar-shortcuts-icon-renderer.c \
    widget/thunar-size-label.c \
    widget/thunar-statusbar.c \
    main.c \
    preferences.c \
    thunar-application.c \
    thunar-marshal.c \
    thunar-window.c \

DISTFILES = \
    Notes.md \
    deps_search.txt \
    install.sh \
    meson.build \
    Readme.md \
    thunar-marshal.list \


