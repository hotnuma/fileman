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

#ifndef __STATUSBAR_H__
#define __STATUSBAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

// Statusbar ------------------------------------------------------------------

typedef struct _StatusbarClass StatusbarClass;
typedef struct _Statusbar      Statusbar;

#define TYPE_STATUSBAR (statusbar_get_type ())
#define STATUSBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_STATUSBAR, Statusbar))
#define STATUSBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_STATUSBAR, StatusbarClass))
#define IS_STATUSBAR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_STATUSBAR))
#define IS_STATUSBAR_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_STATUSBAR))
#define STATUSBAR_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_STATUSBAR, StatusbarClass))

GType statusbar_get_type() G_GNUC_CONST;

// Public ---------------------------------------------------------------------

GtkWidget* statusbar_new();

G_END_DECLS

#endif // __STATUSBAR_H__


