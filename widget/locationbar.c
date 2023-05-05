/*-
 * Copyright(c) 2015 Jonas Kümmerlin <rgcjonas@gmail.com>
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

#include <exo.h>

#include <locationbar.h>
#include <navigator.h>
#include <locationentry.h>

struct _LocationBarClass
{
    GtkBinClass __parent__;

    /* signals */
    void(*reload_requested)();
    void(*entry_done)();
};

struct _LocationBar
{
    GtkBin __parent__;

    ThunarFile *current_directory;

    GtkWidget  *locationEntry;
};

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
};

static void locbar_navigator_init(ThunarNavigatorIface *iface);
static void locbar_finalize(GObject *object);
static void locbar_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void locbar_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static ThunarFile* locbar_get_current_directory(ThunarNavigator *navigator);
static void locbar_set_current_directory(ThunarNavigator *navigator,
                                                      ThunarFile *current_directory);
static GtkWidget* locbar_install_widget(LocationBar *bar,
                                                     GType type);
static void locbar_reload_requested(LocationBar *bar);
static gboolean locbar_settings_changed(LocationBar *bar);
static void locbar_on_enry_edit_done(LocationEntry *entry,
                                                  LocationBar *bar);

G_DEFINE_TYPE_WITH_CODE(LocationBar, locbar, GTK_TYPE_BIN,
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_NAVIGATOR,
                                              locbar_navigator_init));

GtkWidget* locbar_new()
{
    return gtk_widget_new(TYPE_LOCATIONBAR, NULL);
}

static void locbar_class_init(LocationBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->get_property = locbar_get_property;
    gobject_class->set_property = locbar_set_property;
    gobject_class->finalize = locbar_finalize;

    klass->reload_requested = e_noop;

    /* Override ThunarNavigator's properties */
    g_object_class_override_property(gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

    /* install signals */

    /**
     * LocationBar::reload-requested:
     * @location_bar : a #LocationBar.
     *
     * Emitted by @location_bar whenever the user clicked a "reload" button
     **/
    g_signal_new("reload-requested",
                  G_TYPE_FROM_CLASS(klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET(LocationBarClass, reload_requested),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

    /**
     * LocationBar::entry-done:
     * @location_bar : a #LocationBar.
     *
     * Emitted by @location_bar exactly once after an entry has been requested using
     * #locbar_request_entry and the user has finished editing the entry.
     **/
    g_signal_new("entry-done",
                  G_TYPE_FROM_CLASS(klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET(LocationBarClass, entry_done),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void locbar_init(LocationBar *bar)
{
    bar->current_directory = NULL;
    bar->locationEntry = NULL;

    locbar_settings_changed(bar);
}

static void locbar_finalize(GObject *object)
{
    LocationBar *bar = LOCATIONBAR(object);

    e_return_if_fail(IS_LOCATIONBAR(bar));

    if (bar->locationEntry)
        g_object_unref(bar->locationEntry);

    /* release from the current_directory */
    navigator_set_current_directory(THUNAR_NAVIGATOR(bar), NULL);

   (*G_OBJECT_CLASS(locbar_parent_class)->finalize)(object);
}

static void locbar_navigator_init(ThunarNavigatorIface *iface)
{
    iface->set_current_directory = locbar_set_current_directory;
    iface->get_current_directory = locbar_get_current_directory;
}

static void locbar_get_property(GObject              *object,
                                             guint                 prop_id,
                                             GValue               *value,
                                             GParamSpec           *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, navigator_get_current_directory(THUNAR_NAVIGATOR(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void locbar_set_property(GObject              *object,
                                             guint                 prop_id,
                                             const GValue         *value,
                                             GParamSpec           *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNAR_NAVIGATOR(object), g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static ThunarFile* locbar_get_current_directory(ThunarNavigator *navigator)
{
    return LOCATIONBAR(navigator)->current_directory;
}

static void locbar_set_current_directory(ThunarNavigator *navigator,
                                                      ThunarFile      *current_directory)
{
    LocationBar *bar = LOCATIONBAR(navigator);
    GtkWidget         *child;

    if (bar->current_directory) g_object_unref(bar->current_directory);
    bar->current_directory = current_directory;

    if (current_directory) g_object_ref(current_directory);

    if ((child = gtk_bin_get_child(GTK_BIN(bar))) && THUNAR_IS_NAVIGATOR(child))
        navigator_set_current_directory(THUNAR_NAVIGATOR(child), current_directory);

    g_object_notify(G_OBJECT(bar), "current-directory");
}

static void locbar_reload_requested(LocationBar *bar)
{
    g_signal_emit_by_name(bar, "reload-requested");
}

static GtkWidget* locbar_install_widget(LocationBar *bar,
                                                     GType             type)
{
    GtkWidget *installedWidget = NULL;
    GtkWidget *child;

    /* check if the the right type is already installed */
    if ((child = gtk_bin_get_child(GTK_BIN(bar)))
        && G_TYPE_CHECK_INSTANCE_TYPE(child, type))
        return child;

    if (type == TYPE_LOCATIONENTRY)
    {
        if (bar->locationEntry == NULL)
        {
            bar->locationEntry = gtk_widget_new(
                                TYPE_LOCATIONENTRY,
                                "orientation", GTK_ORIENTATION_HORIZONTAL,
                                "current-directory", NULL,
                                NULL);

            g_object_ref(bar->locationEntry);

            g_signal_connect_swapped(bar->locationEntry, "reload-requested",
                                     G_CALLBACK(locbar_reload_requested), bar);

            g_signal_connect_swapped(bar->locationEntry, "change-directory",
                                     G_CALLBACK(navigator_change_directory), THUNAR_NAVIGATOR(bar));
        }

        installedWidget = bar->locationEntry;
    }

    navigator_set_current_directory(THUNAR_NAVIGATOR(installedWidget),
                                    bar->current_directory);

    if ((child = gtk_bin_get_child(GTK_BIN(bar))))
        gtk_container_remove(GTK_CONTAINER(bar), child);

    gtk_container_add(GTK_CONTAINER(bar), installedWidget);
    gtk_widget_show(installedWidget);

    return installedWidget;
}

static void locbar_on_enry_edit_done(LocationEntry *entry,
                                                  LocationBar   *bar)
{
    g_signal_handlers_disconnect_by_func(entry, locbar_on_enry_edit_done, bar);

    g_object_ref(bar);
    g_idle_add_full(G_PRIORITY_HIGH_IDLE,(GSourceFunc)locbar_settings_changed, bar, g_object_unref);

    g_signal_emit_by_name(bar, "entry-done");
}

/**
 * locbar_request_entry
 * @bar          : The #LocationBar
 * @initial_text : The initial text to be placed inside the entry, or NULL to
 *                 use the path of the current directory.
 *
 * Makes the location bar display an entry with the given text and places the cursor
 * accordingly. If the currently displayed location widget is a path bar, it will be
 * temporarily swapped for an entry widget and swapped back once the user completed
 *(or aborted) the input.
 */
void locbar_request_entry(LocationBar *bar, const gchar *initial_text)
{
    GtkWidget *child;

    child = gtk_bin_get_child(GTK_BIN(bar));

    e_return_if_fail(child != NULL && GTK_IS_WIDGET(child));

    if (IS_LOCATIONENTRY(child))
    {
        /* already have an entry */
        locentry_accept_focus(LOCATIONENTRY(child), initial_text);
    }
    else
    {
        /* not an entry => temporarily replace it */
        child = locbar_install_widget(bar,
                                                   TYPE_LOCATIONENTRY);

        locentry_accept_focus(LOCATIONENTRY(child),
                                           initial_text);
    }

    g_signal_connect(child, "edit-done", G_CALLBACK(locbar_on_enry_edit_done), bar);
}

static gboolean locbar_settings_changed(LocationBar *bar)
{
    locbar_install_widget(bar, TYPE_LOCATIONENTRY);

    return FALSE;
}


