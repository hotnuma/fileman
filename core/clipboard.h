/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __CLIPBOARDMANAGER_H__
#define __CLIPBOARDMANAGER_H__

#include "th_file.h"

G_BEGIN_DECLS

// ClipboardManager -----------------------------------------------------------

typedef struct _ClipboardManager      ClipboardManager;
typedef struct _ClipboardManagerClass ClipboardManagerClass;

#define TYPE_CLIPBOARDMANAGER (clipman_get_type())

#define CLIPBOARDMANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_CLIPBOARDMANAGER, ClipboardManager))
#define IS_CLIPBOARDMANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_CLIPBOARDMANAGER))

GType clipman_get_type() G_GNUC_CONST;

ClipboardManager* clipman_get_for_display(GdkDisplay *display);

gboolean clipman_can_paste(ClipboardManager *manager);
gboolean clipman_has_cutted_file(ClipboardManager *manager, const ThunarFile *file);

void clipman_copy_files(ClipboardManager *manager, GList *files);
void clipman_cut_files(ClipboardManager *manager, GList *files);
void clipman_paste_files(ClipboardManager *manager, GFile *target_file,
                         GtkWidget *widget, GClosure *new_files_closure);

G_END_DECLS

#endif // __CLIPBOARDMANAGER_H__


