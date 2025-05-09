/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "dnd.h"

#include "application.h"
#include "launcher.h"
#include "dialogs.h"
#include "gtk-ext.h"

static void _dnd_action_selected(GtkWidget *item,
                                 GdkDragAction *dnd_action_return);

/**
 * thunar_dnd_ask:
 * @widget    : the widget on which the drop was performed.
 * @folder    : the #ThunarFile to which the @path_list is being dropped.
 * @path_list : the #GFile<!---->s of the files being dropped to @file.
 * @actions   : the list of actions supported for the drop.
 *
 * Pops up a menu that asks the user to choose one of the
 * @actions or to cancel the drop. If the user chooses a
 * valid #GdkDragAction from @actions, then this action is
 * returned. Else if the user cancels the drop, 0 will be
 * returned.
 *
 * This method can be used to implement a response to the
 * #GDK_ACTION_ASK action on drops.
 *
 * Return value: the selected #GdkDragAction or 0 to cancel.
 **/
GdkDragAction dnd_ask(GtkWidget *widget, ThunarFile *folder,
                      GList *path_list, GdkDragAction dnd_actions)
{
    static const GdkDragAction dnd_action_items[] = {GDK_ACTION_COPY,
                                                     GDK_ACTION_MOVE,
                                                     GDK_ACTION_LINK};

    static const gchar *dnd_action_names[] = {N_("_Copy here"),
                                              N_("_Move here"),
                                              N_("_Link here")};

    static const gchar *dnd_action_icons[] = {"stock_folder-copy",
                                              "stock_folder-move",
                                              NULL};

    e_return_val_if_fail(th_file_is_directory(folder), 0);
    e_return_val_if_fail(GTK_IS_WIDGET(widget), 0);

    // prepare the popup menu
    GtkWidget *menu = gtk_menu_new();

    GtkWidget *item;
    GdkDragAction dnd_action = 0;
    GtkWidget *image;

    // append the various items
    for (guint n = 0; n < G_N_ELEMENTS(dnd_action_items); ++n)
    {
        if ((dnd_actions & dnd_action_items[n]) != 0)
        {
            G_GNUC_BEGIN_IGNORE_DEPRECATIONS
            item = gtk_image_menu_item_new_with_mnemonic(
                                            _(dnd_action_names[n]));
            G_GNUC_END_IGNORE_DEPRECATIONS
            g_object_set_data(G_OBJECT(item),
                              I_("dnd-action"),
                              GUINT_TO_POINTER(dnd_action_items[n]));
            g_signal_connect(G_OBJECT(item), "activate",
                             G_CALLBACK(_dnd_action_selected), &dnd_action);
            gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
            gtk_widget_show(item);

            // add image to the menu item
            if (dnd_action_icons[n] != NULL)
            {
                image = gtk_image_new_from_icon_name(dnd_action_icons[n],
                                                     GTK_ICON_SIZE_MENU);
                G_GNUC_BEGIN_IGNORE_DEPRECATIONS
                gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item),
                                              image);
                G_GNUC_END_IGNORE_DEPRECATIONS
            }
        }
    }

    // append the separator
    item = gtk_separator_menu_item_new();
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    GtkWidget *window = gtk_widget_get_toplevel(widget);
    ThunarFile *file;
    GList *file_list = NULL;

    if (window != NULL && gtk_widget_get_toplevel(window))
    {
        // check if we can resolve all paths
        for (GList *lp = path_list; lp != NULL; lp = lp->next)
        {
            // try to resolve this path
            file = th_file_cache_lookup(lp->data);

            if (file != NULL)
                file_list = g_list_prepend(file_list, file);
            else
                break;
        }
    }

    // append the cancel item
    item = gtk_menu_item_new_with_mnemonic(_("_Cancel"));
    gtk_menu_shell_append(GTK_MENU_SHELL(menu), item);
    gtk_widget_show(item);

    // run the menu(takes over the floating of menu)
    etk_menu_run(GTK_MENU(menu), NULL);

    g_list_free_full(file_list, g_object_unref);

    return dnd_action;
}

static void _dnd_action_selected(GtkWidget *item,
                                 GdkDragAction *dnd_action_return)
{
    *dnd_action_return = GPOINTER_TO_UINT(g_object_get_data(G_OBJECT(item),
                                                            "dnd-action"));
}

/**
 * thunar_dnd_perform:
 * @widget            : the #GtkWidget on which the drop was done.
 * @file              : the #ThunarFile on which the @file_list was dropped.
 * @file_list         : the list of #GFile<!---->s that was dropped.
 * @action            : the #GdkDragAction that was performed.
 * @new_files_closure : a #GClosure to connect to the job's "new-files" signal,
 *                      which will be emitted when the job finishes with the
 *                      list of #GFile<!---->s created by the job, or
 *                      %NULL if you're not interested in the signal.
 *
 * Performs the drop of @file_list on @file in @widget, as given in
 * @action and returns %TRUE if the drop was started successfully
 *(or even completed successfully), else %FALSE.
 *
 * Return value: %TRUE if the DnD operation was started
 *               successfully, else %FALSE.
 **/
gboolean dnd_perform(GtkWidget *widget, ThunarFile *file, GList *file_list,
                     GdkDragAction action, GClosure *new_files_closure)
{
    e_return_val_if_fail(GTK_IS_WIDGET(widget), FALSE);
    e_return_val_if_fail(IS_THUNARFILE(file), FALSE);
    e_return_val_if_fail(gtk_widget_get_realized(widget), FALSE);

    gboolean succeed = TRUE;
    GError *error = NULL;

    // check if the file is a directory
    if (th_file_is_directory(file))
    {
        // perform the given directory operation
        switch(action)
        {
        case GDK_ACTION_COPY:
            execute_copy_into(widget,
                              file_list,
                              th_file_get_file(file),
                              new_files_closure);
            break;

        case GDK_ACTION_MOVE:
            execute_move_into(widget,
                              file_list,
                              th_file_get_file(file),
                              new_files_closure);
            break;

        case GDK_ACTION_LINK:
            execute_link_into(widget,
                              file_list,
                              th_file_get_file(file),
                              new_files_closure);
            break;

        default:
            succeed = FALSE;
        }
    }
    else if (th_file_is_executable(file))
    {
        // TODO any chance to determine the working dir here?
        succeed = th_file_execute(file,
                                  NULL, widget, file_list, NULL, &error);
        if (!succeed)
        {
            // display an error to the user
            dialog_error(widget,
                         error,
                         _("Failed to execute file \"%s\""),
                         th_file_get_display_name(file));

            // release the error
            g_error_free(error);
        }
    }
    else
    {
        succeed = FALSE;
    }

    return succeed;
}


