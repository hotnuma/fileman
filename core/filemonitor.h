/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
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

#ifndef __FILEMONITOR_H__
#define __FILEMONITOR_H__

#include <th_file.h>

G_BEGIN_DECLS

// FileMonitor ----------------------------------------------------------------

typedef struct _FileMonitorClass FileMonitorClass;
typedef struct _FileMonitor      FileMonitor;

#define TYPE_FILEMONITOR (filemon_get_type())
#define FILEMONITOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_FILEMONITOR, FileMonitor))
#define FILEMONITOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_FILEMONITOR, FileMonitorClass))
#define IS_FILEMONITOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_FILEMONITOR))
#define IS_FILEMONITOR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_FILEMONITOR))
#define FILEMONITOR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_FILEMONITOR, FileMonitorClass))

GType filemon_get_type() G_GNUC_CONST;

FileMonitor *filemon_get_default();

void filemon_file_changed(ThunarFile *file);
void filemon_file_destroyed(ThunarFile *file);

G_END_DECLS

#endif // __FILEMONITOR_H__


