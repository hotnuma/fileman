/*-
 * Copyright(c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2010 Benedikt Meurer <benny@xfce.org>
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

#ifndef __PATHENTRY_H__
#define __PATHENTRY_H__

#include <th-file.h>

G_BEGIN_DECLS

typedef struct _PathEntryClass PathEntryClass;
typedef struct _PathEntry      PathEntry;

#define TYPE_PATHENTRY (pathentry_get_type())
#define PATHENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_PATHENTRY, PathEntry))
#define PATHENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_PATHENTRY, PathEntryClass))
#define IS_PATHENTRY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_PATHENTRY))
#define IS_PATHENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_PATHENTRY))
#define PATHENTRY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_PATHENTRY, PathEntryClass))

GType pathentry_get_type() G_GNUC_CONST;

GtkWidget* thunar_path_entry_new();

ThunarFile* thunar_path_entry_get_current_file(PathEntry *path_entry);
void thunar_path_entry_set_current_file(PathEntry *path_entry,
                                        ThunarFile *current_file);
void thunar_path_entry_set_working_directory(PathEntry *path_entry,
                                             ThunarFile      *directory);

G_END_DECLS

#endif // __PATHENTRY_H__


