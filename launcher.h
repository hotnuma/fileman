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

#include <gtk/gtk.h>

G_BEGIN_DECLS

typedef struct _ThunarLauncherClass ThunarLauncherClass;
typedef struct _ThunarLauncher      ThunarLauncher;

#define THUNAR_TYPE_LAUNCHER (launcher_get_type())
#define THUNAR_LAUNCHER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), THUNAR_TYPE_LAUNCHER, ThunarLauncher))
#define THUNAR_LAUNCHER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), THUNAR_TYPE_LAUNCHER, ThunarLauncherClass))
#define THUNAR_IS_LAUNCHER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), THUNAR_TYPE_LAUNCHER))
#define THUNAR_IS_LAUNCHER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), THUNAR_TYPE_LAUNCHER))
#define THUNAR_LAUNCHER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), THUNAR_TYPE_LAUNCHER, ThunarLauncherClass))

//  Action Entrys provided by this widget

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

    //LAUNCHER_ACTION_TRASH_DELETE,
    LAUNCHER_ACTION_MOVE_TO_TRASH,
    LAUNCHER_ACTION_DELETE,
    LAUNCHER_ACTION_EMPTY_TRASH,
    LAUNCHER_ACTION_RESTORE,

    LAUNCHER_ACTION_DUPLICATE,
    LAUNCHER_ACTION_MAKE_LINK,
    LAUNCHER_ACTION_RENAME,
    //LAUNCHER_ACTION_KEY_RENAME,

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

GType       launcher_get_type() G_GNUC_CONST;

void        launcher_activate_selected_files(ThunarLauncher *launcher,
                                             FolderOpenAction action,
                                             GAppInfo *app_info);
void        launcher_open_selected_folders(ThunarLauncher *launcher);

void        launcher_set_widget(ThunarLauncher *launcher,
                                GtkWidget *widget);
GtkWidget*  launcher_get_widget(ThunarLauncher *launcher);

void        launcher_append_accelerators(ThunarLauncher *launcher,
                                         GtkAccelGroup *accel_group);
GtkWidget*  launcher_append_menu_item(ThunarLauncher *launcher,
                                      GtkMenuShell *menu,
                                      LauncherAction action,
                                      gboolean force);
gboolean    launcher_append_open_section(ThunarLauncher *launcher,
                                         GtkMenuShell *menu,
                                         gboolean support_tabs,
                                         gboolean support_change_directory,
                                         gboolean force);

void launcher_action_mount(ThunarLauncher *launcher);
void launcher_action_unmount(ThunarLauncher *launcher);
void launcher_action_eject(ThunarLauncher *launcher);
void launcher_action_rename(ThunarLauncher *launcher);
void launcher_action_trash_delete(ThunarLauncher *launcher);

G_END_DECLS

#endif // __THUNAR_LAUNCHER_H__


