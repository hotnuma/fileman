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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <glib/gstdio.h>
#ifdef HAVE_GIO_UNIX
#include <gio/gdesktopappinfo.h>
#endif

#ifdef ENABLE_LIBSM
#include <thunar-session-client.h>
#endif

#include <application.h>
#include <thunar-gobject-extensions.h>
#include <thunar-notify.h>

int
main(int argc, char **argv)
{
    ThunarApplication   *application;
    //GError              *error = NULL;

    /* setup translation domain */
#ifdef ENABLE_GETTEXT
    xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");
#endif

    /* setup application name */
    g_set_application_name(_("Fileman"));

#ifdef G_ENABLE_DEBUG
    /* Do NOT remove this line for now, If something doesn't work,
     * fix your code instead!
     */
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif


    /* initialize xfconf */
//    if (!xfconf_init(&error))
//    {
//        g_printerr(PACKAGE_NAME ": Failed to initialize Xfconf: %s\n\n", error->message);
//        g_clear_error(&error);

//        /* disable get/set properties */
//        thunar_preferences_xfconf_init_failed();
//    }


    /* register additional transformation functions */
    thunar_g_initialize_transformations();

    /* acquire a reference on the global application */
    application = thunar_application_get();

    /* use the Thunar icon as default for new windows */
    gtk_window_set_default_icon_name("Thunar");

    /* do further processing inside gapplication */
    g_application_run(G_APPLICATION(application), argc, argv);

    /* release the application reference */
    g_object_unref(G_OBJECT(application));

#ifdef HAVE_LIBNOTIFY
    thunar_notify_uninit();
#endif

    return EXIT_SUCCESS;
}


