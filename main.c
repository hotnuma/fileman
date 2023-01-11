/*-
 * Copyright(c) 2005-2011 os-cillation e.K.
 *
 * Written by Benedikt Meurer <benny@xfce.org>.
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

#include <config.h>
#include <application.h>

#include <gobject-ext.h>
#include <notify.h>

#include <stdio.h>
#include <stdlib.h>
#include <glib/gstdio.h>
#include <gio/gdesktopappinfo.h>

#ifdef ENABLE_LIBSM
#include <thunar-session-client.h>
#endif

int main(int argc, char **argv)
{
    /* setup translation domain */
#ifdef ENABLE_GETTEXT
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

    /* setup application name */
    g_set_application_name(_("Fileman"));

#ifdef G_ENABLE_DEBUG
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

    /* register additional transformation functions */
    eg_initialize_transformations();

    /* acquire a reference on the global application */
    Application *application = application_get();

    /* use the Thunar icon as default for new windows */
    gtk_window_set_default_icon_name("Thunar");

    /* do further processing inside gapplication */
    g_application_run(G_APPLICATION(application), argc, argv);

    /* release the application reference */
    g_object_unref(G_OBJECT(application));

#ifdef HAVE_LIBNOTIFY
    notify_uninit();
#endif

    return EXIT_SUCCESS;
}


