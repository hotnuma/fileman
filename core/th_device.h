/*-
 * Copyright(c) 2012 Nick Schermer <nick@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or(at your option)
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

#ifndef __THUNARDEVICE_H__
#define __THUNARDEVICE_H__

#include <gio_ext.h>

G_BEGIN_DECLS

// ThunarDevice ---------------------------------------------------------------

typedef struct _ThunarDeviceClass ThunarDeviceClass;
typedef struct _ThunarDevice      ThunarDevice;
typedef enum   _ThunarDeviceKind  ThunarDeviceKind;

typedef void (*ThunarDeviceCallback) (ThunarDevice *device, const GError *error,
                                      gpointer user_data);

enum _ThunarDeviceKind
{
    THUNARDEVICE_KIND_VOLUME,
    THUNARDEVICE_KIND_MOUNT_LOCAL,
    THUNARDEVICE_KIND_MOUNT_REMOTE
};

#define TYPE_THUNARDEVICE (th_device_get_type())
#define THUNARDEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_THUNARDEVICE, ThunarDevice))
#define THUNARDEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_THUNARDEVICE, ThunarDeviceClass))
#define IS_THUNARDEVICE(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_THUNARDEVICE))
#define IS_THUNARDEVICE_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((obj),     TYPE_THUNARDEVICE))
#define THUNARDEVICE_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_THUNARDEVICE, ThunarDeviceClass))

GType th_device_get_type() G_GNUC_CONST;

gchar* th_device_get_name(const ThunarDevice *device) G_GNUC_MALLOC;
GIcon* th_device_get_icon(const ThunarDevice *device);
ThunarDeviceKind th_device_get_kind(const ThunarDevice *device) G_GNUC_PURE;
gchar* th_device_get_identifier(const ThunarDevice *device) G_GNUC_MALLOC;
gchar* th_device_get_identifier_unix(const ThunarDevice *device) G_GNUC_MALLOC;
gboolean th_device_get_hidden(const ThunarDevice *device);
GFile* th_device_get_root(const ThunarDevice *device);

gboolean th_device_can_eject(const ThunarDevice *device);
gboolean th_device_can_mount(const ThunarDevice *device);
gboolean th_device_can_unmount(const ThunarDevice *device);

gboolean th_device_is_mounted(const ThunarDevice *device);

void th_device_reload_file(ThunarDevice *device);

gint th_device_sort(const ThunarDevice *device1, const ThunarDevice *device2);

void th_device_mount(ThunarDevice *device,
                     GMountOperation *mount_operation,
                     GCancellable *cancellable,
                     ThunarDeviceCallback callback,
                     gpointer user_data);

void th_device_unmount(ThunarDevice *device,
                       GMountOperation *mount_operation,
                       GCancellable *cancellable,
                       ThunarDeviceCallback callback,
                       gpointer user_data);

void th_device_eject(ThunarDevice *device,
                     GMountOperation *mount_operation,
                     GCancellable *cancellable,
                     ThunarDeviceCallback callback,
                     gpointer user_data);

G_END_DECLS

#endif // __THUNARDEVICE_H__


