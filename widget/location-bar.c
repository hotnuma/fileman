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

#include <location-bar.h>
#include <navigator.h>
#include <location-entry.h>

struct _ThunarLocationBarClass
{
    GtkBinClass __parent__;

    /* signals */
    void(*reload_requested)();
    void(*entry_done)();
};

struct _ThunarLocationBar
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

static void thunar_location_bar_navigator_init(ThunarNavigatorIface *iface);
static void thunar_location_bar_finalize(GObject *object);
static void thunar_location_bar_get_property(GObject *object,
                                             guint prop_id,
                                             GValue *value,
                                             GParamSpec *pspec);
static void thunar_location_bar_set_property(GObject *object,
                                             guint prop_id,
                                             const GValue *value,
                                             GParamSpec *pspec);
static ThunarFile* thunar_location_bar_get_current_directory(ThunarNavigator *navigator);
static void thunar_location_bar_set_current_directory(ThunarNavigator *navigator,
                                                      ThunarFile *current_directory);
static GtkWidget* thunar_location_bar_install_widget(ThunarLocationBar *bar,
                                                     GType type);
static void thunar_location_bar_reload_requested(ThunarLocationBar *bar);
static gboolean thunar_location_bar_settings_changed(ThunarLocationBar *bar);
static void thunar_location_bar_on_enry_edit_done(ThunarLocationEntry *entry,
                                                  ThunarLocationBar *bar);

G_DEFINE_TYPE_WITH_CODE(ThunarLocationBar, thunar_location_bar, GTK_TYPE_BIN,
                        G_IMPLEMENT_INTERFACE(THUNAR_TYPE_NAVIGATOR,
                                              thunar_location_bar_navigator_init));

GtkWidget* thunar_location_bar_new()
{
    return gtk_widget_new(THUNAR_TYPE_LOCATION_BAR, NULL);
}

static void thunar_location_bar_class_init(ThunarLocationBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->get_property = thunar_location_bar_get_property;
    gobject_class->set_property = thunar_location_bar_set_property;
    gobject_class->finalize = thunar_location_bar_finalize;

    klass->reload_requested = e_noop;

    /* Override ThunarNavigator's properties */
    g_object_class_override_property(gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

    /* install signals */

    /**
     * ThunarLocationBar::reload-requested:
     * @location_bar : a #ThunarLocationBar.
     *
     * Emitted by @location_bar whenever the user clicked a "reload" button
     **/
    g_signal_new("reload-requested",
                  G_TYPE_FROM_CLASS(klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET(ThunarLocationBarClass, reload_requested),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);

    /**
     * ThunarLocationBar::entry-done:
     * @location_bar : a #ThunarLocationBar.
     *
     * Emitted by @location_bar exactly once after an entry has been requested using
     * #thunar_location_bar_request_entry and the user has finished editing the entry.
     **/
    g_signal_new("entry-done",
                  G_TYPE_FROM_CLASS(klass),
                  G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                  G_STRUCT_OFFSET(ThunarLocationBarClass, entry_done),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}

static void thunar_location_bar_init(ThunarLocationBar *bar)
{
    bar->current_directory = NULL;
    bar->locationEntry = NULL;

    thunar_location_bar_settings_changed(bar);
}

static void thunar_location_bar_finalize(GObject *object)
{
    ThunarLocationBar *bar = THUNAR_LOCATION_BAR(object);

    e_return_if_fail(THUNAR_IS_LOCATION_BAR(bar));

    if (bar->locationEntry)
        g_object_unref(bar->locationEntry);

    /* release from the current_directory */
    navigator_set_current_directory(THUNAR_NAVIGATOR(bar), NULL);

   (*G_OBJECT_CLASS(thunar_location_bar_parent_class)->finalize)(object);
}

static void thunar_location_bar_navigator_init(ThunarNavigatorIface *iface)
{
    iface->set_current_directory = thunar_location_bar_set_current_directory;
    iface->get_current_directory = thunar_location_bar_get_current_directory;
}

static void thunar_location_bar_get_property(GObject              *object,
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

static void thunar_location_bar_set_property(GObject              *object,
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

static ThunarFile* thunar_location_bar_get_current_directory(ThunarNavigator *navigator)
{
    return THUNAR_LOCATION_BAR(navigator)->current_directory;
}

static void thunar_location_bar_set_current_directory(ThunarNavigator *navigator,
                                                      ThunarFile      *current_directory)
{
    ThunarLocationBar *bar = THUNAR_LOCATION_BAR(navigator);
    GtkWidget         *child;

    if (bar->current_directory) g_object_unref(bar->current_directory);
    bar->current_directory = current_directory;

    if (current_directory) g_object_ref(current_directory);

    if ((child = gtk_bin_get_child(GTK_BIN(bar))) && THUNAR_IS_NAVIGATOR(child))
        navigator_set_current_directory(THUNAR_NAVIGATOR(child), current_directory);

    g_object_notify(G_OBJECT(bar), "current-directory");
}

static void thunar_location_bar_reload_requested(ThunarLocationBar *bar)
{
    g_signal_emit_by_name(bar, "reload-requested");
}

static GtkWidget* thunar_location_bar_install_widget(ThunarLocationBar    *bar,
                                                     GType                 type)
{
    GtkWidget *installedWidget = NULL;
    GtkWidget *child;

    /* check if the the right type is already installed */
    if ((child = gtk_bin_get_child(GTK_BIN(bar))) && G_TYPE_CHECK_INSTANCE_TYPE(child, type))
        return child;

    if (type == THUNAR_TYPE_LOCATION_ENTRY)
    {
        if (bar->locationEntry == NULL)
        {
            bar->locationEntry = gtk_widget_new(THUNAR_TYPE_LOCATION_ENTRY, "current-directory", NULL, NULL);
            g_object_ref(bar->locationEntry);
            g_signal_connect_swapped(bar->locationEntry, "reload-requested", G_CALLBACK(thunar_location_bar_reload_requested), bar);
            g_signal_connect_swapped(bar->locationEntry, "change-directory", G_CALLBACK(navigator_change_directory), THUNAR_NAVIGATOR(bar));
        }
        installedWidget = bar->locationEntry;
    }

    navigator_set_current_directory(THUNAR_NAVIGATOR(installedWidget), bar->current_directory);

    if ((child = gtk_bin_get_child(GTK_BIN(bar))))
        gtk_container_remove(GTK_CONTAINER(bar), child);

    gtk_container_add(GTK_CONTAINER(bar), installedWidget);
    gtk_widget_show(installedWidget);

    return installedWidget;
}

static void thunar_location_bar_on_enry_edit_done(ThunarLocationEntry *entry,
                                                  ThunarLocationBar   *bar)
{
    g_signal_handlers_disconnect_by_func(entry, thunar_location_bar_on_enry_edit_done, bar);

    g_object_ref(bar);
    g_idle_add_full(G_PRIORITY_HIGH_IDLE,(GSourceFunc)thunar_location_bar_settings_changed, bar, g_object_unref);

    g_signal_emit_by_name(bar, "entry-done");
}

/**
 * thunar_location_bar_request_entry
 * @bar          : The #ThunarLocationBar
 * @initial_text : The initial text to be placed inside the entry, or NULL to
 *                 use the path of the current directory.
 *
 * Makes the location bar display an entry with the given text and places the cursor
 * accordingly. If the currently displayed location widget is a path bar, it will be
 * temporarily swapped for an entry widget and swapped back once the user completed
 *(or aborted) the input.
 */
void thunar_location_bar_request_entry(ThunarLocationBar *bar, const gchar *initial_text)
{
    GtkWidget *child;

    child = gtk_bin_get_child(GTK_BIN(bar));

    e_return_if_fail(child != NULL && GTK_IS_WIDGET(child));

    if (THUNAR_IS_LOCATION_ENTRY(child))
    {
        /* already have an entry */
        thunar_location_entry_accept_focus(THUNAR_LOCATION_ENTRY(child), initial_text);
    }
    else
    {
        /* not an entry => temporarily replace it */
        child = thunar_location_bar_install_widget(bar, THUNAR_TYPE_LOCATION_ENTRY);
        thunar_location_entry_accept_focus(THUNAR_LOCATION_ENTRY(child), initial_text);
    }

    g_signal_connect(child, "edit-done", G_CALLBACK(thunar_location_bar_on_enry_edit_done), bar);
}

static gboolean thunar_location_bar_settings_changed(ThunarLocationBar *bar)
{
    thunar_location_bar_install_widget(bar, THUNAR_TYPE_LOCATION_ENTRY);

    return FALSE;
}


