
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


