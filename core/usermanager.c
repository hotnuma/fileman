/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#include "config.h"
#include "usermanager.h"

#include <grp.h>
#include <pwd.h>
#include "utils.h"

// the interval in which the user/group cache is flushed(in seconds)
#define USER_MANAGER_FLUSH_INTERVAL (10 * 60)

// User Manager ---------------------------------------------------------------

static void _usermanager_finalize(GObject *object);
static gboolean _usermanager_flush_timer(gpointer user_data);
static void _usermanager_flush_timer_destroy(gpointer user_data);

// Group ----------------------------------------------------------------------

static ThunarGroup* _group_new(guint32 id);
static void _group_finalize(GObject *object);

// User -----------------------------------------------------------------------

static ThunarUser* _user_new(guint32 id);
static void _user_finalize(GObject *object);
static void _user_load(ThunarUser *user);
static ThunarGroup* _user_get_primary_group(ThunarUser *user);

// User Manager ---------------------------------------------------------------

struct _UserManagerClass
{
    GObjectClass __parent__;
};

struct _UserManager
{
    GObject __parent__;

    GHashTable *groups;
    GHashTable *users;

    guint       flush_timer_id;
};

G_DEFINE_TYPE(UserManager, usermanager, G_TYPE_OBJECT)

static void usermanager_class_init(UserManagerClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = _usermanager_finalize;
}

static void usermanager_init(UserManager *manager)
{
    manager->groups = g_hash_table_new_full(g_direct_hash,
                                            g_direct_equal,
                                            NULL,
                                            g_object_unref);
    manager->users = g_hash_table_new_full(g_direct_hash,
                                           g_direct_equal,
                                           NULL,
                                           g_object_unref);

    // keep the groups file in memory if possible
#ifdef HAVE_SETGROUPENT
    setgroupent(TRUE);
#endif

    // keep the passwd file in memory if possible
#ifdef HAVE_SETPASSENT
    setpassent(TRUE);
#endif

    // start the flush timer
    manager->flush_timer_id = g_timeout_add_seconds_full(
                                            G_PRIORITY_LOW,
                                            USER_MANAGER_FLUSH_INTERVAL,
                                            _usermanager_flush_timer, manager,
                                            _usermanager_flush_timer_destroy);
}

static void _usermanager_finalize(GObject *object)
{
    UserManager *manager = USERMANAGER(object);

    // stop the flush timer
    if (manager->flush_timer_id != 0)
        g_source_remove(manager->flush_timer_id);

    // destroy the hash tables
    g_hash_table_destroy(manager->groups);
    g_hash_table_destroy(manager->users);

    // unload the groups file
    endgrent();

    // unload the passwd file
    endpwent();

    G_OBJECT_CLASS(usermanager_parent_class)->finalize(object);
}

static gboolean _usermanager_flush_timer(gpointer user_data)
{
    UserManager *manager = USERMANAGER(user_data);
    guint size = 0;

    UTIL_THREADS_ENTER

    // drop all cached groups
    size += g_hash_table_foreach_remove(manager->groups,
                                        (GHRFunc)(void(*)(void)) gtk_true,
                                        NULL);

    // drop all cached users
    size += g_hash_table_foreach_remove(manager->users,
                                        (GHRFunc)(void(*)(void)) gtk_true,
                                        NULL);

    // reload groups and passwd files if we had cached entities
    if (size > 0)
    {
        endgrent();
        endpwent();

#ifdef HAVE_SETGROUPENT
        setgroupent(TRUE);
#endif

#ifdef HAVE_SETPASSENT
        setpassent(TRUE);
#endif
    }

    UTIL_THREADS_LEAVE

    return TRUE;
}

static void _usermanager_flush_timer_destroy(gpointer user_data)
{
    USERMANAGER(user_data)->flush_timer_id = 0;
}

UserManager* usermanager_get_default()
{
    // g_object_unref when unneeded

    static UserManager *manager = NULL;

    if (manager == NULL)
    {
        manager = g_object_new(TYPE_USERMANAGER, NULL);
        g_object_add_weak_pointer(G_OBJECT(manager),(gpointer) &manager);
    }
    else
    {
        g_object_ref(G_OBJECT(manager));
    }

    return manager;
}

GList* usermanager_get_all_groups(UserManager *manager)
{
    // g_list_free_full(list, g_object_unref)

    ThunarGroup  *group;
    struct group *grp;
    GList        *groups = NULL;

    g_return_val_if_fail(IS_USERMANAGER(manager), NULL);

    // make sure we reload the groups list
    endgrent();

    // iterate through all groups in the system
    for (;;)
    {
        // lookup the next group
        grp = getgrent();
        if (grp == NULL)
            break;

        // lookup our version of the group
        group = usermanager_get_group_by_id(manager, grp->gr_gid);
        if (group != NULL)
            groups = g_list_append(groups, group);
    }

    return groups;
}

ThunarGroup* usermanager_get_group_by_id(UserManager *manager,
                                          guint32 id)
{
    // g_object_unref when unneeded

    g_return_val_if_fail(IS_USERMANAGER(manager), NULL);

    // lookup/load the group corresponding to id
    ThunarGroup *group = g_hash_table_lookup(manager->groups, GINT_TO_POINTER(id));
    if (group == NULL)
    {
        group = _group_new(id);
        g_hash_table_insert(manager->groups, GINT_TO_POINTER(id), group);
    }

    // take a reference for the caller
    g_object_ref(G_OBJECT(group));

    return group;
}

ThunarUser* usermanager_get_user_by_id(UserManager *manager, guint32 id)
{
    // g_object_unref when unneeded

    g_return_val_if_fail(IS_USERMANAGER(manager), NULL);

    // lookup/load the user corresponding to id
    ThunarUser *user = g_hash_table_lookup(manager->users, GINT_TO_POINTER(id));
    if (user == NULL)
    {
        user = _user_new(id);
        g_hash_table_insert(manager->users, GINT_TO_POINTER(id), user);
    }

    // take a reference for the caller
    g_object_ref(G_OBJECT(user));

    return user;
}

// Group ----------------------------------------------------------------------

struct _ThunarGroupClass
{
    GObjectClass __parent__;
};

struct _ThunarGroup
{
    GObject __parent__;

    guint32 id;
    gchar  *name;
};

G_DEFINE_TYPE(ThunarGroup, group, G_TYPE_OBJECT)

static ThunarGroup* _group_new(guint32 id)
{
    ThunarGroup *group = g_object_new(THUNAR_TYPE_GROUP, NULL);
    group->id = id;

    return group;
}

static void group_class_init(ThunarGroupClass *klass)
{
    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = _group_finalize;
}

static void group_init(ThunarGroup *group)
{
    (void) group;
}

static void _group_finalize(GObject *object)
{
    ThunarGroup *group = THUNAR_GROUP(object);

    // release the group's name
    g_free(group->name);

    G_OBJECT_CLASS(group_parent_class)->finalize(object);
}

guint32 group_get_id(ThunarGroup *group)
{
    g_return_val_if_fail(THUNAR_IS_GROUP(group), 0);

    return group->id;
}

const gchar* group_get_name(ThunarGroup *group)
{
    struct group *grp;

    g_return_val_if_fail(THUNAR_IS_GROUP(group), NULL);

    // determine the name on-demand
    if (group->name == NULL)
    {
        grp = getgrgid(group->id);

        if (grp != NULL)
            group->name = g_strdup(grp->gr_name);
        else
            group->name = g_strdup_printf("%u",(guint) group->id);
    }

    return group->name;
}

// User -----------------------------------------------------------------------

static guint32 _user_effective_uid;

struct _ThunarUserClass
{
    GObjectClass __parent__;
};

struct _ThunarUser
{
    GObject __parent__;

    GList   *groups;
    ThunarGroup *primary_group;
    guint32 id;
    gchar   *name;
    gchar   *real_name;
};

G_DEFINE_TYPE(ThunarUser, user, G_TYPE_OBJECT)

static ThunarUser* _user_new(guint32 id)
{
    ThunarUser *user;

    user = g_object_new(THUNAR_TYPE_USER, NULL);
    user->id = id;

    return user;
}

static void user_class_init(ThunarUserClass *klass)
{
    /* determine the current process' effective user id, we do
     * this only once to avoid the syscall overhead on every
     * is_me() invokation.
     */
    _user_effective_uid = geteuid();

    GObjectClass *gobject_class = G_OBJECT_CLASS(klass);
    gobject_class->finalize = _user_finalize;
}

static void user_init(ThunarUser *user)
{
    (void) user;
}

static void _user_finalize(GObject *object)
{
    ThunarUser *user = THUNAR_USER(object);

    // unref the associated groups
    g_list_free_full(user->groups, g_object_unref);

    // drop the reference on the primary group
    if (user->primary_group != NULL)
        g_object_unref(G_OBJECT(user->primary_group));

    // release the names
    g_free(user->real_name);
    g_free(user->name);

    G_OBJECT_CLASS(user_parent_class)->finalize(object);
}

static void _user_load(ThunarUser *user)
{
    g_return_if_fail(user->name == NULL);

    UserManager *manager;
    const gchar       *s;
    gchar             *name;
    gchar             *t;

    struct passwd     *pw;
    pw = getpwuid(user->id);
    if (pw != NULL)
    {
        manager = usermanager_get_default();

        // query name and primary group
        user->name = g_strdup(pw->pw_name);
        user->primary_group = usermanager_get_group_by_id(manager, pw->pw_gid);

        // try to figure out the real name
        s = strchr(pw->pw_gecos, ',');
        if (s != NULL)
            user->real_name = g_strndup(pw->pw_gecos, s - pw->pw_gecos);
        else if (pw->pw_gecos[0] != '\0')
            user->real_name = g_strdup(pw->pw_gecos);

        // substitute '&' in the real_name with the account name
        if (user->real_name != NULL && strchr(user->real_name, '&') != NULL)
        {
            // generate a version of the username with the first char upper'd
            name = g_strdup(user->name);
            name[0] = g_ascii_toupper(name[0]);

            // replace all occurances of '&'
            t = util_str_replace(user->real_name, "&", name);
            g_free(user->real_name);
            user->real_name = t;

            // clean up
            g_free(name);
        }

        g_object_unref(G_OBJECT(manager));
    }
    else
    {
        user->name = g_strdup_printf("%u",(guint) user->id);
    }
}

static ThunarGroup* _user_get_primary_group(ThunarUser *user)
{
    g_return_val_if_fail(THUNAR_IS_USER(user), NULL);

    // load the user data on-demand
    if (user->name == NULL)
        _user_load(user);

    return user->primary_group;
}

// The return list must not be altered or freed
GList* user_get_groups(ThunarUser *user)
{
    UserManager *manager;
    ThunarGroup       *primary_group;
    ThunarGroup       *group;
    gid_t              gidset[NGROUPS_MAX];
    gint               gidsetlen;
    gint               n;

    g_return_val_if_fail(THUNAR_IS_USER(user), NULL);

    // load the groups on-demand
    if (user->groups == NULL)
    {
        primary_group = _user_get_primary_group(user);

        /* we can only determine the groups list for the current
         * process owner in a portable fashion, and in fact, we
         * only need the list for the current user.
         */
        if (user_is_me(user))
        {
            manager = usermanager_get_default();

            // add all supplementary groups
            gidsetlen = getgroups(G_N_ELEMENTS(gidset), gidset);
            for(n = 0; n < gidsetlen; ++n)
                if (primary_group == NULL || group_get_id(primary_group) != gidset[n])
                {
                    group = usermanager_get_group_by_id(manager, gidset[n]);
                    if (group != NULL)
                        user->groups = g_list_append(user->groups, group);
                }

            g_object_unref(G_OBJECT(manager));
        }

        // prepend the primary group(if any)
        if (primary_group != NULL)
        {
            user->groups = g_list_prepend(user->groups, primary_group);
            g_object_ref(G_OBJECT(primary_group));
        }
    }

    return user->groups;
}

const gchar* user_get_name(ThunarUser *user)
{
    g_return_val_if_fail(THUNAR_IS_USER(user), 0);

    // load the user's data on-demand
    if (user->name == NULL)
        _user_load(user);

    return user->name;
}

const gchar* user_get_real_name(ThunarUser *user)
{
    g_return_val_if_fail(THUNAR_IS_USER(user), 0);

    // load the user's data on-demand
    if (user->name == NULL)
        _user_load(user);

    return user->real_name;
}

gboolean user_is_me(ThunarUser *user)
{
    g_return_val_if_fail(THUNAR_IS_USER(user), FALSE);

    return (user->id == _user_effective_uid);
}

