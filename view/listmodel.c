/*-
 * Copyright(c) 2004-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright(c) 2012      Nick Schermer <nick@xfce.org>
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
#include <listmodel.h>

#include <application.h>
#include <filemonitor.h>
#include <gio_ext.h>

#define ETMFOREACHFUNC (GtkTreeModelForeachFunc) (void(*)(void))

// GObject --------------------------------------------------------------------

static void listmodel_dispose(GObject *object);
static void listmodel_finalize(GObject *object);
static void listmodel_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec);
static void listmodel_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec);

// Get/set --------------------------------------------------------------------

static gboolean _listmodel_get_case_sensitive(ListModel *store);
static void _listmodel_set_case_sensitive(ListModel *store,
                                          gboolean case_sensitive);

static ThunarDateStyle _listmodel_get_date_style(ListModel *store);
static void _listmodel_set_date_style(ListModel *store, ThunarDateStyle date_style);

static const char* _listmodel_get_date_custom_style(ListModel *store);
static void _listmodel_set_date_custom_style(ListModel *store,
                                             const char *date_custom_style);

static gboolean _listmodel_get_folders_first(ListModel *store);

static gint _listmodel_get_num_files(ListModel *store);

// Interfaces -----------------------------------------------------------------

static void listmodel_tree_model_init(GtkTreeModelIface *iface);
static void listmodel_drag_dest_init(GtkTreeDragDestIface *iface);
static void listmodel_sortable_init(GtkTreeSortableIface *iface);

// GtkTreeModel ---------------------------------------------------------------

static GtkTreeModelFlags listmodel_get_flags(GtkTreeModel *model);
static gint listmodel_get_n_columns(GtkTreeModel *model);
static GType listmodel_get_column_type(GtkTreeModel *model, gint idx);
static gboolean listmodel_get_iter(GtkTreeModel *model, GtkTreeIter *iter,
                                   GtkTreePath *path);
static GtkTreePath* listmodel_get_path(GtkTreeModel *model, GtkTreeIter *iter);
static void listmodel_get_value(GtkTreeModel *model, GtkTreeIter *iter,
                                gint column, GValue *value);
static gboolean listmodel_iter_next(GtkTreeModel *model, GtkTreeIter *iter);
static gboolean listmodel_iter_children(GtkTreeModel *model, GtkTreeIter *iter,
                                        GtkTreeIter *parent);
static gboolean listmodel_iter_has_child(GtkTreeModel *model, GtkTreeIter *iter);
static gint listmodel_iter_n_children(GtkTreeModel *model, GtkTreeIter *iter);
static gboolean listmodel_iter_nth_child(GtkTreeModel *model, GtkTreeIter *iter,
                                         GtkTreeIter *parent, gint n);
static gboolean listmodel_iter_parent(GtkTreeModel *model, GtkTreeIter *iter,
                                      GtkTreeIter *child);

// GtkTreeDragDest ------------------------------------------------------------

static gboolean listmodel_drag_data_received(GtkTreeDragDest *dest,
                                             GtkTreePath *path,
                                             GtkSelectionData *data);
static gboolean listmodel_row_drop_possible(GtkTreeDragDest *dest,
                                            GtkTreePath *path,
                                            GtkSelectionData *data);

// GtkTreeSortable ------------------------------------------------------------

static gboolean listmodel_get_sort_column_id(GtkTreeSortable *sortable,
                                             gint *sort_column_id,
                                             GtkSortType *order);
static void listmodel_set_sort_column_id(GtkTreeSortable *sortable,
                                         gint sort_column_id,
                                         GtkSortType order);
static void listmodel_set_sort_func(GtkTreeSortable *sortable,
                                    gint sort_column_id,
                                    GtkTreeIterCompareFunc func,
                                    gpointer data,
                                    GDestroyNotify destroy);
static void listmodel_set_default_sort_func(GtkTreeSortable *sortable,
                                            GtkTreeIterCompareFunc func,
                                            gpointer data,
                                            GDestroyNotify destroy);
static gboolean listmodel_has_default_sort_func(GtkTreeSortable *sortable);

// Sorting --------------------------------------------------------------------

static void _listmodel_sort(ListModel *store);
static gint _listmodel_cmp_func(gconstpointer a, gconstpointer b,
                                gpointer user_data);

static gint _sort_by_date_accessed(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive);
static gint _sort_by_date_modified(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive);
static gint _sort_by_group(const ThunarFile *a, const ThunarFile *b,
                           gboolean case_sensitive);
static gint _sort_by_mime_type(const ThunarFile *a, const ThunarFile *b,
                               gboolean case_sensitive);
static gint _sort_by_owner(const ThunarFile *a, const ThunarFile *b,
                           gboolean case_sensitive);
static gint _sort_by_permissions(const ThunarFile *a, const ThunarFile *b,
                                 gboolean case_sensitive);
static gint _sort_by_size(const ThunarFile *a, const ThunarFile *b,
                          gboolean case_sensitive);
static gint _sort_by_size_in_bytes(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive);
static gint _sort_by_type(const ThunarFile *a, const ThunarFile *b,
                          gboolean case_sensitive);

// File Monitors --------------------------------------------------------------

static void _listmodel_file_changed(FileMonitor *file_monitor, ThunarFile *file,
                                    ListModel *store);

static void _listmodel_folder_destroy(ThunarFolder *folder, ListModel *store);
static void _listmodel_folder_error(ThunarFolder *folder, const GError *error,
                                    ListModel *store);

static void _listmodel_files_added(ThunarFolder *folder, GList *files,
                                   ListModel *store);
static void _listmodel_files_removed(ThunarFolder *folder, GList *files,
                                     ListModel *store);

// Public ---------------------------------------------------------------------

static gchar* _listmodel_get_statusbar_text_for_files(
                                        GList *files,
                                        gboolean show_file_size_binary_format);

// ListModel ------------------------------------------------------------------

enum
{
    PROP_0,
    PROP_CASE_SENSITIVE,
    PROP_DATE_STYLE,
    PROP_DATE_CUSTOM_STYLE,
    PROP_FOLDER,
    PROP_FOLDERS_FIRST,
    PROP_NUM_FILES,
    PROP_SHOW_HIDDEN,
    PROP_FILE_SIZE_BINARY,
    N_PROPERTIES
};
static GParamSpec* _listmodel_props[N_PROPERTIES] = {NULL,};

enum
{
    ERROR,
    LAST_SIGNAL,
};
static guint _listmodel_signals[LAST_SIGNAL];

typedef gint (*ModelSortFunc) (const ThunarFile *a, const ThunarFile *b,
                               gboolean case_sensitive);

struct _ListModelClass
{
    GObjectClass __parent__;

    // signals
    void (*error) (ListModel *store, const GError *error);
};

struct _ListModel
{
    GObject         __parent__;

    /* the model stamp is only used when debugging is
     * enabled, to make sure we don't accept iterators
     * generated by another model. */
#ifndef NDEBUG
    gint            stamp;
#endif

    GSequence       *rows;
    GSList          *hidden;

    ThunarFolder    *folder;

    /* Use the shared FileMonitor instance, so we
     * do not need to connect "changed" handler to every
     * file in the model. */
    FileMonitor     *file_monitor;

    /* ids for the "row-inserted" and "row-deleted" signals
     * of GtkTreeModel to speed up folder changing. */
    guint           row_inserted_id;
    guint           row_deleted_id;

    gboolean        show_hidden : 1;
    gboolean        file_size_binary : 1;
    ThunarDateStyle date_style;
    char            *date_custom_style;

    gboolean        sort_case_sensitive : 1;
    gboolean        sort_folders_first : 1;
    gint            sort_sign;   // 1 = ascending, -1 descending
    ModelSortFunc   sort_func;
};

G_DEFINE_TYPE_WITH_CODE(ListModel,
                        listmodel,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_MODEL,
                                              listmodel_tree_model_init)
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_DRAG_DEST,
                                              listmodel_drag_dest_init)
                        G_IMPLEMENT_INTERFACE(GTK_TYPE_TREE_SORTABLE,
                                              listmodel_sortable_init))

static void listmodel_class_init(ListModelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose      = listmodel_dispose;
    gobject_class->finalize     = listmodel_finalize;
    gobject_class->get_property = listmodel_get_property;
    gobject_class->set_property = listmodel_set_property;

    _listmodel_props[PROP_CASE_SENSITIVE] =
        g_param_spec_boolean("case-sensitive",
                             "case-sensitive",
                             "case-sensitive",
                             TRUE,
                             E_PARAM_READWRITE);

    _listmodel_props[PROP_DATE_STYLE] =
        g_param_spec_enum("date-style",
                          "date-style",
                          "date-style",
                          THUNAR_TYPE_DATE_STYLE,
                          THUNAR_DATE_STYLE_YYYYMMDD,
                          E_PARAM_READWRITE);

    _listmodel_props[PROP_DATE_CUSTOM_STYLE] =
        g_param_spec_string("date-custom-style",
                            "DateCustomStyle",
                            NULL,
                            "%Y-%m-%d %H:%M:%S",
                            E_PARAM_READWRITE);

    // The folder presented by this #ListModel.
    _listmodel_props[PROP_FOLDER] =
        g_param_spec_object("folder",
                            "folder",
                            "folder",
                            TYPE_THUNARFOLDER,
                            E_PARAM_READWRITE);

    _listmodel_props[PROP_FOLDERS_FIRST] =
        g_param_spec_boolean("folders-first",
                             "folders-first",
                             "folders-first",
                             TRUE,
                             E_PARAM_READWRITE);

    // The number of files in the folder presented by this #ListModel.
    _listmodel_props[PROP_NUM_FILES] =
        g_param_spec_uint("num-files",
                          "num-files",
                          "num-files",
                          0,
                          G_MAXUINT,
                          0,
                          E_PARAM_READABLE);

    _listmodel_props[PROP_SHOW_HIDDEN] =
        g_param_spec_boolean("show-hidden",
                             "show-hidden",
                             "show-hidden",
                             FALSE,
                             E_PARAM_READWRITE);

    _listmodel_props[PROP_FILE_SIZE_BINARY] =
        g_param_spec_boolean("file-size-binary",
                             "file-size-binary",
                             "file-size-binary",
                             TRUE,
                             E_PARAM_READWRITE);

    // install properties
    g_object_class_install_properties(gobject_class, N_PROPERTIES, _listmodel_props);

    // Emitted when an error occurs while loading the store content.
    _listmodel_signals[ERROR] =
        g_signal_new(I_("error"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_LAST,
                     G_STRUCT_OFFSET(ListModelClass, error),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__POINTER,
                     G_TYPE_NONE,
                     1,
                     G_TYPE_POINTER);
}

static void listmodel_tree_model_init(GtkTreeModelIface *iface)
{
    iface->get_flags        = listmodel_get_flags;
    iface->get_n_columns    = listmodel_get_n_columns;
    iface->get_column_type  = listmodel_get_column_type;
    iface->get_iter         = listmodel_get_iter;
    iface->get_path         = listmodel_get_path;
    iface->get_value        = listmodel_get_value;
    iface->iter_next        = listmodel_iter_next;
    iface->iter_children    = listmodel_iter_children;
    iface->iter_has_child   = listmodel_iter_has_child;
    iface->iter_n_children  = listmodel_iter_n_children;
    iface->iter_nth_child   = listmodel_iter_nth_child;
    iface->iter_parent      = listmodel_iter_parent;
}

static void listmodel_drag_dest_init(GtkTreeDragDestIface *iface)
{
    iface->drag_data_received = listmodel_drag_data_received;
    iface->row_drop_possible = listmodel_row_drop_possible;
}

static void listmodel_sortable_init(GtkTreeSortableIface *iface)
{
    iface->get_sort_column_id     = listmodel_get_sort_column_id;
    iface->set_sort_column_id     = listmodel_set_sort_column_id;
    iface->set_sort_func          = listmodel_set_sort_func;
    iface->set_default_sort_func  = listmodel_set_default_sort_func;
    iface->has_default_sort_func  = listmodel_has_default_sort_func;
}

static void listmodel_init(ListModel *store)
{
#ifndef NDEBUG
    store->stamp = g_random_int();
#endif

    store->row_inserted_id = g_signal_lookup("row-inserted", GTK_TYPE_TREE_MODEL);
    store->row_deleted_id = g_signal_lookup("row-deleted", GTK_TYPE_TREE_MODEL);

    store->sort_case_sensitive = TRUE;
    store->sort_folders_first = TRUE;
    store->sort_sign = 1;
    store->sort_func = th_file_compare_by_name;

    store->rows = g_sequence_new(g_object_unref);

    /* connect to the shared FileMonitor, so we don't need to
     * connect "changed" to every single ThunarFile we own. */
    store->file_monitor = filemon_get_default();

    g_signal_connect(G_OBJECT(store->file_monitor), "file-changed",
                     G_CALLBACK(_listmodel_file_changed), store);
}

// GObject --------------------------------------------------------------------

static void listmodel_dispose(GObject *object)
{
    // unlink from the folder(if any)
    listmodel_set_folder(LISTMODEL(object), NULL);

    G_OBJECT_CLASS(listmodel_parent_class)->dispose(object);
}

static void listmodel_finalize(GObject *object)
{
    ListModel *store = LISTMODEL(object);

    g_sequence_free(store->rows);

    // disconnect from the file monitor
    g_signal_handlers_disconnect_by_func(G_OBJECT(store->file_monitor),
                                         _listmodel_file_changed, store);

    g_object_unref(G_OBJECT(store->file_monitor));

    G_OBJECT_CLASS(listmodel_parent_class)->finalize(object);
}

static void listmodel_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ListModel *store = LISTMODEL(object);

    switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
        g_value_set_boolean(value, _listmodel_get_case_sensitive(store));
        break;

    case PROP_DATE_STYLE:
        g_value_set_enum(value, _listmodel_get_date_style(store));
        break;

    case PROP_DATE_CUSTOM_STYLE:
        g_value_set_string(value, _listmodel_get_date_custom_style(store));
        break;

    case PROP_FOLDER:
        g_value_set_object(value, listmodel_get_folder(store));
        break;

    case PROP_FOLDERS_FIRST:
        g_value_set_boolean(value, _listmodel_get_folders_first(store));
        break;

    case PROP_NUM_FILES:
        g_value_set_uint(value, _listmodel_get_num_files(store));
        break;

    case PROP_SHOW_HIDDEN:
        g_value_set_boolean(value, listmodel_get_show_hidden(store));
        break;

    case PROP_FILE_SIZE_BINARY:
        g_value_set_boolean(value, listmodel_get_file_size_binary(store));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void listmodel_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ListModel *store = LISTMODEL(object);

    switch (prop_id)
    {
    case PROP_CASE_SENSITIVE:
        _listmodel_set_case_sensitive(store, g_value_get_boolean(value));
        break;

    case PROP_DATE_STYLE:
        _listmodel_set_date_style(store, g_value_get_enum(value));
        break;

    case PROP_DATE_CUSTOM_STYLE:
        _listmodel_set_date_custom_style(store, g_value_get_string(value));
        break;

    case PROP_FOLDER:
        listmodel_set_folder(store, g_value_get_object(value));
        break;

    case PROP_FOLDERS_FIRST:
        listmodel_set_folders_first(store, g_value_get_boolean(value));
        break;

    case PROP_SHOW_HIDDEN:
        listmodel_set_show_hidden(store, g_value_get_boolean(value));
        break;

    case PROP_FILE_SIZE_BINARY:
        listmodel_set_file_size_binary(store, g_value_get_boolean(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static gboolean _listmodel_get_case_sensitive(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    return store->sort_case_sensitive;
}

static void _listmodel_set_case_sensitive(ListModel *store, gboolean case_sensitive)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // normalize the setting
    case_sensitive = !!case_sensitive;

    // check if we have a new setting
    if (store->sort_case_sensitive == case_sensitive)
        return;

    // apply the new setting
    store->sort_case_sensitive = case_sensitive;

    // resort the model with the new setting
    _listmodel_sort(store);

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_CASE_SENSITIVE]);

    /* emit a "changed" signal for each row, so the display is
     * reloaded with the new case-sensitive setting */
    gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                           ETMFOREACHFUNC gtk_tree_model_row_changed,
                           NULL);
}

static ThunarDateStyle _listmodel_get_date_style(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), THUNAR_DATE_STYLE_SIMPLE);

    return store->date_style;
}

static void _listmodel_set_date_style(ListModel *store, ThunarDateStyle date_style)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // check if we have a new setting
    if (store->date_style == date_style)
        return;

    // apply the new setting
    store->date_style = date_style;

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_DATE_STYLE]);

    // emit a "changed" signal for each row, so the display is reloaded with the new date style
    gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                           ETMFOREACHFUNC gtk_tree_model_row_changed,
                           NULL);
}

static const char* _listmodel_get_date_custom_style(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), NULL);

    return store->date_custom_style;
}

static void _listmodel_set_date_custom_style(ListModel *store,
                                             const char *date_custom_style)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // check if we have a new setting
    if (g_strcmp0(store->date_custom_style, date_custom_style) == 0)
        return;

    // apply the new setting
    store->date_custom_style = g_strdup(date_custom_style);

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_DATE_CUSTOM_STYLE]);

    // emit a "changed" signal for each row, so the display is reloaded with the new date style
    gtk_tree_model_foreach(
                GTK_TREE_MODEL(store),
                ETMFOREACHFUNC gtk_tree_model_row_changed,
                NULL);
}

ThunarFolder* listmodel_get_folder(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), NULL);

    return store->folder;
}

void listmodel_set_folder(ListModel *store, ThunarFolder *folder)
{
    e_return_if_fail(IS_LISTMODEL(store));
    e_return_if_fail(folder == NULL || IS_THUNARFOLDER(folder));

    if (store->folder == folder)
        return;

    // remove existing entries, remove previous ThunarFolder
    if (store->folder != NULL)
    {
        // check if we have any handlers connected for "row-deleted"
        gboolean has_handler = g_signal_has_handler_pending(G_OBJECT(store),
                                                            store->row_deleted_id,
                                                            0,
                                                            FALSE);

        GSequenceIter *row = g_sequence_get_begin_iter(store->rows);
        GSequenceIter *end = g_sequence_get_end_iter(store->rows);

        GtkTreePath *path = gtk_tree_path_new_first();

        while (row != end)
        {
            // remove the row from the list
            GSequenceIter *next = g_sequence_iter_next(row);
            g_sequence_remove(row);
            row = next;

            /* notify the view(s) if they're actually
             * interested in the "row-deleted" signal. */
            if (has_handler)
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
        }

        gtk_tree_path_free(path);

        // remove hidden entries
        g_slist_free_full(store->hidden, g_object_unref);
        store->hidden = NULL;

        // unregister signals and drop the reference
        g_signal_handlers_disconnect_matched(G_OBJECT(store->folder),
                                             G_SIGNAL_MATCH_DATA, 0, 0,
                                             NULL, NULL, store);

        g_object_unref(G_OBJECT(store->folder));
    }

    // just to be sure
    e_assert(g_sequence_get_length(store->rows) == 0);

#ifndef NDEBUG
    // new stamp since the model changed
    store->stamp = g_random_int();
#endif

    // activate the new folder
    store->folder = folder;

    // freeze
    g_object_freeze_notify(G_OBJECT(store));

    // connect to the new folder (if any)
    if (folder != NULL)
    {
        g_object_ref(G_OBJECT(folder));

        // get the already loaded files
        GList *files = th_folder_get_files(folder);

        // insert the files
        if (files != NULL)
            _listmodel_files_added(folder, files, store);

        // connect signals to the new folder
        g_signal_connect(G_OBJECT(store->folder), "destroy",
                         G_CALLBACK(_listmodel_folder_destroy), store);

        g_signal_connect(G_OBJECT(store->folder), "error",
                         G_CALLBACK(_listmodel_folder_error), store);

        g_signal_connect(G_OBJECT(store->folder), "files-added",
                         G_CALLBACK(_listmodel_files_added), store);

        g_signal_connect(G_OBJECT(store->folder), "files-removed",
                         G_CALLBACK(_listmodel_files_removed), store);
    }

    // notify listeners that we have a new folder
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_FOLDER]);
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_NUM_FILES]);

    g_object_thaw_notify(G_OBJECT(store));
}

static gboolean _listmodel_get_folders_first(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    return store->sort_folders_first;
}

void listmodel_set_folders_first(ListModel *store, gboolean folders_first)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // check if the new setting differs
    if (store->sort_folders_first == folders_first)
        return;

    // apply the new setting(re-sorting the store)
    store->sort_folders_first = folders_first;

    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_FOLDERS_FIRST]);

    _listmodel_sort(store);

    /* emit a "changed" signal for each row, so the display is
     * reloaded with the new folders first setting */
    gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                           ETMFOREACHFUNC gtk_tree_model_row_changed,
                           NULL);
}

// Counts the number of visible files into the store
static gint _listmodel_get_num_files(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), 0);

    return g_sequence_get_length(store->rows);
}

gboolean listmodel_get_show_hidden(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    return store->show_hidden;
}

void listmodel_set_show_hidden(ListModel *store, gboolean show_hidden)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // check if the settings differ
    if (store->show_hidden == show_hidden)
        return;

    store->show_hidden = show_hidden;

    if (store->show_hidden)
    {
        for (GSList *lp = store->hidden; lp != NULL; lp = lp->next)
        {
            ThunarFile *file = THUNARFILE(lp->data);

            // insert file in the sorted position
            GSequenceIter *row = g_sequence_insert_sorted(
                                            store->rows, file,
                                            _listmodel_cmp_func, store);

            GtkTreeIter iter;
            GTK_TREE_ITER_INIT(iter, store->stamp, row);

            // tell the view about the new row

            GtkTreePath *path = gtk_tree_path_new_from_indices(
                                        g_sequence_iter_get_position(row), -1);

            gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, &iter);

            gtk_tree_path_free(path);
        }

        g_slist_free(store->hidden);

        store->hidden = NULL;
    }
    else
    {
        e_assert(store->hidden == NULL);

        // remove all hidden files
        GSequenceIter *row = g_sequence_get_begin_iter(store->rows);
        GSequenceIter *end = g_sequence_get_end_iter(store->rows);

        while (row != end)
        {
            GSequenceIter *next = g_sequence_iter_next(row);

            ThunarFile *file = g_sequence_get(row);

            if (th_file_is_hidden(file))
            {
                // store file in the list
                store->hidden = g_slist_prepend(store->hidden, g_object_ref(file));

                // setup path for "row-deleted"
                GtkTreePath *path = gtk_tree_path_new_from_indices(
                                        g_sequence_iter_get_position(row), -1);

                // remove file from the model
                g_sequence_remove(row);

                // notify the view(s)
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);

                gtk_tree_path_free(path);
            }

            row = next;
            e_assert(end == g_sequence_get_end_iter(store->rows));
        }
    }

    // notify listeners about the new setting
    g_object_freeze_notify(G_OBJECT(store));
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_NUM_FILES]);
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_SHOW_HIDDEN]);
    g_object_thaw_notify(G_OBJECT(store));
}

gboolean listmodel_get_file_size_binary(ListModel *store)
{
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);
    return store->file_size_binary;
}

void listmodel_set_file_size_binary(ListModel *store, gboolean file_size_binary)
{
    e_return_if_fail(IS_LISTMODEL(store));

    // normalize the setting
    file_size_binary = !!file_size_binary;

    // check if we have a new setting
    if (store->file_size_binary == file_size_binary)
        return;

    // apply the new setting
    store->file_size_binary = file_size_binary;

    // resort the model with the new setting
    _listmodel_sort(store);

    // notify listeners
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_FILE_SIZE_BINARY]);

    /* emit a "changed" signal for each row, so the display is
     * reloaded with the new binary file size setting */
    gtk_tree_model_foreach(GTK_TREE_MODEL(store),
                           ETMFOREACHFUNC gtk_tree_model_row_changed,
                           NULL);
}

// GtkTreeModel ---------------------------------------------------------------

static GtkTreeModelFlags listmodel_get_flags(GtkTreeModel *model)
{
    (void) model;

    return GTK_TREE_MODEL_ITERS_PERSIST | GTK_TREE_MODEL_LIST_ONLY;
}

static gint listmodel_get_n_columns(GtkTreeModel *model)
{
    (void) model;

    return THUNAR_N_COLUMNS;
}

static GType listmodel_get_column_type(GtkTreeModel *model, gint idx)
{
    (void) model;

    switch (idx)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_DATE_MODIFIED:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_GROUP:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_MIME_TYPE:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_NAME:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_OWNER:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_PERMISSIONS:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_SIZE:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_TYPE:
        return G_TYPE_STRING;

    case THUNAR_COLUMN_FILE:
        return TYPE_THUNARFILE;

    case THUNAR_COLUMN_FILE_NAME:
        return G_TYPE_STRING;
    }

    e_assert_not_reached();

    return G_TYPE_INVALID;
}

static gboolean listmodel_get_iter(GtkTreeModel *model, GtkTreeIter *iter,
                                   GtkTreePath *path)
{
    e_return_val_if_fail(gtk_tree_path_get_depth(path) > 0, FALSE);

    ListModel *store = LISTMODEL(model);
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    // determine the row for the path
    gint offset = gtk_tree_path_get_indices(path)[0];
    GSequenceIter *row = g_sequence_get_iter_at_pos(store->rows, offset);

    if (!g_sequence_iter_is_end(row))
    {
        GTK_TREE_ITER_INIT(*iter, store->stamp, row);

        return TRUE;
    }

    return FALSE;
}

static GtkTreePath* listmodel_get_path(GtkTreeModel *model, GtkTreeIter *iter)
{
    ListModel *store = LISTMODEL(model);
    e_return_val_if_fail(IS_LISTMODEL(store), NULL);
    e_return_val_if_fail(iter->stamp == store->stamp, NULL);

    gint idx = g_sequence_iter_get_position(iter->user_data);

    if (idx >= 0)
        return gtk_tree_path_new_from_indices(idx, -1);

    return NULL;
}

static void listmodel_get_value(GtkTreeModel *model, GtkTreeIter *iter,
                                gint column, GValue *value)
{
    e_return_if_fail(IS_LISTMODEL(model));
    e_return_if_fail(iter->stamp == (LISTMODEL(model))->stamp);

    ThunarFile *file = g_sequence_get(iter->user_data);
    e_return_if_fail(IS_THUNARFILE(file));

    gchar *str;
    ThunarGroup *group;
    ThunarUser *user;
    const gchar *name;
    const gchar *real_name;
    const gchar *content_type;

    switch (column)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
        g_value_init(value, G_TYPE_STRING);
        str = th_file_get_date_string(file, FILE_DATE_ACCESSED, LISTMODEL(model)->date_style, LISTMODEL(model)->date_custom_style);
        g_value_take_string(value, str);
        break;

    case THUNAR_COLUMN_DATE_MODIFIED:
        g_value_init(value, G_TYPE_STRING);
        str = th_file_get_date_string(file, FILE_DATE_MODIFIED, LISTMODEL(model)->date_style, LISTMODEL(model)->date_custom_style);
        g_value_take_string(value, str);
        break;

    case THUNAR_COLUMN_GROUP:
        g_value_init(value, G_TYPE_STRING);
        group = th_file_get_group(file);
        if (group != NULL)
        {
            g_value_set_string(value, group_get_name(group));
            g_object_unref(G_OBJECT(group));
        }
        else
        {
            g_value_set_static_string(value, _("Unknown"));
        }
        break;

    case THUNAR_COLUMN_MIME_TYPE:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, th_file_get_content_type(file));
        break;

    case THUNAR_COLUMN_NAME:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, th_file_get_display_name(file));
        break;

    case THUNAR_COLUMN_OWNER:
        g_value_init(value, G_TYPE_STRING);
        user = th_file_get_user(file);
        if (user != NULL)
        {
            // determine sane display name for the owner
            name = user_get_name(user);
            real_name = user_get_real_name(user);
            if (real_name != NULL)
            {
                if (strcmp(name, real_name) == 0)
                    str = g_strdup(name);
                else
                    str = g_strdup_printf("%s(%s)", real_name, name);
            }
            else
            {
                str = g_strdup(name);
            }
            g_value_take_string(value, str);
            g_object_unref(G_OBJECT(user));
        }
        else
        {
            g_value_set_static_string(value, _("Unknown"));
        }
        break;

    case THUNAR_COLUMN_PERMISSIONS:
        g_value_init(value, G_TYPE_STRING);
        g_value_take_string(value, th_file_get_mode_string(file));
        break;

    case THUNAR_COLUMN_SIZE:
        g_value_init(value, G_TYPE_STRING);
        g_value_take_string(value, th_file_get_size_string_formatted(file, LISTMODEL(model)->file_size_binary));
        break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
        g_value_init(value, G_TYPE_STRING);
        g_value_take_string(value, th_file_get_size_in_bytes_string(file));
        break;

    case THUNAR_COLUMN_TYPE:
        g_value_init(value, G_TYPE_STRING);
        if (th_file_is_symlink(file))
        {
            g_value_take_string(value,
                                g_strdup_printf(_("link to %s"),
                                                th_file_get_symlink_target(file)));
        }
        else
        {
            content_type = th_file_get_content_type(file);
            if (content_type != NULL)
                g_value_take_string(value, g_content_type_get_description(content_type));
        }
        break;

    case THUNAR_COLUMN_FILE:
        g_value_init(value, TYPE_THUNARFILE);
        g_value_set_object(value, file);
        break;

    case THUNAR_COLUMN_FILE_NAME:
        g_value_init(value, G_TYPE_STRING);
        g_value_set_static_string(value, th_file_get_display_name(file));
        break;

    default:
        e_assert_not_reached();
        break;
    }
}

static gboolean listmodel_iter_next(GtkTreeModel *model, GtkTreeIter *iter)
{
    e_return_val_if_fail(IS_LISTMODEL(model), FALSE);
    e_return_val_if_fail(iter->stamp ==(LISTMODEL(model))->stamp, FALSE);

    iter->user_data = g_sequence_iter_next(iter->user_data);
    return !g_sequence_iter_is_end(iter->user_data);
}

static gboolean listmodel_iter_children(GtkTreeModel *model, GtkTreeIter *iter,
                                        GtkTreeIter *parent)
{
    ListModel *store = LISTMODEL(model);
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    if (parent == NULL && g_sequence_get_length(store->rows) > 0)
    {
        GTK_TREE_ITER_INIT(*iter, store->stamp, g_sequence_get_begin_iter(store->rows));

        return TRUE;
    }

    return FALSE;
}

static gboolean listmodel_iter_has_child(GtkTreeModel *model, GtkTreeIter *iter)
{
    (void) model;
    (void) iter;

    return FALSE;
}

static gint listmodel_iter_n_children(GtkTreeModel *model, GtkTreeIter *iter)
{
    ListModel *store = LISTMODEL(model);
    e_return_val_if_fail(IS_LISTMODEL(store), 0);

    return (iter == NULL) ? g_sequence_get_length(store->rows) : 0;
}

static gboolean listmodel_iter_nth_child(GtkTreeModel *model, GtkTreeIter *iter,
                                         GtkTreeIter *parent, gint n)
{
    ListModel *store = LISTMODEL(model);
    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    if (parent != NULL)
        return FALSE;

    GSequenceIter *row = g_sequence_get_iter_at_pos(store->rows, n);
    if (g_sequence_iter_is_end(row))
        return FALSE;

    GTK_TREE_ITER_INIT(*iter, store->stamp, row);

    return TRUE;
}

static gboolean listmodel_iter_parent(GtkTreeModel *model, GtkTreeIter *iter,
                                      GtkTreeIter *child)
{
    (void) model;
    (void) iter;
    (void) child;

    return FALSE;
}

// GtkTreeDragDest ------------------------------------------------------------

static gboolean listmodel_drag_data_received(GtkTreeDragDest *dest,
                                             GtkTreePath     *path,
                                             GtkSelectionData *data)
{
    (void) dest;
    (void) path;
    (void) data;

    return FALSE;
}

static gboolean listmodel_row_drop_possible(GtkTreeDragDest *dest,
                                            GtkTreePath     *path,
                                            GtkSelectionData *data)
{
    (void) dest;
    (void) path;
    (void) data;

    return FALSE;
}

// GtkTreeSortable ------------------------------------------------------------

static gboolean listmodel_get_sort_column_id(GtkTreeSortable *sortable,
                                             gint            *sort_column_id,
                                             GtkSortType     *order)
{
    ListModel *store = LISTMODEL(sortable);

    e_return_val_if_fail(IS_LISTMODEL(store), FALSE);

    if (store->sort_func == _sort_by_mime_type)
        *sort_column_id = THUNAR_COLUMN_MIME_TYPE;
    else if (store->sort_func == th_file_compare_by_name)
        *sort_column_id = THUNAR_COLUMN_NAME;
    else if (store->sort_func == _sort_by_permissions)
        *sort_column_id = THUNAR_COLUMN_PERMISSIONS;
    else if (store->sort_func == _sort_by_size)
        *sort_column_id = THUNAR_COLUMN_SIZE;
    else if (store->sort_func == _sort_by_size_in_bytes)
        *sort_column_id = THUNAR_COLUMN_SIZE_IN_BYTES;
    else if (store->sort_func == _sort_by_date_accessed)
        *sort_column_id = THUNAR_COLUMN_DATE_ACCESSED;
    else if (store->sort_func == _sort_by_date_modified)
        *sort_column_id = THUNAR_COLUMN_DATE_MODIFIED;
    else if (store->sort_func == _sort_by_type)
        *sort_column_id = THUNAR_COLUMN_TYPE;
    else if (store->sort_func == _sort_by_owner)
        *sort_column_id = THUNAR_COLUMN_OWNER;
    else if (store->sort_func == _sort_by_group)
        *sort_column_id = THUNAR_COLUMN_GROUP;
    else
        e_assert_not_reached();

    if (order != NULL)
    {
        if (store->sort_sign > 0)
            *order = GTK_SORT_ASCENDING;
        else
            *order = GTK_SORT_DESCENDING;
    }

    return TRUE;
}

static void listmodel_set_sort_column_id(GtkTreeSortable *sortable,
                                         gint sort_column_id,
                                         GtkSortType order)
{
    ListModel *store = LISTMODEL(sortable);

    e_return_if_fail(IS_LISTMODEL(store));

    switch (sort_column_id)
    {
    case THUNAR_COLUMN_DATE_ACCESSED:
        store->sort_func = _sort_by_date_accessed;
        break;

    case THUNAR_COLUMN_DATE_MODIFIED:
        store->sort_func = _sort_by_date_modified;
        break;

    case THUNAR_COLUMN_GROUP:
        store->sort_func = _sort_by_group;
        break;

    case THUNAR_COLUMN_MIME_TYPE:
        store->sort_func = _sort_by_mime_type;
        break;

    case THUNAR_COLUMN_FILE_NAME:
    case THUNAR_COLUMN_NAME:
        store->sort_func = th_file_compare_by_name;
        break;

    case THUNAR_COLUMN_OWNER:
        store->sort_func = _sort_by_owner;
        break;

    case THUNAR_COLUMN_PERMISSIONS:
        store->sort_func = _sort_by_permissions;
        break;

    case THUNAR_COLUMN_SIZE:
        store->sort_func = _sort_by_size;
        break;

    case THUNAR_COLUMN_SIZE_IN_BYTES:
        store->sort_func = _sort_by_size_in_bytes;
        break;

    case THUNAR_COLUMN_TYPE:
        store->sort_func = _sort_by_type;
        break;

    default:
        e_assert_not_reached();
    }

    // new sort sign
    store->sort_sign =(order == GTK_SORT_ASCENDING) ? 1 : -1;

    // re-sort the store
    _listmodel_sort(store);

    // notify listining parties
    gtk_tree_sortable_sort_column_changed(sortable);
}

static void listmodel_set_sort_func(GtkTreeSortable        *sortable,
                                    gint                   sort_column_id,
                                    GtkTreeIterCompareFunc func,
                                    gpointer               data,
                                    GDestroyNotify         destroy)
{
    (void) sortable;
    (void) sort_column_id;
    (void) func;
    (void) data;
    (void) destroy;

    g_critical("ListModel has sorting facilities built-in!");
}

static void listmodel_set_default_sort_func(GtkTreeSortable        *sortable,
                                            GtkTreeIterCompareFunc func,
                                            gpointer               data,
                                            GDestroyNotify         destroy)
{
    (void) sortable;
    (void) func;
    (void) data;
    (void) destroy;

    g_critical("ListModel has sorting facilities built-in!");
}

static gboolean listmodel_has_default_sort_func(GtkTreeSortable *sortable)
{
    (void) sortable;

    return FALSE;
}

// Sorting --------------------------------------------------------------------

static void _listmodel_sort(ListModel *store)
{
    e_return_if_fail(IS_LISTMODEL(store));

    gint length = g_sequence_get_length(store->rows);
    if (length <= 1)
        return;

    GSequenceIter **old_order;
    gint *new_order;

    // be sure to not overuse the stack
    if (length < 2000)
    {
        old_order = g_newa(GSequenceIter*, length);
        new_order = g_newa(gint, length);
    }
    else
    {
        old_order = g_new(GSequenceIter*, length);
        new_order = g_new(gint, length);
    }

    // store old order
    GSequenceIter *row = g_sequence_get_begin_iter(store->rows);

    for (gint n = 0; n < length; ++n)
    {
        old_order[n] = row;
        row = g_sequence_iter_next(row);
    }

    // sort
    g_sequence_sort(store->rows, _listmodel_cmp_func, store);

    // new_order[newpos] = oldpos
    for (gint n = 0; n < length; ++n)
    {
        new_order[g_sequence_iter_get_position(old_order[n])] = n;
    }

    // tell the view about the new item order
    GtkTreePath *path = gtk_tree_path_new_first();

    gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, NULL, new_order);
    gtk_tree_path_free(path);

    // clean up if we used the heap
    if (length >= 2000)
    {
        g_free(old_order);
        g_free(new_order);
    }
}

static gint _listmodel_cmp_func(gconstpointer a, gconstpointer b,
                                gpointer user_data)
{
    e_return_val_if_fail(IS_THUNARFILE(a), 0);
    e_return_val_if_fail(IS_THUNARFILE(b), 0);

    ListModel *store = LISTMODEL(user_data);

    if (store->sort_folders_first)
    {
        gboolean isdir_a = th_file_is_directory(a);
        gboolean isdir_b = th_file_is_directory(b);

        if (isdir_a != isdir_b)
            return isdir_a ? -1 : 1;
    }

    return (store->sort_func(a, b, store->sort_case_sensitive) * store->sort_sign);
}

static gint _sort_by_date_accessed(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive)
{
    guint64 date_a = th_file_get_date(a, FILE_DATE_ACCESSED);
    guint64 date_b = th_file_get_date(b, FILE_DATE_ACCESSED);

    if (date_a < date_b)
        return -1;
    else if (date_a > date_b)
        return 1;

    return th_file_compare_by_name(a, b, case_sensitive);
}

static gint _sort_by_date_modified(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive)
{
    guint64 date_a = th_file_get_date(a, FILE_DATE_MODIFIED);
    guint64 date_b = th_file_get_date(b, FILE_DATE_MODIFIED);

    if (date_a < date_b)
        return -1;
    else if (date_a > date_b)
        return 1;

    return th_file_compare_by_name(a, b, case_sensitive);
}

static gint _sort_by_group(const ThunarFile *a, const ThunarFile *b,
                           gboolean case_sensitive)
{
    if (th_file_get_info(a) == NULL || th_file_get_info(b) == NULL)
        return th_file_compare_by_name(a, b, case_sensitive);

    ThunarGroup *group_a;
    ThunarGroup *group_b;
    guint32      gid_a;
    guint32      gid_b;
    gint         result;
    const gchar *name_a;
    const gchar *name_b;

    group_a = th_file_get_group(a);
    group_b = th_file_get_group(b);

    if (group_a != NULL && group_b != NULL)
    {
        name_a = group_get_name(group_a);
        name_b = group_get_name(group_b);

        if (!case_sensitive)
            result = strcasecmp(name_a, name_b);
        else
            result = strcmp(name_a, name_b);
    }
    else
    {
        gid_a = g_file_info_get_attribute_uint32(th_file_get_info(a),
                G_FILE_ATTRIBUTE_UNIX_GID);
        gid_b = g_file_info_get_attribute_uint32(th_file_get_info(b),
                G_FILE_ATTRIBUTE_UNIX_GID);

        result = CLAMP((gint) gid_a -(gint) gid_b, -1, 1);
    }

    if (group_a != NULL)
        g_object_unref(group_a);

    if (group_b != NULL)
        g_object_unref(group_b);

    if (result == 0)
        return th_file_compare_by_name(a, b, case_sensitive);
    else
        return result;
}

static gint _sort_by_mime_type(const ThunarFile *a, const ThunarFile *b,
                               gboolean case_sensitive)
{
    const gchar *content_type_a;
    const gchar *content_type_b;
    gint         result;

    content_type_a = th_file_get_content_type(THUNARFILE(a));
    content_type_b = th_file_get_content_type(THUNARFILE(b));

    if (content_type_a == NULL)
        content_type_a = "";
    if (content_type_b == NULL)
        content_type_b = "";

    result = strcasecmp(content_type_a, content_type_b);

    if (result == 0)
        result = th_file_compare_by_name(a, b, case_sensitive);

    return result;
}

static gint _sort_by_owner(const ThunarFile *a, const ThunarFile *b,
                           gboolean case_sensitive)
{
    const gchar *name_a;
    const gchar *name_b;
    ThunarUser  *user_a;
    ThunarUser  *user_b;
    guint32      uid_a;
    guint32      uid_b;
    gint         result;

    if (th_file_get_info(a) == NULL || th_file_get_info(b) == NULL)
        return th_file_compare_by_name(a, b, case_sensitive);

    user_a = th_file_get_user(a);
    user_b = th_file_get_user(b);

    if (user_a != NULL && user_b != NULL)
    {
        // compare the system names
        name_a = user_get_name(user_a);
        name_b = user_get_name(user_b);

        if (!case_sensitive)
            result = strcasecmp(name_a, name_b);
        else
            result = strcmp(name_a, name_b);
    }
    else
    {
        uid_a = g_file_info_get_attribute_uint32(th_file_get_info(a),
                G_FILE_ATTRIBUTE_UNIX_UID);
        uid_b = g_file_info_get_attribute_uint32(th_file_get_info(b),
                G_FILE_ATTRIBUTE_UNIX_UID);

        result = CLAMP((gint) uid_a -(gint) uid_b, -1, 1);
    }

    if (result == 0)
        return th_file_compare_by_name(a, b, case_sensitive);
    else
        return result;
}

static gint _sort_by_permissions(const ThunarFile *a, const ThunarFile *b,
                                 gboolean case_sensitive)
{
    ThunarFileMode mode_a;
    ThunarFileMode mode_b;

    mode_a = th_file_get_mode(a);
    mode_b = th_file_get_mode(b);

    if (mode_a < mode_b)
        return -1;
    else if (mode_a > mode_b)
        return 1;

    return th_file_compare_by_name(a, b, case_sensitive);
}

static gint _sort_by_size(const ThunarFile *a, const ThunarFile *b,
                          gboolean case_sensitive)
{
    guint64 size_a;
    guint64 size_b;

    size_a = th_file_get_size(a);
    size_b = th_file_get_size(b);

    if (size_a < size_b)
        return -1;
    else if (size_a > size_b)
        return 1;

    return th_file_compare_by_name(a, b, case_sensitive);
}

static gint _sort_by_size_in_bytes(const ThunarFile *a, const ThunarFile *b,
                                   gboolean case_sensitive)
{
    return _sort_by_size(a, b, case_sensitive);
}

static gint _sort_by_type(const ThunarFile *a, const ThunarFile *b,
                          gboolean case_sensitive)
{
    const gchar *content_type_a;
    const gchar *content_type_b;
    gchar       *description_a = NULL;
    gchar       *description_b = NULL;
    gint         result;

    /* we alter the description of symlinks here because they are
     * displayed as "link to ..." in the detailed list view as well */

    if (th_file_is_symlink(a))
    {
        description_a = g_strdup_printf(_("link to %s"),
                                         th_file_get_symlink_target(a));
    }
    else
    {
        content_type_a = th_file_get_content_type(THUNARFILE(a));
        description_a = g_content_type_get_description(content_type_a);
    }

    if (th_file_is_symlink(b))
    {
        description_b = g_strdup_printf(_("link to %s"),
                                         th_file_get_symlink_target(b));
    }
    else
    {
        content_type_b = th_file_get_content_type(THUNARFILE(b));
        description_b = g_content_type_get_description(content_type_b);
    }

    // avoid calling strcasecmp with NULL parameters
    if (description_a == NULL || description_b == NULL)
    {
        g_free(description_a);
        g_free(description_b);

        return 0;
    }

    if (!case_sensitive)
        result = strcasecmp(description_a, description_b);
    else
        result = strcmp(description_a, description_b);

    g_free(description_a);
    g_free(description_b);

    if (result == 0)
        return th_file_compare_by_name(a, b, case_sensitive);
    else
        return result;
}

// File Monitors --------------------------------------------------------------

static void _listmodel_file_changed(FileMonitor *file_monitor, ThunarFile *file,
                                    ListModel *store)
{
    e_return_if_fail(IS_FILEMONITOR(file_monitor));
    e_return_if_fail(IS_LISTMODEL(store));
    e_return_if_fail(IS_THUNARFILE(file));

    GSequenceIter *row = g_sequence_get_begin_iter(store->rows);
    GSequenceIter *end = g_sequence_get_end_iter(store->rows);

    gint           pos_after;
    gint           pos_before = 0;
    gint          *new_order;
    gint           length;
    gint           i, j;
    GtkTreePath   *path;
    GtkTreeIter    iter;

    while (row != end)
    {
        if (g_sequence_get(row) == file)
        {
            // generate the iterator for this row
            GTK_TREE_ITER_INIT(iter, store->stamp, row);

            e_assert(pos_before == g_sequence_iter_get_position(row));

            // check if the sorting changed
            g_sequence_sort_changed(row, _listmodel_cmp_func, store);
            pos_after = g_sequence_iter_get_position(row);

            if (pos_after != pos_before)
            {
                // do swap sorting here since its much faster than a complete sort
                length = g_sequence_get_length(store->rows);
                if (length < 2000)
                    new_order = g_newa(gint, length);
                else
                    new_order = g_new(gint, length);

                // new_order[newpos] = oldpos
                for (i = 0, j = 0; i < length; ++i)
                {
                    if (i == pos_after)
                    {
                        new_order[i] = pos_before;
                    }
                    else
                    {
                        if (j == pos_before)
                            j++;
                        new_order[i] = j++;
                    }
                }

                // tell the view about the new item order
                path = gtk_tree_path_new_first();
                gtk_tree_model_rows_reordered(GTK_TREE_MODEL(store), path, NULL, new_order);
                gtk_tree_path_free(path);

                // clean up if we used the heap
                if (length >= 2000)
                    g_free(new_order);
            }

            // notify the view that it has to redraw the file
            path = gtk_tree_path_new_from_indices(pos_before, -1);
            gtk_tree_model_row_changed(GTK_TREE_MODEL(store), path, &iter);
            gtk_tree_path_free(path);
            break;
        }

        row = g_sequence_iter_next(row);
        pos_before++;
    }
}

static void _listmodel_folder_destroy(ThunarFolder *folder, ListModel *store)
{
    e_return_if_fail(IS_LISTMODEL(store));
    e_return_if_fail(IS_THUNARFOLDER(folder));

    listmodel_set_folder(store, NULL);

    // TODO: What to do when the folder is deleted?
}

static void _listmodel_folder_error(ThunarFolder *folder, const GError *error,
                                    ListModel *store)
{
    e_return_if_fail(IS_LISTMODEL(store));
    e_return_if_fail(IS_THUNARFOLDER(folder));
    e_return_if_fail(error != NULL);

    // forward the error signal
    g_signal_emit(G_OBJECT(store), _listmodel_signals[ERROR], 0, error);

    // reset the current folder
    listmodel_set_folder(store, NULL);
}

static void _listmodel_files_added(ThunarFolder *folder, GList *files,
                                   ListModel *store)
{
    (void) folder;

    /* we use a simple trick here to avoid allocating
     * GtkTreePath's again and again, by simply accessing
     * the indices directly and only modifying the first
     * item in the integer array... looks a hack, eh? */
    GtkTreePath *path = gtk_tree_path_new_first();
    gint *indices = gtk_tree_path_get_indices(path);

    // check if we have any handlers connected for "row-inserted"
    gboolean has_handler = g_signal_has_handler_pending(
                                                G_OBJECT(store),
                                                store->row_inserted_id, 0, FALSE);
    // process all added files
    for (GList *lp = files; lp != NULL; lp = lp->next)
    {
        // take a reference on that file
        ThunarFile    *file;
        file = THUNARFILE(g_object_ref(G_OBJECT(lp->data)));
        e_return_if_fail(IS_THUNARFILE(file));

        // check if the file should be hidden
        if (!store->show_hidden && th_file_is_hidden(file))
        {
            store->hidden = g_slist_prepend(store->hidden, file);
        }
        else
        {
            // insert the file
            GSequenceIter *row;
            row = g_sequence_insert_sorted(store->rows, file,
                                            _listmodel_cmp_func, store);

            if (has_handler)
            {
                // generate an iterator for the new item
                GtkTreeIter    iter;
                GTK_TREE_ITER_INIT(iter, store->stamp, row);

                indices[0] = g_sequence_iter_get_position(row);
                gtk_tree_model_row_inserted(GTK_TREE_MODEL(store), path, &iter);
            }
        }
    }

    // release the path
    gtk_tree_path_free(path);

    // number of visible files may have changed
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_NUM_FILES]);
}

static void _listmodel_files_removed(ThunarFolder *folder, GList *files,
                                     ListModel *store)
{
    (void) folder;

    // drop all the referenced files from the model
    for (GList *lp = files; lp != NULL; lp = lp->next)
    {
        gboolean found;

        GSequenceIter *row = g_sequence_get_begin_iter(store->rows);
        GSequenceIter *end = g_sequence_get_end_iter(store->rows);

        found = FALSE;

        while (row != end)
        {
            GSequenceIter *next = g_sequence_iter_next(row);

            if (g_sequence_get(row) == lp->data)
            {
                // setup path for "row-deleted"
                GtkTreePath *path = gtk_tree_path_new_from_indices(
                                        g_sequence_iter_get_position(row), -1);

                // remove file from the model
                g_sequence_remove(row);

                // notify the view(s)
                gtk_tree_model_row_deleted(GTK_TREE_MODEL(store), path);
                gtk_tree_path_free(path);

                // no need to look in the hidden files
                found = TRUE;

                break;
            }

            row = next;
        }

        // check if the file was found
        if (!found)
        {
            // file is hidden
            e_assert(g_slist_find(store->hidden, lp->data) != NULL);
            store->hidden = g_slist_remove(store->hidden, lp->data);
            g_object_unref(G_OBJECT(lp->data));
        }
    }

    // this probably changed
    g_object_notify_by_pspec(G_OBJECT(store), _listmodel_props[PROP_NUM_FILES]);
}

// Public ---------------------------------------------------------------------

ListModel* listmodel_new()
{
    return g_object_new(TYPE_LISTMODEL, NULL);
}

ThunarFile* listmodel_get_file(ListModel *store, GtkTreeIter *iter)
{
    // g_object_unref

    e_return_val_if_fail(IS_LISTMODEL(store), NULL);
    e_return_val_if_fail(iter->stamp == store->stamp, NULL);

    return g_object_ref(g_sequence_get(iter->user_data));
}

/**
 * thunar_list_model_get_paths_for_files:
 * @store : a #ListModel instance.
 * @files : a list of #ThunarFile<!---->s.
 *
 * Determines the list of #GtkTreePath<!---->s for the #ThunarFile<!---->s
 * found in the @files list. If a #ThunarFile from the @files list is not
 * available in @store, no #GtkTreePath will be returned for it. So, in effect,
 * only #GtkTreePath<!---->s for the subset of @files available in @store will
 * be returned.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_free_full(list,(GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s for @files.
 **/
GList* listmodel_get_paths_for_files(ListModel *store, GList *files)
{
    GList         *paths = NULL;
    GSequenceIter *row;
    GSequenceIter *end;
    gint           i = 0;

    e_return_val_if_fail(IS_LISTMODEL(store), NULL);

    row = g_sequence_get_begin_iter(store->rows);
    end = g_sequence_get_end_iter(store->rows);

    // find the rows for the given files
    while(row != end)
    {
        if (g_list_find(files, g_sequence_get(row)) != NULL)
        {
            e_assert(i == g_sequence_iter_get_position(row));
            paths = g_list_prepend(paths, gtk_tree_path_new_from_indices(i, -1));
        }

        row = g_sequence_iter_next(row);
        i++;
    }

    return paths;
}

/**
 * thunar_list_model_get_paths_for_pattern:
 * @store   : a #ListModel instance.
 * @pattern : the pattern to match.
 *
 * Looks up all rows in the @store that match @pattern and returns
 * a list of #GtkTreePath<!---->s corresponding to the rows.
 *
 * The caller is responsible to free the returned list using:
 * <informalexample><programlisting>
 * g_list_free_full(list,(GDestroyNotify) gtk_tree_path_free);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GtkTreePath<!---->s that match @pattern.
 **/
GList* listmodel_get_paths_for_pattern(ListModel *store, const gchar *pattern)
{
    GPatternSpec  *pspec;
    GList         *paths = NULL;
    GSequenceIter *row;
    GSequenceIter *end;
    ThunarFile    *file;
    gint           i = 0;

    e_return_val_if_fail(IS_LISTMODEL(store), NULL);
    e_return_val_if_fail(g_utf8_validate(pattern, -1, NULL), NULL);

    // compile the pattern
    pspec = g_pattern_spec_new(pattern);

    row = g_sequence_get_begin_iter(store->rows);
    end = g_sequence_get_end_iter(store->rows);

    // find all rows that match the given pattern
    while(row != end)
    {
        file = g_sequence_get(row);
        if (g_pattern_match_string(pspec, th_file_get_display_name(file)))
        {
            e_assert(i == g_sequence_iter_get_position(row));
            paths = g_list_prepend(paths, gtk_tree_path_new_from_indices(i, -1));
        }

        row = g_sequence_iter_next(row);
        i++;
    }

    // release the pattern
    g_pattern_spec_free(pspec);

    return paths;
}

gchar* listmodel_get_statusbar_text(ListModel *store, GList *selected_items)
{
    // g_free

    const gchar       *content_type;
    const gchar       *original_path;
    GtkTreeIter        iter;
    ThunarFile        *file;
    guint64            size;
    GList             *lp;
    gchar             *absolute_path;
    gchar             *fspace_string;
    gchar             *display_name;
    gchar             *size_string;
    gchar             *text;
    gchar             *s;
    gint               height;
    gint               width;
    gchar             *description;
    GSequenceIter     *row;
    GSequenceIter     *end;
    gboolean           show_file_size_binary_format;
    GList             *relevant_files = NULL;

    e_return_val_if_fail(IS_LISTMODEL(store), NULL);

    show_file_size_binary_format = listmodel_get_file_size_binary(store);

    if (selected_items == NULL) // nothing selected
    {
        // build a GList of all files
        end = g_sequence_get_end_iter(store->rows);
        for(row = g_sequence_get_begin_iter(store->rows); row != end; row = g_sequence_iter_next(row))
        {
            relevant_files = g_list_append(relevant_files, g_sequence_get(row));
        }

        // try to determine a file for the current folder
        file =(store->folder != NULL) ? th_folder_get_thfile(store->folder) : NULL;

        // check if we can determine the amount of free space for the volume
        if (file != NULL
            && e_file_get_free_space(th_file_get_file(file), &size, NULL))
        {
            size_string = _listmodel_get_statusbar_text_for_files(relevant_files, show_file_size_binary_format);

            // humanize the free space
            fspace_string = g_format_size_full(size, show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);

            text = g_strdup_printf(_("%s, Free space: %s"), size_string, fspace_string);

            // cleanup
            g_free(size_string);
            g_free(fspace_string);
        }
        else
        {
            text = _listmodel_get_statusbar_text_for_files(relevant_files, show_file_size_binary_format);
        }
        g_list_free(relevant_files);
    }
    else if (selected_items->next == NULL) // only one item selected
    {
        // resolve the iter for the single path
        gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, selected_items->data);

        // get the file for the given iter
        file = g_sequence_get(iter.user_data);

        // determine the content type of the file
        content_type = th_file_get_content_type(file);

        if (content_type != NULL && g_str_equal(content_type, "inode/symlink"))
        {
            text = g_strdup_printf(_("\"%s\": broken link"), th_file_get_display_name(file));
        }
        else if (th_file_is_symlink(file))
        {
            size_string = th_file_get_size_string_long(file, show_file_size_binary_format);
            text = g_strdup_printf(_("\"%s\": %s link to %s"), th_file_get_display_name(file),
                                    size_string, th_file_get_symlink_target(file));
            g_free(size_string);
        }
        else if (th_file_get_filetype(file) == G_FILE_TYPE_SHORTCUT)
        {
            text = g_strdup_printf(_("\"%s\": shortcut"), th_file_get_display_name(file));
        }
        else if (th_file_get_filetype(file) == G_FILE_TYPE_MOUNTABLE)
        {
            text = g_strdup_printf(_("\"%s\": mountable"), th_file_get_display_name(file));
        }
        else if (th_file_is_regular(file))
        {
            description = g_content_type_get_description(content_type);
            size_string = th_file_get_size_string_long(file, show_file_size_binary_format);
            // I18N, first %s is the display name of the file, 2nd the file size, 3rd the content type
            text = g_strdup_printf(_("\"%s\": %s %s"), th_file_get_display_name(file),
                                    size_string, description);
            g_free(description);
            g_free(size_string);
        }
        else
        {
            description = g_content_type_get_description(content_type);
            // I18N, first %s is the display name of the file, second the content type
            text = g_strdup_printf(_("\"%s\": %s"), th_file_get_display_name(file), description);
            g_free(description);
        }

        // append the original path(if any)
        original_path = th_file_get_original_path(file);
        if (original_path != NULL)
        {
            // append the original path to the statusbar text
            display_name = g_filename_display_name(original_path);
            s = g_strdup_printf("%s, %s %s", text, _("Original Path:"), display_name);
            g_free(display_name);
            g_free(text);
            text = s;
        }
        else if (th_file_is_local(file)
                 && th_file_is_regular(file)
                 && g_str_has_prefix(content_type, "image/")) // bug #2913
        {
            /* check if the size should be visible in the statusbar, disabled by
             * default to avoid high i/o  */

            gboolean show_image_size = false;

            if (show_image_size)
            {
                // check if we can determine the dimension of this file(only for image files)
                absolute_path = g_file_get_path(th_file_get_file(file));
                if (absolute_path != NULL
                        && gdk_pixbuf_get_file_info(absolute_path, &width, &height) != NULL)
                {
                    // append the image dimensions to the statusbar text
                    s = g_strdup_printf("%s, %s %dx%d", text, _("Image Size:"), width, height);
                    g_free(text);
                    text = s;
                }

                g_free(absolute_path);
            }
        }
    }
    else // more than one item selected
    {
        // build GList of files from selection
        for (lp = selected_items; lp != NULL; lp = lp->next)
        {
            gtk_tree_model_get_iter(GTK_TREE_MODEL(store), &iter, lp->data);
            relevant_files = g_list_append(relevant_files, g_sequence_get(iter.user_data));
        }

        size_string = _listmodel_get_statusbar_text_for_files(relevant_files, show_file_size_binary_format);
        text = g_strdup_printf(_("Selection: %s"), size_string);
        g_free(size_string);
        g_list_free(relevant_files);
    }

    return text;
}

static gchar* _listmodel_get_statusbar_text_for_files(
                                        GList *files,
                                        gboolean show_file_size_binary_format)
{
    // g_free

    guint64  size_summary = 0;
    gint     folder_count = 0;
    gint     non_folder_count = 0;
    GList   *lp;
    gchar   *size_string;
    gchar   *text;
    gchar   *folder_text = NULL;
    gchar   *non_folder_text = NULL;

    // analyze files
    for (lp = files; lp != NULL; lp = lp->next)
    {
        if (th_file_is_directory(lp->data))
        {
            folder_count++;
        }
        else
        {
            non_folder_count++;
            if (th_file_is_regular(lp->data))
                size_summary += th_file_get_size(lp->data);
        }
    }

    if (non_folder_count > 0)
    {
        size_string = g_format_size_full(size_summary, G_FORMAT_SIZE_LONG_FORMAT |(show_file_size_binary_format ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT));
        non_folder_text = g_strdup_printf(ngettext("%d file: %s",
                                           "%d files: %s",
                                           non_folder_count), non_folder_count, size_string);
        g_free(size_string);
    }

    if (folder_count > 0)
    {
        folder_text = g_strdup_printf(ngettext("%d folder",
                                       "%d folders",
                                       folder_count), folder_count);
    }

    if (folder_text == NULL && non_folder_text == NULL)
        text = g_strdup_printf(_("0 items"));
    else if (folder_text == NULL)
        text = non_folder_text;
    else if (non_folder_text == NULL)
        text = folder_text;
    else
    {
        /* This is marked for translation in case a localizer
         * needs to change ", " to something else. The comma
         * is between the message about the number of folders
         * and the number of items in the selection */
        // TRANSLATORS: string moved from line 2573 to here
        text = g_strdup_printf(_("%s, %s"), folder_text, non_folder_text);
        g_free(folder_text);
        g_free(non_folder_text);
    }
    return text;
}


