TEMPLATE = app
TARGET = fileman
CONFIG = c11 link_pkgconfig
DEFINES = HAVE_CONFIG_H

PKGCONFIG = \
    gtk+-3.0 \
    tinyc \
    gudev-1.0 \
    libnotify \
    libxfce4ui-2 \
    thunarx-3 \

INCLUDEPATH = core dialog job libext menu misc side view widget

HEADERS = \
    core/clipman.h \
    core/device.h \
    core/devmon.h \
    core/dnd.h \
    core/filemon.h \
    core/th-file.h \
    core/th-folder.h \
    core/thx-file-info.h \
    core/user.h \
    dialog/chooser-combo.h \
    dialog/chooser-dlg.h \
    dialog/chooser-model.h \
    dialog/dialogs.h \
    dialog/permissions.h \
    dialog/progress-dlg.h \
    dialog/progress-view.h \
    dialog/properties-dlg.h \
    job/deep-count-job.h \
    job/io-jobs-util.h \
    job/io-jobs.h \
    job/io-scan-directory.h \
    job/job.h \
    job/simple-job.h \
    job/transfer-job.h \
    libext/exo-icon.h \
    libext/exo-job.h \
    libext/exo-tree-view.h \
    libext/gdk-ext.h \
    libext/gdkpixbuf-ext.h \
    libext/gio-ext.h \
    libext/gobject-ext.h \
    libext/gtk-ext.h \
    libext/libext.h \
    libext/pango-ext.h \
    libext/utils.h \
    menu/launcher.h \
    menu/menu.h \
    misc/browser.h \
    misc/component.h \
    misc/enum-types.h \
    misc/history.h \
    misc/navigator.h \
    misc/notify.h \
    side/side-pane.h \
    side/tree-model.h \
    side/tree-pane.h \
    side/tree-view.h \
    view/column-model.h \
    view/detail-view.h \
    view/list-model.h \
    view/standard-view.h \
    view/view.h \
    widget/icon-factory.h \
    widget/icon-render.h \
    widget/image.h \
    widget/location-bar.h \
    widget/location-entry.h \
    widget/path-entry.h \
    widget/shortcut-render.h \
    widget/size-label.h \
    application.h \
    config.h \
    debug.h \
    marshal.h \
    preferences.h \
    widget/statusbar.h \
    window.h

SOURCES = \
    0Temp.c \
    core/clipman.c \
    core/device.c \
    core/devmon.c \
    core/dnd.c \
    core/filemon.c \
    core/th-file.c \
    core/th-folder.c \
    core/thx-file-info.c \
    core/user.c \
    dialog/chooser-combo.c \
    dialog/chooser-dlg.c \
    dialog/chooser-model.c \
    dialog/dialogs.c \
    dialog/permissions.c \
    dialog/progress-dlg.c \
    dialog/progress-view.c \
    dialog/properties-dlg.c \
    job/deep-count-job.c \
    job/io-jobs-util.c \
    job/io-jobs.c \
    job/io-scan-directory.c \
    job/job.c \
    job/simple-job.c \
    job/transfer-job.c \
    libext/exo-icon.c \
    libext/exo-job.c \
    libext/exo-tree-view.c \
    libext/gdk-ext.c \
    libext/gdkpixbuf-ext.c \
    libext/gio-ext.c \
    libext/gobject-ext.c \
    libext/gtk-ext.c \
    libext/libext.c \
    libext/pango-ext.c \
    libext/utils.c \
    menu/launcher.c \
    menu/menu.c \
    misc/browser.c \
    misc/component.c \
    misc/enum-types.c \
    misc/history.c \
    misc/navigator.c \
    misc/notify.c \
    side/side-pane.c \
    side/tree-model.c \
    side/tree-pane.c \
    side/tree-view.c \
    view/column-model.c \
    view/detail-view.c \
    view/list-model.c \
    view/standard-view.c \
    view/view.c \
    widget/icon-factory.c \
    widget/icon-render.c \
    widget/image.c \
    widget/location-bar.c \
    widget/location-entry.c \
    widget/path-entry.c \
    widget/shortcut-render.c \
    widget/size-label.c \
    application.c \
    main.c \
    marshal.c \
    preferences.c \
    widget/statusbar.c \
    window.c

DISTFILES = \
    Notes.md \
    deps_search.txt \
    install.sh \
    marshal.list \
    meson.build \
    Readme.md \


