project(
    'fileman',
    ['c'],
    version : '1.0',
    license : 'GPL-2.0',
    default_options : [
        'c_std=c99',
    ],
)

#'-D_XOPEN_SOURCE',

c_args = [
    '-Wall',
    '-Wextra',
    '-O2',
    '-D_GNU_SOURCE',
    '-DG_UDEV_API_IS_SUBJECT_TO_CHANGE',
    '-DSN_API_NOT_YET_FROZEN',
]

cc = meson.get_compiler('c')

app_includes = [
    'core',
    'dialog',
    'job',
    'libext',
    'menu',
    'misc',
    'side',
    'view',
    'widget',
]

#dependency('gudev-1.0'),

app_deps = [
    dependency('gtk+-3.0'),
    dependency('tinyc'),
    dependency('libnotify'),
    dependency('libxfce4ui-2'),
]

app_sources = [
    'core/clipman.c',
    'core/devmon.c',
    'core/dnd.c',
    'core/fileinfo.c',
    'core/filemon.c',
    'core/th_device.c',
    'core/th_file.c',
    'core/th_folder.c',
    'core/user.c',
    'dialog/appchooser.c',
    'dialog/appcombo.c',
    'dialog/appmodel.c',
    'dialog/dialogs.c',
    'dialog/permbox.c',
    'dialog/progress-dlg.c',
    'dialog/progress-view.c',
    'dialog/propsdlg.c',
    'job/dcount-job.c',
    'job/io-jobs.c',
    'job/io-scan-directory.c',
    'job/job-utils.c',
    'job/job.c',
    'job/simple-job.c',
    'job/transfer-job.c',
    'libext/exo-job.c',
    'libext/exo-tree-view.c',
    'libext/exo.c',
    'libext/gdk-ext.c',
    'libext/gio-ext.c',
    'libext/gtk-ext.c',
    'libext/pango-ext.c',
    'libext/pixbuf-ext.c',
    'libext/utils.c',
    'menu/launcher.c',
    'menu/menu.c',
    'misc/app_notify.c',
    'misc/browser.c',
    'misc/component.c',
    'misc/enum_types.c',
    'misc/history.c',
    'misc/icon_factory.c',
    'misc/icon_render.c',
    'misc/navigator.c',
    'misc/srender.c',
    'side/sidepane.c',
    'side/treemodel.c',
    'side/treepane.c',
    'side/treeview.c',
    'view/baseview.c',
    'view/column-model.c',
    'view/detailview.c',
    'view/listmodel.c',
    'view/standard-view.c',
    'widget/image.c',
    'widget/location-bar.c',
    'widget/location-entry.c',
    'widget/pathentry.c',
    'widget/size-label.c',
    'widget/statusbar.c',
    'application.c',
    'main.c',
    'marshal.c',
    'preferences.c',
    'window.c',
]

executable(
    'fileman',
    c_args : c_args,
    include_directories : app_includes,
    dependencies : app_deps,
    sources : app_sources,
    install : true,
)


