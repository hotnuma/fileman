/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2010      Nick Schermer <nick@xfce.org>
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
#include "appcombo.h"

#include "appchooser.h"
#include "dialogs.h"
#include "gio-ext.h"
#include "gtk-ext.h"

static void appcombo_finalize(GObject *object);
static void appcombo_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void appcombo_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);
static gboolean appcombo_scroll_event(GtkWidget *widget, GdkEventScroll *event);
static gboolean _appcombo_row_separator(GtkTreeModel *model, GtkTreeIter *iter,
                                        gpointer data);

static void _appcombo_changed(GtkComboBox *combo_box);
static void _appcombo_popup(AppCombo *chooser_button);
static void _appcombo_chooser_dialog(AppCombo *chooser_button);

static void _appcombo_file_changed(AppCombo *chooser_button, ThunarFile *file);
static gint _appcombo_sort_applications(gconstpointer a, gconstpointer b);

// AppCombo -------------------------------------------------------------------

enum
{
    PROP_0,
    PROP_FILE,
};

enum
{
    APPCOMBO_STORE_COLUMN_NAME,
    APPCOMBO_STORE_COLUMN_ICON,
    APPCOMBO_STORE_COLUMN_APPLICATION,
    APPCOMBO_STORE_COLUMN_SENSITIVE,
    APPCOMBO_STORE_COLUMN_STYLE,
    APPCOMBO_STORE_N_COLUMNS
};

struct _AppComboClass
{
    GtkComboBoxClass __parent__;
};

struct _AppCombo
{
    GtkComboBox __parent__;

    GtkListStore *store;
    ThunarFile  *file;
    gboolean    has_default_application;
};

G_DEFINE_TYPE(AppCombo, appcombo, GTK_TYPE_COMBO_BOX)

static void appcombo_class_init(AppComboClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = appcombo_finalize;
    gobject_class->get_property = appcombo_get_property;
    gobject_class->set_property = appcombo_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->scroll_event = appcombo_scroll_event;

    g_object_class_install_property(gobject_class,
                                    PROP_FILE,
                                    g_param_spec_object(
                                        "file",
                                        "file",
                                        "file",
                                        TYPE_THUNARFILE,
                                        E_PARAM_READWRITE));
}

static void appcombo_init(AppCombo *chooser_button)
{
    GtkCellRenderer *renderer;

    // allocate a new store for the combo box
    chooser_button->store = gtk_list_store_new(APPCOMBO_STORE_N_COLUMNS,
                                               G_TYPE_STRING,
                                               G_TYPE_ICON,
                                               G_TYPE_OBJECT,
                                               G_TYPE_BOOLEAN,
                                               PANGO_TYPE_STYLE);
    gtk_combo_box_set_model(GTK_COMBO_BOX(chooser_button),
                            GTK_TREE_MODEL(chooser_button->store));

    g_signal_connect(chooser_button, "changed",
                     G_CALLBACK(_appcombo_changed), NULL);
    g_signal_connect(chooser_button, "popup",
                     G_CALLBACK(_appcombo_popup), NULL);

    // set separator function
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(chooser_button),
                                         _appcombo_row_separator,
                                         NULL, NULL);

    // add renderer for the application icon
    renderer = gtk_cell_renderer_pixbuf_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(chooser_button), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(chooser_button), renderer,
                                   "gicon",
                                   APPCOMBO_STORE_COLUMN_ICON,
                                   "sensitive",
                                   APPCOMBO_STORE_COLUMN_SENSITIVE,
                                   NULL);

    // add renderer for the application name
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(chooser_button), renderer, FALSE);
    gtk_cell_layout_set_attributes(GTK_CELL_LAYOUT(chooser_button), renderer,
                                   "text",
                                   APPCOMBO_STORE_COLUMN_NAME,
                                   "sensitive",
                                   APPCOMBO_STORE_COLUMN_SENSITIVE,
                                   "style",
                                   APPCOMBO_STORE_COLUMN_STYLE,
                                   NULL);
}

static void appcombo_finalize(GObject *object)
{
    AppCombo *chooser_button = APPCOMBO(object);

    // reset the "file" property
    appcombo_set_file(chooser_button, NULL);

    // release the store
    g_object_unref(G_OBJECT(chooser_button->store));

    G_OBJECT_CLASS(appcombo_parent_class)->finalize(object);
}

static void appcombo_get_property(GObject *object, guint prop_id, GValue *value,
                                  GParamSpec *pspec)
{
    (void) pspec;

    AppCombo *chooser_button = APPCOMBO(object);

    switch (prop_id)
    {
    case PROP_FILE:
        g_value_set_object(value, chooser_button->file);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void appcombo_set_property(GObject *object, guint prop_id, const GValue *value,
                                  GParamSpec *pspec)
{
    (void) pspec;

    AppCombo *chooser_button = APPCOMBO(object);

    switch (prop_id)
    {
    case PROP_FILE:
        appcombo_set_file(chooser_button, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean appcombo_scroll_event(GtkWidget *widget, GdkEventScroll *event)
{
    AppCombo *chooser_button = APPCOMBO(widget);
    GtkTreeIter          iter;
    GObject             *application;
    GtkTreeModel        *model = GTK_TREE_MODEL(chooser_button->store);

    g_return_val_if_fail(IS_APPCOMBO(chooser_button), FALSE);

    /* check if the next application in the store is valid if we scroll down,
     * else drop the event so we don't popup the chooser dailog */
    if (event->direction != GDK_SCROLL_UP
            && gtk_combo_box_get_active_iter(GTK_COMBO_BOX(widget), &iter)
            && gtk_tree_model_iter_next(model, &iter))
    {
        gtk_tree_model_get(model, &iter,
                            APPCOMBO_STORE_COLUMN_APPLICATION,
                            &application, -1);

        if (application == NULL)
            return FALSE;

        g_object_unref(G_OBJECT(application));
    }

    return GTK_WIDGET_CLASS(appcombo_parent_class)->scroll_event(widget, event);
}

static gboolean _appcombo_row_separator(GtkTreeModel *model, GtkTreeIter *iter,
                                        gpointer data)
{
    (void) data;
    gchar *name;

    // determine the value of the "name" column
    gtk_tree_model_get(model, iter, APPCOMBO_STORE_COLUMN_NAME, &name, -1);
    if (name != NULL)
    {
        g_free(name);
        return FALSE;
    }

    return TRUE;
}

// Properties -----------------------------------------------------------------

void appcombo_set_file(AppCombo *chooser_button, ThunarFile *file)
{
    e_return_if_fail(IS_APPCOMBO(chooser_button));
    e_return_if_fail(file == NULL || IS_THUNARFILE(file));

    // check if we already use that file
    if (chooser_button->file == file)
        return;

    // disconnect from the previous file
    if (chooser_button->file != NULL)
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(chooser_button->file), _appcombo_file_changed, chooser_button);
        g_object_unref(G_OBJECT(chooser_button->file));
    }

    // activate the new file
    chooser_button->file = file;

    // connect to the new file
    if (file != NULL)
    {
        // take a reference
        g_object_ref(G_OBJECT(file));

        // stay informed about changes
        g_signal_connect_swapped(G_OBJECT(file), "changed",
                                 G_CALLBACK(_appcombo_file_changed), chooser_button);

        // update our state now
        _appcombo_file_changed(chooser_button, file);
    }

    // notify listeners
    g_object_notify(G_OBJECT(chooser_button), "file");
}

// Events ---------------------------------------------------------------------

static void _appcombo_changed(GtkComboBox *combo_box)
{
    AppCombo *chooser_button = APPCOMBO(combo_box);
    GtkTreeIter          iter;
    const gchar         *content_type;
    GAppInfo            *app_info;
    GError              *error = NULL;

    e_return_if_fail(IS_APPCOMBO(chooser_button));
    e_return_if_fail(GTK_IS_LIST_STORE(chooser_button->store));

    // verify that we still have a valid file
    if (chooser_button->file == NULL)
        return;

    // get the selected item in the combo box
    if (!gtk_combo_box_get_active_iter(combo_box, &iter))
        return;

    // determine the application that was set for the item
    gtk_tree_model_get(GTK_TREE_MODEL(chooser_button->store), &iter,
                        APPCOMBO_STORE_COLUMN_APPLICATION,
                        &app_info, -1);

    if (app_info != NULL)
    {
        // determine the mime info for the file
        content_type = th_file_get_content_type(chooser_button->file);

        // try to set application as default for these kind of file
        if (!g_app_info_set_as_default_for_type(app_info, content_type, &error))
        {
            // tell the user that it didn't work
            dialog_error(GTK_WIDGET(chooser_button), error,
                                       _("Failed to set default application for \"%s\""),
                                       th_file_get_display_name(chooser_button->file));
            g_error_free(error);
        }
        else
        {
            // emit "changed" on the file, so everybody updates its state
            th_file_changed(chooser_button->file);
        }

        // release the application
        g_object_unref(app_info);
    }
    else
    {
        // no application was found in the store, looks like the other... option
        _appcombo_chooser_dialog(chooser_button);
    }
}

static void _appcombo_popup(AppCombo *chooser_button)
{
    e_return_if_fail(IS_APPCOMBO(chooser_button));

    if (!chooser_button->has_default_application)
    {
        // don't show the menu
        gtk_combo_box_popdown(GTK_COMBO_BOX(chooser_button));

        // open the chooser dialog if the filetype has no default action
        _appcombo_chooser_dialog(chooser_button);
    }
}

static void _appcombo_chooser_dialog(AppCombo *chooser_button)
{
    GtkWidget *toplevel;
    GtkWidget *dialog;

    e_return_if_fail(IS_APPCOMBO(chooser_button));

    // determine the toplevel window for the chooser
    toplevel = gtk_widget_get_toplevel(GTK_WIDGET(chooser_button));
    if (toplevel == NULL)
        return;

    // popup the application chooser dialog
    dialog = g_object_new(TYPE_APPCHOOSERDIALOG, "open", FALSE, NULL);

    //exo_binding_new(G_OBJECT(chooser_button), "file",
    //                G_OBJECT(dialog), "file");

    g_object_bind_property(G_OBJECT(chooser_button),
                           "file",
                           G_OBJECT (dialog),
                           "file",
                           G_BINDING_SYNC_CREATE);

    gtk_window_set_transient_for(GTK_WINDOW(dialog), GTK_WINDOW(toplevel));
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);

    if (gtk_dialog_run(GTK_DIALOG(dialog)) != GTK_RESPONSE_ACCEPT)
        _appcombo_file_changed(chooser_button, chooser_button->file);

    gtk_widget_destroy(dialog);
}

static void _appcombo_file_changed(AppCombo *chooser_button, ThunarFile *file)
{
    e_return_if_fail(IS_APPCOMBO(chooser_button));
    e_return_if_fail(chooser_button->file == file);
    e_return_if_fail(IS_THUNARFILE(file));

        // clear the store
    gtk_list_store_clear(chooser_button->store);

    // reset the default application flag
    chooser_button->has_default_application = FALSE;

    // block the changed signal for a moment
    g_signal_handlers_block_by_func(chooser_button,
                                     _appcombo_changed,
                                     NULL);

    GtkTreeIter  iter;
    guint        i = 0;

    // determine the content type of the file
    const gchar *content_type;
    content_type = th_file_get_content_type(file);

    if (content_type != NULL)
    {
        GAppInfo    *app_info;
        GList       *app_infos;
        gchar       *description;

        // setup a useful tooltip for the button
        description = g_content_type_get_description(content_type);
        etk_widget_set_tooltip(GTK_WIDGET(chooser_button),
                                       _("The selected application is used to open "
                                         "this and other files of type \"%s\"."),
                                       description);
        g_free(description);

        // determine the default application for that content type
        app_info = g_app_info_get_default_for_type(content_type, FALSE);
        if (app_info != NULL)
        {
            // determine all applications that claim to be able to handle the file
            app_infos = g_app_info_get_all_for_type(content_type);
            app_infos = g_list_sort(app_infos, _appcombo_sort_applications);

            GList       *lp;

            // add all possible applications
            for (lp = app_infos, i = 0; lp != NULL; lp = lp->next, ++i)
            {
                if (e_app_info_should_show(lp->data))
                {
                    // insert the item into the store
                    gtk_list_store_insert_with_values(chooser_button->store, &iter, i,
                                                       APPCOMBO_STORE_COLUMN_NAME,
                                                       g_app_info_get_name(lp->data),
                                                       APPCOMBO_STORE_COLUMN_APPLICATION,
                                                       lp->data,
                                                       APPCOMBO_STORE_COLUMN_ICON,
                                                       g_app_info_get_icon(lp->data),
                                                       APPCOMBO_STORE_COLUMN_SENSITIVE,
                                                       TRUE,
                                                       -1);

                    // pre-select the default application
                    if (g_app_info_equal(lp->data, app_info))
                        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(chooser_button), &iter);
                }

                // release the application
                g_object_unref(lp->data);
            }

            // release the application list
            g_list_free(app_infos);

            // release the default application
            g_object_unref(app_info);

            // assume we have some applications in the list
            chooser_button->has_default_application = TRUE;
        }
    }

    if (content_type == NULL || !chooser_button->has_default_application)
    {
        // add the "No application selected" item and set as active
        gtk_list_store_insert_with_values(chooser_button->store, &iter, 0,
                                           APPCOMBO_STORE_COLUMN_NAME,
                                           _("No application selected"),
                                           APPCOMBO_STORE_COLUMN_STYLE,
                                           PANGO_STYLE_ITALIC,
                                           -1);
        gtk_combo_box_set_active_iter(GTK_COMBO_BOX(chooser_button), &iter);
    }

    // insert empty row that will appear as a separator
    gtk_list_store_insert_with_values(chooser_button->store, NULL, ++i, -1);

    // add the "Other Application..." option
    gtk_list_store_insert_with_values(chooser_button->store, NULL, ++i,
                                       APPCOMBO_STORE_COLUMN_NAME,
                                       _("Other Application..."),
                                       APPCOMBO_STORE_COLUMN_SENSITIVE,
                                       TRUE,
                                       -1);

    // unblock the changed signal
    g_signal_handlers_unblock_by_func(chooser_button,
                                       _appcombo_changed,
                                       NULL);
}

static gint _appcombo_sort_applications(gconstpointer a, gconstpointer b)
{
    e_return_val_if_fail(G_IS_APP_INFO(a), -1);
    e_return_val_if_fail(G_IS_APP_INFO(b), -1);

    return g_utf8_collate(g_app_info_get_name(G_APP_INFO(a)),
                           g_app_info_get_name(G_APP_INFO(b)));
}

// Public ---------------------------------------------------------------------

GtkWidget* appcombo_new()
{
    return g_object_new(TYPE_APPCOMBO, NULL);
}


