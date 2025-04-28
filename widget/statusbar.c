/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2011 Jannis Pohlmann <jannis@xfce.org>
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
#include "statusbar.h"

static void statusbar_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec);
static void _statusbar_set_text(Statusbar *statusbar, const gchar *text);

enum
{
    PROP_0,
    PROP_TEXT,
};

struct _Statusbar
{
    GtkStatusbar __parent__;
    guint context_id;
};

struct _StatusbarClass
{
    GtkStatusbarClass __parent__;
};

G_DEFINE_TYPE(Statusbar, statusbar, GTK_TYPE_STATUSBAR)


// creation / destruction -----------------------------------------------------

GtkWidget* statusbar_new()
{
    return g_object_new(TYPE_STATUSBAR, NULL);
}

static void statusbar_class_init(StatusbarClass *klass)
{
    static gboolean style_initialized = FALSE;

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->set_property = statusbar_set_property;

    g_object_class_install_property(gobject_class,
                                    PROP_TEXT,
                                    g_param_spec_string("text",
                                                        "text",
                                                        "text",
                                                        NULL,
                                                        E_PARAM_WRITABLE));

    if (style_initialized)
        return;

    gtk_widget_class_install_style_property(
        GTK_WIDGET_CLASS(gobject_class),
        g_param_spec_enum("shadow-type",
                          "shadow-type",
                          "type of shadow",
                          gtk_shadow_type_get_type(),
                          GTK_SHADOW_NONE,
                          G_PARAM_READWRITE));
}

static void statusbar_init(Statusbar *statusbar)
{
    statusbar->context_id =
        gtk_statusbar_get_context_id(GTK_STATUSBAR(statusbar), "Main text");

    // make the status thinner
    gtk_widget_set_margin_top(GTK_WIDGET(statusbar), 0);
    gtk_widget_set_margin_bottom(GTK_WIDGET(statusbar), 0);
}

static void statusbar_set_property(GObject *object, guint prop_id,
                                   const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    Statusbar *statusbar = STATUSBAR(object);

    switch (prop_id)
    {
    case PROP_TEXT:
        _statusbar_set_text(statusbar, g_value_get_string(value));
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void _statusbar_set_text(Statusbar *statusbar, const gchar *text)
{
    e_return_if_fail(IS_STATUSBAR(statusbar));
    e_return_if_fail(text != NULL);

    gtk_statusbar_pop(GTK_STATUSBAR(statusbar), statusbar->context_id);
    gtk_statusbar_push(GTK_STATUSBAR(statusbar), statusbar->context_id, text);
}


