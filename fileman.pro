TEMPLATE = app
TARGET = fileman
CONFIG = c11 link_pkgconfig
DEFINES = HAVE_CONFIG_H _XOPEN_SOURCE

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
    core/devmon.h \
    core/dnd.h \
    core/fileinfo.h \
    core/filemon.h \
    core/th-device.h \
    core/th-file.h \
    core/th-folder.h \
    core/user.h \
    dialog/appchooser.h \
    dialog/appcombo.h \
    dialog/appmodel.h \
    dialog/dialogs.h \
    dialog/permbox.h \
    dialog/progress-dlg.h \
    dialog/progress-view.h \
    dialog/propsdlg.h \
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
    libext/gio-ext.h \
    libext/gobject-ext.h \
    libext/gtk-ext.h \
    libext/libext.h \
    libext/pango-ext.h \
    libext/pixbuf-ext.h \
    libext/utils.h \
    menu/launcher.h \
    menu/menu.h \
    misc/app-notify.h \
    misc/browser.h \
    misc/component.h \
    misc/enum-types.h \
    misc/history.h \
    misc/icon-factory.h \
    misc/icon-render.h \
    misc/navigator.h \
    misc/shortcut-render.h \
    side/sidepane.h \
    side/treemodel.h \
    side/treepane.h \
    side/treeview.h \
    view/baseview.h \
    view/column-model.h \
    view/detailview.h \
    view/listmodel.h \
    view/standard-view.h \
    widget/image.h \
    widget/location-bar.h \
    widget/location-entry.h \
    widget/pathentry.h \
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
    core/devmon.c \
    core/dnd.c \
    core/fileinfo.c \
    core/filemon.c \
    core/th-device.c \
    core/th-file.c \
    core/th-folder.c \
    core/user.c \
    dialog/appchooser.c \
    dialog/appcombo.c \
    dialog/appmodel.c \
    dialog/dialogs.c \
    dialog/permbox.c \
    dialog/progress-dlg.c \
    dialog/progress-view.c \
    dialog/propsdlg.c \
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
    libext/gio-ext.c \
    libext/gobject-ext.c \
    libext/gtk-ext.c \
    libext/libext.c \
    libext/pango-ext.c \
    libext/pixbuf-ext.c \
    libext/utils.c \
    menu/launcher.c \
    menu/menu.c \
    misc/app-notify.c \
    misc/browser.c \
    misc/component.c \
    misc/enum-types.c \
    misc/history.c \
    misc/icon-factory.c \
    misc/icon-render.c \
    misc/navigator.c \
    misc/shortcut-render.c \
    side/sidepane.c \
    side/treemodel.c \
    side/treepane.c \
    side/treeview.c \
    view/baseview.c \
    view/column-model.c \
    view/detailview.c \
    view/listmodel.c \
    view/standard-view.c \
    widget/image.c \
    widget/location-bar.c \
    widget/location-entry.c \
    widget/pathentry.c \
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


