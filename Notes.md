#### Biggest files

```
    find . -type f -name "*.c" -printf "%s\t%p\n" | sort -nr | head -10
    
    127510	./view/standard_view.c
    102177	./core/th_file.c
    95904	./menu/launcher.c
    70707	./view/listmodel.c
    70126	./side/treeview.c
    68427	./window.c
    60747	./side/treemodel.c
    60213	./job/transfer_job.c
    54371	./application.c
    50846	./dialog/permbox.c
```

#### History

* Exo

    ```
    commit b11f72edef8b72953fb576d9925b7dc06dbd2723 (tag: exo-4.18.0)
    Author: Alexander Schwinn <alexxcons@xfce.org>
    Date:   Thu Dec 15 10:01:20 2022 +0100

        Updates for release
    ```

* libxfce4util


    ```
    commit 32d2db829d78c3f0573fe440a5250954fab5eda9 (tag: libxfce4util-4.18.0)
    Author: Gaël Bonithon <gael@xfce.org>
    Date:   Thu Dec 15 09:54:10 2022 +0100

        Updates for release
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


