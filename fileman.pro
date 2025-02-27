TEMPLATE = app
TARGET = fileman
CONFIG = c99 link_pkgconfig
DEFINES = _GNU_SOURCE GLIB_CHECK_VERSION bool=BOOL true=TRUE false=FALSE
INCLUDEPATH = core dialog job libext misc side view widget
PKGCONFIG =

PKGCONFIG += gtk+-3.0
PKGCONFIG += tinyc
PKGCONFIG += libnotify
PKGCONFIG += libxfce4ui-2

HEADERS = \
    core/appnotify.h \
    core/clipboard.h \
    core/devmonitor.h \
    core/dnd.h \
    core/fileinfo.h \
    core/filemonitor.h \
    core/th-device.h \
    core/th-file.h \
    core/th-folder.h \
    core/usermanager.h \
    dialog/appchooser.h \
    dialog/appcombo.h \
    dialog/appmodel.h \
    dialog/dialogs.h \
    dialog/permbox.h \
    dialog/progressdlg.h \
    dialog/progressview.h \
    dialog/propsdlg.h \
    job/dcountjob.h \
    job/exo-job.h \
    job/io-jobs.h \
    job/io-scandir.h \
    job/job.h \
    job/jobutils.h \
    job/simplejob.h \
    job/transferjob.h \
    launcher.h \
    libext/etktype.h \
    libext/gdk-ext.h \
    libext/gio-ext.h \
    libext/gtk-ext.h \
    libext/pango-ext.h \
    libext/pixbuf-ext.h \
    libext/utils.h \
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
    view/exotreeview.h \
    view/listmodel.h \
    view/standardview.h \
    widget/locationbar.h \
    widget/locationentry.h \
    widget/pathentry.h \
    widget/sizelabel.h \
    widget/statusbar.h \
    widget/th-image.h \
    application.h \
    appmenu.h \
    appwindow.h \
    config.h \
    debug.h \
    preferences.h \

SOURCES = \
    core/appnotify.c \
    core/clipboard.c \
    core/devmonitor.c \
    core/dnd.c \
    core/fileinfo.c \
    core/filemonitor.c \
    core/th-device.c \
    core/th-file.c \
    core/th-folder.c \
    core/usermanager.c \
    dialog/appchooser.c \
    dialog/appcombo.c \
    dialog/appmodel.c \
    dialog/dialogs.c \
    dialog/permbox.c \
    dialog/progressdlg.c \
    dialog/progressview.c \
    dialog/propsdlg.c \
    job/dcountjob.c \
    job/exo-job.c \
    job/io-jobs.c \
    job/io-scandir.c \
    job/job.c \
    job/jobutils.c \
    job/simplejob.c \
    job/transferjob.c \
    launcher.c \
    libext/gdk-ext.c \
    libext/gio-ext.c \
    libext/gtk-ext.c \
    libext/pango-ext.c \
    libext/pixbuf-ext.c \
    libext/utils.c \
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
    view/exotreeview.c \
    view/listmodel.c \
    view/standardview.c \
    widget/locationbar.c \
    widget/locationentry.c \
    widget/pathentry.c \
    widget/sizelabel.c \
    widget/statusbar.c \
    widget/th-image.c \
    0temp.c \
    appmenu.c \
    appwindow.c \
    application.c \
    main.c \
    preferences.c \

DISTFILES = \
    deps_search.txt \
    install.sh \
    marshal.list \
    meson.build \
    Notes.md \
    Readme.md \


