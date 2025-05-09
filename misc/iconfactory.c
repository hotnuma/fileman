/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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
#include "iconfactory.h"

#include "pixbuf-ext.h"
#include "utils.h"

// the timeout until the sweeper is run (in seconds)
#define ICONFACTORY_SWEEP_TIMEOUT (30)

typedef struct _IconKey IconKey;

// IconFactory ----------------------------------------------------------------

static void iconfact_dispose(GObject *object);
static void iconfact_finalize(GObject *object);
static void iconfact_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec);
static void iconfact_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec);

// Signal ---------------------------------------------------------------------

static gboolean _iconfact_changed(GSignalInvocationHint *ihint,
                                  guint n_param_values,
                                  const GValue *param_values,
                                  gpointer user_data);

// IconKey --------------------------------------------------------------------

static gboolean _iconfact_sweep_timer(gpointer user_data);
static gboolean _iconkey_check_sweep(IconKey *key, GdkPixbuf *pixbuf);
static void _iconfact_sweep_timer_destroy(gpointer user_data);
static guint _iconkey_hash(gconstpointer data);
static gboolean _iconkey_equal(gconstpointer a, gconstpointer b);
static void _iconkey_free(gpointer data);

// Look up --------------------------------------------------------------------

static GdkPixbuf* _iconfact_load_fallback(IconFactory *factory, gint size);
static GdkPixbuf* _iconfact_lookup_icon(IconFactory *factory,
                                        const gchar *name, gint size,
                                        gboolean wants_default);
static GdkPixbuf* _iconfact_load_from_file(IconFactory *factory,
                                           const gchar *path, gint size);

// Globals --------------------------------------------------------------------

struct _IconKey
{
    gchar   *name;
    gint    size;
};

typedef struct
{
    FileIconState icon_state;

    gint        icon_size;
    guint       stamp;
    GdkPixbuf   *icon;

} IconStore;

static GQuark _iconfact_quark = 0;
static GQuark _iconfact_store_quark = 0;

// IconFactory ----------------------------------------------------------------

enum
{
    PROP_0,
    PROP_ICON_THEME,
};

struct _IconFactory
{
    GObject             __parent__;

    GHashTable          *icon_cache;
    GtkIconTheme        *icon_theme;

    guint               sweep_timer_id;
    gulong              changed_hook_id;

    // stamp that gets bumped when the theme changes
    guint               theme_stamp;
};

struct _IconFactoryClass
{
    GObjectClass        __parent__;
};

G_DEFINE_TYPE(IconFactory, iconfact, G_TYPE_OBJECT)

static void iconfact_class_init(IconFactoryClass *klass)
{
    _iconfact_store_quark =
            g_quark_from_static_string("thunar-icon-factory-store");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = iconfact_dispose;
    gobject_class->finalize = iconfact_finalize;
    gobject_class->get_property = iconfact_get_property;
    gobject_class->set_property = iconfact_set_property;

    // the GtkIconTheme on which the given IconFactory instance operates on.
    g_object_class_install_property(gobject_class,
                                    PROP_ICON_THEME,
                                    g_param_spec_object(
                                        "icon-theme",
                                        "icon-theme",
                                        "icon-theme",
                                        GTK_TYPE_ICON_THEME,
                                        E_PARAM_READABLE));
}

static void iconfact_init(IconFactory *factory)
{
    /* connect emission hook for the "changed" signal on the GtkIconTheme
     * class. We use the emission hook way here, because that way we can make
     * sure that the icon cache is definetly cleared before any other part of
     * the application gets notified about the icon theme change. */
    factory->changed_hook_id = g_signal_add_emission_hook(
                            g_signal_lookup("changed", GTK_TYPE_ICON_THEME),
                            0,
                            _iconfact_changed,
                            factory,
                            NULL);

    // allocate the hash table for the icon cache
    factory->icon_cache = g_hash_table_new_full(_iconkey_hash,
                                                _iconkey_equal,
                                                _iconkey_free,
                                                g_object_unref);
}

static void iconfact_dispose(GObject *object)
{
    IconFactory *factory = ICONFACTORY(object);

    e_return_if_fail(IS_ICONFACTORY(factory));

    if (factory->sweep_timer_id != 0)
        g_source_remove(factory->sweep_timer_id);

    G_OBJECT_CLASS(iconfact_parent_class)->dispose(object);
}

static void iconfact_finalize(GObject *object)
{
    IconFactory *factory = ICONFACTORY(object);

    e_return_if_fail(IS_ICONFACTORY(factory));

    // clear the icon cache hash table
    g_hash_table_destroy(factory->icon_cache);

    // remove the "changed" emission hook from the GtkIconTheme class
    g_signal_remove_emission_hook(g_signal_lookup("changed",
                                                  GTK_TYPE_ICON_THEME),
                                  factory->changed_hook_id);

    // disconnect from the associated icon theme(if any)
    if (factory->icon_theme != NULL)
    {
        g_object_set_qdata(G_OBJECT(factory->icon_theme),
                           _iconfact_quark, NULL);
        g_object_unref(G_OBJECT(factory->icon_theme));
    }

    G_OBJECT_CLASS(iconfact_parent_class)->finalize(object);
}

static void iconfact_get_property(GObject *object, guint prop_id,
                                  GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    IconFactory *factory = ICONFACTORY(object);

    switch (prop_id)
    {
    case PROP_ICON_THEME:
        g_value_set_object(value, factory->icon_theme);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iconfact_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) object;
    (void) value;
    (void) pspec;

    //IconFactory *factory = ICONFACTORY(object);

    switch (prop_id)
    {
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

// Signal ---------------------------------------------------------------------

static gboolean _iconfact_changed(GSignalInvocationHint *ihint,
                                  guint n_param_values,
                                  const GValue *param_values,
                                  gpointer user_data)
{
    (void) ihint;
    (void) n_param_values;
    (void) param_values;

    IconFactory *factory = ICONFACTORY(user_data);

    // drop all items from the icon cache
    g_hash_table_remove_all(factory->icon_cache);

    // bump the stamp so all file icons are reloaded
    factory->theme_stamp++;

    // keep the emission hook alive
    return TRUE;
}

// IconStore ------------------------------------------------------------------

static void _iconstore_free(gpointer data)
{
    IconStore *store = data;

    if (store->icon != NULL)
        g_object_unref(store->icon);

    g_slice_free(IconStore, store);
}

// IconKey --------------------------------------------------------------------

static guint _iconkey_hash(gconstpointer data)
{
    const IconKey *key = data;
    const gchar         *p;
    guint                h;

    h =(guint) key->size << 5;

    for (p = key->name; *p != '\0'; ++p)
        h =(h << 5) - h + *p;

    return h;
}

static gboolean _iconkey_equal(gconstpointer a, gconstpointer b)
{
    const IconKey *a_key = a;
    const IconKey *b_key = b;

    // compare sizes first
    if (a_key->size != b_key->size)
        return FALSE;

    // do a full string comparison on the names
    return (g_strcmp0(a_key->name, b_key->name) == 0);
}

static void _iconkey_free(gpointer data)
{
    IconKey *key = data;

    g_free(key->name);
    g_slice_free(IconKey, key);
}

// Lookup Icon ----------------------------------------------------------------

static GdkPixbuf* _iconfact_lookup_icon(IconFactory *factory,
                                        const gchar *name, gint size,
                                        gboolean wants_default)
{
    IconKey  lookup_key;
    IconKey *key;
    GtkIconInfo   *icon_info;
    GdkPixbuf     *pixbuf = NULL;

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);
    e_return_val_if_fail(name != NULL && *name != '\0', NULL);
    e_return_val_if_fail(size > 0, NULL);

    // prepare the lookup key
    lookup_key.name =(gchar *) name;
    lookup_key.size = size;

    // check if we already have a cached version of the icon
    if (!g_hash_table_lookup_extended(factory->icon_cache,
                                      &lookup_key, NULL, (gpointer) &pixbuf))
    {
        // check if we have to load a file instead of a themed icon
        if (g_path_is_absolute(name))
        {
            // load the file directly
            pixbuf = _iconfact_load_from_file(factory, name, size);
        }
        else
        {
            // FIXME: is there a better approach?
            if (g_strcmp0(name, "inode-directory") == 0)
                name = "folder";

            // check if the icon theme contains an icon of that name
            icon_info = gtk_icon_theme_lookup_icon(
                                            factory->icon_theme,
                                            name,
                                            size,
                                            GTK_ICON_LOOKUP_FORCE_SIZE);
            if (icon_info != NULL)
            {
                // try to load the pixbuf from the icon info
                pixbuf = gtk_icon_info_load_icon(icon_info, NULL);

                // cleanup
                g_object_unref(icon_info);
            }
        }

        // use fallback icon if no pixbuf could be loaded
        if (pixbuf == NULL)
        {
            // check if we are allowed to return the fallback icon
            if (!wants_default)
                return NULL;
            else
                return _iconfact_load_fallback(factory, size);
        }

        // generate a key for the new cached icon
        key = g_slice_new(IconKey);
        key->size = size;
        key->name = g_strdup(name);

        // insert the new icon into the cache
        g_hash_table_insert(factory->icon_cache, key, pixbuf);
    }

    // schedule the sweeper
    if (factory->sweep_timer_id == 0)
    {
        factory->sweep_timer_id =
            g_timeout_add_seconds_full(G_PRIORITY_LOW,
                                       ICONFACTORY_SWEEP_TIMEOUT,
                                       _iconfact_sweep_timer,
                                       factory,
                                       _iconfact_sweep_timer_destroy);
    }

    return GDK_PIXBUF(g_object_ref(G_OBJECT(pixbuf)));
}

static GdkPixbuf* _iconfact_load_fallback(IconFactory *factory, gint size)
{
    return _iconfact_lookup_icon(factory, "text-x-generic", size, FALSE);
}

static GdkPixbuf* _iconfact_load_from_file(IconFactory *factory,
                                           const gchar *path, gint size)
{
    GdkPixbuf *pixbuf;
    GdkPixbuf *tmp;
    gboolean   needs_frame;
    gint       max_width;
    gint       max_height;
    gint       width;
    gint       height;

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);

    // try to load the image from the file
    pixbuf = gdk_pixbuf_new_from_file(path, NULL);

    if (pixbuf == NULL)
        return NULL;

    // determine the dimensions of the pixbuf
    width = gdk_pixbuf_get_width(pixbuf);
    height = gdk_pixbuf_get_height(pixbuf);

    needs_frame = FALSE;

    // be sure to make framed pixbufs fit into the size
    if (needs_frame)
    {
        max_width = size - (3 + 6);
        max_height = size - (3 + 6);
    }
    else
    {
        max_width = size;
        max_height = size;
    }

    // scale down the icon(if required)
    if (width > max_width || height > max_height)
    {
        // scale down to the required size
        tmp = pixbuf_scale_down(pixbuf,
                                TRUE,
                                MAX(1, max_height),
                                MAX(1, max_height));
        g_object_unref(G_OBJECT(pixbuf));
        pixbuf = tmp;
    }

    return pixbuf;
}

static gboolean _iconfact_sweep_timer(gpointer user_data)
{
    IconFactory *factory = ICONFACTORY(user_data);

    e_return_val_if_fail(IS_ICONFACTORY(factory), FALSE);

    UTIL_THREADS_ENTER

    // ditch all icons whose ref_count is 1
    g_hash_table_foreach_remove(
                        factory->icon_cache,
                        (GHRFunc) (void(*) (void)) _iconkey_check_sweep,
                        factory);

    UTIL_THREADS_LEAVE

    return FALSE;
}

static gboolean _iconkey_check_sweep(IconKey *key, GdkPixbuf *pixbuf)
{
    (void) key;

    return(G_OBJECT(pixbuf)->ref_count == 1);
}

static void _iconfact_sweep_timer_destroy(gpointer user_data)
{
    ICONFACTORY(user_data)->sweep_timer_id = 0;
}

// Public ---------------------------------------------------------------------

IconFactory* iconfact_get_default()
{
    // g_object_unref when unneeded

    static IconFactory *factory = NULL;

    if (factory == NULL)
    {
        factory = iconfact_get_for_icon_theme(gtk_icon_theme_get_default());
        g_object_add_weak_pointer(G_OBJECT(factory),(gpointer) &factory);

        return factory;
    }

    g_object_ref(G_OBJECT(factory));

    return factory;
}

IconFactory* iconfact_get_for_icon_theme(GtkIconTheme *icon_theme)
{
    // g_object_unref when unneeded

    IconFactory *factory;

    e_return_val_if_fail(GTK_IS_ICON_THEME(icon_theme), NULL);

    // generate the quark on-demand
    if (_iconfact_quark == 0)
        _iconfact_quark = g_quark_from_static_string("thunar-icon-factory");

    // check if the given icon theme already knows about an icon factory
    factory = g_object_get_qdata(G_OBJECT(icon_theme), _iconfact_quark);

    if (factory == NULL)
    {
        // allocate a new factory and connect it to the icon theme
        factory = g_object_new(TYPE_ICONFACTORY, NULL);
        factory->icon_theme =
                GTK_ICON_THEME(g_object_ref(G_OBJECT(icon_theme)));
        g_object_set_qdata(G_OBJECT(factory->icon_theme),
                           _iconfact_quark, factory);
        return factory;

    }

    g_object_ref(G_OBJECT(factory));

    return factory;
}

/* Looks up the icon named @name in the icon theme associated with factory
 * and returns a pixbuf for the icon at the given size. This function will
 * return a default fallback icon if the requested icon could not be found
 * and wants_default is TRUE, else NULL will be returned in that case. */
GdkPixbuf* iconfact_load_icon(IconFactory *factory, const gchar *name,
                              gint size, gboolean wants_default)
{
    // g_object_unref when unneeded

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);
    e_return_val_if_fail(size > 0, NULL);

    /* cannot happen unless there's no XSETTINGS manager
     * for the default screen, but just in case...
     */
    if (!name ||!*name)
    {
        // check if the caller will happly accept the fallback icon
        if (wants_default)
            return _iconfact_load_fallback(factory, size);
        else
            return NULL;
    }

    // lookup the icon
    return _iconfact_lookup_icon(factory, name, size, wants_default);
}

GdkPixbuf* iconfact_load_file_icon(IconFactory *factory,
                                   ThunarFile *file,
                                   FileIconState icon_state,
                                   gint icon_size)
{
    // g_object_unref when unneeded

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);
    e_return_val_if_fail(IS_THUNARFILE(file), NULL);
    e_return_val_if_fail(icon_size > 0, NULL);


    // check if we have a stored icon on the file and it is still valid
    IconStore *store = g_object_get_qdata(G_OBJECT(file),
                                                _iconfact_store_quark);
    if (store != NULL
        && store->icon_state == icon_state
        && store->icon_size == icon_size
        && store->stamp == factory->theme_stamp)
    {
        return g_object_ref(store->icon);
    }

    GdkPixbuf *icon = NULL;

    // check if we have a custom icon for this file
    const gchar *custom_icon = th_file_get_custom_icon(file);

    if (custom_icon != NULL)
    {
        // try to load the icon
        icon = _iconfact_lookup_icon(factory, custom_icon, icon_size, FALSE);

        if (icon != NULL)
            return icon;
    }


    // lookup the icon name for the icon in the given state and load the icon
    if (icon == NULL)
    {
        const gchar *icon_name = th_file_get_icon_name(file,
                                                       icon_state,
                                                       factory->icon_theme);
        icon = iconfact_load_icon(factory, icon_name, icon_size, TRUE);
    }

    if (icon == NULL)
        return NULL;

    store = g_slice_new(IconStore);
    store->icon_size = icon_size;
    store->icon_state = icon_state;
    store->stamp = factory->theme_stamp;
    store->icon = g_object_ref(icon);

    g_object_set_qdata_full(G_OBJECT(file),
                            _iconfact_store_quark,
                            store, _iconstore_free);

    return icon;
}

void iconfact_clear_pixmap_cache(ThunarFile *file)
{
    // Unset the pixmap cache on a file to force a reload on the next request.

    e_return_if_fail(IS_THUNARFILE(file));

    // unset the data
    if (_iconfact_store_quark != 0)
        g_object_set_qdata(G_OBJECT(file), _iconfact_store_quark, NULL);
}


