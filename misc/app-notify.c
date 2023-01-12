/*-
 * Copyright(c) 2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or(at your option) any later version.
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

#include <config.h>
#include <app-notify.h>

#include <libnotify/notify.h>
#include <th-device.h>

static gboolean _app_notify_initted = FALSE;

static gboolean _app_notify_init()
{
    gchar *spec_version = NULL;

    if (!_app_notify_initted && notify_init(PACKAGE_NAME))
    {
        /* we do this to work around bugs in libnotify < 0.6.0. Older
         * versions crash in notify_uninit() when no notifications are
         * displayed before. These versions also segfault when the
         * ret_spec_version parameter of notify_get_server_info is
         * NULL... */
        notify_get_server_info(NULL, NULL, NULL, &spec_version);
        g_free(spec_version);

        _app_notify_initted = TRUE;
    }

    return _app_notify_initted;
}

static void _app_notify_show(ThunarDevice *device, const gchar  *summary,
                             const gchar  *message)
{
    NotifyNotification *notification;
    GIcon              *icon;
    gchar              *icon_name = NULL;
    GFile              *icon_file;
    const gchar* const *icon_names;

    /* get suitable icon for the device */
    icon = th_device_get_icon(device);
    if (icon != NULL)
    {
        if (G_IS_THEMED_ICON(icon))
        {
            icon_names = g_themed_icon_get_names(G_THEMED_ICON(icon));
            if (icon_names != NULL)
                icon_name = g_strdup(icon_names[0]);
        }
        else if (G_IS_FILE_ICON(icon))
        {
            icon_file = g_file_icon_get_file(G_FILE_ICON(icon));
            if (icon_file != NULL)
                icon_name = g_file_get_path(icon_file);
        }
        g_object_unref(icon);
    }

    if (icon_name == NULL)
        icon_name = g_strdup("drive-removable-media");

    /* create notification */
#ifdef NOTIFY_CHECK_VERSION
#if NOTIFY_CHECK_VERSION(0, 7, 0)
    notification = notify_notification_new(summary, message, icon_name);
#else
    notification = notify_notification_new(summary, message, icon_name, NULL);
#endif
#else
    notification = notify_notification_new(summary, message, icon_name, NULL);
#endif

    notify_notification_set_urgency(notification, NOTIFY_URGENCY_CRITICAL);
    notify_notification_set_timeout(notification, NOTIFY_EXPIRES_NEVER);
    notify_notification_show(notification, NULL);

    /* attach to object for finalize */
    g_object_set_data_full(G_OBJECT(device), I_("thunar-notification"),
                           notification, g_object_unref);

    g_free(icon_name);
}

static gboolean _app_notify_device_readonly(ThunarDevice *device)
{
    GFile     *mount_point;
    gboolean   readonly = TRUE;
    GFileInfo *info;

    mount_point = th_device_get_root(device);
    if (mount_point != NULL)
    {
        info = g_file_query_info(mount_point, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE,
                                 G_FILE_QUERY_INFO_NONE, NULL, NULL);

        if (info != NULL)
        {
            if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
                readonly = !g_file_info_get_attribute_boolean(info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);

            g_object_unref(info);
        }

        g_object_unref(mount_point);
    }

    return readonly;
}

void app_notify_unmount(ThunarDevice *device)
{
    gchar       *name;
    const gchar *summary;
    gchar       *message;

    thunar_return_if_fail(THUNAR_IS_DEVICE(device));

    if (!_app_notify_init())
        return;

    name = th_device_get_name(device);

    if (_app_notify_device_readonly(device))
    {
        summary = _("Unmounting device");
        message = g_strdup_printf(_("The device \"%s\" is being unmounted by the system. "
                                     "Please do not remove the media or disconnect the "
                                     "drive"), name);
    }
    else
    {
        summary = _("Writing data to device");
        message = g_strdup_printf(_("There is data that needs to be written to the "
                                     "device \"%s\" before it can be removed. Please "
                                     "do not remove the media or disconnect the drive"),
                                   name);
    }

    _app_notify_show(device, summary, message);

    g_free(name);
    g_free(message);
}

void app_notify_eject(ThunarDevice *device)
{
    gchar       *name;
    const gchar *summary;
    gchar       *message;

    thunar_return_if_fail(THUNAR_IS_DEVICE(device));

    if (!_app_notify_init())
        return;

    name = th_device_get_name(device);

    if (_app_notify_device_readonly(device))
    {
        summary = _("Ejecting device");
        message = g_strdup_printf(_("The device \"%s\" is being ejected. "
                                     "This may take some time"), name);
    }
    else
    {
        summary = _("Writing data to device");
        message = g_strdup_printf(_("There is data that needs to be written to the "
                                     "device \"%s\" before it can be removed. Please "
                                     "do not remove the media or disconnect the drive"),
                                   name);
    }

    _app_notify_show(device, summary, message);

    g_free(name);
    g_free(message);
}

void app_notify_finish(ThunarDevice *device)
{
    NotifyNotification *notification;

    thunar_return_if_fail(THUNAR_IS_DEVICE(device));

    notification = g_object_get_data(G_OBJECT(device), I_("thunar-notification"));
    if (notification != NULL)
    {
        notify_notification_set_urgency(notification, NOTIFY_URGENCY_NORMAL);
        notify_notification_set_timeout(notification, 2000);
        notify_notification_show(notification, NULL);

        g_object_set_data(G_OBJECT(device), I_("thunar-notification"), NULL);
    }
}

void app_notify_uninit()
{
    if (_app_notify_initted && notify_is_initted())
        notify_uninit();
}


