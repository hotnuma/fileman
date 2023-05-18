
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



