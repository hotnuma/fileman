/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
#include <permbox.h>

#include <dialogs.h>
#include <io_jobs.h>
#include <gtk_ext.h>
#include <pango_ext.h>

// PermissionBox --------------------------------------------------------------

static void permbox_finalize(GObject *object);
static void permbox_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec);
static void permbox_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec);
static GList* _permbox_get_files(PermissionBox *permbox);
static void _permbox_set_files(PermissionBox *permbox, GList *files);
static gboolean _permbox_row_separator(GtkTreeModel *treemodel,
                                       GtkTreeIter *iter, gpointer data);

// Events ---------------------------------------------------------------------

static void _permbox_file_changed(PermissionBox *permbox);
static gint _group_compare(gconstpointer group_a, gconstpointer group_b,
                           gpointer group_primary);
static gboolean permbox_has_fixable_directory(PermissionBox *permbox);
static gboolean permbox_is_fixable_directory(ThunarFile *file);

static void _permbox_access_changed(PermissionBox *permbox, GtkWidget *combo);
static gboolean permbox_has_directory(PermissionBox *permbox);
static void _permbox_group_changed(PermissionBox *permbox, GtkWidget *combo);

static void _permbox_program_toggled(PermissionBox *permbox, GtkWidget *button);
static void _permbox_fixperm_clicked(PermissionBox *permbox, GtkWidget *button);
static void _permbox_job_cancel(PermissionBox *permbox);

// Change ---------------------------------------------------------------------

static void _permbox_change_group(PermissionBox *permbox, guint32 gid);
static gboolean _permbox_change_mode(PermissionBox *permbox,
                                     ThunarFileMode dir_mask,
                                     ThunarFileMode dir_mode,
                                     ThunarFileMode file_mask,
                                     ThunarFileMode file_mode);
static GList* permbox_get_file_list(PermissionBox *permbox);
// Ask recusive dialog
static gint _permbox_ask_recursive(PermissionBox *permbox);

// Job ------------------------------------------------------------------------

static void _permbox_job_start(PermissionBox *permbox, ThunarJob *job,
                               gboolean recursive);
static ThunarJobResponse _permbox_job_ask(PermissionBox *permbox,
                                          const gchar *message,
                                          ThunarJobResponse choices,
                                          ThunarJob *job);
static void _permbox_job_error(PermissionBox *permbox, GError *error,
                               ThunarJob *job);
static void _permbox_job_finished(PermissionBox *permbox, ThunarJob *job);
static void _permbox_job_percent(PermissionBox *permbox, gdouble percent,
                                 ThunarJob *job);

static gboolean _permbox_row_separator(GtkTreeModel *model, GtkTreeIter *iter,
                                       gpointer data);

// PermissionBox --------------------------------------------------------------
enum
{
    THUNAR_PERMISSIONS_STORE_COLUMN_NAME,
    THUNAR_PERMISSIONS_STORE_COLUMN_GID,
    THUNAR_PERMISSIONS_STORE_N_COLUMNS,
};

enum
{
    PROP_0,
    PROP_FILES,
    PROP_MUTABLE,
};

struct _PermissionBoxClass
{
    GtkBoxClass __parent__;
};

struct _PermissionBox
{
    GtkBox     __parent__;

    GList      *files;

    // the main grid widget, which contains everything but the job control stuff
    GtkWidget  *grid;

    GtkWidget  *user_label;
    GtkWidget  *group_combo;
    GtkWidget  *access_combos[3];
    GtkWidget  *program_button;
    GtkWidget  *fixperm_label;
    GtkWidget  *fixperm_button;

    // job control stuff
    ThunarJob  *job;
    GtkWidget  *job_progress;
};

G_DEFINE_TYPE(PermissionBox, permbox, GTK_TYPE_BOX)

static void permbox_class_init(PermissionBoxClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = permbox_finalize;
    gobject_class->get_property = permbox_get_property;
    gobject_class->set_property = permbox_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_FILES,
                                    g_param_spec_boxed(
                                        "files",
                                        "files",
                                        "files",
                                        TYPE_FILEINFOLIST,
                                        E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_MUTABLE,
                                    g_param_spec_boolean(
                                        "mutable",
                                        "mutable",
                                        "mutable",
                                        FALSE,
                                        E_PARAM_READABLE));
}

static void permbox_init(PermissionBox *permbox)
{
    gtk_container_set_border_width(GTK_CONTAINER(permbox), 12);

    gtk_orientable_set_orientation(GTK_ORIENTABLE(permbox),
                                   GTK_ORIENTATION_VERTICAL);

    // allocate the shared renderer for the various combo boxes
    GtkCellRenderer *renderer_text;
    renderer_text = gtk_cell_renderer_text_new();

    permbox->grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(permbox->grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(permbox->grid), 6);
    gtk_box_pack_start(GTK_BOX(permbox), permbox->grid, TRUE, TRUE, 0);
    gtk_widget_show(permbox->grid);

    gint row = 0;

    GtkWidget *label = gtk_label_new(_("Owner:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    GtkWidget       *hbox;
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_widget_set_hexpand(hbox, TRUE);
    gtk_grid_attach(GTK_GRID(permbox->grid), hbox, 1, row, 1, 1);
    gtk_widget_show(hbox);

    permbox->user_label = gtk_label_new(_("Unknown"));
    gtk_label_set_xalign(GTK_LABEL(permbox->user_label), 0.0f);
    gtk_box_pack_start(GTK_BOX(hbox), permbox->user_label, TRUE, TRUE, 0);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->user_label);
    gtk_widget_show(permbox->user_label);

    row += 1;

    label = gtk_label_new_with_mnemonic(_("_Access:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    permbox->access_combos[2] = gtk_combo_box_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), permbox->access_combos[2]);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(permbox->access_combos[2]),
            renderer_text, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(permbox->access_combos[2]),
            renderer_text, "text", 0);

    //exo_binding_new(G_OBJECT(permbox), "mutable", G_OBJECT(permbox->access_combos[2]), "sensitive");

    g_object_bind_property(G_OBJECT(permbox), "mutable",
                           G_OBJECT (permbox->access_combos[2]), "sensitive",
                           G_BINDING_SYNC_CREATE);

    g_signal_connect_swapped(G_OBJECT(permbox->access_combos[2]), "changed",
                             G_CALLBACK(_permbox_access_changed), permbox);
    gtk_widget_set_hexpand(permbox->access_combos[2], TRUE);
    gtk_grid_attach(GTK_GRID(permbox->grid), permbox->access_combos[2], 1, row, 1, 1);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->access_combos[2]);
    gtk_widget_show(permbox->access_combos[2]);

    row += 1;

    GtkWidget       *separator;
    separator = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set(separator, "margin", 4, NULL);
    gtk_grid_attach(GTK_GRID(permbox->grid), separator, 0, row, 2, 1);
    gtk_widget_show(separator);

    row += 1;

    label = gtk_label_new_with_mnemonic(_("Gro_up:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    permbox->group_combo = gtk_combo_box_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), permbox->group_combo);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(permbox->group_combo),
                               renderer_text, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(permbox->group_combo),
                                  renderer_text, "text", THUNAR_PERMISSIONS_STORE_COLUMN_NAME);
    gtk_combo_box_set_row_separator_func(GTK_COMBO_BOX(permbox->group_combo),
                                         _permbox_row_separator, NULL, NULL);
    g_object_bind_property(G_OBJECT(permbox), "mutable",
                           G_OBJECT(permbox->group_combo), "sensitive",
                           G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(permbox->group_combo), "changed",
                             G_CALLBACK(_permbox_group_changed), permbox);
    gtk_widget_set_hexpand(permbox->group_combo, TRUE);
    gtk_grid_attach(GTK_GRID(permbox->grid), permbox->group_combo, 1, row, 1, 1);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->group_combo);
    gtk_widget_show(permbox->group_combo);

    row += 1;

    label = gtk_label_new_with_mnemonic(_("Acc_ess:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    permbox->access_combos[1] = gtk_combo_box_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), permbox->access_combos[1]);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(permbox->access_combos[1]),
            renderer_text, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(permbox->access_combos[1]),
            renderer_text, "text", 0);
    g_object_bind_property(G_OBJECT(permbox), "mutable",
                           G_OBJECT(permbox->access_combos[1]), "sensitive",
            G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(permbox->access_combos[1]), "changed",
            G_CALLBACK(_permbox_access_changed), permbox);
    gtk_widget_set_hexpand(permbox->access_combos[1], TRUE);
    gtk_grid_attach(GTK_GRID(permbox->grid), permbox->access_combos[1], 1, row, 1, 1);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->access_combos[1]);
    gtk_widget_show(permbox->access_combos[1]);

    row += 1;

    separator = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set(separator, "margin", 4, NULL);
    gtk_grid_attach(GTK_GRID(permbox->grid), separator, 0, row, 2, 1);
    gtk_widget_show(separator);

    row += 1;

    label = gtk_label_new(_("Others"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    row += 1;

    label = gtk_label_new_with_mnemonic(_("Acce_ss:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    permbox->access_combos[0] = gtk_combo_box_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), permbox->access_combos[0]);
    gtk_cell_layout_pack_start(GTK_CELL_LAYOUT(permbox->access_combos[0]), renderer_text, TRUE);
    gtk_cell_layout_add_attribute(GTK_CELL_LAYOUT(permbox->access_combos[0]), renderer_text, "text", 0);
    g_object_bind_property(G_OBJECT(permbox), "mutable", G_OBJECT(permbox->access_combos[0]), "sensitive", G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(permbox->access_combos[0]), "changed",
            G_CALLBACK(_permbox_access_changed), permbox);
    gtk_widget_set_hexpand(permbox->access_combos[0], TRUE);
    gtk_grid_attach(GTK_GRID(permbox->grid), permbox->access_combos[0], 1, row, 1, 1);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->access_combos[0]);
    gtk_widget_show(permbox->access_combos[0]);

    row += 1;

    separator = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    g_object_set(separator, "margin", 4, NULL);
    gtk_grid_attach(GTK_GRID(permbox->grid), separator, 0, row, 2, 1);
    gtk_widget_show(separator);

    row += 1;

    label = gtk_label_new(_("Program:"));
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_grid_attach(GTK_GRID(permbox->grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    permbox->program_button = gtk_check_button_new_with_mnemonic(_("Allow this file to _run as a program"));
    g_object_bind_property(G_OBJECT(permbox->program_button), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    g_object_bind_property(G_OBJECT(permbox), "mutable", G_OBJECT(permbox->program_button), "sensitive", G_BINDING_SYNC_CREATE);
    g_signal_connect_swapped(G_OBJECT(permbox->program_button), "toggled",
                             G_CALLBACK(_permbox_program_toggled), permbox);
    gtk_grid_attach(GTK_GRID(permbox->grid), permbox->program_button, 1, row, 1, 1);
    etk_label_set_a11y_relation(GTK_LABEL(label), permbox->program_button);
    gtk_widget_show(permbox->program_button);

    row += 1;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_bind_property(G_OBJECT(permbox), "mutable", G_OBJECT(hbox), "sensitive", G_BINDING_SYNC_CREATE);
    g_object_bind_property(G_OBJECT(permbox->program_button), "visible", G_OBJECT(hbox), "visible", G_BINDING_SYNC_CREATE);
    gtk_grid_attach(GTK_GRID(permbox->grid), hbox, 1, row, 1, 1);
    gtk_widget_show(hbox);

    GtkWidget       *image;
    image = gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_widget_show(image);

    label = gtk_label_new(_("Allowing untrusted programs to run presents a security risk to your system."));
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_small_italic());
    gtk_label_set_line_wrap(GTK_LABEL(label), TRUE);
    gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_bind_property(G_OBJECT(permbox), "mutable", G_OBJECT(hbox), "sensitive", G_BINDING_SYNC_CREATE);
    gtk_grid_attach(GTK_GRID(permbox->grid), hbox, 1, row, 1, 1);
    gtk_widget_show(hbox);

    image = gtk_image_new_from_icon_name("dialog-warning", GTK_ICON_SIZE_LARGE_TOOLBAR);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
    gtk_widget_show(image);

    permbox->fixperm_label = gtk_label_new(_("The folder permissions are inconsistent, you may not be able to work with files in this folder."));
    gtk_label_set_xalign(GTK_LABEL(permbox->fixperm_label), 0.0f);
    gtk_label_set_attributes(GTK_LABEL(permbox->fixperm_label), e_pango_attr_list_small_italic());
    gtk_label_set_line_wrap(GTK_LABEL(permbox->fixperm_label), TRUE);
    g_object_bind_property(G_OBJECT(permbox->fixperm_label), "visible", G_OBJECT(hbox), "visible", G_BINDING_SYNC_CREATE);
    gtk_box_pack_start(GTK_BOX(hbox), permbox->fixperm_label, TRUE, TRUE, 0);
    gtk_widget_show(permbox->fixperm_label);

    row += 1;

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_grid_attach(GTK_GRID(permbox->grid), hbox, 1, row, 1, 1);
    gtk_widget_show(hbox);

    permbox->fixperm_button = gtk_button_new_with_mnemonic(_("Correct _folder permissions..."));
    gtk_widget_set_tooltip_text(permbox->fixperm_button, _("Click here to automatically fix the folder permissions."));
    g_signal_connect_swapped(G_OBJECT(permbox->fixperm_button), "clicked",
                             G_CALLBACK(_permbox_fixperm_clicked), permbox);
    g_object_bind_property(G_OBJECT(permbox->fixperm_button), "visible", G_OBJECT(hbox), "visible", G_BINDING_SYNC_CREATE);
    gtk_box_pack_end(GTK_BOX(hbox), permbox->fixperm_button, FALSE, FALSE, 0);
    gtk_widget_show(permbox->fixperm_button);

    // the job control stuff
    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(permbox), hbox, FALSE, FALSE, 0);

    permbox->job_progress = gtk_progress_bar_new();
    gtk_progress_bar_set_text(GTK_PROGRESS_BAR(permbox->job_progress), _("Please wait..."));
    g_object_bind_property(G_OBJECT(permbox->job_progress), "visible", G_OBJECT(hbox), "visible", G_BINDING_SYNC_CREATE);
    gtk_box_pack_start(GTK_BOX(hbox), permbox->job_progress, TRUE, TRUE, 0);

    GtkWidget *button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, _("Stop applying permissions recursively."));
    g_signal_connect_swapped(G_OBJECT(button), "clicked",
                             G_CALLBACK(_permbox_job_cancel), permbox);
    gtk_box_pack_start(GTK_BOX(hbox), button, FALSE, FALSE, 0);
    gtk_widget_show(button);
}

static void permbox_finalize(GObject *object)
{
    PermissionBox *permbox = PERMISSIONBOX(object);

    // cancel any pending job
    if (G_UNLIKELY(permbox->job != NULL))
    {
        // cancel the job(if not already done)
        exo_job_cancel(EXOJOB(permbox->job));

        // disconnect from the job
        g_signal_handlers_disconnect_matched(permbox->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, permbox);
        g_object_unref(permbox->job);
        permbox->job = NULL;
    }

    // drop the reference on the file(if any)
    _permbox_set_files(permbox, NULL);

    G_OBJECT_CLASS(permbox_parent_class)->finalize(object);
}

static void permbox_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    PermissionBox *permbox = PERMISSIONBOX(object);
    GList *lp;

    switch (prop_id)
    {
    case PROP_FILES:
        g_value_set_boxed(value, _permbox_get_files(permbox));
        break;

    case PROP_MUTABLE:
        for (lp = permbox->files; lp != NULL; lp = lp->next)
        {
            if (!th_file_is_chmodable(THUNAR_FILE(lp->data)))
                break;
        }
        g_value_set_boolean(value,(permbox->files != NULL) &&(lp == NULL));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void permbox_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    PermissionBox *permbox = PERMISSIONBOX(object);

    switch (prop_id)
    {
    case PROP_FILES:
        _permbox_set_files(permbox, g_value_get_boxed(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GList* _permbox_get_files(PermissionBox *permbox)
{
    e_return_val_if_fail(IS_PERMISSIONBOX(permbox), NULL);

    return permbox->files;
}

/**
 * permbox_set_files:
 * @permbox : a #PermissionBox.
 * @files   : a list of #ThunarFiles or %NULL.
 *
 * Associates @permbox with the specified @file.
 **/
static void _permbox_set_files(PermissionBox *permbox, GList *files)
{
    GList *lp;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));

    // check if we already use that file
    if (G_UNLIKELY(permbox->files == files))
        return;

    // disconnect from the previous files
    for(lp = permbox->files; lp != NULL; lp = lp->next)
    {
        e_assert(THUNAR_IS_FILE(lp->data));
        g_signal_handlers_disconnect_by_func(G_OBJECT(lp->data),
                                             _permbox_file_changed, permbox);
        g_object_unref(G_OBJECT(lp->data));
    }
    g_list_free(permbox->files);

    // activate the new file
    permbox->files = g_list_copy(files);

    // connect to the new files
    for(lp = permbox->files; lp != NULL; lp = lp->next)
    {
        // take a reference on the file
        e_assert(THUNAR_IS_FILE(lp->data));
        g_object_ref(G_OBJECT(lp->data));

        // stay informed about changes
        g_signal_connect_swapped(G_OBJECT(lp->data), "changed",
                                 G_CALLBACK(_permbox_file_changed), permbox);
    }

    // update our state
    if (permbox->files != NULL)
        _permbox_file_changed(permbox);

    // notify listeners
    g_object_notify(G_OBJECT(permbox), "files");
}

static gboolean _permbox_row_separator(GtkTreeModel *treemodel,
                                       GtkTreeIter *iter, gpointer data)
{
    (void) data;

    gchar *name;

    // determine the value of the "name" column
    gtk_tree_model_get(treemodel, iter, THUNAR_PERMISSIONS_STORE_COLUMN_NAME, &name, -1);

    if (G_LIKELY(name != NULL))
    {
        g_free(name);
        return FALSE;
    }

    return TRUE;
}

// Events ---------------------------------------------------------------------

static void _permbox_file_changed(PermissionBox *permbox)
{
    ThunarFile        *file;
    UserManager *user_manager;
    ThunarFileMode     mode = 0;
    ThunarGroup       *group = NULL;
    ThunarUser        *user = NULL;
    GtkListStore      *store;
    GtkTreeIter        iter;
    const gchar       *user_name;
    const gchar       *real_name;
    GList             *groups = NULL;
    GList             *lp;
    gchar              buffer[1024];
    guint              n;
    guint              n_files = 0;
    gint               modes[3] = { 0, };
    gint               file_modes[3];
    GtkListStore      *access_store;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));

    // compare multiple files
    for(lp = permbox->files; lp != NULL; lp = lp->next)
    {
        file = THUNAR_FILE(lp->data);

        // transform the file modes in r/w/r+w for each group
        mode = th_file_get_mode(file);
        for(n = 0; n < 3; n++)
            file_modes[n] =((mode >>(n * 3)) & 0007) >> 1;

        if (n_files == 0)
        {
            // get information of the first file
            user = th_file_get_user(file);
            group = th_file_get_group(file);

            for(n = 0; n < 3; n++)
                modes[n] = file_modes[n];
        }
        else
        {
            // unset the file info if it is different from the other files
            if (user != NULL && user != th_file_get_user(file))
                user = NULL;

            if (group != NULL && group != th_file_get_group(file))
                group = NULL;

            for(n = 0; n < 3; n++)
                if (file_modes[n] != modes[n])
                    modes[n] = 4;
        }

        n_files++;
    }

    file = THUNAR_FILE(permbox->files->data);

    // allocate a new store for the group combo box
    g_signal_handlers_block_by_func(G_OBJECT(permbox->group_combo), _permbox_group_changed, permbox);
    store = gtk_list_store_new(THUNAR_PERMISSIONS_STORE_N_COLUMNS, G_TYPE_STRING, G_TYPE_UINT);
    gtk_combo_box_set_model(GTK_COMBO_BOX(permbox->group_combo), GTK_TREE_MODEL(store));

    // determine the owner of the new file
    if (G_LIKELY(user != NULL))
    {
        // determine sane display name for the owner
        user_name = user_get_name(user);
        real_name = user_get_real_name(user);
        if (G_LIKELY(real_name != NULL))
            g_snprintf(buffer, sizeof(buffer), "%s(%s)", real_name, user_name);
        else
            g_strlcpy(buffer, user_name, sizeof(buffer));
        gtk_label_set_text(GTK_LABEL(permbox->user_label), buffer);
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(permbox->user_label),
                            n_files > 1 ? _("Mixed file owners") :_("Unknown file owner"));
    }

    // check if we have superuser privileges
    if (G_UNLIKELY(geteuid() == 0))
    {
        // determine all groups in the system
        user_manager = usermanager_get_default();
        groups = usermanager_get_all_groups(user_manager);
        g_object_unref(G_OBJECT(user_manager));
    }
    else
    {
        if (G_UNLIKELY(user == NULL && n_files > 1))
        {
            // get groups of the active user
            user_manager = usermanager_get_default();
            user = usermanager_get_user_by_id(user_manager, geteuid());
            g_object_unref(G_OBJECT(user_manager));
        }

        // determine the groups for the user and take a copy
        if (G_LIKELY(user != NULL))
        {
            groups = g_list_copy(user_get_groups(user));
            g_list_foreach(groups,(GFunc)(void(*)(void)) g_object_ref, NULL);
        }
    }

    // make sure that the group list includes the file group
    if (G_UNLIKELY(group != NULL && g_list_find(groups, group) == NULL))
        groups = g_list_prepend(groups, g_object_ref(G_OBJECT(group)));

    // sort the groups according to group_compare()
    groups = g_list_sort_with_data(groups, _group_compare, group);

    // add the groups to the store
    for (lp = groups, n = 0; lp != NULL; lp = lp->next)
    {
        // append a separator after the primary group and after the user-groups(not system groups)
        if (group != NULL
                && group_get_id(groups->data) == group_get_id(group)
                && lp != groups && n == 0)
        {
            gtk_list_store_append(store, &iter);
            n += 1;
        }
        else if (lp != groups && group_get_id(lp->data) < 100 && n == 1)
        {
            gtk_list_store_append(store, &iter);
            n += 1;
        }

        // append a new item for the group
        gtk_list_store_append(store, &iter);
        gtk_list_store_set(store, &iter,
                            THUNAR_PERMISSIONS_STORE_COLUMN_NAME, group_get_name(lp->data),
                            THUNAR_PERMISSIONS_STORE_COLUMN_GID, group_get_id(lp->data),
                            -1);

        // set the active iter for the combo box if this group is the primary group
        if (G_UNLIKELY(lp->data == group))
            gtk_combo_box_set_active_iter(GTK_COMBO_BOX(permbox->group_combo), &iter);
    }

    // cleanup
    if (G_LIKELY(user != NULL))
        g_object_unref(G_OBJECT(user));
    if (G_LIKELY(group != NULL))
        g_object_unref(G_OBJECT(group));

    g_list_free_full(groups, g_object_unref);

    // determine the file mode and update the combo boxes
    for(n = 0; n < G_N_ELEMENTS(permbox->access_combos); ++n)
    {
        g_signal_handlers_block_by_func(G_OBJECT(permbox->access_combos[n]), _permbox_access_changed, permbox);

        // allocate the store for the permission combos
        access_store = gtk_list_store_new(1, G_TYPE_STRING);
        gtk_list_store_insert_with_values(access_store, NULL, 0, 0, _("None"), -1);         // 0000
        gtk_list_store_insert_with_values(access_store, NULL, 1, 0, _("Write only"), -1);   // 0002
        gtk_list_store_insert_with_values(access_store, NULL, 2, 0, _("Read only"), -1);    // 0004
        gtk_list_store_insert_with_values(access_store, NULL, 3, 0, _("Read & Write"), -1); // 0006
        if (modes[n] == 4)
            gtk_list_store_insert_with_values(access_store, NULL, 4, 0, _("Varying(no change)"), -1);

        gtk_combo_box_set_model(GTK_COMBO_BOX(permbox->access_combos[n]), GTK_TREE_MODEL(access_store));
        gtk_combo_box_set_active(GTK_COMBO_BOX(permbox->access_combos[n]), modes[n]);

        g_signal_handlers_unblock_by_func(G_OBJECT(permbox->access_combos[n]), _permbox_access_changed, permbox);

        g_object_unref(G_OBJECT(access_store));
    }

    // update the program setting based on the mode(only visible for regular files)
    g_signal_handlers_block_by_func(G_OBJECT(permbox->program_button), _permbox_program_toggled, permbox);
    g_object_set(G_OBJECT(permbox->program_button), "visible", th_file_is_regular(file), NULL);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(permbox->program_button),(mode & 0111) != 0);
    g_signal_handlers_unblock_by_func(G_OBJECT(permbox->program_button), _permbox_program_toggled, permbox);

    // update the "inconsistent folder permissions" warning and the "fix permissions" button based on the mode
    if (permbox_has_fixable_directory(permbox))
    {
        // always display the warning even if we cannot fix it
        gtk_widget_show(permbox->fixperm_label);
        gtk_widget_show(permbox->fixperm_button);
    }
    else
    {
        // hide both the warning text and the fix button
        gtk_widget_hide(permbox->fixperm_button);
        gtk_widget_hide(permbox->fixperm_label);
    }

    // release our reference on the new combo store and unblock the combo
    g_signal_handlers_unblock_by_func(G_OBJECT(permbox->group_combo), _permbox_group_changed, permbox);
    g_object_unref(G_OBJECT(store));

    // emit notification on "mutable", so all widgets update their sensitivity
    g_object_notify(G_OBJECT(permbox), "mutable");
}

// group sort function
static gint _group_compare(gconstpointer group_a, gconstpointer group_b,
                           gpointer group_primary)
{
    guint32 group_primary_id;
    guint32 group_a_id = group_get_id(THUNAR_GROUP(group_a));
    guint32 group_b_id = group_get_id(THUNAR_GROUP(group_b));

    // check if the groups are equal
    if (group_a_id == group_b_id)
        return 0;

    // the primary group is always sorted first
    if (group_primary != NULL)
    {
        group_primary_id = group_get_id(THUNAR_GROUP(group_primary));
        if (group_a_id == group_primary_id)
            return -1;
        else if (group_b_id == group_primary_id)
            return 1;
    }

    // system groups(< 100) are always sorted last
    if (group_a_id < 100 && group_b_id >= 100)
        return 1;
    else if (group_b_id < 100 && group_a_id >= 100)
        return -1;

    // otherwise just sort by name
    return g_ascii_strcasecmp(group_get_name(THUNAR_GROUP(group_a)),
                               group_get_name(THUNAR_GROUP(group_b)));
}

static gboolean permbox_has_fixable_directory(PermissionBox *permbox)
{
    for (GList *lp = permbox->files; lp != NULL; lp = lp->next)
    {
        if (permbox_is_fixable_directory(THUNAR_FILE(lp->data)))
            return TRUE;
    }

    return FALSE;
}

static gboolean permbox_is_fixable_directory(ThunarFile *file)
{
    ThunarFileMode mode;

    e_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (!th_file_is_directory(file)
            || !th_file_is_chmodable(file))
        return FALSE;

    mode = th_file_get_mode(file);

    return((mode & 0111) !=((mode >> 2) & 0111));
}

static void _permbox_access_changed(PermissionBox *permbox, GtkWidget *combo)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(GTK_IS_COMBO_BOX(combo));

    ThunarFileMode  file_mask;
    ThunarFileMode  file_mode;
    ThunarFileMode  dir_mask;
    ThunarFileMode  dir_mode;
    guint           n;
    gint            active_mode;
    GtkTreeModel   *model;
    GtkTreeIter     iter;

    // leave if the active mode is varying
    active_mode = gtk_combo_box_get_active(GTK_COMBO_BOX(combo));
    if (active_mode > 3)
        return;

    // determine the new mode from the combo box
    for(n = 0; n < G_N_ELEMENTS(permbox->access_combos) && permbox->access_combos[n] != combo ; ++n);
    dir_mode = file_mode =(active_mode << 1) <<(n * 3);
    dir_mask = file_mask = 0006 <<(n * 3);

    // keep exec bit in sync for folders
    if (permbox_has_directory(permbox))
    {
        // if either read or write is, set exec as well, else unset exec as well
        if ((dir_mode &(0004 <<(n * 3))) != 0)
            dir_mode |=(0001 <<(n * 3));
        dir_mask = 0007 <<(n * 3);
    }

    // change the permissions
    if (_permbox_change_mode(permbox, dir_mask, dir_mode, file_mask, file_mode))
    {
        // for better feedback remove the varying item
        model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));
        if (gtk_tree_model_get_iter_from_string(model, &iter, "4"))
            gtk_list_store_remove(GTK_LIST_STORE(model), &iter);
    }
}

static gboolean permbox_has_directory(PermissionBox *permbox)
{
    GList *lp;

    for(lp = permbox->files; lp != NULL; lp = lp->next)
        if (th_file_is_directory(THUNAR_FILE(lp->data)))
            return TRUE;

    return FALSE;
}

// permbox_init
static void _permbox_group_changed(PermissionBox *permbox, GtkWidget *combo)
{
    GtkTreeModel *model;
    GtkTreeIter   iter;
    guint32       gid;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(permbox->group_combo == combo);
    e_return_if_fail(GTK_IS_COMBO_BOX(combo));

    // verify that we have a valid file
    if (G_UNLIKELY(permbox->files == NULL))
        return;

    // determine the tree model from the combo box
    model = gtk_combo_box_get_model(GTK_COMBO_BOX(combo));

    // determine the iterator for the selected item
    if (gtk_combo_box_get_active_iter(GTK_COMBO_BOX(combo), &iter))
    {
        // determine the group id for the selected item...
        gtk_tree_model_get(model, &iter, THUNAR_PERMISSIONS_STORE_COLUMN_GID, &gid, -1);

        // ...and try to change the group to the new gid
        _permbox_change_group(permbox, gid);
    }
}

static void _permbox_program_toggled(PermissionBox *permbox, GtkWidget *button)
{
    ThunarFileMode mode;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(permbox->program_button == button);
    e_return_if_fail(GTK_IS_TOGGLE_BUTTON(button));

    // verify that we have a valid file
    if (G_UNLIKELY(permbox->files == NULL))
        return;

    // determine the new mode based on the toggle state
    mode =(gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button))) ? 0111 : 0000;

    // apply the new mode(only the executable bits for files)
    _permbox_change_mode(permbox, 0000, 0000, 0111, mode);
}

static void _permbox_fixperm_clicked(PermissionBox *permbox, GtkWidget *button)
{
    ThunarFileMode mode;
    GtkWidget     *dialog;
    GtkWidget     *window;
    ThunarJob     *job;
    gint           response;
    GList         *lp;
    GList          file_list;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(permbox->fixperm_button == button);
    e_return_if_fail(GTK_IS_BUTTON(button));
    e_return_if_fail(permbox_has_fixable_directory(permbox));

    // verify that we have a valid file
    if (G_UNLIKELY(permbox->files == NULL))
        return;

    // determine the toplevel widget
    window = gtk_widget_get_toplevel(GTK_WIDGET(permbox));
    if (G_UNLIKELY(window == NULL))
        return;

    // popup a confirm dialog
    dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                     GTK_DIALOG_DESTROY_WITH_PARENT
                                     | GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_QUESTION,
                                     GTK_BUTTONS_NONE,
                                     _("Correct folder permissions automatically?"));
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("Correct _folder permissions"), GTK_RESPONSE_OK);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), _("The folder permissions will be reset to a consistent state. Only users "
            "allowed to read the contents of this folder will be allowed to enter the "
            "folder afterwards."));
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    // check if we should apply the changes
    if (response == GTK_RESPONSE_OK)
    {
        for(lp = permbox->files; lp != NULL; lp = lp->next)
        {
            // skip files that are fine
            if (!permbox_is_fixable_directory(THUNAR_FILE(lp->data)))
                continue;

            // determine the current mode
            mode = th_file_get_mode(THUNAR_FILE(lp->data));

            // determine the new mode(making sure the owner can read/enter the folder)
            mode =(THUNAR_FILE_MODE_USR_READ | THUNAR_FILE_MODE_USR_EXEC)
                   |(((mode & THUNAR_FILE_MODE_GRP_READ) != 0) ? THUNAR_FILE_MODE_GRP_EXEC : 0)
                   |(((mode & THUNAR_FILE_MODE_OTH_READ) != 0) ? THUNAR_FILE_MODE_OTH_EXEC : 0);

            file_list.prev = NULL;
            file_list.data = th_file_get_file(THUNAR_FILE(lp->data));
            file_list.next = NULL;

            // try to allocate the new job
            job = io_change_mode(&file_list,
                                              0511, mode, 0000, 0000, FALSE);

            // handle the job
            _permbox_job_start(permbox, job, FALSE);
            g_object_unref(job);
        }
    }
}

static void _permbox_job_cancel(PermissionBox *permbox)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));

    // verify that we have a job to cancel
    if (G_UNLIKELY(permbox->job == NULL))
        return;

    // cancel the job(if not already done)
    exo_job_cancel(EXOJOB(permbox->job));

    // disconnect from the job
    g_signal_handlers_disconnect_matched(permbox->job, G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, permbox);
    g_object_unref(G_OBJECT(permbox->job));
    permbox->job = NULL;

    // hide the progress bar
    gtk_widget_hide(permbox->job_progress);

    // make the remaining widgets sensitive again
    gtk_widget_set_sensitive(permbox->grid, TRUE);
}

// Change ---------------------------------------------------------------------

static void _permbox_change_group(PermissionBox *permbox, guint32 gid)
{
    ThunarJob *job;
    gboolean   recursive = FALSE;
    gint       response;
    GList     *file_list;

    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(permbox->files != NULL);

    // check if we should operate recursively
    if (permbox_has_directory(permbox))
    {
        response = _permbox_ask_recursive(permbox);
        switch(response)
        {
        case GTK_RESPONSE_YES:
            recursive = TRUE;
            break;

        case GTK_RESPONSE_NO:
            recursive = FALSE;
            break;

        default:  // cancelled by the user
            _permbox_file_changed(permbox);
            return;
        }
    }

    // try to allocate the new job
    file_list = permbox_get_file_list(permbox);

    job = io_change_group(file_list, gid, recursive);
    _permbox_job_start(permbox, job, recursive);

    g_list_free_full(file_list, g_object_unref);
    g_object_unref(job);
}

static gboolean _permbox_change_mode(PermissionBox *permbox,
                                     ThunarFileMode dir_mask,
                                     ThunarFileMode dir_mode,
                                     ThunarFileMode file_mask,
                                     ThunarFileMode file_mode)
{
    e_return_val_if_fail(IS_PERMISSIONBOX(permbox), FALSE);
    e_return_val_if_fail(permbox->files != NULL, FALSE);

    ThunarJob *job;
    gboolean   recursive = FALSE;
    gint       response;
    GList     *file_list;

    // check if we should operate recursively
    if (permbox_has_directory(permbox))
    {
        response = _permbox_ask_recursive(permbox);
        switch(response)
        {
        case GTK_RESPONSE_YES:
            recursive = TRUE;
            break;

        case GTK_RESPONSE_NO:
            recursive = FALSE;
            break;

        default:  // cancelled by the user
            _permbox_file_changed(permbox);
            return FALSE;
        }
    }

    // try to allocate the new job
    file_list = permbox_get_file_list(permbox);

    job = io_change_mode(file_list, dir_mask, dir_mode, file_mask, file_mode, recursive);
    _permbox_job_start(permbox, job, recursive);

    g_list_free_full(file_list, g_object_unref);
    g_object_unref(job);

    return TRUE;
}

static GList* permbox_get_file_list(PermissionBox *permbox)
{
    // free returned list with g_list_free_full(file_list, g_object_unref);

    GList *file_list = NULL;
    GList *lp;
    GFile *gfile;

    for (lp = permbox->files; lp != NULL; lp = lp->next)
    {
        gfile = th_file_get_file(THUNAR_FILE(lp->data));
        e_assert(G_IS_FILE(gfile));
        file_list = g_list_prepend(file_list, g_object_ref(G_OBJECT(gfile)));
    }

    return file_list;
}

// Ask recusive dialog
static gboolean _permbox_ask_recursive(PermissionBox *permbox)
{
    ThunarRecursivePermissionsMode mode = THUNAR_RECURSIVE_PERMISSIONS_ALWAYS;

    // check if we should ask the user first
    if (G_UNLIKELY(mode == THUNAR_RECURSIVE_PERMISSIONS_ASK))
    {
        // determine the toplevel widget for the permbox
        GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(permbox));

        // allocate the question dialog
        GtkWidget *dialog;
        dialog = gtk_dialog_new_with_buttons(_("Question"), GTK_WINDOW(toplevel),
                                             GTK_DIALOG_DESTROY_WITH_PARENT
                                             | GTK_DIALOG_MODAL,
                                             _("_Cancel"), GTK_RESPONSE_CANCEL,
                                             _("_No"), GTK_RESPONSE_NO,
                                             _("_Yes"), GTK_RESPONSE_YES,
                                             NULL);
        gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);

        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
        gtk_container_set_border_width(GTK_CONTAINER(hbox), 8);
        gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), hbox, TRUE, TRUE, 0);
        gtk_widget_show(hbox);

        GtkWidget *image;
        image = gtk_image_new_from_icon_name("dialog-question", GTK_ICON_SIZE_DIALOG);
        gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
        gtk_widget_set_valign(image, GTK_ALIGN_START);
        gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
        gtk_widget_show(image);

        GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
        gtk_box_pack_start(GTK_BOX(hbox), vbox, TRUE, TRUE, 0);
        gtk_widget_show(vbox);

        GtkWidget *label = gtk_label_new(_("Apply recursively?"));
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_big_bold());
        gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
        gtk_widget_show(label);

        label = gtk_label_new(_("Do you want to apply your changes recursively to\nall files and subfolders below the selected folder?"));
        gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
        gtk_box_pack_start(GTK_BOX(vbox), label, TRUE, TRUE, 0);
        gtk_widget_show(label);

        GtkWidget *button;
        button = gtk_check_button_new_with_mnemonic(_("Do _not ask me again"));
        gtk_widget_set_tooltip_text(button, _("If you select this option your choice will be remembered and you won't be asked "
                                               "again. You can use the preferences dialog to alter your choice afterwards."));
        gtk_box_pack_start(GTK_BOX(vbox), button, FALSE, FALSE, 0);
        gtk_widget_show(button);

        // run the dialog and save the selected option (if requested)
        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        //switch (response)
        //{
        //case GTK_RESPONSE_YES:
        //    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        //        g_object_set(G_OBJECT(preferences), "misc-recursive-permissions", THUNAR_RECURSIVE_PERMISSIONS_ALWAYS, NULL);
        //    break;

        //case GTK_RESPONSE_NO:
        //    if (gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(button)))
        //        g_object_set(G_OBJECT(preferences), "misc-recursive-permissions", THUNAR_RECURSIVE_PERMISSIONS_NEVER, NULL);
        //    break;

        //default:
        //    break;
        //}

        // destroy the dialog resources
        gtk_widget_destroy(dialog);

        return response;
    }
    else if (mode == THUNAR_RECURSIVE_PERMISSIONS_ALWAYS)
    {
        return GTK_RESPONSE_YES;
    }

    return GTK_RESPONSE_NO;
}

// Job ------------------------------------------------------------------------

static void _permbox_job_start(PermissionBox *permbox, ThunarJob *job,
                               gboolean recursive)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(permbox->job == NULL);

    // take a reference to the job and connect signals
    permbox->job = g_object_ref(job);

    g_signal_connect_swapped(job, "ask",
                             G_CALLBACK(_permbox_job_ask), permbox);

    g_signal_connect_swapped(job, "error",
                             G_CALLBACK(_permbox_job_error), permbox);

    g_signal_connect_swapped(job, "finished",
                             G_CALLBACK(_permbox_job_finished), permbox);

    // don't connect percent for single file operations
    if (G_UNLIKELY(recursive))
    {
        g_signal_connect_swapped(job, "percent",
                                 G_CALLBACK(_permbox_job_percent), permbox);
    }

    // setup the progress bar
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(permbox->job_progress), 0.0);

    // make the majority of widgets insensitive if doing recursively
    gtk_widget_set_sensitive(permbox->grid, !recursive);
}

static ThunarJobResponse _permbox_job_ask(PermissionBox *permbox,
                                          const gchar *message,
                                          ThunarJobResponse choices,
                                          ThunarJob *job)
{
    e_return_val_if_fail(IS_PERMISSIONBOX(permbox), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(g_utf8_validate(message, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(THUNAR_IS_JOB(job), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(permbox->job == job, THUNAR_JOB_RESPONSE_CANCEL);

    // be sure to display the progress bar prior to opening the question dialog
    gtk_widget_show_now(permbox->job_progress);

    // determine the toplevel window for the permbox
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(permbox));

    if (G_UNLIKELY(toplevel == NULL))
        return THUNAR_JOB_RESPONSE_CANCEL;

    // display the question dialog
    return dialog_job_ask(GTK_WINDOW(toplevel), message, choices);
}

static void _permbox_job_error(PermissionBox *permbox, GError *error, ThunarJob *job)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(error != NULL && error->message != NULL);
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(permbox->job == job);

    // be sure to display the progress bar prior to opening the error dialog
    gtk_widget_show_now(permbox->job_progress);

    // determine the toplevel widget for the permbox
    GtkWidget *toplevel = gtk_widget_get_toplevel(GTK_WIDGET(permbox));

    if (G_UNLIKELY(toplevel == NULL))
        return;

    // popup the error message dialog
    dialog_job_error(GTK_WINDOW(toplevel), error);
}

static void _permbox_job_finished(PermissionBox *permbox, ThunarJob *job)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(permbox->job == job);

    // we can just use job_cancel(), since the job is already done
    _permbox_job_cancel(permbox);
}

static void _permbox_job_percent(PermissionBox *permbox, gdouble percent, ThunarJob *job)
{
    e_return_if_fail(IS_PERMISSIONBOX(permbox));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(permbox->job == job);

    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(permbox->job_progress),
                                  percent / 100.0);

    gtk_widget_show(permbox->job_progress);
}

// Public ---------------------------------------------------------------------

GtkWidget* permbox_new()
{
    return g_object_new(TYPE_PERMISSIONBOX, NULL);
}


