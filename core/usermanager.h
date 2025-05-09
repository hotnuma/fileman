/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifndef __USERMANAGER_H__
#define __USERMANAGER_H__

#include <glib-object.h>

G_BEGIN_DECLS

typedef struct _ThunarGroup ThunarGroup;
typedef struct _ThunarUser  ThunarUser;

// User Manager ---------------------------------------------------------------

typedef struct _UserManager      UserManager;
typedef struct _UserManagerClass UserManagerClass;

#define TYPE_USERMANAGER (usermanager_get_type())

#define USERMANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_USERMANAGER, UserManager))
#define USERMANAGER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_USERMANAGER, UserManagerClass))
#define IS_USERMANAGER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_USERMANAGER))
#define IS_USERMANAGER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_USERMANAGER))
#define USERMANAGER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_USERMANAGER, UserManagerClass))

GType usermanager_get_type() G_GNUC_CONST;

UserManager* usermanager_get_default() G_GNUC_WARN_UNUSED_RESULT;

GList* usermanager_get_all_groups(UserManager *manager)
                                  G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
ThunarGroup* usermanager_get_group_by_id(UserManager *manager, guint32 id)
                                         G_GNUC_WARN_UNUSED_RESULT;
ThunarUser* usermanager_get_user_by_id(UserManager *manager, guint32 id)
                                       G_GNUC_WARN_UNUSED_RESULT;

// Group ----------------------------------------------------------------------

typedef struct _ThunarGroupClass ThunarGroupClass;

#define THUNAR_TYPE_GROUP (group_get_type())
#define THUNAR_GROUP(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  THUNAR_TYPE_GROUP, ThunarGroup))
#define THUNAR_GROUP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   THUNAR_TYPE_GROUP, ThunarGroupClass))
#define THUNAR_IS_GROUP(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  THUNAR_TYPE_GROUP))
#define THUNAR_IS_GROUP_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   THUNAR_TYPE_GROUP))
#define THUNAR_GROUP_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   THUNAR_TYPE_GROUP, ThunarGroupClass))

GType group_get_type() G_GNUC_CONST;

guint32 group_get_id(ThunarGroup *group);
const gchar* group_get_name(ThunarGroup *group);

// User -----------------------------------------------------------------------

typedef struct _ThunarUserClass ThunarUserClass;

#define THUNAR_TYPE_USER (user_get_type())
#define THUNAR_USER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  THUNAR_TYPE_USER, ThunarUser))
#define THUNAR_USER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   THUNAR_TYPE_USER, ThunarUserClass))
#define THUNAR_IS_USER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  THUNAR_TYPE_USER))
#define THUNAR_IS_USER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   THUNAR_TYPE_USER))
#define THUNAR_USER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   THUNAR_TYPE_USER, ThunarUserClass))

GType user_get_type() G_GNUC_CONST;

GList* user_get_groups(ThunarUser *user);
const gchar* user_get_name(ThunarUser *user);
const gchar* user_get_real_name(ThunarUser *user);
gboolean user_is_me(ThunarUser *user);

G_END_DECLS

#endif // __USERMANAGER_H__


