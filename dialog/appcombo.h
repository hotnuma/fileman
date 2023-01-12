/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __APPCOMBO_H__
#define __APPCOMBO_H__

#include <th-file.h>

G_BEGIN_DECLS;

typedef struct _AppComboClass AppComboClass;
typedef struct _AppCombo      AppCombo;

#define TYPE_APPCOMBO (appcombo_get_type())
#define APPCOMBO(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST ((obj), TYPE_APPCOMBO, AppCombo))
#define APPCOMBO_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST ((klass),  TYPE_APPCOMBO, AppComboClass))
#define IS_APPCOMBO(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE ((obj), TYPE_APPCOMBO))
#define IS_APPCOMBO_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE ((klass),  TYPE_APPCOMBO))
#define APPCOMBO_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS ((obj),  TYPE_APPCOMBO, AppComboClass))

GType appcombo_get_type() G_GNUC_CONST;

GtkWidget* thunar_chooser_button_new() G_GNUC_MALLOC;

void thunar_chooser_button_set_file(AppCombo *chooser_button,
                                    ThunarFile          *file);

G_END_DECLS;

#endif /* !__APPCOMBO_H__ */

