
#if 0

thunar_window_notebook_insert
thunar_window_notebook_page_added
thunar_window_notebook_show_tabs
thunar_window_notebook_switch_page
thunar_window_notebook_page_removed



// ============================================================================
#if 0
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
#endif



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


