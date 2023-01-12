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

#ifndef __SIDEPANE_H__
#define __SIDEPANE_H__

#include <component.h>

G_BEGIN_DECLS

typedef struct _SidePaneIface SidePaneIface;
typedef struct _SidePane      SidePane;

#define TYPE_SIDEPANE (sidepane_get_type())
#define SIDEPANE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj),     TYPE_SIDEPANE, SidePane))
#define IS_SIDEPANE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj),     TYPE_SIDEPANE))
#define SIDEPANE_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE ((obj),  TYPE_SIDEPANE, SidePaneIface))

struct _SidePaneIface
{
    GTypeInterface __parent__;

    /* virtual methods */
    gboolean (*get_show_hidden) (SidePane *side_pane);
    void     (*set_show_hidden) (SidePane *side_pane, gboolean show_hidden);
};

GType sidepane_get_type() G_GNUC_CONST;

gboolean    sidepane_get_show_hidden(SidePane *side_pane);
void        sidepane_set_show_hidden(SidePane *side_pane, gboolean show_hidden);

G_END_DECLS

#endif // __SIDEPANE_H__


