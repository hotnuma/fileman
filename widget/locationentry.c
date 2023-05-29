/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
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

#include <config.h>
#include <locationentry.h>
#include <marshal.h>

#include <pathentry.h>
#include <browser.h>
#include <navigator.h>
#include <dialogs.h>

static void locentry_navigator_init(ThunarNavigatorIface *iface);
static void locentry_finalize(GObject *object);
static void locentry_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void locentry_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);
static gboolean locentry_reset(LocationEntry *location_entry);

static ThunarFile* locentry_get_current_directory(ThunarNavigator *navigator);
static void locentry_set_current_directory(ThunarNavigator *navigator,
                                           ThunarFile *current_directory);

static void locentry_activate(GtkWidget *path_entry,
                              LocationEntry *location_entry);
static void locentry_poke_file_finish(ThunarBrowser *browser,
                                      ThunarFile    *file,
                                      ThunarFile    *target_file,
                                      GError        *error,
                                      gpointer      ignored);
static void locentry_open_or_launch(LocationEntry *location_entry,
                                    ThunarFile    *file);

static void locentry_reload(GtkEntry *entry, GtkEntryIconPosition icon_pos,
                            GdkEvent *event, LocationEntry *location_entry);
static void locentry_emit_edit_done(LocationEntry *entry);
static gboolean locentry_button_press_event(GtkWidget *path_entry,
                                            GdkEventButton *event,
                                            LocationEntry *location_entry);

// LocationEntry --------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CURRENT_DIRECTORY,
};

struct _LocationEntryClass
{
    GtkBoxClass __parent__;

    // internal action signals
    gboolean    (*reset) (LocationEntry *location_entry);

    // externally visible signals
    void        (*reload_requested) (void);
    void        (*edit_done) (void);
};

struct _LocationEntry
{
    GtkBox      __parent__;

    ThunarFile  *current_directory;
    GtkWidget   *path_entry;

    gboolean    right_click_occurred;
};

G_DEFINE_TYPE_WITH_CODE(LocationEntry, locentry, GTK_TYPE_BOX,
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARBROWSER,
                                              NULL)
                        G_IMPLEMENT_INTERFACE(TYPE_THUNARNAVIGATOR,
                                              locentry_navigator_init))

static void locentry_class_init(LocationEntryClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = locentry_finalize;
    gobject_class->get_property = locentry_get_property;
    gobject_class->set_property = locentry_set_property;

    klass->reset = locentry_reset;

    // override ThunarNavigator's properties
    g_object_class_override_property(gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

    /**
     * LocationEntry::reset:
     * @location_entry : a #LocationEntry.
     *
     * Emitted by @location_entry whenever the user requests to
     * reset the @location_entry contents to the current directory.
     * This is an internal signal used to bind the action to keys.
     **/
    g_signal_new(I_("reset"),
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET(LocationEntryClass, reset),
                 g_signal_accumulator_true_handled, NULL,
                 _thunar_marshal_BOOLEAN__VOID,
                 G_TYPE_BOOLEAN, 0);

    /**
     * LocationEntry::reload-requested:
     * @location_entry : a #LocationEntry.
     *
     * Emitted by @location_entry whenever the user clicked a "reload" button
     **/
    g_signal_new("reload-requested",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET(LocationEntryClass, reload_requested),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE, 0);

    /**
     * LocationEntry::edit-done:
     * @location_entry : a #LocationEntry.
     *
     * Emitted by @location_entry whenever the user finished or aborted an edit
     * operation by either changing to a directory, pressing Escape or moving the
     * focus away from the entry.
     **/
    g_signal_new("edit-done",
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET(LocationEntryClass, edit_done),
                 NULL, NULL,
                 NULL,
                 G_TYPE_NONE, 0);

    // setup the key bindings for the location entry
    GtkBindingSet *binding_set = gtk_binding_set_by_class(klass);

    gtk_binding_entry_add_signal(binding_set, GDK_KEY_Escape, 0, "reset", 0);
}

static void locentry_navigator_init(ThunarNavigatorIface *iface)
{
    iface->get_current_directory = locentry_get_current_directory;
    iface->set_current_directory = locentry_set_current_directory;
}

static void locentry_init(LocationEntry *entry)
{
    gtk_box_set_spacing(GTK_BOX(entry), 0);
    gtk_orientable_set_orientation(GTK_ORIENTABLE(entry),
                                   GTK_ORIENTATION_HORIZONTAL);

    entry->path_entry = pathentry_new();
    g_object_bind_property(G_OBJECT(entry), "current-directory",
                           G_OBJECT(entry->path_entry), "current-file",
                           G_BINDING_SYNC_CREATE);
    g_signal_connect_after(G_OBJECT(entry->path_entry), "activate",
                           G_CALLBACK(locentry_activate), entry);

    gtk_box_pack_start(GTK_BOX(entry), entry->path_entry, TRUE, TRUE, 0);
    gtk_widget_show(entry->path_entry);

    // put reload button in entry
    gtk_entry_set_icon_from_icon_name(GTK_ENTRY(entry->path_entry),
                                       GTK_ENTRY_ICON_SECONDARY, "view-refresh-symbolic");
    gtk_entry_set_icon_tooltip_text(GTK_ENTRY(entry->path_entry),
                                     GTK_ENTRY_ICON_SECONDARY, _("Reload the current folder"));
    g_signal_connect(G_OBJECT(entry->path_entry), "icon-release",
                     G_CALLBACK(locentry_reload), entry);

    // make sure the edit-done signal is emitted upon moving the focus somewhere else
    g_signal_connect_swapped(entry->path_entry, "focus-out-event",
                             G_CALLBACK(locentry_emit_edit_done), entry);

    // ...except if it is grabbed by the context menu
    entry->right_click_occurred = FALSE;
    g_signal_connect(G_OBJECT(entry->path_entry), "button-press-event",
                     G_CALLBACK(locentry_button_press_event), entry);
}

static void locentry_finalize(GObject *object)
{
    // disconnect from the current directory
    navigator_set_current_directory(THUNARNAVIGATOR(object), NULL);

    G_OBJECT_CLASS(locentry_parent_class)->finalize(object);
}

static void locentry_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        g_value_set_object(value, navigator_get_current_directory(THUNARNAVIGATOR(object)));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void locentry_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    LocationEntry *entry = LOCATIONENTRY(object);

    switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
        navigator_set_current_directory(THUNARNAVIGATOR(object),
                                        g_value_get_object(value));
        pathentry_set_working_directory(PATHENTRY(entry->path_entry),
                                        entry->current_directory);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean locentry_reset(LocationEntry *location_entry)
{
    // just reset the path entry to our current directory...
    pathentry_set_current_file(PATHENTRY(location_entry->path_entry), location_entry->current_directory);

    // ...and select the whole text again
    gtk_editable_select_region(GTK_EDITABLE(location_entry->path_entry), 0, -1);

    locentry_emit_edit_done(location_entry);

    return TRUE;
}

static ThunarFile* locentry_get_current_directory(ThunarNavigator *navigator)
{
    return LOCATIONENTRY(navigator)->current_directory;
}

static void locentry_set_current_directory(ThunarNavigator *navigator,
                                           ThunarFile      *current_directory)
{
    LocationEntry *location_entry = LOCATIONENTRY(navigator);

    // disconnect from the previous directory
    if (location_entry->current_directory != NULL)
        g_object_unref(G_OBJECT(location_entry->current_directory));

    // activate the new directory
    location_entry->current_directory = current_directory;

    // connect to the new directory
    if (current_directory != NULL)
        g_object_ref(G_OBJECT(current_directory));

    // notify listeners
    g_object_notify(G_OBJECT(location_entry), "current-directory");
}

void locentry_accept_focus(LocationEntry *location_entry,
                           const gchar   *initial_text)
{
    // give the keyboard focus to the path entry
    gtk_widget_grab_focus(location_entry->path_entry);

    // check if we have an initial text for the location bar
    if (initial_text != NULL)
    {
        // setup the new text
        gtk_entry_set_text(GTK_ENTRY(location_entry->path_entry), initial_text);

        // move the cursor to the end of the text
        gtk_editable_set_position(GTK_EDITABLE(location_entry->path_entry), -1);
    }
    else
    {
        // select the whole path in the path entry
        gtk_editable_select_region(GTK_EDITABLE(location_entry->path_entry), 0, -1);
    }
}

// Events ---------------------------------------------------------------------

static void locentry_activate(GtkWidget *path_entry, LocationEntry *location_entry)
{
    e_return_if_fail(IS_LOCATIONENTRY(location_entry));
    e_return_if_fail(location_entry->path_entry == path_entry);

    // determine the current file from the path entry
    ThunarFile *file = pathentry_get_current_file(PATHENTRY(path_entry));

    if (file == NULL)
        return;

    browser_poke_file(THUNARBROWSER(location_entry),
                      file,
                      path_entry,
                      locentry_poke_file_finish,
                      NULL);

    locentry_emit_edit_done(location_entry);
}

static void locentry_poke_file_finish(ThunarBrowser *browser,
                                      ThunarFile    *file,
                                      ThunarFile    *target_file,
                                      GError        *error,
                                      gpointer      ignored)
{
    (void) ignored;

    e_return_if_fail(IS_LOCATIONENTRY(browser));
    e_return_if_fail(IS_THUNARFILE(file));

    if (error != NULL)
    {
        // display an error explaining why we couldn't open/mount the file
        dialog_error(LOCATIONENTRY(browser)->path_entry,
                     error, _("Failed to open \"%s\""),
                     th_file_get_display_name(file));
        return;
    }

    // try to open or launch the target file
    locentry_open_or_launch(LOCATIONENTRY(browser), target_file);
}

static void locentry_open_or_launch(LocationEntry *location_entry,
                                    ThunarFile    *file)
{
    GError *error = NULL;

    e_return_if_fail(IS_LOCATIONENTRY(location_entry));
    e_return_if_fail(IS_THUNARFILE(file));

    // check if the file is mounted
    if (th_file_is_mounted(file))
    {
        // check if we have a new directory or a file to launch
        if (th_file_is_directory(file))
        {
            // open the new directory
            navigator_change_directory(THUNARNAVIGATOR(location_entry), file);
        }
        else
        {
            // try to launch the selected file
            th_file_launch(file, location_entry->path_entry, NULL, &error);

            // be sure to reset the current file of the path entry
            if (location_entry->current_directory != NULL)
            {
                pathentry_set_current_file(PATHENTRY(location_entry->path_entry),
                                                    location_entry->current_directory);
            }
        }
    }
    else
    {
        g_set_error(&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, _("File does not exist"));
    }

    // check if we need to display an error dialog
    if (error != NULL)
    {
        dialog_error(location_entry->path_entry, error,
                                   _("Failed to open \"%s\""),
                                   th_file_get_display_name(file));
        g_error_free(error);
    }
}

static void locentry_reload(GtkEntry *entry, GtkEntryIconPosition icon_pos,
                            GdkEvent *event, LocationEntry *location_entry)
{
    (void) entry;
    (void) event;

    e_return_if_fail(IS_LOCATIONENTRY(location_entry));

    if (icon_pos == GTK_ENTRY_ICON_SECONDARY)
    {
        g_signal_emit_by_name(location_entry, "reload-requested");
    }
}

static void locentry_emit_edit_done(LocationEntry *entry)
{
    // do not emit signal if the context menu was opened

    if (entry->right_click_occurred == FALSE)
    {
        g_signal_emit_by_name(entry, "edit-done");
    }

    entry->right_click_occurred = FALSE;
}

static gboolean locentry_button_press_event(GtkWidget      *path_entry,
                                            GdkEventButton *event,
                                            LocationEntry  *location_entry)
{
    (void) path_entry;

    e_return_val_if_fail(IS_LOCATIONENTRY(location_entry), FALSE);

    // check if the context menu was triggered
    if (event->type == GDK_BUTTON_PRESS && event->button == 3)
    {
        location_entry->right_click_occurred = TRUE;
    }

    return FALSE;
}


