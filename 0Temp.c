
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

// Thumbnails -----------------------------------------------------------------

#if 0

// _io_unlink

// try to delete the file
if (_io_delete_file(lp->data, exo_job_get_cancellable(EXOJOB(job)), &err))
{
    /* notify the thumbnail cache that the corresponding thumbnail can also
     * be deleted now */

    //thunar_thumbnail_cache_delete_file(thumbnail_cache, lp->data);
}

#endif

#if 0
// iconfact_load_file_icon

GInputStream    *stream;
GtkIconInfo     *icon_info;
const gchar     *thumbnail_path;
GIcon           *gicon;

// check if thumbnails are enabled and we can display a thumbnail for the item
if (thunar_icon_factory_get_show_thumbnail(factory, file)
        && (thunar_file_is_regular(file) || thunar_file_is_directory(file)))
{
    // determine the preview icon first
    gicon = thunar_file_get_preview_icon(file);

    // check if we have a preview icon
    if (gicon != NULL)
    {
        if (G_IS_THEMED_ICON(gicon))
        {
            // we have a themed preview icon, look it up using the icon theme
            icon_info =
                gtk_icon_theme_lookup_by_gicon(factory->icon_theme,
                                                gicon, icon_size,
                                                GTK_ICON_LOOKUP_USE_BUILTIN
                                                | GTK_ICON_LOOKUP_FORCE_SIZE);

            // check if the lookup succeeded
            if (icon_info != NULL)
            {
                // try to load the pixbuf from the icon info
                icon = gtk_icon_info_load_icon(icon_info, NULL);
                g_object_unref(icon_info);
            }
        }
        else if (G_IS_LOADABLE_ICON(gicon))
        {
            // we have a loadable icon, try to open it for reading
            stream = g_loadable_icon_load(G_LOADABLE_ICON(gicon), icon_size,
                                           NULL, NULL, NULL);

            // check if we have a valid input stream
            if (stream != NULL)
            {
                // load the pixbuf from the stream
                icon = gdk_pixbuf_new_from_stream_at_scale(stream, icon_size,
                        icon_size, TRUE,
                        NULL, NULL);

                // destroy the stream
                g_object_unref(stream);
            }
        }

        // return the icon if we have one
        if (icon != NULL)
            return icon;
    }
    else
    {
        /* we have no preview icon but the thumbnail should be ready. determine
         * the filename of the thumbnail */
        thumbnail_path = thunar_file_get_thumbnail_path(file, factory->thumbnail_size);

        // check if we have a valid path
        if (thumbnail_path != NULL)
        {
            // try to load the thumbnail
            icon = thunar_icon_factory_load_from_file(factory, thumbnail_path, icon_size);
        }
    }
}
#endif


