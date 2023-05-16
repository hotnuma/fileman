/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <config.h>
#include <clipboard.h>

#include <application.h>
#include <dialogs.h>

#include <string.h>
#include <memory.h>
#include <gio_ext.h>

static void clipman_finalize(GObject *object);
static void clipman_dispose(GObject *object);
static void clipman_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec);

// Public ---------------------------------------------------------------------

static void _clipman_owner_changed(GtkClipboard *clipboard,
                                   GdkEventOwnerChange *event,
                                   ClipboardManager *manager);
static void _clipman_targets_received(GtkClipboard *clipboard,
                                      GtkSelectionData *selection_data,
                                      gpointer user_data);
static void _clipman_transfer_files(ClipboardManager *manager, gboolean copy,
                                    GList *files);
static void _clipman_file_destroyed(ThunarFile *file, ClipboardManager *manager);
static void _clipman_get_callback(GtkClipboard *clipboard,
                                  GtkSelectionData *selection_data,
                                  guint info,
                                  gpointer user_data);
static gchar* _clipman_file_list_to_string(GList *list, const gchar *prefix,
                                           gboolean format_for_text, gsize *len);
static void _clipman_clear_callback(GtkClipboard *clipboard, gpointer user_data);
static void _clipman_contents_received(GtkClipboard *clipboard,
                                       GtkSelectionData *selection_data,
                                       gpointer user_data);

enum
{
    PROP_0,
    PROP_CAN_PASTE,
};

enum
{
    CHANGED,
    LAST_SIGNAL,
};

enum
{
    TARGET_TEXT_URI_LIST,
    TARGET_GNOME_COPIED_FILES,
    TARGET_UTF8_STRING,
};

struct _ClipboardManagerClass
{
    GObjectClass __parent__;

    void (*changed) (ClipboardManager *manager);
};

struct _ClipboardManager
{
    GObject     __parent__;

    GtkClipboard *clipboard;

    gboolean    can_paste;
    GdkAtom     x_special_gnome_copied_files;

    gboolean    files_cutted;
    GList       *files;
};

typedef struct
{
    ClipboardManager *manager;
    GFile       *target_file;
    GtkWidget   *widget;
    GClosure    *new_files_closure;

} ClipboardPasteRequest;

static const GtkTargetEntry _clipman_targets[] =
{
    {"text/uri-list",                   0, TARGET_TEXT_URI_LIST},
    {"x-special/gnome-copied-files",    0, TARGET_GNOME_COPIED_FILES},
    {"UTF8_STRING",                     0, TARGET_UTF8_STRING}
};

static GQuark _clipman_quark = 0;
static guint  _clipman_signals[LAST_SIGNAL];

G_DEFINE_TYPE(ClipboardManager, clipman, G_TYPE_OBJECT)

static void clipman_class_init(ClipboardManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = clipman_dispose;
    gobject_class->finalize = clipman_finalize;
    gobject_class->get_property = clipman_get_property;

    /**
     * ClipboardManager:can-paste:
     *
     * This property tells whether the current clipboard content of
     * this #ClipboardManager can be pasted into a folder
     * displayed by a #BaseView.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_CAN_PASTE,
                                    g_param_spec_boolean("can-paste",
                                                         "can-paste",
                                                         "can-paste",
                                                         FALSE,
                                                         E_PARAM_READABLE));

    /**
     * ClipboardManager::changed:
     * @manager : a #ClipboardManager.
     *
     * This signal is emitted whenever the contents of the
     * clipboard associated with @manager changes.
     **/
    _clipman_signals[CHANGED] =
        g_signal_new(I_("changed"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_FIRST,
                     G_STRUCT_OFFSET(ClipboardManagerClass, changed),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE,
                     0);
}

static void clipman_init(ClipboardManager *manager)
{
    manager->x_special_gnome_copied_files = gdk_atom_intern_static_string("x-special/gnome-copied-files");
}

static void clipman_dispose(GObject *object)
{
    ClipboardManager *manager = CLIPBOARDMANAGER(object);

    /* store the clipboard if we still own it and a clipboard
     * manager is running(gtk_clipboard_store checks this) */
    if (gtk_clipboard_get_owner(manager->clipboard) == object
        && manager->files != NULL)
    {
        gtk_clipboard_set_can_store(manager->clipboard,
                                    _clipman_targets,
                                    G_N_ELEMENTS(_clipman_targets));

        gtk_clipboard_store(manager->clipboard);
    }

    G_OBJECT_CLASS(clipman_parent_class)->dispose(object);
}

static void clipman_finalize(GObject *object)
{
    ClipboardManager *manager = CLIPBOARDMANAGER(object);

    // release any pending files
    for (GList *lp = manager->files; lp != NULL; lp = lp->next)
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(lp->data),
                                             _clipman_file_destroyed, manager);
        g_object_unref(G_OBJECT(lp->data));
    }

    g_list_free(manager->files);

    // disconnect from the clipboard
    g_signal_handlers_disconnect_by_func(G_OBJECT(manager->clipboard),
                                         _clipman_owner_changed, manager);
    g_object_set_qdata(G_OBJECT(manager->clipboard), _clipman_quark, NULL);
    g_object_unref(G_OBJECT(manager->clipboard));

    G_OBJECT_CLASS(clipman_parent_class)->finalize(object);
}

static void clipman_get_property(GObject *object, guint prop_id, GValue *value,
                                 GParamSpec *pspec)
{
    (void) pspec;

    ClipboardManager *manager = CLIPBOARDMANAGER(object);

    switch (prop_id)
    {
    case PROP_CAN_PASTE:
        g_value_set_boolean(value, clipman_can_paste(manager));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

gboolean clipman_can_paste(ClipboardManager *manager)
{
    e_return_val_if_fail(IS_CLIPBOARDMANAGER(manager), FALSE);

    return manager->can_paste;
}

// Public ---------------------------------------------------------------------

/**
 * thunar_clipboard_manager_get_for_display:
 * @display : a #GdkDisplay.
 *
 * Determines the #ClipboardManager that is used to manage
 * the clipboard on the given @display.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref() when it's no longer needed.
 *
 * Return value: the #ClipboardManager for @display.
 **/
ClipboardManager* clipman_get_for_display(GdkDisplay *display)
{
    e_return_val_if_fail(GDK_IS_DISPLAY(display), NULL);

    // generate the quark on-demand
    if (G_UNLIKELY(_clipman_quark == 0))
        _clipman_quark = g_quark_from_static_string("thunar-clipboard-manager");

    // figure out the clipboard for the given display
    GtkClipboard *clipboard = gtk_clipboard_get_for_display(
                                                    display,
                                                    GDK_SELECTION_CLIPBOARD);

    // check if a clipboard manager exists
    ClipboardManager *manager = g_object_get_qdata(G_OBJECT(clipboard),
                                                   _clipman_quark);
    if (G_LIKELY(manager != NULL))
    {
        g_object_ref(G_OBJECT(manager));
        return manager;
    }

    // allocate a new manager
    manager = g_object_new(TYPE_CLIPBOARDMANAGER, NULL);
    manager->clipboard = GTK_CLIPBOARD(g_object_ref(G_OBJECT(clipboard)));
    g_object_set_qdata(G_OBJECT(clipboard), _clipman_quark, manager);

    // listen for the "owner-change" signal on the clipboard
    g_signal_connect(G_OBJECT(manager->clipboard), "owner-change",
                     G_CALLBACK(_clipman_owner_changed), manager);

    // look for usable data on the clipboard
    _clipman_owner_changed(manager->clipboard, NULL, manager);

    return manager;
}

static void _clipman_owner_changed(GtkClipboard        *clipboard,
                                   GdkEventOwnerChange *event,
                                   ClipboardManager    *manager)
{
    e_return_if_fail(GTK_IS_CLIPBOARD(clipboard));
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));
    e_return_if_fail(manager->clipboard == clipboard);

    (void) event;

    /* need to take a reference on the manager, because the clipboards
     * "targets received callback" mechanism is not cancellable.
     */
    g_object_ref(G_OBJECT(manager));

    // request the list of supported targets from the new owner
    gtk_clipboard_request_contents(clipboard,
                                   gdk_atom_intern_static_string("TARGETS"),
                                   _clipman_targets_received,
                                   manager);
}

static void _clipman_targets_received(GtkClipboard     *clipboard,
                                      GtkSelectionData *selection_data,
                                      gpointer         user_data)
{
    ClipboardManager *manager = CLIPBOARDMANAGER(user_data);

    e_return_if_fail(GTK_IS_CLIPBOARD(clipboard));
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));
    e_return_if_fail(manager->clipboard == clipboard);

    // reset the "can-paste" state
    manager->can_paste = FALSE;

    GdkAtom *targets;
    gint n_targets;

    // check the list of targets provided by the owner
    if (gtk_selection_data_get_targets(selection_data, &targets, &n_targets))
    {
        for (gint n = 0; n < n_targets; ++n)
        {
            if (targets[n] == manager->x_special_gnome_copied_files)
            {
                manager->can_paste = TRUE;
                break;
            }
        }

        g_free(targets);
    }

    // notify listeners that we have a new clipboard state
    g_signal_emit(manager, _clipman_signals[CHANGED], 0);
    g_object_notify(G_OBJECT(manager), "can-paste");

    // drop the reference taken for the callback
    g_object_unref(manager);
}

gboolean clipman_has_cutted_file(ClipboardManager *manager, const ThunarFile *file)
{
    e_return_val_if_fail(IS_CLIPBOARDMANAGER(manager), FALSE);
    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    return (manager->files_cutted && g_list_find(manager->files, file) != NULL);
}

void clipman_copy_files(ClipboardManager *manager, GList *files)
{
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));

    _clipman_transfer_files(manager, TRUE, files);
}

void clipman_cut_files(ClipboardManager *manager, GList *files)
{
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));

    _clipman_transfer_files(manager, FALSE, files);
}

static void _clipman_transfer_files(ClipboardManager *manager,
                                    gboolean         copy,
                                    GList            *files)
{
    GList *lp;

    // release any pending files
    for (lp = manager->files; lp != NULL; lp = lp->next)
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(lp->data), _clipman_file_destroyed, manager);
        g_object_unref(G_OBJECT(lp->data));
    }
    g_list_free(manager->files);

    // remember the transfer operation
    manager->files_cutted = !copy;

    ThunarFile *file;

    // setup the new file list
    for (lp = g_list_last(files), manager->files = NULL; lp != NULL; lp = lp->prev)
    {
        file = THUNAR_FILE(g_object_ref(G_OBJECT(lp->data)));
        manager->files = g_list_prepend(manager->files, file);
        g_signal_connect(G_OBJECT(file), "destroy",
                         G_CALLBACK(_clipman_file_destroyed), manager);
    }

    // acquire the CLIPBOARD ownership
    gtk_clipboard_set_with_owner(manager->clipboard,
                                 _clipman_targets,
                                 G_N_ELEMENTS(_clipman_targets),
                                 _clipman_get_callback,
                                 _clipman_clear_callback,
                                 G_OBJECT(manager));

    // Need to fake a "owner-change" event here if the Xserver doesn't support clipboard notification
    if (!gdk_display_supports_selection_notification(
                gtk_clipboard_get_display(manager->clipboard)))
        _clipman_owner_changed(manager->clipboard, NULL, manager);
}

static void _clipman_file_destroyed(ThunarFile *file, ClipboardManager *manager)
{
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));
    e_return_if_fail(g_list_find(manager->files, file) != NULL);

    // remove the file from our list
    manager->files = g_list_remove(manager->files, file);

    // disconnect from the file
    g_signal_handlers_disconnect_by_func(G_OBJECT(file),
                                         _clipman_file_destroyed,
                                         manager);
    g_object_unref(G_OBJECT(file));
}

static void _clipman_get_callback(GtkClipboard     *clipboard,
                                  GtkSelectionData *selection_data,
                                  guint            target_info,
                                  gpointer         user_data)
{
    e_return_if_fail(GTK_IS_CLIPBOARD(clipboard));

    ClipboardManager *manager = CLIPBOARDMANAGER(user_data);
    e_return_if_fail(manager->clipboard == clipboard);
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));

    // determine the path list from the file list
    GList *file_list = th_filelist_to_thunar_g_file_list(manager->files);

    gchar **uris;
    const gchar *prefix;
    gchar *str;
    gsize len;

    switch (target_info)
    {
    case TARGET_TEXT_URI_LIST:
        uris = e_filelist_to_stringv(file_list);
        gtk_selection_data_set_uris(selection_data, uris);
        g_strfreev(uris);
        break;

    case TARGET_GNOME_COPIED_FILES:
        prefix = manager->files_cutted ? "cut\n" : "copy\n";
        str = _clipman_file_list_to_string(file_list, prefix, FALSE, &len);
        gtk_selection_data_set(selection_data,
                               gtk_selection_data_get_target(selection_data),
                               8,
                               (guchar*) str,
                               len);
        g_free(str);
        break;

    case TARGET_UTF8_STRING:
        str = _clipman_file_list_to_string(file_list, NULL, TRUE, &len);
        gtk_selection_data_set_text(selection_data, str, len);
        g_free(str);
        break;

    default:
        e_assert_not_reached();
    }

    // cleanup
    e_list_free(file_list);
}

static gchar* _clipman_file_list_to_string(GList *list, const gchar *prefix,
                                           gboolean format_for_text, gsize *len)
{
    gchar *tmp;

    // allocate initial string
    GString *string = g_string_new(prefix);

    for (GList *lp = list; lp != NULL; lp = lp->next)
    {
        if (format_for_text)
            tmp = g_file_get_parse_name(G_FILE(lp->data));
        else
            tmp = g_file_get_uri(G_FILE(lp->data));

        string = g_string_append(string, tmp);
        g_free(tmp);

        if (lp->next != NULL)
            string = g_string_append_c(string, '\n');
    }

    if (len != NULL)
        *len = string->len;

    return g_string_free(string, FALSE);
}

static void _clipman_clear_callback(GtkClipboard *clipboard, gpointer user_data)
{
    ClipboardManager *manager = CLIPBOARDMANAGER(user_data);

    e_return_if_fail(GTK_IS_CLIPBOARD(clipboard));
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));
    e_return_if_fail(manager->clipboard == clipboard);

    // release the pending files
    for (GList *lp = manager->files; lp != NULL; lp = lp->next)
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(lp->data),
                                             _clipman_file_destroyed,
                                             manager);
        g_object_unref(G_OBJECT(lp->data));
    }

    g_list_free(manager->files);
    manager->files = NULL;
}

void clipman_paste_files(ClipboardManager *manager, GFile *target_file,
                         GtkWidget *widget, GClosure *new_files_closure)
{
    e_return_if_fail(IS_CLIPBOARDMANAGER(manager));
    e_return_if_fail(widget == NULL || GTK_IS_WIDGET(widget));

    // prepare the paste request
    ClipboardPasteRequest *request = g_slice_new0(ClipboardPasteRequest);

    request->manager = CLIPBOARDMANAGER(g_object_ref(G_OBJECT(manager)));
    request->target_file = g_object_ref(target_file);
    request->widget = widget;

    // take a reference on the closure(if any)
    if (G_LIKELY(new_files_closure != NULL))
    {
        request->new_files_closure = new_files_closure;
        g_closure_ref(new_files_closure);
        g_closure_sink(new_files_closure);
    }

    /* get notified when the widget is destroyed prior to
     * completing the clipboard contents retrieval
     */
    if (G_LIKELY(request->widget != NULL))
        g_object_add_weak_pointer(G_OBJECT(request->widget), (gpointer) &request->widget);

    // schedule the request
    gtk_clipboard_request_contents(manager->clipboard,
                                   manager->x_special_gnome_copied_files,
                                   _clipman_contents_received,
                                   request);
}

static void _clipman_contents_received(GtkClipboard *clipboard,
                                       GtkSelectionData *selection_data,
                                       gpointer user_data)
{
    (void) clipboard;

    ClipboardPasteRequest *request = user_data;
    ClipboardManager *manager = CLIPBOARDMANAGER(request->manager);
    gboolean path_copy = TRUE;
    GList *file_list = NULL;

    // check whether the retrieval worked
    if (G_LIKELY(gtk_selection_data_get_length(selection_data) > 0))
    {
        // be sure the selection data is zero-terminated
        gchar *data;
        data = (gchar *) gtk_selection_data_get_data(selection_data);
        data[gtk_selection_data_get_length(selection_data)] = '\0';

        // check whether to copy or move
        if (g_ascii_strncasecmp(data, "copy\n", 5) == 0)
        {
            path_copy = TRUE;
            data += 5;
        }
        else if (g_ascii_strncasecmp(data, "cut\n", 4) == 0)
        {
            path_copy = FALSE;
            data += 4;
        }

        // determine the path list stored with the selection
        file_list = e_filelist_new_from_string(data);
    }

    // perform the action if possible
    if (G_LIKELY(file_list != NULL))
    {
        Application *application = application_get();

        if (G_LIKELY(path_copy))
            application_copy_into(application, request->widget, file_list, request->target_file, request->new_files_closure);
        else
            application_move_into(application, request->widget, file_list, request->target_file, request->new_files_closure);

        g_object_unref(G_OBJECT(application));
        e_list_free(file_list);

        /* clear the clipboard if it contained "cutted data"
         * (gtk_clipboard_clear takes care of not clearing
         * the selection if we don't own it)
         */
        if (G_UNLIKELY(!path_copy))
            gtk_clipboard_clear(manager->clipboard);

        /* check the contents of the clipboard again if either the Xserver or
         * our GTK+ version doesn't support the XFixes extension */
        if (!gdk_display_supports_selection_notification(gtk_clipboard_get_display(manager->clipboard)))
        {
            _clipman_owner_changed(manager->clipboard, NULL, manager);
        }
    }
    else
    {
        // tell the user that we cannot paste
        dialog_error(request->widget, NULL, _("There is nothing on the clipboard to paste"));
    }

    // free the request
    if (G_LIKELY(request->widget != NULL))
        g_object_remove_weak_pointer(G_OBJECT(request->widget),(gpointer) &request->widget);

    if (G_LIKELY(request->new_files_closure != NULL))
        g_closure_unref(request->new_files_closure);

    g_object_unref(G_OBJECT(request->manager));
    g_object_unref(request->target_file);
    g_slice_free(ClipboardPasteRequest, request);
}


