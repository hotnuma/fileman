/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_WINDOW_H__
#define __THUNAR_WINDOW_H__

#include <thunar-enum-types.h>
#include <thunar-folder.h>
#include <thunar-launcher.h>

#include <libxfce4ui/libxfce4ui.h>

G_BEGIN_DECLS;

typedef struct _ThunarWindowClass ThunarWindowClass;
typedef struct _ThunarWindow      ThunarWindow;

#define THUNAR_TYPE_WINDOW            (thunar_window_get_type ())
#define THUNAR_WINDOW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_WINDOW, ThunarWindow))
#define THUNAR_WINDOW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_WINDOW, ThunarWindowClass))
#define THUNAR_IS_WINDOW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_WINDOW))
#define THUNAR_IS_WINDOW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_WINDOW))
#define THUNAR_WINDOW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_WINDOW, ThunarWindowClass))

/* #XfceGtkActionEntrys provided by this widget */
typedef enum
{
  THUNAR_WINDOW_ACTION_BACK,
  THUNAR_WINDOW_ACTION_FORWARD,
  THUNAR_WINDOW_ACTION_OPEN_PARENT,
  THUNAR_WINDOW_ACTION_OPEN_HOME,

  THUNAR_WINDOW_ACTION_RELOAD_ALT,
  THUNAR_WINDOW_ACTION_SHOW_HIDDEN,
  THUNAR_WINDOW_ACTION_BACK_ALT,

  THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_1,
  THUNAR_WINDOW_ACTION_ZOOM_IN_ALT_2,
  THUNAR_WINDOW_ACTION_ZOOM_IN,
  THUNAR_WINDOW_ACTION_ZOOM_OUT,
  THUNAR_WINDOW_ACTION_ZOOM_RESET,
  THUNAR_WINDOW_ACTION_ZOOM_OUT_ALT,
  THUNAR_WINDOW_ACTION_ZOOM_RESET_ALT,
} ThunarWindowAction;

GType                     thunar_window_get_type                            (void) G_GNUC_CONST;
ThunarFile               *thunar_window_get_current_directory               (ThunarWindow        *window);
void                      thunar_window_set_current_directory               (ThunarWindow        *window,
                                                                             ThunarFile          *current_directory);
void                      thunar_window_scroll_to_file                      (ThunarWindow        *window,
                                                                             ThunarFile          *file,
                                                                             gboolean             select,
                                                                             gboolean             use_align,
                                                                             gfloat               row_align,
                                                                             gfloat               col_align);
gchar                   **thunar_window_get_directories                     (ThunarWindow        *window,
                                                                             gint                *active_page);
gboolean                  thunar_window_set_directories                     (ThunarWindow        *window,
                                                                             gchar              **uris,
                                                                             gint                 active_page);
void                      thunar_window_update_directories                  (ThunarWindow        *window,
                                                                             ThunarFile          *old_directory,
                                                                             ThunarFile          *new_directory);
void                      thunar_window_notebook_open_new_tab               (ThunarWindow        *window,
                                                                             ThunarFile          *directory);
gboolean                  thunar_window_has_shortcut_sidepane               (ThunarWindow        *window);
GtkWidget*                thunar_window_get_sidepane                        (ThunarWindow        *window);
void                      thunar_window_append_menu_item                    (ThunarWindow        *window,
                                                                             GtkMenuShell        *menu,
                                                                             ThunarWindowAction   action);
ThunarLauncher*           thunar_window_get_launcher                        (ThunarWindow        *window);
void                      thunar_window_redirect_menu_tooltips_to_statusbar (ThunarWindow        *window,
                                                                             GtkMenu             *menu);
const XfceGtkActionEntry* thunar_window_get_action_entry                    (ThunarWindow        *window,
                                                                             ThunarWindowAction   action);
G_END_DECLS;

#endif /* !__THUNAR_WINDOW_H__ */
