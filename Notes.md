#### Formatting
    
core/th-file
core/th-folder

treemodel

#### Biggest files

find . -type f -name "*.c" -printf "%s\t%p\n" | sort -nr | head -15

129511	./view/standard-view.c
102854	./core/th-file.c
97137	./menu/launcher.c
75604	./view/list-model.c
74250	./window.c
72503	./side/treeview.c
70502	./application.c
65623	./side/treemodel.c
62578	./job/transfer-job.c
53843	./dialog/permissions.c
52050	./dialog/properties-dlg.c
47185	./dialog/chooser-dlg.c
45043	./job/io-jobs.c
44158	./widget/path-entry.c
43267	./view/detail-view.c



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


