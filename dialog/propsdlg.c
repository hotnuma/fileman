/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 * Copyright(c) 2012      Nick Schermer <nick@xfce.org>
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
#include <propsdlg.h>
#include <marshal.h>

#include <appcombo.h>
#include <iconfactory.h>
#include <th_image.h>
#include <sizelabel.h>
#include <permbox.h>

#include <dialogs.h>
#include <io_jobs.h>
#include <pango_ext.h>
#include <gio_ext.h>
#include <utils.h>
#include <libxfce4ui/libxfce4ui.h>

// PropertiesDialog -----------------------------------------------------------

enum
{
    PROP_0,
    PROP_FILES,
    PROP_FILE_SIZE_BINARY,
};

enum
{
    RELOAD,
    LAST_SIGNAL,
};

static void propsdlg_dispose(GObject *object);
static void propsdlg_finalize(GObject *object);
static void propsdlg_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void propsdlg_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);
static GList* _propsdlg_get_files(PropertiesDialog *dialog);
static void propsdlg_response(GtkDialog *dialog, gint response);
static gboolean propsdlg_reload(PropertiesDialog *dialog);

// Events ---------------------------------------------------------------------

static void _propsdlg_name_activate(GtkWidget *entry, PropertiesDialog *dialog);
static void _propsdlg_rename_error(ExoJob *job, GError *error,
                                   PropertiesDialog *dialog);
static void _propsdlg_rename_finished(ExoJob *job, PropertiesDialog *dialog);
static gboolean _propsdlg_name_focus_out_event(GtkWidget *entry,
                                               GdkEventFocus *event,
                                               PropertiesDialog *dialog);
static void _propsdlg_update(PropertiesDialog *dialog);
static void _propsdlg_update_single(PropertiesDialog *dialog);
static void _propsdlg_update_multiple(PropertiesDialog *dialog);

struct _PropertiesDialogClass
{
    GtkDialogClass __parent__;

    gboolean (*reload) (PropertiesDialog *dialog);
};

struct _PropertiesDialog
{
    GtkDialog   __parent__;

    GList       *files;
    gboolean    file_size_binary;

    XfceFilenameInput *name_entry;

    GtkWidget   *notebook;
    GtkWidget   *icon_button;
    GtkWidget   *icon_image;
    GtkWidget   *names_label;
    GtkWidget   *single_box;
    GtkWidget   *kind_ebox;
    GtkWidget   *kind_label;
    GtkWidget   *openwith_chooser;
    GtkWidget   *link_label;
    GtkWidget   *location_label;
    GtkWidget   *origin_label;
    GtkWidget   *deleted_label;
    GtkWidget   *modified_label;
    GtkWidget   *accessed_label;
    GtkWidget   *freespace_vbox;
    GtkWidget   *freespace_bar;
    GtkWidget   *freespace_label;
    GtkWidget   *volume_image;
    GtkWidget   *volume_label;
    GtkWidget   *permissions_chooser;
};

G_DEFINE_TYPE(PropertiesDialog, propsdlg, GTK_TYPE_DIALOG)

static void propsdlg_class_init(PropertiesDialogClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = propsdlg_dispose;
    gobject_class->finalize = propsdlg_finalize;
    gobject_class->get_property = propsdlg_get_property;
    gobject_class->set_property = propsdlg_set_property;

    GtkDialogClass *gtkdialog_class = GTK_DIALOG_CLASS(klass);
    gtkdialog_class->response = propsdlg_response;

    klass->reload = propsdlg_reload;

    g_object_class_install_property(gobject_class,
                                    PROP_FILES,
                                    g_param_spec_boxed(
                                        "files",
                                        "files",
                                        "files",
                                        TYPE_FILEINFO_LIST,
                                        E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_FILE_SIZE_BINARY,
                                    g_param_spec_boolean(
                                        "file-size-binary",
                                        "FileSizeBinary",
                                        NULL,
                                        TRUE,
                                        E_PARAM_READWRITE));

    g_signal_new(I_("reload"),
                 G_TYPE_FROM_CLASS(klass),
                 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                 G_STRUCT_OFFSET(PropertiesDialogClass, reload),
                 g_signal_accumulator_true_handled, NULL,
                 _thunar_marshal_BOOLEAN__VOID,
                 G_TYPE_BOOLEAN, 0);

    // setup the key bindings for the properties dialog
    GtkBindingSet *binding_set = gtk_binding_set_by_class(klass);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_F5, 0, "reload", 0);
    gtk_binding_entry_add_signal(binding_set, GDK_KEY_r, GDK_CONTROL_MASK, "reload", 0);
}

static void propsdlg_init(PropertiesDialog *dialog)
{
    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                           _("_Help"), GTK_RESPONSE_HELP,
                           _("_Close"), GTK_RESPONSE_CLOSE,
                           NULL);

    gtk_window_set_default_size(GTK_WINDOW(dialog), 400, 450);

    dialog->notebook = gtk_notebook_new();
    gtk_container_set_border_width(GTK_CONTAINER(dialog->notebook), 6);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), dialog->notebook, TRUE, TRUE, 0);
    gtk_widget_show(dialog->notebook);

    GtkWidget *grid = gtk_grid_new();
    gtk_widget_show(grid);

    gtk_grid_set_column_spacing(GTK_GRID(grid), 12);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 12);

    GtkWidget *label = gtk_label_new(_("General"));
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), grid, label);
    gtk_widget_show(label);

    guint row = 0;

    // First box(icon, name) for 1 file

    dialog->single_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_grid_attach(GTK_GRID(grid), dialog->single_box, 0, row, 1, 1);

    dialog->icon_button = gtk_button_new();
    //g_signal_connect(G_OBJECT(dialog->icon_button), "clicked",
    //G_CALLBACK(thunar_properties_dialog_icon_button_clicked), dialog);
    gtk_box_pack_start(GTK_BOX(dialog->single_box), dialog->icon_button, FALSE, TRUE, 0);
    gtk_widget_show(dialog->icon_button);

    dialog->icon_image = th_image_new();
    gtk_box_pack_start(GTK_BOX(dialog->single_box), dialog->icon_image, FALSE, TRUE, 0);
    gtk_widget_show(dialog->icon_image);

    label = gtk_label_new_with_mnemonic(_("_Name:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_box_pack_end(GTK_BOX(dialog->single_box), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    // set up the widget for entering the filename
    dialog->name_entry = g_object_new(XFCE_TYPE_FILENAME_INPUT, NULL);
    gtk_widget_set_hexpand(GTK_WIDGET(dialog->name_entry), TRUE);
    gtk_widget_set_valign(GTK_WIDGET(dialog->name_entry), GTK_ALIGN_CENTER);
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), GTK_WIDGET(xfce_filename_input_get_entry(dialog->name_entry)));
    gtk_widget_show_all(GTK_WIDGET(dialog->name_entry));

    g_signal_connect(G_OBJECT(xfce_filename_input_get_entry(dialog->name_entry)),
                     "activate",
                     G_CALLBACK(_propsdlg_name_activate), dialog);

    g_signal_connect(G_OBJECT(xfce_filename_input_get_entry(dialog->name_entry)),
                     "focus-out-event",
                     G_CALLBACK(_propsdlg_name_focus_out_event), dialog);

    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(dialog->name_entry), 1, row, 1, 1);
    g_object_bind_property(G_OBJECT(dialog->single_box), "visible", G_OBJECT(dialog->name_entry), "visible", G_BINDING_SYNC_CREATE);

    ++row;

    /*
       First box(icon, name) for multiple files
     */
    GtkWidget *box;
    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_grid_attach(GTK_GRID(grid), box, 0, row, 1, 1);

    g_object_bind_property(G_OBJECT(dialog->single_box),
                           "visible",
                           G_OBJECT(box),
                           "visible",
                           G_BINDING_INVERT_BOOLEAN | G_BINDING_SYNC_CREATE);

    GtkWidget *image;
    image = gtk_image_new_from_icon_name("text-x-generic", GTK_ICON_SIZE_DIALOG);
    gtk_box_pack_start(GTK_BOX(box), image, FALSE, TRUE, 0);
    gtk_widget_show(image);

    label = gtk_label_new(_("Names:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_box_pack_end(GTK_BOX(box), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    dialog->names_label = gtk_label_new("");
    gtk_label_set_xalign(GTK_LABEL(dialog->names_label), 0.0f);
    gtk_widget_set_hexpand(dialog->names_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->names_label, 1, row, 1, 1);
    gtk_label_set_ellipsize(GTK_LABEL(dialog->names_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_selectable(GTK_LABEL(dialog->names_label), TRUE);
    g_object_bind_property(G_OBJECT(box), "visible", G_OBJECT(dialog->names_label), "visible", G_BINDING_SYNC_CREATE);

    ++row;

    /*
       Second box(kind, open with, link target)
     */
    label = gtk_label_new(_("Kind:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->kind_ebox = gtk_event_box_new();
    gtk_event_box_set_above_child(GTK_EVENT_BOX(dialog->kind_ebox), TRUE);
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(dialog->kind_ebox), FALSE);
    g_object_bind_property(G_OBJECT(dialog->kind_ebox), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->kind_ebox, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->kind_ebox, 1, row, 1, 1);
    gtk_widget_show(dialog->kind_ebox);

    dialog->kind_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->kind_label), TRUE);
    gtk_label_set_ellipsize(GTK_LABEL(dialog->kind_label), PANGO_ELLIPSIZE_END);
    gtk_container_add(GTK_CONTAINER(dialog->kind_ebox), dialog->kind_label);
    gtk_widget_show(dialog->kind_label);

    ++row;

    label = gtk_label_new_with_mnemonic(_("_Open With:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->openwith_chooser = appcombo_new();
    gtk_label_set_mnemonic_widget(GTK_LABEL(label), dialog->openwith_chooser);
    g_object_bind_property(G_OBJECT(dialog->openwith_chooser), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->openwith_chooser, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->openwith_chooser, 1, row, 1, 1);
    gtk_widget_show(dialog->openwith_chooser);

    ++row;

    label = gtk_label_new(_("Link Target:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->link_label = g_object_new(GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->link_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->link_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->link_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->link_label, 1, row, 1, 1);
    gtk_widget_show(dialog->link_label);

    ++row;

    /* TRANSLATORS: Try to come up with a short translation of "Original Path"(which is the path
     * where the trashed file/folder was located before it was moved to the trash), otherwise the
     * properties dialog width will be messed up.
     */
    label = gtk_label_new(_("Original Path:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->origin_label = g_object_new(GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->origin_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->origin_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->origin_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->origin_label, 1, row, 1, 1);
    gtk_widget_show(dialog->origin_label);

    ++row;

    label = gtk_label_new(_("Location:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->location_label = g_object_new(GTK_TYPE_LABEL, "ellipsize", PANGO_ELLIPSIZE_START, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->location_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->location_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->location_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->location_label, 1, row, 1, 1);
    gtk_widget_show(dialog->location_label);

    ++row;

    GtkWidget *spacer;
    spacer = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
    gtk_grid_attach(GTK_GRID(grid), spacer, 0, row, 2, 1);
    gtk_widget_show(spacer);

    ++row;

    /*
       Third box(deleted, modified, accessed)
     */
    label = gtk_label_new(_("Deleted:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->deleted_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->deleted_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->deleted_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->deleted_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->deleted_label, 1, row, 1, 1);
    gtk_widget_show(dialog->deleted_label);

    ++row;

    label = gtk_label_new(_("Modified:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->modified_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->modified_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->modified_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->modified_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->modified_label, 1, row, 1, 1);
    gtk_widget_show(dialog->modified_label);

    ++row;

    label = gtk_label_new(_("Accessed:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->accessed_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->accessed_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->accessed_label), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(dialog->accessed_label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->accessed_label, 1, row, 1, 1);
    gtk_widget_show(dialog->accessed_label);

    ++row;

    spacer = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
    gtk_grid_attach(GTK_GRID(grid), spacer, 0, row, 2, 1);
    g_object_bind_property(G_OBJECT(dialog->accessed_label), "visible", G_OBJECT(spacer), "visible", G_BINDING_SYNC_CREATE);

    ++row;

    /*
       Fourth box(size, volume, free space)
     */
    label = gtk_label_new(_("Size:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    label = szlabel_new();
    g_object_bind_property(G_OBJECT(dialog), "files", G_OBJECT(label), "files", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 1, row, 1, 1);
    gtk_widget_show(label);

    ++row;

    label = gtk_label_new(_("Volume:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    g_object_bind_property(G_OBJECT(box), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_set_hexpand(box, TRUE);
    gtk_grid_attach(GTK_GRID(grid), box, 1, row, 1, 1);
    gtk_widget_show(box);

    dialog->volume_image = gtk_image_new();
    g_object_bind_property(G_OBJECT(dialog->volume_image), "visible", G_OBJECT(box), "visible", G_BINDING_SYNC_CREATE);
    gtk_box_pack_start(GTK_BOX(box), dialog->volume_image, FALSE, TRUE, 0);
    gtk_widget_show(dialog->volume_image);

    dialog->volume_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->volume_label), TRUE);
    g_object_bind_property(G_OBJECT(dialog->volume_label), "visible", G_OBJECT(dialog->volume_image), "visible", G_BINDING_SYNC_CREATE);
    gtk_box_pack_start(GTK_BOX(box), dialog->volume_label, TRUE, TRUE, 0);
    gtk_widget_show(dialog->volume_label);

    ++row;

    label = gtk_label_new(_("Usage:"));
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_bold());
    gtk_label_set_xalign(GTK_LABEL(label), 1.0f);
    gtk_label_set_yalign(GTK_LABEL(label), 0.0f);
    gtk_grid_attach(GTK_GRID(grid), label, 0, row, 1, 1);
    gtk_widget_show(label);

    dialog->freespace_vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_hexpand(dialog->freespace_vbox, TRUE);
    gtk_grid_attach(GTK_GRID(grid), dialog->freespace_vbox, 1, row, 1, 1);
    g_object_bind_property(G_OBJECT(dialog->freespace_vbox), "visible", G_OBJECT(label), "visible", G_BINDING_SYNC_CREATE);
    gtk_widget_show(dialog->freespace_vbox);

    dialog->freespace_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_selectable(GTK_LABEL(dialog->freespace_label), TRUE);
    gtk_box_pack_start(GTK_BOX(dialog->freespace_vbox), dialog->freespace_label, TRUE, TRUE, 0);
    gtk_widget_show(dialog->freespace_label);

    dialog->freespace_bar = g_object_new(GTK_TYPE_PROGRESS_BAR, NULL);
    gtk_box_pack_start(GTK_BOX(dialog->freespace_vbox), dialog->freespace_bar, TRUE, TRUE, 0);
    gtk_widget_set_size_request(dialog->freespace_bar, -1, 10);
    gtk_widget_show(dialog->freespace_bar);

    ++row;

    spacer = g_object_new(GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 12, NULL);
    gtk_grid_attach(GTK_GRID(grid), spacer, 0, row, 2, 1);
    gtk_widget_show(spacer);

    ++row;

    /*
       Permissions chooser
     */
    label = gtk_label_new(_("Permissions"));
    dialog->permissions_chooser = permbox_new();
    g_object_bind_property(G_OBJECT(dialog), "files", G_OBJECT(dialog->permissions_chooser), "files", G_BINDING_SYNC_CREATE);
    gtk_notebook_append_page(GTK_NOTEBOOK(dialog->notebook), dialog->permissions_chooser, label);
    gtk_widget_show(dialog->permissions_chooser);
    gtk_widget_show(label);
}

static void propsdlg_dispose(GObject *object)
{
    PropertiesDialog *dialog = PROPERTIESDIALOG(object);

    // reset the file displayed by the dialog
    propsdlg_set_files(dialog, NULL);

    G_OBJECT_CLASS(propsdlg_parent_class)->dispose(object);
}

static void propsdlg_finalize(GObject *object)
{
    PropertiesDialog *dialog = PROPERTIESDIALOG(object);
    e_return_if_fail(dialog->files == NULL);

    G_OBJECT_CLASS(propsdlg_parent_class)->finalize(object);
}

static void propsdlg_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    PropertiesDialog *dialog = PROPERTIESDIALOG(object);

    switch (prop_id)
    {
    case PROP_FILES:
        g_value_set_boxed(value, _propsdlg_get_files(dialog));
        break;

    case PROP_FILE_SIZE_BINARY:
        g_value_set_boolean(value, dialog->file_size_binary);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void propsdlg_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    PropertiesDialog *dialog = PROPERTIESDIALOG(object);

    switch (prop_id)
    {
    case PROP_FILES:
        propsdlg_set_files(dialog, g_value_get_boxed(value));
        break;

    case PROP_FILE_SIZE_BINARY:
        dialog->file_size_binary = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static GList* _propsdlg_get_files(PropertiesDialog *dialog)
{
    e_return_val_if_fail(IS_PROPERTIESDIALOG(dialog), NULL);

    return dialog->files;
}

void propsdlg_set_files(PropertiesDialog *dialog, GList *files)
{
    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));

    // check if the same lists are used(or null)
    if (G_UNLIKELY(dialog->files == files))
        return;

    ThunarFile *file;

    // disconnect from any previously set files
    for (GList *lp = dialog->files; lp != NULL; lp = lp->next)
    {
        file = THUNAR_FILE(lp->data);

        // unregister our file watch
        th_file_unwatch(file);

        // unregister handlers
        g_signal_handlers_disconnect_by_func(G_OBJECT(file), _propsdlg_update, dialog);
        g_signal_handlers_disconnect_by_func(G_OBJECT(file), gtk_widget_destroy, dialog);

        g_object_unref(G_OBJECT(file));
    }

    g_list_free(dialog->files);

    // activate the new list
    dialog->files = g_list_copy(files);

    // connect to the new files
    for (GList *lp = dialog->files; lp != NULL; lp = lp->next)
    {
        e_assert(THUNAR_IS_FILE(lp->data));
        file = THUNAR_FILE(g_object_ref(G_OBJECT(lp->data)));

        // watch the file for changes
        th_file_watch(file);

        // install signal handlers
        g_signal_connect_swapped(G_OBJECT(file), "changed",
                                 G_CALLBACK(_propsdlg_update), dialog);
        g_signal_connect_swapped(G_OBJECT(file), "destroy",
                                 G_CALLBACK(gtk_widget_destroy), dialog);
    }

    // update the dialog contents
    if (dialog->files != NULL)
    {
        // update the UI for the new file
        _propsdlg_update(dialog);

        // update the provider property pages
        //thunar_properties_dialog_update_providers(dialog);
    }

    // tell everybody that we have a new file here
    g_object_notify(G_OBJECT(dialog), "files");
}

static void propsdlg_response(GtkDialog *dialog, gint response)
{
    if (response == GTK_RESPONSE_CLOSE)
    {
        gtk_widget_destroy(GTK_WIDGET(dialog));
    }
    else if (response == GTK_RESPONSE_HELP)
    {
        xfce_dialog_show_help(GTK_WINDOW(dialog), "thunar",
                               "working-with-files-and-folders",
                               "file_properties");
    }
    else if (GTK_DIALOG_CLASS(propsdlg_parent_class)->response != NULL)
    {
        GTK_DIALOG_CLASS(propsdlg_parent_class)->response(dialog, response);
    }
}

static gboolean propsdlg_reload(PropertiesDialog *dialog)
{
    // reload the active files
    g_list_foreach(dialog->files,(GFunc)(void(*)(void)) th_file_reload, NULL);

    return dialog->files != NULL;
}

// Events ---------------------------------------------------------------------

static void _propsdlg_name_activate(GtkWidget *entry, PropertiesDialog *dialog)
{
    (void) entry;

    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));

    // check if we still have a valid file and if the user is allowed to rename
    if (G_UNLIKELY(!gtk_widget_get_sensitive(GTK_WIDGET(xfce_filename_input_get_entry(dialog->name_entry)))
                    || g_list_length(dialog->files) != 1))
        return;

    // determine new and old name
    ThunarFile *file = THUNAR_FILE(dialog->files->data);

    const gchar *new_name;
    new_name = xfce_filename_input_get_text(dialog->name_entry);

    const gchar *old_name = th_file_get_display_name(file);

    if (g_utf8_collate(new_name, old_name) == 0)
        return;

    ThunarJob *job = io_rename_file(file, new_name);
    if (!job)
        return;

    g_signal_connect(job, "error",
                     G_CALLBACK(_propsdlg_rename_error), dialog);

    g_signal_connect(job, "finished",
                     G_CALLBACK(_propsdlg_rename_finished), dialog);
}

static void _propsdlg_rename_error(ExoJob *job, GError *error,
                                   PropertiesDialog *dialog)
{
    e_return_if_fail(IS_EXOJOB(job));
    e_return_if_fail(error != NULL);
    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(g_list_length(dialog->files) == 1);

    /* reset the entry display name to the original name, so the focus
       out event does not trigger the rename again by calling
       thunar_properties_dialog_name_activate */
    gtk_entry_set_text(GTK_ENTRY(xfce_filename_input_get_entry(dialog->name_entry)),
                        th_file_get_display_name(THUNAR_FILE(dialog->files->data)));

    // display an error message
    dialog_error(GTK_WIDGET(dialog), error, _("Failed to rename \"%s\""),
                               th_file_get_display_name(THUNAR_FILE(dialog->files->data)));
}

static void _propsdlg_rename_finished(ExoJob *job, PropertiesDialog *dialog)
{
    e_return_if_fail(IS_EXOJOB(job));
    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(g_list_length(dialog->files) == 1);

    g_signal_handlers_disconnect_matched(job,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL,
                                         dialog);
    g_object_unref(job);
}

static gboolean _propsdlg_name_focus_out_event(GtkWidget *entry,
                                               GdkEventFocus *event,
                                               PropertiesDialog *dialog)
{
    (void) event;

    _propsdlg_name_activate(entry, dialog);

    return FALSE;
}

// Public ---------------------------------------------------------------------

// propsdlg_set_files
static void _propsdlg_update(PropertiesDialog *dialog)
{
    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(dialog->files != NULL);

    if (dialog->files->next == NULL)
    {
        // show single file name box
        gtk_widget_show(dialog->single_box);

        // update the properties for a dialog showing 1 file
        _propsdlg_update_single(dialog);

        return;
    }

    // show multiple files box
    gtk_widget_hide(dialog->single_box);

    // update the properties for a dialog showing multiple files
    _propsdlg_update_multiple(dialog);
}

static void _propsdlg_update_single(PropertiesDialog *dialog)
{
    IconFactory *icon_factory;
    GtkIconTheme      *icon_theme;
    const gchar       *content_type;
    const gchar       *name;
    const gchar       *path;
    GVolume           *volume;
    GIcon             *gicon;
    glong              offset;
    gchar             *date;
    gchar             *display_name;
    gchar             *fs_string;
    gchar             *str;
    gchar             *volume_name;
    gchar             *volume_id;
    gchar             *volume_label;
    ThunarFile        *file;
    ThunarFile        *parent_file;
    gboolean           show_chooser;
    guint64            fs_free;
    guint64            fs_size;
    gdouble            fs_fraction = 0.0;

    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(g_list_length(dialog->files) == 1);
    e_return_if_fail(THUNAR_IS_FILE(dialog->files->data));

    // whether the dialog shows a single file or a group of files
    file = THUNAR_FILE(dialog->files->data);

    // hide the permissions chooser for trashed files
    gtk_widget_set_visible(dialog->permissions_chooser, !th_file_is_trashed(file));

    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(GTK_WIDGET(dialog)));
    icon_factory = iconfact_get_for_icon_theme(icon_theme);


    // update the properties dialog title
    str = g_strdup_printf(_("%s - Properties"), th_file_get_display_name(file));
    gtk_window_set_title(GTK_WINDOW(dialog), str);
    g_free(str);

    // update the preview image
    th_image_set_file(THUNAR_IMAGE(dialog->icon_image), file);

    // check if the icon may be changed(only for writable .desktop files)
    g_object_ref(G_OBJECT(dialog->icon_image));
    gtk_container_remove(GTK_CONTAINER(gtk_widget_get_parent(dialog->icon_image)), dialog->icon_image);
    if (th_file_is_writable(file)
            && th_file_is_desktop_file(file, NULL))
    {
        gtk_container_add(GTK_CONTAINER(dialog->icon_button), dialog->icon_image);
        gtk_widget_show(dialog->icon_button);
    }
    else
    {
        gtk_box_pack_start(GTK_BOX(gtk_widget_get_parent(dialog->icon_button)), dialog->icon_image, FALSE, TRUE, 0);
        gtk_widget_hide(dialog->icon_button);
    }
    g_object_unref(G_OBJECT(dialog->icon_image));

    // update the name(if it differs)
    gtk_editable_set_editable(GTK_EDITABLE(xfce_filename_input_get_entry(dialog->name_entry)),
                               th_file_is_renameable(file));
    name = th_file_get_display_name(file);
    if (G_LIKELY(strcmp(name, xfce_filename_input_get_text(dialog->name_entry)) != 0))
    {
        gtk_entry_set_text(xfce_filename_input_get_entry(dialog->name_entry), name);

        // grab the input focus to the name entry
        gtk_widget_grab_focus(GTK_WIDGET(xfce_filename_input_get_entry(dialog->name_entry)));

        // select the pre-dot part of the name
        str = util_str_get_extension(name);
        if (G_LIKELY(str != NULL))
        {
            // calculate the offset
            offset = g_utf8_pointer_to_offset(name, str);

            // select the region
            if (G_LIKELY(offset > 0))
                gtk_editable_select_region(GTK_EDITABLE(xfce_filename_input_get_entry(dialog->name_entry)), 0, offset);
        }
    }

    // update the content type
    content_type = th_file_get_content_type(file);
    if (content_type != NULL)
    {
        if (G_UNLIKELY(g_content_type_equals(content_type, "inode/symlink")))
            str = g_strdup(_("broken link"));
        else if (G_UNLIKELY(th_file_is_symlink(file)))
            str = g_strdup_printf(_("link to %s"), th_file_get_symlink_target(file));
        else
            str = g_content_type_get_description(content_type);

        gtk_widget_set_tooltip_text(dialog->kind_ebox, content_type);
        gtk_label_set_text(GTK_LABEL(dialog->kind_label), str);
        g_free(str);
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(dialog->kind_label), _("unknown"));
    }

    // update the application chooser(shown only for non-executable regular files!)
    show_chooser = th_file_is_regular(file) && !th_file_is_executable(file);
    gtk_widget_set_visible(dialog->openwith_chooser, show_chooser);
    if (show_chooser)
        appcombo_set_file(APPCOMBO(dialog->openwith_chooser), file);

    // update the link target
    path = th_file_is_symlink(file) ? th_file_get_symlink_target(file) : NULL;
    if (G_UNLIKELY(path != NULL))
    {
        display_name = g_filename_display_name(path);
        gtk_label_set_text(GTK_LABEL(dialog->link_label), display_name);
        gtk_widget_show(dialog->link_label);
        g_free(display_name);
    }
    else
    {
        gtk_widget_hide(dialog->link_label);
    }

    // update the original path
    path = th_file_get_original_path(file);
    if (G_UNLIKELY(path != NULL))
    {
        display_name = g_filename_display_name(path);
        gtk_label_set_text(GTK_LABEL(dialog->origin_label), display_name);
        gtk_widget_show(dialog->origin_label);
        g_free(display_name);
    }
    else
    {
        gtk_widget_hide(dialog->origin_label);
    }

    // update the file or folder location(parent)
    parent_file = th_file_get_parent(file, NULL);
    if (G_UNLIKELY(parent_file != NULL))
    {
        display_name = g_file_get_parse_name(th_file_get_file(parent_file));
        gtk_label_set_text(GTK_LABEL(dialog->location_label), display_name);
        gtk_widget_show(dialog->location_label);
        g_object_unref(G_OBJECT(parent_file));
        g_free(display_name);
    }
    else
    {
        gtk_widget_hide(dialog->location_label);
    }

    // determine the style used to format dates
    ThunarDateStyle date_style = THUNAR_DATE_STYLE_YYYYMMDD;
    gchar *date_custom_style = NULL;

    // update the deleted time
    date = th_file_get_deletion_date(file, date_style, date_custom_style);
    if (G_LIKELY(date != NULL))
    {
        gtk_label_set_text(GTK_LABEL(dialog->deleted_label), date);
        gtk_widget_show(dialog->deleted_label);
        g_free(date);
    }
    else
    {
        gtk_widget_hide(dialog->deleted_label);
    }

    // update the modified time
    date = th_file_get_date_string(file, THUNAR_FILE_DATE_MODIFIED, date_style, date_custom_style);
    if (G_LIKELY(date != NULL))
    {
        gtk_label_set_text(GTK_LABEL(dialog->modified_label), date);
        gtk_widget_show(dialog->modified_label);
        g_free(date);
    }
    else
    {
        gtk_widget_hide(dialog->modified_label);
    }

    // update the accessed time
    date = th_file_get_date_string(file, THUNAR_FILE_DATE_ACCESSED, date_style, date_custom_style);
    if (G_LIKELY(date != NULL))
    {
        gtk_label_set_text(GTK_LABEL(dialog->accessed_label), date);
        gtk_widget_show(dialog->accessed_label);
        g_free(date);
    }
    else
    {
        gtk_widget_hide(dialog->accessed_label);
    }

    // update the free space(only for folders)
    if (th_file_is_directory(file))
    {
        fs_string = e_file_get_free_space_string(th_file_get_file(file),
                    dialog->file_size_binary);
        if (e_file_get_free_space(th_file_get_file(file), &fs_free, &fs_size)
                && fs_size > 0)
        {
            // free disk space fraction
            fs_fraction =((fs_size - fs_free) * 100 / fs_size);
        }
        if (fs_string != NULL)
        {
            gtk_label_set_text(GTK_LABEL(dialog->freespace_label), fs_string);
            gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(dialog->freespace_bar), fs_fraction / 100);
            gtk_widget_show(dialog->freespace_vbox);
            g_free(fs_string);
        }
        else
        {
            gtk_widget_hide(dialog->freespace_vbox);
        }
    }
    else
    {
        gtk_widget_hide(dialog->freespace_vbox);
    }

    // update the volume
    volume = th_file_get_volume(file);
    if (G_LIKELY(volume != NULL))
    {
        gicon = g_volume_get_icon(volume);
        gtk_image_set_from_gicon(GTK_IMAGE(dialog->volume_image), gicon, GTK_ICON_SIZE_MENU);
        if (G_LIKELY(gicon != NULL))
            g_object_unref(gicon);

        volume_name = g_volume_get_name(volume);
        volume_id = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        volume_label= g_strdup_printf("%s(%s)", volume_name, volume_id);
        gtk_label_set_text(GTK_LABEL(dialog->volume_label), volume_label);
        gtk_widget_show(dialog->volume_label);
        g_free(volume_name);
        g_free(volume_id);
        g_free(volume_label);

        g_object_unref(G_OBJECT(volume));
    }
    else
    {
        gtk_widget_hide(dialog->volume_label);
    }

    // cleanup
    g_object_unref(G_OBJECT(icon_factory));
}

static void _propsdlg_update_multiple(PropertiesDialog *dialog)
{
    ThunarFile  *file;
    GString     *names_string;
    gboolean     first_file = TRUE;
    GList       *lp;
    const gchar *content_type = NULL;
    const gchar *tmp;
    gchar       *str;
    GVolume     *volume = NULL;
    GVolume     *tmp_volume;
    GIcon       *gicon;
    gchar       *volume_name;
    gchar       *volume_id;
    gchar       *volume_label;
    gchar       *display_name;
    ThunarFile  *parent_file = NULL;
    ThunarFile  *tmp_parent;
    gboolean     has_trashed_files = FALSE;

    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(g_list_length(dialog->files) > 1);

    // update the properties dialog title
    gtk_window_set_title(GTK_WINDOW(dialog), _("Properties"));

    // widgets not used with > 1 file selected
    gtk_widget_hide(dialog->deleted_label);
    gtk_widget_hide(dialog->modified_label);
    gtk_widget_hide(dialog->accessed_label);
    gtk_widget_hide(dialog->freespace_vbox);
    gtk_widget_hide(dialog->origin_label);
    gtk_widget_hide(dialog->openwith_chooser);
    gtk_widget_hide(dialog->link_label);

    names_string = g_string_new(NULL);

    // collect data of the selected files
    for (lp = dialog->files; lp != NULL; lp = lp->next)
    {
        e_assert(THUNAR_IS_FILE(lp->data));
        file = THUNAR_FILE(lp->data);

        // append the name
        if (!first_file)
            g_string_append(names_string, ", ");
        g_string_append(names_string, th_file_get_display_name(file));

        // update the content type
        if (first_file)
        {
            content_type = th_file_get_content_type(file);
        }
        else if (content_type != NULL)
        {
            // check the types match
            tmp = th_file_get_content_type(file);
            if (tmp == NULL || !g_content_type_equals(content_type, tmp))
                content_type = NULL;
        }

        // check if all selected files are on the same volume
        tmp_volume = th_file_get_volume(file);
        if (first_file)
        {
            volume = tmp_volume;
        }
        else if (tmp_volume != NULL)
        {
            // we only display information if the files are on the same volume
            if (tmp_volume != volume)
            {
                if (volume != NULL)
                    g_object_unref(G_OBJECT(volume));
                volume = NULL;
            }

            g_object_unref(G_OBJECT(tmp_volume));
        }

        // check if all files have the same parent
        tmp_parent = th_file_get_parent(file, NULL);
        if (first_file)
        {
            parent_file = tmp_parent;
        }
        else if (tmp_parent != NULL)
        {
            // we only display the location if they are all equal
            if (!g_file_equal(th_file_get_file(parent_file), th_file_get_file(tmp_parent)))
            {
                if (parent_file != NULL)
                    g_object_unref(G_OBJECT(parent_file));
                parent_file = NULL;
            }

            g_object_unref(G_OBJECT(tmp_parent));
        }

        if (th_file_is_trashed(file))
            has_trashed_files = TRUE;

        first_file = FALSE;
    }

    // set the labels string
    gtk_label_set_text(GTK_LABEL(dialog->names_label), names_string->str);
    gtk_widget_set_tooltip_text(dialog->names_label, names_string->str);
    g_string_free(names_string, TRUE);

    // hide the permissions chooser for trashed files
    gtk_widget_set_visible(dialog->permissions_chooser, !has_trashed_files);

    // update the content type
    if (content_type != NULL
            && !g_content_type_equals(content_type, "inode/symlink"))
    {
        str = g_content_type_get_description(content_type);
        gtk_widget_set_tooltip_text(dialog->kind_ebox, content_type);
        gtk_label_set_text(GTK_LABEL(dialog->kind_label), str);
        g_free(str);
    }
    else
    {
        gtk_label_set_text(GTK_LABEL(dialog->kind_label), _("mixed"));
    }

    // update the file or folder location(parent)
    if (G_UNLIKELY(parent_file != NULL))
    {
        display_name = g_file_get_parse_name(th_file_get_file(parent_file));
        gtk_label_set_text(GTK_LABEL(dialog->location_label), display_name);
        gtk_widget_show(dialog->location_label);
        g_object_unref(G_OBJECT(parent_file));
        g_free(display_name);
    }
    else
    {
        gtk_widget_hide(dialog->location_label);
    }

    // update the volume
    if (G_LIKELY(volume != NULL))
    {
        gicon = g_volume_get_icon(volume);
        gtk_image_set_from_gicon(GTK_IMAGE(dialog->volume_image), gicon, GTK_ICON_SIZE_MENU);
        if (G_LIKELY(gicon != NULL))
            g_object_unref(gicon);

        volume_name = g_volume_get_name(volume);
        volume_id = g_volume_get_identifier(volume, G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        volume_label = g_strdup_printf("%s(%s)", volume_name, volume_id);
        gtk_label_set_text(GTK_LABEL(dialog->volume_label), volume_label);
        gtk_widget_show(dialog->volume_label);
        g_free(volume_name);
        g_free(volume_id);
        g_free(volume_label);

        g_object_unref(G_OBJECT(volume));
    }
    else
    {
        gtk_widget_hide(dialog->volume_label);
    }
}

// Public ---------------------------------------------------------------------

GtkWidget* propsdlg_new(GtkWindow *parent)
{
    e_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), NULL);
    return g_object_new(TYPE_PROPERTIESDIALOG,
                         "transient-for", parent,
                         "destroy-with-parent", parent != NULL,
                         NULL);
}

void propsdlg_set_file(PropertiesDialog *dialog, ThunarFile *file)
{
    e_return_if_fail(IS_PROPERTIESDIALOG(dialog));
    e_return_if_fail(file == NULL || THUNAR_IS_FILE(file));

    if (file == NULL)
    {
        propsdlg_set_files(dialog, NULL);
        return;
    }

    // create a fake list
    GList foo;
    foo.next = NULL;
    foo.prev = NULL;
    foo.data = file;

    propsdlg_set_files(dialog, &foo);
}


