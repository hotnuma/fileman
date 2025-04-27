
#if 0

ThunarFileMode th_file_get_mode(const ThunarFile *file)
{
    e_return_val_if_fail(IS_THUNARFILE(file), 0);

    if (file->gfileinfo == NULL)
        return 0;

    if (g_file_info_has_attribute(file->gfileinfo, G_FILE_ATTRIBUTE_UNIX_MODE))
        return g_file_info_get_attribute_uint32(file->gfileinfo, G_FILE_ATTRIBUTE_UNIX_MODE);
    else
        return th_file_is_directory(file) ? 0777 : 0666;
}

static void _execute_mkdir(gpointer parent,
                           GList *file_list, GClosure *new_files_closure);
static void _execute_mkdir(gpointer parent,
                           GList *file_list, GClosure *new_files_closure)
{
    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));

    Application *application = application_get();
    e_return_if_fail(IS_APPLICATION(application));

    application_launch(application,
                       parent,
                       "folder-new",
                       _("Creating directories..."),
                       io_make_directories,
                       file_list,
                       file_list,
                       TRUE,
                       FALSE,
                       new_files_closure);

    g_object_unref(G_OBJECT(application));
}

static void _execute_create(gpointer parent,
                            GList *file_list, GFile *template_file,
                            GClosure *new_files_closure);
static void _execute_create(gpointer parent, GList *file_list,
                            GFile *template_file, GClosure *new_files_closure)
{
    e_return_if_fail(parent == NULL
                     || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));

    GList template_list;
    template_list.next = template_list.prev = NULL;
    template_list.data = template_file;

    Application *application = application_get();
    e_return_if_fail(IS_APPLICATION(application));

    application_launch(application,
                        parent,
                        "document-new",
                        _("Creating files..."),
                        io_create_files,
                        &template_list,
                        file_list,
                        FALSE,
                        TRUE,
                        new_files_closure);

    g_object_unref(G_OBJECT(application));
}

static void _execute_empty_trash(gpointer parent, const gchar *startup_id);

    _execute_empty_trash(launcher->widget, NULL);
}
static void _execute_empty_trash(gpointer parent, const gchar *startup_id)
{
    e_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent)
                     || GTK_IS_WIDGET(parent));

#endif


