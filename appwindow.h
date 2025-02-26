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

#define APP_TYPE_WINDOW (window_get_type())

#if 0
G_DECLARE_FINAL_TYPE(AppWindow, window, APP, WINDOW, GtkWindow)
#else
typedef struct _AppWindowClass AppWindowClass;
#define APP_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  APP_TYPE_WINDOW, AppWindow))
#define APP_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   APP_TYPE_WINDOW, AppWindowClass))
#define APP_IS_WINDOW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  APP_TYPE_WINDOW))
#define APP_IS_WINDOW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   APP_TYPE_WINDOW))
#define APP_WINDOW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   APP_TYPE_WINDOW, AppWindowClass))
#endif

GType window_get_type() G_GNUC_CONST;

ThunarLauncher* window_get_launcher(AppWindow *window);
void window_set_current_directory(AppWindow *window, ThunarFile *current_directory);

void window_redirect_tooltips(AppWindow *window, GtkMenu *menu);

G_END_DECLS

#endif // __APPWINDOW_H__


