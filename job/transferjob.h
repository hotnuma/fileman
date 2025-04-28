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

#ifndef __TRANSFERJOB_H__
#define __TRANSFERJOB_H__

#include "job.h"

G_BEGIN_DECLS

typedef enum
{
    TRANSFERJOB_COPY,
    TRANSFERJOB_LINK,
    TRANSFERJOB_MOVE,
    TRANSFERJOB_TRASH,

} TransferJobType;

// TransferJob ----------------------------------------------------------------

typedef struct _TransferJob        TransferJob;
typedef struct _TransferJobClass   TransferJobClass;
typedef struct _TransferJobPrivate TransferJobPrivate;

#define TYPE_TRANSFERJOB (transferjob_get_type())

#define TRANSFERJOB(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_TRANSFERJOB, TransferJob))
#define TRANSFERJOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_TRANSFERJOB, TransferJobClass))
#define IS_TRANSFERJOB(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_TRANSFERJOB))
#define IS_TRANSFERJOB_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_TRANSFERJOB)
#define TRANSFERJOB_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),   TYPE_TRANSFERJOB, TransferJobClass))

GType transferjob_get_type() G_GNUC_CONST;

ThunarJob* transferjob_new(GList *source_file_list, GList *target_file_list,
                           TransferJobType type)
                           G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

gchar* transferjob_get_status(TransferJob *job);

G_END_DECLS

#endif // __TRANSFERJOB_H__


