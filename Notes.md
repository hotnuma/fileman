

#### TODO

* remove C-style comments

find . -type f -name "*.c" -printf "%s\t%p\n" | sort -nr

+   127451	./view/standard_view.c
+   102177	./core/th_file.c
+   95911	./launcher.c
+   70707	./view/listmodel.c
+   70220	./side/treeview.c
+   68375	./window.c
+   60747	./side/treemodel.c
+   60213	./job/transfer_job.c
+   55265	./application.c
+   50846	./dialog/permbox.c
    44101	./dialog/appchooser.c
    43641	./dialog/propsdlg.c
    43501	./job/io_jobs.c
    41070	./widget/pathentry.c
    40301	./view/detailview.c
    38433	./dialog/dialogs.c
    33723	./libext/exo_tree_view.c
    30271	./core/th_folder.c
    28869	./misc/icon_factory.c
    27798	./view/column_model.c
    24656	./core/th_device.c
    24424	./job/job.c
    24323	./misc/browser.c
    23925	./dialog/progress_view.c
    22433	./core/devmon.c
    21420	./core/clipman.c
    19735	./misc/history.c
    19042	./misc/enum_types.c
    18274	./libext/gio_ext.c
    18067	./libext/exo_job.c
    18012	./libext/utils.c
    17925	./dialog/appcombo.c
    15914	./misc/icon_render.c
    15093	./widget/sizelabel.c
    14752	./widget/locationentry.c
    14556	./appmenu.c
    14348	./marshal.c
    14307	./core/user.c
    13649	./dialog/progress_dlg.c
    13212	./dialog/appmodel.c
    11857	./job/dcount_job.c
    10476	./libext/pixbuf_ext.c
    9982	./view/baseview.c
    9732	./core/dnd.c
    9639	./misc/srender.c
    9372	./widget/locationbar.c
    9157	./libext/exo.c
    9015	./side/treepane.c
    7617	./job/job_utils.c
    7231	./libext/gtk_ext.c
    6860	./misc/navigator.c
    6626	./misc/app_notify.c
    5838	./libext/pango_ext.c
    5587	./widget/th_image.c
    5560	./job/io_scan_directory.c
    5452	./job/simple_job.c
    5425	./core/fileinfo.c
    5013	./libext/gdk_ext.c
    4816	./core/filemon.c
    4558	./widget/statusbar.c
    4544	./misc/component.c
    2598	./preferences.c
    2359	./side/sidepane.c
    1422	./main.c

* Port SizeLabel to GtkBox
    
    ```
    SizeLabel
        GtkHBox
    ```


