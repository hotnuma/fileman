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
#include <history.h>

#include <navigator.h>
#include <iconfactory.h>
#include <dialogs.h>
#include <libxfce4ui/libxfce4ui.h>
#include <gio_ext.h>
#include <gtk_ext.h>

static void history_navigator_init(ThunarNavigatorIface *iface);
static void history_finalize(GObject *object);
static void history_get_property(GObject *object,
                                 guint prop_id,
                                 GValue *value,
                                 GParamSpec *pspec);
static void history_set_property(GObject *object,
                                 guint prop_id,
                                 const GValue *value,
                                 GParamSpec *pspec);
static ThunarFile* history_get_current_directory(ThunarNavigator *navigator);
static void history_set_current_directory(ThunarNavigator *navigator,
                                          ThunarFile *current_directory);
static GFile* _history_get_gfile(ThunarFile *file);

// Public ---------------------------------------------------------------------

// history_show_menu
static void _history_action_back_nth(GtkWidget *item, ThunarHistory *history);
static void _history_action_forward_nth(GtkWidget *item, ThunarHistory *history);

static void _history_go_back(ThunarHistory *history, GFile *goto_file);
static void _history_go_forward(ThunarHistory *history, GFile *goto_file);
static void _history_error_not_found(GFile *goto_file, gpointer parent);

// ThunarHistory --------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
};

enum
{
    HISTORY_CHANGED,
    LAST_SIGNAL,
};
static guint _history_signals[LAST_SIGNAL];

struct _ThunarHistory
{
    GObject     __parent__;

    ThunarFile  *current_directory;

    GSList      *back_list;
    GSList      *forward_list;
};

G_DEFINE_TYPE_WITH_CODE(ThunarHistory, history, G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARNAVIGATOR,
                                              history_navigator_init))

static GQuark _history_display_name_quark;
static GQuark _history_gfile_quark;

static void history_class_init(ThunarHistoryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = history_finalize;
    gobject_class->get_property = history_get_property;
    gobject_class->set_property = history_set_property;

    _history_display_name_quark = g_quark_from_static_string("thunar-history-display-name");
    _history_gfile_quark = g_quark_from_static_string("thunar-history-gfile");

    /**
     * ThunarHistory::current-directory:
     *
     * Inherited from #ThunarNavigator.
     **/
    g_object_class_override_property(gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     "current-directory");

    _history_signals[HISTORY_CHANGED] =
        g_signal_new(I_("history-changed"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(ThunarHistoryClass, history_changed),
                     NULL, NULL,
                     g_cclosure_marshal_VOID__STRING,
                     G_TYPE_NONE, 1, G_TYPE_STRING);
}

static void history_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = history_get_current_directory;
    iface->set_current_directory = history_set_current_directory;
}

static void history_init(ThunarHistory *history)
{
    (void) history;
}

static void history_finalize(GObject *object)
{
    ThunarHistory *history = THUNARHISTORY(object);

    // release the "forward" and "back" lists
    g_slist_free_full(history->forward_list, g_object_unref);
    g_slist_free_full(history->back_list, g_object_unref);

    G_OBJECT_CLASS(history_parent_class)->finalize(object);
}

static void history_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarHistory *history = THUNARHISTORY(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, navigator_get_current_directory(THUNARNAVIGATOR(history)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void history_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
    (void) pspec;
    ThunarHistory *history = THUNARHISTORY(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNARNAVIGATOR(history), g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static ThunarFile* history_get_current_directory(ThunarNavigator *navigator)
{
    return THUNARHISTORY(navigator)->current_directory;
}

static void history_set_current_directory(ThunarNavigator *navigator,
                                          ThunarFile      *current_directory)
{
    ThunarHistory *history = THUNARHISTORY(navigator);
    GFile         *gfile;

    // verify that we don't already use that directory
    if (G_UNLIKELY(current_directory == history->current_directory))
        return;

    /* if the new directory is the first one in the forward history, we
     * just move forward one step instead of clearing the whole forward
     * history */
    if (current_directory != NULL
            && history->forward_list != NULL
            && g_file_equal(th_file_get_file(current_directory), history->forward_list->data))
    {
        _history_go_forward(history, history->forward_list->data);
    }
    else
    {
        // clear the "forward" list
        g_slist_free_full(history->forward_list, g_object_unref);
        history->forward_list = NULL;

        // prepend the previous current directory to the "back" list
        if (G_LIKELY(history->current_directory != NULL))
        {
            gfile = _history_get_gfile(history->current_directory);
            history->back_list = g_slist_prepend(history->back_list, gfile);

            g_object_unref(history->current_directory);
        }

        // activate the new current directory
        history->current_directory = current_directory;

        // connect to the new current directory
        if (G_LIKELY(current_directory != NULL))
            g_object_ref(G_OBJECT(current_directory));
    }

    // notify listeners
    if (current_directory != NULL)
    {
        g_object_notify(G_OBJECT(history), "current-directory");
        g_signal_emit(G_OBJECT(history), _history_signals[HISTORY_CHANGED], 0, history);
    }
}

static GFile* _history_get_gfile(ThunarFile *file)
{
    GFile       *gfile;
    const gchar *display_name;

    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    gfile = th_file_get_file(file);

    display_name = th_file_get_display_name(file);
    g_object_set_qdata_full(G_OBJECT(gfile), _history_display_name_quark,
                            g_strdup(display_name), g_free);

    return g_object_ref(gfile);
}

// Public ---------------------------------------------------------------------

ThunarHistory* history_copy(ThunarHistory *history)
{
    ThunarHistory *copy;
    GSList        *lp;

    e_return_val_if_fail(history == NULL || IS_THUNARHISTORY(history), NULL);

    if (G_UNLIKELY(history == NULL))
        return NULL;

    copy = g_object_new(TYPE_THUNARHISTORY, NULL);

    // take a ref on the current directory
    copy->current_directory = g_object_ref(history->current_directory);

    // copy the back list
    for(lp = history->back_list; lp != NULL; lp = lp->next)
        copy->back_list = g_slist_append(copy->back_list, g_object_ref(G_OBJECT(lp->data)));

    // copy the forward list
    for(lp = history->forward_list; lp != NULL; lp = lp->next)
        copy->forward_list = g_slist_append(copy->forward_list, g_object_ref(G_OBJECT(lp->data)));

    return copy;
}

void history_show_menu(ThunarHistory         *history,
                       ThunarHistoryMenuType type,
                       GtkWidget             *parent)
{
    IconFactory *icon_factory;
    GtkIconTheme      *icon_theme;
    GCallback          handler;
    GtkWidget         *image;
    GtkWidget         *menu;
    GtkWidget         *item;
    GdkPixbuf         *icon;
    GSList            *lp;
    ThunarFile        *file;
    const gchar       *display_name;
    const gchar       *icon_name;
    gchar             *parse_name;

    e_return_if_fail(GTK_IS_WIDGET(parent));
    e_return_if_fail(IS_THUNARHISTORY(history));

    menu = gtk_menu_new();

    // determine the icon factory to use to load the icons
    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(parent));
    icon_factory = iconfact_get_for_icon_theme(icon_theme);

    // check if we have "Back" or "Forward" here
    if (type == THUNARHISTORY_MENU_BACK)
    {
        // display the "back" list
        lp = history->back_list;
        handler = G_CALLBACK(_history_action_back_nth);
    }
    else
    {
        // display the "forward" list
        lp = history->forward_list;
        handler = G_CALLBACK(_history_action_forward_nth);
    }

    // add menu items for all list items
    for(; lp != NULL; lp = lp->next)
    {
        parse_name = g_file_get_parse_name(lp->data);
        file = th_file_cache_lookup(lp->data);
        image = NULL;
        if (file != NULL)
        {
            // load the icon for the file
            icon = iconfact_load_file_icon(icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 16);
            if (icon != NULL)
            {
                // setup the image for the file
                image = gtk_image_new_from_pixbuf(icon);
                g_object_unref(G_OBJECT(icon));
            }

            g_object_unref(file);
        }

        if (image == NULL)
        {
            // some custom likely alternatives
            if (e_file_is_home(lp->data))
                icon_name = "user-home";
            else if (!g_file_has_uri_scheme(lp->data, "file"))
                icon_name = "folder-remote";
            else if (e_file_is_root(lp->data))
                icon_name = "drive-harddisk";
            else
                icon_name = "folder";

            image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        }

        // add an item for this file
        display_name = g_object_get_qdata(G_OBJECT(lp->data), _history_display_name_quark);
        item = xfce_gtk_image_menu_item_new(display_name, parse_name, NULL,
                                             NULL, NULL, image, GTK_MENU_SHELL(menu));
        g_object_set_qdata(G_OBJECT(item), _history_gfile_quark, lp->data);
        g_signal_connect(G_OBJECT(item), "activate", handler, history);

        g_free(parse_name);
    }

    gtk_widget_show_all(menu);
    // release the icon factory
    g_object_unref(G_OBJECT(icon_factory));

    // run the menu(takes over the floating of menu)
    etk_menu_run(GTK_MENU(menu));
}

// Actions --------------------------------------------------------------------

static void _history_action_back_nth(GtkWidget *item, ThunarHistory *history)
{
    GFile *file;

    e_return_if_fail(GTK_IS_MENU_ITEM(item));
    e_return_if_fail(IS_THUNARHISTORY(history));

    file = g_object_get_qdata(G_OBJECT(item), _history_gfile_quark);
    if (G_LIKELY(file != NULL))
        _history_go_back(history, file);
}

void history_action_back(ThunarHistory *history)
{
    e_return_if_fail(IS_THUNARHISTORY(history));

    // go back one step
    if (history->back_list != NULL)
        _history_go_back(history, history->back_list->data);
}

static void _history_go_back(ThunarHistory *history, GFile *goto_file)
{
    GFile      *gfile;
    GSList     *lp;
    GSList     *lnext;
    ThunarFile *directory;

    e_return_if_fail(IS_THUNARHISTORY(history));
    e_return_if_fail(G_IS_FILE(goto_file));

    // check if the directory still exists
    directory = th_file_get(goto_file, NULL);
    if (directory == NULL || ! th_file_is_mounted(directory))
    {
        _history_error_not_found(goto_file, NULL);

        // delete item from the history
        lp = g_slist_find(history->back_list, goto_file);
        if (lp != NULL)
        {
            g_object_unref(lp->data);
            history->back_list = g_slist_delete_link(history->back_list, lp);
        }
        return;
    }

    // prepend the previous current directory to the "forward" list
    if (G_LIKELY(history->current_directory != NULL))
    {
        gfile = _history_get_gfile(history->current_directory);
        history->forward_list = g_slist_prepend(history->forward_list, gfile);

        g_object_unref(history->current_directory);
        history->current_directory = NULL;
    }

    /* add all the items of the back list to the "forward" list until
     * the target file is reached  */
    for(lp = history->back_list; lp != NULL; lp = lnext)
    {
        lnext = lp->next;

        if (g_file_equal(goto_file, G_FILE(lp->data)))
        {
            if (directory != NULL)
                history->current_directory = g_object_ref(directory);

            // remove the new directory from the list
            g_object_unref(lp->data);
            history->back_list = g_slist_delete_link(history->back_list, lp);

            break;
        }

        // remove item from the list
        history->back_list = g_slist_remove_link(history->back_list, lp);

        // prepend element to the other list
        lp->next = history->forward_list;
        history->forward_list = lp;
    }

    if (directory != NULL)
        g_object_unref(directory);

    // tell the other modules to change the current directory
    if (G_LIKELY(history->current_directory != NULL))
        navigator_change_directory(THUNARNAVIGATOR(history), history->current_directory);
}

static void _history_action_forward_nth(GtkWidget *item, ThunarHistory *history)
{
    GFile *file;

    e_return_if_fail(GTK_IS_MENU_ITEM(item));
    e_return_if_fail(IS_THUNARHISTORY(history));

    file = g_object_get_qdata(G_OBJECT(item), _history_gfile_quark);
    if (G_LIKELY(file != NULL))
        _history_go_forward(history, file);
}

void history_action_forward(ThunarHistory *history)
{
    e_return_if_fail(IS_THUNARHISTORY(history));

    // go forward one step
    if (history->forward_list != NULL)
        _history_go_forward(history, history->forward_list->data);
}

static void _history_go_forward(ThunarHistory *history, GFile *goto_file)
{
    GFile      *gfile;
    GSList     *lnext;
    GSList     *lp;
    ThunarFile *directory;

    e_return_if_fail(IS_THUNARHISTORY(history));
    e_return_if_fail(G_IS_FILE(goto_file));

    // check if the directory still exists
    directory = th_file_get(goto_file, NULL);
    if (directory == NULL || ! th_file_is_mounted(directory))
    {
        _history_error_not_found(goto_file, NULL);

        // delete item from the history
        lp = g_slist_find(history->forward_list, goto_file);
        if (lp != NULL)
        {
            g_object_unref(lp->data);
            history->forward_list = g_slist_delete_link(history->forward_list, lp);
        }
        return;
    }

    // prepend the previous current directory to the "back" list
    if (G_LIKELY(history->current_directory != NULL))
    {
        gfile = _history_get_gfile(history->current_directory);
        history->back_list = g_slist_prepend(history->back_list, gfile);

        g_object_unref(history->current_directory);
        history->current_directory = NULL;
    }

    for (lp = history->forward_list; lp != NULL; lp = lnext)
    {
        lnext = lp->next;

        if (g_file_equal(goto_file, G_FILE(lp->data)))
        {
            if (directory != NULL)
                history->current_directory = g_object_ref(directory);

            // remove the new dirctory from the list
            g_object_unref(lp->data);
            history->forward_list = g_slist_delete_link(history->forward_list, lp);

            break;
        }

        // remove item from the list
        history->forward_list = g_slist_remove_link(history->forward_list, lp);

        // prepend item to the back list
        lp->next = history->back_list;
        history->back_list = lp;
    }

    if (directory != NULL)
        g_object_unref(directory);

    // tell the other modules to change the current directory
    if (G_LIKELY(history->current_directory != NULL))
        navigator_change_directory(THUNARNAVIGATOR(history), history->current_directory);
}

static void _history_error_not_found(GFile *goto_file, gpointer parent)
{
    gchar  *parse_name;
    gchar  *path;
    GError *error = NULL;

    g_set_error_literal(&error, G_FILE_ERROR, G_FILE_ERROR_EXIST,
                         _("The item will be removed from the history"));

    path = g_file_get_path(goto_file);
    if (path == NULL)
    {
        path = g_file_get_uri(goto_file);
        parse_name = g_uri_unescape_string(path, NULL);
        g_free(path);
    }
    else
        parse_name = g_file_get_parse_name(goto_file);

    dialog_error(parent, error, _("Could not find \"%s\""), parse_name);
    g_free(parse_name);

    g_error_free(error);
}

/**
 * thunar_history_has_back:
 * @history : a #ThunarHistory.
 *
 * Return value: TRUE if there is a backward history
 **/
gboolean history_has_back(ThunarHistory *history)
{
    e_return_val_if_fail(IS_THUNARHISTORY(history), FALSE);

    return history->back_list != NULL;
}

/**
 * thunar_file_history_peek_back:
 * @history : a #ThunarHistory.
 *
 * Returns the previous directory in the history.
 *
 * The returned #ThunarFile must be released by the caller.
 *
 * Return value: the previous #ThunarFile in the history.
 **/
ThunarFile* history_peek_back(ThunarHistory *history)
{
    ThunarFile *result = NULL;

    e_return_val_if_fail(IS_THUNARHISTORY(history), NULL);

    // pick the first(conceptually the last) file in the back list, if there are any
    if (history->back_list != NULL)
        result = th_file_get(history->back_list->data, NULL);

    return result;
}

/**
 * thunar_history_has_forward:
 * @history : a #ThunarHistory.
 *
 * Return value: TRUE if there is a forward history
 **/
gboolean history_has_forward(ThunarHistory *history)
{
    e_return_val_if_fail(IS_THUNARHISTORY(history), FALSE);

    return history->forward_list != NULL;
}

/**
 * thunar_file_history_peek_forward:
 * @history : a #ThunarHistory.
 *
 * Returns the next directory in the history. This often but not always
 * refers to a child of the current directory.
 *
 * The returned #ThunarFile must be released by the caller.
 *
 * Return value: the next #ThunarFile in the history.
 **/
ThunarFile* history_peek_forward(ThunarHistory *history)
{
    ThunarFile *result = NULL;

    e_return_val_if_fail(IS_THUNARHISTORY(history), NULL);

    // pick the first file in the forward list, if there are any
    if (history->forward_list != NULL)
        result = th_file_get(history->forward_list->data, NULL);

    return result;
}


