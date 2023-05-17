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

#ifndef __DEEPCOUNTJOB_H__
#define __DEEPCOUNTJOB_H__

#include <gio/gio.h>

G_BEGIN_DECLS

// DeepCountJob ---------------------------------------------------------------

typedef struct _DeepCountJobPrivate DeepCountJobPrivate;
typedef struct _DeepCountJobClass   DeepCountJobClass;
typedef struct _DeepCountJob        DeepCountJob;

#define TYPE_DEEPCOUNTJOB (dcjob_get_type())
#define DEEPCOUNTJOB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_DEEPCOUNTJOB, DeepCountJob))
#define DEEPCOUNTJOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_DEEPCOUNTJOB, DeepCountJobClass))
#define IS_DEEPCOUNTJOB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_DEEPCOUNTJOB))
#define IS_DEEPCOUNTJOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_DEEPCOUNTJOB)
#define DEEPCOUNTJOB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_DEEPCOUNTJOB, DeepCountJobClass))

GType dcjob_get_type() G_GNUC_CONST;

DeepCountJob* dcjob_new(GList *files, GFileQueryInfoFlags flags)
                        G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif // __DEEPCOUNTJOB_H__


