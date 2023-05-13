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

#ifndef __APPWINDOW_H__
#define __APPWINDOW_H__

#include <launcher.h>
#include <th_file.h>
#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS

typedef struct _AppWindowClass AppWindowClass;
typedef struct _AppWindow      AppWindow;

#define TYPE_APPWINDOW (window_get_type())
#define APPWINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_APPWINDOW, AppWindow))
#define APPWINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_APPWINDOW, AppWindowClass))
#define IS_APPWINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_APPWINDOW))
#define IS_APPWINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_APPWINDOW))
#define APPWINDOW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_APPWINDOW, AppWindowClass))

GType window_get_type() G_GNUC_CONST;

typedef enum
{
    WINDOW_ACTION_BACK,
    WINDOW_ACTION_KEY_BACK,
    WINDOW_ACTION_FORWARD,
    WINDOW_ACTION_OPEN_PARENT,
    WINDOW_ACTION_OPEN_HOME,
    WINDOW_ACTION_KEY_RELOAD,
    WINDOW_ACTION_KEY_RENAME,
    WINDOW_ACTION_KEY_TRASH,
    WINDOW_ACTION_SHOW_HIDDEN,
    WINDOW_ACTION_DEBUG,

} WindowAction;

ThunarFile* window_get_current_directory(AppWindow *window);
void window_set_current_directory(AppWindow *window, ThunarFile *current_directory);

void window_scroll_to_file(AppWindow *window, ThunarFile *file, gboolean select,
                           gboolean use_align, gfloat row_align, gfloat col_align);

gchar** window_get_directories(AppWindow *window, gint *active_page);
gboolean window_set_directories(AppWindow *window, gchar **uris, gint active_page);

void window_update_directories(AppWindow *window, ThunarFile *old_directory,
                               ThunarFile *new_directory);

gboolean window_has_shortcut_sidepane(AppWindow *window);
GtkWidget* window_get_sidepane(AppWindow *window);
ThunarLauncher* window_get_launcher(AppWindow *window);

void window_redirect_tooltips(AppWindow *window, GtkMenu *menu);

const XfceGtkActionEntry* window_get_action_entry(AppWindow *window,
                                                  WindowAction action);

GtkWidget* window_get_focused_tree_view(AppWindow *window);

G_END_DECLS

#endif // __APPWINDOW_H__


