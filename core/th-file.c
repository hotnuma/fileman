/*-
 * Copyright(c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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
#include <th-file.h>

#include <libxfce4ui/libxfce4ui.h>
#include <thx-file-info.h>
#include <application.h>
#include <chooser-dlg.h>
#include <filemon.h>
#include <gio-ext.h>
#include <gobject-ext.h>
#include <user.h>
#include <utils.h>
#include <dialogs.h>
#include <icon-factory.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <memory.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <gio/gio.h>

/* Dump the file cache every X second, set to 0 to disable */
#define DUMP_FILE_CACHE 0

/* Signal identifiers */
enum
{
    DESTROY,
    LAST_SIGNAL,
};

// Allocate -------------------------------------------------------------------

static void th_file_info_init(ThunarxFileInfoIface *iface);
static void th_file_dispose(GObject *object);
static void th_file_finalize(GObject *object);

// ThunarxFileInfo ------------------------------------------------------------

static gchar* th_file_info_get_name(ThunarxFileInfo *file_info);
static gchar* th_file_info_get_uri(ThunarxFileInfo *file_info);
static gchar* th_file_info_get_parent_uri(ThunarxFileInfo *file_info);
static gchar* th_file_info_get_uri_scheme(ThunarxFileInfo *file_info);
static gchar* th_file_info_get_mime_type(ThunarxFileInfo *file_info);
static gboolean th_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                           const gchar *mime_type);
static gboolean th_file_info_is_directory(ThunarxFileInfo *file_info);
static GFileInfo* th_file_info_get_file_info(ThunarxFileInfo *file_info);
static GFileInfo* th_file_info_get_filesystem_info(ThunarxFileInfo *file_info);
static GFile* th_file_info_get_location(ThunarxFileInfo *file_info);
static void th_file_info_changed(ThunarxFileInfo *file_info);

// Public ---------------------------------------------------------------------

static gboolean _th_file_load(ThunarFile *file, GCancellable *cancellable,
                              GError **error);
static void _th_file_info_clear(ThunarFile *file);
static void _th_file_info_reload(ThunarFile *file, GCancellable *cancellable);

static void _th_file_get_async_finish(GObject *object, GAsyncResult *result,
                                      gpointer user_data);

static gboolean _th_file_same_filesystem(const ThunarFile *file_a,
                                         const ThunarFile *file_b);

// Monitor --------------------------------------------------------------------

static void _th_file_monitor(GFileMonitor *monitor,
                             GFile *path,
                             GFile *other_path,
                             GFileMonitorEvent event_type,
                             gpointer user_data);
static void _th_file_monitor_moved(ThunarFile *file, GFile *renamed_file);
static void _th_file_watch_reconnect(ThunarFile *file);
static void _th_file_monitor_update(GFile *path, GFileMonitorEvent event_type);
static void _th_file_reload_parent(ThunarFile *file);
static void _th_file_watch_destroyed(gpointer data);

static ThunarUserManager    *_user_manager;
static GHashTable           *_file_cache;
static guint32              _effective_user_id;
static GQuark               _file_watch_quark;
static guint                _file_signals[LAST_SIGNAL];

G_LOCK_DEFINE_STATIC(_file_cache_mutex);
G_LOCK_DEFINE_STATIC(_file_content_type_mutex);
G_LOCK_DEFINE_STATIC(_file_rename_mutex);

#define FLAG_SET_THUMB_STATE(file,new_state) G_STMT_START{(file)->flags =((file)->flags & ~THUNAR_FILE_FLAG_THUMB_MASK) |(new_state); }G_STMT_END

#define FLAG_SET(file,flag)    G_STMT_START{((file)->flags |=(flag)); }G_STMT_END
#define FLAG_UNSET(file,flag)  G_STMT_START{((file)->flags &= ~(flag)); }G_STMT_END
#define FLAG_IS_SET(file,flag) (((file)->flags &(flag)) != 0)

#define DEFAULT_CONTENT_TYPE "application/octet-stream"

typedef enum
{
    // storage for ThunarFileThumbState
    THUNAR_FILE_FLAG_THUMB_MASK     = 0x03,

    // for avoiding recursion during destroy
    THUNAR_FILE_FLAG_IN_DESTRUCTION = 1 << 2,

    // whether this file is mounted
    THUNAR_FILE_FLAG_IS_MOUNTED     = 1 << 3,

} ThunarFileFlags;

struct _ThunarFileClass
{
    GObjectClass __parent__;

    /* signals */
    void (*destroy) (ThunarFile *file);
};

struct _ThunarFile
{
    GObject     __parent__;

    /* storage for the file information */
    GFileInfo   *info;
    GFileType   kind;
    GFile       *gfile;
    gchar       *content_type;
    gchar       *icon_name;

    gchar       *custom_icon_name;
    gchar       *display_name;
    gchar       *basename;
    gchar       *thumbnail_path;

    /* sorting */
    gchar       *collate_key;
    gchar       *collate_key_nocase;

    /* flags for thumbnail state etc */
    ThunarFileFlags flags;

    /* tells whether the file watch is not set */
    gboolean    no_file_watch;
};

typedef struct
{
    GFileMonitor    *monitor;
    guint           watch_count;
}
ThunarFileWatch;

typedef struct
{
    ThunarFileGetFunc func;
    gpointer        user_data;
    GCancellable    *cancellable;
}
ThunarFileGetData;

static struct
{
    GUserDirectory  type;
    const gchar     *icon_name;
}

thunar_file_dirs[] =
{
    {G_USER_DIRECTORY_DESKTOP,      "user-desktop"},
    {G_USER_DIRECTORY_DOCUMENTS,    "folder-documents"},
    {G_USER_DIRECTORY_DOWNLOAD,     "folder-download"},
    {G_USER_DIRECTORY_MUSIC,        "folder-music"},
    {G_USER_DIRECTORY_PICTURES,     "folder-pictures"},
    {G_USER_DIRECTORY_PUBLIC_SHARE, "folder-publicshare"},
    {G_USER_DIRECTORY_TEMPLATES,    "folder-templates"},
    {G_USER_DIRECTORY_VIDEOS,       "folder-videos"}
};

G_DEFINE_TYPE_WITH_CODE(ThunarFile,
                        th_file,
                        G_TYPE_OBJECT,
                        G_IMPLEMENT_INTERFACE(THUNARX_TYPE_FILE_INFO,
                                              th_file_info_init))

static GWeakRef* weak_ref_new(GObject *obj)
{
    GWeakRef *ref = g_slice_new(GWeakRef);
    g_weak_ref_init(ref, obj);

    return ref;
}

static void weak_ref_free(GWeakRef *ref)
{
    g_weak_ref_clear(ref);
    g_slice_free(GWeakRef, ref);
}

#ifdef G_ENABLE_DEBUG
#ifdef HAVE_ATEXIT
static gboolean thunar_file_atexit_registered = FALSE;

static void thnar_file_atexit_foreach(gpointer key,
                                      gpointer value,
                                      gpointer user_data)
{
    gchar *uri;

    uri = g_file_get_uri(key);
    g_print("--> %s\n", uri);

    if (G_OBJECT(key)->ref_count > 2)
        g_print("    GFile(%u)\n", G_OBJECT(key)->ref_count - 2);

    g_free(uri);
}

static void thunar_file_atexit()
{
    G_LOCK(file_cache_mutex);

    if (file_cache == NULL || g_hash_table_size(file_cache) == 0)
    {
        G_UNLOCK(file_cache_mutex);
        return;
    }

    g_print("--- Leaked a total of %u ThunarFile objects:\n",
             g_hash_table_size(file_cache));

    g_hash_table_foreach(file_cache, thunar_file_atexit_foreach, NULL);

    g_print("\n");

    G_UNLOCK(file_cache_mutex);
}
#endif
#endif

#if DUMP_FILE_CACHE
static void thunar_file_cache_dump_foreach(gpointer gfile,
                                           gpointer value,
                                           gpointer user_data)
{
    gchar *name;

    name = g_file_get_parse_name(G_FILE(gfile));
    g_print("    %s\n", name);
    g_free(name);
}

static gboolean thunar_file_cache_dump(gpointer user_data)
{
    G_LOCK(file_cache_mutex);

    if (file_cache != NULL)
    {
        g_print("--- %d ThunarFile objects in cache:\n",
                 g_hash_table_size(file_cache));

        g_hash_table_foreach(file_cache, thunar_file_cache_dump_foreach, NULL);

        g_print("\n");
    }

    G_UNLOCK(file_cache_mutex);

    return TRUE;
}
#endif


// Allocate -------------------------------------------------------------------

static void th_file_class_init(ThunarFileClass *klass)
{

#ifdef G_ENABLE_DEBUG
#ifdef HAVE_ATEXIT
    if (G_UNLIKELY(!thunar_file_atexit_registered))
    {
        atexit((void(*)(void)) thunar_file_atexit);
        thunar_file_atexit_registered = TRUE;
    }
#endif
#endif

#if DUMP_FILE_CACHE
    g_timeout_add_seconds(DUMP_FILE_CACHE, thunar_file_cache_dump, NULL);
#endif

    /* pre-allocate the required quarks */
    _file_watch_quark = g_quark_from_static_string("thunar-file-watch");

    /* grab a reference on the user manager */
    _user_manager = thunar_user_manager_get_default();

    /* determine the effective user id of the process */
    _effective_user_id = geteuid();

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->dispose = th_file_dispose;
    gobject_class->finalize = th_file_finalize;

    // Emitted when the system notices that the file was destroyed.
    _file_signals[DESTROY] =
        g_signal_new(I_("destroy"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_RUN_CLEANUP
                         | G_SIGNAL_NO_RECURSE
                         | G_SIGNAL_NO_HOOKS,
                     G_STRUCT_OFFSET(ThunarFileClass, destroy),
                     NULL,
                     NULL,
                     g_cclosure_marshal_VOID__VOID,
                     G_TYPE_NONE, 0);
}

static void th_file_init(ThunarFile *file)
{
    UNUSED(file);
}

static void th_file_info_init(ThunarxFileInfoIface *iface)
{
    iface->get_name = th_file_info_get_name;
    iface->get_uri = th_file_info_get_uri;
    iface->get_parent_uri = th_file_info_get_parent_uri;
    iface->get_uri_scheme = th_file_info_get_uri_scheme;
    iface->get_mime_type = th_file_info_get_mime_type;
    iface->has_mime_type = th_file_info_has_mime_type;
    iface->is_directory = th_file_info_is_directory;
    iface->get_file_info = th_file_info_get_file_info;
    iface->get_filesystem_info = th_file_info_get_filesystem_info;
    iface->get_location = th_file_info_get_location;
    iface->changed = th_file_info_changed;
}

static void th_file_dispose(GObject *object)
{
    ThunarFile *file = THUNAR_FILE(object);

    /* check that we don't recurse here */
    if (!FLAG_IS_SET(file, THUNAR_FILE_FLAG_IN_DESTRUCTION))
    {
        /* emit the "destroy" signal */
        FLAG_SET(file, THUNAR_FILE_FLAG_IN_DESTRUCTION);
        g_signal_emit(object, _file_signals[DESTROY], 0);
        FLAG_UNSET(file, THUNAR_FILE_FLAG_IN_DESTRUCTION);
    }

    G_OBJECT_CLASS(th_file_parent_class)->dispose(object);
}

static void th_file_finalize(GObject *object)
{
    ThunarFile *file = THUNAR_FILE(object);

    /* verify that nobody's watching the file anymore */
#ifdef G_ENABLE_DEBUG
    ThunarFileWatch *file_watch = g_object_get_qdata(G_OBJECT(file),
                                                     thunar_file_watch_quark);
    if (file_watch != NULL)
    {
        g_error("Attempt to finalize a ThunarFile, which has an active "
                 "watch count of %d", file_watch->watch_count);
    }
#endif

    /* drop the entry from the cache */
    G_LOCK(_file_cache_mutex);
    g_hash_table_remove(_file_cache, file->gfile);
    G_UNLOCK(_file_cache_mutex);

    /* release file info */
    if (file->info != NULL)
        g_object_unref(file->info);

    /* free the custom icon name */
    g_free(file->custom_icon_name);

    /* content type info */
    g_free(file->content_type);
    g_free(file->icon_name);

    /* free display name and basename */
    g_free(file->display_name);
    g_free(file->basename);

    /* free collate keys */
    if (file->collate_key_nocase != file->collate_key)
        g_free(file->collate_key_nocase);
    g_free(file->collate_key);

    /* free the thumbnail path */
    g_free(file->thumbnail_path);

    /* release file */
    g_object_unref(file->gfile);

    G_OBJECT_CLASS(th_file_parent_class)->finalize(object);
}


// ThunarxFileInfo ------------------------------------------------------------

static gchar* th_file_info_get_name(ThunarxFileInfo *file_info)
{
    // g_free when unneeded

    return g_strdup(th_file_get_basename(THUNAR_FILE(file_info)));
}

static gchar* th_file_info_get_uri(ThunarxFileInfo *file_info)
{
    // g_free when unneeded

    return th_file_dup_uri(THUNAR_FILE(file_info));
}

static gchar* th_file_info_get_parent_uri(ThunarxFileInfo *file_info)
{
    // g_free when unneeded

    GFile *parent = g_file_get_parent(THUNAR_FILE(file_info)->gfile);

    if (G_UNLIKELY(parent == NULL))
        return NULL;

    gchar *uri = g_file_get_uri(parent);
    g_object_unref(parent);

    return uri;
}

static gchar* th_file_info_get_uri_scheme(ThunarxFileInfo *file_info)
{
    return g_file_get_uri_scheme(THUNAR_FILE(file_info)->gfile);
}

static gchar* th_file_info_get_mime_type(ThunarxFileInfo *file_info)
{
    // g_free

    return g_strdup(th_file_get_content_type(THUNAR_FILE(file_info)));
}

static gboolean th_file_info_has_mime_type(ThunarxFileInfo *file_info,
                                               const gchar     *mime_type)
{
    if (THUNAR_FILE(file_info)->info == NULL)
        return FALSE;

    return g_content_type_is_a(th_file_get_content_type(THUNAR_FILE(file_info)),
                               mime_type);
}

static gboolean th_file_info_is_directory(ThunarxFileInfo *file_info)
{
    return th_file_is_directory(THUNAR_FILE(file_info));
}

static GFileInfo* th_file_info_get_file_info(ThunarxFileInfo *file_info)
{
    // g_object_unref

    thunar_return_val_if_fail(THUNAR_IS_FILE(file_info), NULL);

    if (THUNAR_FILE(file_info)->info != NULL)
        return g_object_ref(THUNAR_FILE(file_info)->info);
    else
        return NULL;
}

static GFileInfo* th_file_info_get_filesystem_info(ThunarxFileInfo *file_info)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file_info), NULL);

    return g_file_query_filesystem_info(THUNAR_FILE(file_info)->gfile,
                                        THUNARX_FILESYSTEM_INFO_NAMESPACE,
                                        NULL,
                                        NULL);
}

static GFile* th_file_info_get_location(ThunarxFileInfo *file_info)
{
    // g_object_unref

    thunar_return_val_if_fail(THUNAR_IS_FILE(file_info), NULL);

    return g_object_ref(THUNAR_FILE(file_info)->gfile);
}

static void th_file_info_changed(ThunarxFileInfo *file_info)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file_info));

    ThunarFile *file = THUNAR_FILE(file_info);

    FLAG_SET_THUMB_STATE(file, 0);

    // tell the file monitor that this file has changed
    thunar_file_monitor_file_changed(file);
}


// Public ---------------------------------------------------------------------

ThunarFile* th_file_get(GFile *gfile, GError **error)
{
    // g_object_unref

    thunar_return_val_if_fail(G_IS_FILE(gfile), NULL);

    // try from the file cache
    ThunarFile *file = th_file_cache_lookup(gfile);

    if (G_UNLIKELY(file != NULL))
        return file;

    // allocate a new object
    file = g_object_new(THUNAR_TYPE_FILE, NULL);
    file->gfile = g_object_ref(gfile);

    if (!_th_file_load(file, NULL, error))
    {
        g_object_unref(file);
        return NULL;
    }

    // insert into the cache

    G_LOCK(_file_cache_mutex);

    g_hash_table_insert(_file_cache,
                        g_object_ref(file->gfile),
                        weak_ref_new(G_OBJECT(file)));

    G_UNLOCK(_file_cache_mutex);

    return file;
}

static gboolean _th_file_load(ThunarFile *file, GCancellable *cancellable,
                              GError **error)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    thunar_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
    thunar_return_val_if_fail(G_IS_FILE(file->gfile), FALSE);

    /* remove the file from cache */
    G_LOCK(_file_cache_mutex);

    if (g_hash_table_lookup(_file_cache, file->gfile) != NULL)
        g_hash_table_remove(_file_cache, file->gfile);

    G_UNLOCK(_file_cache_mutex);

    /* reset the file */
    _th_file_info_clear(file);

    GError *err = NULL;

    /* query a new file info */
    file->info = g_file_query_info(file->gfile,
                                   THUNARX_FILE_INFO_NAMESPACE,
                                   G_FILE_QUERY_INFO_NONE,
                                   cancellable,
                                   &err);

    /* update the file from the information */
    _th_file_info_reload(file, cancellable);

    /* update the mounted info */
    if (err != NULL
        && err->domain == G_IO_ERROR
        && err->code == G_IO_ERROR_NOT_MOUNTED)
    {
        FLAG_UNSET(file, THUNAR_FILE_FLAG_IS_MOUNTED);
        g_clear_error(&err);
    }

    if (err != NULL)
    {
        g_propagate_error(error, err);

        return FALSE;
    }

    /*(re)insert the file into the cache */
    if (file != NULL && file->kind != G_FILE_TYPE_UNKNOWN)
    {
        G_LOCK(_file_cache_mutex);
        g_hash_table_insert(_file_cache,
                            g_object_ref(file->gfile),
                            weak_ref_new(G_OBJECT(file)));
        G_UNLOCK(_file_cache_mutex);
    }

    return TRUE;
}

static void _th_file_info_clear(ThunarFile *file)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    /* release the current file info */
    if (file->info != NULL)
    {
        g_object_unref(file->info);
        file->info = NULL;
    }

    /* unset */
    file->kind = G_FILE_TYPE_UNKNOWN;

    /* free the custom icon name */
    g_free(file->custom_icon_name);
    file->custom_icon_name = NULL;

    /* free display name and basename */
    g_free(file->display_name);
    file->display_name = NULL;

    g_free(file->basename);
    file->basename = NULL;

    /* content type */
    g_free(file->content_type);
    file->content_type = NULL;

    g_free(file->icon_name);
    file->icon_name = NULL;

    /* free collate keys */
    if (file->collate_key_nocase != file->collate_key)
        g_free(file->collate_key_nocase);

    file->collate_key_nocase = NULL;

    g_free(file->collate_key);
    file->collate_key = NULL;

    /* free thumbnail path */
    g_free(file->thumbnail_path);
    file->thumbnail_path = NULL;

    /* assume the file is mounted by default */
    FLAG_SET(file, THUNAR_FILE_FLAG_IS_MOUNTED);

    /* set thumb state to unknown */
    FLAG_SET_THUMB_STATE(file, 0);
}

static void _th_file_info_reload(ThunarFile *file, GCancellable *cancellable)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));
    thunar_return_if_fail(file->info == NULL || G_IS_FILE_INFO(file->info));

    const gchar *target_uri;
    GKeyFile    *key_file;
    gchar       *p;
    const gchar *display_name;
    gboolean    is_secure = FALSE;
    gchar       *casefold;
    gchar       *path;

    if (G_LIKELY(file->info != NULL))
    {
        /* this is requested so often, cache it */
        file->kind = g_file_info_get_file_type(file->info);

        if (file->kind == G_FILE_TYPE_MOUNTABLE)
        {
            target_uri = g_file_info_get_attribute_string(
                                        file->info,
                                        G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);

            if (target_uri != NULL && !g_file_info_get_attribute_boolean(
                                        file->info,
                                        G_FILE_ATTRIBUTE_MOUNTABLE_CAN_MOUNT))
                FLAG_SET(file, THUNAR_FILE_FLAG_IS_MOUNTED);
            else
                FLAG_UNSET(file, THUNAR_FILE_FLAG_IS_MOUNTED);
        }
    }

    /* determine the basename */
    file->basename = g_file_get_basename(file->gfile);
    thunar_assert(file->basename != NULL);

    /* problematic files with content type reading */
    if (strcmp(file->basename, "kmsg") == 0 && g_file_is_native(file->gfile))
    {
        path = g_file_get_path(file->gfile);

        if (g_strcmp0(path, "/proc/kmsg") == 0)
            file->content_type = g_strdup(DEFAULT_CONTENT_TYPE);

        g_free(path);
    }

    /* check if this file is a desktop entry */
    if (th_file_is_desktop_file(file, &is_secure) && is_secure)
    {
        /* determine the custom icon and display name for .desktop files */

        /* query a key file for the .desktop file */
        key_file = eg_file_query_key_file(file->gfile, cancellable, NULL);

        if (key_file != NULL)
        {
            /* read the icon name from the .desktop file */
            file->custom_icon_name = g_key_file_get_string(key_file,
                                     G_KEY_FILE_DESKTOP_GROUP,
                                     G_KEY_FILE_DESKTOP_KEY_ICON,
                                     NULL);

            if (G_UNLIKELY(!file->custom_icon_name || !*file->custom_icon_name))
            {
                /* make sure we set null if the string is empty else the assertion in
                 * thunar_icon_factory_lookup_icon() will fail */
                g_free(file->custom_icon_name);
                file->custom_icon_name = NULL;
            }
            else
            {
                /* drop any suffix(e.g. '.png') from themed icons */
                if (!g_path_is_absolute(file->custom_icon_name))
                {
                    p = strrchr(file->custom_icon_name, '.');
                    if (p != NULL)
                        *p = '\0';
                }
            }
            /* free the key file */
            g_key_file_free(key_file);
        }
    }

    /* determine the display name */
    if (file->display_name == NULL)
    {
        if (G_UNLIKELY(eg_file_is_trash(file->gfile)))
            file->display_name = g_strdup(_("Trash"));
        else if (G_LIKELY(file->info != NULL))
        {
            display_name = g_file_info_get_display_name(file->info);
            if (G_LIKELY(display_name != NULL))
            {
                if (strcmp(display_name, "/") == 0)
                    file->display_name = g_strdup(_("File System"));
                else
                    file->display_name = g_strdup(display_name);
            }
        }

        /* fall back to a name for the gfile */
        if (file->display_name == NULL)
            file->display_name = eg_file_get_display_name(file->gfile);
    }

    /* create case sensitive collation key */
    file->collate_key = g_utf8_collate_key_for_filename(file->display_name, -1);

    /* lowercase the display name */
    casefold = g_utf8_casefold(file->display_name, -1);

    /* if the lowercase name is equal, only peek the already hash key */
    if (casefold != NULL && strcmp(casefold, file->display_name) != 0)
        file->collate_key_nocase = g_utf8_collate_key_for_filename(casefold, -1);
    else
        file->collate_key_nocase = file->collate_key;

    /* cleanup */
    g_free(casefold);
}

ThunarFile* th_file_get_with_info(GFile *gfile, GFileInfo *info, gboolean not_mounted)
{
    // g_object_unref

    thunar_return_val_if_fail(G_IS_FILE(gfile), NULL);
    thunar_return_val_if_fail(G_IS_FILE_INFO(info), NULL);

    /* check if we already have a cached version of that file */
    ThunarFile *file = th_file_cache_lookup(gfile);

    if (G_UNLIKELY(file != NULL))
        return file;

    /* allocate a new object */
    file = g_object_new(THUNAR_TYPE_FILE, NULL);
    file->gfile = g_object_ref(gfile);

    /* reset the file */
    _th_file_info_clear(file);

    /* set the passed info */
    file->info = g_object_ref(info);

    /* update the file from the information */
    _th_file_info_reload(file, NULL);

    /* update the mounted info */
    if (not_mounted)
        FLAG_UNSET(file, THUNAR_FILE_FLAG_IS_MOUNTED);

    /* setup lock until the file is inserted */
    G_LOCK(_file_cache_mutex);

    /* insert the file into the cache */
    g_hash_table_insert(_file_cache,
                        g_object_ref(file->gfile),
                        weak_ref_new(G_OBJECT(file)));

    /* done inserting in the cache */
    G_UNLOCK(_file_cache_mutex);

    return file;
}

ThunarFile* th_file_get_for_uri(const gchar *uri, GError **error)
{
    // g_object_unref

    thunar_return_val_if_fail(uri != NULL, NULL);
    thunar_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GFile *path = g_file_new_for_commandline_arg(uri);
    ThunarFile *file = th_file_get(path, error);
    g_object_unref(path);

    return file;
}

void th_file_get_async(GFile *location, GCancellable *cancellable,
                       ThunarFileGetFunc func, gpointer user_data)
{
    thunar_return_if_fail(G_IS_FILE(location));
    thunar_return_if_fail(func != NULL);

    /* check if we already have a cached version of that file */
    ThunarFile *file = th_file_cache_lookup(location);

    if (G_UNLIKELY(file != NULL))
    {
        /* call the return function with the file from the cache */
        (func) (location, file, NULL, user_data);
        g_object_unref(file);

        return;
    }

    /* allocate get data */
    ThunarFileGetData *data = g_slice_new0(ThunarFileGetData);
    data->user_data = user_data;
    data->func = func;

    if (cancellable != NULL)
        data->cancellable = g_object_ref(cancellable);

    /* load the file information asynchronously */
    g_file_query_info_async(location,
                            THUNARX_FILE_INFO_NAMESPACE,
                            G_FILE_QUERY_INFO_NONE,
                            G_PRIORITY_DEFAULT,
                            cancellable,
                            _th_file_get_async_finish,
                            data);
}

static void _th_file_get_async_finish(GObject *object, GAsyncResult *result,
                                      gpointer user_data)
{
    GFile *location = G_FILE(object);

    thunar_return_if_fail(G_IS_FILE(location));
    thunar_return_if_fail(G_IS_ASYNC_RESULT(result));


    /* finish querying the file information */
    GError *error = NULL;
    GFileInfo *file_info = g_file_query_info_finish(location, result, &error);

    /* allocate a new file object */
    ThunarFile *file = g_object_new(THUNAR_TYPE_FILE, NULL);
    file->gfile = g_object_ref(location);

    /* reset the file */
    _th_file_info_clear(file);

    /* set the file information */
    file->info = file_info;

    ThunarFileGetData *data = user_data;

    /* update the file from the information */
    _th_file_info_reload(file, data->cancellable);

    /* update the mounted info */
    if (error != NULL
        && error->domain == G_IO_ERROR
        && error->code == G_IO_ERROR_NOT_MOUNTED)
    {
        FLAG_UNSET(file, THUNAR_FILE_FLAG_IS_MOUNTED);
        g_clear_error(&error);
    }

    /* insert the file into the cache */
    G_LOCK(_file_cache_mutex);
    g_hash_table_insert(_file_cache,
                        g_object_ref(file->gfile),
                        weak_ref_new(G_OBJECT(file)));
    G_UNLOCK(_file_cache_mutex);

    /* pass the loaded file and possible errors to the return function */
    (data->func) (location, file, error, data->user_data);

    /* release the file, see description in ThunarFileGetFunc */
    g_object_unref(file);

    /* free the error, if there is any */
    if (error != NULL)
        g_error_free(error);

    /* release the get data */
    if (data->cancellable != NULL)
        g_object_unref(data->cancellable);

    g_slice_free(ThunarFileGetData, data);
}

GFile* th_file_get_file(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(G_IS_FILE(file->gfile), NULL);

    return file->gfile;
}

GFileInfo* th_file_get_info(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(file->info == NULL || G_IS_FILE_INFO(file->info), NULL);

    return file->info;
}


ThunarFile* th_file_get_parent(const ThunarFile *file, GError **error)
{
    // You may want to call th_file_has_parent() first
    // g_object_unref

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(error == NULL || *error == NULL, NULL);

    GFile *parent_file = g_file_get_parent(file->gfile);

    if (parent_file == NULL)
    {
        g_set_error(error,
                    G_FILE_ERROR,
                    G_FILE_ERROR_NOENT,
                    _("The root folder has no parent"));

        return NULL;
    }

    ThunarFile *parent = th_file_get(parent_file, error);
    g_object_unref(parent_file);

    return parent;
}

gboolean th_file_check_loaded(ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (G_UNLIKELY(file->info == NULL))
        _th_file_load(file, NULL, NULL);

    return (file->info != NULL);
}

gboolean th_file_execute(ThunarFile  *file,
                         GFile       *working_directory,
                         gpointer    parent,
                         GList       *file_list,
                         const gchar *startup_id,
                         GError      **error)
{
    gboolean    snotify = FALSE;
    gboolean    terminal;
    gboolean    result = FALSE;
    GKeyFile   *key_file;
    GError     *err = NULL;
    GFile      *file_parent;
    gchar      *icon_name = NULL;
    gchar      *name;
    gchar      *type;
    gchar      *url;
    gchar      *escaped_location;
    gchar     **argv = NULL;
    gchar      *exec;
    gchar      *command;
    gchar      *directory = NULL;
    guint32     stimestamp = 0;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    gchar      *location;
    location = eg_file_get_location(file->gfile);
    GSList     *uri_list = NULL;
    GList      *li;
    for (li = file_list; li != NULL; li = li->next)
        uri_list = g_slist_prepend(uri_list, g_file_get_uri(li->data));

    uri_list = g_slist_reverse(uri_list);

    gboolean    is_secure = FALSE;
    if (th_file_is_desktop_file(file, &is_secure))
    {
        /* parse file first, even if it is insecure */
        key_file = eg_file_query_key_file(file->gfile, NULL, &err);
        if (key_file == NULL)
        {
            g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                         _("Failed to parse the desktop file: %s"), err->message);
            g_error_free(err);
            return FALSE;
        }

        type = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TYPE, NULL);
        if (G_LIKELY(g_strcmp0(type, "Application") == 0))
        {
            exec = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_EXEC, NULL);
            if (G_LIKELY(exec != NULL))
            {
                /* if the .desktop file is not secure, ask user what to do */
                if (is_secure || dialog_insecure_program(parent, _("Untrusted application launcher"), file, exec))
                {
                    /* parse other fields */
                    name = g_key_file_get_locale_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_NAME, NULL, NULL);
                    icon_name = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_ICON, NULL);
                    directory = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_PATH, NULL);
                    terminal = g_key_file_get_boolean(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_TERMINAL, NULL);
                    snotify = g_key_file_get_boolean(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_STARTUP_NOTIFY, NULL);

                    /* expand the field codes and parse the execute command */
                    command = e_expand_desktop_entry_field_codes(exec, uri_list, icon_name,
                              name, location, terminal);
                    g_free(name);
                    result = g_shell_parse_argv(command, NULL, &argv, error);
                    g_free(command);
                }
                else
                {
                    /* fall-through to free value and leave without execution */
                    result = TRUE;
                }

                g_free(exec);
            }
            else
            {
                /* TRANSLATORS: `Exec' is a field name in a .desktop file. Don't translate it. */
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                             _("No Exec field specified"));
            }
        }
        else if (g_strcmp0(type, "Link") == 0)
        {
            url = g_key_file_get_string(key_file, G_KEY_FILE_DESKTOP_GROUP, G_KEY_FILE_DESKTOP_KEY_URL, NULL);
            if (G_LIKELY(url != NULL))
            {
                /* if the .desktop file is not secure, ask user what to do */
                if (is_secure || dialog_insecure_program(parent, _("Untrusted link launcher"), file, url))
                {
                    /* pass the URL to the webbrowser, this could be a bit strange,
                     * but then at least we are on the secure side */
                    argv = g_new(gchar *, 3);
                    argv[0] = g_strdup("exo-open");
                    argv[1] = url;
                    argv[2] = NULL;
                }

                result = TRUE;
            }
            else
            {
                /* TRANSLATORS: `URL' is a field name in a .desktop file. Don't translate it. */
                g_set_error(error, G_FILE_ERROR, G_FILE_ERROR_INVAL,
                             _("No URL field specified"));
            }
        }
        else
        {
            g_set_error_literal(error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("Invalid desktop file"));
        }

        g_free(type);
        g_key_file_free(key_file);
    }
    else
    {
        /* fake the Exec line */
        escaped_location = g_shell_quote(location);
        exec = g_strconcat(escaped_location, " %F", NULL);
        command = e_expand_desktop_entry_field_codes(exec, uri_list, NULL, NULL, NULL, FALSE);
        result = g_shell_parse_argv(command, NULL, &argv, error);
        g_free(escaped_location);
        g_free(exec);
        g_free(command);
    }

    if (G_LIKELY(result && argv != NULL))
    {
        /* use other directory if the Path from the desktop file was not set */
        if (G_LIKELY(directory == NULL))
        {
            /* determine the working directory */
            if (G_LIKELY(working_directory != NULL))
            {
                /* copy the working directory provided to this method */
                directory = g_file_get_path(working_directory);
            }
            else if (file_list != NULL)
            {
                /* use the directory of the first list item */
                file_parent = g_file_get_parent(file_list->data);
                directory =(file_parent != NULL) ? eg_file_get_location(file_parent) : NULL;
                g_object_unref(file_parent);
            }
            else
            {
                /* use the directory of the executable file */
                parent = g_file_get_parent(file->gfile);
                directory =(parent != NULL) ? eg_file_get_location(parent) : NULL;
                g_object_unref(parent);
            }
        }

        /* check if a startup id was passed(launch request over dbus) */
        if (startup_id != NULL && *startup_id != '\0')
        {
            /* parse startup_id string and extract timestamp
             * format: <unique>_TIME<timestamp>) */
            gchar *time_str = g_strrstr(startup_id, "_TIME");
            if (time_str != NULL)
            {
                gchar *end;

                /* ignore the "_TIME" part */
                time_str += 5;

                stimestamp = strtoul(time_str, &end, 0);
                if (end == time_str)
                    stimestamp = 0;
            }
        }
        else
        {
            /* use current event time */
            stimestamp = gtk_get_current_event_time();
        }

        /* execute the command */
        result = xfce_spawn_on_screen(thunar_util_parse_parent(parent, NULL),
                                       directory, argv, NULL, G_SPAWN_SEARCH_PATH,
                                       snotify, stimestamp, icon_name, error);
    }

    /* clean up */
    g_slist_free_full(uri_list, g_free);
    g_strfreev(argv);
    g_free(location);
    g_free(directory);
    g_free(icon_name);

    return result;
}

gboolean th_file_launch(ThunarFile *file, gpointer parent, const gchar *startup_id,
                        GError **error)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);
    thunar_return_val_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent), FALSE);

    GdkScreen           *screen;
    screen = thunar_util_parse_parent(parent, NULL);

    /* check if we have a folder here */
    if (th_file_is_directory(file))
    {
        ThunarApplication   *application;
        application = thunar_application_get();

        // Create app window
        thunar_application_open_window(application, file, screen, startup_id, FALSE);

        g_object_unref(G_OBJECT(application));
        return TRUE;
    }

    /* check if we should execute the file */
    if (th_file_is_executable(file))
        return th_file_execute(file, NULL, parent, NULL, NULL, error);

    /* determine the default application to open the file */
    /* TODO We should probably add a cancellable argument to thunar_file_launch() */
    GAppInfo            *app_info;
    app_info = th_file_get_default_handler(THUNAR_FILE(file));

    /* display the application chooser if no application is defined for this file
     * type yet */
    if (G_UNLIKELY(app_info == NULL))
    {
        thunar_show_chooser_dialog(parent, file, TRUE);
        return TRUE;
    }

    /* HACK: check if we're not trying to launch another file manager again, possibly
     * ourselfs which will end in a loop */
    if (g_strcmp0(g_app_info_get_id(app_info), "exo-file-manager.desktop") == 0
            || g_strcmp0(g_app_info_get_id(app_info), "thunar.desktop") == 0
            || g_strcmp0(g_app_info_get_name(app_info), "exo-file-manager") == 0)
    {
        g_object_unref(G_OBJECT(app_info));
        thunar_show_chooser_dialog(parent, file, TRUE);
        return TRUE;
    }

    /* fake a path list */
    GList path_list;
    path_list.data = file->gfile;
    path_list.next = path_list.prev = NULL;

    /* create a launch context */
    GdkAppLaunchContext *context;
    context = gdk_display_get_app_launch_context(gdk_screen_get_display(screen));
    gdk_app_launch_context_set_screen(context, screen);
    gdk_app_launch_context_set_timestamp(context, gtk_get_current_event_time());

    /* otherwise try to execute the application */
    gboolean succeed = g_app_info_launch(app_info,
                                         &path_list,
                                         G_APP_LAUNCH_CONTEXT(context),
                                         error);

    /* destroy the launch context */
    g_object_unref(context);

    /* release the handler reference */
    g_object_unref(G_OBJECT(app_info));

    return succeed;
}

gboolean th_file_rename(ThunarFile *file, const gchar *name, GCancellable *cancellable,
                        gboolean called_from_job, GError **error)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(g_utf8_validate(name, -1, NULL), FALSE);
    thunar_return_val_if_fail(cancellable == NULL || G_IS_CANCELLABLE(cancellable), FALSE);
    thunar_return_val_if_fail(error == NULL || *error == NULL, FALSE);

    G_LOCK(_file_rename_mutex);

    /* try to rename the file */
    GFile *renamed_file = g_file_set_display_name(file->gfile, name, cancellable, error);

    /* check if we succeeded */
    if (renamed_file == NULL)
    {
        G_UNLOCK(_file_rename_mutex);

        return FALSE;
    }

    /* notify the file is renamed */
    _th_file_monitor_moved(file, renamed_file);

    g_object_unref(G_OBJECT(renamed_file));

    if (!called_from_job)
    {
        /* emit the file changed signal */
        th_file_changed(file);
    }

    G_UNLOCK(_file_rename_mutex);

    return TRUE;
}

/**
 * thunar_file_accepts_drop:
 * @file                    : a #ThunarFile instance.
 * @file_list               : the list of #GFile<!---->s that will be dropped.
 * @context                 : the current #GdkDragContext, which is used for the drop.
 * @suggested_action_return : return location for the suggested #GdkDragAction or %NULL.
 *
 * Checks whether @file can accept @file_list for the given @context and
 * returns the #GdkDragAction<!---->s that can be used or 0 if no actions
 * apply.
 *
 * If any #GdkDragAction<!---->s apply and @suggested_action_return is not
 * %NULL, the suggested #GdkDragAction for this drop will be stored to the
 * location pointed to by @suggested_action_return.
 *
 * Return value: the #GdkDragAction<!---->s supported for the drop or
 *               0 if no drop is possible.
 **/
GdkDragAction th_file_accepts_drop(ThunarFile     *file,
                                       GList          *file_list,
                                       GdkDragContext *context,
                                       GdkDragAction  *suggested_action_return)
{
    GdkDragAction suggested_action;
    GdkDragAction actions;
    ThunarFile   *ofile;
    GFile        *parent_file;
    GList        *lp;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), 0);
    thunar_return_val_if_fail(GDK_IS_DRAG_CONTEXT(context), 0);

    /* we can never drop an empty list */
    if (G_UNLIKELY(file_list == NULL))
        return 0;

    /* default to whatever GTK+ thinks for the suggested action */
    suggested_action = gdk_drag_context_get_suggested_action(context);

    /* get the possible actions */
    actions = gdk_drag_context_get_actions(context);

    /* when the option to ask the user is set, make it the preferred action */
    if (G_UNLIKELY((actions & GDK_ACTION_ASK) != 0))
        suggested_action = GDK_ACTION_ASK;

    /* check if we have a writable directory here or an executable file */
    if (th_file_is_directory(file) && th_file_is_writable(file))
    {
        /* determine the possible actions */
        actions &=(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_ASK);

        /* cannot create symbolic links in the trash or copy to the trash */
        if (th_file_is_trashed(file))
            actions &= ~(GDK_ACTION_COPY | GDK_ACTION_LINK);

        for(lp = file_list; lp != NULL; lp = lp->next)
        {
            /* we cannot drop a file on itself */
            if (G_UNLIKELY(g_file_equal(file->gfile, lp->data)))
                return 0;

            /* check whether source and destination are the same */
            parent_file = g_file_get_parent(lp->data);
            if (G_LIKELY(parent_file != NULL))
            {
                if (g_file_equal(file->gfile, parent_file))
                {
                    g_object_unref(parent_file);
                    return 0;
                }
                else
                    g_object_unref(parent_file);
            }

            /* copy/move/link within the trash not possible */
            if (G_UNLIKELY(eg_file_is_trashed(lp->data) && th_file_is_trashed(file)))
                return 0;
        }

        /* if the source offers both copy and move and the GTK+ suggested action is copy, try to be smart telling whether
         * we should copy or move by default by checking whether the source and target are on the same disk.
         */
        if ((actions &(GDK_ACTION_COPY | GDK_ACTION_MOVE)) != 0
                &&(suggested_action == GDK_ACTION_COPY))
        {
            /* default to move as suggested action */
            suggested_action = GDK_ACTION_MOVE;

            for(lp = file_list; lp != NULL; lp = lp->next)
            {
                /* dropping from the trash always suggests move */
                if (G_UNLIKELY(eg_file_is_trashed(lp->data)))
                    break;

                /* determine the cached version of the source file */
                ofile = th_file_cache_lookup(lp->data);

                /* fallback to non-cached version */
                if (ofile == NULL)
                    ofile = th_file_get(lp->data, NULL);

                /* we have only move if we know the source and both the source and the target
                 * are on the same disk, and the source file is owned by the current user.
                 */
                if (ofile == NULL
                        || !_th_file_same_filesystem(file, ofile)
                        ||(ofile->info != NULL
                            && g_file_info_get_attribute_uint32(ofile->info,
                                    G_FILE_ATTRIBUTE_UNIX_UID) != _effective_user_id))
                {
                    /* default to copy and get outa here */
                    suggested_action = GDK_ACTION_COPY;
                    break;
                }

                if (ofile != NULL)
                    g_object_unref(ofile);
            }
        }
    }
    else if (th_file_is_executable(file))
    {
        /* determine the possible actions */
        actions &=(GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK | GDK_ACTION_PRIVATE);
    }
    else
        return 0;

    /* determine the preferred action based on the context */
    if (G_LIKELY(suggested_action_return != NULL))
    {
        /* determine a working action */
        if (G_LIKELY((suggested_action & actions) != 0))
            *suggested_action_return = suggested_action;
        else if ((actions & GDK_ACTION_ASK) != 0)
            *suggested_action_return = GDK_ACTION_ASK;
        else if ((actions & GDK_ACTION_COPY) != 0)
            *suggested_action_return = GDK_ACTION_COPY;
        else if ((actions & GDK_ACTION_LINK) != 0)
            *suggested_action_return = GDK_ACTION_LINK;
        else if ((actions & GDK_ACTION_MOVE) != 0)
            *suggested_action_return = GDK_ACTION_MOVE;
        else
            *suggested_action_return = GDK_ACTION_PRIVATE;
    }

    /* yeppa, we can drop here */
    return actions;
}

static gboolean _th_file_same_filesystem(const ThunarFile *file_a,
                                            const ThunarFile *file_b)
{
    const gchar *filesystem_id_a;
    const gchar *filesystem_id_b;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file_a), FALSE);
    thunar_return_val_if_fail(THUNAR_IS_FILE(file_b), FALSE);

    /* return false if we have no information about one of the files */
    if (file_a->info == NULL || file_b->info == NULL)
        return FALSE;

    /* determine the filesystem IDs */
    filesystem_id_a = g_file_info_get_attribute_string(file_a->info,
                      G_FILE_ATTRIBUTE_ID_FILESYSTEM);

    filesystem_id_b = g_file_info_get_attribute_string(file_b->info,
                      G_FILE_ATTRIBUTE_ID_FILESYSTEM);

    /* compare the filesystem IDs */
    return (g_strcmp0(filesystem_id_a, filesystem_id_b) == 0);
}

/**
 * thunar_file_get_date:
 * @file        : a #ThunarFile instance.
 * @date_type   : the kind of date you are interested in.
 *
 * Queries the given @date_type from @file and returns the result.
 *
 * Return value: the time for @file of the given @date_type.
 **/
guint64 th_file_get_date(const ThunarFile  *file,
                             ThunarFileDateType date_type)
{
    const gchar *attribute;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), 0);

    if (file->info == NULL)
        return 0;

    switch(date_type)
    {
    case THUNAR_FILE_DATE_ACCESSED:
        attribute = G_FILE_ATTRIBUTE_TIME_ACCESS;
        break;
    case THUNAR_FILE_DATE_CHANGED:
        attribute = G_FILE_ATTRIBUTE_TIME_CHANGED;
        break;
    case THUNAR_FILE_DATE_MODIFIED:
        attribute = G_FILE_ATTRIBUTE_TIME_MODIFIED;
        break;
    default:
        thunar_assert_not_reached();
    }

    return g_file_info_get_attribute_uint64(file->info, attribute);
}

/**
 * thunar_file_get_date_string:
 * @file       : a #ThunarFile instance.
 * @date_type  : the kind of date you are interested to know about @file.
 * @date_style : the style used to format the date.
 * @date_custom_style : custom style to apply, if @date_style is set to custom
 *
 * Tries to determine the @date_type of @file, and if @file supports the
 * given @date_type, it'll be formatted as string and returned. The
 * caller is responsible for freeing the string using the g_free()
 * function.
 *
 * Return value: the @date_type of @file formatted as string.
 **/
gchar* th_file_get_date_string(const ThunarFile  *file,
                                   ThunarFileDateType date_type,
                                   ThunarDateStyle    date_style,
                                   const gchar       *date_custom_style)
{
    return thunar_util_humanize_file_time(th_file_get_date(file, date_type), date_style, date_custom_style);
}

/**
 * thunar_file_get_mode_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the mode of @file as text. You'll need to free
 * the result using g_free() when you're done with it.
 *
 * Return value: the mode of @file as string.
 **/
gchar* th_file_get_mode_string(const ThunarFile *file)
{
    ThunarFileMode mode;
    GFileType      kind;
    gchar         *text;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    kind = th_file_get_kind(file);
    mode = th_file_get_mode(file);
    text = g_new(gchar, 11);

    /* file type */
    /* TODO earlier versions of Thunar had 'P' for ports and
     * 'D' for doors. Do we still need those? */
    switch (kind)
    {
    case G_FILE_TYPE_SYMBOLIC_LINK:
        text[0] = 'l';
        break;
    case G_FILE_TYPE_REGULAR:
        text[0] = '-';
        break;
    case G_FILE_TYPE_DIRECTORY:
        text[0] = 'd';
        break;
    case G_FILE_TYPE_SPECIAL:
    case G_FILE_TYPE_UNKNOWN:
    default:
        if (S_ISCHR(mode))
            text[0] = 'c';
        else if (S_ISSOCK(mode))
            text[0] = 's';
        else if (S_ISFIFO(mode))
            text[0] = 'f';
        else if (S_ISBLK(mode))
            text[0] = 'b';
        else
            text[0] = ' ';
    }

    /* permission flags */
    text[1] =(mode & THUNAR_FILE_MODE_USR_READ)  ? 'r' : '-';
    text[2] =(mode & THUNAR_FILE_MODE_USR_WRITE) ? 'w' : '-';
    text[3] =(mode & THUNAR_FILE_MODE_USR_EXEC)  ? 'x' : '-';
    text[4] =(mode & THUNAR_FILE_MODE_GRP_READ)  ? 'r' : '-';
    text[5] =(mode & THUNAR_FILE_MODE_GRP_WRITE) ? 'w' : '-';
    text[6] =(mode & THUNAR_FILE_MODE_GRP_EXEC)  ? 'x' : '-';
    text[7] =(mode & THUNAR_FILE_MODE_OTH_READ)  ? 'r' : '-';
    text[8] =(mode & THUNAR_FILE_MODE_OTH_WRITE) ? 'w' : '-';
    text[9] =(mode & THUNAR_FILE_MODE_OTH_EXEC)  ? 'x' : '-';

    /* special flags */
    if (G_UNLIKELY(mode & THUNAR_FILE_MODE_SUID))
        text[3] = 's';
    if (G_UNLIKELY(mode & THUNAR_FILE_MODE_SGID))
        text[6] = 's';
    if (G_UNLIKELY(mode & THUNAR_FILE_MODE_STICKY))
        text[9] = 't';

    text[10] = '\0';

    return text;
}

/**
 * thunar_file_get_size_in_bytes_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the size in bytes of the file as text.
 * You'll need to free the result using g_free()
 * if you're done with it.
 *
 * Return value: the size of @file in bytes.
 **/
gchar* th_file_get_size_in_bytes_string(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return g_strdup_printf("%'"G_GUINT64_FORMAT, th_file_get_size(file));
}

/**
 * thunar_file_get_size_string_formatted:
 * @file             : a #ThunarFile instance.
 * @file_size_binary : indicates if file size format
 *                     should be binary or not.
 *
 * Returns the size of the file as text in a human readable
 * format in decimal or binary format. You'll need to free
 * the result using g_free() if you're done with it.
 *
 * Return value: the size of @file in a human readable
 *               format.
 **/
gchar* th_file_get_size_string_formatted(const ThunarFile *file,
                                             const gboolean file_size_binary)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return g_format_size_full(th_file_get_size(file),
                               file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT);
}

/**
 * thunar_file_get_size_string_long:
 * @file             : a #ThunarFile instance.
 * @file_size_binary : indicates if file size format
 *                     should be binary or not.
 *
 * Returns the size of the file as text in a human readable
 * format in decimal or binary format, including the exact
 * size in bytes. You'll need to free the result using
 * g_free() if you're done with it.
 *
 * Return value: the size of @file in a human readable
 *               format, including size in bytes.
 **/
gchar* th_file_get_size_string_long(const ThunarFile *file,
                                        const gboolean file_size_binary)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return g_format_size_full(th_file_get_size(file),
                               G_FORMAT_SIZE_LONG_FORMAT |(file_size_binary ? G_FORMAT_SIZE_IEC_UNITS : G_FORMAT_SIZE_DEFAULT));
}

/**
 * thunar_file_get_volume:
 * @file           : a #ThunarFile instance.
 *
 * Attempts to determine the #GVolume on which @file is located. If @file cannot
 * determine it's volume, then %NULL will be returned. Else a #GVolume instance
 * is returned which has to be released by the caller using g_object_unref().
 *
 * Return value: the #GVolume for @file or %NULL.
 **/
GVolume* th_file_get_volume(const ThunarFile *file)
{
    GVolume *volume = NULL;
    GMount  *mount;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    /* TODO make this function call asynchronous */
    mount = g_file_find_enclosing_mount(file->gfile, NULL, NULL);
    if (mount != NULL)
    {
        volume = g_mount_get_volume(mount);
        g_object_unref(mount);
    }

    return volume;
}

/**
 * thunar_file_get_group:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarGroup for @file. If there's no
 * group associated with @file or if the system is unable to
 * determine the group, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarGroup for @file or %NULL.
 **/
ThunarGroup* th_file_get_group(const ThunarFile *file)
{
    guint32 gid;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    /* TODO what are we going to do on non-UNIX systems? */
    gid = g_file_info_get_attribute_uint32(file->info,
                                            G_FILE_ATTRIBUTE_UNIX_GID);

    return thunar_user_manager_get_group_by_id(_user_manager, gid);
}

/**
 * thunar_file_get_user:
 * @file : a #ThunarFile instance.
 *
 * Determines the #ThunarUser for @file. If there's no
 * user associated with @file or if the system is unable
 * to determine the user, %NULL will be returned.
 *
 * The caller is responsible for freeing the returned object
 * using g_object_unref().
 *
 * Return value: the #ThunarUser for @file or %NULL.
 **/
ThunarUser* th_file_get_user(const ThunarFile *file)
{
    guint32 uid;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    /* TODO what are we going to do on non-UNIX systems? */
    uid = g_file_info_get_attribute_uint32(file->info,
                                            G_FILE_ATTRIBUTE_UNIX_UID);

    return thunar_user_manager_get_user_by_id(_user_manager, uid);
}

/**
 * thunar_file_get_content_type:
 * @file : a #ThunarFile.
 *
 * Returns the content type of @file.
 *
 * Return value: content type of @file.
 **/
const gchar* th_file_get_content_type(ThunarFile *file)
{
    GFileInfo   *info;
    GError      *err = NULL;
    const gchar *content_type = NULL;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    if (G_UNLIKELY(file->content_type == NULL))
    {
        G_LOCK(_file_content_type_mutex);

        /* make sure we weren't waiting for a lock */
        if (G_UNLIKELY(file->content_type != NULL))
            goto bailout;

        /* make sure this is not loaded in the general info */
        thunar_assert(file->info == NULL
                        || !g_file_info_has_attribute(file->info, G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE));

        if (G_UNLIKELY(file->kind == G_FILE_TYPE_DIRECTORY))
        {
            /* this we known for sure */
            file->content_type = g_strdup("inode/directory");
        }
        else
        {
            /* async load the content-type */
            info = g_file_query_info(file->gfile,
                                      G_FILE_ATTRIBUTE_STANDARD_CONTENT_TYPE ","
                                      G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE,
                                      G_FILE_QUERY_INFO_NONE,
                                      NULL, &err);

            if (G_LIKELY(info != NULL))
            {
                /* store the new content type */
                content_type = g_file_info_get_content_type(info);
                if (G_UNLIKELY(content_type == NULL))
                    content_type = g_file_info_get_attribute_string(info,
                                   G_FILE_ATTRIBUTE_STANDARD_FAST_CONTENT_TYPE);
                if (G_LIKELY(content_type != NULL))
                    file->content_type = g_strdup(content_type);
                g_object_unref(G_OBJECT(info));
            }
            else
            {
                g_warning("Content type loading failed for %s: %s",
                           th_file_get_display_name(file),
                           err->message);
                g_error_free(err);
            }

            /* always provide a fallback */
            if (file->content_type == NULL)
                file->content_type = g_strdup(DEFAULT_CONTENT_TYPE);
        }

bailout:

        G_UNLOCK(_file_content_type_mutex);
    }

    return file->content_type;
}

gboolean th_file_load_content_type(ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), TRUE);

    if (file->content_type != NULL)
        return FALSE;

    th_file_get_content_type(file);

    return TRUE;
}

/**
 * thunar_file_get_symlink_target:
 * @file : a #ThunarFile.
 *
 * Returns the path of the symlink target or %NULL if the @file
 * is not a symlink.
 *
 * Return value: path of the symlink target or %NULL.
 **/
const gchar* th_file_get_symlink_target(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    if (file->info == NULL)
        return NULL;

    return g_file_info_get_symlink_target(file->info);
}

const gchar* th_file_get_basename(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    return file->basename;
}

gboolean th_file_is_symlink(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    return g_file_info_get_is_symlink(file->info);
}

guint64 th_file_get_size(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), 0);

    if (file->info == NULL)
        return 0;

    return g_file_info_get_size(file->info);
}

/**
 * thunar_file_get_default_handler:
 * @file : a #ThunarFile instance.
 *
 * Returns the default #GAppInfo for @file or %NULL if there is none.
 *
 * The caller is responsible to free the returned #GAppInfo using
 * g_object_unref().
 *
 * Return value: Default #GAppInfo for @file or %NULL if there is none.
 **/
GAppInfo* th_file_get_default_handler(const ThunarFile *file)
{
    const gchar *content_type;
    GAppInfo    *app_info = NULL;
    gboolean     must_support_uris = FALSE;
    gchar       *path;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    content_type = th_file_get_content_type(THUNAR_FILE(file));
    if (content_type != NULL)
    {
        path = g_file_get_path(file->gfile);
        must_support_uris =(path == NULL);
        g_free(path);

        app_info = g_app_info_get_default_for_type(content_type, must_support_uris);
    }

    if (app_info == NULL)
        app_info = g_file_query_default_handler(file->gfile, NULL, NULL);

    return app_info;
}

/**
 * thunar_file_get_kind:
 * @file : a #ThunarFile instance.
 *
 * Returns the kind of @file.
 *
 * Return value: the kind of @file.
 **/
GFileType th_file_get_kind(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), G_FILE_TYPE_UNKNOWN);
    return file->kind;
}

GFile* th_file_get_target_location(const ThunarFile *file)
{
    const gchar *uri;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    if (file->info == NULL)
        return g_object_ref(file->gfile);

    uri = g_file_info_get_attribute_string(file->info,
                                            G_FILE_ATTRIBUTE_STANDARD_TARGET_URI);

    return(uri != NULL) ? g_file_new_for_uri(uri) : NULL;
}

/**
 * thunar_file_get_mode:
 * @file : a #ThunarFile instance.
 *
 * Returns the permission bits of @file.
 *
 * Return value: the permission bits of @file.
 **/
ThunarFileMode th_file_get_mode(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), 0);

    if (file->info == NULL)
        return 0;

    if (g_file_info_has_attribute(file->info, G_FILE_ATTRIBUTE_UNIX_MODE))
        return g_file_info_get_attribute_uint32(file->info, G_FILE_ATTRIBUTE_UNIX_MODE);
    else
        return th_file_is_directory(file) ? 0777 : 0666;
}

gboolean th_file_is_mounted(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return FLAG_IS_SET(file, THUNAR_FILE_FLAG_IS_MOUNTED);
}

gboolean th_file_exists(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return g_file_query_exists(file->gfile, NULL);
}

gboolean th_file_is_directory(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return file->kind == G_FILE_TYPE_DIRECTORY;
}

gboolean th_file_is_shortcut(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return file->kind == G_FILE_TYPE_SHORTCUT;
}

gboolean th_file_is_mountable(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return file->kind == G_FILE_TYPE_MOUNTABLE;
}

gboolean th_file_is_local(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return g_file_has_uri_scheme(file->gfile, "file");
}

/**
 * thunar_file_is_parent:
 * @file  : a #ThunarFile instance.
 * @child : another #ThunarFile instance.
 *
 * Determines whether @file is the parent directory of @child.
 *
 * Return value: %TRUE if @file is the parent of @child.
 **/
gboolean th_file_is_parent(const ThunarFile *file,
                               const ThunarFile *child)
{
    gboolean is_parent = FALSE;
    GFile   *parent;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(THUNAR_IS_FILE(child), FALSE);

    parent = g_file_get_parent(child->gfile);
    if (parent != NULL)
    {
        is_parent = g_file_equal(file->gfile, parent);
        g_object_unref(parent);
    }

    return is_parent;
}

/**
 * thunar_file_is_ancestor:
 * @file     : a #ThunarFile instance.
 * @ancestor : another #GFile instance.
 *
 * Determines whether @file is somewhere inside @ancestor,
 * possibly with intermediate folders.
 *
 * Return value: %TRUE if @ancestor contains @file as a
 *               child, grandchild, great grandchild, etc.
 **/
gboolean th_file_is_gfile_ancestor(const ThunarFile *file,
                                       GFile            *ancestor)
{
    GFile   *current = NULL;
    GFile   *tmp;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(G_IS_FILE(file->gfile), FALSE);
    thunar_return_val_if_fail(G_IS_FILE(ancestor), FALSE);

    current = g_object_ref(file->gfile);
    while(TRUE)
    {
        tmp = g_file_get_parent(current);
        g_object_unref(current);
        current = tmp;

        if (current == NULL) /* parent of root is NULL */
            return FALSE;

        if (G_UNLIKELY(g_file_equal(current, ancestor)))
        {
            g_object_unref(current);
            return TRUE;
        }
    }

    return FALSE;
}

/**
 * thunar_file_is_ancestor:
 * @file     : a #ThunarFile instance.
 * @ancestor : another #ThunarFile instance.
 *
 * Determines whether @file is somewhere inside @ancestor,
 * possibly with intermediate folders.
 *
 * Return value: %TRUE if @ancestor contains @file as a
 *               child, grandchild, great grandchild, etc.
 **/
gboolean th_file_is_ancestor(const ThunarFile *file,
                                 const ThunarFile *ancestor)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(THUNAR_IS_FILE(ancestor), FALSE);

    return th_file_is_gfile_ancestor(file, ancestor->gfile);
}

/**
 * thunar_file_is_executable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to execute the @file(or enter the directory refered to by
 * @file). On UNIX it also returns %TRUE if @file refers to a
 * desktop entry.
 *
 * Return value: %TRUE if @file can be executed.
 **/
gboolean th_file_is_executable(const ThunarFile *file)
{
    gboolean           can_execute = FALSE;
    gboolean           exec_shell_scripts = FALSE;
    const gchar       *content_type;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    if (g_file_info_get_attribute_boolean(file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    {
        /* get the content type of the file */
        content_type = th_file_get_content_type(THUNAR_FILE(file));
        if (G_LIKELY(content_type != NULL))
        {
            can_execute = g_content_type_can_be_executable(content_type);

            if (can_execute)
            {
                /* do never execute plain text files which are not shell scripts but marked executable */
                if (g_strcmp0(content_type, "text/plain") == 0)
                    can_execute = FALSE;
                else if (g_content_type_is_a(content_type, "text/plain") && ! exec_shell_scripts)
                    can_execute = FALSE;
            }
        }
    }

    return can_execute || th_file_is_desktop_file(file, NULL);
}

/**
 * thunar_file_is_writable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to write the @file.
 *
 * Return value: %TRUE if @file can be read.
 **/
gboolean th_file_is_writable(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    if (!g_file_info_has_attribute(file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE))
        return TRUE;

    return g_file_info_get_attribute_boolean(file->info,
            G_FILE_ATTRIBUTE_ACCESS_CAN_WRITE);
}

/**
 * thunar_file_is_hidden:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file can be considered a hidden file.
 *
 * Return value: %TRUE if @file is a hidden file, else %FALSE.
 **/
gboolean th_file_is_hidden(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    return g_file_info_get_is_hidden(file->info)
           || g_file_info_get_is_backup(file->info);
}

/**
 * thunar_file_is_regular:
 * @file : a #ThunarFile.
 *
 * Checks whether @file refers to a regular file.
 *
 * Return value: %TRUE if @file is a regular file.
 **/
gboolean th_file_is_regular(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return file->kind == G_FILE_TYPE_REGULAR;
}

/**
 * thunar_file_is_trashed:
 * @file : a #ThunarFile instance.
 *
 * Returns %TRUE if @file is a local file that resides in
 * the trash bin.
 *
 * Return value: %TRUE if @file is in the trash, or
 *               the trash folder itself.
 **/
gboolean th_file_is_trashed(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    return eg_file_is_trashed(file->gfile);
}

/**
 * thunar_file_is_desktop_file:
 * @file      : a #ThunarFile.
 * @is_secure : if %NULL do a simple check, else it will set this boolean
 *              to indicate if the desktop file is safe see bug #5012
 *              for more info.
 *
 * Returns %TRUE if @file is a .desktop file. The @is_secure return value
 * will tell if the .desktop file is also secure.
 *
 * Return value: %TRUE if @file is a .desktop file.
 **/
gboolean th_file_is_desktop_file(const ThunarFile *file,
                                     gboolean         *is_secure)
{
    const gchar * const *data_dirs;
    guint                n;
    gchar               *path;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    /* only allow regular files with a .desktop extension */
    if (!g_str_has_suffix(file->basename, ".desktop")
            || file->kind != G_FILE_TYPE_REGULAR)
        return FALSE;

    /* don't check more if not needed */
    if (is_secure == NULL)
        return TRUE;

    /* desktop files outside xdg directories need to be executable for security reasons */
    if (g_file_info_get_attribute_boolean(file->info, G_FILE_ATTRIBUTE_ACCESS_CAN_EXECUTE))
    {
        /* has +x */
        *is_secure = TRUE;
    }
    else
    {
        /* assume the file is not safe */
        *is_secure = FALSE;

        /* deskopt files in xdg directories are also fine... */
        if (g_file_is_native(th_file_get_file(file)))
        {
            data_dirs = g_get_system_data_dirs();
            if (G_LIKELY(data_dirs != NULL))
            {
                path = g_file_get_path(th_file_get_file(file));
                for(n = 0; data_dirs[n] != NULL; n++)
                {
                    if (g_str_has_prefix(path, data_dirs[n]))
                    {
                        /* has known prefix, can launch without problems */
                        *is_secure = TRUE;
                        break;
                    }
                }
                g_free(path);
            }
        }
    }

    return TRUE;
}

const gchar* th_file_get_display_name(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    return file->display_name;
}

/**
 * thunar_file_get_deletion_date:
 * @file       : a #ThunarFile instance.
 * @date_style : the style used to format the date.
 * @date_custom_style : custom style to apply, if @date_style is set to custom
 *
 * Returns the deletion date of the @file if the @file
 * is located in the trash. Otherwise %NULL will be
 * returned.
 *
 * The caller is responsible to free the returned string
 * using g_free() when no longer needed.
 *
 * Return value: the deletion date of @file if @file is
 *               in the trash, %NULL otherwise.
 **/
gchar* th_file_get_deletion_date(const ThunarFile   *file,
                                     ThunarDateStyle    date_style,
                                     const gchar        *date_custom_style)
{
    const gchar *date;
    time_t       deletion_time;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(G_IS_FILE_INFO(file->info), NULL);

    date = g_file_info_get_attribute_string(file->info, G_FILE_ATTRIBUTE_TRASH_DELETION_DATE);
    if (G_UNLIKELY(date == NULL))
        return NULL;

    /* try to parse the DeletionDate(RFC 3339 string) */
    deletion_time = thunar_util_time_from_rfc3339(date);

    /* humanize the time value */
    return thunar_util_humanize_file_time(deletion_time, date_style, date_custom_style);
}

/**
 * thunar_file_get_original_path:
 * @file : a #ThunarFile instance.
 *
 * Returns the original path of the @file if the @file
 * is located in the trash. Otherwise %NULL will be
 * returned.
 *
 * Return value: the original path of @file if @file is
 *               in the trash, %NULL otherwise.
 **/
const gchar* th_file_get_original_path(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);

    if (file->info == NULL)
        return NULL;

    return g_file_info_get_attribute_byte_string(file->info, G_FILE_ATTRIBUTE_TRASH_ORIG_PATH);
}

/**
 * thunar_file_get_item_count:
 * @file : a #ThunarFile instance.
 *
 * Returns the number of items in the trash, if @file refers to the
 * trash root directory. Otherwise returns 0.
 *
 * Return value: number of files in the trash if @file is the trash
 *               root dir, 0 otherwise.
 **/
guint32 th_file_get_item_count(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), 0);

    if (file->info == NULL)
        return 0;

    return g_file_info_get_attribute_uint32(file->info,
            G_FILE_ATTRIBUTE_TRASH_ITEM_COUNT);
}

/**
 * thunar_file_is_chmodable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether the owner of the current process is allowed
 * to changed the file mode of @file.
 *
 * Return value: %TRUE if the mode of @file can be changed.
 **/
gboolean th_file_is_chmodable(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    /* we can only change the mode if we the euid is
     *   a) equal to the file owner id
     * or
     *   b) the super-user id
     * and the file is not in the trash.
     */
    if (file->info == NULL)
    {
        return(_effective_user_id == 0 && !th_file_is_trashed(file));
    }
    else
    {
        return((_effective_user_id == 0
                 || _effective_user_id == g_file_info_get_attribute_uint32(file->info,
                         G_FILE_ATTRIBUTE_UNIX_UID))
                && !th_file_is_trashed(file));
    }
}

/**
 * thunar_file_is_renameable:
 * @file : a #ThunarFile instance.
 *
 * Determines whether @file can be renamed using
 * #thunar_file_rename(). Note that the return
 * value is just a guess and #thunar_file_rename()
 * may fail even if this method returns %TRUE.
 *
 * Return value: %TRUE if @file can be renamed.
 **/
gboolean th_file_is_renameable(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    return g_file_info_get_attribute_boolean(file->info,
                                             G_FILE_ATTRIBUTE_ACCESS_CAN_RENAME);
}

gboolean th_file_can_be_trashed(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    if (file->info == NULL)
        return FALSE;

    return g_file_info_get_attribute_boolean(file->info,
            G_FILE_ATTRIBUTE_ACCESS_CAN_TRASH);
}

/**
 * thunar_file_get_custom_icon:
 * @file : a #ThunarFile instance.
 *
 * Queries the custom icon from @file if any, else %NULL is returned.
 * The custom icon can be either a themed icon name or an absolute path
 * to an icon file in the local file system.
 *
 * Return value: the custom icon for @file or %NULL.
 **/
const gchar* th_file_get_custom_icon(const ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    return file->custom_icon_name;
}

static const gchar* thunar_file_get_icon_name_for_state(
                                                const gchar         *icon_name,
                                                ThunarFileIconState icon_state)
{
    if (!icon_name || !*icon_name)
        return NULL;

    /* check if we have an accept icon for the icon we found */
    if (icon_state != THUNAR_FILE_ICON_STATE_DEFAULT
            &&(strcmp(icon_name, "inode-directory") == 0
                || strcmp(icon_name, "folder") == 0))
    {
        if (icon_state == THUNAR_FILE_ICON_STATE_DROP)
            return "folder-drag-accept";
        else if (icon_state == THUNAR_FILE_ICON_STATE_OPEN)
            return "folder-open";
    }

    return icon_name;
}

/**
 * thunar_file_get_icon_name:
 * @file       : a #ThunarFile instance.
 * @icon_state : the state of the @file<!---->s icon we are interested in.
 * @icon_theme : the #GtkIconTheme on which to lookup up the icon name.
 *
 * Returns the name of the icon that can be used to present @file, based
 * on the given @icon_state and @icon_theme.
 *
 * Return value: the icon name for @file in @icon_theme.
 **/
const gchar* th_file_get_icon_name(ThunarFile           *file,
                                       ThunarFileIconState  icon_state,
                                       GtkIconTheme         *icon_theme)
{
    GFile               *icon_file;
    GIcon               *icon = NULL;
    const gchar * const *names;
    gchar               *icon_name = NULL;
    gchar               *path;
    const gchar         *special_names[] = { NULL, "folder", NULL };
    guint                i;
    const gchar         *special_dir;
    GFileInfo           *fileinfo;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), NULL);
    thunar_return_val_if_fail(GTK_IS_ICON_THEME(icon_theme), NULL);

    /* return cached name */
    if (G_LIKELY(file->icon_name != NULL))
        return thunar_file_get_icon_name_for_state(file->icon_name, icon_state);

    /* the system root folder has a special icon */
    if (th_file_is_directory(file))
    {
        if (G_LIKELY(th_file_is_local(file)))
        {
            path = g_file_get_path(file->gfile);
            if (G_LIKELY(path != NULL))
            {
                if (strcmp(path, G_DIR_SEPARATOR_S) == 0)
                    *special_names = "drive-harddisk";
                else if (strcmp(path, g_get_home_dir()) == 0)
                    *special_names = "user-home";
                else
                {
                    for(i = 0; i < G_N_ELEMENTS(thunar_file_dirs); i++)
                    {
                        special_dir = g_get_user_special_dir(thunar_file_dirs[i].type);
                        if (special_dir != NULL
                                && strcmp(path, special_dir) == 0)
                        {
                            *special_names = thunar_file_dirs[i].icon_name;
                            break;
                        }
                    }
                }

                g_free(path);
            }
        }
        else if (!th_file_has_parent(file))
        {
            if (g_file_has_uri_scheme(file->gfile, "trash"))
            {
                special_names[0] = th_file_get_item_count(file) > 0 ? "user-trash-full" : "user-trash";
                special_names[1] = "user-trash";
            }
            else if (g_file_has_uri_scheme(file->gfile, "network"))
            {
                special_names[0] = "network-workgroup";
            }
            else if (g_file_has_uri_scheme(file->gfile, "recent"))
            {
                special_names[0] = "document-open-recent";
            }
            else if (g_file_has_uri_scheme(file->gfile, "computer"))
            {
                special_names[0] = "computer";
            }
        }

        if (*special_names != NULL)
        {
            names = special_names;
            goto check_names;
        }
    }
    else if (th_file_is_mountable(file)
             || g_file_has_uri_scheme(file->gfile, "network"))
    {
        /* query the icon(computer:// and network:// backend) */
        fileinfo = g_file_query_info(file->gfile,
                                      G_FILE_ATTRIBUTE_STANDARD_ICON,
                                      G_FILE_QUERY_INFO_NONE, NULL, NULL);
        if (G_LIKELY(fileinfo != NULL))
        {
            /* take the icon from the info */
            icon = g_file_info_get_icon(fileinfo);
            if (G_LIKELY(icon != NULL))
                g_object_ref(icon);

            /* release */
            g_object_unref(G_OBJECT(fileinfo));

            if (G_LIKELY(icon != NULL))
                goto check_icon;
        }
    }

    /* try again later */
    if (file->info == NULL)
        return NULL;

    /* lookup for content type, just like gio does for local files */
    icon = g_content_type_get_icon(th_file_get_content_type(file));
    if (G_LIKELY(icon != NULL))
    {
check_icon:
        if (G_IS_THEMED_ICON(icon))
        {
            names = g_themed_icon_get_names(G_THEMED_ICON(icon));

check_names:

            if (G_LIKELY(names != NULL))
            {
                for(i = 0; names[i] != NULL; ++i)
                    if (*names[i] != '(' /* see gnome bug 688042 */
                            && gtk_icon_theme_has_icon(icon_theme, names[i]))
                    {
                        icon_name = g_strdup(names[i]);
                        break;
                    }
            }
        }
        else if (G_IS_FILE_ICON(icon))
        {
            icon_file = g_file_icon_get_file(G_FILE_ICON(icon));
            if (icon_file != NULL)
                icon_name = g_file_get_path(icon_file);
        }

        if (G_LIKELY(icon != NULL))
            g_object_unref(icon);
    }

    /* store new name, fallback to legacy names, or empty string to avoid recursion */
    g_free(file->icon_name);
    if (G_LIKELY(icon_name != NULL))
        file->icon_name = icon_name;
    else if (file->kind == G_FILE_TYPE_DIRECTORY
             && gtk_icon_theme_has_icon(icon_theme, "folder"))
        file->icon_name = g_strdup("folder");
    else
        file->icon_name = g_strdup("");

    return thunar_file_get_icon_name_for_state(file->icon_name, icon_state);
}


// Monitor --------------------------------------------------------------------
/**
 * thunar_file_watch:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to watch itself for changes. Not all #ThunarFile
 * implementations must support this, but if a #ThunarFile
 * implementation implements the thunar_file_watch() method,
 * it must also implement the thunar_file_unwatch() method.
 *
 * The #ThunarFile base class implements automatic "ref
 * counting" for watches, that says, you can call thunar_file_watch()
 * multiple times, but the virtual method will be invoked only
 * once. This also means that you MUST call thunar_file_unwatch()
 * for every thunar_file_watch() invokation, else the application
 * will abort.
 **/
void th_file_watch(ThunarFile *file)
{
    ThunarFileWatch *file_watch;
    GError          *error = NULL;

    thunar_return_if_fail(THUNAR_IS_FILE(file));

    file_watch = g_object_get_qdata(G_OBJECT(file), _file_watch_quark);
    if (file_watch == NULL)
    {
        file_watch = g_slice_new(ThunarFileWatch);
        file_watch->watch_count = 1;

        /* create a file or directory monitor */
        file_watch->monitor = g_file_monitor(file->gfile, G_FILE_MONITOR_WATCH_MOUNTS |
                                              G_FILE_MONITOR_WATCH_MOVES, NULL, &error);

        if (G_UNLIKELY(file_watch->monitor == NULL))
        {
            g_debug("Failed to create file monitor: %s", error->message);
            g_error_free(error);
            file->no_file_watch = TRUE;
        }
        else
        {
            /* watch monitor for file changes */
            g_signal_connect(file_watch->monitor, "changed", G_CALLBACK(_th_file_monitor), file);
        }

        /* attach to file */
        g_object_set_qdata_full(G_OBJECT(file), _file_watch_quark, file_watch, _th_file_watch_destroyed);
    }
    else if (G_LIKELY(!file->no_file_watch))
    {
        /* increase watch count */
        thunar_return_if_fail(G_IS_FILE_MONITOR(file_watch->monitor));
        file_watch->watch_count++;
    }
}

static void _th_file_watch_destroyed(gpointer data)
{
    ThunarFileWatch *file_watch = data;

    if (G_LIKELY(file_watch->monitor != NULL))
    {
        g_file_monitor_cancel(file_watch->monitor);
        g_object_unref(file_watch->monitor);
    }

    g_slice_free(ThunarFileWatch, file_watch);
}

static void _th_file_monitor(GFileMonitor *monitor,
                                GFile        *event_path,
                                GFile        *other_path,
                                GFileMonitorEvent event_type,
                                gpointer     user_data)
{
    ThunarFile *file = THUNAR_FILE(user_data);
    ThunarFile *other_file;

    thunar_return_if_fail(G_IS_FILE_MONITOR(monitor));
    thunar_return_if_fail(G_IS_FILE(event_path));
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    if (g_file_equal(event_path, file->gfile))
    {
        /* the event occurred for the monitored ThunarFile */
        if (event_type == G_FILE_MONITOR_EVENT_RENAMED
            || event_type == G_FILE_MONITOR_EVENT_MOVED_IN
            || event_type == G_FILE_MONITOR_EVENT_MOVED_OUT)
        {
            G_LOCK(_file_rename_mutex);
            _th_file_monitor_moved(file, other_path);
            G_UNLOCK(_file_rename_mutex);
            return;
        }

        if (G_LIKELY(event_path))
            _th_file_monitor_update(event_path, event_type);
    }
    else
    {
        /* The event did not occur for the monitored ThunarFile, but for
           a file that is contained in ThunarFile which is actually a
           directory. */

        if (event_type == G_FILE_MONITOR_EVENT_RENAMED
            || event_type == G_FILE_MONITOR_EVENT_MOVED_IN
            || event_type == G_FILE_MONITOR_EVENT_MOVED_OUT)
        {
            /* reload the target file if cached */
            if (other_path == NULL)
                return;

            G_LOCK(_file_rename_mutex);

            other_file = th_file_cache_lookup(other_path);
            if (other_file)
                th_file_reload(other_file);
            else
                other_file = th_file_get(other_path, NULL);

            if (other_file != NULL)
            {
                /* notify the thumbnail cache that we can now also move the thumbnail */

                //thunar_file_move_thumbnail_cache_file(event_path, other_path);

                /* reload the containing target folder */
                _th_file_reload_parent(other_file);

                g_object_unref(other_file);
            }

            G_UNLOCK(_file_rename_mutex);
        }
        return;
    }
}

static void _th_file_monitor_moved(ThunarFile *file, GFile *renamed_file)
{
    /* ref the old location */
    GFile *previous_file = G_FILE(g_object_ref(G_OBJECT(file->gfile)));

    /* notify the thumbnail cache that we can now also move the thumbnail */
    //thunar_file_move_thumbnail_cache_file(previous_file, renamed_file);

    /* set the new file */
    file->gfile = G_FILE(g_object_ref(G_OBJECT(renamed_file)));

    /* reload file information */
    _th_file_load(file, NULL, NULL);

    /* need to re-register the monitor handle for the new uri */
    _th_file_watch_reconnect(file);

    G_LOCK(_file_cache_mutex);

    /* drop the previous entry from the cache */
    g_hash_table_remove(_file_cache, previous_file);

    /* drop the reference on the previous file */
    g_object_unref(previous_file);

    /* insert the new entry */
    g_hash_table_insert(_file_cache,
                         g_object_ref(file->gfile),
                         weak_ref_new(G_OBJECT(file)));

    G_UNLOCK(_file_cache_mutex);
}

static void _th_file_watch_reconnect(ThunarFile *file)
{
    ThunarFileWatch *file_watch;

    /* recreate the monitor without changing the watch_count for file renames */
    file_watch = g_object_get_qdata(G_OBJECT(file), _file_watch_quark);
    if (file_watch != NULL)
    {
        /* reset the old monitor */
        if (G_LIKELY(file_watch->monitor != NULL))
        {
            g_file_monitor_cancel(file_watch->monitor);
            g_object_unref(file_watch->monitor);
        }

        /* create a file or directory monitor */
        file_watch->monitor = g_file_monitor(file->gfile, G_FILE_MONITOR_WATCH_MOUNTS | G_FILE_MONITOR_SEND_MOVED, NULL, NULL);
        if (G_LIKELY(file_watch->monitor != NULL))
        {
            /* watch monitor for file changes */
            g_signal_connect(file_watch->monitor, "changed", G_CALLBACK(_th_file_monitor), file);
        }
    }
}

static void _th_file_monitor_update(GFile *path, GFileMonitorEvent event_type)
{
    thunar_return_if_fail(G_IS_FILE(path));

    ThunarFile *file = th_file_cache_lookup(path);

    if (G_LIKELY(file != NULL))
    {
        switch (event_type)
        {
        case G_FILE_MONITOR_EVENT_CREATED:
        case G_FILE_MONITOR_EVENT_ATTRIBUTE_CHANGED:
        case G_FILE_MONITOR_EVENT_CHANGES_DONE_HINT:
        case G_FILE_MONITOR_EVENT_PRE_UNMOUNT:
        case G_FILE_MONITOR_EVENT_DELETED:
            th_file_reload(file);
            break;

        default:
            break;
        }

        g_object_unref(file);
    }
}

static void _th_file_reload_parent(ThunarFile *file)
{
    ThunarFile *parent = NULL;

    thunar_return_if_fail(THUNAR_IS_FILE(file));

    if (th_file_has_parent(file))
    {
        GFile *parent_file;

        /* only reload file if it is in cache */
        parent_file = g_file_get_parent(file->gfile);
        parent = th_file_cache_lookup(parent_file);
        g_object_unref(parent_file);
    }

    if (parent)
    {
        th_file_reload(parent);
        g_object_unref(parent);
    }
}

/**
 * thunar_file_unwatch:
 * @file : a #ThunarFile instance.
 *
 * See thunar_file_watch() for a description of how watching
 * #ThunarFile<!---->s works.
 **/
void th_file_unwatch(ThunarFile *file)
{
    ThunarFileWatch *file_watch;

    thunar_return_if_fail(THUNAR_IS_FILE(file));

    if (G_UNLIKELY(file->no_file_watch))
    {
        return;
    }

    file_watch = g_object_get_qdata(G_OBJECT(file), _file_watch_quark);
    if (file_watch != NULL)
    {
        /* remove if this was the last ref */
        if (--file_watch->watch_count == 0)
            g_object_set_qdata(G_OBJECT(file), _file_watch_quark, NULL);
    }
    else
    {
        thunar_assert_not_reached();
    }
}

/**
 * thunar_file_reload:
 * @file : a #ThunarFile instance.
 *
 * Tells @file to reload its internal state, e.g. by reacquiring
 * the file info from the underlying media.
 *
 * You must be able to handle the case that @file is
 * destroyed during the reload call.
 *
 * Return value: As this function can be used as a callback function
 * for thunar_file_reload_idle, it will always return FALSE to prevent
 * being called repeatedly.
 **/
gboolean th_file_reload(ThunarFile *file)
{
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    /* clear file pxmap cache */
    thunar_icon_factory_clear_pixmap_cache(file);

    if (!_th_file_load(file, NULL, NULL))
    {
        /* destroy the file if we cannot query any file information */
        th_file_destroy(file);
        return FALSE;
    }

    /* ... and tell others */
    th_file_changed(file);

    return FALSE;
}

/**
 * thunar_file_reload_idle_unref:
 * @file : a #ThunarFile instance.
 *
 * Schedules a reload of the @file by calling thunar_file_reload
 * when idle. When scheduled function returns @file object will be
 * unreferenced.
 *
 **/
void th_file_reload_idle_unref(ThunarFile *file)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    g_idle_add_full(G_PRIORITY_DEFAULT_IDLE,
                    (GSourceFunc) th_file_reload,
                     file,
                    (GDestroyNotify) g_object_unref);
}

/**
 * thunar_file_destroy:
 * @file : a #ThunarFile instance.
 *
 * Emits the ::destroy signal notifying all reference holders
 * that they should release their references to the @file.
 *
 * This method is very similar to what gtk_object_destroy()
 * does for #GtkObject<!---->s.
 **/
void th_file_destroy(ThunarFile *file)
{
    thunar_return_if_fail(THUNAR_IS_FILE(file));

    if (!FLAG_IS_SET(file, THUNAR_FILE_FLAG_IN_DESTRUCTION))
    {
        /* take an additional reference on the file, as the file-destroyed
         * invocation may already release the last reference.
         */
        g_object_ref(G_OBJECT(file));

        /* tell the file monitor that this file was destroyed */
        thunar_file_monitor_file_destroyed(file);

        /* run the dispose handler */
        g_object_run_dispose(G_OBJECT(file));

        /* release our reference */
        g_object_unref(G_OBJECT(file));
    }
}

/**
 * thunar_file_compare_by_type:
 * @a : the first #ThunarFile.
 * @b : the second #ThunarFile.
 *
 * Compares items so that directories come before files and ancestors come
 * before descendants.
 *
 * Return value: -1 if @file_a should be sorted before @file_b, 1 if
 *               @file_b should be sorted before @file_a, 0 if equal.
 **/
gint th_file_compare_by_type(ThunarFile *a, ThunarFile *b)
{
    GFile *file_a;
    GFile *file_b;
    GFile *parent_a;
    GFile *parent_b;
    gint   ret = 0;

    file_a = th_file_get_file(a);
    file_b = th_file_get_file(b);

    /* check whether the files are equal */
    if (g_file_equal(file_a, file_b))
        return 0;

    /* directories always come first */
    if (a->kind == G_FILE_TYPE_DIRECTORY
            && b->kind != G_FILE_TYPE_DIRECTORY)
    {
        return -1;
    }
    if (a->kind != G_FILE_TYPE_DIRECTORY
            && b->kind == G_FILE_TYPE_DIRECTORY)
    {
        return 1;
    }

    /* ancestors come first */
    if (g_file_has_prefix(file_b, file_a))
        return -1;
    if (g_file_has_prefix(file_a, file_b))
        return 1;

    parent_a = g_file_get_parent(file_a);
    parent_b = g_file_get_parent(file_b);

    if (g_file_equal(parent_a, parent_b))
    {
        /* compare siblings by their display name */
        ret = g_utf8_collate(a->display_name,
                              b->display_name);
    }
    /* again, ancestors come first */
    else if (g_file_has_prefix(file_b, parent_a))
        ret = -1;
    else if (g_file_has_prefix(file_a, parent_b))
        ret = 1;

    g_object_unref(parent_a);
    g_object_unref(parent_b);

    return ret;
}

/**
 * thunar_file_compare_by_name:
 * @file_a         : the first #ThunarFile.
 * @file_b         : the second #ThunarFile.
 * @case_sensitive : whether the comparison should be case-sensitive.
 *
 * Compares @file_a and @file_b by their display names. If @case_sensitive
 * is %TRUE the comparison will be case-sensitive.
 *
 * Return value: -1 if @file_a should be sorted before @file_b, 1 if
 *               @file_b should be sorted before @file_a, 0 if equal.
 **/
gint th_file_compare_by_name(const ThunarFile   *file_a,
                                 const ThunarFile   *file_b,
                                 gboolean           case_sensitive)
{
    gint result = 0;

#ifdef G_ENABLE_DEBUG
    /* probably too expensive to do the instance check every time
     * this function is called, so only for debugging builds.
     */
    thunar_return_val_if_fail(THUNAR_IS_FILE(file_a), 0);
    thunar_return_val_if_fail(THUNAR_IS_FILE(file_b), 0);
#endif

    /* case insensitive checking */
    if (G_LIKELY(!case_sensitive))
        result = g_strcmp0(file_a->collate_key_nocase, file_b->collate_key_nocase);

    /* fall-back to case sensitive */
    if (result == 0)
        result = g_strcmp0(file_a->collate_key, file_b->collate_key);

    /* this happens in the trash */
    if (result == 0)
    {
        result = g_strcmp0(th_file_get_original_path(file_a),
                            th_file_get_original_path(file_b));
    }

    return result;
}

/**
 * thunar_file_cache_lookup:
 * @file : a #GFile.
 *
 * Looks up the #ThunarFile for @file in the internal file
 * cache and returns the file present for @file in the
 * cache or %NULL if no #ThunarFile is cached for @file.
 *
 * Note that no reference is taken for the caller.
 *
 * This method should not be used but in very rare cases.
 * Consider using thunar_file_get() instead.
 *
 * Return value: the #ThunarFile for @file in the internal
 *               cache, or %NULL. If you are done with the
 *               file, use g_object_unref to release.
 **/
ThunarFile* th_file_cache_lookup(const GFile *file)
{
    GWeakRef   *ref;
    ThunarFile *cached_file;

    thunar_return_val_if_fail(G_IS_FILE(file), NULL);

    G_LOCK(_file_cache_mutex);

    /* allocate the ThunarFile cache on-demand */
    if (G_UNLIKELY(_file_cache == NULL))
    {
        _file_cache = g_hash_table_new_full(g_file_hash,
                                           (GEqualFunc) g_file_equal,
                                           (GDestroyNotify) g_object_unref,
                                           (GDestroyNotify) weak_ref_free);
    }

    ref = g_hash_table_lookup(_file_cache, file);

    if (ref == NULL)
        cached_file = NULL;
    else
        cached_file = g_weak_ref_get(ref);

    G_UNLOCK(_file_cache_mutex);

    return cached_file;
}

gchar* th_file_cached_display_name(const GFile *file)
{
    ThunarFile *cached_file;
    gchar      *display_name;

    /* check if we have a ThunarFile for it in the cache(usually is the case) */
    cached_file = th_file_cache_lookup(file);
    if (cached_file != NULL)
    {
        /* determine the display name of the file */
        display_name = g_strdup(th_file_get_display_name(cached_file));
        g_object_unref(cached_file);
    }
    else
    {
        /* determine something a hopefully good approximation of the display name */
        display_name = eg_file_get_display_name(G_FILE(file));
    }

    return display_name;
}

static gint compare_app_infos(gconstpointer a, gconstpointer b)
{
    return g_app_info_equal(G_APP_INFO(a), G_APP_INFO(b)) ? 0 : 1;
}

/**
 * thunar_file_list_get_applications:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Returns the #GList of #GAppInfo<!---->s that can be used to open
 * all #ThunarFile<!---->s in the given @file_list.
 *
 * The caller is responsible to free the returned list using something like:
 * <informalexample><programlisting>
 * g_list_free_full(list, g_object_unref);
 * </programlisting></informalexample>
 *
 * Return value: the list of #GAppInfo<!---->s that can be used to open all
 *               items in the @file_list.
 **/
GList* th_file_list_get_applications(GList *file_list)
{
    GList       *applications = NULL;
    GList       *list;
    GList       *next;
    GList       *ap;
    GList       *lp;
    GAppInfo    *default_application;
    const gchar *previous_type = NULL;
    const gchar *current_type;

    /* determine the set of applications that can open all files */
    for(lp = file_list; lp != NULL; lp = lp->next)
    {
        current_type = th_file_get_content_type(lp->data);

        /* no need to check anything if this file has the same mimetype as the previous file */
        if (current_type != NULL && previous_type != NULL)
            if (G_LIKELY(g_content_type_equals(previous_type, current_type)))
                continue;

        /* store the previous type */
        previous_type = current_type;

        /* determine the list of applications that can open this file */
        if (G_UNLIKELY(current_type != NULL))
        {
            list = g_app_info_get_all_for_type(current_type);

            /* move any default application in front of the list */
            default_application = g_app_info_get_default_for_type(current_type, FALSE);
            if (G_LIKELY(default_application != NULL))
            {
                for(ap = list; ap != NULL; ap = ap->next)
                {
                    if (g_app_info_equal(ap->data, default_application))
                    {
                        g_object_unref(ap->data);
                        list = g_list_delete_link(list, ap);
                        break;
                    }
                }
                list = g_list_prepend(list, default_application);
            }
        }
        else
            list = NULL;

        if (G_UNLIKELY(applications == NULL))
        {
            /* first file, so just use the applications list */
            applications = list;
        }
        else
        {
            /* keep only the applications that are also present in list */
            for(ap = applications; ap != NULL; ap = next)
            {
                /* grab a pointer on the next application */
                next = ap->next;

                /* check if the application is present in list */
                if (g_list_find_custom(list, ap->data, compare_app_infos) == NULL)
                {
                    /* drop our reference on the application */
                    g_object_unref(G_OBJECT(ap->data));

                    /* drop this application from the list */
                    applications = g_list_delete_link(applications, ap);
                }
            }

            /* release the list of applications for this file */
            g_list_free_full(list, g_object_unref);
        }

        /* check if the set is still not empty */
        if (G_LIKELY(applications == NULL))
            break;
    }

    /* remove hidden applications */
    for(ap = applications; ap != NULL; ap = next)
    {
        /* grab a pointer on the next application */
        next = ap->next;

        if (!eg_app_info_should_show(ap->data))
        {
            /* drop our reference on the application */
            g_object_unref(G_OBJECT(ap->data));

            /* drop this application from the list */
            applications = g_list_delete_link(applications, ap);
        }
    }

    return applications;
}

/**
 * thunar_file_list_to_thunar_g_file_list:
 * @file_list : a #GList of #ThunarFile<!---->s.
 *
 * Transforms the @file_list to a #GList of #GFile<!---->s for
 * the #ThunarFile<!---->s contained within @file_list.
 *
 * The caller is responsible to free the returned list using
 * eg_list_free() when no longer needed.
 *
 * Return value: the list of #GFile<!---->s for @file_list.
 **/
GList* th_file_list_to_thunar_g_file_list(GList *file_list)
{
    GList *list = NULL;
    GList *lp;

    for(lp = g_list_last(file_list); lp != NULL; lp = lp->prev)
        list = g_list_prepend(list, g_object_ref(THUNAR_FILE(lp->data)->gfile));

    return list;
}


