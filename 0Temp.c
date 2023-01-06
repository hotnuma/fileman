
#if 0

GdkScreen* thunar_gdk_screen_open(const gchar *display_name,
                                  GError **error);
/*
 * thunar_gdk_screen_open:
 * @display_name : a fully qualified display name.
 * @error        : return location for errors or %NULL.
 *
 * Opens the screen referenced by the @display_name. Returns a
 * reference on the #GdkScreen for @display_name or %NULL if
 * @display_name couldn't be opened.
 *
 * If @display_name is the empty string "", a reference on the
 * default screen will be returned.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the #GdkScreen for @display_name or %NULL if
 *               @display_name could not be opened.
 */
GdkScreen* thunar_gdk_screen_open(const gchar *display_name,
                                  GError     **error)
{
    const gchar *other_name;
    GdkDisplay  *display = NULL;
    GdkScreen   *screen = NULL;
    GSList      *displays;
    GSList      *dp;

    thunar_return_val_if_fail(display_name != NULL, NULL);
    thunar_return_val_if_fail(error == NULL || *error == NULL, NULL);

    /* check if the default screen should be opened */
    if (G_UNLIKELY(*display_name == '\0'))
        return g_object_ref(gdk_screen_get_default());

    /* check if we already have that display */
    displays = gdk_display_manager_list_displays(gdk_display_manager_get());
    for(dp = displays; dp != NULL; dp = dp->next)
    {
        other_name = gdk_display_get_name(dp->data);
        if (strncmp(other_name, display_name, strlen(display_name)) == 0)
        {
            display = dp->data;
            break;
        }
        /* This second comparison will as well match the short notation, ":0" with the long notation ":0.0" */
        if (strncmp(other_name, display_name, strlen(other_name)) == 0)
        {
            display = dp->data;
            break;
        }
    }
    g_slist_free(displays);

    /* try to open the display if not already done */
    if (display == NULL)
        display = gdk_display_open(display_name);

    /* try to locate the screen on the display */
    if (display != NULL)
    {
        screen = gdk_display_get_default_screen(display);

        if (screen != NULL)
            g_object_ref(G_OBJECT(screen));
    }

    /* check if we were successfull here */
    if (G_UNLIKELY(screen == NULL))
        g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "Failed to open display \"%s\"", display_name);

    return screen;
}

// ----------------------------------------------------------------------------

GdkPixbuf *egdk_pixbuf_lucent(const GdkPixbuf *source, guint percent)
                              G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf *egdk_pixbuf_scale_ratio(GdkPixbuf *source, gint dest_size)
                                   G_GNUC_WARN_UNUSED_RESULT;

/*
 * egdk_pixbuf_lucent:
 * @source  : the source #GdkPixbuf.
 * @percent : the percentage of translucency.
 *
 * Returns a version of @source, whose pixels translucency is
 * @percent of the original @source pixels.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Returns: a translucent version of @source.
 *
 * Since: 0.3.1.3
 */

GdkPixbuf*
egdk_pixbuf_lucent (const GdkPixbuf *source,
                       guint            percent)
{
  GdkPixbuf *dst;
  guchar    *dst_pixels;
  guchar    *src_pixels;
  guchar    *pixdst;
  guchar    *pixsrc;
  gint       dst_row_stride;
  gint       src_row_stride;
  gint       width;
  gint       height;
  gint       i, j;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail ((gint) percent >= 0 && percent <= 100, NULL);

  /* determine source parameters */
  width = gdk_pixbuf_get_width (source);
  height = gdk_pixbuf_get_height (source);

  /* allocate the destination pixbuf */
  dst = gdk_pixbuf_new (gdk_pixbuf_get_colorspace (source), TRUE, gdk_pixbuf_get_bits_per_sample (source), width, height);

  /* determine row strides on src/dst */
  dst_row_stride = gdk_pixbuf_get_rowstride (dst);
  src_row_stride = gdk_pixbuf_get_rowstride (source);

  /* determine pixels on src/dst */
  dst_pixels = gdk_pixbuf_get_pixels (dst);
  src_pixels = gdk_pixbuf_get_pixels (source);

  /* check if the source already contains an alpha channel */
  if (G_LIKELY (gdk_pixbuf_get_has_alpha (source)))
    {
      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; --j >= 0; )
            {
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = ((guint) *pixsrc++ * percent) / 100u;
            }
        }
    }
  else
    {
      /* pre-calculate the alpha value */
      percent = (255u * percent) / 100u;

      for (i = height; --i >= 0; )
        {
          pixdst = dst_pixels + i * dst_row_stride;
          pixsrc = src_pixels + i * src_row_stride;

          for (j = width; --j >= 0; )
            {
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = *pixsrc++;
              *pixdst++ = percent;
            }
        }
    }

  return dst;
}

/*
 * egdk_pixbuf_scale_ratio:
 * @source    : The source #GdkPixbuf.
 * @dest_size : The target size in pixel.
 *
 * Scales @source to @dest_size while preserving the aspect ratio of
 * @source.
 *
 * Returns: A newly created #GdkPixbuf.
 */

GdkPixbuf*
egdk_pixbuf_scale_ratio (GdkPixbuf *source,
                            gint       dest_size)
{
  gdouble wratio;
  gdouble hratio;
  gint    source_width;
  gint    source_height;
  gint    dest_width;
  gint    dest_height;

  g_return_val_if_fail (GDK_IS_PIXBUF (source), NULL);
  g_return_val_if_fail (dest_size > 0, NULL);

  source_width  = gdk_pixbuf_get_width  (source);
  source_height = gdk_pixbuf_get_height (source);

  wratio = (gdouble) source_width  / (gdouble) dest_size;
  hratio = (gdouble) source_height / (gdouble) dest_size;

  if (hratio > wratio)
    {
      dest_width  = rint (source_width / hratio);
      dest_height = dest_size;
    }
  else
    {
      dest_width  = dest_size;
      dest_height = rint (source_height / wratio);
    }

  return gdk_pixbuf_scale_simple (source, MAX (dest_width, 1), MAX (dest_height, 1), GDK_INTERP_BILINEAR);
}

// ----------------------------------------------------------------------------

GFile*      eg_file_new_for_desktop();
GFile*      thunar_g_file_new_for_computer();
GFile*      thunar_g_file_new_for_bookmarks();
gboolean    eg_file_write_key_file(GFile *file,
                                         GKeyFile *key_file,
                                         GCancellable *cancellable,
                                         GError **error);
gboolean    eg_vfs_metadata_is_supported();

GFile* eg_file_new_for_desktop()
{
    return g_file_new_for_path(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
}

GFile* thunar_g_file_new_for_bookmarks()
{
    gchar *filename;
    GFile *bookmarks;

    filename = g_build_filename(g_get_user_config_dir(), "gtk-3.0", "bookmarks", NULL);
    bookmarks = g_file_new_for_path(filename);
    g_free(filename);

    return bookmarks;
}

gboolean eg_file_write_key_file(GFile         *file,
                                      GKeyFile      *key_file,
                                      GCancellable  *cancellable,
                                      GError        **error)
{
    gchar    *contents;
    gsize     length;
    gboolean  result = TRUE;

    thunar_return_val_if_fail(G_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(key_file != NULL, FALSE);
    thunar_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    /* write the key file into the contents buffer */
    contents = g_key_file_to_data(key_file, &length, NULL);

    /* try to replace the file contents with the key file data */
    if (contents != NULL)
    {
        result = g_file_replace_contents(file, contents, length, NULL, FALSE,
                                          G_FILE_CREATE_NONE,
                                          NULL, cancellable, error);

        /* cleanup */
        g_free(contents);
    }

    return result;
}

gboolean eg_vfs_metadata_is_supported()
{
    GFile                  *root;
    GFileAttributeInfoList *attr_info_list;
    gint                    n;
    gboolean                metadata_is_supported = FALSE;

    /* get a GFile for the root directory, and obtain the list of writable namespaces for it */
    root = eg_file_new_for_root();
    attr_info_list = g_file_query_writable_namespaces(root, NULL, NULL);
    g_object_unref(root);

    /* loop through the returned namespace names and check if "metadata" is included */
    for(n = 0; n < attr_info_list->n_infos; n++)
    {
        if (g_strcmp0(attr_info_list->infos[n].name, "metadata") == 0)
        {
            metadata_is_supported = TRUE;
            break;
        }
    }

    /* release the attribute info list */
    g_file_attribute_info_list_unref(attr_info_list);

    return metadata_is_supported;
}

// ----------------------------------------------------------------------------

// marshal.list
VOID:STRING,STRING
VOID:UINT,BOXED,UINT,STRING
VOID:UINT,BOXED

// ----------------------------------------------------------------------------

GtkWidget* menu_get_launcher(ThunarMenu *menu)
{
    thunar_return_val_if_fail(THUNAR_IS_MENU(menu), NULL);

    return GTK_WIDGET(menu->launcher);
}

// ----------------------------------------------------------------------------

gchar*      th_file_get_size_string(const ThunarFile *file)
                                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gboolean    th_file_is_home(const ThunarFile *file);
gboolean    th_file_set_custom_icon(ThunarFile *file,
                                        const gchar *custom_icon,
                                        GError **error);
GIcon*      th_file_get_preview_icon(const ThunarFile *file);
GFilesystemPreviewType th_file_get_preview_type(const ThunarFile *file);

void        th_file_reload_idle(ThunarFile *file);
gboolean    th_file_is_desktop(const ThunarFile *file);

gchar* th_file_get_size_string(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return g_format_size(th_file_get_size(file));
}

gboolean th_file_is_home(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return thunar_g_file_is_home(file->gfile);
}

gboolean th_file_set_custom_icon(ThunarFile  *file,
                                     const gchar *custom_icon,
                                     GError     **error)
{
    GKeyFile *key_file;

    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(custom_icon != NULL, FALSE);

    key_file = thunar_g_file_query_key_file(file->gfile, NULL, error);

    if (key_file == NULL)
        return FALSE;

    g_key_file_set_string(key_file, G_KEY_FILE_DESKTOP_GROUP,
                           G_KEY_FILE_DESKTOP_KEY_ICON, custom_icon);

    if (thunar_g_file_write_key_file(file->gfile, key_file, NULL, error))
    {
        /* tell everybody that we have changed */
        th_file_changed(file);

        g_key_file_free(key_file);
        return TRUE;
    }
    else
    {
        g_key_file_free(key_file);
        return FALSE;
    }
}

gboolean th_file_is_desktop(const ThunarFile *file)
{
    GFile   *desktop;
    gboolean is_desktop = FALSE;

    desktop = g_file_new_for_path(g_get_user_special_dir(G_USER_DIRECTORY_DESKTOP));
    is_desktop = g_file_equal(file->gfile, desktop);
    g_object_unref(desktop);

    return is_desktop;
}

GIcon* th_file_get_preview_icon(const ThunarFile *file)
{
    GObject *icon;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(G_IS_FILE_INFO(file->info), NULL);

    icon = g_file_info_get_attribute_object(file->info, G_FILE_ATTRIBUTE_PREVIEW_ICON);
    if (G_LIKELY(icon != NULL))
        return G_ICON(icon);

    return NULL;
}

GFilesystemPreviewType th_file_get_preview_type(const ThunarFile *file)
{
    GFilesystemPreviewType  preview;
    GFileInfo               *info;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), G_FILESYSTEM_PREVIEW_TYPE_NEVER);
    thunar_return_val_if_fail(G_IS_FILE(file->gfile), G_FILESYSTEM_PREVIEW_TYPE_NEVER);

    info = g_file_query_filesystem_info(file->gfile, G_FILE_ATTRIBUTE_FILESYSTEM_USE_PREVIEW, NULL, NULL);
    if (G_LIKELY(info != NULL))
    {
        preview = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_FILESYSTEM_USE_PREVIEW);
        g_object_unref(G_OBJECT(info));
    }
    else
    {
        /* assume we don't know */
        preview = G_FILESYSTEM_PREVIEW_TYPE_NEVER;
    }

    return preview;
}

void th_file_reload_idle(ThunarFile *file)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    g_idle_add((GSourceFunc) th_file_reload, file);
}


//-----------------------------------------------------------------------------

void        thunar_file_set_thumb_state(ThunarFile *file,
                                        ThunarFileThumbState state);
void thunar_file_set_thumb_state(ThunarFile *file,
                                 ThunarFileThumbState state)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    /* check if the state changes */
    if (thunar_file_get_thumb_state(file) == state)
        return;

    /* set the new thumbnail state */
    FLAG_SET_THUMB_STATE(file, state);

    /* remove path if the type is not supported */
    if (state == THUNAR_FILE_THUMB_STATE_NONE
            && file->thumbnail_path != NULL)
    {
        g_free(file->thumbnail_path);
        file->thumbnail_path = NULL;
    }

    /* if the file has a thumbnail, reload it */
    if (state == THUNAR_FILE_THUMB_STATE_READY)
        thunar_file_monitor_file_changed(file);
}


//-----------------------------------------------------------------------------

// Public
GList*      thunar_file_get_emblem_names(ThunarFile *file);
void        thunar_file_set_emblem_names(ThunarFile *file, GList *emblem_names);
const gchar* thunar_file_get_thumbnail_path(ThunarFile *file,
                                            ThunarThumbnailSize thumbnail_size);
const gchar* thunar_file_get_metadata_setting(ThunarFile *file,
                                              const gchar *setting_name);
void        thunar_file_set_metadata_setting(ThunarFile *file,
                                             const gchar *setting_name,
                                             const gchar *setting_value);
void        thunar_file_clear_directory_specific_settings(ThunarFile *file);
gboolean    thunar_file_has_directory_specific_settings(ThunarFile *file);

// Implementation
static gboolean thunar_file_denies_access_permission(
                                                const ThunarFile *file,
                                                ThunarFileMode usr_permissions,
                                                ThunarFileMode grp_permissions,
                                                ThunarFileMode oth_permissions);
static gboolean thunar_file_is_readable(const ThunarFile *file);

static gboolean thunar_file_denies_access_permission(const ThunarFile *file,
                                                     ThunarFileMode usr_permissions,
                                                     ThunarFileMode grp_permissions,
                                                     ThunarFileMode oth_permissions)
{
    ThunarFileMode mode;
    ThunarGroup   *group;
    ThunarUser    *user;
    gboolean       result;
    GList         *groups;
    GList         *lp;

    /* query the file mode */
    mode = thunar_file_get_mode(file);

    /* query the owner of the file, if we cannot determine
     * the owner, we can't tell if we're denied to access
     * the file, so we simply return FALSE then.
     */
    user = thunar_file_get_user(file);
    if (G_UNLIKELY(user == NULL))
        return FALSE;

    /* root is allowed to do everything */
    if (G_UNLIKELY(effective_user_id == 0))
        return FALSE;

    if (thunar_user_is_me(user))
    {
        /* we're the owner, so the usr permissions must be granted */
        result =((mode & usr_permissions) == 0);

        /* release the user */
        g_object_unref(G_OBJECT(user));
    }
    else
    {
        group = thunar_file_get_group(file);
        if (G_LIKELY(group != NULL))
        {
            /* release the file owner */
            g_object_unref(G_OBJECT(user));

            /* determine the effective user */
            user = thunar_user_manager_get_user_by_id(user_manager, effective_user_id);
            if (G_LIKELY(user != NULL))
            {
                /* check the group permissions */
                groups = thunar_user_get_groups(user);
                for(lp = groups; lp != NULL; lp = lp->next)
                    if (THUNAR_GROUP(lp->data) == group)
                    {
                        g_object_unref(G_OBJECT(user));
                        g_object_unref(G_OBJECT(group));
                        return((mode & grp_permissions) == 0);
                    }

                /* release the effective user */
                g_object_unref(G_OBJECT(user));
            }

            /* release the file group */
            g_object_unref(G_OBJECT(group));
        }

        /* check other permissions */
        result =((mode & oth_permissions) == 0);
    }

    return result;
}

static void thunar_file_set_emblem_names_ready(GObject      *source_object,
                                               GAsyncResult *result,
                                               gpointer     user_data)
{
    ThunarFile *file = THUNAR_FILE(user_data);
    GError     *error = NULL;

    if (!g_file_set_attributes_finish(G_FILE(source_object),
                                      result, NULL, &error))
    {
        g_warning("Failed to set metadata: %s", error->message);
        g_error_free(error);

        g_file_info_remove_attribute(file->info, "metadata::emblems");
    }

    th_file_changed(file);
}

static gboolean thunar_file_is_readable(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    if (!g_file_info_has_attribute(file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_READ))
        return TRUE;

    return g_file_info_get_attribute_boolean(file->info,
                                        G_FILE_ATTRIBUTE_ACCESS_CAN_READ);
}

GList* thunar_file_get_emblem_names(ThunarFile *file)
{
    guint32   uid;
    gchar   **emblem_names;
    GList    *emblems = NULL;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    /* leave if there is no info */
    if (file->info == NULL)
        return NULL;

    /* determine the custom emblems */
    emblem_names = g_file_info_get_attribute_stringv(file->info, "metadata::emblems");
    if (G_UNLIKELY(emblem_names != NULL))
    {
        for(; *emblem_names != NULL; ++emblem_names)
            emblems = g_list_append(emblems, *emblem_names);
    }

    if (thunar_file_is_symlink(file))
        emblems = g_list_prepend(emblems, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK);

    /* determine the user ID of the file owner */
    /* TODO what are we going to do here on non-UNIX systems? */
    uid = file->info != NULL
          ? g_file_info_get_attribute_uint32(file->info, G_FILE_ATTRIBUTE_UNIX_UID)
          : 0;

    /* we add "cant-read" if either(a) the file is not readable or(b) a directory, that lacks the
     * x-bit, see https://bugzilla.xfce.org/show_bug.cgi?id=1408 for the details about this change.
     */
    if (!thunar_file_is_readable(file)
            ||(thunar_file_is_directory(file)
                && thunar_file_denies_access_permission(file, THUNAR_FILE_MODE_USR_EXEC,
                        THUNAR_FILE_MODE_GRP_EXEC,
                        THUNAR_FILE_MODE_OTH_EXEC)))
    {
        emblems = g_list_prepend(emblems, THUNAR_FILE_EMBLEM_NAME_CANT_READ);
    }
    else if (G_UNLIKELY(uid == effective_user_id && !thunar_file_is_writable(file) && !thunar_file_is_trashed(file)))
    {
        /* we own the file, but we cannot write to it, that's why we mark it as "cant-write", so
         * users won't be surprised when opening the file in a text editor, but are unable to save.
         */
        emblems = g_list_prepend(emblems, THUNAR_FILE_EMBLEM_NAME_CANT_WRITE);
    }

    return emblems;
}

void thunar_file_set_emblem_names(ThunarFile *file,
                                  GList      *emblem_names)
{
    GList      *lp;
    gchar     **emblems = NULL;
    gint        n;
    GFileInfo  *info;

    thunar_return_if_fail(THUNAR_IS_FILE(file));
    thunar_return_if_fail(G_IS_FILE_INFO(file->info));

    /* allocate a zero-terminated array for the emblem names */
    emblems = g_new0(gchar *, g_list_length(emblem_names) + 1);

    /* turn the emblem_names list into a zero terminated array */
    for(lp = emblem_names, n = 0; lp != NULL; lp = lp->next)
    {
        /* skip special emblems */
        if (strcmp(lp->data, THUNAR_FILE_EMBLEM_NAME_SYMBOLIC_LINK) == 0
                || strcmp(lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_READ) == 0
                || strcmp(lp->data, THUNAR_FILE_EMBLEM_NAME_CANT_WRITE) == 0
                || strcmp(lp->data, THUNAR_FILE_EMBLEM_NAME_DESKTOP) == 0)
            continue;

        /* add the emblem to our list */
        emblems[n++] = g_strdup(lp->data);
    }

    /* set the value in the current info. this call is needed to update the in-memory
     * GFileInfo structure to ensure that the new attribute value is available immediately */
    if (n == 0)
        g_file_info_remove_attribute(file->info, "metadata::emblems");
    else
        g_file_info_set_attribute_stringv(file->info, "metadata::emblems", emblems);

    /* send meta data to the daemon. this call is needed to store the new value of
     * the attribute in the file system */
    info = g_file_info_new();
    g_file_info_set_attribute_stringv(info, "metadata::emblems", emblems);
    g_file_set_attributes_async(file->gfile, info,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 thunar_file_set_emblem_names_ready,
                                 file);
    g_object_unref(G_OBJECT(info));

    g_strfreev(emblems);
}

const gchar* thunar_file_get_metadata_setting(ThunarFile  *file,
                                              const gchar *setting_name)
{
    gchar       *attr_name;
    const gchar *attr_value;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    if (file->info == NULL)
        return NULL;

    /* convert the setting name to an attribute name */
    attr_name = g_strdup_printf("metadata::thunar-%s", setting_name);

    if (!g_file_info_has_attribute(file->info, attr_name))
    {
        g_free(attr_name);
        return NULL;
    }

    attr_value = g_file_info_get_attribute_string(file->info, attr_name);
    g_free(attr_name);

    return attr_value;
}

static void thunar_file_set_metadata_setting_finish(GObject      *source_object,
                                                    GAsyncResult *result,
                                                    gpointer      user_data)
{
    ThunarFile *file = THUNAR_FILE(user_data);
    GError     *error = NULL;

    if (!g_file_set_attributes_finish(G_FILE(source_object), result, NULL, &error))
    {
        g_warning("Failed to set metadata: %s", error->message);
        g_error_free(error);
    }

    th_file_changed(file);
}

void thunar_file_set_metadata_setting(ThunarFile  *file,
                                      const gchar *setting_name,
                                      const gchar *setting_value)
{
    GFileInfo *info;
    gchar     *attr_name;

    thunar_return_if_fail(THUNAR_IS_FILE(file));
    thunar_return_if_fail(G_IS_FILE_INFO(file->info));

    /* convert the setting name to an attribute name */
    attr_name = g_strdup_printf("metadata::thunar-%s", setting_name);

    /* set the value in the current info. this call is needed to update the in-memory
     * GFileInfo structure to ensure that the new attribute value is available immediately */
    g_file_info_set_attribute_string(file->info, attr_name, setting_value);

    /* send meta data to the daemon. this call is needed to store the new value of
     * the attribute in the file system */
    info = g_file_info_new();
    g_file_info_set_attribute_string(info, attr_name, setting_value);
    g_file_set_attributes_async(file->gfile, info,
                                 G_FILE_QUERY_INFO_NONE,
                                 G_PRIORITY_DEFAULT,
                                 NULL,
                                 thunar_file_set_metadata_setting_finish,
                                 file);
    g_free(attr_name);
    g_object_unref(G_OBJECT(info));
}

void thunar_file_clear_directory_specific_settings(ThunarFile *file)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    if (file->info == NULL)
        return;

    g_file_info_remove_attribute(file->info, "metadata::thunar-view-type");
    g_file_info_remove_attribute(file->info, "metadata::thunar-sort-column");
    g_file_info_remove_attribute(file->info, "metadata::thunar-sort-order");

    g_file_set_attribute(file->gfile, "metadata::thunar-view-type", G_FILE_ATTRIBUTE_TYPE_INVALID,
                          NULL, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    g_file_set_attribute(file->gfile, "metadata::thunar-sort-column", G_FILE_ATTRIBUTE_TYPE_INVALID,
                          NULL, G_FILE_QUERY_INFO_NONE, NULL, NULL);
    g_file_set_attribute(file->gfile, "metadata::thunar-sort-order", G_FILE_ATTRIBUTE_TYPE_INVALID,
                          NULL, G_FILE_QUERY_INFO_NONE, NULL, NULL);

    th_file_changed(file);
}

const gchar* thunar_file_get_thumbnail_path(ThunarFile *file,
                                            ThunarThumbnailSize thumbnail_size)
{
    GChecksum *checksum;
    gchar     *filename;
    gchar     *uri;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    /* if the thumbstate is known to be not there, return null */
    if (thunar_file_get_thumb_state(file) == THUNAR_FILE_THUMB_STATE_NONE)
        return NULL;

    if (G_UNLIKELY(file->thumbnail_path == NULL))
    {
        checksum = g_checksum_new(G_CHECKSUM_MD5);
        if (G_LIKELY(checksum != NULL))
        {
            uri = th_file_dup_uri(file);
            g_checksum_update(checksum,(const guchar *) uri, strlen(uri));
            g_free(uri);

            filename = g_strconcat(g_checksum_get_string(checksum), ".png", NULL);
            g_checksum_free(checksum);

            /* The thumbnail is in the format/location
             * $XDG_CACHE_HOME/thumbnails/(nromal|large)/MD5_Hash_Of_URI.png
             * for version 0.8.0 if XDG_CACHE_HOME is defined, otherwise
             * /homedir/.thumbnails/(normal|large)/MD5_Hash_Of_URI.png
             * will be used, which is also always used for versions prior
             * to 0.7.0.
             */

            /* build and check if the thumbnail is in the new location */
            file->thumbnail_path = g_build_path("/", g_get_user_cache_dir(),
                                                 "thumbnails", thunar_thumbnail_size_get_nick(thumbnail_size),
                                                 filename, NULL);

            if (!g_file_test(file->thumbnail_path, G_FILE_TEST_EXISTS))
            {
                /* Fallback to old version */
                g_free(file->thumbnail_path);

                file->thumbnail_path = g_build_filename(g_get_home_dir(),
                                       ".thumbnails", thunar_thumbnail_size_get_nick(thumbnail_size),
                                       filename, NULL);

                if (!g_file_test(file->thumbnail_path, G_FILE_TEST_EXISTS))
                {
                    /* Thumbnail doesn't exist in either spot */
                    g_free(file->thumbnail_path);
                    file->thumbnail_path = NULL;
                }
            }

            g_free(filename);
        }
    }

    return file->thumbnail_path;
}

gboolean thunar_file_has_directory_specific_settings(ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    if (g_file_info_has_attribute(file->info, "metadata::thunar-view-type"))
        return TRUE;

    if (g_file_info_has_attribute(file->info, "metadata::thunar-sort-column"))
        return TRUE;

    if (g_file_info_has_attribute(file->info, "metadata::thunar-sort-order"))
        return TRUE;

    return FALSE;
}


//-----------------------------------------------------------------------------

thunar_window_notebook_insert
thunar_window_notebook_page_added
thunar_window_notebook_show_tabs
thunar_window_notebook_switch_page
thunar_window_notebook_page_removed


//-----------------------------------------------------------------------------

void standard_view_open_on_middle_click(ThunarStandardView *standard_view,
                                        GtkTreePath        *tree_path,
                                        guint               event_state)
{
    UNUSED(event_state);

    GtkTreeIter     iter;
    ThunarFile     *file;
    GtkWidget      *window;
    ThunarLauncher *launcher;

    thunar_return_if_fail(THUNAR_IS_STANDARD_VIEW(standard_view));

    /* determine the file for the path */
    gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, tree_path);

    file = thunar_list_model_get_file(standard_view->model, &iter);
    if (G_LIKELY(file != NULL))
    {
        if (thunar_file_is_directory(file))
        {
            window = gtk_widget_get_toplevel(GTK_WIDGET(standard_view));
            launcher = thunar_window_get_launcher(THUNAR_WINDOW(window));
            thunar_launcher_open_selected_folders(launcher);
        }
        /* release the file reference */
        g_object_unref(G_OBJECT(file));
    }
}


//-----------------------------------------------------------------------------

static void thunar_file_move_thumbnail_cache_file(GFile *old_file,
                                                  GFile *new_file)
{
    ThunarApplication    *application;
    ThunarThumbnailCache *thumbnail_cache;

    thunar_return_if_fail(G_IS_FILE(old_file));
    thunar_return_if_fail(G_IS_FILE(new_file));

    application = thunar_application_get();
    thumbnail_cache = thunar_application_get_thumbnail_cache(application);
    thunar_thumbnail_cache_move_file(thumbnail_cache, old_file, new_file);

    g_object_unref(thumbnail_cache);
    g_object_unref(application);
}


//-----------------------------------------------------------------------------

gboolean thunar_launcher_append_custom_actions(ThunarLauncher *launcher,
                                               GtkMenuShell   *menu)
{
    gboolean                uca_added = FALSE;
    GtkWidget              *window;
    GtkWidget              *gtk_menu_item;
    ThunarxProviderFactory *provider_factory;
    GList                  *providers;
    GList                  *thunarx_menu_items = NULL;
    GList                  *lp_provider;
    GList                  *lp_item;

    thunar_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);
    thunar_return_val_if_fail(GTK_IS_MENU(menu), FALSE);

    /* determine the toplevel window we belong to */
    window = gtk_widget_get_toplevel(launcher->widget);

    /* load the menu providers from the provider factory */
    provider_factory = thunarx_provider_factory_get_default();
    providers = thunarx_provider_factory_list_providers(provider_factory, THUNARX_TYPE_MENU_PROVIDER);
    g_object_unref(provider_factory);

    if (G_UNLIKELY(providers == NULL))
        return FALSE;

    /* This may occur when the thunar-window is build */
    if (G_UNLIKELY(launcher->files_to_process == NULL))
        return FALSE;

    /* load the menu items offered by the menu providers */
    for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
        if (launcher->files_are_selected == FALSE)
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, THUNARX_FILE_INFO(launcher->current_directory));
        else
            thunarx_menu_items = thunarx_menu_provider_get_file_menu_items(lp_provider->data, window, launcher->files_to_process);

        for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
            gtk_menu_item = thunar_gtk_menu_thunarx_menu_item_new(lp_item->data, menu);

            /* Each thunarx_menu_item will be destroyed together with its related gtk_menu_item*/
            g_signal_connect_swapped(G_OBJECT(gtk_menu_item), "destroy", G_CALLBACK(g_object_unref), lp_item->data);
            uca_added = TRUE;
        }
        g_list_free(thunarx_menu_items);
    }

    g_list_free_full(providers, g_object_unref);

    return uca_added;
}

gboolean thunar_launcher_check_uca_key_activation(ThunarLauncher *launcher,
                                                  GdkEventKey    *key_event)
{
    GtkWidget              *window;
    ThunarxProviderFactory *provider_factory;
    GList                  *providers;
    GList                  *thunarx_menu_items = NULL;
    GList                  *lp_provider;
    GList                  *lp_item;
    GtkAccelKey             uca_key;
    gchar                  *name, *accel_path;
    gboolean                uca_activated = FALSE;

    /* determine the toplevel window we belong to */
    window = gtk_widget_get_toplevel(launcher->widget);

    /* load the menu providers from the provider factory */
    provider_factory = thunarx_provider_factory_get_default();
    providers = thunarx_provider_factory_list_providers(provider_factory, THUNARX_TYPE_MENU_PROVIDER);
    g_object_unref(provider_factory);

    if (G_UNLIKELY(providers == NULL))
        return uca_activated;

    /* load the menu items offered by the menu providers */
    for (lp_provider = providers; lp_provider != NULL; lp_provider = lp_provider->next)
    {
        if (launcher->files_are_selected == FALSE)
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, THUNARX_FILE_INFO(launcher->current_directory));
        else
            thunarx_menu_items = thunarx_menu_provider_get_file_menu_items(lp_provider->data, window, launcher->files_to_process);

        for (lp_item = thunarx_menu_items; lp_item != NULL; lp_item = lp_item->next)
        {
            g_object_get(G_OBJECT(lp_item->data), "name", &name, NULL);

            accel_path = g_strconcat("<Actions>/ThunarActions/", name, NULL);
            if (gtk_accel_map_lookup_entry(accel_path, &uca_key) == TRUE)
            {
                if (g_ascii_tolower(key_event->keyval) == g_ascii_tolower(uca_key.accel_key))
                {
                    if ((key_event->state & gtk_accelerator_get_default_mod_mask()) == uca_key.accel_mods)
                    {
                        thunarx_menu_item_activate(lp_item->data);
                        uca_activated = TRUE;
                    }
                }
            }
            g_free(name);
            g_free(accel_path);
            g_object_unref(lp_item->data);
        }
        g_list_free(thunarx_menu_items);
    }
    g_list_free_full(providers, g_object_unref);
    return uca_activated;
}


gboolean e_mkdirhier(const gchar *whole_path, gulong mode, GError **error)
{
  /* Stolen from FreeBSD's mkdir(1) with modifications to make it
   * work properly with NFS on Solaris */

    char path[1024];
  struct stat sb;
  mode_t numask, oumask;
  int first, last, sverrno;
  gboolean retval;
  char *p;

  g_return_val_if_fail (whole_path != NULL, FALSE);

  g_strlcpy (path, whole_path, sizeof (path));
  p = path;
  oumask = 0;
  retval = TRUE;

  if (p[0] == G_DIR_SEPARATOR) /* Skip leading '/'. */
    ++p;

  for (first = 1, last = 0; !last ; ++p)
    {
      if (p[0] == '\0')
        last = 1;
      else if (p[0] != G_DIR_SEPARATOR)
        continue;
      else if (p[1] == '\0')
        last = 1;

      *p = '\0';

      if (first)
        {
          /*
           * POSIX 1003.2:
           * For each dir operand that does not name an existing
           * directory, effects equivalent to those cased by the
           * following command shall occcur:
           *
           * mkdir -p -m $(umask -S),u+wx $(dirname dir) &&
           *    mkdir [-m mode] dir
           *
           * We change the user's umask and then restore it,
           * instead of doing chmod's.
           */
          oumask = umask(0);
          numask = oumask & ~(S_IWUSR | S_IXUSR);
          umask(numask);
          first = 0;
        }

      if (last)
        umask(oumask);

      if (mkdir (path, last ? mode : S_IRWXU | S_IRWXG | S_IRWXO) < 0)
        {
          sverrno = errno;

          if (stat (path, &sb) < 0)
            {
              errno = sverrno;
              retval = FALSE;
              break;
            }
          else if (!S_ISDIR (sb.st_mode))
            {
              errno = ENOTDIR;
              retval = FALSE;
              break;
            }
        }

      if (!last)
        *p = G_DIR_SEPARATOR;
    }

  if (!first && !last)
    umask (oumask);

  if (!retval && error != NULL)
    {
      /* be sure to initialize the i18n support */
      //_xfce_i18n_init ();

      g_set_error (error, G_FILE_ERROR,
                   g_file_error_from_errno (errno),
                   _("Error creating directory '%s': %s"),
                   whole_path, g_strerror (errno));
    }

  return retval;
}

#endif


