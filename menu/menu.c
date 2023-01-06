/*-
 * Copyright(c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#include <menu.h>

#include <thunar-gtk-extensions.h>
#include <launcher.h>
#include <window.h>

/*
 * ThunarMenu is a GtkMenu which provides a unified menu-creation
 * service for different thunar widgets.
 *
 * Based on the passed flags and selected sections, it fills itself
 * with the requested menu-items
 * by creating them with ThunarLauncher.
 */

// property identifiers

enum
{
    PROP_0,
    PROP_MENU_TYPE,
    PROP_LAUNCHER,
    PROP_FORCE_SECTION_OPEN,
    PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED,
};

static void menu_finalize(GObject *object);
static void menu_get_property(GObject *object, guint prop_id, GValue *value,
                              GParamSpec *pspec);
static void menu_set_property(GObject *object, guint prop_uid, const GValue *value,
                              GParamSpec *pspec);


// Allocation -----------------------------------------------------------------

struct _ThunarMenuClass
{
    GtkMenuClass __parent__;
};

struct _ThunarMenu
{
    GtkMenu __parent__;

    ThunarLauncher *launcher;

    // true, if the 'open' section should be forced
    gboolean    force_section_open;

    // true, if 'open' for folders, which would result
    // in changing the directory, should not be shown
    gboolean    change_directory_support_disabled;

    // detailed type of the thunar menu
    MenuType    type;
};

static GQuark _menu_handler_quark;

G_DEFINE_TYPE(ThunarMenu, menu, GTK_TYPE_MENU)

static void menu_class_init(ThunarMenuClass *klass)
{
    // determine the "thunar-menu-handler" quark
    _menu_handler_quark = g_quark_from_static_string("thunar-menu-handler");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);

    gobject_class->finalize = menu_finalize;
    gobject_class->get_property = menu_get_property;
    gobject_class->set_property = menu_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_MENU_TYPE,
                                    g_param_spec_int(
                                        "menu-type",
                                        "menu-type",
                                        "menu-type",
                                        // min, max, default
                                        0,
                                        N_MENU_TYPE - 1,
                                        0,
                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class,
                                    PROP_LAUNCHER,
                                    g_param_spec_object(
                                        "launcher",
                                        "launcher",
                                        "launcher",
                                        THUNAR_TYPE_LAUNCHER,
                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class,
                                    PROP_FORCE_SECTION_OPEN,
                                    g_param_spec_boolean(
                                        "force-section-open",
                                        "force-section-open",
                                        "force-section-open",
                                        FALSE,
                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class,
                                    PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED,
                                    g_param_spec_boolean(
                                        "change_directory-support-disabled",
                                        "change_directory-support-disabled",
                                        "change_directory-support-disabled",
                                        FALSE,
                                        G_PARAM_WRITABLE | G_PARAM_CONSTRUCT_ONLY));
}

static void menu_init(ThunarMenu *menu)
{
    menu->force_section_open = FALSE;
    menu->type = FALSE;
    menu->change_directory_support_disabled = FALSE;
}

static void menu_finalize(GObject *object)
{
    ThunarMenu *menu = THUNAR_MENU(object);

    g_object_unref(menu->launcher);

    G_OBJECT_CLASS(menu_parent_class)->finalize(object);
}


// Properties -----------------------------------------------------------------

static void menu_get_property(GObject *object, guint prop_id, GValue *value,
                              GParamSpec *pspec)
{
    UNUSED(value);

    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void menu_set_property(GObject *object, guint prop_id, const GValue *value,
                              GParamSpec *pspec)
{
    ThunarMenu *menu = THUNAR_MENU(object);

    switch (prop_id)
    {
    case PROP_MENU_TYPE:
        menu->type = g_value_get_int(value);
        break;

    case PROP_LAUNCHER:
        menu->launcher = g_value_dup_object(value);
        g_object_ref(G_OBJECT(menu->launcher));
        break;

    case PROP_FORCE_SECTION_OPEN:
        menu->force_section_open = g_value_get_boolean(value);
        break;

    case PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED:
        menu->change_directory_support_disabled = g_value_get_boolean(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


// Public ---------------------------------------------------------------------

gboolean menu_add_sections(ThunarMenu *menu, MenuSections menu_sections)
{
    thunar_return_val_if_fail(THUNAR_IS_MENU(menu), FALSE);

    gboolean force = (menu->type == MENU_TYPE_WINDOW
                      || menu->type == MENU_TYPE_CONTEXT_TREE_VIEW);


    if (menu_sections & MENU_SECTION_OPEN)
    {
        if (launcher_append_open_section(menu->launcher,
                                         GTK_MENU_SHELL(menu),
                                         FALSE,
                                         !menu->change_directory_support_disabled,
                                         menu->force_section_open))
        {
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
        }
    }

    gboolean item_added = FALSE;

    if (menu_sections & MENU_SECTION_CREATE_NEW_FILES)
    {
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_CREATE_FOLDER,
                                                 force) != NULL);

        // No document creation for tree-view
        if (menu->type != MENU_TYPE_CONTEXT_TREE_VIEW)
            item_added |= (launcher_append_menu_item(menu->launcher,
                                                     GTK_MENU_SHELL(menu),
                                                     LAUNCHER_ACTION_CREATE_DOCUMENT,
                                                     force) != NULL);
        if (item_added)
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
    }

    item_added = FALSE;

    if (menu_sections & MENU_SECTION_CUT)
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_CUT,
                                                 force) != NULL);

    if (menu_sections & MENU_SECTION_COPY_PASTE)
    {
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_COPY,
                                                 force) != NULL);

        if (menu->type == MENU_TYPE_CONTEXT_LOCATION_BUTTONS)
            item_added |= (launcher_append_menu_item(menu->launcher,
                                                     GTK_MENU_SHELL(menu),
                                                     LAUNCHER_ACTION_PASTE_INTO_FOLDER,
                                                     force) != NULL);
        else
            item_added |= (launcher_append_menu_item(menu->launcher,
                                                     GTK_MENU_SHELL(menu),
                                                     LAUNCHER_ACTION_PASTE,
                                                     force) != NULL);
    }

    if (item_added)
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));

    if (menu_sections & MENU_SECTION_TRASH_DELETE)
    {
        item_added = FALSE;

        item_added |=(launcher_append_menu_item(menu->launcher,
                                                GTK_MENU_SHELL(menu),
                                                LAUNCHER_ACTION_MOVE_TO_TRASH,
                                                force) != NULL);
        item_added |=(launcher_append_menu_item(menu->launcher,
                                                GTK_MENU_SHELL(menu),
                                                LAUNCHER_ACTION_DELETE,
                                                force) != NULL);
        if (item_added)
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
    }

    if (menu_sections & MENU_SECTION_EMPTY_TRASH)
    {
        if (launcher_append_menu_item(menu->launcher,
                                      GTK_MENU_SHELL(menu),
                                      LAUNCHER_ACTION_EMPTY_TRASH,
                                      FALSE) != NULL)
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
    }

    if (menu_sections & MENU_SECTION_RESTORE)
    {
        if (launcher_append_menu_item(menu->launcher,
                                      GTK_MENU_SHELL(menu),
                                      LAUNCHER_ACTION_RESTORE,
                                      FALSE) != NULL)
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
    }

    item_added = FALSE;

    if (menu_sections & MENU_SECTION_DUPLICATE)
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_DUPLICATE,
                                                 force) != NULL);

    if (menu_sections & MENU_SECTION_MAKELINK)
        item_added |=(launcher_append_menu_item(menu->launcher,
                                                GTK_MENU_SHELL(menu),
                                                LAUNCHER_ACTION_MAKE_LINK,
                                                force) != NULL);

    if (menu_sections & MENU_SECTION_RENAME)
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_RENAME,
                                                 force) != NULL);

    if (item_added)
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));

    // custom actions ---------------------------------------------------------

    item_added = FALSE;

    if (menu_sections & MENU_SECTION_TERMINAL)
    {
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_TERMINAL,
                                                 force) != NULL);
    }

    if (menu_sections & MENU_SECTION_EXTRACT)
    {
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_EXTRACT,
                                                 force) != NULL);
    }

    if (item_added)
        xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));

    // mountable --------------------------------------------------------------

    item_added = FALSE;

    if (menu_sections & MENU_SECTION_MOUNTABLE)
    {
        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_MOUNT,
                                                 FALSE) != NULL);

        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_UNMOUNT,
                                                 FALSE) != NULL);

        item_added |= (launcher_append_menu_item(menu->launcher,
                                                 GTK_MENU_SHELL(menu),
                                                 LAUNCHER_ACTION_EJECT,
                                                 FALSE) != NULL);

        if (item_added)
            xfce_gtk_menu_append_seperator(GTK_MENU_SHELL(menu));
    }

    // properties -------------------------------------------------------------

    if (menu_sections & MENU_SECTION_PROPERTIES)
    {
        launcher_append_menu_item(menu->launcher,
                                  GTK_MENU_SHELL(menu),
                                  LAUNCHER_ACTION_PROPERTIES,
                                  FALSE);
    }

    return TRUE;
}

void menu_hide_accel_labels(ThunarMenu *menu)
{
    thunar_return_if_fail(THUNAR_IS_MENU(menu));

    GList *children = gtk_container_get_children(GTK_CONTAINER(menu));

    for (GList *lp = children; lp != NULL; lp = lp->next)
        xfce_gtk_menu_item_set_accel_label(lp->data, NULL);

    g_list_free(children);
}


