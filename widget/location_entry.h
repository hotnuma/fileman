/*-
 * Copyright(c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
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

#ifndef __LOCATION_ENTRY_H__
#define __LOCATION_ENTRY_H__

#include <location_bar.h>

G_BEGIN_DECLS

typedef struct _LocationEntryClass LocationEntryClass;
typedef struct _LocationEntry      LocationEntry;

#define TYPE_LOCATION_ENTRY                   \
    (location_entry_get_type())
#define LOCATION_ENTRY(obj)            \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_LOCATION_ENTRY, LocationEntry))
#define LOCATION_ENTRY_CLASS(klass)    \
    (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_LOCATION_ENTRY, LocationEntryClass))
#define IS_LOCATION_ENTRY(obj)         \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_LOCATION_ENTRY))
#define IS_LOCATION_ENTRY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_LOCATION_ENTRY))
#define LOCATION_ENTRY_GET_CLASS(obj)  \
    (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_LOCATION_ENTRY, LocationEntryClass))

GType location_entry_get_type() G_GNUC_CONST;

void location_entry_accept_focus(LocationEntry *entry,
                                        const gchar *initial_text);

G_END_DECLS

#endif /* !__LOCATION_ENTRY_H__ */


