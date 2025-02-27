/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNARHISTORY_H__
#define __THUNARHISTORY_H__

#include "th-file.h"

G_BEGIN_DECLS

// ThunarHistory --------------------------------------------------------------

typedef struct _ThunarHistoryClass ThunarHistoryClass;
typedef struct _ThunarHistory      ThunarHistory;

#define TYPE_THUNARHISTORY (history_get_type())
#define THUNARHISTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARHISTORY, ThunarHistory))
#define THUNARHISTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARHISTORY, ThunarHistoryClass))
#define IS_THUNARHISTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARHISTORY))
#define IS_THUNARHISTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_THUNARHISTORY))
#define THUNARHISTORY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARHISTORY, ThunarHistoryClass))

typedef enum
{
    THUNARHISTORY_MENU_BACK,
    THUNARHISTORY_MENU_FORWARD,

} ThunarHistoryMenuType;

struct _ThunarHistoryClass
{
    GObjectClass __parent__;

    // external signals
    void (*history_changed) (ThunarHistory *history, const gchar *initial_text);
};

GType history_get_type() G_GNUC_CONST;

ThunarHistory* history_copy(ThunarHistory *history);
void history_show_menu(ThunarHistory *history, ThunarHistoryMenuType type,
                       GtkWidget *parent);

void history_action_back(ThunarHistory *history);
void history_action_forward(ThunarHistory *history);

gboolean history_has_back(ThunarHistory *history);
ThunarFile* history_peek_back(ThunarHistory *history);

gboolean history_has_forward(ThunarHistory *history);
ThunarFile* history_peek_forward(ThunarHistory *history);

G_END_DECLS

#endif // __THUNARHISTORY_H__


