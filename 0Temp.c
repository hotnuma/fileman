
#if 0
        // we don't handle XdndDirectSave stage(3), result "F" yet
        if (G_UNLIKELY(gtk_selection_data_get_format(selection_data) == 8
                       && gtk_selection_data_get_length(selection_data) == 1
                       && gtk_selection_data_get_data(selection_data)[0] == 'F'))
        {
            // indicate that we don't provide "F" fallback
            gdk_property_change(gdk_drag_context_get_source_window(context),
                                gdk_atom_intern_static_string("XdndDirectSave0"),
                                gdk_atom_intern_static_string("text/plain"), 8,
                                GDK_PROP_MODE_REPLACE, (const guchar*) "", 0);
        }
        else if (G_LIKELY(gtk_selection_data_get_format(selection_data) == 8
                 && gtk_selection_data_get_length(selection_data) == 1
                 && gtk_selection_data_get_data(selection_data)[0] == 'S'))
        {
            // XDS was successfull, so determine the file for the drop position
            file = _standardview_get_drop_file(view, x, y, NULL);
            if (G_LIKELY(file != NULL))
            {
                // verify that we have a directory here
                if (th_file_is_directory(file))
                {
                    // reload the folder corresponding to the file
                    ThunarFolder *folder;
                    folder = th_folder_get_for_thfile(file);
                    th_folder_load(folder, false);
                    g_object_unref(G_OBJECT(folder));
                }

                // cleanup
                g_object_unref(G_OBJECT(file));
            }
        }

        // in either case, we succeed!
        succeed = true;
#endif

#if 0
        // check if the format is valid and we have any data
        if (G_LIKELY(gtk_selection_data_get_format(selection_data) == 8
                     && gtk_selection_data_get_length(selection_data) > 0))
        {
            // _NETSCAPE_URL looks like this: "$URL\n$TITLE"
            gchar       **bits;
            bits = g_strsplit((const gchar*) gtk_selection_data_get_data(selection_data), "\n", -1);
            if (G_LIKELY(g_strv_length(bits) == 2))
            {
                // determine the file for the drop position
                file = _standardview_get_drop_file(view, x, y, NULL);
                if (G_LIKELY(file != NULL))
                {
                    // determine the absolute path to the target directory
                    gchar        *working_directory;
                    working_directory = g_file_get_path(th_file_get_file(file));
                    if (G_LIKELY(working_directory != NULL))
                    {
                        // prepare the basic part of the command
                        gchar *argv[11];
                        gint n = 0;
                        argv[n++] = "exo-desktop-item-edit";
                        argv[n++] = "--type=Link";
                        argv[n++] = "--url";
                        argv[n++] = bits[0];
                        argv[n++] = "--name";
                        argv[n++] = bits[1];

                        // determine the toplevel window
                        GtkWidget    *toplevel;
                        toplevel = gtk_widget_get_toplevel(widget);
                        if (toplevel != NULL && gtk_widget_is_toplevel(toplevel))
                        {

#if defined(GDK_WINDOWING_X11)
                            // on X11, we can supply the parent window id here
                            argv[n++] = "--xid";
                            argv[n++] = g_newa(gchar, 32);
                            g_snprintf(argv[n - 1], 32, "%ld",(glong) GDK_WINDOW_XID(gtk_widget_get_window(GTK_WIDGET(toplevel))));
#endif

                        }

                        // terminate the parameter list
                        argv[n++] = "--create-new";
                        argv[n++] = working_directory;
                        argv[n++] = NULL;

                        GdkScreen *screen;
                        screen = gtk_widget_get_screen(GTK_WIDGET(widget));

                        char         *display = NULL;

                        if (screen != NULL)
                            display = g_strdup(gdk_display_get_name(gdk_screen_get_display(screen)));

                        // try to run exo-desktop-item-edit
                        gint          pid;
                        GError *error = NULL;
                        succeed = g_spawn_async(working_directory, argv, NULL,
                                                 G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH,
                                                 util_set_display_env, display, &pid, &error);

                        if (G_UNLIKELY(!succeed))
                        {
                            // display an error dialog to the user
                            dialog_error(view, error, _("Failed to create a link for the URL \"%s\""), bits[0]);
                            g_free(working_directory);
                            g_error_free(error);
                        }
                        else
                        {
                            // reload the directory when the command terminates
                            g_child_watch_add_full(
                                            G_PRIORITY_LOW,
                                            pid,
                                            _standardview_reload_directory,
                                            working_directory,
                                            g_free);
                        }

                        // cleanup
                        g_free(display);
                    }

                    g_object_unref(G_OBJECT(file));
                }
            }

            // cleanup
            g_strfreev(bits);
        }
#endif

#if 0
        // determine the drop position
        GdkDragAction actions;
        actions = _standardview_get_dest_actions(view, context, x, y, timestamp, &file);

        if (G_LIKELY((actions & (GDK_ACTION_COPY
                                 | GDK_ACTION_MOVE
                                 | GDK_ACTION_LINK)) != 0))
        {
            // ask the user what to do with the drop data
            GdkDragAction action;
            action = (gdk_drag_context_get_selected_action(context) == GDK_ACTION_ASK)
                     ? dnd_ask(GTK_WIDGET(view),
                               file,
                               view->priv->drop_file_list, actions)
                     : gdk_drag_context_get_selected_action(context);

            // perform the requested action
            if (G_LIKELY(action != 0))
            {
                // look if we can find the drag source widget
                GtkWidget    *source_widget;
                source_widget = gtk_drag_get_source_widget(context);

                GtkWidget    *source_view = NULL;
                if (source_widget != NULL)
                {
                    /* if this is a source view, attach it to the view receiving
                     * the data, see thunar_standardview_new_files */
                    source_view = gtk_widget_get_parent(source_widget);
                    if (!THUNAR_IS_VIEW(source_view))
                        source_view = NULL;
                }

                succeed = dnd_perform(
                                GTK_WIDGET(view),
                                file,
                                view->priv->drop_file_list,
                                action,
                                _standardview_new_files_closure(view, source_view));
            }
        }

        // release the file reference
        if (G_LIKELY(file != NULL))
            g_object_unref(G_OBJECT(file));
#endif


