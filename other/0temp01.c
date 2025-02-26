
#if 0
void browser_poke_location(ThunarBrowser *browser, GFile *location,
                           gpointer widget, ThunarBrowserPokeLocationFunc func,
                           gpointer user_data);

static void _browser_poke_location_file_finish(GFile      *location,
                                               ThunarFile *file,
                                               GError     *error,
                                               gpointer    user_data);

/**
 * thunar_browser_poke_location:
 * @browser   : a #ThunarBrowser.
 * @location  : a #GFile.
 * @widget    : a #GtkWidget, a #GdkScreen or %NULL.
 * @func      : a #ThunarBrowserPokeDeviceFunc callback or %NULL.
 * @user_data : pointer to arbitrary user data or %NULL.
 *
 * Pokes a #GFile to see what's behind it.
 *
 * It first resolves the #GFile into a #ThunarFile asynchronously using
 * thunar_file_get_async(). It then performs the same steps as
 * thunar_browser_poke_file().
 **/
void browser_poke_location(ThunarBrowser                 *browser,
                           GFile                         *location,
                           gpointer                      widget,
                           ThunarBrowserPokeLocationFunc func,
                           gpointer                      user_data)
{
    (void) widget;

    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(G_IS_FILE(location));

    PokeFileData *poke_data = _browser_poke_file_data_new(browser,
                                                          location,
                                                          NULL,
                                                          NULL,
                                                          NULL,
                                                          func,
                                                          user_data);

    th_file_get_async(location,
                      NULL,
                      _browser_poke_location_file_finish,
                      poke_data);
}

static void _browser_poke_location_file_finish(GFile      *location,
                                               ThunarFile *file,
                                               GError     *error,
                                               gpointer    user_data)
{
    PokeFileData *poke_data = user_data;

    e_return_if_fail(G_IS_FILE(location));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(G_IS_FILE(poke_data->location));

    if (error == NULL)
    {
        _browser_poke_file_internal(poke_data->browser,
                                    location,
                                    file,
                                    file,
                                    NULL,
                                    poke_data->func,
                                    poke_data->location_func,
                                    poke_data->user_data);
    }
    else
    {
        if (poke_data->location_func != NULL)
        {
            (poke_data->location_func)(poke_data->browser,
                                       location,
                                       NULL,
                                       NULL,
                                       error,
                                       poke_data->user_data);
        }
    }

    _browser_poke_file_data_free(poke_data);
}
#endif


