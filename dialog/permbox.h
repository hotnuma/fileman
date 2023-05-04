/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __PERMISSIONBOX_H__
#define __PERMISSIONBOX_H__

#include <th_file.h>

G_BEGIN_DECLS

typedef struct _PermissionBoxClass PermissionBoxClass;
typedef struct _PermissionBox      PermissionBox;

#define TYPE_PERMISSIONBOX (permbox_get_type ())
#define PERMISSIONBOX(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_PERMISSIONBOX, PermissionBox))
#define PERMISSIONBOX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_PERMISSIONBOX, PermissionBoxClass))
#define IS_PERMISSIONBOX(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_PERMISSIONBOX))
#define IS_PERMISSIONBOX_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_PERMISSIONBOX))
#define PERMISSIONBOX_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_PERMISSIONBOX, PermissionBoxClass))

GType       permbox_get_type() G_GNUC_CONST;

GtkWidget*  permbox_new() G_GNUC_MALLOC;

G_END_DECLS

#endif // __PERMISSIONBOX_H__


