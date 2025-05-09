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

#ifndef __JOBUTILS_H__
#define __JOBUTILS_H__

#include "job.h"

G_BEGIN_DECLS

GFile* jobutil_next_duplicate_file(ThunarJob *job,
                                   GFile *file, gboolean copy,
                                   guint  n, GError **error)
                                   G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

GFile* jobutil_next_renamed_file(ThunarJob *job, GFile *src_file,
                                 GFile *tgt_file, guint n, GError **error)
                                 G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;

G_END_DECLS

#endif // __JOBUTILS_H__


