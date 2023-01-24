
#if 0

gboolean util_looks_like_an_uri(const gchar *string) G_GNUC_WARN_UNUSED_RESULT;

// deprecated replaced with g_uri_is_valid
gboolean    e_str_looks_like_an_uri(const gchar *str);

gboolean e_str_looks_like_an_uri(const gchar *str)
{
  const gchar *s = str;

  if (G_UNLIKELY (str == NULL))
    return FALSE;

  /* <scheme> starts with an alpha character */
  if (g_ascii_isalpha (*s))
    {
      /* <scheme> continues with (alpha | digit | "+" | "-" | ".")* */
      for (++s; g_ascii_isalnum (*s) || *s == '+' || *s == '-' || *s == '.'; ++s);

      /* <scheme> must be followed by ":" */
      return (*s == ':' && *(s+1) != '\0');
    }

  return FALSE;
}


//------------------------------------------------------------------------------

/**
 * thunar_gtk_menu_clean:
 * @menu : a #GtkMenu.
 *
 * Walks through the menu and all submenus and removes them,
 * so that the result will be a clean #GtkMenu without any items
 **/

void etk_menu_clean(GtkMenu *menu);

void etk_menu_clean(GtkMenu *menu)
{
    GList     *children, *lp;
    GtkWidget *submenu;

    children = gtk_container_get_children(GTK_CONTAINER(menu));
    for(lp = children; lp != NULL; lp = lp->next)
    {
        submenu = gtk_menu_item_get_submenu(lp->data);
        if (submenu != NULL)
            gtk_widget_destroy(submenu);
        gtk_container_remove(GTK_CONTAINER(menu), lp->data);
    }
    g_list_free(children);
}

/**
 * thunar_gtk_menu_thunarx_menu_item_new:
 * @thunarx_menu_item   : a #ThunarxMenuItem
 * @menu_to_append_item : #GtkMenuShell on which the item should be appended, or NULL
 *
 * method to create a #GtkMenuItem from a #ThunarxMenuItem and append it to the passed #GtkMenuShell
 * This method will as well add all sub-items in case the passed #ThunarxMenuItem is a submenu
 *
 * Return value:(transfer full): The new #GtkImageMenuItem.
 **/
GtkWidget* thunar_gtk_menu_thunarx_menu_item_new(
                                        GObject *thunarx_menu_item,
                                        GtkMenuShell *menu_to_append_item);

GtkWidget* thunar_gtk_menu_thunarx_menu_item_new(GObject      *thunarx_menu_item,
                                                 GtkMenuShell *menu_to_append_item)
{
    gchar        *name, *label_text, *tooltip_text, *icon_name, *accel_path;
    gboolean      sensitive;
    GtkWidget    *gtk_menu_item;
    ThunarxMenu  *thunarx_menu;
    GList        *children;
    GList        *lp;
    GtkWidget    *submenu;
    GtkWidget    *image;
    GIcon        *icon;

    g_return_val_if_fail(THUNARX_IS_MENU_ITEM(thunarx_menu_item), NULL);

    g_object_get(G_OBJECT(thunarx_menu_item),
                  "name", &name,
                  "label", &label_text,
                  "tooltip", &tooltip_text,
                  "icon", &icon_name,
                  "sensitive", &sensitive,
                  "menu", &thunarx_menu,
                  NULL);

    accel_path = g_strconcat("<Actions>/ThunarActions/", name, NULL);
    icon = g_icon_new_for_string(icon_name, NULL);
    image = gtk_image_new_from_gicon(icon,GTK_ICON_SIZE_MENU);
    gtk_menu_item = xfce_gtk_image_menu_item_new(label_text, tooltip_text, accel_path,
                    G_CALLBACK(thunarx_menu_item_activate),
                    G_OBJECT(thunarx_menu_item), image, menu_to_append_item);

    /* recursively add submenu items if any */
    if (gtk_menu_item != NULL && thunarx_menu != NULL)
    {
        children = thunarx_menu_get_items(thunarx_menu);
        submenu = gtk_menu_new();
        for(lp = children; lp != NULL; lp = lp->next)
            thunar_gtk_menu_thunarx_menu_item_new(lp->data, GTK_MENU_SHELL(submenu));
        gtk_menu_item_set_submenu(GTK_MENU_ITEM(gtk_menu_item), submenu);
        thunarx_menu_item_list_free(children);
    }
    g_free(name);
    g_free(accel_path);
    g_free(label_text);
    g_free(tooltip_text);
    g_free(icon_name);
    g_object_unref(icon);

    return gtk_menu_item;
}


// ----------------------------------------------------------------------------

#include <sys/types.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <mmintrin.h>
#include <stdlib.h>
#include <unistd.h>

/* required for GdkPixbufFormat */
#ifndef GDK_PIXBUF_ENABLE_BACKEND
#define GDK_PIXBUF_ENABLE_BACKEND
#endif

/* _O_BINARY is required on some platforms */
#ifndef _O_BINARY
#define _O_BINARY 0
#endif

#define g_open(path, mode, flags)(open((path),(mode),(flags)))

typedef struct
{
  gint     max_width;
  gint     max_height;
  gboolean preserve_aspect_ratio;

} SizePreparedInfo;

static void _size_prepared(GdkPixbufLoader *loader, gint width, gint height,
                           SizePreparedInfo *info);

/**
 * egdk_pixbuf_new_from_file_at_max_size:
 * @filename              : name of the file to load, in the GLib file
 *                          name encoding.
 * @max_width             : the maximum width of the loaded image.
 * @max_height            : the maximum height of the loaded image.
 * @preserve_aspect_ratio : %TRUE to preserve the image's aspect ratio
 *                          while scaling to fit into @max_width and @max_height.
 * @error                 : return location for errors or %NULL.
 *
 * Creates a new #GdkPixbuf by loading an image from the file at
 * @filename. The file format is detected automatically. If %NULL is
 * returned, then @error will be set. Possible errors are in the
 * #GDK_PIXBUF_ERROR and #G_FILE_ERROR domains. If the image dimensions
 * exceed @max_width or @max_height, the image will be scaled down to
 * fit into the dimensions, optionally preservingthe image's aspect
 * ratio. The image may still be larger, depending on the loader.
 *
 * The advantage of using this function over
 * gdk_pixbuf_new_from_file_at_scale() is that images will never be
 * scaled up, whichwould otherwise result in ugly images.
 *
 * Returns: a newly created #GdkPixbuf with a reference count or 1, or
 *          %NULL if any of several error conditions occurred: the file
 *          could not be opened, there was no loader for the file's format,
 *          there was not enough memory to allocate the buffer for the
 *          image, or the image file contained invalid data.
 *
 * Since: 0.3.1.9
 **/
GdkPixbuf* pixbuf_new_from_file_at_max_size(const gchar *filename,
                                                 gint        max_width,
                                                 gint        max_height,
                                                 gboolean    preserve_aspect_ratio,
                                                 GError      **error)
{
  SizePreparedInfo info;
  GdkPixbufLoader *loader;
  struct stat      statb;
  GdkPixbuf       *pixbuf;
  guchar          *buffer;
  gchar           *display_name;
  gint             sverrno;
  gint             fd;
  gint             n;

  g_return_val_if_fail(error == NULL || *error == NULL, NULL);
  g_return_val_if_fail(filename != NULL, NULL);
  g_return_val_if_fail(max_height > 0, NULL);
  g_return_val_if_fail(max_width > 0, NULL);

  /* try to open the file for reading */
  fd = g_open(filename, _O_BINARY | O_RDONLY, 0000);
  if(G_UNLIKELY(fd < 0))
    {
err0: /* remember the errno value */
      sverrno = errno;

err1: /* initialize the library's i18n support */
      //_exo_i18n_init();

      /* generate a useful error message */
      display_name = g_filename_display_name(filename);
      g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(sverrno), _("Failed to open file \"%s\": %s"), display_name, g_strerror(sverrno));
      g_free(display_name);

      if(fd >= 0)
        {
          close(fd);
        }

      return NULL;
    }

  /* try to stat the file */
  if(fstat(fd, &statb) < 0)
    goto err0;

  /* verify that we have a regular file here */
  if(!S_ISREG(statb.st_mode))
    {
      sverrno = EINVAL;
      goto err1;
    }

  /* setup the size-prepared info */
  info.max_width = max_width;
  info.max_height = max_height;
  info.preserve_aspect_ratio = preserve_aspect_ratio;

  /* allocate a new pixbuf loader */
  loader = gdk_pixbuf_loader_new();
  g_signal_connect(G_OBJECT(loader), "size-prepared", G_CALLBACK(_size_prepared), &info);

#ifdef HAVE_MMAP
  /* try to mmap() the file it's not too large */
  buffer = mmap(NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if(G_LIKELY(buffer != MAP_FAILED))
    {
      /* feed the data into the loader */
      if(!gdk_pixbuf_loader_write(loader, buffer, statb.st_size, error))
        {
          /* something went wrong */
          munmap(buffer, statb.st_size);
          goto err2;
        }

      /* unmap the file */
      munmap(buffer, statb.st_size);
    }
  else
#endif
    {
      /* allocate the read buffer */
      buffer = g_newa(guchar, 8192);

      /* read the file content */
      for(;;)
        {
          /* read the next chunk */
          n = read(fd, buffer, 8192);
          if(G_UNLIKELY(n < 0))
            {
              /* remember the errno value */
              sverrno = errno;

              /* initialize the library's i18n support */
              //_exo_i18n_init();

              /* generate a useful error message */
              display_name = g_filename_display_name(filename);
              g_set_error(error, G_FILE_ERROR, g_file_error_from_errno(sverrno), _("Failed to read file \"%s\": %s"), display_name, g_strerror(sverrno));
              g_free(display_name);

              /* close the loader and the file */
err2:         gdk_pixbuf_loader_close(loader, NULL);
              close(fd);
              goto err3;
            }
          else if(n == 0)
            {
              /* file read completely */
              break;
            }

          /* feed the data into the loader */
          if(!gdk_pixbuf_loader_write(loader, buffer, n, error))
            goto err2;
        }
    }

  /* close the file */
  close(fd);

  /* finalize the loader */
  if(!gdk_pixbuf_loader_close(loader, error))
    {
err3: /* we failed for some reason */
      g_object_unref(G_OBJECT(loader));
      return NULL;
    }

  /* check if we have a pixbuf now */
  pixbuf = gdk_pixbuf_loader_get_pixbuf(loader);
  if(G_UNLIKELY(pixbuf == NULL))
    {
      /* initialize the library's i18n support */
      //_exo_i18n_init();

      /* generate a useful error message */
      display_name = g_filename_display_name(filename);
      g_set_error(error, GDK_PIXBUF_ERROR, GDK_PIXBUF_ERROR_FAILED,
                   _("Failed to load image \"%s\": Unknown reason, probably a corrupt image file"),
                   display_name);
      g_free(display_name);
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref(G_OBJECT(pixbuf));
    }

  /* release the loader */
  g_object_unref(G_OBJECT(loader));

  return pixbuf;
}

static void _size_prepared(GdkPixbufLoader *loader, gint width, gint height,
                           SizePreparedInfo *info)
{
  gboolean scalable;
  gdouble  wratio;
  gdouble  hratio;

  /* check if the loader format is scalable */
  scalable =((gdk_pixbuf_loader_get_format(loader)->flags & GDK_PIXBUF_FORMAT_SCALABLE) != 0);

  /* check if we need to scale down(scalable formats are special) */
  if(scalable || width > info->max_width || height > info->max_height)
    {
      if(G_LIKELY(info->preserve_aspect_ratio))
        {
          /* calculate the new dimensions */
          wratio =(gdouble) width /(gdouble) info->max_width;
          hratio =(gdouble) height /(gdouble) info->max_height;

          if(hratio > wratio)
            {
              width = rint(width / hratio);
              height = info->max_height;
            }
          else
            {
              width = info->max_width;
              height = rint(height / wratio);
            }
        }
      else
        {
          /* just scale down to the dimension(scalable is special) */
          if(scalable || width > info->max_width)
            width = info->max_width;
          if(scalable || height > info->max_height)
            height = info->max_height;
        }
    }

  /* apply the new dimensions */
  gdk_pixbuf_loader_set_size(loader, MAX(width, 1), MAX(height, 1));
}

GdkPixbuf* pixbuf_new_from_file_at_max_size(
                                    const gchar *filename,
                                    gint        max_width,
                                    gint        max_height,
                                    gboolean    preserve_aspect_ratio,
                                    GError      **error)
                                    G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GdkPixbuf* pixbuf_frame(const GdkPixbuf *source, const GdkPixbuf *frame,
                        gint left_offset, gint top_offset,
                        gint right_offset, gint bottom_offset)
                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

static inline void _draw_frame_row(const GdkPixbuf *frame_image,
                                   gint            target_width,
                                   gint            source_width,
                                   gint            source_v_position,
                                   gint            dest_v_position,
                                   GdkPixbuf       *result_pixbuf,
                                   gint            left_offset,
                                   gint            height);

static inline void _draw_frame_column(const GdkPixbuf *frame_image,
                                      gint            target_height,
                                      gint            source_height,
                                      gint            source_h_position,
                                      gint            dest_h_position,
                                      GdkPixbuf       *result_pixbuf,
                                      gint            top_offset,
                                      gint            width);

GdkPixbuf* pixbuf_frame(const GdkPixbuf *source,
                             const GdkPixbuf *frame,
                             gint            left_offset,
                             gint            top_offset,
                             gint            right_offset,
                             gint            bottom_offset)
{
  // g_object_unref

    GdkPixbuf *dst;
  gint       dst_width;
  gint       dst_height;
  gint       frame_width;
  gint       frame_height;
  gint       src_width;
  gint       src_height;

  g_return_val_if_fail(GDK_IS_PIXBUF(frame), NULL);
  g_return_val_if_fail(GDK_IS_PIXBUF(source), NULL);

  src_width = gdk_pixbuf_get_width(source);
  src_height = gdk_pixbuf_get_height(source);

  frame_width = gdk_pixbuf_get_width(frame);
  frame_height = gdk_pixbuf_get_height(frame);

  dst_width = src_width + left_offset + right_offset;
  dst_height = src_height + top_offset + bottom_offset;

  /* allocate the resulting pixbuf */
  dst = gdk_pixbuf_new(GDK_COLORSPACE_RGB, TRUE, 8, dst_width, dst_height);

  /* fill the destination if the source has an alpha channel */
  if(G_UNLIKELY(gdk_pixbuf_get_has_alpha(source)))
    gdk_pixbuf_fill(dst, 0xffffffff);

  /* draw the left top cornder and top row */
  gdk_pixbuf_copy_area(frame, 0, 0, left_offset, top_offset, dst, 0, 0);
  _draw_frame_row(frame, src_width, frame_width - left_offset - right_offset, 0, 0, dst, left_offset, top_offset);

  /* draw the right top corner and left column */
  gdk_pixbuf_copy_area(frame, frame_width - right_offset, 0, right_offset, top_offset, dst, dst_width - right_offset, 0);
  _draw_frame_column(frame, src_height, frame_height - top_offset - bottom_offset, 0, 0, dst, top_offset, left_offset);

  /* draw the bottom right corner and bottom row */
  gdk_pixbuf_copy_area(frame, frame_width - right_offset, frame_height - bottom_offset, right_offset,
                        bottom_offset, dst, dst_width - right_offset, dst_height - bottom_offset);
  _draw_frame_row(frame, src_width, frame_width - left_offset - right_offset, frame_height - bottom_offset,
                  dst_height - bottom_offset, dst, left_offset, bottom_offset);

  /* draw the bottom left corner and the right column */
  gdk_pixbuf_copy_area(frame, 0, frame_height - bottom_offset, left_offset, bottom_offset, dst, 0, dst_height - bottom_offset);
  _draw_frame_column(frame, src_height, frame_height - top_offset - bottom_offset, frame_width - right_offset,
                     dst_width - right_offset, dst, top_offset, right_offset);

  /* copy the source pixbuf into the framed area */
  gdk_pixbuf_copy_area(source, 0, 0, src_width, src_height, dst, left_offset, top_offset);

  return dst;
}

static inline void _draw_frame_row(const GdkPixbuf *frame_image,
                                   gint            target_width,
                                   gint            source_width,
                                   gint            source_v_position,
                                   gint            dest_v_position,
                                   GdkPixbuf       *result_pixbuf,
                                   gint            left_offset,
                                   gint            height)
{
  gint remaining_width;
  gint slab_width;
  gint h_offset;

  for(h_offset = 0, remaining_width = target_width; remaining_width > 0; h_offset += slab_width, remaining_width -= slab_width)
    {
      slab_width =(remaining_width > source_width) ? source_width : remaining_width;
      gdk_pixbuf_copy_area(frame_image, left_offset, source_v_position, slab_width, height, result_pixbuf, left_offset + h_offset, dest_v_position);
    }
}

static inline void _draw_frame_column(const GdkPixbuf *frame_image,
                                      gint            target_height,
                                      gint            source_height,
                                      gint            source_h_position,
                                      gint            dest_h_position,
                                      GdkPixbuf       *result_pixbuf,
                                      gint            top_offset,
                                      gint            width)
{
  gint remaining_height;
  gint slab_height;
  gint v_offset;

  for(v_offset = 0, remaining_height = target_height; remaining_height > 0; v_offset += slab_height, remaining_height -= slab_height)
    {
      slab_height =(remaining_height > source_height) ? source_height : remaining_height;
      gdk_pixbuf_copy_area(frame_image, source_h_position, top_offset, width, slab_height, result_pixbuf, dest_h_position, top_offset + v_offset);
    }
}


// ----------------------------------------------------------------------------

static void thunar_column_model_notify_column_order(void *preferences,
                                                    GParamSpec *pspec,
                                                    ColumnModel *column_model);
static void thunar_column_model_notify_column_widths(void *preferences,
                                                     GParamSpec *pspec,
                                                     ColumnModel *column_model);
static void thunar_column_model_notify_visible_columns(void *preferences,
                                                       GParamSpec *pspec,
                                                       ColumnModel *column_model);
static void thunar_column_model_notify_column_order(/*ThunarPreferences*/ void *preferences,
                                                    GParamSpec        *pspec,
                                                    ColumnModel *column_model)
{
    UNUSED(preferences);
    UNUSED(pspec);
    GtkTreePath *path;
    GtkTreeIter  iter;
    gint         n;

    e_return_if_fail(IS_COLUMN_MODEL(column_model));
    //e_return_if_fail(THUNAR_IS_PREFERENCES(preferences));

    /* load the new column order */
    thunar_column_model_load_column_order(column_model);

    /* emit "row-changed" for all rows */
    for(n = 0; n < THUNAR_N_VISIBLE_COLUMNS; ++n)
    {
        path = gtk_tree_path_new_from_indices(n, -1);
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(column_model), &iter, path))
            gtk_tree_model_row_changed(GTK_TREE_MODEL(column_model), path, &iter);
        gtk_tree_path_free(path);
    }

    /* emit "columns-changed" */
    g_signal_emit(G_OBJECT(column_model), column_model_signals[COLUMNS_CHANGED], 0);
}

static void thunar_column_model_notify_column_widths(void *preferences,
                                                     GParamSpec        *pspec,
                                                     ColumnModel *column_model)
{
    UNUSED(preferences);
    UNUSED(pspec);
    e_return_if_fail(IS_COLUMN_MODEL(column_model));
    //e_return_if_fail(THUNAR_IS_PREFERENCES(preferences));

    /* load the new column widths */
    thunar_column_model_load_column_widths(column_model);
}

static void thunar_column_model_notify_visible_columns(void *preferences,
                                                       GParamSpec        *pspec,
                                                       ColumnModel *column_model)
{
    UNUSED(preferences);
    UNUSED(pspec);
    GtkTreePath *path;
    GtkTreeIter  iter;
    gint         n;

    e_return_if_fail(IS_COLUMN_MODEL(column_model));
    //e_return_if_fail(THUNAR_IS_PREFERENCES(preferences));

    /* load the new list of visible columns */
    thunar_column_model_load_visible_columns(column_model);

    /* emit "row-changed" for all rows */
    for(n = 0; n < THUNAR_N_VISIBLE_COLUMNS; ++n)
    {
        path = gtk_tree_path_new_from_indices(n, -1);
        if (gtk_tree_model_get_iter(GTK_TREE_MODEL(column_model), &iter, path))
            gtk_tree_model_row_changed(GTK_TREE_MODEL(column_model), path, &iter);
        gtk_tree_path_free(path);
    }

    /* emit "columns-changed" */
    g_signal_emit(G_OBJECT(column_model), column_model_signals[COLUMNS_CHANGED], 0);
}


// ----------------------------------------------------------------------------

static void thunar_properties_dialog_icon_button_clicked(
                                                GtkWidget *button,
                                                PropertiesDialog *dialog);

static void thunar_properties_dialog_icon_button_clicked(
                                                GtkWidget              *button,
                                                PropertiesDialog *dialog)
{
    GtkWidget   *chooser;
    GError      *err = NULL;
    const gchar *custom_icon;
    gchar       *title;
    gchar       *icon;
    ThunarFile  *file;

    e_return_if_fail(IS_PROPERTIES_DIALOG(dialog));
    e_return_if_fail(GTK_IS_BUTTON(button));
    e_return_if_fail(g_list_length(dialog->files) == 1);

    /* make sure we still have a file */
    if (G_UNLIKELY(dialog->files == NULL))
        return;

    file = THUNAR_FILE(dialog->files->data);

    /* allocate the icon chooser */
    title = g_strdup_printf(_("Select an Icon for \"%s\""), thunar_file_get_display_name(file));
    chooser = exo_icon_chooser_dialog_new(title, GTK_WINDOW(dialog),
                                           _("_Cancel"), GTK_RESPONSE_CANCEL,
                                           _("_OK"), GTK_RESPONSE_ACCEPT,
                                           NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(chooser), GTK_RESPONSE_ACCEPT);
    g_free(title);

    /* use the custom_icon of the file as default */
    custom_icon = thunar_file_get_custom_icon(file);
    if (G_LIKELY(custom_icon != NULL && *custom_icon != '\0'))
        exo_icon_chooser_dialog_set_icon(EXO_ICON_CHOOSER_DIALOG(chooser), custom_icon);

    /* run the icon chooser dialog and make sure the dialog still has a file */
    if (gtk_dialog_run(GTK_DIALOG(chooser)) == GTK_RESPONSE_ACCEPT && file != NULL)
    {
        /* determine the selected icon and use it for the file */
        icon = exo_icon_chooser_dialog_get_icon(EXO_ICON_CHOOSER_DIALOG(chooser));
        if (!thunar_file_set_custom_icon(file, icon, &err))
        {
            /* hide the icon chooser dialog first */
            gtk_widget_hide(chooser);

            /* tell the user that we failed to change the icon of the .desktop file */
            dialog_error(GTK_WIDGET(dialog), err,
                                       _("Failed to change icon of \"%s\""),
                                       thunar_file_get_display_name(file));
            g_error_free(err);
        }
        g_free(icon);
    }

    /* destroy the chooser */
    gtk_widget_destroy(chooser);
}

static void thunar_properties_dialog_update_providers(PropertiesDialog *dialog)
{
    GtkWidget *label_widget;
    GList     *pages = NULL;
    GList     *lp;

    GList     *providers;
    GList     *tmp;

    /* load the property page providers from the provider factory */
    providers = thunarx_provider_factory_list_providers(dialog->provider_factory, THUNARX_TYPE_PROPERTY_PAGE_PROVIDER);
    if (G_LIKELY(providers != NULL))
    {
        /* load the pages offered by the menu providers */
        for(lp = providers; lp != NULL; lp = lp->next)
        {
            //g_print("load pages\n");

            tmp = thunarx_property_page_provider_get_pages(lp->data, dialog->files);
            pages = g_list_concat(pages, tmp);
            g_object_unref(G_OBJECT(lp->data));
        }
        g_list_free(providers);
    }

    /* destroy any previous set pages */
    for(lp = dialog->provider_pages; lp != NULL; lp = lp->next)
    {
        gtk_widget_destroy(GTK_WIDGET(lp->data));
        g_object_unref(G_OBJECT(lp->data));
    }
    g_list_free(dialog->provider_pages);

    /* apply the new set of pages */
    dialog->provider_pages = pages;
    for(lp = pages; lp != NULL; lp = lp->next)
    {
        label_widget = thunarx_property_page_get_label_widget(THUNARX_PROPERTY_PAGE(lp->data));
        gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), GTK_WIDGET(lp->data), label_widget);
        g_object_ref(G_OBJECT(lp->data));
        gtk_widget_show(lp->data);
    }
}


// ----------------------------------------------------------------------------

gboolean treemodel_node_has_dummy(TreeModel *model, GNode *node);

gboolean treemodel_node_has_dummy(TreeModel *model,
                                          GNode           *node)
{
    e_return_val_if_fail(THUNAR_IS_TREE_MODEL(model), TRUE);
    return G_NODE_HAS_DUMMY(node);
}

// ----------------------------------------------------------------------------

struct _ThunarBookmark
{
    GFile *g_file;
    gchar *name;
};

typedef struct _ThunarBookmark ThunarBookmark;

static gboolean _window_check_uca_key_activation(ThunarWindow *window,
                                                       GdkEventKey  *key_event,
                                                       gpointer      user_data)
{
    UNUSED(user_data);

    if (launcher_check_uca_key_activation(window->launcher, key_event))
        return GDK_EVENT_STOP;

    return GDK_EVENT_PROPAGATE;
}

// ----------------------------------------------------------------------------

GList* thunarx_file_info_list_copy(GList *file_infos)
{
    return g_list_copy_deep(file_infos,(GCopyFunc)(void(*)(void)) g_object_ref, NULL);
}

void thunarx_file_info_list_free(GList *file_infos)
{
    g_list_free_full(file_infos, g_object_unref);
}

// ----------------------------------------------------------------------------

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

    e_return_val_if_fail(display_name != NULL, NULL);
    e_return_val_if_fail(error == NULL || *error == NULL, NULL);

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

    e_return_val_if_fail(G_IS_FILE(file), FALSE);
    e_return_val_if_fail(key_file != NULL, FALSE);
    e_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);

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
    e_return_val_if_fail(THUNAR_IS_MENU(menu), NULL);

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
    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return g_format_size(th_file_get_size(file));
}

gboolean th_file_is_home(const ThunarFile *file)
{
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return thunar_g_file_is_home(file->gfile);
}

gboolean th_file_set_custom_icon(ThunarFile  *file,
                                     const gchar *custom_icon,
                                     GError     **error)
{
    GKeyFile *key_file;

    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    e_return_val_if_fail(custom_icon != NULL, FALSE);

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

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    e_return_val_if_fail(G_IS_FILE_INFO(file->info), NULL);

    icon = g_file_info_get_attribute_object(file->info, G_FILE_ATTRIBUTE_PREVIEW_ICON);
    if (G_LIKELY(icon != NULL))
        return G_ICON(icon);

    return NULL;
}

GFilesystemPreviewType th_file_get_preview_type(const ThunarFile *file)
{
    GFilesystemPreviewType  preview;
    GFileInfo               *info;

    e_return_val_if_fail(THUNAR_IS_FILE(file), G_FILESYSTEM_PREVIEW_TYPE_NEVER);
    e_return_val_if_fail(G_IS_FILE(file->gfile), G_FILESYSTEM_PREVIEW_TYPE_NEVER);

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
    e_return_if_fail(THUNAR_IS_FILE(file));

    g_idle_add((GSourceFunc) th_file_reload, file);
}


//-----------------------------------------------------------------------------

void        thunar_file_set_thumb_state(ThunarFile *file,
                                        ThunarFileThumbState state);
void thunar_file_set_thumb_state(ThunarFile *file,
                                 ThunarFileThumbState state)
{
    e_return_if_fail(THUNAR_IS_FILE(file));

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
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

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

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

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

    e_return_if_fail(THUNAR_IS_FILE(file));
    e_return_if_fail(G_IS_FILE_INFO(file->info));

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

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

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

    e_return_if_fail(THUNAR_IS_FILE(file));
    e_return_if_fail(G_IS_FILE_INFO(file->info));

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
    e_return_if_fail(THUNAR_IS_FILE(file));

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

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

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
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

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

window_notebook_insert
window_notebook_page_added
window_notebook_show_tabs
window_notebook_switch_page
window_notebook_page_removed


//-----------------------------------------------------------------------------

void standard_view_open_on_middle_click(StandardView *standard_view,
                                        GtkTreePath        *tree_path,
                                        guint               event_state)
{
    UNUSED(event_state);

    GtkTreeIter     iter;
    ThunarFile     *file;
    GtkWidget      *window;
    ThunarLauncher *launcher;

    e_return_if_fail(IS_STANDARD_VIEW(standard_view));

    /* determine the file for the path */
    gtk_tree_model_get_iter(GTK_TREE_MODEL(standard_view->model), &iter, tree_path);

    file = thunar_list_model_get_file(standard_view->model, &iter);
    if (G_LIKELY(file != NULL))
    {
        if (thunar_file_is_directory(file))
        {
            window = gtk_widget_get_toplevel(GTK_WIDGET(standard_view));
            launcher = window_get_launcher(THUNAR_WINDOW(window));
            launcher_open_selected_folders(launcher);
        }
        /* release the file reference */
        g_object_unref(G_OBJECT(file));
    }
}


//-----------------------------------------------------------------------------

static void thunar_file_move_thumbnail_cache_file(GFile *old_file,
                                                  GFile *new_file)
{
    Application *application;
    ThunarThumbnailCache *thumbnail_cache;

    e_return_if_fail(G_IS_FILE(old_file));
    e_return_if_fail(G_IS_FILE(new_file));

    application = thunar_application_get();
    thumbnail_cache = thunar_application_get_thumbnail_cache(application);
    thunar_thumbnail_cache_move_file(thumbnail_cache, old_file, new_file);

    g_object_unref(thumbnail_cache);
    g_object_unref(application);
}


//-----------------------------------------------------------------------------

gboolean launcher_append_custom_actions(ThunarLauncher *launcher,
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

    e_return_val_if_fail(THUNAR_IS_LAUNCHER(launcher), FALSE);
    e_return_val_if_fail(GTK_IS_MENU(menu), FALSE);

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
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, FILEINFO(launcher->current_directory));
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

gboolean launcher_check_uca_key_activation(ThunarLauncher *launcher,
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
            thunarx_menu_items = thunarx_menu_provider_get_folder_menu_items(lp_provider->data, window, FILEINFO(launcher->current_directory));
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


