/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_LAUNCHER_H__
#define __THUNAR_LAUNCHER_H__

#include "etktype.h"
#include <gtk/gtk.h>

G_BEGIN_DECLS

// Action ---------------------------------------------------------------------

typedef enum
{
    LAUNCHER_ACTION_OPEN,
    LAUNCHER_ACTION_OPEN_IN_WINDOW,
    LAUNCHER_ACTION_OPEN_WITH_OTHER,

    LAUNCHER_ACTION_EXECUTE,

    LAUNCHER_ACTION_CREATE_FOLDER,
    LAUNCHER_ACTION_CREATE_DOCUMENT,

    LAUNCHER_ACTION_CUT,
    LAUNCHER_ACTION_COPY,
    LAUNCHER_ACTION_PASTE_INTO_FOLDER,
    LAUNCHER_ACTION_PASTE,

    LAUNCHER_ACTION_MOVE_TO_TRASH,
    LAUNCHER_ACTION_DELETE,
    LAUNCHER_ACTION_EMPTY_TRASH,
    LAUNCHER_ACTION_RESTORE,

    LAUNCHER_ACTION_DUPLICATE,
    LAUNCHER_ACTION_MAKE_LINK,
    LAUNCHER_ACTION_RENAME,

    LAUNCHER_ACTION_TERMINAL,
    LAUNCHER_ACTION_EXTRACT,

    LAUNCHER_ACTION_MOUNT,
    LAUNCHER_ACTION_UNMOUNT,
    LAUNCHER_ACTION_EJECT,

    LAUNCHER_ACTION_PROPERTIES,

} LauncherAction;

typedef enum
{
    LAUNCHER_CHANGE_DIRECTORY,
    LAUNCHER_OPEN_AS_NEW_WINDOW,
    LAUNCHER_NO_ACTION,

} FolderOpenAction;

// Launcher -------------------------------------------------------------------

typedef struct _ThunarLauncher ThunarLauncher;

#define TYPE_LAUNCHER (launcher_get_type())
E_DECLARE_FINAL_TYPE(ThunarLauncher, launcher, LAUNCHER, GObject)

GType launcher_get_type() G_GNUC_CONST;

// Properties -----------------------------------------------------------------

GtkWidget* launcher_get_widget(ThunarLauncher *launcher);
void launcher_set_widget(ThunarLauncher *launcher, GtkWidget *widget);

// Build Menu -----------------------------------------------------------------

void launcher_append_accelerators(ThunarLauncher *launcher,
                                  GtkAccelGroup *accel_group);
GtkWidget* launcher_append_menu_item(ThunarLauncher *launcher,
                                     GtkMenuShell *menu,
                                     LauncherAction action,
                                     gboolean force);
gboolean launcher_append_open_section(ThunarLauncher *launcher,
                                      GtkMenuShell *menu,
                                      gboolean support_tabs,
                                      gboolean support_change_directory,
                                      gboolean force);

void launcher_activate_selected_files(ThunarLauncher *launcher,
                                      FolderOpenAction action,
                                      GAppInfo *app_info);
void launcher_open_selected_folders(ThunarLauncher *launcher);

void launcher_action_mount(ThunarLauncher *launcher);
void launcher_action_unmount(ThunarLauncher *launcher);
void launcher_action_eject(ThunarLauncher *launcher);
void launcher_action_rename(ThunarLauncher *launcher);
void launcher_action_trash_delete(ThunarLauncher *launcher);

void execute_copy_into(gpointer parent,
                           GList *source_file_list, GFile *target_file,
                           GClosure *new_files_closure);
void execute_link_into(gpointer parent,
                           GList *source_file_list, GFile *target_file,
                           GClosure *new_files_closure);
void execute_move_into(gpointer parent,
                           GList *source_file_list, GFile *target_file,
                           GClosure *new_files_closure);
void execute_unlink_files(gpointer parent,
                              GList *file_list, gboolean permanently);
void execute_trash(gpointer parent,
                       GList *file_list);

G_END_DECLS

#endif // __THUNAR_LAUNCHER_H__


