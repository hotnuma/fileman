/*-
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include "config.h"
#include "browser.h"

#include "gtk-ext.h"

typedef struct _PokeFileData   PokeFileData;
typedef struct _PokeDeviceData PokeDeviceData;

static PokeDeviceData* _browser_poke_device_data_new(
                                        ThunarBrowser *browser,
                                        ThunarDevice *device,
                                        PokeDeviceFunc func,
                                        gpointer user_data);
static void _browser_poke_device_data_free(PokeDeviceData *poke_data);

// browser_poke_device
static void _browser_poke_device_file_finish(GFile      *location,
                                             ThunarFile *file,
                                             GError     *error,
                                             gpointer    user_data);
static void _browser_poke_device_finish(ThunarDevice *device,
                                        const GError *error,
                                        gpointer      user_data);

// browser_poke_file
static PokeFileData* _browser_poke_file_data_new(
                                            ThunarBrowser *browser,
                                            GFile *location,
                                            ThunarFile *source,
                                            ThunarFile *file,
                                            PokeFileFunc func,
                                            PokeLocationFunc location_func,
                                            gpointer user_data);
static void _browser_poke_file_data_free(PokeFileData *poke_data);

static void _browser_poke_file_internal(ThunarBrowser *browser,
                                        GFile *location,
                                        ThunarFile *source,
                                        ThunarFile *file,
                                        gpointer widget,
                                        PokeFileFunc func,
                                        PokeLocationFunc location_func,
                                        gpointer user_data);
static void _browser_poke_shortcut_file_finish(GFile *location,
                                               ThunarFile *file,
                                               GError *error,
                                               gpointer user_data);
static void _browser_poke_mountable_file_finish(GFile *location,
                                                ThunarFile *file,
                                                GError *error,
                                                gpointer user_data);
static void _browser_poke_mountable_finish(GObject *object,
                                           GAsyncResult *result,
                                           gpointer user_data);
static void _browser_poke_file_finish(GObject *object,
                                      GAsyncResult *result,
                                      gpointer user_data);

struct _PokeFileData
{
    GFile               *location;
    ThunarBrowser       *browser;
    ThunarFile          *source;
    ThunarFile          *file;
    PokeFileFunc        func;
    PokeLocationFunc    location_func;
    gpointer            user_data;
};

struct _PokeDeviceData
{
    ThunarBrowser   *browser;
    ThunarDevice    *device;
    ThunarFile      *mount_point;
    PokeDeviceFunc  func;
    gpointer        user_data;
};

GType browser_get_type()
{
    static volatile gsize type__volatile = 0;
    GType type;

    if (g_once_init_enter((gsize*) &type__volatile))
    {
        type = g_type_register_static_simple(G_TYPE_INTERFACE,
                                              I_("ThunarBrowser"),
                                              sizeof(ThunarBrowserIface),
                                              NULL,
                                              0,
                                              NULL,
                                              0);

        g_type_interface_add_prerequisite(type, G_TYPE_OBJECT);

        g_once_init_leave(&type__volatile, type);
    }

    return type__volatile;
}

// ----------------------------------------------------------------------------

/**
 * thunar_browser_poke_device:
 * @browser   : a #ThunarBrowser.
 * @device    : a #ThunarDevice.
 * @widget    : a #GtkWidget, a #GdkScreen or %NULL.
 * @func      : a #ThunarBrowserPokeDeviceFunc callback or %NULL.
 * @user_data : pointer to arbitrary user data or %NULL.
 *
 * This function checks if @device is mounted or not. If it is, it loads
 * a #ThunarFile for the mount root and calls @func. If it is not mounted,
 * it first mounts the device asynchronously and calls @func with the
 * #ThunarFile corresponding to the mount root when the mounting is finished.
 *
 * The #ThunarFile passed to @func will be %NULL if, and only if mounting
 * the @device failed. The #GError passed to @func will be set if, and only
 * if mounting failed.
 **/
static PokeDeviceData* _browser_poke_device_data_new(ThunarBrowser *browser,
                                                     ThunarDevice *device,
                                                     PokeDeviceFunc func,
                                                     gpointer user_data)
{
    PokeDeviceData *poke_data;

    e_return_val_if_fail(THUNAR_IS_BROWSER(browser), NULL);
    e_return_val_if_fail(IS_THUNARDEVICE(device), NULL);

    poke_data = g_slice_new0(PokeDeviceData);
    poke_data->browser = g_object_ref(browser);
    poke_data->device = g_object_ref(device);
    poke_data->func = func;
    poke_data->user_data = user_data;

    return poke_data;
}

static void _browser_poke_device_data_free(PokeDeviceData *poke_data)
{
    e_return_if_fail(poke_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARDEVICE(poke_data->device));

    g_object_unref(poke_data->browser);
    g_object_unref(poke_data->device);

    g_slice_free(PokeDeviceData, poke_data);
}

void browser_poke_device(ThunarBrowser   *browser,
                         ThunarDevice    *device,
                         gpointer        widget,
                         PokeDeviceFunc func,
                         gpointer        user_data)
{
    GMountOperation *mount_operation;
    PokeDeviceData  *poke_data;
    GFile           *mount_point;

    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(THUNARDEVICE(device));

    if (th_device_is_mounted(device))
    {
        mount_point = th_device_get_root(device);

        poke_data = _browser_poke_device_data_new(browser,
                                                  device, func, user_data);

        th_file_get_async(mount_point, NULL,
                               _browser_poke_device_file_finish,
                               poke_data);

        g_object_unref(mount_point);
    }
    else
    {
        poke_data = _browser_poke_device_data_new(browser,
                                                  device, func, user_data);

        mount_operation = etk_mount_operation_new(widget);

        th_device_mount(device,
                             mount_operation,
                             NULL,
                             _browser_poke_device_finish,
                             poke_data);

        g_object_unref(mount_operation);
    }
}

static void _browser_poke_device_file_finish(GFile *location,
                                             ThunarFile *file,
                                             GError *error,
                                             gpointer user_data)
{
    PokeDeviceData *poke_data = user_data;

    e_return_if_fail(G_IS_FILE(location));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARDEVICE(poke_data->device));

    if (poke_data->func != NULL)
    {
       (poke_data->func) (poke_data->browser,
                          poke_data->device, file, error,
                          poke_data->user_data, FALSE);
    }

    _browser_poke_device_data_free(poke_data);
}

static void _browser_poke_device_finish(ThunarDevice *device,
                                        const GError *error,
                                        gpointer user_data)
{
    PokeDeviceData *poke_data = user_data;
    GFile          *mount_point = NULL;
    gboolean        cancelled = FALSE;

    e_return_if_fail(IS_THUNARDEVICE(device));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARDEVICE(poke_data->device));
    e_return_if_fail(device == poke_data->device);

    if (error == NULL)
        mount_point = th_device_get_root(device);

    if (mount_point == NULL)
    {
        // mount_point is NULL when user dismisses the authentication dialog
        cancelled = TRUE;
    }
    else
    {
        /* resolve the ThunarFile for the mount point asynchronously
         * and defer cleaning up the poke data until that has finished */
        th_file_get_async(mount_point, NULL,
                               _browser_poke_device_file_finish,
                               poke_data);

        g_object_unref(mount_point);

        return;
    }

    if (poke_data->func != NULL)
    {
       (poke_data->func)(poke_data->browser, poke_data->device, NULL,
                          (GError *) error, poke_data->user_data, cancelled);
    }

    _browser_poke_device_data_free(poke_data);
}

// ----------------------------------------------------------------------------

/**
 * thunar_browser_poke_file:
 * @browser : a #ThunarBrowser.
 * @file      : a #ThunarFile.
 * @widget    : a #GtkWidget, a #GdkScreen or %NULL.
 * @func      : a #ThunarBrowserPokeFileFunc callback or %NULL.
 * @user_data : pointer to arbitrary user data or %NULL.
 *
 * Pokes a #ThunarFile to see what's behind it.
 *
 * If @file has the type %G_FILE_TYPE_SHORTCUT, it tries to load and mount
 * the file that is referred to by the %G_FILE_ATTRIBUTE_STANDARD_TARGET_URI
 * of the @file.
 *
 * If @file has the type %G_FILE_TYPE_MOUNTABLE, it tries to mount the @file.
 *
 * In the other cases, if the enclosing volume of the @file is not mounted
 * yet, it tries to mount it.
 *
 * When finished or if an error occured, it calls @func with the provided
 * @user data. The #GError parameter to @func is set if, and only if there
 * was an error.
 **/
static PokeFileData* _browser_poke_file_data_new(
                                    ThunarBrowser                 *browser,
                                    GFile                         *location,
                                    ThunarFile                    *source,
                                    ThunarFile                    *file,
                                    PokeFileFunc     func,
                                    PokeLocationFunc location_func,
                                    gpointer                      user_data)
{
    PokeFileData *poke_data;

    e_return_val_if_fail(THUNAR_IS_BROWSER(browser), NULL);

    poke_data = g_slice_new0(PokeFileData);
    poke_data->browser = g_object_ref(browser);

    if (location != NULL)
        poke_data->location = g_object_ref(location);

    if (source != NULL)
        poke_data->source = g_object_ref(source);

    if (file != NULL)
        poke_data->file = g_object_ref(file);

    poke_data->func = func;
    poke_data->location_func = location_func;

    poke_data->user_data = user_data;

    return poke_data;
}

static void _browser_poke_file_data_free(PokeFileData *poke_data)
{
    e_return_if_fail(poke_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));

    g_object_unref(poke_data->browser);

    if (poke_data->location != NULL)
        g_object_unref(poke_data->location);

    if (poke_data->source != NULL)
        g_object_unref(poke_data->source);

    if (poke_data->file != NULL)
        g_object_unref(poke_data->file);

    g_slice_free(PokeFileData, poke_data);
}

void browser_poke_file(ThunarBrowser *browser,
                       ThunarFile *file, gpointer widget,
                       PokeFileFunc func, gpointer user_data)
{
    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(IS_THUNARFILE(file));

    _browser_poke_file_internal(browser,
                                th_file_get_file(file),
                                file,
                                file,
                                widget,
                                func,
                                NULL,
                                user_data);
}

static void _browser_poke_file_internal(
                                ThunarBrowser *browser,
                                GFile *location,
                                ThunarFile *source,
                                ThunarFile *file,
                                gpointer widget,
                                PokeFileFunc func,
                                PokeLocationFunc location_func,
                                gpointer user_data)
{
    e_return_if_fail(THUNAR_IS_BROWSER(browser));
    e_return_if_fail(G_IS_FILE(location));
    e_return_if_fail(IS_THUNARFILE(source));
    e_return_if_fail(IS_THUNARFILE(file));

    GFile           *target;
    PokeFileData    *poke_data;
    GMountOperation *mount_operation;

    if (th_file_get_filetype(file) == G_FILE_TYPE_SHORTCUT)
    {
        target = th_file_get_target_location(file);

        poke_data = _browser_poke_file_data_new(browser,
                                                location,
                                                source,
                                                file,
                                                func,
                                                location_func,
                                                user_data);

        th_file_get_async(target,
                          NULL,
                          _browser_poke_shortcut_file_finish,
                          poke_data);

        g_object_unref(target);
    }
    else if (th_file_get_filetype(file) == G_FILE_TYPE_MOUNTABLE)
    {
        if (th_file_is_mounted(file))
        {
            target = th_file_get_target_location(file);

            poke_data = _browser_poke_file_data_new(browser,
                                                    location,
                                                    source,
                                                    file,
                                                    func,
                                                    location_func,
                                                    user_data);

            th_file_get_async(target,
                              NULL,
                              _browser_poke_mountable_file_finish,
                              poke_data);

            g_object_unref(target);
        }
        else
        {
            poke_data = _browser_poke_file_data_new(browser,
                                                    location,
                                                    source,
                                                    file,
                                                    func,
                                                    location_func,
                                                    user_data);

            mount_operation = etk_mount_operation_new(widget);

            g_file_mount_mountable(th_file_get_file(file),
                                   G_MOUNT_MOUNT_NONE,
                                   mount_operation,
                                   NULL,
                                   _browser_poke_mountable_finish,
                                   poke_data);

            g_object_unref(mount_operation);
        }
    }
    else if (!th_file_is_mounted(file))
    {
        poke_data = _browser_poke_file_data_new(browser,
                                                location, source,
                                                file,
                                                func,
                                                location_func,
                                                user_data);

        mount_operation = etk_mount_operation_new(widget);

        g_file_mount_enclosing_volume(th_file_get_file(file),
                                      G_MOUNT_MOUNT_NONE,
                                      mount_operation,
                                      NULL,
                                      _browser_poke_file_finish,
                                      poke_data);

        g_object_unref(mount_operation);
    }
    else
    {
        if (location_func != NULL)
            location_func(browser, location, source, file, NULL, user_data);

        if (func != NULL)
            func(browser, source, file, NULL, user_data);
    }
}

static void _browser_poke_shortcut_file_finish(GFile *location,
                                               ThunarFile *file,
                                               GError *error,
                                               gpointer user_data)
{
    PokeFileData *poke_data = user_data;

    e_return_if_fail(G_IS_FILE(location));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARFILE(poke_data->file));

    if (error == NULL)
    {
        _browser_poke_file_internal(poke_data->browser,
                                    poke_data->location,
                                    poke_data->source,
                                    file,
                                    NULL,
                                    poke_data->func,
                                    poke_data->location_func,
                                    poke_data->user_data);
    }
    else
    {
        if (poke_data->location_func != NULL)
        {
            poke_data->location_func(poke_data->browser,
                                     poke_data->location,
                                     poke_data->source,
                                     NULL,
                                     error,
                                     poke_data->user_data);
        }

        if (poke_data->func != NULL)
        {
            poke_data->func(poke_data->browser,
                            poke_data->source,
                            NULL,
                            error,
                            poke_data->user_data);
        }
    }

    _browser_poke_file_data_free(poke_data);
}

static void _browser_poke_mountable_file_finish(GFile *location,
                                                ThunarFile *file,
                                                GError *error,
                                                gpointer user_data)
{
    e_return_if_fail(G_IS_FILE(location));
    e_return_if_fail(user_data != NULL);

    PokeFileData *poke_data = user_data;

    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARFILE(poke_data->file));
    e_return_if_fail(IS_THUNARFILE(poke_data->source));

    if (poke_data->location_func != NULL)
    {
        poke_data->location_func(poke_data->browser,
                                 poke_data->location,
                                 poke_data->source,
                                 file,
                                 error,
                                 poke_data->user_data);
    }

    if (poke_data->func != NULL)
    {
        poke_data->func(poke_data->browser,
                        poke_data->source,
                        file,
                        error,
                        poke_data->user_data);
    }

    _browser_poke_file_data_free(poke_data);
}

static void _browser_poke_mountable_finish(GObject      *object,
                                           GAsyncResult *result,
                                           gpointer      user_data)
{
    PokeFileData *poke_data = user_data;
    GError       *error = NULL;
    GFile        *location;

    e_return_if_fail(G_IS_FILE(object));
    e_return_if_fail(G_IS_ASYNC_RESULT(result));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARFILE(poke_data->file));

    if (!g_file_mount_mountable_finish(G_FILE(object), result, &error))
    {
        if (error->domain == G_IO_ERROR)
        {
            if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
                g_clear_error(&error);
        }
    }

    if (error == NULL)
    {
        th_file_reload(poke_data->file);

        location = th_file_get_target_location(poke_data->file);

        /* resolve the ThunarFile for the target location asynchronously
         * and defer cleaning up the poke data until that has finished */
        th_file_get_async(location,
                          NULL,
                          _browser_poke_mountable_file_finish,
                          poke_data);

        g_object_unref(location);
    }
    else
    {
        if (poke_data->location_func != NULL)
        {
            poke_data->location_func(poke_data->browser,
                                     poke_data->location,
                                     poke_data->source,
                                     NULL,
                                     error,
                                     poke_data->user_data);
        }

        if (poke_data->func != NULL)
        {
            poke_data->func(poke_data->browser,
                            poke_data->source,
                            NULL,
                            error,
                            poke_data->user_data);
        }

        _browser_poke_file_data_free(poke_data);
    }

    g_clear_error(&error);
}

static void _browser_poke_file_finish(GObject *object, GAsyncResult *result,
                                      gpointer user_data)
{
    PokeFileData *poke_data = user_data;
    GError       *error = NULL;

    e_return_if_fail(G_IS_FILE(object));
    e_return_if_fail(G_IS_ASYNC_RESULT(result));
    e_return_if_fail(user_data != NULL);
    e_return_if_fail(THUNAR_IS_BROWSER(poke_data->browser));
    e_return_if_fail(IS_THUNARFILE(poke_data->file));

    if (!g_file_mount_enclosing_volume_finish(G_FILE(object), result, &error))
    {
        if (error->domain == G_IO_ERROR)
        {
            if (error->code == G_IO_ERROR_ALREADY_MOUNTED)
                g_clear_error(&error);
        }
    }

    if (error == NULL)
        th_file_reload(poke_data->file);

    if (poke_data->location_func != NULL)
    {
        if (error == NULL)
        {
            poke_data->location_func(poke_data->browser,
                                     poke_data->location,
                                     poke_data->source,
                                     poke_data->file,
                                     NULL,
                                     poke_data->user_data);
        }
        else
        {
            poke_data->location_func(poke_data->browser,
                                     poke_data->location,
                                     poke_data->source,
                                     NULL,
                                     error,
                                     poke_data->user_data);
        }
    }

    if (poke_data->func != NULL)
    {
        if (error == NULL)
        {
            poke_data->func(poke_data->browser,
                            poke_data->source,
                            poke_data->file,
                            NULL,
                            poke_data->user_data);
        }
        else
        {
            poke_data->func(poke_data->browser,
                            poke_data->source,
                            NULL,
                            error,
                            poke_data->user_data);
        }
    }

    g_clear_error(&error);

    _browser_poke_file_data_free(poke_data);
}


