/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include "appmodel.h"
#include "config.h"
#include "etktype.h"
#include "gio-ext.h"

static void appmodel_constructed(GObject *object);
static void appmodel_finalize(GObject *object);
static void appmodel_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static const gchar* _appmodel_get_content_type(AppChooserModel *model);
static void appmodel_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);

static void _appmodel_load(AppChooserModel *model, GtkWidget *widget);
static gint _sort_app_infos(gconstpointer a, gconstpointer b);
static gint _compare_app_infos(gconstpointer a, gconstpointer b);
static void _appmodel_append(AppChooserModel *model, const gchar *title,
                             const gchar *icon_name, GList *app_infos);

static GdkPixbuf* _pixbuf_get_default(GtkWidget *widget, const gchar *id);
static GdkPixbuf* _pixbuf_from_gicon(GtkWidget *widget, GIcon *gicon,
                                     const gchar *id);

// AppChooserModel ------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CONTENT_TYPE,
};

struct _AppChooserModelClass
{
    GtkTreeStoreClass __parent__;
};

struct _AppChooserModel
{
    GtkTreeStore __parent__;

    gchar *content_type;
};

G_DEFINE_TYPE(AppChooserModel, appmodel, GTK_TYPE_TREE_STORE)

static void appmodel_class_init(AppChooserModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = appmodel_finalize;
    gobject_class->constructed = appmodel_constructed;
    gobject_class->get_property = appmodel_get_property;
    gobject_class->set_property = appmodel_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_CONTENT_TYPE,
                                    g_param_spec_string("content-type",
                                                        "content-type",
                                                        "content-type",
                                                        NULL,
                                                        G_PARAM_CONSTRUCT_ONLY
                                                        | E_PARAM_READWRITE));
}

static void appmodel_init(AppChooserModel *model)
{
    // allocate the types array for the columns
    GType column_types[APPCHOOSER_N_COLUMNS] =
    {
        G_TYPE_STRING,
        G_TYPE_ICON,
        G_TYPE_APP_INFO,
        PANGO_TYPE_STYLE,
        PANGO_TYPE_WEIGHT,
    };

    // register the column types
    gtk_tree_store_set_column_types(GTK_TREE_STORE(model),
                                    G_N_ELEMENTS(column_types),
                                    column_types);
}

static void appmodel_constructed(GObject *object)
{
    AppChooserModel *model = APPCHOOSER_MODEL(object);

    // start to load the applications installed on the system
    _appmodel_load(model, NULL);
}

static void appmodel_finalize(GObject *object)
{
    AppChooserModel *model = APPCHOOSER_MODEL(object);

    // free the content type
    g_free(model->content_type);

    G_OBJECT_CLASS(appmodel_parent_class)->finalize(object);
}

static void appmodel_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    AppChooserModel *model = APPCHOOSER_MODEL(object);

    switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
        g_value_set_string(value, _appmodel_get_content_type(model));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static const gchar* _appmodel_get_content_type(AppChooserModel *model)
{
    e_return_val_if_fail(IS_APPCHOOSER_MODEL(model), NULL);

    return model->content_type;
}

static void appmodel_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    AppChooserModel *model = APPCHOOSER_MODEL(object);

    switch (prop_id)
    {
    case PROP_CONTENT_TYPE:
        model->content_type = g_value_dup_string(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


// public ---------------------------------------------------------------------

AppChooserModel* appmodel_new(const gchar *content_type)
{
    return g_object_new(TYPE_APPCHOOSER_MODEL,
                        "content-type", content_type,
                        NULL);
}

gboolean appmodel_remove(AppChooserModel *model,
                         GtkTreeIter *iter, GError **error)
{
    e_return_val_if_fail(IS_APPCHOOSER_MODEL(model), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    e_return_val_if_fail(gtk_tree_store_iter_is_valid(GTK_TREE_STORE(model),
                                                      iter), FALSE);

    GAppInfo *app_info;
    gboolean succeed;

    // determine the app info for the iter
    gtk_tree_model_get(GTK_TREE_MODEL(model),
                       iter,
                       APPCHOOSER_COLUMN_APPLICATION, &app_info,
                       -1);

    if (app_info == NULL)
        return TRUE;

    // try to remove support for this content type
    succeed = g_app_info_remove_supports_type(app_info,
                                              model->content_type,
                                              error);

    // try to delete the file
    if (succeed && g_app_info_delete(app_info))
    {
        g_set_error(error,
                    G_IO_ERROR,
                    G_IO_ERROR_FAILED,
                    _("Failed to remove \"%s\"."),
                    g_app_info_get_id(app_info));
    }

    // clean up
    g_object_unref(app_info);

    // if the removal was successfull, delete the row from the model
    if (succeed)
        gtk_tree_store_remove(GTK_TREE_STORE(model), iter);

    return succeed;
}


// ----------------------------------------------------------------------------

static void _appmodel_load(AppChooserModel *model, GtkWidget *widget)
{
    (void) widget;

    e_return_if_fail(IS_APPCHOOSER_MODEL(model));
    e_return_if_fail(model->content_type != NULL);

    gtk_tree_store_clear(GTK_TREE_STORE(model));

    // check if we have any applications for this type

    GList *recommended = g_app_info_get_all_for_type(model->content_type);

    // append them as recommended
    recommended = g_list_sort(recommended, _sort_app_infos);
    _appmodel_append(model,
                     _("Recommended Applications"),
                     "preferences-desktop-default-applications",
                     recommended);

    GList *other = NULL;

    GList *all = g_app_info_get_all();
    for (GList *lp = all; lp != NULL; lp = lp->next)
    {
        if (g_list_find_custom(recommended,
                               lp->data,
                               _compare_app_infos) == NULL)
        {
            other = g_list_prepend(other, lp->data);
        }
    }

    // append the other applications
    other = g_list_sort(other, _sort_app_infos);

    _appmodel_append(model,
                     _("Other Applications"),
                     "preferences-desktop-default-applications",
                     other);

    g_list_free_full(recommended, g_object_unref);
    g_list_free_full(all, g_object_unref);

    g_list_free(other);
}

static gint _sort_app_infos(gconstpointer a, gconstpointer b)
{
    return g_utf8_collate(g_app_info_get_name(G_APP_INFO(a)),
                          g_app_info_get_name(G_APP_INFO(b)));
}

static gint _compare_app_infos(gconstpointer a, gconstpointer b)
{
    return g_app_info_equal(G_APP_INFO(a), G_APP_INFO(b)) ? 0 : 1;
}

static void _appmodel_append(AppChooserModel *model, const gchar *title,
                             const gchar *icon_name, GList *app_infos)
{
    e_return_if_fail(IS_APPCHOOSER_MODEL(model));
    e_return_if_fail(title != NULL);
    e_return_if_fail(icon_name != NULL);

    c_autounref GIcon *icon = g_themed_icon_new(icon_name);
    GdkPixbuf *pix = _pixbuf_from_gicon(NULL, icon, "cat");

    GtkTreeIter parent_iter;
    gtk_tree_store_append(GTK_TREE_STORE(model), &parent_iter, NULL);
    gtk_tree_store_set(GTK_TREE_STORE(model), &parent_iter,
                       APPCHOOSER_COLUMN_NAME, title,
                       APPCHOOSER_COLUMN_ICON, pix,
                       APPCHOOSER_COLUMN_WEIGHT, PANGO_WEIGHT_BOLD,
                       -1);

    g_object_unref(pix);

    gboolean    inserted_infos = FALSE;
    GtkTreeIter child_iter;

    if (app_infos != NULL)
    {
        // insert the program items
        for (GList *lp = app_infos; lp != NULL; lp = lp->next)
        {
            if (!e_app_info_should_show(lp->data))
                continue;

            pix = _pixbuf_from_gicon(NULL,
                                     g_app_info_get_icon(lp->data),
                                     g_app_info_get_id(lp->data));

            // append the tree row with the program data
            gtk_tree_store_append(GTK_TREE_STORE(model),
                                  &child_iter, &parent_iter);
            gtk_tree_store_set(
                    GTK_TREE_STORE(model), &child_iter,
                    APPCHOOSER_COLUMN_NAME, g_app_info_get_name(lp->data),
                    APPCHOOSER_COLUMN_ICON, pix,
                    APPCHOOSER_COLUMN_APPLICATION, lp->data,
                    APPCHOOSER_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                    -1);

            g_object_unref(pix);

            inserted_infos = TRUE;
        }
    }

    if (!inserted_infos)
    {
        // tell the user that we don't have any applications for this category
        gtk_tree_store_append(GTK_TREE_STORE(model),
                              &child_iter, &parent_iter);
        gtk_tree_store_set(GTK_TREE_STORE(model), &child_iter,
                           APPCHOOSER_COLUMN_NAME, _("None available"),
                           APPCHOOSER_COLUMN_STYLE, PANGO_STYLE_ITALIC,
                           APPCHOOSER_COLUMN_WEIGHT, PANGO_WEIGHT_NORMAL,
                           -1);
    }
}

static GdkPixbuf* _pixbuf_from_gicon(GtkWidget *widget, GIcon *gicon,
                                     const gchar *id)
{
    if (!gicon)
    {
        //g_print("_pixbuf_from_gicon : %s : gicon = null\n", id);

        return _pixbuf_get_default(widget, id);
    }

    GtkIconTheme *icon_theme;
    gint requested_icon_size = 24;

    if (!widget)
    {
        icon_theme = gtk_icon_theme_get_default();
    }
    else
    {
        icon_theme =
                gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
        gint scale_factor = gtk_widget_get_scale_factor(widget);
        requested_icon_size = 24 * scale_factor;
    }

    c_autounref GtkIconInfo *icon_info =
        gtk_icon_theme_lookup_by_gicon(icon_theme,
                                       gicon,
                                       requested_icon_size,
                                       GTK_ICON_LOOKUP_USE_BUILTIN
                                       | GTK_ICON_LOOKUP_FORCE_SIZE);

    if (icon_info == NULL)
    {
        //g_print("_pixbuf_from_gicon : %s : icon_info = null\n", id);

        return _pixbuf_get_default(widget, id);
    }

    return gtk_icon_info_load_icon(icon_info, NULL);
}

static GdkPixbuf* _pixbuf_get_default(GtkWidget *widget, const gchar *id)
{
    (void) id;

    GtkIconTheme *icon_theme;
    gint requested_icon_size = 24;

    if (!widget)
    {
        icon_theme = gtk_icon_theme_get_default();
    }
    else
    {
        icon_theme =
                gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
        gint scale_factor = gtk_widget_get_scale_factor(widget);
        requested_icon_size = 24 * scale_factor;
    }

    c_autounref GtkIconInfo *icon_info =
        gtk_icon_theme_lookup_icon(icon_theme,
                                   "application-x-executable",
                                   requested_icon_size,
                                   GTK_ICON_LOOKUP_USE_BUILTIN
                                   | GTK_ICON_LOOKUP_FORCE_SIZE);

    if (icon_info == NULL)
    {
        //g_print("_pixbuf_get_default : %s : icon_info = null\n", id);

        return NULL;
    }

    return gtk_icon_info_load_icon(icon_info, NULL);
}


