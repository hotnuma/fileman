/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNARCOMPONENT_H__
#define __THUNARCOMPONENT_H__

#include <gio/gio.h>

G_BEGIN_DECLS

// ThunarComponent ------------------------------------------------------------

typedef struct _ThunarComponentIface ThunarComponentIface;
typedef struct _ThunarComponent      ThunarComponent;

#define TYPE_THUNARCOMPONENT (component_get_type())
#define THUNARCOMPONENT(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),      TYPE_THUNARCOMPONENT, ThunarComponent))
#define IS_THUNARCOMPONENT(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),      TYPE_THUNARCOMPONENT))
#define THUNARCOMPONENT_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj),   TYPE_THUNARCOMPONENT, ThunarComponentIface))

struct _ThunarComponentIface
{
    GTypeInterface __parent__;

    // methods
    GList* (*get_selected_files) (ThunarComponent *component);
    void (*set_selected_files) (ThunarComponent *component, GList *selected_files);
};

GType component_get_type() G_GNUC_CONST;

GList* component_get_selected_files(ThunarComponent *component);
void component_set_selected_files(ThunarComponent *component,
                                  GList *selected_files);
void component_restore_selection(ThunarComponent *component);

G_END_DECLS

#endif // __THUNARCOMPONENT_H__


