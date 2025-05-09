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

#include "config.h"
#include "component.h"

#include "navigator.h"
#include "gio-ext.h"

/*
 * ThunarComponent:selected-files:
 *
 * The list of currently selected files for the #AppWindow to
 * which this ThunarComponent belongs.
 *
 * The exact semantics of this property depend on the implementor
 * of this interface. For example, ThunarComponent will update
 * the property depending on the users selection with the
 * #GtkTreeComponent or #ExoIconComponent. While other components in a
 * window, like the #ThunarShortcutsPane, will not update this property on
 * their own, but rely on #AppWindow to synchronize the selected
 * files list with the selected files list from the active #ThunarComponent.
 *
 * This way all components can behave properly depending on the
 * set of selected files even though they don't have direct access
 * to the #ThunarComponent.
 */

static void component_class_init(gpointer klass);

// ThunarComponent ------------------------------------------------------------

GType component_get_type()
{
    static volatile gsize type__volatile = 0;
    GType type;

    if (g_once_init_enter((gsize*) &type__volatile))
    {
        type = g_type_register_static_simple(
                    G_TYPE_INTERFACE,
                    I_("ThunarComponent"),
                    sizeof(ThunarComponentIface),
                    (GClassInitFunc) (void(*)(void)) component_class_init,
                    0,
                    NULL,
                    0);

        g_type_interface_add_prerequisite(type, TYPE_THUNARNAVIGATOR);

        g_once_init_leave(&type__volatile, type);
    }

    return type__volatile;
}

static void component_class_init(gpointer klass)
{
    g_object_interface_install_property(klass,
                                        g_param_spec_boxed(
                                            "selected-files",
                                            "selected-files",
                                            "selected-files",
                                            TYPE_FILEINFOLIST,
                                            E_PARAM_READWRITE));
}

// Public ---------------------------------------------------------------------

GList* component_get_selected_files(ThunarComponent *component)
{
    e_return_val_if_fail(IS_THUNARCOMPONENT(component), NULL);

    ThunarComponentIface *iface = THUNARCOMPONENT_GET_IFACE(component);

    if (iface->get_selected_files != NULL)
        return iface->get_selected_files(component);
    else
        return NULL;
}

void component_set_selected_files(ThunarComponent *component,
                                  GList *selected_files)
{
    e_return_if_fail(IS_THUNARCOMPONENT(component));

    ThunarComponentIface *iface = THUNARCOMPONENT_GET_IFACE(component);

    if (iface->set_selected_files != NULL)
        iface->set_selected_files(component, selected_files);
}

/* Make sure that the @selected_files stay selected when a @component
 * updates. This may be necessary on row changes etc. */

void component_restore_selection(ThunarComponent *component)
{
    e_return_if_fail(IS_THUNARCOMPONENT(component));

    GList *selected_files;
    selected_files = e_list_copy(component_get_selected_files(component));
    component_set_selected_files(component, selected_files);
    e_list_free(selected_files);
}


