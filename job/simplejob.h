/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
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

#ifndef __SIMPLEJOB_H__
#define __SIMPLEJOB_H__

#include "job.h"

G_BEGIN_DECLS

/**
 * SimpleJobFunc:
 * @job            : a #ThunarJob.
 * @param_values   : a #GArray of the #GValue<!---->s passed to
 *                   simplejob_launch().
 * @error          : return location for errors.
 *
 * Used by the #SimpleJob to process the @job. See simplejob_launch()
 * for further details.
 *
 * Return value: %TRUE on success, %FALSE in case of an error.
 **/
typedef gboolean (*SimpleJobFunc) (ThunarJob *job, GArray *param_values,
                                   GError **error);

// SimpleJob ------------------------------------------------------------------

typedef struct _SimpleJobClass SimpleJobClass;
typedef struct _SimpleJob      SimpleJob;

#define TYPE_SIMPLEJOB (simplejob_get_type())
#define THUNAR_SIMPLE_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_SIMPLEJOB, SimpleJob))
#define THUNAR_SIMPLE_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_SIMPLEJOB, SimpleJobClass))
#define THUNAR_IS_SIMPLE_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_SIMPLEJOB))
#define THUNAR_IS_SIMPLE_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_SIMPLEJOB))
#define THUNAR_SIMPLE_JOB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_SIMPLEJOB, SimpleJobClass))

GType simplejob_get_type() G_GNUC_CONST;

ThunarJob* simplejob_launch(SimpleJobFunc func, guint n_param_values, ...)
                            G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
GArray* simplejob_get_param_values(SimpleJob *job);

G_END_DECLS

#endif // __SIMPLEJOB_H__


