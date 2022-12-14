/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <config.h>
#include <gtk-ext.h>

#include <libext.h>
#include <utils.h>
#include <stdarg.h>

#include <libxfce4ui/libxfce4ui.h>

/**
 * thunar_gtk_label_set_a11y_relation:
 * @label  : a #GtkLabel.
 * @widget : a #GtkWidget.
 *
 * Sets the %ATK_RELATION_LABEL_FOR relation on @label for @widget, which means
 * accessiblity tools will identify @label as descriptive item for the specified
 * @widget.
 **/
void thunar_gtk_label_set_a11y_relation(GtkLabel  *label, GtkWidget *widget)
{
    AtkRelationSet *relations;
    AtkRelation    *relation;
    AtkObject      *object;

    thunar_return_if_fail(GTK_IS_WIDGET(widget));
    thunar_return_if_fail(GTK_IS_LABEL(label));

    object = gtk_widget_get_accessible(widget);
    relations = atk_object_ref_relation_set(gtk_widget_get_accessible(GTK_WIDGET(label)));
    relation = atk_relation_new(&object, 1, ATK_RELATION_LABEL_FOR);
    atk_relation_set_add(relations, relation);
    g_object_unref(G_OBJECT(relation));
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
#if 0
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
#endif

/**
 * thunar_gtk_menu_clean:
 * @menu : a #GtkMenu.
 *
 * Walks through the menu and all submenus and removes them,
 * so that the result will be a clean #GtkMenu without any items
 **/
void thunar_gtk_menu_clean(GtkMenu *menu)
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
 * thunar_gtk_menu_run:
 * @menu : a #GtkMenu.
 *
 * Conveniance wrapper for thunar_gtk_menu_run_at_event_pointer, to run a menu for the current event
 **/
void thunar_gtk_menu_run(GtkMenu *menu)
{
    GdkEvent *event = gtk_get_current_event();
    thunar_gtk_menu_run_at_event(menu, event);
    gdk_event_free(event);
}

#if GTK_CHECK_VERSION(3, 24, 8)
static void moved_to_rect_cb(GdkWindow          *window,
                             const GdkRectangle *flipped_rect,
                             const GdkRectangle *final_rect,
                             gboolean            flipped_x,
                             gboolean            flipped_y,
                             GtkMenu            *menu)
{
    g_signal_emit_by_name(menu, "popped-up", 0, flipped_rect, final_rect, flipped_x, flipped_y);
    g_signal_stop_emission_by_name(window, "moved-to-rect");
}

static void popup_menu_realized(GtkWidget *menu, gpointer   user_data)
{
    UNUSED(user_data);
    GdkWindow *toplevel = gtk_widget_get_window(gtk_widget_get_toplevel(menu));
    g_signal_handlers_disconnect_by_func(toplevel, moved_to_rect_cb, menu);
    g_signal_connect(toplevel, "moved-to-rect", G_CALLBACK(moved_to_rect_cb), menu);
}
#endif

/**
 * thunar_gtk_menu_run_at_event:
 * @menu  : a #GtkMenu.
 * @event : a #GdkEvent which may be NULL if no previous event was stored.
 *
 * A simple wrapper around gtk_menu_popup_at_pointer(), which runs the @menu in a separate
 * main loop and returns only after the @menu was deactivated.
 *
 * This method automatically takes over the floating reference of @menu if any and
 * releases it on return. That means if you created the menu via gtk_menu_new() you'll
 * not need to take care of destroying the menu later.
 *
 **/
void thunar_gtk_menu_run_at_event(GtkMenu *menu, GdkEvent *event)
{
    GMainLoop *loop;
    gulong     signal_id;

    thunar_return_if_fail(GTK_IS_MENU(menu));

    /* take over the floating reference on the menu */
    g_object_ref_sink(G_OBJECT(menu));

    /* run an internal main loop */
    loop = g_main_loop_new(NULL, FALSE);
    signal_id = g_signal_connect_swapped(G_OBJECT(menu), "deactivate", G_CALLBACK(g_main_loop_quit), loop);

#if GTK_CHECK_VERSION(3, 24, 8)
    /* Workaround for incorrect popup menus size */
    g_signal_connect(G_OBJECT(menu), "realize", G_CALLBACK(popup_menu_realized), NULL);
    gtk_widget_realize(GTK_WIDGET(menu));
#endif

    gtk_menu_popup_at_pointer(menu, event);
    gtk_menu_reposition(menu);
    gtk_grab_add(GTK_WIDGET(menu));
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    gtk_grab_remove(GTK_WIDGET(menu));

    g_signal_handler_disconnect(G_OBJECT(menu), signal_id);

    /* release the menu reference */
    g_object_unref(G_OBJECT(menu));
}

/**
 * thunar_gtk_widget_set_tooltip:
 * @widget : a #GtkWidget for which to set the tooltip.
 * @format : a printf(3)-style format string.
 * @...    : additional arguments for @format.
 *
 * Sets the tooltip for the @widget to a string generated
 * from the @format and the additional arguments in @...<!--->.
 **/
void thunar_gtk_widget_set_tooltip(GtkWidget   *widget, const gchar *format, ...)
{
    va_list  var_args;
    gchar   *tooltip;

    thunar_return_if_fail(GTK_IS_WIDGET(widget));
    thunar_return_if_fail(g_utf8_validate(format, -1, NULL));

    /* determine the tooltip */
    va_start(var_args, format);
    tooltip = g_strdup_vprintf(format, var_args);
    va_end(var_args);

    /* setup the tooltip for the widget */
    gtk_widget_set_tooltip_text(widget, tooltip);

    /* release the tooltip */
    g_free(tooltip);
}

/**
 * thunar_gtk_get_focused_widget:
 * Return value:(transfer none): currently focused widget or NULL, if there is none.
 **/
GtkWidget* thunar_gtk_get_focused_widget()
{
    GtkApplication *app;
    GtkWindow      *window;
    app = GTK_APPLICATION(g_application_get_default());
    if (NULL == app)
        return NULL;

    window = gtk_application_get_active_window(app);

    return gtk_window_get_focus(window);
}

/**
 * thunar_gtk_mount_operation_new:
 * @parent : a #GtkWindow or non-toplevel widget.
 *
 * Create a mount operation with some defaults.
 **/
GMountOperation* thunar_gtk_mount_operation_new(gpointer parent)
{
    GMountOperation *operation;
    GdkScreen       *screen;
    GtkWindow       *window = NULL;

    screen = thunar_util_parse_parent(parent, &window);

    operation = gtk_mount_operation_new(window);
    g_mount_operation_set_password_save(G_MOUNT_OPERATION(operation), G_PASSWORD_SAVE_FOR_SESSION);
    if (window == NULL && screen != NULL)
        gtk_mount_operation_set_screen(GTK_MOUNT_OPERATION(operation), screen);

    return operation;
}

/**
 * thunar_gtk_editable_can_cut:
 *
 * Return value: TRUE if it's possible to cut text off of a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean thunar_gtk_editable_can_cut(GtkEditable *editable)
{
    return gtk_editable_get_editable(editable) &&
           thunar_gtk_editable_can_copy(editable);
}

/**
 * thunar_gtk_editable_can_copy:
 *
 * Return value: TRUE if it's possible to copy text from a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean thunar_gtk_editable_can_copy(GtkEditable *editable)
{
    return gtk_editable_get_selection_bounds(editable, NULL,NULL);
}

/**
 * thunar_gtk_editable_can_paste:
 *
 * Return value: TRUE if it's possible to paste text to a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean thunar_gtk_editable_can_paste(GtkEditable *editable)
{
    return gtk_editable_get_editable(editable);
}


