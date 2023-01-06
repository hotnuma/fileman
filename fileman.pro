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
    application.h \
    core/clipman.h \
    core/device.h \
    core/devmon.h \
    core/dnd.h \
    core/filemon.h \
    core/th-file.h \
    core/th-folder.h \
    core/thx-file-info.h \
    core/user.h \
    debug.h \
    dialog/dialogs.h \
    dialog/thunar-chooser-button.h \
    dialog/thunar-chooser-dialog.h \
    dialog/thunar-chooser-model.h \
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
    libext/egdk-pixbuf.h \
    libext/exo-icon.h \
    libext/exo-job.h \
    libext/exo-tree-view.h \
    libext/gdk-extensions.h \
    libext/gio-extensions.h \
    libext/gobject-extensions.h \
    libext/gtk-extensions.h \
    libext/libext.h \
    libext/pango-extensions.h \
    libext/utils.h \
    marshal.h \
    menu/launcher.h \
    menu/menu.h \
    misc/enum-types.h \
    misc/thunar-browser.h \
    misc/thunar-component.h \
    misc/thunar-history.h \
    misc/thunar-navigator.h \
    misc/thunar-notify.h \
    side/thunar-side-pane.h \
    side/thunar-tree-model.h \
    side/thunar-tree-pane.h \
    side/thunar-tree-view.h \
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
    window.h

SOURCES = \
    0Temp.c \
    application.c \
    core/clipman.c \
    core/device.c \
    core/devmon.c \
    core/dnd.c \
    core/filemon.c \
    core/th-file.c \
    core/th-folder.c \
    core/thx-file-info.c \
    core/user.c \
    core/utils.c \
    dialog/dialogs.c \
    dialog/thunar-chooser-button.c \
    dialog/thunar-chooser-dialog.c \
    dialog/thunar-chooser-model.c \
    dialog/thunar-permissions-chooser.c \
    dialog/thunar-progress-dialog.c \
    dialog/thunar-progress-view.c \
    dialog/thunar-properties-dialog.c \
    job/io-jobs.c \
    job/io-scan-directory.c \
    libext/egdk-pixbuf.c \
    libext/exo-icon.c \
    libext/exo-job.c \
    libext/exo-tree-view.c \
    libext/gdk-extensions.c \
    libext/gio-extensions.c \
    libext/gobject-extensions.c \
    libext/gtk-extensions.c \
    libext/libext.c \
    libext/pango-extensions.c \
    marshal.c \
    menu/launcher.c \
    menu/menu.c \
    misc/enum-types.c \
    misc/thunar-browser.c \
    misc/thunar-component.c \
    misc/thunar-history.c \
    misc/thunar-navigator.c \
    misc/thunar-notify.c \
    job/thunar-deep-count-job.c \
    job/thunar-io-jobs-util.c \
    job/thunar-job.c \
    job/thunar-simple-job.c \
    job/thunar-transfer-job.c \
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
    window.c

DISTFILES = \
    Notes.md \
    deps_search.txt \
    install.sh \
    marshal.list \
    meson.build \
    Readme.md \


