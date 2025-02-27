/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_JOB_H__
#define __THUNAR_JOB_H__

#include "exo-job.h"
#include "th-file.h"

// ThunarJob ------------------------------------------------------------------

G_BEGIN_DECLS

typedef struct _ThunarJobPrivate ThunarJobPrivate;
typedef struct _ThunarJobClass   ThunarJobClass;
typedef struct _ThunarJob        ThunarJob;

#define TYPE_THUNARJOB (job_get_type())
#define THUNAR_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARJOB, ThunarJob))
#define THUNAR_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARJOB, ThunarJobClass))
#define THUNAR_IS_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARJOB))
#define THUNAR_IS_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_THUNARJOB))
#define THUNAR_JOB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARJOB, ThunarJobClass))

struct _ThunarJobClass
{
    ExoJobClass __parent__;

    // signals
    ThunarJobResponse (*ask) (ThunarJob *job, const gchar *message,
                              ThunarJobResponse choices);
    ThunarJobResponse (*ask_replace) (ThunarJob *job, ThunarFile *source_file,
                                      ThunarFile *target_file);
};

struct _ThunarJob
{
    ExoJob __parent__;

    ThunarJobPrivate *priv;
};

GType job_get_type() G_GNUC_CONST;

void job_set_total_files(ThunarJob *job, GList *total_files);
void job_set_pausable(ThunarJob *job, gboolean pausable);
gboolean job_is_pausable(ThunarJob *job);
void job_pause(ThunarJob *job);
void job_resume(ThunarJob *job);
void job_freeze(ThunarJob *job);
void job_unfreeze(ThunarJob *job);
gboolean job_is_paused(ThunarJob *job);
gboolean job_is_frozen(ThunarJob *job);
void job_processing_file(ThunarJob *job, GList *current_file,
                         guint n_processed);

// Ask ------------------------------------------------------------------------

ThunarJobResponse job_ask_create(ThunarJob *job, const gchar *format, ...);
ThunarJobResponse job_ask_delete(ThunarJob *job, const gchar *format, ...);
gboolean job_ask_no_size(ThunarJob *job, const gchar *format, ...);
ThunarJobResponse job_ask_overwrite(ThunarJob *job, const gchar *format, ...);
ThunarJobResponse job_ask_replace(ThunarJob *job, GFile *source_path,
                                  GFile *target_path, GError **error);
ThunarJobResponse job_ask_skip(ThunarJob *job, const gchar *format, ...);

gboolean job_files_ready(ThunarJob *job, GList *file_list);
void job_new_files(ThunarJob *job, const GList *file_list);
GList* job_ask_jobs(ThunarJob *job);

G_END_DECLS

#endif // __THUNAR_JOB_H__


