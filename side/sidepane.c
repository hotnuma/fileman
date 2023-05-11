/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <config.h>
#include <sidepane.h>

#include <component.h>

static void sidepane_class_init(gpointer klass);

GType sidepane_get_type()
{
    static volatile gsize type__volatile = 0;

    if (g_once_init_enter((gsize*) &type__volatile))
    {
        GType type = g_type_register_static_simple(
            G_TYPE_INTERFACE,
            I_("SidePane"),
            sizeof(SidePaneIface),
            (GClassInitFunc)(void(*)(void)) sidepane_class_init,
            0,
            NULL,
            0);

        g_type_interface_add_prerequisite(type, GTK_TYPE_WIDGET);
        g_type_interface_add_prerequisite(type, THUNAR_TYPE_COMPONENT);

        g_once_init_leave(&type__volatile, type);
    }

    return type__volatile;
}

static void sidepane_class_init(gpointer klass)
{
    g_object_interface_install_property(klass,
                                        g_param_spec_boolean(
                                            "show-hidden",
                                            "show-hidden",
                                            "show-hidden",
                                            FALSE,
                                            E_PARAM_READWRITE));
}

gboolean sidepane_get_show_hidden(SidePane *side_pane)
{
    e_return_val_if_fail(IS_SIDEPANE(side_pane), FALSE);

    return SIDEPANE_GET_IFACE(side_pane)->get_show_hidden(side_pane);
}

void sidepane_set_show_hidden(SidePane *side_pane, gboolean show_hidden)
{
    e_return_if_fail(IS_SIDEPANE(side_pane));

    SIDEPANE_GET_IFACE(side_pane)->set_show_hidden(side_pane, show_hidden);
}


