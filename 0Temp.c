
#if 0

// AppWindow

const XfceGtkActionEntry* window_get_action_entry(AppWindow *window,
                                                  WindowAction action);
const XfceGtkActionEntry* window_get_action_entry(AppWindow *window,
                                                  WindowAction action)
{
    (void) window;

    return get_action_entry(action);
}

void window_scroll_to_file(AppWindow *window, ThunarFile *file, gboolean select,
                           gboolean use_align, gfloat row_align, gfloat col_align);

void window_scroll_to_file(AppWindow *window, ThunarFile *file,
                           gboolean select_file, gboolean use_align,
                           gfloat row_align, gfloat col_align)
{
    e_return_if_fail(IS_APPWINDOW(window));
    e_return_if_fail(IS_THUNARFILE(file));

    // verify that we have a valid view
    if (G_LIKELY(window->view != NULL))
    {
        baseview_scroll_to_file(BASEVIEW(window->view),
                                file,
                                select_file,
                                use_align, row_align, col_align);
    }
}

gchar** window_get_directories(AppWindow *window, gint *active_page);
gboolean window_set_directories(AppWindow *window, gchar **uris, gint active_page);

gchar** window_get_directories(AppWindow *window, gint *active_page)
{
    e_return_val_if_fail(IS_APPWINDOW(window), NULL);

    gint n_pages;
    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));

    if (G_UNLIKELY(n_pages == 0))
        return NULL;

    // create array of uris
    gchar **uris = g_new0(gchar*, n_pages + 1);

    for (gint n = 0; n < n_pages; ++n)
    {
        // get the view

        GtkWidget *view;
        view = gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), n);
        e_return_val_if_fail(IS_THUNARNAVIGATOR(view), FALSE);

        // get the directory of the view
        ThunarFile *directory;
        directory = navigator_get_current_directory(THUNARNAVIGATOR(view));
        e_return_val_if_fail(IS_THUNARFILE(directory), FALSE);

        // add to array
        uris[n] = th_file_get_uri(directory);
    }

    // selected tab
    if (active_page != NULL)
        *active_page = gtk_notebook_get_current_page(
                                GTK_NOTEBOOK(window->notebook));
    return uris;
}

gboolean window_set_directories(AppWindow *window, gchar **uris,
                                gint active_page)
{
    e_return_val_if_fail(IS_APPWINDOW(window), FALSE);
    e_return_val_if_fail(uris != NULL, FALSE);

    for (guint n = 0; uris[n] != NULL; ++n)
    {
        // check if the string looks like an uri
        if (!g_uri_is_valid(uris[n], G_URI_FLAGS_NONE, NULL))
            continue;

        // get the file for the uri
        ThunarFile *directory = th_file_get_for_uri(uris[n], NULL);
        if (G_UNLIKELY(directory == NULL))
            continue;

        // open the directory in a new notebook
        if (th_file_is_directory(directory))
        {
            if (gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook)) == 0)
                window_set_current_directory(window, directory);
            //else
            //    DPRINT("new tab oops");
        }

        g_object_unref(G_OBJECT(directory));
    }

    // select the page
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), active_page);

    // we succeeded if new pages have been opened
    return gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook)) > 0;
}

#endif

// ----------------------------------------------------------------------------

#if 0

    if (folder->gfilemonitor)
    {
        gchar *file_uri = th_file_get_uri(folder->thunar_file);

        if (file_uri && folder->monitor_uri
            && g_strcmp0(folder->monitor_uri, file_uri) != 0)
        {
            //printf("_monitor_on_changed : %s\n", file_uri);

            g_signal_handlers_disconnect_matched(folder->gfilemonitor,
                                                 G_SIGNAL_MATCH_DATA,
                                                 0, 0, NULL, NULL, folder);
            g_file_monitor_cancel(folder->gfilemonitor);
            g_object_unref(folder->gfilemonitor);

            if (folder->monitor_uri)
                g_free(folder->monitor_uri);

            folder->monitor_uri = file_uri;
            file_uri = NULL;
            folder->gfilemonitor = g_file_monitor_directory(
                                        th_file_get_file(folder->thunar_file),
                                        G_FILE_MONITOR_WATCH_MOVES,
                                        NULL,
                                        NULL);

            if (folder->gfilemonitor)
            {
                g_signal_connect(folder->gfilemonitor, "changed",
                                 G_CALLBACK(_gfmonitor_on_changed), folder);
            }
        }

        if (file_uri)
            g_free(file_uri);
    }




    if (isdir)
    {
        if (event_type == G_FILE_MONITOR_EVENT_RENAMED)
        {
            if (G_LIKELY(folder->gfilemonitor != NULL))
            {
                g_signal_handlers_disconnect_matched(folder->gfilemonitor,
                                                     G_SIGNAL_MATCH_DATA,
                                                     0, 0, NULL, NULL, folder);
                g_file_monitor_cancel(folder->gfilemonitor);
                g_object_unref(folder->gfilemonitor);
            }

            GError *error = NULL;

            folder->gfilemonitor = g_file_monitor_directory(
                                        other_file,
                                        G_FILE_MONITOR_WATCH_MOVES,
                                        NULL,
                                        &error);

            if (G_LIKELY(folder->gfilemonitor != NULL))
            {
                g_signal_connect(folder->gfilemonitor, "changed",
                                 G_CALLBACK(_gfmonitor_on_changed), folder);
            }
            else
            {
                g_debug("Could not create folder monitor: %s", error->message);
                g_error_free(error);
            }
        }

        //return;
    }
#endif


