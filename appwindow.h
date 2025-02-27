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
    WINDOW_ACTION_KEY_SHOW_HIDDEN,
    WINDOW_ACTION_DEBUG,

} WindowAction;

// AppWindow ------------------------------------------------------------------

typedef struct _AppWindow AppWindow;
typedef struct _AppWindowClass AppWindowClass;

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

ThunarLauncher* window_get_launcher(AppWindow *window);
void window_set_current_directory(AppWindow *window, ThunarFile *current_directory);

void window_redirect_tooltips(AppWindow *window, GtkMenu *menu);

G_END_DECLS

#endif // __APPWINDOW_H__


