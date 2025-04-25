/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005      Jeff Franks <jcfranks@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef __APPLICATION_H__
#define __APPLICATION_H__

#include "etktype.h"
#include "appwindow.h"
#include "job.h"

G_BEGIN_DECLS

// application ----------------------------------------------------------------

typedef struct _Application Application;

#define TYPE_APPLICATION (application_get_type())
E_DECLARE_FINAL_TYPE(Application, application, APPLICATION, GtkApplication)

GType application_get_type() G_GNUC_CONST;

// properties -----------------------------------------------------------------

//gboolean application_get_daemon(Application *application);
//void application_set_daemon(Application *application, gboolean daemon);

// public ---------------------------------------------------------------------

Application* application_get();
void application_take_window(Application *application, GtkWindow *window);
GtkWidget* application_open_window(Application *application,
                                   ThunarFile *directory,
                                   GdkScreen *screen, const gchar *startup_id);

// actions --------------------------------------------------------------------

typedef ThunarJob* (*LauncherFunc) (GList *source_path_list,
                                    GList *target_path_list);

void application_launch(Application *application,
                        gpointer parent,
                        const gchar *icon_name,
                        const gchar *title,
                        LauncherFunc launcher,
                        GList *source_path_list,
                        GList *target_path_list,
                        gboolean update_source_folders,
                        gboolean update_target_folders,
                        GClosure *new_files_closure);

G_END_DECLS

#endif // __APPLICATION_H__


