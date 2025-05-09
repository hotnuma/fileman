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

#include "config.h"
#include "th-device.h"

#include "appnotify.h"
#include "devmonitor.h"
#include "th-file.h"

typedef struct _ThunarDeviceOperation ThunarDeviceOperation;

typedef gboolean (*AsyncCallbackFinish) (GObject *object,
                                         GAsyncResult *result,
                                         GError **error);

// ThunarDevice ---------------------------------------------------------------

static void th_device_finalize(GObject *object);
static void th_device_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec);
static void th_device_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec);

// public ---------------------------------------------------------------------

static void _th_device_emit_pre_unmount(ThunarDevice *device,
                                        gboolean all_volumes);

static ThunarDeviceOperation* _th_device_operation_new(
                                        ThunarDevice *device,
                                        ThunarDeviceCallback callback,
                                        gpointer user_data,
                                        gpointer callback_finish);

static void _th_device_operation_finish(GObject *object, GAsyncResult *result,
                                        gpointer user_data);

enum
{
    PROP_0,
    PROP_DEVICE,
    PROP_HIDDEN,
    PROP_KIND
};

struct _ThunarDevice
{
    GObject     __parent__;

    // a GVolume/GMount/GDrive
    gpointer    device;

    ThunarDeviceKind kind;

    // if the device is the list of hidden names
    guint       hidden : 1;

    // added time for sorting
    gint64      stamp;
};

struct _ThunarDeviceClass
{
    GObjectClass __parent__;
};

struct _ThunarDeviceOperation
{
    // the device the operation is working on
    ThunarDevice    *device;

    // finish function for the async callback
    AsyncCallbackFinish callback_finish;

    // callback for the user
    ThunarDeviceCallback callback;
    gpointer        user_data;

};

G_DEFINE_TYPE(ThunarDevice, th_device, G_TYPE_OBJECT)

static void th_device_class_init(ThunarDeviceClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = th_device_finalize;
    gobject_class->get_property = th_device_get_property;
    gobject_class->set_property = th_device_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_DEVICE,
                                    g_param_spec_object(
                                        "device",
                                        "device",
                                        "device",
                                        G_TYPE_OBJECT,
                                        E_PARAM_READWRITE
                                        | G_PARAM_CONSTRUCT_ONLY));

    g_object_class_install_property(gobject_class,
                                    PROP_HIDDEN,
                                    g_param_spec_boolean(
                                        "hidden",
                                        "hidden",
                                        "hidden",
                                        FALSE,
                                        E_PARAM_READWRITE));

    g_object_class_install_property(gobject_class,
                                    PROP_KIND,
                                    g_param_spec_uint(
                                        "kind",
                                        "kind",
                                        "kind",
                                        THUNARDEVICE_KIND_VOLUME,
                                        THUNARDEVICE_KIND_MOUNT_REMOTE,
                                        THUNARDEVICE_KIND_VOLUME,
                                        E_PARAM_READWRITE
                                        | G_PARAM_CONSTRUCT_ONLY));
}

static void th_device_init(ThunarDevice *device)
{
    device->kind = THUNARDEVICE_KIND_VOLUME;
    device->stamp = g_get_real_time();
}

static void th_device_finalize(GObject *object)
{
    ThunarDevice *device = THUNARDEVICE(object);

    if (device->device != NULL)
        g_object_unref(G_OBJECT(device->device));

    G_OBJECT_CLASS(th_device_parent_class)->finalize(object);
}

static void th_device_get_property(GObject *object, guint prop_id,
                                   GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarDevice *device = THUNARDEVICE(object);

    switch (prop_id)
    {
    case PROP_DEVICE:
        g_value_set_object(value, device->device);
        break;

    case PROP_HIDDEN:
        g_value_set_boolean(value, device->hidden);
        break;

    case PROP_KIND:
        g_value_set_uint(value, device->kind);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void th_device_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarDevice *device = THUNARDEVICE(object);

    switch (prop_id)
    {
    case PROP_DEVICE:
        device->device = g_value_dup_object(value);
        e_assert(G_IS_VOLUME(device->device) || G_IS_MOUNT(device->device));
        break;

    case PROP_HIDDEN:
        device->hidden = g_value_get_boolean(value);
        break;

    case PROP_KIND:
        device->kind = g_value_get_uint(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}


// public ---------------------------------------------------------------------

gchar* th_device_get_name(const ThunarDevice *device)
{
    gchar *display_name = NULL;

    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    if (G_IS_VOLUME(device->device))
    {
        return g_volume_get_name(device->device);
    }
    else if (G_IS_MOUNT(device->device))
    {
        // probably we can make a nicer name for this mount
        GFile *mount_point = th_device_get_root(device);

        if (mount_point != NULL)
        {
            display_name = e_file_get_display_name_remote(mount_point);
            g_object_unref(mount_point);
        }

        if (display_name == NULL)
            display_name = g_mount_get_name(device->device);

        return display_name;
    }
    else
    {
        e_assert_not_reached();
    }

    return NULL;
}

GIcon* th_device_get_icon(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    if (G_IS_VOLUME(device->device))
        return g_volume_get_icon(device->device);
    else if (G_IS_MOUNT(device->device))
        return g_mount_get_icon(device->device);
    else
        e_assert_not_reached();

    return NULL;
}

ThunarDeviceKind th_device_get_kind(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), THUNARDEVICE_KIND_VOLUME);

    return device->kind;
}

gchar* th_device_get_identifier(const ThunarDevice *device)
{
    gchar *ident = NULL;

    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    if (G_IS_VOLUME(device->device))
    {
        ident = g_volume_get_uuid(device->device);

        if (ident == NULL)
            ident = g_volume_get_identifier(device->device,
                                            G_VOLUME_IDENTIFIER_KIND_UUID);

        if (ident == NULL)
            ident = g_volume_get_name(device->device);
    }
    else if (G_IS_MOUNT(device->device))
    {
        ident = g_mount_get_uuid(device->device);

        if (ident == NULL)
            ident = g_mount_get_name(device->device);
    }
    else
    {
        e_assert_not_reached();
    }

    return ident;
}

gchar* th_device_get_identifier_unix(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    if (G_IS_VOLUME(device->device))
    {
        gchar *ident = g_volume_get_identifier(
                            device->device,
                            G_VOLUME_IDENTIFIER_KIND_UNIX_DEVICE);
        return ident;
    }
    else if (G_IS_MOUNT(device->device))
    {
        return NULL;
    }
    else
    {
        e_assert_not_reached();
    }

    return NULL;
}

gboolean th_device_get_hidden(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), FALSE);

    return device->hidden;
}

GFile* th_device_get_root(const ThunarDevice *device)
{
    GFile  *root = NULL;
    GMount *volume_mount;

    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    if (G_IS_VOLUME(device->device))
    {
        volume_mount = g_volume_get_mount(device->device);
        if (volume_mount != NULL)
        {
            root = g_mount_get_root(volume_mount);
            g_object_unref(volume_mount);
        }

        if (root == NULL)
            root = g_volume_get_activation_root(device->device);
    }
    else if (G_IS_MOUNT(device->device))
    {
        root = g_mount_get_root(device->device);
    }
    else
    {
        e_assert_not_reached();
    }

    return root;
}

gboolean th_device_can_eject(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), FALSE);

    gboolean can_eject = FALSE;

    if (G_IS_VOLUME(device->device))
    {
        GDrive *drive = g_volume_get_drive(device->device);

        if (drive != NULL)
        {
            can_eject = g_drive_can_eject(drive) || g_drive_can_stop(drive);
            g_object_unref(drive);
        }
        else
        {
            // check if the volume can eject
            can_eject = g_volume_can_eject(device->device);
        }
    }
    else if (G_IS_MOUNT(device->device))
    {
        /* eject or unmount because thats for the user the same
         * because we prefer volumes over mounts as devices */
        can_eject = g_mount_can_eject(device->device)
                    || g_mount_can_unmount(device->device);
    }
    else
    {
        e_assert_not_reached();
    }

    return can_eject;
}

gboolean th_device_can_mount(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), FALSE);

    gboolean  can_mount = FALSE;

    if (G_IS_VOLUME(device->device))
    {
        // only volumes without a mountpoint and mount capability
        GMount *volume_mount = g_volume_get_mount(device->device);

        if (volume_mount == NULL)
            can_mount = g_volume_can_mount(device->device);
        else
            g_object_unref(volume_mount);
    }
    else if (G_IS_MOUNT(device->device))
    {
        // a mount is already mounted...
        can_mount = FALSE;
    }
    else
    {
        e_assert_not_reached();
    }

    return can_mount;
}

gboolean th_device_can_unmount(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), FALSE);

    gboolean can_unmount = FALSE;

    if (G_IS_VOLUME(device->device))
    {
        // only volumes with a mountpoint and unmount capability
        GMount *volume_mount = g_volume_get_mount(device->device);

        if (volume_mount != NULL)
        {
            can_unmount = g_mount_can_unmount(volume_mount);
            g_object_unref(volume_mount);
        }
    }
    else if (G_IS_MOUNT(device->device))
    {
        // check if the mount can unmount
        can_unmount = g_mount_can_unmount(device->device);
    }
    else
    {
        e_assert_not_reached();
    }

    return can_unmount;
}

gboolean th_device_is_mounted(const ThunarDevice *device)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device), FALSE);

    gboolean is_mounted = FALSE;

    if (G_IS_VOLUME(device->device))
    {
        // a volume with a mount point is mounted
        GMount *volume_mount = g_volume_get_mount(device->device);

        if (volume_mount != NULL)
        {
            is_mounted = TRUE;
            g_object_unref(volume_mount);
        }
    }
    else if (G_IS_MOUNT(device->device))
    {
        // mounter are always mounted...
        is_mounted = TRUE;
    }
    else
    {
        e_assert_not_reached();
    }

    return is_mounted;
}

void th_device_reload_file(ThunarDevice *device)
{
    e_return_if_fail(IS_THUNARDEVICE(device));

    // Reload the related ThunarFile of the ThunarDevice

    GFile *mount_point = th_device_get_root(device);

    if (mount_point == NULL)
        return;

    // try to determine the file for the mount point
    ThunarFile *file = th_file_get(mount_point, NULL);

    if (file != NULL)
    {
        th_file_reload(file);
        g_object_unref(file);
    }

    g_object_unref(mount_point);
}

gint th_device_sort(const ThunarDevice *device1, const ThunarDevice *device2)
{
    e_return_val_if_fail(IS_THUNARDEVICE(device1), 0);
    e_return_val_if_fail(IS_THUNARDEVICE(device2), 0);

    // sort mounts above volumes
    if (G_OBJECT_TYPE(device1->device) != G_OBJECT_TYPE(device2->device))
        return G_IS_MOUNT(device1->device) ? 1 : -1;

    // sort by detect stamp
    return device1->stamp > device2->stamp ? 1 : -1;
}


// actions --------------------------------------------------------------------

void th_device_mount(ThunarDevice *device, GMountOperation *mount_operation,
                     GCancellable *cancellable, ThunarDeviceCallback callback,
                     gpointer user_data)
{
    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(G_IS_MOUNT_OPERATION(mount_operation));
    e_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    e_return_if_fail(callback != NULL);

    if (G_IS_VOLUME(device->device) == false)
        return;

    ThunarDeviceOperation *op;
    op = _th_device_operation_new(device,
                                  callback,
                                  user_data,
                                  g_volume_mount_finish);

    g_volume_mount(G_VOLUME(device->device),
                   G_MOUNT_MOUNT_NONE,
                   mount_operation,
                   cancellable,
                   _th_device_operation_finish,
                   op);
}

void th_device_unmount(ThunarDevice *device, GMountOperation *mount_operation,
                       GCancellable *cancellable,
                       ThunarDeviceCallback callback,
                       gpointer user_data)
{
    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(G_IS_MOUNT_OPERATION(mount_operation));
    e_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    e_return_if_fail(callback != NULL);

    GMount *mount;

    // get the mount from the volume or use existing mount
    if (G_IS_VOLUME(device->device))
        mount = g_volume_get_mount(device->device);
    else if (G_IS_MOUNT(device->device))
        mount = g_object_ref(device->device);
    else
        mount = NULL;

    if (mount == NULL)
        return;

    if (!g_mount_can_unmount(mount))
    {
        g_object_unref(G_OBJECT(mount));

        return;
    }

    // inform user
    app_notify_unmount(device);

    // try unmounting the mount
    _th_device_emit_pre_unmount(device, FALSE);

    ThunarDeviceOperation *op;
    op = _th_device_operation_new(device,
                                  callback,
                                  user_data,
                                  g_mount_unmount_with_operation_finish);

    g_mount_unmount_with_operation(mount,
                                   G_MOUNT_UNMOUNT_NONE,
                                   mount_operation,
                                   cancellable,
                                   _th_device_operation_finish,
                                   op);

    g_object_unref(G_OBJECT(mount));
}

void th_device_eject(ThunarDevice *device, GMountOperation *mount_operation,
                     GCancellable *cancellable, ThunarDeviceCallback callback,
                     gpointer user_data)
{
    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(G_IS_MOUNT_OPERATION(mount_operation));
    e_return_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable));
    e_return_if_fail(callback != NULL);

    ThunarDeviceOperation *op;
    GMount                *mount = NULL;
    GVolume               *volume;
    GDrive                *drive;

    if (G_IS_VOLUME(device->device))
    {
        volume = device->device;
        drive = g_volume_get_drive(volume);

        if (drive != NULL)
        {
            if (g_drive_can_stop(drive))
            {
                // inform user
                app_notify_eject(device);

                // try to stop the drive
                _th_device_emit_pre_unmount(device, TRUE);
                op = _th_device_operation_new(device, callback, user_data,
                                              g_drive_stop_finish);
                g_drive_stop(drive,
                             G_MOUNT_UNMOUNT_NONE,
                             mount_operation,
                             cancellable,
                             _th_device_operation_finish,
                             op);

                g_object_unref(drive);

                // done
                return;
            }
            else if (g_drive_can_eject(drive))
            {
                // inform user
                app_notify_eject(device);

                // try to stop the drive
                _th_device_emit_pre_unmount(device, TRUE);
                op = _th_device_operation_new(
                                        device,
                                        callback, user_data,
                                        g_drive_eject_with_operation_finish);
                g_drive_eject_with_operation(drive,
                                             G_MOUNT_UNMOUNT_NONE,
                                             mount_operation,
                                             cancellable,
                                             _th_device_operation_finish,
                                             op);

                g_object_unref(drive);

                // done
                return;
            }

            g_object_unref(drive);
        }

        if (g_volume_can_eject(volume))
        {
            // inform user
            app_notify_eject(device);

            // try ejecting the volume
            _th_device_emit_pre_unmount(device, TRUE);
            op = _th_device_operation_new(
                                    device,
                                    callback, user_data,
                                    g_volume_eject_with_operation_finish);

            g_volume_eject_with_operation(volume,
                                          G_MOUNT_UNMOUNT_NONE,
                                          mount_operation,
                                          cancellable,
                                          _th_device_operation_finish,
                                          op);

            // done
            return;
        }
        else
        {
            // get the mount and fall-through
            mount = g_volume_get_mount(volume);
        }
    }
    else if (G_IS_MOUNT(device->device))
    {
        // set the mount and fall-through
        mount = g_object_ref(device->device);
    }

    // handle mounts
    if (mount != NULL)
    {
        // distinguish between ejectable and unmountable mounts
        if (g_mount_can_eject(mount))
        {
            // inform user
            app_notify_eject(device);

            // try ejecting the mount
            _th_device_emit_pre_unmount(device, FALSE);
            op = _th_device_operation_new(
                                device, callback, user_data,
                                g_mount_eject_with_operation_finish);
            g_mount_eject_with_operation(mount,
                                         G_MOUNT_UNMOUNT_NONE,
                                         mount_operation,
                                         cancellable,
                                         _th_device_operation_finish,
                                         op);
        }
        else if (g_mount_can_unmount(mount))
        {
            // inform user
            app_notify_unmount(device);

            // try unmounting the mount
            _th_device_emit_pre_unmount(device, FALSE);
            op = _th_device_operation_new(
                                device, callback, user_data,
                                g_mount_unmount_with_operation_finish);
            g_mount_unmount_with_operation(mount,
                                           G_MOUNT_UNMOUNT_NONE,
                                           mount_operation,
                                           cancellable,
                                           _th_device_operation_finish,
                                           op);
        }

        g_object_unref(G_OBJECT(mount));
    }
}


// notify ---------------------------------------------------------------------

static void _th_device_emit_pre_unmount(ThunarDevice *device,
                                        gboolean all_volumes)
{
    (void) all_volumes;
    GFile *root_file = th_device_get_root(device);

    if (root_file == NULL)
        return;

    /* make sure the pre-unmount event is emitted, this is important
     * for the interface */
    DeviceMonitor *monitor = devmon_get();
    g_signal_emit_by_name(monitor, "device-pre-unmount", device, root_file);
    g_object_unref(monitor);
    g_object_unref(root_file);
}


// DeviceOperation ------------------------------------------------------------

static ThunarDeviceOperation* _th_device_operation_new(
                                            ThunarDevice *device,
                                            ThunarDeviceCallback callback,
                                            gpointer user_data,
                                            gpointer callback_finish)
{
    ThunarDeviceOperation *op = g_slice_new0(ThunarDeviceOperation);

    op->device = g_object_ref(device);
    op->callback = callback;
    op->callback_finish = callback_finish;
    op->user_data = user_data;

    return op;
}

static void _th_device_operation_finish(GObject *object, GAsyncResult *result,
                                        gpointer user_data)
{
    e_return_if_fail(G_IS_OBJECT(object));
    e_return_if_fail(G_IS_ASYNC_RESULT(result));

    ThunarDeviceOperation *op = user_data;
    GError                *error = NULL;

    // remove notification
    app_notify_finish(op->device);

    // finish the operation
    if (!(op->callback_finish) (object, result, &error))
    {
        // unset the error if a helper program has already interacted
        // with the user
        if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_FAILED_HANDLED))
            g_clear_error(&error);

        if (op->callback_finish == (AsyncCallbackFinish) g_volume_mount_finish)
        {
            // special handling for mount operation
            if (g_error_matches(error, G_IO_ERROR, G_IO_ERROR_ALREADY_MOUNTED)
                || g_error_matches(error, G_IO_ERROR, G_IO_ERROR_PENDING))
            {
                g_clear_error(&error);
            }
        }
    }

    // user callback
    (op->callback) (op->device, error, op->user_data);

    // cleanup
    g_clear_error(&error);
    g_object_unref(G_OBJECT(op->device));
    g_slice_free(ThunarDeviceOperation, op);
}


