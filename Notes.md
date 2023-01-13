#### Biggest files

```
find . -type f -name "*.c" -printf "%s\t%p\n" | sort -nr | head -15

+   128008	./view/standard-view.c
+   102794	./core/th-file.c
+   96577	./menu/launcher.c
+   71996	./window.c
+   71060	./view/listmodel.c
+   70235	./side/treeview.c
+   68449	./application.c
+   61154	./side/treemodel.c
+   60480	./job/transfer-job.c
+   51175	./dialog/permbox.c
+   44139	./widget/path-entry.c
+   44121	./job/io-jobs.c
+   43849	./dialog/propsdlg.c
+   43724	./dialog/appchooser.c
+   40474	./view/detailview.c
+   38645	./dialog/dialogs.c
+   33719	./libext/exo-tree-view.c
+   33578	./view/column-model.c
    30881	./widget/icon-factory.c
+   30354	./core/th-folder.c
    25293	./job/job.c
+   24813	./core/th-device.c
    24589	./misc/browser.c
+   24507	./libext/gdkpixbuf-ext.c
    24217	./dialog/progress-view.c
    22837	./libext/exo-icon.c
+   22570	./core/devmon.c
    22332	./widget/icon-render.c
+   21570	./core/clipman.c
    20594	./misc/history.c
+   19810	./dialog/appcombo.c
    19073	./misc/enum-types.c
    18633	./libext/utils.c
+   18560	./libext/gio-ext.c
    18018	./libext/exo-job.c
    16890	./widget/location-entry.c
    16491	./widget/size-label.c
+   14503	./menu/menu.c
+   14312	./core/user.c
    13752	./dialog/progress-dlg.c
    12321	./job/deep-count-job.c
    10992	./widget/shortcut-render.c
    10711	./widget/location-bar.c
+   10567	./dialog/appmodel.c
    10313	./libext/gtk-ext.c
+   10060	./view/baseview.c
+   9765	./core/dnd.c
+   9569	./libext/libext.c
+   9029	./side/treepane.c
    7837	./job/io-jobs-util.c
    6857	./misc/navigator.c
+   6654	./misc/app-notify.c
    6286	./widget/image.c
    5994	./libext/pango-ext.c
    5943	./job/simple-job.c
+   5580	./job/io-scan-directory.c
    5449	./core/fileinfo.c
    5019	./libext/gdk-ext.c
    4783	./core/filemon.c
    4712	./misc/component.c
    4708	./libext/gobject-ext.c
    4597	./widget/statusbar.c
    2591	./preferences.c
    2372	./side/sidepane.c
    1985	./main.c
```



#### TODO

* Port ThunarLocationEntry to GtkBox
    
    ```
    ThunarLocationEntry
        GtkHBox
    ```

* Preferences options cleanup

<property name="last-details-view-column-widths" type="string"
value="50,123,50,50,347,50,50,73,50,91"/>

<!--
metadata

gboolean directory_specific_settings;
thunar_file_get_metadata_setting()

HAVE_LINUX

CTYPE_H
ERRNO_H
FCNTL_H
GRP_H
LIMITS_H
LOCALE_H
MEMORY_H
PATHS_H
PWD_H
SCHED_H
SIGNAL_H
STDARG_H
STDLIB_H
STRING_H
SYS_MMAN_H
SYS_PARAM_H
SYS_STAT_H
SYS_TIME_H
SYS_TYPES_H
SYS_UIO_H
SYS_WAIT_H
TIME_H

AC_FUNC_MMAP()
-->


