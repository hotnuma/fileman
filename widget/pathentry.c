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
 *
 * The icon code is based on ideas from SexyIconEntry, which was written by
 * Christian Hammond <chipx86@chipx86.com>.
 */

#include <config.h>
#include <pathentry.h>

#include <errno.h>
#include <memory.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>

#include <utils.h>

#include <iconfactory.h>
#include <iconrender.h>
#include <listmodel.h>

#include <gio_ext.h>

#define ICON_MARGIN (2)

enum
{
    PROP_0,
    PROP_CURRENT_FILE,
};

static void pathentry_editable_init(GtkEditableInterface *iface);
static void pathentry_finalize(GObject *object);
static void pathentry_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec);
static void pathentry_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec);
static gboolean pathentry_focus(GtkWidget *widget, GtkDirectionType direction);

static void _pathentry_icon_press_event(GtkEntry *entry,
                                        GtkEntryIconPosition icon_pos,
                                        GdkEventButton *event,
                                        gpointer userdata);
static void _pathentry_icon_release_event(GtkEntry *entry,
                                          GtkEntryIconPosition icon_pos,
                                          GdkEventButton *event,
                                          gpointer user_data);
static gboolean pathentry_motion_notify_event(GtkWidget *widget,
                                              GdkEventMotion *event);
static gboolean _pathentry_key_press_event(GtkWidget *widget, GdkEventKey *event);
static void pathentry_drag_data_get(GtkWidget *widget,
                                    GdkDragContext *context,
                                    GtkSelectionData *selection_data,
                                    guint info,
                                    guint timestamp);
static void pathentry_activate(GtkEntry *entry);
static void pathentry_changed(GtkEditable *editable);
static void _pathentry_update_icon(PathEntry *path_entry);
static void pathentry_do_insert_text(GtkEditable *editable, const gchar *new_text,
                                     gint new_text_length, gint *position);
static void _pathentry_clear_completion(PathEntry *path_entry);
static void _pathentry_common_prefix_append(PathEntry *path_entry,
                                            gboolean highlight);
static void _pathentry_common_prefix_lookup(PathEntry *path_entry,
                                            gchar **prefix_return,
                                            ThunarFile **file_return);
static gboolean _pathentry_match_func(GtkEntryCompletion *completion,
                                      const gchar *key,
                                      GtkTreeIter *iter,
                                      gpointer user_data);
static gboolean _pathentry_match_selected(GtkEntryCompletion *completion,
                                          GtkTreeModel *model,
                                          GtkTreeIter *iter,
                                          gpointer user_data);
static gboolean _pathentry_parse(PathEntry *path_entry,
                                 gchar **folder_part,
                                 gchar **file_part,
                                 GError **error);
static void _pathentry_queue_check_completion(PathEntry *path_entry);
static gboolean _pathentry_check_completion_idle(gpointer user_data);
static void _pathentry_check_completion_idle_destroy(gpointer user_data);


struct _PathEntryClass
{
    GtkEntryClass __parent__;
};

struct _PathEntry
{
    GtkEntry    __parent__;

    IconFactory *icon_factory;
    ThunarFile  *current_folder;
    ThunarFile  *current_file;
    GFile       *working_directory;

    guint       drag_button;
    gint        drag_x;
    gint        drag_y;

    // auto completion support
    guint       in_change : 1;
    guint       has_completion : 1;
    guint       check_completion_idle_id;
};

static const GtkTargetEntry _pathentry_drag_targets[] =
{
    {"text/uri-list", 0, 0},
};

static GtkEditableInterface* _pathentry_editable_parent_iface;

G_DEFINE_TYPE_WITH_CODE(PathEntry,
                        pathentry,
                        GTK_TYPE_ENTRY,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_EDITABLE,
                                              pathentry_editable_init))

static void pathentry_class_init(PathEntryClass *klass)
{
    GObjectClass   *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = pathentry_finalize;
    gobject_class->get_property = pathentry_get_property;
    gobject_class->set_property = pathentry_set_property;

    GtkWidgetClass *gtkwidget_class = GTK_WIDGET_CLASS(klass);
    gtkwidget_class->focus = pathentry_focus;
    gtkwidget_class->motion_notify_event = pathentry_motion_notify_event;
    gtkwidget_class->drag_data_get = pathentry_drag_data_get;

    GtkEntryClass  *gtkentry_class = GTK_ENTRY_CLASS(klass);
    gtkentry_class->activate = pathentry_activate;

    g_object_class_install_property(gobject_class,
                                    PROP_CURRENT_FILE,
                                    g_param_spec_object(
                                        "current-file",
                                        "current-file",
                                        "current-file",
                                        THUNAR_TYPE_FILE,
                                        E_PARAM_READWRITE));

    gtk_widget_class_install_style_property(
                                    gtkwidget_class,
                                    g_param_spec_int(
                                        "icon-size",
                                        _("Icon size"),
                                        _("The icon size for the path entry"),
                                        1,
                                        G_MAXINT,
                                        16,
                                        E_PARAM_READABLE));
}

static void pathentry_editable_init(GtkEditableInterface *iface)
{
    _pathentry_editable_parent_iface = g_type_interface_peek_parent(iface);

    iface->changed = pathentry_changed;
    iface->do_insert_text = pathentry_do_insert_text;
}

static void pathentry_init(PathEntry *path_entry)
{
    path_entry->check_completion_idle_id = 0;
    path_entry->working_directory = NULL;

    // allocate a new entry completion for the given model
    GtkEntryCompletion *completion = gtk_entry_completion_new();
    gtk_entry_completion_set_popup_single_match(completion, FALSE);
    gtk_entry_completion_set_match_func(completion, _pathentry_match_func, path_entry, NULL);
    g_signal_connect(G_OBJECT(completion), "match-selected", G_CALLBACK(_pathentry_match_selected), path_entry);

    // add the icon renderer to the entry completion
    GtkCellRenderer *renderer = g_object_new(TYPE_ICONRENDERER,
                                             "size", 16,
                                             NULL);

    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(completion), renderer, FALSE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(completion), renderer, "file", THUNAR_COLUMN_FILE);

    // add the text renderer to the entry completion
    renderer = gtk_cell_renderer_text_new();
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(completion), renderer, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(completion), renderer, "text", THUNAR_COLUMN_NAME);

    // allocate a new list mode for the completion
    ListModel *store = listmodel_new();
    listmodel_set_show_hidden(store, TRUE);
    listmodel_set_folders_first(store, TRUE);

    gtk_tree_sortable_set_sort_column_id(GTK_TREE_SORTABLE(store), THUNAR_COLUMN_FILE_NAME, GTK_SORT_ASCENDING);
    gtk_entry_completion_set_model(completion, GTK_TREE_MODEL(store));
    g_object_unref(G_OBJECT(store));

    /* need to connect the "key-press-event" before the GtkEntry class connects the completion signals, so
     * we get the Tab key before its handled as part of the completion stuff. */
    g_signal_connect(G_OBJECT(path_entry), "key-press-event", G_CALLBACK(_pathentry_key_press_event), NULL);

    // setup the new completion
    gtk_entry_set_completion(GTK_ENTRY(path_entry), completion);

    // cleanup
    g_object_unref(G_OBJECT(completion));

    // clear the auto completion whenever the cursor is moved manually or the selection is changed manually
    g_signal_connect(G_OBJECT(path_entry), "notify::cursor-position", G_CALLBACK(_pathentry_clear_completion), NULL);
    g_signal_connect(G_OBJECT(path_entry), "notify::selection-bound", G_CALLBACK(_pathentry_clear_completion), NULL);

    // connect the icon signals
    g_signal_connect(G_OBJECT(path_entry), "icon-press", G_CALLBACK(_pathentry_icon_press_event), NULL);
    g_signal_connect(G_OBJECT(path_entry), "icon-release", G_CALLBACK(_pathentry_icon_release_event), NULL);
}

static void pathentry_finalize(GObject *object)
{
    PathEntry *path_entry = PATHENTRY(object);

    // release factory
    if (path_entry->icon_factory != NULL)
        g_object_unref(path_entry->icon_factory);

    // release the current-folder reference
    if (G_LIKELY(path_entry->current_folder != NULL))
        g_object_unref(G_OBJECT(path_entry->current_folder));

    // release the current-file reference
    if (G_LIKELY(path_entry->current_file != NULL))
    {
        g_signal_handlers_disconnect_by_func(G_OBJECT(path_entry->current_file), pathentry_set_current_file, path_entry);
        g_object_unref(G_OBJECT(path_entry->current_file));
    }

    // release the working directory
    if (G_LIKELY(path_entry->working_directory != NULL))
        g_object_unref(G_OBJECT(path_entry->working_directory));

    // drop the check_completion_idle source
    if (G_UNLIKELY(path_entry->check_completion_idle_id != 0))
        g_source_remove(path_entry->check_completion_idle_id);

    G_OBJECT_CLASS(pathentry_parent_class)->finalize(object);
}

static void pathentry_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    (void) pspec;
    PathEntry *path_entry = PATHENTRY(object);

    switch(prop_id)
    {
    case PROP_CURRENT_FILE:
        g_value_set_object(value, pathentry_get_current_file(path_entry));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void pathentry_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    PathEntry *path_entry = PATHENTRY(object);

    switch(prop_id)
    {
    case PROP_CURRENT_FILE:
        pathentry_set_current_file(path_entry, g_value_get_object(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean pathentry_focus(GtkWidget *widget, GtkDirectionType direction)
{
    PathEntry *path_entry = PATHENTRY(widget);
    GdkModifierType  state;
    gboolean         control_pressed;

    // determine whether control is pressed
    control_pressed =(gtk_get_current_event_state(&state) &&(state & GDK_CONTROL_MASK) != 0);

    // evil hack, but works for GtkFileChooserEntry, so who cares :-)
    if ((direction == GTK_DIR_TAB_FORWARD) &&(gtk_widget_has_focus(widget)) && !control_pressed)
    {
        // if we don't have a completion and the cursor is at the end of the line, we just insert the common prefix
        if (!path_entry->has_completion && gtk_editable_get_position(GTK_EDITABLE(path_entry)) == gtk_entry_get_text_length(GTK_ENTRY(path_entry)))
            _pathentry_common_prefix_append(path_entry, FALSE);

        // place the cursor at the end
        gtk_editable_set_position(GTK_EDITABLE(path_entry), gtk_entry_get_text_length(GTK_ENTRY(path_entry)));

        return TRUE;
    }
    else
        return GTK_WIDGET_CLASS(pathentry_parent_class)->focus(widget, direction);
}

static void _pathentry_icon_press_event(GtkEntry             *entry,
                                        GtkEntryIconPosition icon_pos,
                                        GdkEventButton       *event,
                                        gpointer             userdata)
{
    (void) userdata;

    PathEntry *path_entry = PATHENTRY(entry);

    if (event->button == 1 && icon_pos == GTK_ENTRY_ICON_PRIMARY)
    {
        // consume the event
        path_entry->drag_button = event->button;
        path_entry->drag_x = event->x;
        path_entry->drag_y = event->y;
    }
}

static void _pathentry_icon_release_event(GtkEntry             *entry,
                                          GtkEntryIconPosition icon_pos,
                                          GdkEventButton       *event,
                                          gpointer             user_data)
{
    (void) user_data;

    PathEntry *path_entry = PATHENTRY(entry);

    if (event->button == path_entry->drag_button && icon_pos == GTK_ENTRY_ICON_PRIMARY)
    {
        // reset the drag button state
        path_entry->drag_button = 0;
    }
}

static gboolean pathentry_motion_notify_event(GtkWidget *widget,
                                              GdkEventMotion *event)
{
    PathEntry *path_entry = PATHENTRY(widget);
    GdkDragContext  *context;
    GtkTargetList   *target_list;
    GdkPixbuf       *icon;
    gint             size;

    if (path_entry->drag_button > 0
            && path_entry->current_file != NULL
            //FIXME && event->window == gtk_entry_get_icon_window(GTK_ENTRY(widget), GTK_ENTRY_ICON_PRIMARY)
            && gtk_drag_check_threshold(widget, path_entry->drag_x, path_entry->drag_y, event->x, event->y))
    {
        // create the drag context
        target_list = gtk_target_list_new(_pathentry_drag_targets, G_N_ELEMENTS(_pathentry_drag_targets));
        context = gtk_drag_begin_with_coordinates(widget, target_list,
                  GDK_ACTION_COPY |
                  GDK_ACTION_LINK,
                  path_entry->drag_button,
                (GdkEvent *) event, -1, -1);
        gtk_target_list_unref(target_list);

        // setup the drag icon(atleast 24px)
        gtk_widget_style_get(widget, "icon-size", &size, NULL);
        icon = iconfact_load_file_icon(path_entry->icon_factory,
                path_entry->current_file,
                THUNAR_FILE_ICON_STATE_DEFAULT,
                MAX(size, 16));
        if (G_LIKELY(icon != NULL))
        {
            gtk_drag_set_icon_pixbuf(context, icon, 0, 0);
            g_object_unref(G_OBJECT(icon));
        }

        // reset the drag button state
        path_entry->drag_button = 0;

        return TRUE;
    }

    return GTK_WIDGET_CLASS(pathentry_parent_class)->motion_notify_event(widget, event);
}

static gboolean _pathentry_key_press_event(GtkWidget *widget, GdkEventKey *event)
{
    PathEntry *path_entry = PATHENTRY(widget);

    // check if we have a tab key press here and control is not pressed
    if (G_UNLIKELY(event->keyval == GDK_KEY_Tab &&(event->state & GDK_CONTROL_MASK) == 0))
    {
        // if we don't have a completion and the cursor is at the end of the line, we just insert the common prefix
        if (!path_entry->has_completion && gtk_editable_get_position(GTK_EDITABLE(path_entry)) == gtk_entry_get_text_length(GTK_ENTRY(path_entry)))
            _pathentry_common_prefix_append(path_entry, FALSE);

        // place the cursor at the end
        gtk_editable_set_position(GTK_EDITABLE(path_entry), gtk_entry_get_text_length(GTK_ENTRY(path_entry)));

        // emit "changed", so the completion window is popped up
        g_signal_emit_by_name(G_OBJECT(path_entry), "changed", 0);

        // we handled the event
        return TRUE;
    }

    return FALSE;
}

static void pathentry_drag_data_get(GtkWidget        *widget,
                                    GdkDragContext   *context,
                                    GtkSelectionData *selection_data,
                                    guint            info,
                                    guint            timestamp)
{
    (void) context;
    (void) info;
    (void) timestamp;

    PathEntry  *path_entry = PATHENTRY(widget);
    GList             file_list;
    gchar           **uris;

    // verify that we actually display a path
    if (G_LIKELY(path_entry->current_file != NULL))
    {
        // transform the path for the current file into an uri string list
        file_list.next = file_list.prev = NULL;
        file_list.data = th_file_get_file(path_entry->current_file);

        // setup the uri list for the drag selection
        uris = e_file_list_to_stringv(&file_list);
        gtk_selection_data_set_uris(selection_data, uris);
        g_strfreev(uris);
    }
}

static void pathentry_activate(GtkEntry *entry)
{
    PathEntry *path_entry = PATHENTRY(entry);

    if (G_LIKELY(path_entry->has_completion))
    {
        // place cursor at the end of the text if we have completion set
        gtk_editable_set_position(GTK_EDITABLE(path_entry), -1);
    }

    // emit the "activate" signal
   GTK_ENTRY_CLASS(pathentry_parent_class)->activate(entry);
}

static void pathentry_changed(GtkEditable *editable)
{
    PathEntry *path_entry = PATHENTRY(editable);

    // check if we should ignore this event
    if (G_UNLIKELY(path_entry->in_change))
        return;

    // parse the entered string(handling URIs properly)
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(path_entry));

    gchar *escaped_text;
    GFile *file_path = NULL;
    GFile *folder_path = NULL;
    gchar *folder_part = NULL;
    gchar *file_part = NULL;

    if (G_UNLIKELY(g_uri_is_valid(text, G_URI_FLAGS_NONE, NULL)))
    {
        // try to parse the URI text
        escaped_text = g_uri_escape_string(text, G_URI_RESERVED_CHARS_ALLOWED_IN_PATH, TRUE);
        file_path = g_file_new_for_uri(escaped_text);
        g_free(escaped_text);

        // use the same file if the text assumes we're in a directory
        if (g_str_has_suffix(text, "/"))
            folder_path = G_FILE(g_object_ref(G_OBJECT(file_path)));
        else
            folder_path = g_file_get_parent(file_path);
    }
    else if (_pathentry_parse(path_entry, &folder_part, &file_part, NULL))
    {
        // determine the folder path
        folder_path = g_file_new_for_path(folder_part);

        // determine the relative file path
        if (G_LIKELY(*file_part != '\0'))
            file_path = g_file_resolve_relative_path(folder_path, file_part);
        else
            file_path = g_object_ref(folder_path);

        // cleanup the part strings
        g_free(folder_part);
        g_free(file_part);
    }

    // determine new current file/folder from the paths
    ThunarFile *current_folder;
    current_folder = (folder_path != NULL) ? th_file_get(folder_path, NULL) : NULL;
    ThunarFile *current_file;
    current_file = (file_path != NULL) ? th_file_get(file_path, NULL) : NULL;

    // determine the entry completion
    GtkEntryCompletion *completion;
    completion = gtk_entry_get_completion(GTK_ENTRY(path_entry));

    ThunarFolder *folder;
    GtkTreeModel *model;
    gboolean update_icon = FALSE;

    // update the current folder if required
    if (current_folder != path_entry->current_folder)
    {
        // take a reference on the current folder
        if (G_LIKELY(path_entry->current_folder != NULL))
            g_object_unref(G_OBJECT(path_entry->current_folder));

        path_entry->current_folder = current_folder;

        if (G_LIKELY(current_folder != NULL))
            g_object_ref(G_OBJECT(current_folder));

        // try to open the current-folder file as folder
        if (current_folder != NULL && th_file_is_directory(current_folder))
            folder = th_folder_get_for_file(current_folder);
        else
            folder = NULL;

        /* set the new folder for the completion model, but disconnect the model from the
         * completion first, because GtkEntryCompletion has become very slow recently when
         * updating the model being used(https://bugzilla.xfce.org/show_bug.cgi?id=1681).
         */
        model = gtk_entry_completion_get_model(completion);
        g_object_ref(G_OBJECT(model));
        gtk_entry_completion_set_model(completion, NULL);

        listmodel_set_folder(LISTMODEL(model), folder);

        gtk_entry_completion_set_model(completion, model);
        g_object_unref(G_OBJECT(model));

        // cleanup
        if (G_LIKELY(folder != NULL))
            g_object_unref(G_OBJECT(folder));

        // we most likely need a new icon
        update_icon = TRUE;
    }

    // update the current file if required
    if (current_file != path_entry->current_file)
    {
        if (G_UNLIKELY(path_entry->current_file != NULL))
        {
            g_signal_handlers_disconnect_by_func(G_OBJECT(path_entry->current_file), pathentry_set_current_file, path_entry);
            g_object_unref(G_OBJECT(path_entry->current_file));
        }

        path_entry->current_file = current_file;

        if (G_UNLIKELY(current_file != NULL))
        {
            g_object_ref(G_OBJECT(current_file));
            g_signal_connect_swapped(G_OBJECT(current_file), "changed",
                                     G_CALLBACK(pathentry_set_current_file), path_entry);
        }

        g_object_notify(G_OBJECT(path_entry), "current-file");

        // we most likely need a new icon
        update_icon = TRUE;
    }

    if (update_icon)
        _pathentry_update_icon(path_entry);

    // cleanup
    if (G_LIKELY(current_folder != NULL))
        g_object_unref(G_OBJECT(current_folder));

    if (G_LIKELY(current_file != NULL))
        g_object_unref(G_OBJECT(current_file));

    if (G_LIKELY(folder_path != NULL))
        g_object_unref(folder_path);

    if (G_LIKELY(file_path != NULL))
        g_object_unref(file_path);
}

static void _pathentry_update_icon(PathEntry *path_entry)
{
    GdkPixbuf          *icon = NULL;
    GtkIconTheme       *icon_theme;
    gint                icon_size;

    if (path_entry->icon_factory == NULL)
    {
        icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(GTK_WIDGET(path_entry)));
        path_entry->icon_factory = iconfact_get_for_icon_theme(icon_theme);
    }

    gtk_widget_style_get(GTK_WIDGET(path_entry), "icon-size", &icon_size, NULL);

    if (G_UNLIKELY(path_entry->current_file != NULL))
    {
        icon = iconfact_load_file_icon(path_entry->icon_factory,
                path_entry->current_file,
                THUNAR_FILE_ICON_STATE_DEFAULT,
                icon_size);
    }
    else if (G_LIKELY(path_entry->current_folder != NULL))
    {
        icon = iconfact_load_file_icon(path_entry->icon_factory,
                path_entry->current_folder,
                THUNAR_FILE_ICON_STATE_DEFAULT,
                icon_size);
    }

    if (icon != NULL)
    {
        gtk_entry_set_icon_from_pixbuf(GTK_ENTRY(path_entry),
                                        GTK_ENTRY_ICON_PRIMARY,
                                        icon);
        g_object_unref(icon);
    }
    else
    {
        gtk_entry_set_icon_from_icon_name(GTK_ENTRY(path_entry),
                                           GTK_ENTRY_ICON_PRIMARY,
                                           "dialog-error-symbolic");
    }
}

static void pathentry_do_insert_text(GtkEditable *editable,
                                     const gchar *new_text,
                                     gint        new_text_length,
                                     gint        *position)
{
    PathEntry *path_entry = PATHENTRY(editable);

    // let the GtkEntry class handle the insert
  (*_pathentry_editable_parent_iface->do_insert_text)(editable, new_text, new_text_length, position);

    // queue a completion check if this insert operation was triggered by the user
    if (G_LIKELY(!path_entry->in_change))
        _pathentry_queue_check_completion(path_entry);
}

static void _pathentry_clear_completion(PathEntry *path_entry)
{
    // reset the completion and apply the new text
    if (G_UNLIKELY(path_entry->has_completion))
    {
        path_entry->has_completion = FALSE;
        pathentry_changed(GTK_EDITABLE(path_entry));
    }
}

static void _pathentry_common_prefix_append(PathEntry *path_entry,
                                            gboolean  highlight)
{
    const gchar *last_slash;
    const gchar *text;
    ThunarFile  *file;
    gchar       *prefix;
    gchar       *tmp;
    gint         prefix_length;
    gint         text_length;
    gint         offset;
    gint         base;

    // determine the common prefix
    _pathentry_common_prefix_lookup(path_entry, &prefix, &file);

    // check if we should append a slash to the prefix
    if (G_LIKELY(file != NULL))
    {
        // we only append slashes for directories
        if (th_file_is_directory(file) && file != path_entry->current_file)
        {
            tmp = g_strconcat(prefix, G_DIR_SEPARATOR_S, NULL);
            g_free(prefix);
            prefix = tmp;
        }

        // release the file
        g_object_unref(G_OBJECT(file));
    }

    // check if we have a common prefix
    if (G_LIKELY(prefix != NULL))
    {
        // determine the UTF-8 length of the entry text
        text = gtk_entry_get_text(GTK_ENTRY(path_entry));
        last_slash = g_utf8_strrchr(text, -1, G_DIR_SEPARATOR);
        if (G_LIKELY(last_slash != NULL))
            offset = g_utf8_strlen(text, last_slash - text) + 1;
        else
            offset = 0;
        text_length = g_utf8_strlen(text, -1) - offset;

        // determine the UTF-8 length of the prefix
        prefix_length = g_utf8_strlen(prefix, -1);

        // append only if the prefix is longer than the already entered text
        if (G_LIKELY(prefix_length > text_length))
        {
            // remember the base offset
            base = offset;

            // insert the prefix
            path_entry->in_change = TRUE;
            gtk_editable_delete_text(GTK_EDITABLE(path_entry), offset, -1);
            gtk_editable_insert_text(GTK_EDITABLE(path_entry), prefix, -1, &offset);
            path_entry->in_change = FALSE;

            // highlight the prefix if requested
            if (G_LIKELY(highlight))
            {
                gtk_editable_select_region(GTK_EDITABLE(path_entry), base + text_length, base + prefix_length);
                path_entry->has_completion = TRUE;
            }
        }

        // cleanup
        g_free(prefix);
    }
}

static gboolean _pathentry_has_prefix_casefolded(const gchar *string,
                                                 const gchar *prefix)
{
    gchar *string_casefolded;
    gchar *prefix_casefolded;
    gboolean has_prefix;

    if (string == NULL || prefix == NULL)
        return FALSE;

    string_casefolded = g_utf8_casefold(string, -1);
    prefix_casefolded = g_utf8_casefold(prefix, -1);

    has_prefix = g_str_has_prefix(string_casefolded, prefix_casefolded);

    g_free(string_casefolded);
    g_free(prefix_casefolded);

    return has_prefix;
}

static void _pathentry_common_prefix_lookup(PathEntry  *path_entry,
                                            gchar      **prefix_return,
                                            ThunarFile **file_return)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    const gchar  *text;
    const gchar  *s;
    gchar        *name;
    gchar        *t;

    *prefix_return = NULL;
    *file_return = NULL;

    // lookup the last slash character in the entry text
    text = gtk_entry_get_text(GTK_ENTRY(path_entry));
    s = strrchr(text, G_DIR_SEPARATOR);
    if (G_UNLIKELY(s != NULL && s[1] == '\0'))
        return;
    else if (G_LIKELY(s != NULL))
        text = s + 1;

    // check all items in the model
    model = gtk_entry_completion_get_model(gtk_entry_get_completion(GTK_ENTRY(path_entry)));
    if (gtk_tree_model_get_iter_first(model, &iter))
    {
        do
        {
            // determine the real file name for the iter
            gtk_tree_model_get(model, &iter, THUNAR_COLUMN_FILE_NAME, &name, -1);

            // check if we have a valid prefix here
            if (_pathentry_has_prefix_casefolded(name, text))
            {
                // check if we're the first to match
                if (*prefix_return == NULL)
                {
                    // remember the prefix
                    *prefix_return = g_strdup(name);

                    // determine the file for the iter
                    gtk_tree_model_get(model, &iter, THUNAR_COLUMN_FILE, file_return, -1);
                }
                else
                {
                    // we already have another prefix, so determine the common part
                    for(s = name, t = *prefix_return; *s != '\0' && *s == *t; ++s, ++t)
                        ;
                    *t = '\0';

                    // release the file, since it's not a unique match
                    if (G_LIKELY(*file_return != NULL))
                    {
                        g_object_unref(G_OBJECT(*file_return));
                        *file_return = NULL;
                    }
                }
            }

            // cleanup
            g_free(name);
        }
        while(gtk_tree_model_iter_next(model, &iter));
    }
}

static gboolean _pathentry_match_func(GtkEntryCompletion *completion,
                                      const gchar        *key,
                                      GtkTreeIter        *iter,
                                      gpointer           user_data)
{
    (void) key;

    GtkTreeModel    *model;
    PathEntry *path_entry;
    const gchar     *last_slash;
    ThunarFile      *file;
    gboolean         matched;
    gchar           *text_normalized;
    gchar           *name_normalized;
    gchar           *name;

    // determine the model from the completion
    model = gtk_entry_completion_get_model(completion);

    /* leave if the model is null, we do this in thunar_path_entry_changed() to speed
     * things up, but that causes https://bugzilla.xfce.org/show_bug.cgi?id=4847. */
    if (G_UNLIKELY(model == NULL))
        return FALSE;

    /* leave if the auto completion highlight was not cleared yet, to prevent
     * https://bugzilla.xfce.org/show_bug.cgi?id=16267. */
    path_entry = PATHENTRY(user_data);
    if (G_UNLIKELY(path_entry->has_completion))
        return FALSE;

    // determine the current text(UTF-8 normalized)
    text_normalized = g_utf8_normalize(gtk_entry_get_text(GTK_ENTRY(user_data)), -1, G_NORMALIZE_ALL);

    // lookup the last slash character in the key
    last_slash = strrchr(text_normalized, G_DIR_SEPARATOR);
    if (G_UNLIKELY(last_slash != NULL && last_slash[1] == '\0'))
    {
        // check if the file is hidden
        gtk_tree_model_get(model, iter, THUNAR_COLUMN_FILE, &file, -1);
        matched = !th_file_is_hidden(file);
        g_object_unref(G_OBJECT(file));
    }
    else
    {
        if (G_UNLIKELY(last_slash == NULL))
            last_slash = text_normalized;
        else
            last_slash += 1;

        // determine the real file name for the iter
        gtk_tree_model_get(model, iter, THUNAR_COLUMN_FILE_NAME, &name, -1);
        name_normalized = g_utf8_normalize(name, -1, G_NORMALIZE_ALL);
        if (G_LIKELY(name_normalized != NULL))
            g_free(name);
        else
            name_normalized = name;

        // check if we have a match here
        if (name_normalized != NULL)
            matched = _pathentry_has_prefix_casefolded(name_normalized, last_slash);
        else
            matched = FALSE;

        // cleanup
        g_free(name_normalized);
    }

    // cleanup
    g_free(text_normalized);

    return matched;
}

static gboolean _pathentry_match_selected(GtkEntryCompletion *completion,
                                          GtkTreeModel       *model,
                                          GtkTreeIter        *iter,
                                          gpointer           user_data)
{
    (void) completion;

    PathEntry *path_entry = PATHENTRY(user_data);
    const gchar     *last_slash;
    const gchar     *text;
    ThunarFile      *file;
    gchar           *real_name;
    gchar           *tmp;
    gint             offset;

    // determine the file for the iterator
    gtk_tree_model_get(model, iter, THUNAR_COLUMN_FILE, &file, -1);

    // determine the real name for the file
    gtk_tree_model_get(model, iter, THUNAR_COLUMN_FILE_NAME, &real_name, -1);

    // append a slash if we have a folder here
    if (G_LIKELY(th_file_is_directory(file)))
    {
        tmp = g_strconcat(real_name, G_DIR_SEPARATOR_S, NULL);
        g_free(real_name);
        real_name = tmp;
    }

    // determine the UTF-8 offset of the last slash on the entry text
    text = gtk_entry_get_text(GTK_ENTRY(path_entry));
    last_slash = g_utf8_strrchr(text, -1, G_DIR_SEPARATOR);
    if (G_LIKELY(last_slash != NULL))
        offset = g_utf8_strlen(text, last_slash - text) + 1;
    else
        offset = 0;

    // delete the previous text at the specified offset
    gtk_editable_delete_text(GTK_EDITABLE(path_entry), offset, -1);

    // insert the new file/folder name
    gtk_editable_insert_text(GTK_EDITABLE(path_entry), real_name, -1, &offset);

    // move the cursor to the end of the text entry
    gtk_editable_set_position(GTK_EDITABLE(path_entry), -1);

    // cleanup
    g_object_unref(G_OBJECT(file));
    g_free(real_name);

    return TRUE;
}

static gboolean _pathentry_parse(PathEntry *path_entry,
                                 gchar     **folder_part,
                                 gchar     **file_part,
                                 GError    **error)
{
    const gchar *last_slash;
    gchar       *filename;
    gchar       *path;

    e_return_val_if_fail(IS_PATHENTRY(path_entry), FALSE);
    e_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    e_return_val_if_fail(folder_part != NULL, FALSE);
    e_return_val_if_fail(file_part != NULL, FALSE);

    // expand the filename
    filename = util_expand_filename(gtk_entry_get_text(GTK_ENTRY(path_entry)),
                                            path_entry->working_directory,
                                            error);
    if (G_UNLIKELY(filename == NULL))
        return FALSE;

    // lookup the last slash character in the filename
    last_slash = strrchr(filename, G_DIR_SEPARATOR);
    if (G_UNLIKELY(last_slash == NULL))
    {
        // no slash character, it's relative to the home dir
        *file_part = g_filename_from_utf8(filename, -1, NULL, NULL, error);
        if (G_LIKELY(*file_part != NULL))
            *folder_part = g_strdup(g_get_home_dir());
    }
    else
    {
        if (G_LIKELY(last_slash != filename))
            *folder_part = g_filename_from_utf8(filename, last_slash - filename, NULL, NULL, error);
        else
            *folder_part = g_strdup("/");

        if (G_LIKELY(*folder_part != NULL))
        {
            // if folder_part doesn't start with '/', it's relative to the home dir
            if (G_UNLIKELY(**folder_part != G_DIR_SEPARATOR))
            {
                path = g_build_filename(g_get_home_dir(), *folder_part, NULL);
                g_free(*folder_part);
                *folder_part = path;
            }

            // determine the file part
            *file_part = g_filename_from_utf8(last_slash + 1, -1, NULL, NULL, error);
            if (G_UNLIKELY(*file_part == NULL))
            {
                g_free(*folder_part);
                *folder_part = NULL;
            }
        }
        else
        {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, "%s", g_strerror(EINVAL));
        }
    }

    // release the filename
    g_free(filename);

    return(*folder_part != NULL);
}

static void _pathentry_queue_check_completion(PathEntry *path_entry)
{
    if (G_LIKELY(path_entry->check_completion_idle_id == 0))
    {
        path_entry->check_completion_idle_id =
                g_idle_add_full(G_PRIORITY_HIGH,
                                _pathentry_check_completion_idle,
                                path_entry,
                                _pathentry_check_completion_idle_destroy);
    }
}

static gboolean _pathentry_check_completion_idle(gpointer user_data)
{
    PathEntry *path_entry = PATHENTRY(user_data);

    UTIL_THREADS_ENTER

    // check if the user entered at least part of a filename
    const gchar *text = gtk_entry_get_text(GTK_ENTRY(path_entry));
    if (*text != '\0' && text[strlen(text) - 1] != '/')
    {
        // automatically insert the common prefix
        _pathentry_common_prefix_append(path_entry, TRUE);
    }

    UTIL_THREADS_LEAVE

    return FALSE;
}

static void _pathentry_check_completion_idle_destroy(gpointer user_data)
{
    PATHENTRY(user_data)->check_completion_idle_id = 0;
}

GtkWidget* pathentry_new()
{
    return g_object_new(TYPE_PATHENTRY, NULL);
}

ThunarFile* pathentry_get_current_file(PathEntry *path_entry)
{
    e_return_val_if_fail(IS_PATHENTRY(path_entry), NULL);
    return path_entry->current_file;
}

void pathentry_set_current_file(PathEntry *path_entry, ThunarFile *current_file)
{
    GFile    *file;
    gchar    *text;
    gchar    *unescaped;
    gchar    *tmp;
    gboolean  is_uri = FALSE;

    e_return_if_fail(IS_PATHENTRY(path_entry));
    e_return_if_fail(current_file == NULL || THUNAR_IS_FILE(current_file));

    file =(current_file != NULL) ? th_file_get_file(current_file) : NULL;
    if (G_UNLIKELY(file == NULL))
    {
        // invalid file
        text = g_strdup("");
    }
    else
    {
        // check if the file is native to the platform
        if (g_file_is_native(file))
        {
            // it is, try the local path first
            text = g_file_get_path(file);

            // if there is no local path, use the URI(which always works)
            if (text == NULL)
            {
                text = g_file_get_uri(file);
                is_uri = TRUE;
            }
        }
        else
        {
            // not a native file, use the URI
            text = g_file_get_uri(file);
            is_uri = TRUE;
        }

        /* if the file is a directory, end with a / to avoid loading the parent
         * directory which is probably not something the user wants */
        if (th_file_is_directory(current_file)
                && !g_str_has_suffix(text, "/"))
        {
            tmp = g_strconcat(text, "/", NULL);
            g_free(text);
            text = tmp;
        }

        // convert filename into valid UTF-8 string for display
        tmp = text;
        text = g_filename_display_name(tmp);
        g_free(tmp);
    }

    if (is_uri)
        unescaped = g_uri_unescape_string(text, NULL);
    else
        unescaped = g_strdup(text);
    g_free(text);

    // setup the entry text
    gtk_entry_set_text(GTK_ENTRY(path_entry), unescaped);
    g_free(unescaped);

    // update the icon
    _pathentry_update_icon(path_entry);

    gtk_editable_set_position(GTK_EDITABLE(path_entry), -1);

    gtk_widget_queue_draw(GTK_WIDGET(path_entry));
}

void pathentry_set_working_directory(PathEntry *path_entry,
                                     ThunarFile *working_directory)
{
    e_return_if_fail(IS_PATHENTRY(path_entry));
    e_return_if_fail(working_directory == NULL || THUNAR_IS_FILE(working_directory));

    if (G_LIKELY(path_entry->working_directory != NULL))
        g_object_unref(path_entry->working_directory);

    path_entry->working_directory = NULL;

    if (THUNAR_IS_FILE(working_directory))
        path_entry->working_directory = g_object_ref(th_file_get_file(working_directory));
}


