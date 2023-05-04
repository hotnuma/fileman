TEMPLATE = app
TARGET = fileman
CONFIG = c99 link_pkgconfig
DEFINES = _GNU_SOURCE
PKGCONFIG =

PKGCONFIG += gtk+-3.0
PKGCONFIG += tinyc
PKGCONFIG += libnotify
PKGCONFIG += libxfce4ui-2

#PKGCONFIG += gudev-1.0

INCLUDEPATH = core dialog job libext menu misc side view widget

HEADERS = \
    core/clipman.h \
    core/devmon.h \
    core/dnd.h \
    core/fileinfo.h \
    core/filemon.h \
    core/th_device.h \
    core/th_file.h \
    core/th_folder.h \
    core/user.h \
    dialog/appchooser.h \
    dialog/appcombo.h \
    dialog/appmodel.h \
    dialog/dialogs.h \
    dialog/permbox.h \
    dialog/progress-dlg.h \
    dialog/progress-view.h \
    dialog/propsdlg.h \
    job/dcount-job.h \
    job/io-jobs.h \
    job/io-scan-directory.h \
    job/job-utils.h \
    job/job.h \
    job/simple-job.h \
    job/transfer-job.h \
    libext/exo-job.h \
    libext/exo-tree-view.h \
    libext/exo.h \
    libext/gdk-ext.h \
    libext/gio-ext.h \
    libext/gtk-ext.h \
    libext/pango-ext.h \
    libext/pixbuf-ext.h \
    libext/utils.h \
    menu/launcher.h \
    menu/menu.h \
    misc/app_notify.h \
    misc/browser.h \
    misc/component.h \
    misc/enum_types.h \
    misc/history.h \
    misc/icon_factory.h \
    misc/icon_render.h \
    misc/navigator.h \
    misc/srender.h \
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
    widget/statusbar.h \
    application.h \
    config.h \
    debug.h \
    marshal.h \
    preferences.h \
    window.h

SOURCES = \
    0Temp.c \
    core/clipman.c \
    core/devmon.c \
    core/dnd.c \
    core/fileinfo.c \
    core/filemon.c \
    core/th_device.c \
    core/th_file.c \
    core/th_folder.c \
    core/user.c \
    dialog/appchooser.c \
    dialog/appcombo.c \
    dialog/appmodel.c \
    dialog/dialogs.c \
    dialog/permbox.c \
    dialog/progress-dlg.c \
    dialog/progress-view.c \
    dialog/propsdlg.c \
    job/dcount-job.c \
    job/io-jobs.c \
    job/io-scan-directory.c \
    job/job-utils.c \
    job/job.c \
    job/simple-job.c \
    job/transfer-job.c \
    libext/exo-job.c \
    libext/exo-tree-view.c \
    libext/exo.c \
    libext/gdk-ext.c \
    libext/gio-ext.c \
    libext/gtk-ext.c \
    libext/pango-ext.c \
    libext/pixbuf-ext.c \
    libext/utils.c \
    menu/launcher.c \
    menu/menu.c \
    misc/app_notify.c \
    misc/browser.c \
    misc/component.c \
    misc/enum_types.c \
    misc/history.c \
    misc/icon_factory.c \
    misc/icon_render.c \
    misc/navigator.c \
    misc/srender.c \
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
    widget/statusbar.c \
    application.c \
    main.c \
    marshal.c \
    preferences.c \
    window.c

DISTFILES = \
    deps_search.txt \
    install.sh \
    marshal.list \
    meson.build \
    Notes.md \
    Readme.md \


