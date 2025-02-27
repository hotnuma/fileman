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

#include "config.h"
#include "baseview.h"

#include "component.h"

static void baseview_class_init(gpointer klass);

GType baseview_get_type()
{
    static volatile gsize type__volatile = 0;
    GType type;

    if (g_once_init_enter((gsize*) &type__volatile))
    {
        type = g_type_register_static_simple(
            G_TYPE_INTERFACE,
            I_("BaseView"),
            sizeof(BaseViewIface),
            (GClassInitFunc) (void(*)(void)) baseview_class_init,
            0,
            NULL,
            0);

        g_type_interface_add_prerequisite(type, GTK_TYPE_WIDGET);
        g_type_interface_add_prerequisite(type, TYPE_THUNARCOMPONENT);

        g_once_init_leave(&type__volatile, type);
    }

    return type__volatile;
}

static void baseview_class_init(gpointer klass)
{
    /**
     * BaseView:loading:
     *
     * Indicates whether the given #BaseView is currently loading or
     * layouting its contents. Implementations should invoke
     * #g_object_notify() on this property whenever they start to load
     * the contents and then once they have finished loading.
     *
     * Other modules can use this property to display some kind of
     * user visible notification about the loading state, e.g. a
     * progress bar or an animated image.
     **/
    g_object_interface_install_property(klass,
                                        g_param_spec_boolean(
                                            "loading",
                                            "loading",
                                            "loading",
                                            FALSE,
                                            E_PARAM_READABLE));

    /**
     * BaseView:statusbar-text:
     *
     * The text to be displayed in the status bar, which is associated
     * with this #BaseView instance. Implementations should invoke
     * #g_object_notify() on this property, whenever they have a new
     * text to be display in the status bar(e.g. the selection changed
     * or similar).
     **/
    g_object_interface_install_property(klass,
                                        g_param_spec_string(
                                            "statusbar-text",
                                            "statusbar-text",
                                            "statusbar-text",
                                            NULL,
                                            E_PARAM_READABLE));

    /**
     * BaseView:show-hidden:
     *
     * Tells whether to display hidden and backup files in the
     * #BaseView or whether to hide them.
     **/
    g_object_interface_install_property(klass,
                                        g_param_spec_boolean(
                                            "show-hidden",
                                            "show-hidden",
                                            "show-hidden",
                                            FALSE,
                                            E_PARAM_READWRITE));

    /**
     * BaseView:zoom-level:
     *
     * The #ThunarZoomLevel at which the items within this
     * #BaseView should be displayed.
     **/
    g_object_interface_install_property(klass,
                                        g_param_spec_enum(
                                            "zoom-level",
                                            "zoom-level",
                                            "zoom-level",
                                            THUNAR_TYPE_ZOOM_LEVEL,
                                            THUNAR_ZOOM_LEVEL_100_PERCENT,
                                            E_PARAM_READWRITE));
}

/**
 * thunar_view_get_loading:
 * @view : a #BaseView instance.
 *
 * Tells whether the given #BaseView is currently loading or
 * layouting its contents.
 *
 * Return value: %TRUE if @view is currently being loaded, else %FALSE.
 **/
gboolean baseview_get_loading(BaseView *view)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), FALSE);

    return BASEVIEW_GET_IFACE(view)->get_loading(view);
}

/**
 * thunar_view_get_statusbar_text:
 * @view : a #BaseView instance.
 *
 * Queries the text that should be displayed in the status bar
 * associated with @view.
 *
 * Return value: the text to be displayed in the status bar
 *               asssociated with @view.
 **/
const gchar* baseview_get_statusbar_text(BaseView *view)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), NULL);

    return BASEVIEW_GET_IFACE(view)->get_statusbar_text(view);
}

/**
 * thunar_view_get_show_hidden:
 * @view : a #BaseView instance.
 *
 * Returns %TRUE if hidden and backup files are shown
 * in @view. Else %FALSE is returned.
 *
 * Return value: whether @view displays hidden files.
 **/
gboolean baseview_get_show_hidden(BaseView *view)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), FALSE);

    return BASEVIEW_GET_IFACE(view)->get_show_hidden(view);
}

/**
 * thunar_view_set_show_hidden:
 * @view        : a #BaseView instance.
 * @show_hidden : &TRUE to display hidden files, else %FALSE.
 *
 * If @show_hidden is %TRUE then @view will display hidden and
 * backup files, else those files will be hidden from the user
 * interface.
 **/
void baseview_set_show_hidden(BaseView *view, gboolean show_hidden)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));

    BASEVIEW_GET_IFACE(view)->set_show_hidden(view, show_hidden);
}

/**
 * thunar_view_get_zoom_level:
 * @view : a #BaseView instance.
 *
 * Returns the #ThunarZoomLevel currently used for the @view.
 *
 * Return value: the #ThunarZoomLevel currently used for the @view.
 **/
ThunarZoomLevel baseview_get_zoom_level(BaseView *view)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), THUNAR_ZOOM_LEVEL_100_PERCENT);

    return BASEVIEW_GET_IFACE(view)->get_zoom_level(view);
}

/**
 * thunar_view_set_zoom_level:
 * @view       : a #BaseView instance.
 * @zoom_level : the new #ThunarZoomLevel for @view.
 *
 * Sets the zoom level used for @view to @zoom_level.
 **/
void baseview_set_zoom_level(BaseView *view, ThunarZoomLevel zoom_level)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));
    e_return_if_fail(zoom_level < THUNAR_ZOOM_N_LEVELS);

    BASEVIEW_GET_IFACE(view)->set_zoom_level(view, zoom_level);
}

/**
 * thunar_view_reload:
 * @view : a #BaseView instance.
 * @reload_info : whether to reload file info for all files too
 *
 * Tells @view to reread the currently displayed folder
 * contents from the underlying media. If reload_info is
 * TRUE, it will reload information for all files too.
 **/
void baseview_reload(BaseView *view, gboolean reload_info)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));

    BASEVIEW_GET_IFACE(view)->reload(view, reload_info);
}

/**
 * thunar_view_get_visible_range:
 * @view       : a #BaseView instance.
 * @start_file : return location for start of region, or %NULL.
 * @end_file   : return location for end of region, or %NULL.
 *
 * Sets @start_file and @end_file to be the first and last visible
 * #ThunarFile.
 *
 * The files should be freed with g_object_unref() when no
 * longer needed.
 *
 * Return value: %TRUE if valid files were placed in @start_file
 *               and @end_file.
 **/
gboolean baseview_get_visible_range(BaseView  *view,
                                       ThunarFile **start_file,
                                       ThunarFile **end_file)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), FALSE);

    return BASEVIEW_GET_IFACE(view)->get_visible_range(view, start_file, end_file);
}

/**
 * thunar_view_scroll_to_file:
 * @view        : a #BaseView instance.
 * @file        : the #ThunarFile to scroll to.
 * @select_file : %TRUE to also select the @file in the @view.
 * @use_align   : whether to use alignment arguments.
 * @row_align   : the vertical alignment.
 * @col_align   : the horizontal alignment.
 *
 * Tells @view to scroll to the @file. If @view is currently
 * loading, it'll remember to scroll to @file later when
 * the contents are loaded.
 **/
void baseview_scroll_to_file(BaseView *view, ThunarFile *file,
                            gboolean select_file, gboolean use_align,
                            gfloat row_align, gfloat col_align)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));
    e_return_if_fail(IS_THUNARFILE(file));
    e_return_if_fail(row_align >= 0.0f && row_align <= 1.0f);
    e_return_if_fail(col_align >= 0.0f && col_align <= 1.0f);

    BASEVIEW_GET_IFACE(view)->scroll_to_file(view, file,
                                             select_file, use_align,
                                             row_align, col_align);
}

GList* baseview_get_selected_files(BaseView *view)
{
    e_return_val_if_fail(THUNAR_IS_VIEW(view), NULL);

    return BASEVIEW_GET_IFACE(view)->get_selected_files(view);
}

void baseview_set_selected_files(BaseView *view, GList *path_list)
{
    e_return_if_fail(THUNAR_IS_VIEW(view));

    BASEVIEW_GET_IFACE(view)->set_selected_files(view, path_list);
}


