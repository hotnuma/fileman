
// ----------------------------------------------------------------------------

#if 0


static void print_monitor_event(GFileMonitor *monitor,
                                GFile *event_file, GFile *other_file,
                                GFileMonitorEvent event_type,
                                gpointer user_data)
{
    (void) monitor;
    (void) user_data;

    switch (event_type)
    {
    case G_FILE_MONITOR_EVENT_CHANGED:
        printf("G_FILE_MONITOR_EVENT_CHANGED\n");
        break;

    case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        printf("G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT\n");
        break;

    case G_FILE_MONITOR_EVENT_DELETED:
        printf("G_FILE_MONITOR_EVENT_DELETED\n");
        break;

    case G_FILE_MONITOR_EVENT_CREATED:
        printf("G_FILE_MONITOR_EVENT_CREATED\n");
        break;

    case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        printf("G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED\n");
        break;

    case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        printf("G_FILE_MONITOR_EVENT_PRE_UNMOUNT\n");
        break;

    case G_FILE_MONITOR_EVENT_UNMOUNTED:
        printf("G_FILE_MONITOR_EVENT_UNMOUNTED\n");
        break;

    case G_FILE_MONITOR_EVENT_MOVED:
        printf("G_FILE_MONITOR_EVENT_MOVED\n");
        break;

    case G_FILE_MONITOR_EVENT_RENAMED:
        printf("G_FILE_MONITOR_EVENT_RENAMED\n");
        break;

    case G_FILE_MONITOR_EVENT_MOVED_IN:
        printf("G_FILE_MONITOR_EVENT_MOVED_IN\n");
        break;

    case G_FILE_MONITOR_EVENT_MOVED_OUT:
        printf("G_FILE_MONITOR_EVENT_MOVED_OUT\n");
        break;

    default:
        printf("INVALID\n");
        break;
    }

    if (g_file_monitor_is_cancelled(monitor))
    {
        printf("monitor_is_cancelled\n");
    }

    if (event_file)
    {
        gchar *file_uri = g_file_get_uri(event_file);
        printf("event_file : %s\n", file_uri);
        g_free(file_uri);
    }

    if (other_file)
    {
        gchar *file_uri = g_file_get_uri(other_file);
        printf("other_file : %s\n", file_uri);
        g_free(file_uri);
    }
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

// ----------------------------------------------------------------------------

#if 0

#define FLAG_SET_THUMB_STATE(file, new_state) \
    G_STMT_START{ \
    (file)->flags =((file)->flags & ~THUNARFILE_FLAG_THUMB_MASK) |(new_state); \
    }G_STMT_END

static inline gboolean thumbnail_needs_frame(const GdkPixbuf *thumbnail,
                                             gint             width,
                                             gint             height,
                                             gint             size)
{
    const guchar *pixels;
    gint          rowstride;
    gint          n;

    // don't add frames to small thumbnails
    if (size < THUNAR_ICON_SIZE_64 )
        return FALSE;

    // always add a frame to thumbnails w/o alpha channel
    if (G_LIKELY(!gdk_pixbuf_get_has_alpha(thumbnail)))
        return TRUE;

    // get a pointer to the thumbnail data
    pixels = gdk_pixbuf_get_pixels(thumbnail);

    // check if we have a transparent pixel on the first row
    for(n = width * 4; n > 0; n -= 4)
        if (pixels[n - 1] < 255u)
            return FALSE;

    // determine the rowstride
    rowstride = gdk_pixbuf_get_rowstride(thumbnail);

    // skip the first row
    pixels += rowstride;

    // check if we have a transparent pixel in the first or last column
    for(n = height - 2; n > 0; --n, pixels += rowstride)
        if (pixels[3] < 255u || pixels[width * 4 - 1] < 255u)
            return FALSE;

    // check if we have a transparent pixel on the last row
    for(n = width * 4; n > 0; n -= 4)
        if (pixels[n - 1] < 255u)
            return FALSE;

    return TRUE;
}

gboolean thunar_icon_factory_get_show_thumbnail(const IconFactory *factory,
                                                const ThunarFile *file);

static GdkPixbuf* thunar_icon_factory_get_thumbnail_frame()
{
    GInputStream *stream;
    static GdkPixbuf *frame = NULL;

    if (G_LIKELY(frame != NULL))
        return frame;

    stream = g_resources_open_stream("/org/xfce/thunar/thumbnail-frame.png", 0, NULL);
    if (G_UNLIKELY(stream != NULL))
    {
        frame = gdk_pixbuf_new_from_stream(stream, NULL, NULL);
        g_object_unref(stream);
    }

    return frame;
}

gboolean thunar_icon_factory_get_show_thumbnail(const IconFactory *factory,
                                                const ThunarFile        *file)
{
    GFilesystemPreviewType preview;

    e_return_val_if_fail(IS_ICONFACTORY(factory), THUNAR_THUMBNAIL_MODE_NEVER);
    e_return_val_if_fail(file == NULL || IS_THUNARFILE(file), THUNAR_THUMBNAIL_MODE_NEVER);

    if (file == NULL
            || factory->thumbnail_mode == THUNAR_THUMBNAIL_MODE_NEVER)
        return FALSE;

    // always create thumbs for local files
    if (thunar_file_is_local(file))
        return TRUE;

    preview = thunar_file_get_preview_type(file);

    // file system says to never thumbnail anything
    if (preview == G_FILESYSTEM_PREVIEW_TYPE_NEVER)
        return FALSE;

    // only if the setting is local and the fs reports to be local
    if (factory->thumbnail_mode == THUNAR_THUMBNAIL_MODE_ONLY_LOCAL)
        return preview == G_FILESYSTEM_PREVIEW_TYPE_IF_LOCAL;

    // THUNAR_THUMBNAIL_MODE_ALWAYS
    return TRUE;
}
#endif



