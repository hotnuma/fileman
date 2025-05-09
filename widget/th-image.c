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
#include "th-image.h"

#include "iconfactory.h"
#include "filemonitor.h"

static void th_image_finalize(GObject *object);
static void th_image_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void th_image_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);

static void _th_image_file_changed(FileMonitor *monitor, ThunarFile *file,
                                   ThunarImage *image);
static void _th_image_update(ThunarImage *image);

// ThunarImage ----------------------------------------------------------------

enum
{
    PROP_0,
    PROP_FILE,
};

struct _ThunarImage
{
    GtkImage __parent__;

    ThunarImagePrivate *priv;
};

struct _ThunarImageClass
{
    GtkImageClass __parent__;
};

struct _ThunarImagePrivate
{
    FileMonitor *monitor;
    ThunarFile *file;
};

G_DEFINE_TYPE_WITH_PRIVATE(ThunarImage, th_image, GTK_TYPE_IMAGE)


// creation / destruction -----------------------------------------------------

GtkWidget* th_image_new()
{
    return g_object_new(TYPE_THUNARIMAGE, NULL);
}

static void th_image_class_init(ThunarImageClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = th_image_finalize;
    gobject_class->get_property = th_image_get_property;
    gobject_class->set_property = th_image_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_FILE,
                                    g_param_spec_object(
                                        "file",
                                        "file",
                                        "file",
                                        TYPE_THUNARFILE,
                                        G_PARAM_READWRITE));
}

static void th_image_init(ThunarImage *image)
{
    image->priv = th_image_get_instance_private(image);
    image->priv->file = NULL;

    image->priv->monitor = filemon_get_default();

    g_signal_connect(image->priv->monitor, "file-changed",
                     G_CALLBACK(_th_image_file_changed), image);
}

static void th_image_finalize(GObject *object)
{
    ThunarImage *image = THUNARIMAGE(object);

    g_signal_handlers_disconnect_by_func(image->priv->monitor,
                                         _th_image_file_changed, image);
    g_object_unref(image->priv->monitor);

    th_image_set_file(image, NULL);

    G_OBJECT_CLASS(th_image_parent_class)->finalize(object);
}

static void th_image_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarImage *image = THUNARIMAGE(object);

    switch (prop_id)
    {
    case PROP_FILE:
        g_value_set_object(value, image->priv->file);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void th_image_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    ThunarImage *image = THUNARIMAGE(object);

    switch (prop_id)
    {
    case PROP_FILE:
        th_image_set_file(image, g_value_get_object(value));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void _th_image_file_changed(FileMonitor *monitor, ThunarFile *file,
                                   ThunarImage *image)
{
    e_return_if_fail(IS_FILEMONITOR(monitor));
    e_return_if_fail(IS_THUNARFILE(file));
    e_return_if_fail(IS_THUNARIMAGE(image));

    if (file == image->priv->file)
        _th_image_update(image);
}

static void _th_image_update(ThunarImage *image)
{
    e_return_if_fail(IS_THUNARIMAGE(image));

    if (!IS_THUNARFILE(image->priv->file))
        return;

    GdkScreen *screen = gtk_widget_get_screen(GTK_WIDGET(image));
    GtkIconTheme *icon_theme = gtk_icon_theme_get_for_screen(screen);
    IconFactory *icon_factory = iconfact_get_for_icon_theme(icon_theme);

    GdkPixbuf *icon = iconfact_load_file_icon(
                                        icon_factory,
                                        image->priv->file,
                                        FILE_ICON_STATE_DEFAULT, 48);

    gtk_image_set_from_pixbuf(GTK_IMAGE(image), icon);

    g_object_unref(icon_factory);
}


// public ---------------------------------------------------------------------

void th_image_set_file(ThunarImage *image, ThunarFile *file)
{
    e_return_if_fail(IS_THUNARIMAGE(image));

    if (image->priv->file != NULL)
    {
        if (image->priv->file == file)
            return;

        g_object_unref(image->priv->file);
    }

    if (file != NULL)
        image->priv->file = g_object_ref(file);
    else
        image->priv->file = NULL;

    _th_image_update(image);

    g_object_notify(G_OBJECT(image), "file");
}


