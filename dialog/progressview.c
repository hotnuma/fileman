/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
#include <progressview.h>

#include <dialogs.h>
#include <transferjob.h>
#include <pango_ext.h>

// ProgressView ---------------------------------------------------------------

static void progressview_finalize(GObject *object);
static void progressview_dispose(GObject *object);
static void progressview_get_property(GObject *object, guint prop_id,
                                      GValue *value, GParamSpec *pspec);
static void progressview_set_property(GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec);
static void _progressview_set_job(ProgressView *view, ThunarJob *job);

// Events ---------------------------------------------------------------------

// progressview_init
static void _progressview_cancel_job(ProgressView *view);

static ThunarJobResponse _progressview_ask(ProgressView *view,
                                           const gchar *message,
                                           ThunarJobResponse choices,
                                           ThunarJob *job);
static ThunarJobResponse _progressview_ask_replace(ProgressView *view,
                                                   ThunarFile *src_file,
                                                   ThunarFile *dst_file,
                                                   ThunarJob *job);
static void _progressview_error(ProgressView *view, GError *error, ExoJob *job);
static void _progressview_finished(ProgressView *view, ExoJob *job);
static void _progressview_info_message(ProgressView *view, const gchar *message,
                                      ExoJob *job);
static void _progressview_percent(ProgressView *view, gdouble percent, ExoJob *job);
static void _progressview_frozen(ProgressView *view, ExoJob *job);
static void _progressview_unfrozen(ProgressView *view, ExoJob *job);

//static void progressview_pause_job(ProgressView *view);
//static void progressview_unpause_job(ProgressView *view);

// ProgressView ---------------------------------------------------------------

enum
{
    PROP_0,
    PROP_JOB,
    PROP_ICON_NAME,
    PROP_TITLE,
};

struct _ProgressViewClass
{
    GtkBoxClass __parent__;
};

struct _ProgressView
{
    GtkBox      __parent__;

    ThunarJob   *job;

    GtkWidget   *progress_bar;
    GtkWidget   *progress_label;
    GtkWidget   *message_label;

    //GtkWidget *pause_button;
    //GtkWidget *unpause_button;

    gchar       *icon_name;
    gchar       *title;
};

G_DEFINE_TYPE(ProgressView, progressview, GTK_TYPE_BOX)

static void progressview_class_init(ProgressViewClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = progressview_finalize;
    gobject_class->dispose = progressview_dispose;
    gobject_class->get_property = progressview_get_property;
    gobject_class->set_property = progressview_set_property;

    /**
     * ProgressView:job:
     *
     * The #ThunarJob, whose progress is displayed by this view, or
     * %NULL if no job is set.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_JOB,
                                    g_param_spec_object("job", "job", "job",
                                             THUNAR_TYPE_JOB,
                                             E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_ICON_NAME,
                                    g_param_spec_string("icon-name",
                                             "icon-name",
                                             "icon-name",
                                             NULL,
                                             E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_TITLE,
                                    g_param_spec_string("title",
                                             "title",
                                             "title",
                                             NULL,
                                             E_PARAM_READWRITE));

    g_signal_new("need-attention",
                 TYPE_PROGRESSVIEW,
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 0,
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);

    g_signal_new("finished",
                 TYPE_PROGRESSVIEW,
                 G_SIGNAL_RUN_LAST | G_SIGNAL_NO_HOOKS,
                 0,
                 NULL,
                 NULL,
                 g_cclosure_marshal_VOID__VOID,
                 G_TYPE_NONE,
                 0);
}

static void progressview_init(ProgressView *view)
{
    gtk_orientable_set_orientation(GTK_ORIENTABLE(view), GTK_ORIENTATION_VERTICAL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_container_add(GTK_CONTAINER(view), vbox);
    gtk_widget_show(vbox);

    GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    GtkWidget *image = g_object_new(GTK_TYPE_IMAGE,
                                    "icon-size", GTK_ICON_SIZE_DND,
                                    NULL);
    gtk_image_set_pixel_size(GTK_IMAGE(image), 32);
    gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, TRUE, 0);
    g_object_bind_property(G_OBJECT(view), "icon-name",
                           G_OBJECT(image), "icon-name",
                           G_BINDING_SYNC_CREATE);
    gtk_widget_show(image);

    GtkWidget *vbox2 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    gtk_box_pack_start(GTK_BOX(hbox), vbox2, TRUE, TRUE, 0);
    gtk_widget_show(vbox2);

    GtkWidget *label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_attributes(GTK_LABEL(label), e_pango_attr_list_big_bold());
    gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(vbox2), label, TRUE, TRUE, 0);
    gtk_widget_show(label);

    view->message_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_ellipsize(GTK_LABEL(view->message_label), PANGO_ELLIPSIZE_MIDDLE);
    gtk_box_pack_start(GTK_BOX(vbox2), view->message_label, TRUE, TRUE, 0);
    gtk_widget_show(view->message_label);

    hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 6);
    gtk_box_pack_start(GTK_BOX(vbox), hbox, TRUE, TRUE, 0);
    gtk_widget_show(hbox);

    GtkWidget *vbox3 = gtk_box_new(GTK_ORIENTATION_VERTICAL, 3);
    gtk_box_pack_start(GTK_BOX(hbox), vbox3, TRUE, TRUE, 0);
    gtk_widget_show(vbox3);

    view->progress_bar = gtk_progress_bar_new();
    gtk_box_pack_start(GTK_BOX(vbox3), view->progress_bar, TRUE, TRUE, 0);
    gtk_widget_show(view->progress_bar);

    view->progress_label = g_object_new(GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
    gtk_label_set_ellipsize(GTK_LABEL(view->progress_label), PANGO_ELLIPSIZE_END);
    gtk_label_set_attributes(GTK_LABEL(view->progress_label), e_pango_attr_list_small());
    gtk_box_pack_start(GTK_BOX(vbox3), view->progress_label, FALSE, TRUE, 0);
    gtk_widget_show(view->progress_label);

    //view->pause_button = gtk_button_new_from_icon_name("media-playback-pause", GTK_ICON_SIZE_BUTTON);
    //gtk_button_set_label(GTK_BUTTON(view->pause_button), _("Pause"));
    //g_signal_connect_swapped(view->pause_button, "clicked",
    //                         G_CALLBACK(progressview_pause_job), view);
    //gtk_box_pack_start(GTK_BOX(hbox), view->pause_button, FALSE, FALSE, 0);
    //gtk_widget_set_can_focus(view->pause_button, FALSE);
    //gtk_widget_hide(view->pause_button);

    //view->unpause_button = gtk_button_new_from_icon_name("media-playback-start", GTK_ICON_SIZE_BUTTON);
    //gtk_button_set_label(GTK_BUTTON(view->unpause_button), _("Resume"));
    //g_signal_connect_swapped(view->unpause_button, "clicked",
    //                         G_CALLBACK(progressview_unpause_job), view);
    //gtk_box_pack_start(GTK_BOX(hbox), view->unpause_button, FALSE, FALSE, 0);
    //gtk_widget_set_can_focus(view->unpause_button, FALSE);
    //gtk_widget_hide(view->unpause_button);

    GtkWidget *cancel_button = gtk_button_new_from_icon_name("process-stop",
                                                             GTK_ICON_SIZE_BUTTON);
    gtk_button_set_label(GTK_BUTTON(cancel_button), _("Cancel"));
    g_signal_connect_swapped(cancel_button, "clicked",
                             G_CALLBACK(_progressview_cancel_job), view);
    gtk_box_pack_start(GTK_BOX(hbox), cancel_button, FALSE, FALSE, 0);
    gtk_widget_set_can_focus(cancel_button, FALSE);
    gtk_widget_show(cancel_button);

    // connect the view title to the action label
    g_object_bind_property(G_OBJECT(view), "title",
                           G_OBJECT(label), "label",
                           G_BINDING_SYNC_CREATE);
}

static void progressview_finalize(GObject *object)
{
    ProgressView *view = PROGRESSVIEW(object);

    g_free(view->icon_name);
    g_free(view->title);

    G_OBJECT_CLASS(progressview_parent_class)->finalize(object);
}

static void progressview_dispose(GObject *object)
{
    ProgressView *view = PROGRESSVIEW(object);

    // disconnect from the job (if any)
    if (view->job != NULL)
    {
        exo_job_cancel(EXOJOB(view->job));
        _progressview_set_job(view, NULL);
    }

    G_OBJECT_CLASS(progressview_parent_class)->dispose(object);
}

static void progressview_get_property(GObject *object, guint prop_id,
                                      GValue *value, GParamSpec *pspec)
{
    (void) pspec;
    ProgressView *view = PROGRESSVIEW(object);

    switch (prop_id)
    {
    case PROP_JOB:
        g_value_set_object(value, progressview_get_job(view));
        break;

    case PROP_ICON_NAME:
        g_value_set_string(value, view->icon_name);
        break;

    case PROP_TITLE:
        g_value_set_string(value, view->title);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void progressview_set_property(GObject *object, guint prop_id,
                                      const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ProgressView *view = PROGRESSVIEW(object);

    switch (prop_id)
    {
    case PROP_JOB:
        _progressview_set_job(view, g_value_get_object(value));
        break;

    case PROP_ICON_NAME:
        progressview_set_icon_name(view, g_value_get_string(value));
        break;

    case PROP_TITLE:
        progressview_set_title(view, g_value_get_string(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Properties -----------------------------------------------------------------

ThunarJob* progressview_get_job(ProgressView *view)
{
    e_return_val_if_fail(IS_PROGRESSVIEW(view), NULL);

    return view->job;
}

static void _progressview_set_job(ProgressView *view, ThunarJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(job == NULL || THUNAR_IS_JOB(job));

    // check if we're already on that job
    if (G_UNLIKELY(view->job == job))
        return;

    // disconnect from the previous job
    if (G_LIKELY(view->job != NULL))
    {
        g_signal_handlers_disconnect_matched(view->job,
                                             G_SIGNAL_MATCH_DATA,
                                             0, 0, NULL, NULL,
                                             view);
        g_object_unref(G_OBJECT(view->job));
    }

    // activate the new job
    view->job = job;

    // connect to the new job
    if (G_LIKELY(job != NULL))
    {
        g_object_ref(job);

        g_signal_connect_swapped(job, "ask",
                                 G_CALLBACK(_progressview_ask), view);
        g_signal_connect_swapped(job, "ask-replace",
                                 G_CALLBACK(_progressview_ask_replace), view);
        g_signal_connect_swapped(job, "error",
                                 G_CALLBACK(_progressview_error), view);
        g_signal_connect_swapped(job, "finished",
                                 G_CALLBACK(_progressview_finished), view);
        g_signal_connect_swapped(job, "info-message",
                                 G_CALLBACK(_progressview_info_message), view);
        g_signal_connect_swapped(job, "percent",
                                 G_CALLBACK(_progressview_percent), view);
        g_signal_connect_swapped(job, "frozen",
                                 G_CALLBACK(_progressview_frozen), view);
        g_signal_connect_swapped(job, "unfrozen",
                                 G_CALLBACK(_progressview_unfrozen), view);

        //if (job_is_pausable(job))
        //{
        //    gtk_widget_show(view->pause_button);
        //}
    }

    g_object_notify(G_OBJECT(view), "job");
}

void progressview_set_icon_name(ProgressView *view, const gchar *icon_name)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));

    if (g_strcmp0(view->icon_name, icon_name) == 0)
        return;

    g_free(view->icon_name);
    view->icon_name = g_strdup(icon_name);

    g_object_notify(G_OBJECT(view), "icon-name");
}

void progressview_set_title(ProgressView *view, const gchar *title)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));

    if (g_strcmp0(view->title, title) == 0)
        return;

    g_free(view->title);
    view->title = g_strdup(title);

    g_object_notify(G_OBJECT(view), "title");
}

// Events ---------------------------------------------------------------------

static void _progressview_cancel_job(ProgressView *view)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(THUNAR_IS_JOB(view->job));

    if (view->job != NULL)
    {
        // cancel the job
        exo_job_cancel(EXOJOB(view->job));

        // don't listen to frozen/unfrozen states updates any more
        g_signal_handlers_disconnect_matched(view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                              _progressview_frozen, NULL);

        g_signal_handlers_disconnect_matched(view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                              _progressview_unfrozen, NULL);

        // don't listen to percentage updates any more
        g_signal_handlers_disconnect_matched(view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                              _progressview_percent, NULL);

        // don't listen to info messages any more
        g_signal_handlers_disconnect_matched(view->job, G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                                              _progressview_info_message, NULL);

        // update the status text
        gtk_label_set_text(GTK_LABEL(view->progress_label), _("Cancelling..."));
    }
}

static ThunarJobResponse _progressview_ask(ProgressView *view,
                                           const gchar *message,
                                           ThunarJobResponse choices,
                                           ThunarJob *job)
{
    e_return_val_if_fail(IS_PROGRESSVIEW(view), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(g_utf8_validate(message, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(THUNAR_IS_JOB(job), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(view->job == job, THUNAR_JOB_RESPONSE_CANCEL);

    // be sure to display the corresponding dialog prior to opening the question view
    g_signal_emit_by_name(view, "need-attention");

    // determine the toplevel window of the view
    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    // display the question view
    return dialog_job_ask(window != NULL ? GTK_WINDOW(window) : NULL,
                                        message, choices);
}

static ThunarJobResponse _progressview_ask_replace(ProgressView *view,
                                                   ThunarFile *src_file,
                                                   ThunarFile *dst_file,
                                                   ThunarJob *job)
{
    e_return_val_if_fail(IS_PROGRESSVIEW(view), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(THUNAR_IS_JOB(job), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(view->job == job, THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(THUNAR_IS_FILE(src_file), THUNAR_JOB_RESPONSE_CANCEL);
    e_return_val_if_fail(THUNAR_IS_FILE(dst_file), THUNAR_JOB_RESPONSE_CANCEL);

    // be sure to display the corresponding dialog prior to opening the question view
    g_signal_emit_by_name(view, "need-attention");

    // determine the toplevel window of the view
    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    // display the question view
    return dialog_job_ask_replace(window != NULL ? GTK_WINDOW(window) : NULL,
                                  src_file, dst_file);
}

static void _progressview_error(ProgressView *view, GError *error, ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(error != NULL && error->message != NULL);
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    // be sure to display the corresponding dialog prior to opening the question view
    g_signal_emit_by_name(view, "need-attention");

    // determine the toplevel window of the view
    GtkWidget *window = gtk_widget_get_toplevel(GTK_WIDGET(view));

    // display the error message
    dialog_job_error(window != NULL ? GTK_WINDOW(window) : NULL, error);
}

static void _progressview_finished(ProgressView *view, ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    // emit finished signal to notify others that the job is finished
    g_signal_emit_by_name(view, "finished");
}

static void _progressview_info_message(ProgressView *view, const gchar *message,
                                       ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(g_utf8_validate(message, -1, NULL));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    gtk_label_set_text(GTK_LABEL(view->message_label), message);
}

static void _progressview_percent(ProgressView *view, gdouble percent, ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(percent >= 0.0 && percent <= 100.0);
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    // update progressbar
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(view->progress_bar),
                                  percent / 100.0);

    gchar *text;

    // set progress text
    if (IS_TRANSFERJOB(job))
        text = transferjob_get_status(TRANSFERJOB(job));
    else
        text = g_strdup_printf("%.2f%%", percent);

    gtk_label_set_text(GTK_LABEL(view->progress_label), text);

    g_free(text);
}

static void _progressview_frozen(ProgressView *view, ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    if (IS_TRANSFERJOB(job))
    {
        // update the UI
        //gtk_widget_hide(view->pause_button);
        //gtk_widget_show(view->unpause_button);

        gtk_label_set_text(GTK_LABEL(view->progress_label),
                           _("Frozen by another job on same device"));
    }
}

static void _progressview_unfrozen(ProgressView *view, ExoJob *job)
{
    e_return_if_fail(IS_PROGRESSVIEW(view));
    e_return_if_fail(THUNAR_IS_JOB(job));
    e_return_if_fail(view->job == THUNAR_JOB(job));

    if (IS_TRANSFERJOB(job))
    {
        // update the UI
        //gtk_widget_hide(view->unpause_button);
        //gtk_widget_show(view->pause_button);

        gtk_label_set_text(GTK_LABEL(view->progress_label), _("Unfreezing..."));
    }
}

// Public ---------------------------------------------------------------------

GtkWidget* progressview_new_with_job(ThunarJob *job)
{
    e_return_val_if_fail(job == NULL || THUNAR_IS_JOB(job), NULL);

    return g_object_new(TYPE_PROGRESSVIEW,
                        "job", job,
                        "orientation", GTK_ORIENTATION_VERTICAL,
                        NULL);
}

// Pause ----------------------------------------------------------------------

//static void progressview_pause_job(ProgressView *view)
//{
//    e_return_if_fail(IS_PROGRESSVIEW(view));
//    e_return_if_fail(THUNAR_IS_JOB(view->job));

//    if (view->job != NULL)
//    {
//        // pause the job
//        job_pause(view->job);

//        // update the UI
//        gtk_widget_hide(view->pause_button);
//        gtk_widget_show(view->unpause_button);
//        gtk_label_set_text(GTK_LABEL(view->progress_label), _("Paused"));
//    }
//}

//static void progressview_unpause_job(ProgressView *view)
//{
//    e_return_if_fail(IS_PROGRESSVIEW(view));
//    e_return_if_fail(THUNAR_IS_JOB(view->job));

//    if (view->job != NULL)
//    {
//        if (job_is_paused(view->job))
//            job_resume(view->job);
//        if (job_is_frozen(view->job))
//            job_unfreeze(view->job);
//        // update the UI
//        gtk_widget_hide(view->unpause_button);
//        gtk_widget_show(view->pause_button);
//        gtk_label_set_text(GTK_LABEL(view->progress_label), _("Resuming..."));
//    }
//}


