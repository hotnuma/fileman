
// Notebook View --------------------------------------------------------------

#if 0
static void _window_create_view_notebook(AppWindow *window);
static GtkWidget* _window_notebook_insert(AppWindow *window, ThunarFile *directory,
                                          GType view_type, gint position,
                                          ThunarHistory *history);
static void _window_notebook_switch_page(GtkWidget *notebook, GtkWidget *page,
                                         guint page_num, AppWindow *window);
static void _window_notebook_page_added(GtkWidget *notebook, GtkWidget *page,
                                        guint page_num, AppWindow *window);
static void _window_notebook_page_removed(GtkWidget *notebook, GtkWidget *page,
                                          guint page_num, AppWindow *window);
static gboolean window_tab_change(AppWindow *window, gint nth);
void window_update_directories(AppWindow *window, ThunarFile *old_directory,
                               ThunarFile *new_directory);
#endif

#if 0
    // Notebook ---------------------------------------------------------------

    gboolean with_notebook = false;
    if (with_notebook)
    {
        window->notebook = gtk_notebook_new();
        gtk_widget_set_hexpand(window->notebook, TRUE);
        gtk_widget_set_vexpand(window->notebook, TRUE);
        gtk_grid_attach(GTK_GRID(window->view_box), window->notebook, 0, 0, 1, 1);

        g_signal_connect(G_OBJECT(window->notebook), "switch-page",
                         G_CALLBACK(_window_notebook_switch_page), window);

        g_signal_connect(G_OBJECT(window->notebook), "page-added",
                         G_CALLBACK(_window_notebook_page_added), window);

        g_signal_connect(G_OBJECT(window->notebook), "page-removed",
                         G_CALLBACK(_window_notebook_page_removed), window);

        gtk_notebook_set_show_border(GTK_NOTEBOOK(window->notebook), FALSE);
        gtk_notebook_set_scrollable(GTK_NOTEBOOK(window->notebook), TRUE);
        gtk_container_set_border_width(GTK_CONTAINER(window->notebook), 0);
        gtk_notebook_set_group_name(GTK_NOTEBOOK(window->notebook), "thunar-tabs");
        gtk_widget_show(window->notebook);

        gtk_widget_set_can_focus(window->notebook, FALSE);
    }
    else
    {
        _window_create_detailview(window);
    }
#endif

#if 0
        // create a new view if the window is new
        if (window->view == NULL)
        {
            if (window->notebook != NULL)
                _window_create_view_notebook(window);
        }
#endif

#if 0
// Notebook -------------------------------------------------------------------

static void _window_create_view_notebook(AppWindow *window)
{
    DPRINT("window_create_view\n");

    ThunarFile *current_directory = NULL;

    /* if we have not got a current directory from the old view, use the
     *  window's current directory */
    if (window->current_directory != NULL)
        current_directory = g_object_ref(window->current_directory);

    e_assert(current_directory != NULL);

    // insert the new view
    DPRINT("enter : _window_notebook_insert\n");

    GtkWidget *new_view = _window_notebook_insert(window, current_directory,
                                                  TYPE_DETAILVIEW, 0,
                                                  NULL);
    DPRINT("leave : _window_notebook_insert\n");

    // switch to the new view
    gint page_num;
    page_num = gtk_notebook_page_num(GTK_NOTEBOOK(window->notebook), new_view);
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook), page_num);

    // take focus on the new view
    gtk_widget_grab_focus(new_view);

    ThunarFile *file = NULL;

    // scroll to the previously visible file in the old view
    if (G_UNLIKELY(file != NULL))
        baseview_scroll_to_file(BASEVIEW(new_view),
                                file, FALSE, TRUE, 0.0f, 0.0f);

    // restore the file selection
    GList *selected_files = NULL;
    component_set_selected_files(THUNARCOMPONENT(new_view), selected_files);
    e_list_free(selected_files);

    // release the file references
    if (G_UNLIKELY(file != NULL))
        g_object_unref(G_OBJECT(file));

    if (G_UNLIKELY(current_directory != NULL))
        g_object_unref(G_OBJECT(current_directory));

    // connect to the new history if this is the active view
    ThunarHistory *history = standardview_get_history(STANDARD_VIEW(new_view));

    if (window->history_changed_id == 0)
    {
        window->history_changed_id =
            g_signal_connect_swapped(G_OBJECT(history),
                                     "history-changed",
                                     G_CALLBACK(_window_history_changed),
                                     window);
    }
}

static GtkWidget* _window_notebook_insert(AppWindow     *window,
                                          ThunarFile    *directory,
                                          GType         view_type,
                                          gint          position,
                                          ThunarHistory *history)
{
    e_return_val_if_fail(IS_APPWINDOW(window), NULL);
    e_return_val_if_fail(IS_THUNARFILE(directory), NULL);
    e_return_val_if_fail(view_type != G_TYPE_NONE, NULL);
    e_return_val_if_fail(history == NULL || IS_THUNARHISTORY(history), NULL);

    DPRINT("window_notebook_insert\n");

    // allocate and setup a new view
    GtkWidget *view = g_object_new(view_type,
                                   "current-directory", directory,
                                   NULL);
    baseview_set_show_hidden(BASEVIEW(view), window->show_hidden);
    gtk_widget_show(view);

    // set the history of the view if a history is provided
    if (history != NULL)
        standardview_set_history(STANDARD_VIEW(view), history);

    GtkWidget *label_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    GtkWidget *label = gtk_label_new(NULL);

    g_object_bind_property(G_OBJECT(view), "display-name",
                           G_OBJECT(label), "label",
                           G_BINDING_SYNC_CREATE);
    g_object_bind_property(G_OBJECT(view), "tooltip-text",
                           G_OBJECT(label), "tooltip-text",
                           G_BINDING_SYNC_CREATE);

    gtk_widget_set_has_tooltip(label, TRUE);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_margin_start(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_end(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_top(GTK_WIDGET(label), 3);
    gtk_widget_set_margin_bottom(GTK_WIDGET(label), 3);
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
    gtk_label_set_single_line_mode(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(label_box), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    GtkWidget *button = gtk_button_new();
    gtk_box_pack_start(GTK_BOX(label_box), button, FALSE, FALSE, 0);
    gtk_widget_set_can_default(button, FALSE);
    gtk_widget_set_focus_on_click(button, FALSE);
    gtk_button_set_relief(GTK_BUTTON(button), GTK_RELIEF_NONE);
    gtk_widget_set_tooltip_text(button, _("Close tab"));
    g_signal_connect_swapped(G_OBJECT(button), "clicked",
                             G_CALLBACK(gtk_widget_destroy), view);
    gtk_widget_show(button);

    GtkWidget *icon;
    icon = gtk_image_new_from_icon_name("window-close", GTK_ICON_SIZE_MENU);
    gtk_container_add(GTK_CONTAINER(button), icon);
    gtk_widget_show(icon);

    // insert the new page
    gtk_notebook_insert_page(GTK_NOTEBOOK(window->notebook),
                             view, label_box, position);

    // set tab child properties
    gtk_container_child_set(GTK_CONTAINER(window->notebook), view,
                            "tab-expand", TRUE, NULL);
    gtk_notebook_set_tab_reorderable(GTK_NOTEBOOK(window->notebook), view, TRUE);
    gtk_notebook_set_tab_detachable(GTK_NOTEBOOK(window->notebook), view, TRUE);

    return view;
}

static void _window_notebook_page_added(GtkWidget *notebook, GtkWidget *page,
                                        guint page_num, AppWindow *window)
{
    (void) page_num;
    e_return_if_fail(IS_APPWINDOW(window));
    e_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    e_return_if_fail(THUNAR_IS_VIEW(page));
    e_return_if_fail(window->notebook == notebook);

    DPRINT("window_notebook_page_added\n");

    // connect signals
    g_signal_connect(G_OBJECT(page), "notify::loading",
                     G_CALLBACK(_window_notify_loading), window);

    g_signal_connect_swapped(G_OBJECT(page), "start-open-location",
                             G_CALLBACK(_window_start_open_location), window);

    g_signal_connect_swapped(G_OBJECT(page), "change-directory",
                             G_CALLBACK(window_set_current_directory), window);

    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(window->notebook), FALSE);
}

static void _window_notebook_switch_page(GtkWidget *notebook, GtkWidget *page,
                                         guint page_num, AppWindow *window)
{
    e_return_if_fail(window->notebook == notebook);
    e_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    e_return_if_fail(THUNAR_IS_VIEW(page));
    e_return_if_fail(IS_APPWINDOW(window));

    (void) page_num;

    // leave if nothing changed
    if (window->view == page)
        return;

    DPRINT("window_notebook_switch_page\n");

    // Use accelerators only on the current active tab
    if (window->view != NULL)
        g_object_set(G_OBJECT(window->view), "accel-group", NULL, NULL);

    g_object_set(G_OBJECT(page), "accel-group", window->accel_group, NULL);

    ThunarHistory *history;

    if (G_LIKELY(window->view != NULL))
    {
        // disconnect from previous history
        if (window->history_changed_id != 0)
        {
            history = standardview_get_history(STANDARD_VIEW(window->view));
            g_signal_handler_disconnect(history,
                                        window->history_changed_id);
            window->history_changed_id = 0;
        }

        // unset view during switch
        window->view = NULL;
    }

    if (window->view_bindings)
    {
        DPRINT("*** disconnect existing bindings\n");

        // disconnect existing bindings
        g_slist_free_full(window->view_bindings, g_object_unref);
        window->view_bindings = NULL;
    }

    // update the directory of the current window
    ThunarFile *current_directory;
    current_directory = navigator_get_current_directory(THUNARNAVIGATOR(page));
    window_set_current_directory(window, current_directory);

    // add stock bindings
    _window_binding_create(window,
                           window, "current-directory",
                           page, "current-directory",
                           G_BINDING_DEFAULT);

    _window_binding_create(window,
                           page, "selected-files",
                           window->launcher, "selected-files",
                           G_BINDING_SYNC_CREATE);

    _window_binding_create(window,
                           page, "zoom-level",
                           window, "zoom-level",
                           G_BINDING_SYNC_CREATE | G_BINDING_BIDIRECTIONAL);

    // connect to the sidepane(if any)
    if (G_LIKELY(window->sidepane != NULL))
    {
        _window_binding_create(window,
                               page, "selected-files",
                               window->sidepane, "selected-files",
                               G_BINDING_SYNC_CREATE);
    }

    // connect to the statusbar(if any)
    if (G_LIKELY(window->statusbar != NULL))
    {
        _window_binding_create(window,
                               page, "statusbar-text",
                               window->statusbar, "text",
                               G_BINDING_SYNC_CREATE);
    }

    // activate new view
    window->view = page;

    // connect to the new history
    history = standardview_get_history(STANDARD_VIEW(window->view));

    if (history)
    {
        if (window->history_changed_id == 0)
        {
            window->history_changed_id =
                g_signal_connect_swapped(G_OBJECT(history), "history-changed",
                                         G_CALLBACK(_window_history_changed),
                                         window);
        }

        _window_history_changed(window);
    }

    // update the selection
    standardview_selection_changed(STANDARD_VIEW(page));

    gtk_widget_grab_focus(page);
}

static void _window_notebook_page_removed(GtkWidget *notebook, GtkWidget *page,
                                          guint page_num, AppWindow *window)
{
    e_return_if_fail(IS_APPWINDOW(window));
    e_return_if_fail(GTK_IS_NOTEBOOK(notebook));
    e_return_if_fail(THUNAR_IS_VIEW(page));
    e_return_if_fail(window->notebook == notebook);

    (void) page_num;

    gint n_pages;

    DPRINT("window_notebook_page_removed\n");

    // drop connected signals
    g_signal_handlers_disconnect_matched(page, G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, window);

    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook));
    if (n_pages == 0)
    {
        // destroy the window
        gtk_widget_destroy(GTK_WIDGET(window));
    }
}

static gboolean window_tab_change(AppWindow *window, gint nth)
{
    e_return_val_if_fail(IS_APPWINDOW(window), FALSE);

    // Alt+0 is 10th tab
    gtk_notebook_set_current_page(GTK_NOTEBOOK(window->notebook),
                                  nth == -1 ? 9 : nth);

    return TRUE;
}

void window_update_directories(AppWindow *window, ThunarFile *old_directory,
                               ThunarFile *new_directory)
{
    e_return_if_fail(IS_APPWINDOW(window));
    e_return_if_fail(IS_THUNARFILE(old_directory));
    e_return_if_fail(IS_THUNARFILE(new_directory));

    gint n_pages;
    n_pages = gtk_notebook_get_n_pages(GTK_NOTEBOOK(window->notebook));


    if (G_UNLIKELY(n_pages == 0))
        return;

    gint active_page;
    active_page = gtk_notebook_get_current_page(GTK_NOTEBOOK(window->notebook));

    for (gint n = 0; n < n_pages; ++n)
    {
        // get the view
        GtkWidget *view;
        view = gtk_notebook_get_nth_page(GTK_NOTEBOOK(window->notebook), n);

        if (! IS_THUNARNAVIGATOR(view))
            continue;

        // get the directory of the view
        ThunarFile *directory;
        directory = navigator_get_current_directory(THUNARNAVIGATOR(view));

        if (! IS_THUNARFILE(directory))
            continue;

        // if it matches the old directory, change to the new one
        if (directory == old_directory)
        {
            if (n == active_page)
                navigator_change_directory(THUNARNAVIGATOR(view), new_directory);
            else
                navigator_set_current_directory(THUNARNAVIGATOR(view), new_directory);
        }
    }
}
#endif


