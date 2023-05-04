/*-
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __APP_NOTIFY_H__
#define __APP_NOTIFY_H__

#include <th_device.h>

G_BEGIN_DECLS

gboolean app_notify_init();
void app_notify_uninit();

void app_notify_unmount(ThunarDevice *device);
void app_notify_eject(ThunarDevice *device);
void app_notify_finish(ThunarDevice *device);

G_END_DECLS

#endif // __APP_NOTIFY_H__


