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

#include <config.h>
#include <iconfactory.h>

#include <pixbuf_ext.h>
#include <utils.h>

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
static GdkPixbuf* _iconfact_lookup_icon(IconFactory *factory, const gchar *name,
                                        gint size, gboolean wants_default);
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
    ThunarFileIconState icon_state;
    //ThunarFileThumbState thumb_state;

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
    PROP_THUMBNAIL_MODE,
    PROP_THUMBNAIL_DRAW_FRAMES,
    PROP_THUMBNAIL_SIZE,
};

struct _IconFactoryClass
{
    GObjectClass        __parent__;
};

struct _IconFactory
{
    GObject             __parent__;

    GHashTable          *icon_cache;
    GtkIconTheme        *icon_theme;

    ThunarThumbnailMode thumbnail_mode;
    gboolean            thumbnail_draw_frames;
    ThunarThumbnailSize thumbnail_size;

    guint               sweep_timer_id;
    gulong              changed_hook_id;

    // stamp that gets bumped when the theme changes
    guint               theme_stamp;
};

G_DEFINE_TYPE(IconFactory, iconfact, G_TYPE_OBJECT)

static void iconfact_class_init(IconFactoryClass *klass)
{
    _iconfact_store_quark = g_quark_from_static_string("thunar-icon-factory-store");

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = iconfact_dispose;
    gobject_class->finalize = iconfact_finalize;
    gobject_class->get_property = iconfact_get_property;
    gobject_class->set_property = iconfact_set_property;

    /**
     * IconFactory:icon-theme:
     *
     * The #GtkIconTheme on which the given #IconFactory instance operates
     * on.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_ICON_THEME,
                                    g_param_spec_object(
                                        "icon-theme",
                                        "icon-theme",
                                        "icon-theme",
                                        GTK_TYPE_ICON_THEME,
                                        E_PARAM_READABLE));

    /**
     * IconFactory:thumbnail-mode:
     *
     * Whether this #IconFactory will try to generate and load thumbnails
     * when loading icons for #ThunarFile<!---->s.
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_THUMBNAIL_MODE,
                                    g_param_spec_enum(
                                        "thumbnail-mode",
                                        "thumbnail-mode",
                                        "thumbnail-mode",
                                        THUNAR_TYPE_THUMBNAIL_MODE,
                                        THUNAR_THUMBNAIL_MODE_NEVER,
                                        E_PARAM_READWRITE));

    /**
     * IconFactory:thumbnail-size:
     *
     * Size of the thumbnails to load
     **/
    g_object_class_install_property(gobject_class,
                                    PROP_THUMBNAIL_SIZE,
                                    g_param_spec_enum(
                                        "thumbnail-size",
                                        "thumbnail-size",
                                        "thumbnail-size",
                                        THUNAR_TYPE_THUMBNAIL_SIZE,
                                        THUNAR_THUMBNAIL_SIZE_NORMAL,
                                        E_PARAM_READWRITE));
}

static void iconfact_init(IconFactory *factory)
{
    factory->thumbnail_mode = THUNAR_THUMBNAIL_MODE_NEVER;
    factory->thumbnail_size = THUNAR_THUMBNAIL_SIZE_NORMAL;

    /* connect emission hook for the "changed" signal on the GtkIconTheme class. We use the emission
     * hook way here, because that way we can make sure that the icon cache is definetly cleared
     * before any other part of the application gets notified about the icon theme change.
     */
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

    if (G_UNLIKELY(factory->sweep_timer_id != 0))
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
    g_signal_remove_emission_hook(g_signal_lookup("changed", GTK_TYPE_ICON_THEME), factory->changed_hook_id);

    // disconnect from the associated icon theme(if any)
    if (G_LIKELY(factory->icon_theme != NULL))
    {
        g_object_set_qdata(G_OBJECT(factory->icon_theme), _iconfact_quark, NULL);
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

    case PROP_THUMBNAIL_MODE:
        g_value_set_enum(value, factory->thumbnail_mode);
        break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
        g_value_set_boolean(value, factory->thumbnail_draw_frames);
        break;

    case PROP_THUMBNAIL_SIZE:
        g_value_set_enum(value, factory->thumbnail_size);
        break;

    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, prop_id, pspec);
        break;
    }
}

static void iconfact_set_property(GObject *object, guint prop_id,
                                  const GValue *value, GParamSpec *pspec)
{
    (void) pspec;

    IconFactory *factory = ICONFACTORY(object);

    switch (prop_id)
    {
    case PROP_THUMBNAIL_MODE:
        factory->thumbnail_mode = g_value_get_enum(value);
        break;

    case PROP_THUMBNAIL_DRAW_FRAMES:
        factory->thumbnail_draw_frames = g_value_get_boolean(value);
        break;

    case PROP_THUMBNAIL_SIZE:
        factory->thumbnail_size = g_value_get_enum(value);
        break;

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

    for(p = key->name; *p != '\0'; ++p)
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

static GdkPixbuf* _iconfact_lookup_icon(IconFactory *factory, const gchar *name,
                                        gint size, gboolean wants_default)
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
    if (!g_hash_table_lookup_extended(factory->icon_cache, &lookup_key, NULL,(gpointer) &pixbuf))
    {
        // check if we have to load a file instead of a themed icon
        if (G_UNLIKELY(g_path_is_absolute(name)))
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
            icon_info = gtk_icon_theme_lookup_icon(factory->icon_theme, name, size, GTK_ICON_LOOKUP_FORCE_SIZE);
            if (G_LIKELY(icon_info != NULL))
            {
                // try to load the pixbuf from the icon info
                pixbuf = gtk_icon_info_load_icon(icon_info, NULL);

                // cleanup
                g_object_unref(icon_info);
            }
        }

        // use fallback icon if no pixbuf could be loaded
        if (G_UNLIKELY(pixbuf == NULL))
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
    if (G_UNLIKELY(factory->sweep_timer_id == 0))
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

static GdkPixbuf* _iconfact_load_from_file(IconFactory *factory, const gchar *path,
                                           gint size)
{
    GdkPixbuf *pixbuf;
    //GdkPixbuf *frame;
    GdkPixbuf *tmp;
    gboolean   needs_frame;
    gint       max_width;
    gint       max_height;
    gint       width;
    gint       height;

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);

    // try to load the image from the file
    pixbuf = gdk_pixbuf_new_from_file(path, NULL);
    if (G_LIKELY(pixbuf != NULL))
    {
        // determine the dimensions of the pixbuf
        width = gdk_pixbuf_get_width(pixbuf);
        height = gdk_pixbuf_get_height(pixbuf);

        needs_frame = FALSE;

        //if (factory->thumbnail_draw_frames)
        //{
        //    /* check if we want to add a frame to the image(we really don't
        //     * want to do this for icons displayed in the details view).
        //     * */
        //    needs_frame =
        //        (strstr(path,
        //        G_DIR_SEPARATOR_S ".cache/thumbnails" G_DIR_SEPARATOR_S) != NULL)
        //        && (size >= 32)
        //        && thumbnail_needs_frame(pixbuf, width, height, size);
        //}

        // be sure to make framed thumbnails fit into the size
        if (G_LIKELY(needs_frame))
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
        if (G_LIKELY(width > max_width || height > max_height))
        {
            // scale down to the required size
            tmp = pixbuf_scale_down(pixbuf, TRUE, MAX(1, max_height), MAX(1, max_height));
            g_object_unref(G_OBJECT(pixbuf));
            pixbuf = tmp;
        }

        // add a frame around thumbnail(large) images
        //if (G_LIKELY(needs_frame))
        //{
        //    // add a frame to the thumbnail
        //    frame = thunar_icon_factory_get_thumbnail_frame();
        //    tmp = pixbuf_frame(pixbuf, frame, 4, 3, 5, 6);
        //    g_object_unref(G_OBJECT(pixbuf));
        //    pixbuf = tmp;
        //}
    }

    return pixbuf;
}

static gboolean _iconfact_sweep_timer(gpointer user_data)
{
    IconFactory *factory = ICONFACTORY(user_data);

    e_return_val_if_fail(IS_ICONFACTORY(factory), FALSE);

    UTIL_THREADS_ENTER

    // ditch all icons whose ref_count is 1
    g_hash_table_foreach_remove(factory->icon_cache,
                                (GHRFunc)(void(*)(void)) _iconkey_check_sweep,
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
    // g_object_unref when no longer needed

    static IconFactory *factory = NULL;

    if (G_UNLIKELY(factory == NULL))
    {
        factory = iconfact_get_for_icon_theme(gtk_icon_theme_get_default());
        g_object_add_weak_pointer(G_OBJECT(factory),(gpointer) &factory);
    }
    else
    {
        g_object_ref(G_OBJECT(factory));
    }

    return factory;
}

IconFactory* iconfact_get_for_icon_theme(GtkIconTheme *icon_theme)
{
    // g_object_unref when no longer needed

    IconFactory *factory;

    e_return_val_if_fail(GTK_IS_ICON_THEME(icon_theme), NULL);

    // generate the quark on-demand
    if (G_UNLIKELY(_iconfact_quark == 0))
        _iconfact_quark = g_quark_from_static_string("thunar-icon-factory");

    // check if the given icon theme already knows about an icon factory
    factory = g_object_get_qdata(G_OBJECT(icon_theme), _iconfact_quark);
    if (G_UNLIKELY(factory == NULL))
    {
        // allocate a new factory and connect it to the icon theme
        factory = g_object_new(TYPE_ICONFACTORY, NULL);
        factory->icon_theme = GTK_ICON_THEME(g_object_ref(G_OBJECT(icon_theme)));
        g_object_set_qdata(G_OBJECT(factory->icon_theme), _iconfact_quark, factory);

    }
    else
    {
        g_object_ref(G_OBJECT(factory));
    }

    return factory;
}

/* Looks up the icon named @name in the icon theme associated with factory
 * and returns a pixbuf for the icon at the given size. This function will
 * return a default fallback icon if the requested icon could not be found
 * and wants_default is TRUE, else NULL will be returned in that case. */
GdkPixbuf* iconfact_load_icon(IconFactory *factory, const gchar *name,
                              gint size, gboolean wants_default)
{
    // g_object_unref when no longer needed

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);
    e_return_val_if_fail(size > 0, NULL);

    /* cannot happen unless there's no XSETTINGS manager
     * for the default screen, but just in case...
     */
    if (G_UNLIKELY(!name ||!*name))
    {
        // check if the caller will happly accept the fallback icon
        if (G_LIKELY(wants_default))
            return _iconfact_load_fallback(factory, size);
        else
            return NULL;
    }

    // lookup the icon
    return _iconfact_lookup_icon(factory, name, size, wants_default);
}

GdkPixbuf* iconfact_load_file_icon(IconFactory *factory,
                                   ThunarFile *file,
                                   ThunarFileIconState icon_state,
                                   gint icon_size)
{
    // g_object_unref when no longer needed

    e_return_val_if_fail(IS_ICONFACTORY(factory), NULL);
    e_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    e_return_val_if_fail(icon_size > 0, NULL);


    // check if we have a stored icon on the file and it is still valid
    IconStore *store = g_object_get_qdata(G_OBJECT(file),
                                                _iconfact_store_quark);
    if (store != NULL
        && store->icon_state == icon_state
        && store->icon_size == icon_size
        && store->stamp == factory->theme_stamp)
    {
        //static int count = 0;
        //DPRINT("%d load from store\n", ++count);

        return g_object_ref(store->icon);
    }

    GdkPixbuf *icon = NULL;

    // check if we have a custom icon for this file
    const gchar *custom_icon = th_file_get_custom_icon(file);

    if (custom_icon != NULL)
    {
        // try to load the icon
        icon = _iconfact_lookup_icon(factory, custom_icon, icon_size, FALSE);
        if (G_LIKELY(icon != NULL))
            return icon;
    }

#if 0
    GInputStream    *stream;
    GtkIconInfo     *icon_info;
    const gchar     *thumbnail_path;
    GIcon           *gicon;

    // check if thumbnails are enabled and we can display a thumbnail for the item
    if (thunar_icon_factory_get_show_thumbnail(factory, file)
            && (thunar_file_is_regular(file) || thunar_file_is_directory(file)) )
    {
        // determine the preview icon first
        gicon = thunar_file_get_preview_icon(file);

        // check if we have a preview icon
        if (gicon != NULL)
        {
            if (G_IS_THEMED_ICON(gicon))
            {
                // we have a themed preview icon, look it up using the icon theme
                icon_info =
                    gtk_icon_theme_lookup_by_gicon(factory->icon_theme,
                                                    gicon, icon_size,
                                                    GTK_ICON_LOOKUP_USE_BUILTIN
                                                    | GTK_ICON_LOOKUP_FORCE_SIZE);

                // check if the lookup succeeded
                if (icon_info != NULL)
                {
                    // try to load the pixbuf from the icon info
                    icon = gtk_icon_info_load_icon(icon_info, NULL);
                    g_object_unref(icon_info);
                }
            }
            else if (G_IS_LOADABLE_ICON(gicon))
            {
                // we have a loadable icon, try to open it for reading
                stream = g_loadable_icon_load(G_LOADABLE_ICON(gicon), icon_size,
                                               NULL, NULL, NULL);

                // check if we have a valid input stream
                if (stream != NULL)
                {
                    // load the pixbuf from the stream
                    icon = gdk_pixbuf_new_from_stream_at_scale(stream, icon_size,
                            icon_size, TRUE,
                            NULL, NULL);

                    // destroy the stream
                    g_object_unref(stream);
                }
            }

            // return the icon if we have one
            if (icon != NULL)
                return icon;
        }
        else
        {
            /* we have no preview icon but the thumbnail should be ready. determine
             * the filename of the thumbnail */
            thumbnail_path = thunar_file_get_thumbnail_path(file, factory->thumbnail_size);

            // check if we have a valid path
            if (thumbnail_path != NULL)
            {
                // try to load the thumbnail
                icon = thunar_icon_factory_load_from_file(factory, thumbnail_path, icon_size);
            }
        }
    }
#endif

    // lookup the icon name for the icon in the given state and load the icon
    if (G_LIKELY(icon == NULL))
    {
        const gchar *icon_name = th_file_get_icon_name(file, icon_state, factory->icon_theme);
        icon = iconfact_load_icon(factory, icon_name, icon_size, TRUE);
    }

    if (G_LIKELY(icon != NULL))
    {
        store = g_slice_new(IconStore);
        store->icon_size = icon_size;
        store->icon_state = icon_state;
        store->stamp = factory->theme_stamp;
        //store->thumb_state = thunar_file_get_thumb_state(file);
        store->icon = g_object_ref(icon);

        g_object_set_qdata_full(G_OBJECT(file),
                                _iconfact_store_quark,
                                store, _iconstore_free);
    }

    return icon;
}

void iconfact_clear_pixmap_cache(ThunarFile *file)
{
    // Unset the pixmap cache on a file to force a reload on the next request.

    e_return_if_fail(THUNAR_IS_FILE(file));

    // unset the data
    if (_iconfact_store_quark != 0)
        g_object_set_qdata(G_OBJECT(file), _iconfact_store_quark, NULL);
}


