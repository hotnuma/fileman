#### Formatting
    
dialog/progress-dlg

listmodel

#### Biggest files

find . -type f -name "*.c" -printf "%s\t%p\n" | sort -nr | head -15

128247	./view/standard-view.c
102794	./core/th-file.c
96577	./menu/launcher.c
75555	./view/listmodel.c
72057	./window.c
70235	./side/treeview.c
68445	./application.c
62566	./job/transfer-job.c
61154	./side/treemodel.c
51175	./dialog/permbox.c
45043	./job/io-jobs.c
44158	./widget/path-entry.c
43849	./dialog/propsdlg.c
43724	./dialog/appchooser.c



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


