/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 * Copyright(c) 2012 Nick Schermer <nick@xfce.org>
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
#include "sizelabel.h"

#include "job.h"
#include "dcountjob.h"

static void szlabel_finalize(GObject *object);
static void szlabel_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec);
static void szlabel_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec);
static GList* _szlabel_get_files(SizeLabel *size_label);
static void _szlabel_set_files(SizeLabel *size_label, GList *files);

static gboolean _szlabel_button_press_event(GtkWidget *ebox,
                                            GdkEventButton *event,
                                            SizeLabel *size_label);
static void _szlabel_files_changed(SizeLabel *size_label);
static void _szlabel_error(ExoJob *job, const GError *error,
                           SizeLabel *size_label);
static void _szlabel_finished(ExoJob *job, SizeLabel *size_label);
static void _szlabel_status_update(DeepCountJob *job,
                                   guint64 total_size,
                                   guint file_count,
                                   guint directory_count,
                                   guint unreadable_directory_count,
                                   SizeLabel *size_label);

// SizeLabel ------------------------------------------------------------------

enum
{
    PROP_0,
    PROP_FILES,
    PROP_FILE_SIZE_BINARY
};

struct _SizeLabel
{
    GtkBox      __parent__;

    DeepCountJob *job;

    GList       *files;
    gboolean    file_size_binary;

    GtkWidget   *label;
    GtkWidget   *spinner;
};

struct _SizeLabelClass
{
    GtkBoxClass __parent__;
};

G_DEFINE_TYPE(SizeLabel, szlabel, GTK_TYPE_BOX)


// creation / destruction -----------------------------------------------------

GtkWidget* szlabel_new()
{
    return g_object_new(TYPE_SIZELABEL,
                        "orientation", GTK_ORIENTATION_HORIZONTAL,
                        NULL);
}

static void szlabel_class_init(SizeLabelClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = szlabel_finalize;
    gobject_class->get_property = szlabel_get_property;
    gobject_class->set_property = szlabel_set_property;

    /**
     * SizeLabel:file:
     *
     * The #ThunarFile whose size should be displayed
     * by this #SizeLabel.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_FILES,
                                    g_param_spec_boxed(
                                        "files",
                                        "files",
                                        "files",
                                        TYPE_FILEINFOLIST,
                                        E_PARAM_READWRITE));

    /**
     * PropertiesDialog:file_size_binary:
     *
     * Whether the file size should be shown in binary or decimal.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_FILE_SIZE_BINARY,
                                    g_param_spec_boolean(
                                        "file-size-binary",
                                        "FileSizeBinary",
                                        NULL,
                                        TRUE,
                                        E_PARAM_READWRITE));
}

static void szlabel_init(SizeLabel *size_label)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(size_label),
                                   GTK_ORIENTATION_HORIZONTAL);

    // configure the box
    gtk_box_set_spacing(GTK_BOX(size_label), 6);

    // add an evenbox for the spinner
    GtkWidget *ebox = gtk_event_box_new();
    gtk_event_box_set_visible_window(GTK_EVENT_BOX(ebox), FALSE);
    g_signal_connect(G_OBJECT(ebox), "button-press-event",
                     G_CALLBACK(_szlabel_button_press_event), size_label);
    gtk_widget_set_tooltip_text(ebox,
                                _("Click here to stop calculating the total"
                                  " size of the folder."));
    gtk_box_pack_start(GTK_BOX(size_label), ebox, FALSE, FALSE, 0);

    // add the spinner widget
    size_label->spinner = gtk_spinner_new();
    g_object_bind_property(G_OBJECT(size_label->spinner),
                           "visible", G_OBJECT(ebox),
                           "visible", G_BINDING_SYNC_CREATE);
    gtk_container_add(GTK_CONTAINER(ebox), size_label->spinner);
    gtk_widget_show(size_label->spinner);

    // add the label widget
    size_label->label = gtk_label_new(_("Calculating..."));
    gtk_label_set_xalign(GTK_LABEL(size_label->label), 0.0f);
    gtk_label_set_selectable(GTK_LABEL(size_label->label), TRUE);
    gtk_label_set_line_wrap(GTK_LABEL(size_label->label), TRUE);
    gtk_box_pack_start(GTK_BOX(size_label), size_label->label, TRUE, TRUE, 0);
    gtk_widget_show(size_label->label);
}

static void szlabel_finalize(GObject *object)
{
    SizeLabel *size_label = SIZELABEL(object);

    // cancel the pending job(if any)
    if (size_label->job != NULL)
    {
        g_signal_handlers_disconnect_matched(G_OBJECT(size_label->job),
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, size_label);
        exo_job_cancel(EXOJOB(size_label->job));
        g_object_unref(size_label->job);
    }

    // reset the file property
    _szlabel_set_files(size_label, NULL);

    G_OBJECT_CLASS(szlabel_parent_class)->finalize(object);
}

static void szlabel_get_property(GObject *object, guint prop_id,
                                 GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    SizeLabel *size_label = SIZELABEL(object);

    switch (prop_id)
    {
    case PROP_FILES:
        g_value_set_boxed(value, _szlabel_get_files(size_label));
        break;

    case PROP_FILE_SIZE_BINARY:
        g_value_set_boolean(value, size_label->file_size_binary);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void szlabel_set_property(GObject *object, guint prop_id,
                                 const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    SizeLabel *size_label = SIZELABEL(object);

    switch (prop_id)
    {
    case PROP_FILES:
        _szlabel_set_files(size_label, g_value_get_boxed(value));
        break;

    case PROP_FILE_SIZE_BINARY:
        size_label->file_size_binary = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

/**
 * thunar_size_label_get_files:
 * @size_label : a #SizeLabel.
 *
 * Get the files displayed by the @size_label.
 **/
static GList* _szlabel_get_files(SizeLabel *size_label)
{
    e_return_val_if_fail(IS_SIZELABEL(size_label), NULL);
    return size_label->files;
}

/**
 * thunar_size_label_set_files:
 * @size_label : a #SizeLabel.
 * @files      : a list of #ThunarFile's or %NULL.
 *
 * Sets @file as the #ThunarFile displayed by the @size_label.
 **/
static void _szlabel_set_files(SizeLabel *size_label, GList *files)
{
    GList *lp;

    e_return_if_fail(IS_SIZELABEL(size_label));
    e_return_if_fail(files == NULL || IS_THUNARFILE(files->data));

    // disconnect from the previous files
    for(lp = size_label->files; lp != NULL; lp = lp->next)
    {
        e_assert(IS_THUNARFILE(lp->data));

        g_signal_handlers_disconnect_by_func(G_OBJECT(lp->data),
                                             _szlabel_files_changed,
                                             size_label);
        g_object_unref(G_OBJECT(lp->data));
    }
    g_list_free(size_label->files);

    size_label->files = g_list_copy(files);

    // connect to the new file
    for(lp = size_label->files; lp != NULL; lp = lp->next)
    {
        e_assert(IS_THUNARFILE(lp->data));

        g_object_ref(G_OBJECT(lp->data));
        g_signal_connect_swapped(G_OBJECT(lp->data), "changed",
                                 G_CALLBACK(_szlabel_files_changed),
                                 size_label);
    }

    if (size_label->files != NULL)
        _szlabel_files_changed(size_label);

    // notify listeners
    g_object_notify(G_OBJECT(size_label), "files");
}

// Events ---------------------------------------------------------------------

static gboolean _szlabel_button_press_event(GtkWidget *ebox,
                                            GdkEventButton  *event,
                                            SizeLabel *size_label)
{
    e_return_val_if_fail(GTK_IS_EVENT_BOX(ebox), FALSE);
    e_return_val_if_fail(IS_SIZELABEL(size_label), FALSE);

    // left button press on the spinner cancels the calculation
    if (event->button == 1)
    {
        // cancel the pending job(if any)
        if (size_label->job != NULL)
        {
            g_signal_handlers_disconnect_matched(
                        size_label->job, G_SIGNAL_MATCH_DATA,
                        0, 0, NULL, NULL, size_label);
            exo_job_cancel(EXOJOB(size_label->job));
            g_object_unref(size_label->job);
            size_label->job = NULL;
        }

        // be sure to stop and hide the spinner
        gtk_spinner_stop(GTK_SPINNER(size_label->spinner));
        gtk_widget_hide(size_label->spinner);

        // tell the user that the operation was canceled
        gtk_label_set_text(GTK_LABEL(size_label->label),
                           _("Calculation aborted"));

        // we handled the event
        return TRUE;
    }

    return FALSE;
}

static void _szlabel_files_changed(SizeLabel *size_label)
{
    e_return_if_fail(IS_SIZELABEL(size_label));
    e_return_if_fail(size_label->files != NULL);
    e_return_if_fail(IS_THUNARFILE(size_label->files->data));

    // cancel the pending job(if any)
    if (size_label->job != NULL)
    {
        g_signal_handlers_disconnect_matched(size_label->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL, size_label);
        exo_job_cancel(EXOJOB(size_label->job));
        g_object_unref(size_label->job);
        size_label->job = NULL;
    }

    // check if there are multiple files or the single file is a directory
    if (size_label->files->next != NULL
        || th_file_is_directory(THUNARFILE(size_label->files->data)))
    {
        // schedule a new job to determine the total size of the directory
        // (not following symlinks)
        size_label->job = dcjob_new(size_label->files,
                                    G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS);
        g_signal_connect(size_label->job, "error",
                         G_CALLBACK(_szlabel_error), size_label);
        g_signal_connect(size_label->job, "finished",
                         G_CALLBACK(_szlabel_finished), size_label);
        g_signal_connect(size_label->job, "status-update",
                         G_CALLBACK(_szlabel_status_update), size_label);

        // tell the user that we started calculation
        gtk_label_set_text(GTK_LABEL(size_label->label), _("Calculating..."));
        gtk_spinner_start(GTK_SPINNER(size_label->spinner));
        gtk_widget_show(size_label->spinner);

        // launch the job
        exo_job_launch(EXOJOB(size_label->job));
    }
    else
    {
        // this is going to be quick, stop and hide the spinner
        gtk_spinner_stop(GTK_SPINNER(size_label->spinner));
        gtk_widget_hide(size_label->spinner);

        // determine the size of the file
        guint64 size = th_file_get_size(THUNARFILE(size_label->files->data));

        // setup the new label
        gchar *size_string = g_format_size_full(
                    size,
                    size_label->file_size_binary
                        ? G_FORMAT_SIZE_LONG_FORMAT | G_FORMAT_SIZE_IEC_UNITS
                        : G_FORMAT_SIZE_LONG_FORMAT);
        gtk_label_set_text(GTK_LABEL(size_label->label), size_string);
        g_free(size_string);
    }
}

static void _szlabel_error(ExoJob *job, const GError *error,
                           SizeLabel *size_label)
{
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(IS_SIZELABEL(size_label));
    e_return_if_fail(size_label->job == DEEPCOUNTJOB(job));

    // setup the error text as label
    gtk_label_set_text(GTK_LABEL(size_label->label), error->message);
}

static void _szlabel_finished(ExoJob *job, SizeLabel *size_label)
{
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(IS_SIZELABEL(size_label));
    e_return_if_fail(size_label->job == DEEPCOUNTJOB(job));

    // stop and hide the spinner
    gtk_spinner_stop(GTK_SPINNER(size_label->spinner));
    gtk_widget_hide(size_label->spinner);

    // disconnect from the job
    g_signal_handlers_disconnect_matched(size_label->job,
                                         G_SIGNAL_MATCH_DATA,
                                         0, 0, NULL, NULL, size_label);
    g_object_unref(size_label->job);
    size_label->job = NULL;
}

static void _szlabel_status_update(DeepCountJob *job,
                                   guint64      total_size,
                                   guint        file_count,
                                   guint        directory_count,
                                   guint        unreadable_directory_count,
                                   SizeLabel    *size_label)
{
    gchar             *size_string;
    gchar             *text;
    guint              n;
    gchar             *unreable_text;

    e_return_if_fail(IS_DEEPCOUNTJOB(job));
    e_return_if_fail(IS_SIZELABEL(size_label));
    e_return_if_fail(size_label->job == job);

    // determine the total number of items
    n = file_count + directory_count + unreadable_directory_count;

    if (n > unreadable_directory_count)
    {
        // update the label
        size_string = g_format_size_full(
                total_size,
                G_FORMAT_SIZE_LONG_FORMAT
                | (size_label->file_size_binary
                   ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT));
        text = g_strdup_printf(
                    ngetstr("%u item, totalling %s",
                            "%u items, totalling %s", n),
                    n, size_string);
        g_free(size_string);

        if (unreadable_directory_count > 0)
        {
            /* TRANSLATORS: this is shows if during the deep count size
             * directories were not accessible */
            unreable_text = g_strconcat(text,
                                        "\n",
                                        _("(some contents unreadable)"),
                                        NULL);
            g_free(text);
            text = unreable_text;
        }

        gtk_label_set_text(GTK_LABEL(size_label->label), text);
        g_free(text);
    }
    else
    {
        // nothing was readable, so permission was denied
        gtk_label_set_text(GTK_LABEL(size_label->label),
                           _("Permission denied"));
    }
}


