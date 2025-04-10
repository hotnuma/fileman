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

#ifndef __GTK_EXT_H__
#define __GTK_EXT_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

GMountOperation* etk_mount_operation_new(gpointer parent);

GtkWidget* etk_get_focused_widget();
void etk_widget_set_tooltip(GtkWidget *widget, const gchar *format, ...)
                            G_GNUC_PRINTF (2, 3);

gboolean etk_editable_can_cut(GtkEditable *editable);
gboolean etk_editable_can_copy(GtkEditable *editable);
gboolean etk_editable_can_paste(GtkEditable *editable);

void etk_label_set_a11y_relation(GtkLabel *label, GtkWidget *widget);

void etk_menu_run(GtkMenu *menu, GtkWidget *widget);

G_END_DECLS

#endif // __GTK_EXT_H__


