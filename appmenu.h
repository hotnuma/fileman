/*-
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#ifndef __APPMENU_H__
#define __APPMENU_H__

#include "etktype.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

// For window menu, some items are shown insensitive, instead of hidden
typedef enum
{
    MENU_TYPE_WINDOW,
    MENU_TYPE_CONTEXT_LOCATION_BUTTONS,
    MENU_TYPE_CONTEXT_TREE_VIEW,
    MENU_TYPE_CONTEXT_STANDARD_VIEW,
    N_MENU_TYPE,

} MenuType;

// Bundles of GtkMenuItems, which can be created by this widget
typedef enum
{
    MENU_SECTION_OPEN             = 1 << 0,
    MENU_SECTION_CREATE_NEW_FILES = 1 << 1,
    MENU_SECTION_CUT              = 1 << 2,
    MENU_SECTION_COPY_PASTE       = 1 << 3,
    MENU_SECTION_TRASH_DELETE     = 1 << 4,
    MENU_SECTION_RESTORE          = 1 << 5,
    MENU_SECTION_EMPTY_TRASH      = 1 << 6,
    MENU_SECTION_DUPLICATE        = 1 << 7,
    MENU_SECTION_MAKELINK         = 1 << 8,
    MENU_SECTION_RENAME           = 1 << 9,
    MENU_SECTION_TERMINAL         = 1 << 10,
    MENU_SECTION_EXTRACT          = 1 << 11,
    MENU_SECTION_MOUNTABLE        = 1 << 12,
    MENU_SECTION_PROPERTIES       = 1 << 13,

} MenuSections;


// AppMenu --------------------------------------------------------------------

typedef struct _AppMenu AppMenu;

#define TYPE_APPMENU (appmenu_get_type())
E_DECLARE_FINAL_TYPE(AppMenu, appmenu, APPMENU, GtkMenu)

GType appmenu_get_type() G_GNUC_CONST;

gboolean appmenu_add_sections(AppMenu *menu, MenuSections menu_sections);
void appmenu_hide_accel_labels(AppMenu *menu);

G_END_DECLS

#endif // __APPMENU_H__


