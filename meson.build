project(
    'fileman',
    ['c'],
    version : '1.0',
    license : 'GPL-2.0',
    default_options : [
        'c_std=c99',
    ],
)

prefix = get_option('prefix')
datadir = join_paths(prefix, get_option('datadir'))

gnome = import('gnome')

marshal = gnome.genmarshal(
    'marshal',
    sources : 'marshal.list',
    prefix : '_thunar_marshal',
)
marshal_c = marshal[0]
marshal_h = marshal[1]

c_args = [
    '-Wall',
    '-Wextra',
    '-O2',
    '-D_GNU_SOURCE',
    '-DG_UDEV_API_IS_SUBJECT_TO_CHANGE',
    '-DSN_API_NOT_YET_FROZEN',
]

app_includes = [
    'core',
    'dialog',
    'job',
    'libext',
    'misc',
    'side',
    'view',
    'widget',
]

app_deps = [
    dependency('gtk+-3.0'),
    dependency('tinyc'),
    dependency('libnotify'),
    dependency('libxfce4ui-2'),
]

app_sources = [
    'core/appnotify.c',
    'core/clipboard.c',
    'core/devmonitor.c',
    'core/dnd.c',
    'core/fileinfo.c',
    'core/filemonitor.c',
    'core/th_device.c',
    'core/th_file.c',
    'core/th_folder.c',
    'core/usermanager.c',
    'dialog/appchooser.c',
    'dialog/appcombo.c',
    'dialog/appmodel.c',
    'dialog/dialogs.c',
    'dialog/permbox.c',
    'dialog/progressdlg.c',
    'dialog/progressview.c',
    'dialog/propsdlg.c',
    'job/dcountjob.c',
    'job/exo_job.c',
    'job/io_jobs.c',
    'job/io_scandir.c',
    'job/job.c',
    'job/jobutils.c',
    'job/simplejob.c',
    'job/transferjob.c',
    'libext/gdk_ext.c',
    'libext/gio_ext.c',
    'libext/gtk_ext.c',
    'libext/pango_ext.c',
    'libext/pixbuf_ext.c',
    'libext/utils.c',
    'misc/browser.c',
    'misc/component.c',
    'misc/enumtypes.c',
    'misc/history.c',
    'misc/iconfactory.c',
    'misc/iconrender.c',
    'misc/navigator.c',
    'misc/shortrender.c',
    'side/sidepane.c',
    'side/treemodel.c',
    'side/treepane.c',
    'side/treeview.c',
    'view/baseview.c',
    'view/columnmodel.c',
    'view/detailview.c',
    'view/exotreeview.c',
    'view/listmodel.c',
    'view/standardview.c',
    'widget/th_image.c',
    'widget/locationbar.c',
    'widget/locationentry.c',
    'widget/pathentry.c',
    'widget/sizelabel.c',
    'widget/statusbar.c',
    'application.c',
    'appmenu.c',
    'appwindow.c',
    'launcher.c',
    'main.c',
    'preferences.c',
]

executable(
    meson.project_name(),
    c_args : c_args,
    include_directories : app_includes,
    dependencies : app_deps,
    sources : [app_sources, marshal_h, marshal_c],
    install : true,
)

install_data(
    join_paths('data', meson.project_name() + '.desktop'),
    install_dir: join_paths(datadir, 'applications')
)


