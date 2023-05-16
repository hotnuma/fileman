/*-
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNARBROWSER_H__
#define __THUNARBROWSER_H__

#include <th_file.h>
#include <th_device.h>

G_BEGIN_DECLS

typedef struct _ThunarBrowser      ThunarBrowser;
typedef struct _ThunarBrowserIface ThunarBrowserIface;

#define TYPE_THUNARBROWSER (browser_get_type())
#define THUNARBROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),      TYPE_THUNARBROWSER, ThunarBrowser))
#define THUNAR_IS_BROWSER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),      TYPE_THUNARBROWSER))
#define THUNARBROWSER_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj),   TYPE_THUNARBROWSER, ThunarBrowserIface))

struct _ThunarBrowserIface
{
    GTypeInterface __parent__;
};

typedef void (*ThunarBrowserPokeFileFunc) (ThunarBrowser *browser,
                                           ThunarFile    *file,
                                           ThunarFile    *target_file,
                                           GError        *error,
                                           gpointer      user_data);

typedef void (*ThunarBrowserPokeDeviceFunc) (ThunarBrowser *browser,
                                             ThunarDevice  *volume,
                                             ThunarFile    *mount_point,
                                             GError        *error,
                                             gpointer      user_data,
                                             gboolean      cancelled);

typedef void (*ThunarBrowserPokeLocationFunc) (ThunarBrowser *browser,
                                               GFile         *location,
                                               ThunarFile    *file,
                                               ThunarFile    *target_file,
                                               GError        *error,
                                               gpointer      user_data);

GType browser_get_type() G_GNUC_CONST;

void browser_poke_file(ThunarBrowser *browser, ThunarFile *file,
                       gpointer widget, ThunarBrowserPokeFileFunc func,
                       gpointer user_data);
void browser_poke_device(ThunarBrowser *browser, ThunarDevice *device,
                         gpointer widget, ThunarBrowserPokeDeviceFunc func,
                         gpointer user_data);
void browser_poke_location(ThunarBrowser *browser, GFile *location,
                           gpointer widget, ThunarBrowserPokeLocationFunc func,
                           gpointer user_data);

G_END_DECLS

#endif // __THUNARBROWSER_H__


