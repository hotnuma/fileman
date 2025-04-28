/*-
 * Copyright(c) 2006 Benedikt Meurer <benny@xfce.org>
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
#include "filemonitor.h"

static FileMonitor* _filemon_default;

// FileMonitor ----------------------------------------------------------------

enum
{
    FILE_CHANGED,
    FILE_DESTROYED,
    LAST_SIGNAL,
};
static guint _filemon_signals[LAST_SIGNAL];

struct _FileMonitor
{
    GObject __parent__;
};

struct _FileMonitorClass
{
    GObjectClass __parent__;
};

G_DEFINE_TYPE(FileMonitor, filemon, G_TYPE_OBJECT)

/*
 * The FileMonitor default instance can
 * be used to monitor the lifecycle of all currently existing
 * #ThunarFile instances. The ::file-changed and ::file-destroyed
 * signals will be emitted whenever any of the currently
 * existing #ThunarFile<!---->s is changed or destroyed.
 */
FileMonitor* filemon_get_default()
{
    if (_filemon_default == NULL)
    {
        // allocate the default monitor
        _filemon_default = g_object_new(TYPE_FILEMONITOR, NULL);

        g_object_add_weak_pointer(G_OBJECT(_filemon_default),
                                  (gpointer) &_filemon_default);

        return _filemon_default;
    }

    g_object_ref(G_OBJECT(_filemon_default));

    return _filemon_default;
}

static void filemon_class_init(FileMonitorClass *klass)
{
    /*
     * This signal is emitted on file_monitor whenever any of the currently
     * existing ThunarFile instances changes. file identifies the instance
     * that changed.
     */
    _filemon_signals[FILE_CHANGED] =
        g_signal_new(I_("file-changed"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_NO_HOOKS,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1, TYPE_THUNARFILE);

    /*
     * This signal is emitted on file_monitor whenever any of the currently
     * existing ThunarFile instances is about to be destroyed. file identifies
     * the instance that is about to be destroyed.
     *
     * Note that this signal is only emitted if file is explicitly destroyed,
     * i.e. because Thunar noticed that it was removed from disk, it is not
     * emitted when the last reference on @file is released.
     */
    _filemon_signals[FILE_DESTROYED] =
        g_signal_new(I_("file-destroyed"),
                     G_TYPE_FROM_CLASS(klass),
                     G_SIGNAL_NO_HOOKS,
                     0, NULL, NULL,
                     g_cclosure_marshal_VOID__OBJECT,
                     G_TYPE_NONE, 1, TYPE_THUNARFILE);
}

static void filemon_init(FileMonitor *monitor)
{
    (void) monitor;
}


// public ---------------------------------------------------------------------

// this method should only be used by ThunarFile.
void filemon_file_changed(ThunarFile *file)
{
    e_return_if_fail(IS_THUNARFILE(file));

    if (_filemon_default == NULL)
        return;

    g_signal_emit(G_OBJECT(_filemon_default),
                  _filemon_signals[FILE_CHANGED], 0, file);
}

// this method should only be used by ThunarFile
void filemon_file_destroyed(ThunarFile *file)
{
    e_return_if_fail(IS_THUNARFILE(file));

    if (_filemon_default == NULL)
        return;

    g_signal_emit(G_OBJECT(_filemon_default),
                  _filemon_signals[FILE_DESTROYED], 0, file);
}


