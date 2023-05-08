TEMPLATE = app
TARGET = fileman
CONFIG = c99 link_pkgconfig
DEFINES = _GNU_SOURCE
INCLUDEPATH = core dialog job libext misc side view widget
PKGCONFIG =

PKGCONFIG += gtk+-3.0
PKGCONFIG += tinyc
PKGCONFIG += libnotify
PKGCONFIG += libxfce4ui-2

HEADERS = \
    appmenu.h \
    core/clipboard.h \
    core/devmonitor.h \
    core/dnd.h \
    core/fileinfo.h \
    core/filemonitor.h \
    core/th_device.h \
    core/th_file.h \
    core/th_folder.h \
    core/user.h \
    dialog/appchooser.h \
    dialog/appcombo.h \
    dialog/appmodel.h \
    dialog/dialogs.h \
    dialog/permbox.h \
    dialog/progressdlg.h \
    dialog/progressview.h \
    dialog/propsdlg.h \
    job/dcount_job.h \
    job/io_jobs.h \
    job/io_scan_directory.h \
    job/job.h \
    job/job_utils.h \
    job/simple_job.h \
    job/transfer_job.h \
    launcher.h \
    libext/exo_job.h \
    libext/exo_treeview.h \
    libext/gdk_ext.h \
    libext/gio_ext.h \
    libext/gtk_ext.h \
    libext/pango_ext.h \
    libext/pixbuf_ext.h \
    libext/utils.h \
    misc/appnotify.h \
    misc/browser.h \
    misc/component.h \
    misc/enumtypes.h \
    misc/history.h \
    misc/iconfactory.h \
    misc/iconrender.h \
    misc/navigator.h \
    misc/shortrender.h \
    side/sidepane.h \
    side/treemodel.h \
    side/treepane.h \
    side/treeview.h \
    view/baseview.h \
    view/columnmodel.h \
    view/detailview.h \
    view/listmodel.h \
    view/standardview.h \
    widget/locationbar.h \
    widget/locationentry.h \
    widget/pathentry.h \
    widget/sizelabel.h \
    widget/statusbar.h \
    application.h \
    config.h \
    debug.h \
    marshal.h \
    preferences.h \
    widget/th_image.h \
    window.h

SOURCES = \
    0Temp.c \
    appmenu.c \
    core/clipboard.c \
    core/devmonitor.c \
    core/dnd.c \
    core/fileinfo.c \
    core/filemonitor.c \
    core/th_device.c \
    core/th_file.c \
    core/th_folder.c \
    core/user.c \
    dialog/appchooser.c \
    dialog/appcombo.c \
    dialog/appmodel.c \
    dialog/dialogs.c \
    dialog/permbox.c \
    dialog/progressdlg.c \
    dialog/progressview.c \
    dialog/propsdlg.c \
    job/dcount_job.c \
    job/io_jobs.c \
    job/io_scan_directory.c \
    job/job.c \
    job/job_utils.c \
    job/simple_job.c \
    job/transfer_job.c \
    launcher.c \
    libext/exo_job.c \
    libext/exo_treeview.c \
    libext/gdk_ext.c \
    libext/gio_ext.c \
    libext/gtk_ext.c \
    libext/pango_ext.c \
    libext/pixbuf_ext.c \
    libext/utils.c \
    misc/appnotify.c \
    misc/browser.c \
    misc/component.c \
    misc/enumtypes.c \
    misc/history.c \
    misc/iconfactory.c \
    misc/iconrender.c \
    misc/navigator.c \
    misc/shortrender.c \
    side/sidepane.c \
    side/treemodel.c \
    side/treepane.c \
    side/treeview.c \
    view/baseview.c \
    view/columnmodel.c \
    view/detailview.c \
    view/listmodel.c \
    view/standardview.c \
    widget/locationbar.c \
    widget/locationentry.c \
    widget/pathentry.c \
    widget/sizelabel.c \
    widget/statusbar.c \
    application.c \
    main.c \
    marshal.c \
    preferences.c \
    widget/th_image.c \
    window.c

DISTFILES = \
    deps_search.txt \
    install.sh \
    marshal.list \
    meson.build \
    Notes.md \
    Readme.md \


