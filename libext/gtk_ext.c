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
#include <gtk_ext.h>

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
void etk_label_set_a11y_relation(GtkLabel  *label, GtkWidget *widget)
{
    AtkRelationSet *relations;
    AtkRelation    *relation;
    AtkObject      *object;

    e_return_if_fail(GTK_IS_WIDGET(widget));
    e_return_if_fail(GTK_IS_LABEL(label));

    object = gtk_widget_get_accessible(widget);
    relations = atk_object_ref_relation_set(gtk_widget_get_accessible(GTK_WIDGET(label)));
    relation = atk_relation_new(&object, 1, ATK_RELATION_LABEL_FOR);
    atk_relation_set_add(relations, relation);
    g_object_unref(G_OBJECT(relation));
}

/**
 * thunar_gtk_menu_run:
 * @menu : a #GtkMenu.
 *
 * Conveniance wrapper for thunar_gtk_menu_run_at_event_pointer, to run a menu for the current event
 **/
void etk_menu_run(GtkMenu *menu)
{
    GdkEvent *event = gtk_get_current_event();
    etk_menu_run_at_event(menu, event);
    gdk_event_free(event);
}

#if GTK_CHECK_VERSION(3, 24, 8)
static void _moved_to_rect_cb(GdkWindow          *window,
                             const GdkRectangle *flipped_rect,
                             const GdkRectangle *final_rect,
                             gboolean            flipped_x,
                             gboolean            flipped_y,
                             GtkMenu            *menu)
{
    g_signal_emit_by_name(menu, "popped-up", 0, flipped_rect, final_rect, flipped_x, flipped_y);
    g_signal_stop_emission_by_name(window, "moved-to-rect");
}

static void _popup_menu_realized(GtkWidget *menu, gpointer   user_data)
{
    (void) user_data;
    GdkWindow *toplevel = gtk_widget_get_window(gtk_widget_get_toplevel(menu));
    g_signal_handlers_disconnect_by_func(toplevel, _moved_to_rect_cb, menu);
    g_signal_connect(toplevel, "moved-to-rect", G_CALLBACK(_moved_to_rect_cb), menu);
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
void etk_menu_run_at_event(GtkMenu *menu, GdkEvent *event)
{
    GMainLoop *loop;
    gulong     signal_id;

    e_return_if_fail(GTK_IS_MENU(menu));

    // take over the floating reference on the menu
    g_object_ref_sink(G_OBJECT(menu));

    // run an internal main loop
    loop = g_main_loop_new(NULL, FALSE);
    signal_id = g_signal_connect_swapped(G_OBJECT(menu), "deactivate", G_CALLBACK(g_main_loop_quit), loop);

#if GTK_CHECK_VERSION(3, 24, 8)
    // Workaround for incorrect popup menus size
    g_signal_connect(G_OBJECT(menu), "realize",
                     G_CALLBACK(_popup_menu_realized), NULL);
    gtk_widget_realize(GTK_WIDGET(menu));
#endif

    gtk_menu_popup_at_pointer(menu, event);
    gtk_menu_reposition(menu);
    gtk_grab_add(GTK_WIDGET(menu));
    g_main_loop_run(loop);
    g_main_loop_unref(loop);
    gtk_grab_remove(GTK_WIDGET(menu));

    g_signal_handler_disconnect(G_OBJECT(menu), signal_id);

    // release the menu reference
    g_object_unref(G_OBJECT(menu));
}

/**
 * etk_widget_set_tooltip:
 * @widget : a #GtkWidget for which to set the tooltip.
 * @format : a printf(3)-style format string.
 * @...    : additional arguments for @format.
 *
 * Sets the tooltip for the @widget to a string generated
 * from the @format and the additional arguments in @...<!--->.
 **/
void etk_widget_set_tooltip(GtkWidget *widget, const gchar *format, ...)
{
    va_list  var_args;
    gchar   *tooltip;

    e_return_if_fail(GTK_IS_WIDGET(widget));
    e_return_if_fail(g_utf8_validate(format, -1, NULL));

    // determine the tooltip
    va_start(var_args, format);
    tooltip = g_strdup_vprintf(format, var_args);
    va_end(var_args);

    // setup the tooltip for the widget
    gtk_widget_set_tooltip_text(widget, tooltip);

    // release the tooltip
    g_free(tooltip);
}

/**
 * thunar_gtk_get_focused_widget:
 * Return value:(transfer none): currently focused widget or NULL, if there is none.
 **/
GtkWidget* etk_get_focused_widget()
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
 * etk_mount_operation_new:
 * @parent : a #GtkWindow or non-toplevel widget.
 *
 * Create a mount operation with some defaults.
 **/
GMountOperation* etk_mount_operation_new(gpointer parent)
{
    GtkWindow *window = NULL;
    GdkScreen *screen = util_parse_parent(parent, &window);

    GMountOperation *operation = gtk_mount_operation_new(window);
    g_mount_operation_set_password_save(G_MOUNT_OPERATION(operation),
                                        G_PASSWORD_SAVE_FOR_SESSION);

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
gboolean etk_editable_can_cut(GtkEditable *editable)
{
    return gtk_editable_get_editable(editable) &&
           etk_editable_can_copy(editable);
}

/**
 * thunar_gtk_editable_can_copy:
 *
 * Return value: TRUE if it's possible to copy text from a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean etk_editable_can_copy(GtkEditable *editable)
{
    return gtk_editable_get_selection_bounds(editable, NULL,NULL);
}

/**
 * thunar_gtk_editable_can_paste:
 *
 * Return value: TRUE if it's possible to paste text to a GtkEditable.
 *               FALSE, otherwise.
 **/
gboolean etk_editable_can_paste(GtkEditable *editable)
{
    return gtk_editable_get_editable(editable);
}


