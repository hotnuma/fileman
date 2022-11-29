TEMPLATE = app
TARGET = fileman
CONFIG = c11 link_pkgconfig
DEFINES = GTK HAVE_CONFIG_H

PKGCONFIG = \
    gudev-1.0 \
    libnotify \
    gtk+-3.0 \
    exo-2 \
    libxfconf-0 \
    libxfce4util-1.0 \
    libxfce4ui-2 \
    thunarx-3 \

INCLUDEPATH = core dialog extension job menu side view widget

HEADERS = \
    core/thunar-browser.h \
    core/thunar-clipboard-manager.h \
    core/thunar-component.h \
    core/thunar-device.h \
    core/thunar-device-monitor.h \
    core/thunar-dnd.h \
    core/thunar-enum-types.h \
    core/thunar-file.h \
    core/thunar-file-monitor.h \
    core/thunar-folder.h \
    core/thunar-history.h \
    core/thunar-navigator.h \
    core/thunar-notify.h \
    core/thunar-user.h \
    core/thunar-util.h \
    dialog/thunar-chooser-button.h \
    dialog/thunar-chooser-dialog.h \
    dialog/thunar-chooser-model.h \
    dialog/thunar-dialogs.h \
    dialog/thunar-permissions-chooser.h \
    dialog/thunar-progress-dialog.h \
    dialog/thunar-progress-view.h \
    dialog/thunar-properties-dialog.h \
    extension/thunar-gdk-extensions.h \
    extension/thunar-gio-extensions.h \
    extension/thunar-gobject-extensions.h \
    extension/thunar-gtk-extensions.h \
    extension/thunar-pango-extensions.h \
    job/thunar-deep-count-job.h \
    job/thunar-io-jobs.h \
    job/thunar-io-jobs-util.h \
    job/thunar-io-scan-directory.h \
    job/thunar-job.h \
    job/thunar-simple-job.h \
    job/thunar-transfer-job.h \
    menu/thunar-launcher.h \
    menu/thunar-menu.h \
    menu/thunar-sendto-model.h \
    side/thunar-side-pane.h \
    side/thunar-tree-model.h \
    side/thunar-tree-pane.h \
    side/thunar-tree-view.h \
    view/thunar-column-editor.h \
    view/thunar-column-model.h \
    view/thunar-details-view.h \
    view/thunar-list-model.h \
    view/thunar-standard-view.h \
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
    thunar-application.h \
    thunar-marshal.h \
    thunar-preferences.h \
    thunar-private.h \
    thunar-window.h \

SOURCES = \
  0Temp.c \
    core/thunar-browser.c \
    core/thunar-clipboard-manager.c \
    core/thunar-component.c \
    core/thunar-device.c \
    core/thunar-device-monitor.c \
    core/thunar-dnd.c \
    core/thunar-enum-types.c \
    core/thunar-file.c \
    core/thunar-file-monitor.c \
    core/thunar-folder.c \
    core/thunar-history.c \
    core/thunar-navigator.c \
    core/thunar-notify.c \
    core/thunar-user.c \
    core/thunar-util.c \
    dialog/thunar-chooser-button.c \
    dialog/thunar-chooser-dialog.c \
    dialog/thunar-chooser-model.c \
    dialog/thunar-dialogs.c \
    dialog/thunar-permissions-chooser.c \
    dialog/thunar-progress-dialog.c \
    dialog/thunar-progress-view.c \
    dialog/thunar-properties-dialog.c \
    extension/thunar-gdk-extensions.c \
    extension/thunar-gio-extensions.c \
    extension/thunar-gobject-extensions.c \
    extension/thunar-gtk-extensions.c \
    extension/thunar-pango-extensions.c \
    job/thunar-deep-count-job.c \
    job/thunar-io-jobs.c \
    job/thunar-io-jobs-util.c \
    job/thunar-io-scan-directory.c \
    job/thunar-job.c \
    job/thunar-simple-job.c \
    job/thunar-transfer-job.c \
    menu/thunar-launcher.c \
    menu/thunar-menu.c \
    menu/thunar-sendto-model.c \
    side/thunar-side-pane.c \
    side/thunar-tree-model.c \
    side/thunar-tree-pane.c \
    side/thunar-tree-view.c \
    view/thunar-column-editor.c \
    view/thunar-column-model.c \
    view/thunar-details-view.c \
    view/thunar-list-model.c \
    view/thunar-standard-view.c \
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
    thunar-application.c \
    thunar-marshal.c \
    thunar-preferences.c \
    thunar-window.c \

DISTFILES = \
    install.sh \
    meson.build \
    Readme.md \
    thunar-marshal.list \


