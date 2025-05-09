/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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
#include "iconrender.h"

#include "iconfactory.h"
#include "pixbuf-ext.h"
#include "clipboard.h"
#include "gdk-ext.h"

static void iconrender_finalize(GObject *object);
static void iconrender_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec);
static void iconrender_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec);
static void iconrender_get_preferred_width(GtkCellRenderer *renderer,
                                           GtkWidget *widget,
                                           gint *minimum,
                                           gint *natural);
static void iconrender_get_preferred_height(GtkCellRenderer *renderer,
                                            GtkWidget *widget,
                                            gint *minimum,
                                            gint *natural);
static void iconrender_render(GtkCellRenderer *renderer,
                              cairo_t *cr,
                              GtkWidget *widget,
                              const GdkRectangle *background_area,
                              const GdkRectangle *cell_area,
                              GtkCellRendererState flags);
static void _iconrender_color_insensitive(cairo_t *cr, GtkWidget *widget);
static void _iconrender_color_lighten(cairo_t *cr, GtkWidget *widget);
static void _iconrender_color_selected(cairo_t *cr, GtkWidget *widget);

// IconRenderer ---------------------------------------------------------------

enum
{
    PROP_0,
    PROP_DROP_FILE,
    PROP_FILE,
    PROP_EMBLEMS,
    PROP_FOLLOW_STATE,
    PROP_SIZE,
};

G_DEFINE_TYPE(IconRenderer, iconrender, GTK_TYPE_CELL_RENDERER)

static void iconrender_class_init(IconRendererClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = iconrender_finalize;
    gobject_class->get_property = iconrender_get_property;
    gobject_class->set_property = iconrender_set_property;

    GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS(klass);
    renderer_class->get_preferred_width = iconrender_get_preferred_width;
    renderer_class->get_preferred_height = iconrender_get_preferred_height;
    renderer_class->render = iconrender_render;

    /**
     * IconRenderer:drop-file:
     *
     * The file which should be rendered in the drop
     * accept state.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_DROP_FILE,
                                    g_param_spec_object(
                                        "drop-file",
                                        "drop-file",
                                        "drop-file",
                                        TYPE_THUNARFILE,
                                        E_PARAM_READWRITE));

    /**
     * IconRenderer:file:
     *
     * The file whose icon to render.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_FILE,
                                    g_param_spec_object(
                                        "file",
                                        "file",
                                        "file",
                                        TYPE_THUNARFILE,
                                        E_PARAM_READWRITE));

    /**
     * IconRenderer:emblems:
     *
     * Specifies whether to render emblems in addition to the file icons.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_EMBLEMS,
                                    g_param_spec_boolean(
                                        "emblems",
                                        "emblems",
                                        "emblems",
                                        false,
                                        G_PARAM_CONSTRUCT
                                        | E_PARAM_READWRITE));

    /**
     * IconRenderer:follow-state:
     *
     * Specifies whether the icon renderer should render icons
     * based on the selection state of the items. This is necessary
     * for #ExoIconView, which doesn't draw any item state indicators
     * itself.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_FOLLOW_STATE,
                                    g_param_spec_boolean(
                                        "follow-state",
                                        "follow-state",
                                        "follow-state",
                                        FALSE,
                                        E_PARAM_READWRITE));

    /**
     * IconRenderer:size:
     *
     * The size at which icons should be rendered by this
     * #IconRenderer instance.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_SIZE,
                                    g_param_spec_enum(
                                        "size",
                                        "size",
                                        "size",
                                        THUNAR_TYPE_ICON_SIZE,
                                        THUNAR_ICON_SIZE_16,
                                        G_PARAM_CONSTRUCT
                                        | E_PARAM_READWRITE));
}

static void iconrender_init(IconRenderer *icon_renderer)
{
    // use 1px padding
    gtk_cell_renderer_set_padding(GTK_CELL_RENDERER(icon_renderer), 1, 1);
}

static void iconrender_finalize(GObject *object)
{
    IconRenderer *icon_renderer = ICONRENDERER(object);

    // free the icon data
    if (icon_renderer->drop_file != NULL)
        g_object_unref(G_OBJECT(icon_renderer->drop_file));

    if (icon_renderer->file != NULL)
        g_object_unref(G_OBJECT(icon_renderer->file));

    G_OBJECT_CLASS(iconrender_parent_class)->finalize(object);
}

static void iconrender_get_property(GObject *object, guint prop_id,
                                    GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    IconRenderer *icon_renderer = ICONRENDERER(object);

    switch (prop_id)
    {
    case PROP_DROP_FILE:
        g_value_set_object(value, icon_renderer->drop_file);
        break;

    case PROP_FILE:
        g_value_set_object(value, icon_renderer->file);
        break;

    case PROP_EMBLEMS:
        g_value_set_boolean(value, icon_renderer->emblems);
        break;

    case PROP_FOLLOW_STATE:
        g_value_set_boolean(value, icon_renderer->follow_state);
        break;

    case PROP_SIZE:
        g_value_set_enum(value, icon_renderer->size);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iconrender_set_property(GObject *object, guint prop_id,
                                    const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    IconRenderer *icon_renderer = ICONRENDERER(object);

    switch (prop_id)
    {
    case PROP_DROP_FILE:
        if (icon_renderer->drop_file != NULL)
            g_object_unref(G_OBJECT(icon_renderer->drop_file));
        icon_renderer->drop_file =(gpointer) g_value_dup_object(value);
        break;

    case PROP_FILE:
        if (icon_renderer->file != NULL)
            g_object_unref(G_OBJECT(icon_renderer->file));
        icon_renderer->file =(gpointer) g_value_dup_object(value);
        break;

    case PROP_EMBLEMS:
        icon_renderer->emblems = g_value_get_boolean(value);
        break;

    case PROP_FOLLOW_STATE:
        icon_renderer->follow_state = g_value_get_boolean(value);
        break;

    case PROP_SIZE:
        icon_renderer->size = g_value_get_enum(value);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iconrender_get_preferred_width(GtkCellRenderer *renderer,
                                           GtkWidget *widget, gint *minimum,
                                           gint *natural)
{
    (void) widget;

    IconRenderer *icon_renderer = ICONRENDERER(renderer);
    int xpad;

    gtk_cell_renderer_get_padding(renderer, &xpad, NULL);

    if (minimum)
        *minimum =(gint) xpad * 2 + icon_renderer->size;

    if (natural)
        *natural =(gint) xpad * 2 + icon_renderer->size;
}

static void iconrender_get_preferred_height(GtkCellRenderer *renderer,
                                            GtkWidget *widget, gint *minimum,
                                            gint *natural)
{
    (void) widget;

    IconRenderer *icon_renderer = ICONRENDERER(renderer);
    int ypad;

    gtk_cell_renderer_get_padding(renderer, NULL, &ypad);

    if (minimum)
        *minimum =(gint) ypad * 2 + icon_renderer->size;

    if (natural)
        *natural =(gint) ypad * 2 + icon_renderer->size;
}

static void iconrender_render(GtkCellRenderer *renderer,
                              cairo_t *cr, GtkWidget *widget,
                              const GdkRectangle *background_area,
                              const GdkRectangle *cell_area,
                              GtkCellRendererState flags)
{
    (void) background_area;

    ClipboardManager *clipboard;
    FileIconState    icon_state;
    IconRenderer     *icon_renderer = ICONRENDERER(renderer);
    IconFactory      *icon_factory;
    GtkIconTheme     *icon_theme;
    GdkRectangle     icon_area;
    GdkRectangle     clip_area;
    GdkPixbuf        *icon;
    GdkPixbuf        *temp;
    gdouble          alpha;
    gboolean         color_selected;
    gboolean         color_lighten;
    gboolean         is_expanded;

    if (icon_renderer->file == NULL)
        return;

    if (!gdk_cairo_get_clip_rectangle(cr, &clip_area))
        return;

    g_object_get(renderer, "is-expanded", &is_expanded, NULL);

    // determine the icon state
    icon_state = (icon_renderer->drop_file != icon_renderer->file)
                  ? is_expanded
                        ? FILE_ICON_STATE_OPEN
                        : FILE_ICON_STATE_DEFAULT
                  : FILE_ICON_STATE_DROP;

    // load the main icon
    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(widget));
    icon_factory = iconfact_get_for_icon_theme(icon_theme);
    icon = iconfact_load_file_icon(icon_factory,
                                   icon_renderer->file,
                                   icon_state,
                                   icon_renderer->size);
    if (icon == NULL)
    {
        g_object_unref(G_OBJECT(icon_factory));
        return;
    }

    // pre-light the item if we're dragging about it
    if (icon_state == FILE_ICON_STATE_DROP)
        flags |= GTK_CELL_RENDERER_PRELIT;

    // determine the real icon size
    icon_area.width = gdk_pixbuf_get_width(icon);
    icon_area.height = gdk_pixbuf_get_height(icon);

    // scale down the icon on-demand
    if (icon_area.width > cell_area->width
        || icon_area.height > cell_area->height)
    {
        // scale down to fit
        temp = pixbuf_scale_down(icon,
                                 TRUE,
                                 MAX(1, cell_area->width),
                                 MAX(1, cell_area->height));
        g_object_unref(G_OBJECT(icon));
        icon = temp;

        // determine the icon dimensions again
        icon_area.width = gdk_pixbuf_get_width(icon);
        icon_area.height = gdk_pixbuf_get_height(icon);
    }

    icon_area.x = cell_area->x +(cell_area->width - icon_area.width) / 2;
    icon_area.y = cell_area->y +(cell_area->height - icon_area.height) / 2;

    // bools for cairo transformations
    color_selected = (flags & GTK_CELL_RENDERER_SELECTED) != 0
                      && icon_renderer->follow_state;
    color_lighten = (flags & GTK_CELL_RENDERER_PRELIT) != 0
                     && icon_renderer->follow_state;

    // check whether the icon is affected by the expose event
    if (gdk_rectangle_intersect(&clip_area, &icon_area, NULL))
    {
        // use a translucent icon to represent cutted
        // and hidden files to the user
        clipboard = clipman_get_for_display(gtk_widget_get_display(widget));
        if (clipman_has_cutted_file(clipboard, icon_renderer->file))
        {
            // 50% translucent for cutted files
            alpha = 0.50;
        }
        else if (th_file_is_hidden(icon_renderer->file))
        {
            // 75% translucent for hidden files
            alpha = 0.75;
        }
        else
        {
            alpha = 1.00;
        }

        g_object_unref(G_OBJECT(clipboard));

        // render the invalid parts of the icon
        edk_cairo_set_source_pixbuf(cr, icon, icon_area.x, icon_area.y);
        cairo_paint_with_alpha(cr, alpha);

        // check if we should render an insensitive icon
        if (gtk_widget_get_state_flags(widget) == GTK_STATE_FLAG_INSENSITIVE
            || !gtk_cell_renderer_get_sensitive(renderer))
            _iconrender_color_insensitive(cr,widget);

        // paint the lighten mask
        if (color_lighten)
            _iconrender_color_lighten(cr, widget);

        // paint the selected mask
        if (color_selected)
            _iconrender_color_selected(cr, widget);
    }

    // release the file's icon
    g_object_unref(G_OBJECT(icon));

    // release our reference on the icon factory
    g_object_unref(G_OBJECT(icon_factory));
}

static void _iconrender_color_insensitive(cairo_t *cr, GtkWidget *widget)
{
    cairo_pattern_t *source;
    GdkRGBA          *color;
    GtkStyleContext *context = gtk_widget_get_style_context(widget);

    cairo_save(cr);

    source = cairo_pattern_reference(cairo_get_source(cr));
    gtk_style_context_get(context,
                          GTK_STATE_FLAG_INSENSITIVE,
                          GTK_STYLE_PROPERTY_COLOR,
                          &color,
                          NULL);
    gdk_cairo_set_source_rgba(cr, color);
    gdk_rgba_free(color);
    cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);

    cairo_mask(cr, source);

    cairo_pattern_destroy(source);
    cairo_restore(cr);
}

static void _iconrender_color_lighten(cairo_t *cr, GtkWidget *widget)
{
    (void) widget;

    cairo_pattern_t *source;

    cairo_save(cr);

    source = cairo_pattern_reference(cairo_get_source(cr));
    cairo_set_source_rgb(cr, .15, .15, .15);
    cairo_set_operator(cr, CAIRO_OPERATOR_COLOR_DODGE);

    cairo_mask(cr, source);

    cairo_pattern_destroy(source);
    cairo_restore(cr);
}

static void _iconrender_color_selected(cairo_t *cr, GtkWidget *widget)
{
    cairo_pattern_t *source;
    GtkStateFlags   state;
    GdkRGBA         *color;
    GtkStyleContext *context = gtk_widget_get_style_context(widget);

    cairo_save(cr);

    source = cairo_pattern_reference(cairo_get_source(cr));
    state = gtk_widget_has_focus(widget)
            ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE;
    gtk_style_context_get(context,
                          state,
                          GTK_STYLE_PROPERTY_BACKGROUND_COLOR,
                          &color,
                          NULL);
    gdk_cairo_set_source_rgba(cr, color);
    gdk_rgba_free(color);
    cairo_set_operator(cr, CAIRO_OPERATOR_MULTIPLY);

    cairo_mask(cr, source);

    cairo_pattern_destroy(source);
    cairo_restore(cr);
}

GtkCellRenderer* iconrender_new()
{
    return g_object_new(TYPE_ICONRENDERER, NULL);
}


