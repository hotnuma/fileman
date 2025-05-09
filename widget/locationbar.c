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

#include "config.h"
#include "locationbar.h"

#include "locationentry.h"
#include "navigator.h"

static void _locbar_noop();

// LocationBar ----------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
};

struct _LocationBar
{
    GtkBin      __parent__;

    ThunarFile  *current_directory;
    GtkWidget   *locationEntry;
};

struct _LocationBarClass
{
    GtkBinClass __parent__;

    // signals
    void (*reload_requested) ();
    void (*entry_done) ();
};

static void locbar_navigator_init(ThunarNavigatorIface *iface);
static void locbar_finalize(GObject *object);
static void locbar_get_property(GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec);
static void locbar_set_property(GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec);
static ThunarFile* locbar_get_current_directory(ThunarNavigator *navigator);
static void locbar_set_current_directory(ThunarNavigator *navigator,
                                         ThunarFile *current_directory);

static gboolean _locbar_settings_changed(LocationBar *bar);
static GtkWidget* _locbar_install_widget(LocationBar *bar, GType type);
static void _locbar_reload_requested(LocationBar *bar);

static void locbar_on_entry_edit_done(LocationEntry *entry, LocationBar *bar);

static void _locbar_noop()
{
    //g_print("_locbar_noop\n");
}

G_DEFINE_TYPE_WITH_CODE(LocationBar, locbar, GTK_TYPE_BIN,
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARNAVIGATOR,
                                              locbar_navigator_init))

// creation / destruction -----------------------------------------------------

GtkWidget* locbar_new()
{
    return gtk_widget_new(TYPE_LOCATIONBAR, NULL);
}

static void locbar_class_init(LocationBarClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = locbar_finalize;
    gobject_class->get_property = locbar_get_property;
    gobject_class->set_property = locbar_set_property;

    klass->reload_requested = _locbar_noop;

    // Override ThunarNavigator's properties
    g_object_class_override_property(gobject_class,
                                     PROP_CURRENT_DIRECTORY,
                                     "current-directory");

    g_signal_new("reload-requested",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET(LocationBarClass, reload_requested),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE, 0);

    // Emitted exactly once after an entry has been requested using
    // locbar_request_entry and the user has finished editing the entry.
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

    _locbar_settings_changed(bar);
}

static void locbar_finalize(GObject *object)
{
    LocationBar *bar = LOCATIONBAR(object);
    e_return_if_fail(IS_LOCATIONBAR(bar));

    if (bar->locationEntry)
        g_object_unref(bar->locationEntry);

    // release from the current_directory
    navigator_set_current_directory(THUNARNAVIGATOR(bar), NULL);

    G_OBJECT_CLASS(locbar_parent_class)->finalize(object);
}

static void locbar_navigator_init(ThunarNavigatorIface *iface)
{
    iface->set_current_directory = locbar_set_current_directory;
    iface->get_current_directory = locbar_get_current_directory;
}

static void locbar_get_property(GObject *object, guint prop_id,
                                GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value,
                           navigator_get_current_directory(
                                            THUNARNAVIGATOR(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void locbar_set_property(GObject *object, guint prop_id,
                                const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNARNAVIGATOR(object),
                                        g_value_get_object(value));
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
                                         ThunarFile *current_directory)
{
    LocationBar *bar = LOCATIONBAR(navigator);

    if (bar->current_directory)
        g_object_unref(bar->current_directory);

    bar->current_directory = current_directory;

    if (current_directory) g_object_ref(current_directory);

    GtkWidget *child = gtk_bin_get_child(GTK_BIN(bar));

    if (child && IS_THUNARNAVIGATOR(child))
        navigator_set_current_directory(THUNARNAVIGATOR(child),
                                        current_directory);

    g_object_notify(G_OBJECT(bar), "current-directory");
}

static gboolean _locbar_settings_changed(LocationBar *bar)
{
    _locbar_install_widget(bar, TYPE_LOCATIONENTRY);

    return FALSE;
}

static GtkWidget* _locbar_install_widget(LocationBar *bar, GType type)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(bar));

    // check if the the right type is already installed
    if (child && G_TYPE_CHECK_INSTANCE_TYPE(child, type))
        return child;

    GtkWidget *widget = NULL;

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
                                     G_CALLBACK(_locbar_reload_requested),
                                     bar);

            g_signal_connect_swapped(bar->locationEntry, "change-directory",
                                     G_CALLBACK(navigator_change_directory),
                                     THUNARNAVIGATOR(bar));
        }

        widget = bar->locationEntry;
    }

    navigator_set_current_directory(THUNARNAVIGATOR(widget),
                                    bar->current_directory);

    child = gtk_bin_get_child(GTK_BIN(bar));

    if (child)
        gtk_container_remove(GTK_CONTAINER(bar), child);

    gtk_container_add(GTK_CONTAINER(bar), widget);
    gtk_widget_show(widget);

    return widget;
}


// events ---------------------------------------------------------------------

static void _locbar_reload_requested(LocationBar *bar)
{
    g_signal_emit_by_name(bar, "reload-requested");
}


// public ---------------------------------------------------------------------

/* Makes the location bar display an entry with the given text and places
 * the cursor accordingly. If the currently displayed location widget is
 * a path bar, it will be temporarily swapped for an entry widget and
 * swapped back once the user completed (or aborted) the input. */
void locbar_request_entry(LocationBar *bar, const gchar *initial_text)
{
    GtkWidget *child = gtk_bin_get_child(GTK_BIN(bar));

    e_return_if_fail(child != NULL && GTK_IS_WIDGET(child));

    if (IS_LOCATIONENTRY(child))
    {
        // already have an entry
        locentry_accept_focus(LOCATIONENTRY(child), initial_text);
    }
    else
    {
        // not an entry => temporarily replace it
        child = _locbar_install_widget(bar, TYPE_LOCATIONENTRY);

        locentry_accept_focus(LOCATIONENTRY(child), initial_text);
    }

    g_signal_connect(child, "edit-done",
                     G_CALLBACK(locbar_on_entry_edit_done), bar);
}

static void locbar_on_entry_edit_done(LocationEntry *entry, LocationBar *bar)
{
    g_signal_handlers_disconnect_by_func(entry,
                                         locbar_on_entry_edit_done, bar);

    g_object_ref(bar);

    g_idle_add_full(G_PRIORITY_HIGH_IDLE,
                    (GSourceFunc)_locbar_settings_changed,
                    bar, g_object_unref);

    g_signal_emit_by_name(bar, "entry-done");
}


