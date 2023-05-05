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

#ifndef __LOCATIONBAR_H__
#define __LOCATIONBAR_H__

#include <component.h>

G_BEGIN_DECLS;

typedef struct _LocationBarClass LocationBarClass;
typedef struct _LocationBar      LocationBar;

#define TYPE_LOCATIONBAR (locbar_get_type ())
#define LOCATIONBAR(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_LOCATIONBAR, LocationBar))
#define IS_LOCATIONBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_LOCATIONBAR))
#define LOCATIONBAR_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS ((obj), TYPE_LOCATIONBAR, LocationBarClass))


GType locbar_get_type() G_GNUC_CONST;

GtkWidget* locbar_new();

void locbar_request_entry(LocationBar *bar, const gchar *initial_text);

G_END_DECLS;

#endif /* !__LOCATIONBAR_H__ */


