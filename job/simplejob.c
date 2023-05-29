/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 *(at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include <config.h>
#include <simplejob.h>

#include <gobject/gvaluecollector.h>

static void simplejob_finalize(GObject *object);
static gboolean simplejob_execute(ExoJob *job, GError **error);

// SimpleJob ------------------------------------------------------------------

struct _SimpleJobClass
{
    ThunarJobClass __parent__;
};

struct _SimpleJob
{
    ThunarJob       __parent__;

    SimpleJobFunc   func;
    GArray          *param_values;
};

G_DEFINE_TYPE(SimpleJob, simplejob, TYPE_THUNARJOB)

static void simplejob_class_init(SimpleJobClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = simplejob_finalize;

    ExoJobClass *exojob_class = EXOJOB_CLASS(klass);
    exojob_class->execute = simplejob_execute;
}

static void simplejob_init(SimpleJob *simple_job)
{
    (void) simple_job;
}

static void simplejob_finalize(GObject *object)
{
    SimpleJob *simple_job = THUNAR_SIMPLE_JOB(object);

    // release the param values
    for (guint i = 0; i < simple_job->param_values->len; ++i)
        g_value_unset(&g_array_index(simple_job->param_values, GValue, i));

    g_array_free(simple_job->param_values, TRUE);

    G_OBJECT_CLASS(simplejob_parent_class)->finalize(object);
}

static gboolean simplejob_execute(ExoJob *job, GError **error)
{
    SimpleJob *simple_job = THUNAR_SIMPLE_JOB(job);
    gboolean         success = TRUE;
    GError          *err = NULL;

    e_return_val_if_fail(THUNAR_IS_SIMPLE_JOB(job), FALSE);
    e_return_val_if_fail(simple_job->func != NULL, FALSE);

    // try to execute the job using the supplied function
    success = simple_job->func(THUNAR_JOB(job), simple_job->param_values, &err);

    if (!success)
    {
        g_assert(err != NULL || exo_job_is_cancelled(job));

        /* set error if the job was cancelled. otherwise just propagate
         * the results of the processing function */
        if (exo_job_set_error_if_cancelled(job, error))
        {
            g_clear_error(&err);
        }
        else
        {
            if (err != NULL)
                g_propagate_error(error, err);
        }

        return FALSE;
    }
    else
        return TRUE;
}

// Public ---------------------------------------------------------------------

/**
 * simplejob_launch:
 * @func           : the #SimpleJobFunc to execute the job.
 * @n_param_values : the number of parameters to pass to the @func.
 * @...            : a list of #GType and parameter pairs(exactly
 *                   @n_param_values pairs) that are passed to @func.
 *
 * Allocates a new #SimpleJob, which executes the specified
 * @func with the specified parameters.
 *
 * For example the listdir @func expects a #ThunarPath for the
 * folder to list, so the call to simplejob_launch()
 * would look like this:
 *
 * <informalexample><programlisting>
 * simplejob_launch(_thunar_io_jobs_listdir, 1,
 *                               THUNAR_TYPE_PATH, path);
 * </programlisting></informalexample>
 *
 * The caller is responsible to release the returned object using
 * thunar_job_unref() when no longer needed.
 *
 * Return value: the launched #ThunarJob.
 **/
ThunarJob* simplejob_launch(SimpleJobFunc func, guint n_param_values, ...)
{
    // allocate and initialize the simple job
    SimpleJob *simple_job;
    simple_job = g_object_new(TYPE_SIMPLEJOB, NULL);
    simple_job->func = func;
    simple_job->param_values = g_array_sized_new(FALSE, TRUE, sizeof(GValue), n_param_values);

    va_list var_args;
    gchar *error_message;
    GValue value = { 0, };

    // collect the parameters
    va_start(var_args, n_param_values);
    for (guint n = 0; n < n_param_values; ++n)
    {
        // initialize the value to hold the next parameter
        g_value_init(&value, va_arg(var_args, GType));

        // collect the value from the stack
        G_VALUE_COLLECT(&value, var_args, 0, &error_message);

        // check if an error occurred
        if (error_message != NULL)
        {
            g_error("%s: %s", G_STRLOC, error_message);
            g_free(error_message);
        }

        g_array_insert_val(simple_job->param_values, n, value);

        /* manually unset the value, g_value_unset doesn't work
         * because we don't want to free the data */
        memset(&value, 0, sizeof(GValue));
    }
    va_end(var_args);

    // launch the job
    return THUNAR_JOB(exo_job_launch(EXOJOB(simple_job)));
}

GArray* simplejob_get_param_values(SimpleJob *job)
{
    e_return_val_if_fail(THUNAR_IS_SIMPLE_JOB(job), NULL);
    return job->param_values;
}


