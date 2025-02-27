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

#ifndef __THUNARNAVIGATOR_H__
#define __THUNARNAVIGATOR_H__

#include <th-file.h>

G_BEGIN_DECLS

// ThunarNavigator ------------------------------------------------------------

typedef struct _ThunarNavigatorIface ThunarNavigatorIface;
typedef struct _ThunarNavigator      ThunarNavigator;

#define TYPE_THUNARNAVIGATOR (navigator_get_type())
#define THUNARNAVIGATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),      TYPE_THUNARNAVIGATOR, ThunarNavigator))
#define IS_THUNARNAVIGATOR(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),      TYPE_THUNARNAVIGATOR))
#define THUNARNAVIGATOR_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj),   TYPE_THUNARNAVIGATOR, ThunarNavigatorIface))

struct _ThunarNavigatorIface
{
    GTypeInterface __parent__;

    // methods
    ThunarFile* (*get_current_directory) (ThunarNavigator *navigator);
    void (*set_current_directory) (ThunarNavigator *navigator,
                                   ThunarFile *current_directory);

    // signals
    void (*change_directory) (ThunarNavigator *navigator, ThunarFile *directory);
};

GType navigator_get_type() G_GNUC_CONST;

ThunarFile* navigator_get_current_directory(ThunarNavigator *navigator);
void navigator_set_current_directory(ThunarNavigator *navigator,
                                     ThunarFile *current_directory);

void navigator_change_directory(ThunarNavigator *navigator, ThunarFile *directory);

G_END_DECLS

#endif // __THUNARNAVIGATOR_H__


