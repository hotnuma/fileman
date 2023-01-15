/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at
 * your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston,
 * MA  02111-1307  USA
 */

#ifndef __DEEPCOUNT_JOB_H__
#define __DEEPCOUNT_JOB_H__

#include <job.h>
#include <gio/gio.h>

G_BEGIN_DECLS

typedef struct _DeepCountJobPrivate DeepCountJobPrivate;
typedef struct _DeepCountJobClass   DeepCountJobClass;
typedef struct _DeepCountJob        DeepCountJob;

#define TYPE_DEEPCOUNT_JOB (dcjob_get_type())
#define DEEPCOUNT_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_DEEPCOUNT_JOB, DeepCountJob))
#define DEEPCOUNT_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_DEEPCOUNT_JOB, DeepCountJobClass))
#define IS_DEEPCOUNT_JOB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_DEEPCOUNT_JOB))
#define IS_DEEPCOUNT_JOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_DEEPCOUNT_JOB)
#define DEEPCOUNT_JOB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_DEEPCOUNT_JOB, DeepCountJobClass))

GType dcjob_get_type() G_GNUC_CONST;

DeepCountJob* dcjob_new(GList *files, GFileQueryInfoFlags flags)
                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif // __DEEPCOUNT_JOB_H__


