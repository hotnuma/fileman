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
#include <appnotify.h>

int main(int argc, char **argv)
{
    //xfce_textdomain(GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

    g_set_application_name(_("Fileman"));

    app_notify_init();

    #ifdef G_ENABLE_DEBUG
    g_log_set_always_fatal(G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
    #endif

    Application *application = application_get();
    gtk_window_set_default_icon_name("Thunar");

    g_application_run(G_APPLICATION(application), argc, argv);

    g_object_unref(application);

    app_notify_uninit();

    return EXIT_SUCCESS;
}


