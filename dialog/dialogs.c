/*-
 * Copyright(c) 2005-2011 Benedikt Meurer <benny@xfce.org>
 * Copyright(c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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
#include <memory.h>
#include <stdarg.h>
#include <string.h>

#include <libxfce4ui/libxfce4ui.h>
#include <libext.h>

#include <dialogs.h>
#include <gtk-ext.h>
#include <icon-factory.h>
#include <io-jobs.h>
#include <job.h>
#include <pango-ext.h>
#include <utils.h>

static void _dialog_select_filename(GtkWidget *entry, ThunarFile *file);
static void _dialog_job_ask_replace_callback(GtkWidget *button, gpointer user_data);


gboolean dialog_insecure_program(gpointer parent, const gchar *primary, ThunarFile *file,
                                 const gchar *command)
{
    GdkScreen      *screen;
    GtkWindow      *window;
    gint            response;
    GtkWidget      *dialog;
    GString        *secondary;
    ThunarFileMode  old_mode;
    ThunarFileMode  new_mode;
    GFileInfo      *info;
    GError         *err = NULL;

    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);
    thunar_return_val_if_fail(g_utf8_validate(command, -1, NULL), FALSE);

    /* parse the parent window and screen */
    screen = thunar_util_parse_parent(parent, &window);

    /* secondary text */
    secondary = g_string_new(NULL);
    g_string_append_printf(secondary, _("The desktop file \"%s\" is in an insecure location "
                                         "and not marked as executable. If you do not trust "
                                         "this program, click Cancel."),
                            th_file_get_display_name(file));
    g_string_append(secondary, "\n\n");
    if (e_str_looks_like_an_uri(command))
        g_string_append_printf(secondary, G_KEY_FILE_DESKTOP_KEY_URL"=%s", command);
    else
        g_string_append_printf(secondary, G_KEY_FILE_DESKTOP_KEY_EXEC"=%s", command);

    /* allocate and display the error message dialog */
    dialog = gtk_message_dialog_new(window,
                                     GTK_DIALOG_MODAL |
                                     GTK_DIALOG_DESTROY_WITH_PARENT,
                                     GTK_MESSAGE_WARNING,
                                     GTK_BUTTONS_NONE,
                                     "%s", primary);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Launch Anyway"), GTK_RESPONSE_OK);
    if (th_file_is_chmodable(file))
        gtk_dialog_add_button(GTK_DIALOG(dialog), _("Mark _Executable"), GTK_RESPONSE_APPLY);
    gtk_dialog_add_button(GTK_DIALOG(dialog), _("_Cancel"), GTK_RESPONSE_CANCEL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_CANCEL);
    if (screen != NULL && window == NULL)
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s", secondary->str);
    g_string_free(secondary, TRUE);
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    /* check if we should make the file executable */
    if (response == GTK_RESPONSE_APPLY)
    {
        /* try to query information about the file */
        info = g_file_query_info(th_file_get_file(file),
                                  G_FILE_ATTRIBUTE_UNIX_MODE,
                                  G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                  NULL, &err);

        if (G_LIKELY(info != NULL))
        {
            if (g_file_info_has_attribute(info, G_FILE_ATTRIBUTE_UNIX_MODE))
            {
                /* determine the current mode */
                old_mode = g_file_info_get_attribute_uint32(info, G_FILE_ATTRIBUTE_UNIX_MODE);

                /* generate the new mode */
                new_mode = old_mode | THUNAR_FILE_MODE_USR_EXEC | THUNAR_FILE_MODE_GRP_EXEC | THUNAR_FILE_MODE_OTH_EXEC;

                if (old_mode != new_mode)
                {
                    g_file_set_attribute_uint32(th_file_get_file(file),
                                                 G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                                 G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                                 NULL, &err);
                }
            }
            else
            {
                g_warning("No %s attribute found", G_FILE_ATTRIBUTE_UNIX_MODE);
            }

            g_object_unref(info);
        }

        if (err != NULL)
        {
            dialog_error(parent, err,("Unable to mark launcher executable"));
            g_error_free(err);
        }

        /* just launch */
        response = GTK_RESPONSE_OK;
    }

    return (response == GTK_RESPONSE_OK);
}


gchar* dialog_create(gpointer parent, const gchar *content_type, const gchar *filename,
                     const gchar *title)
{
    // The caller must free the returned string.

    thunar_return_val_if_fail(parent == NULL || GDK_IS_SCREEN(parent)
                              || GTK_IS_WIDGET(parent), NULL);

    /* parse the parent window and screen */
    GtkWindow *window;
    GdkScreen *screen = thunar_util_parse_parent(parent, &window);

    /* create a new dialog window */
    GtkWidget *dialog = gtk_dialog_new_with_buttons(title,
                                                    window,
                                                    GTK_DIALOG_MODAL
                                                    | GTK_DIALOG_DESTROY_WITH_PARENT,
                                                    _("_Cancel"), GTK_RESPONSE_CANCEL,
                                                    _("C_reate"), GTK_RESPONSE_OK,
                                                    NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    gtk_dialog_set_response_sensitive(GTK_DIALOG(dialog), GTK_RESPONSE_OK, FALSE);
    gtk_window_set_default_size(GTK_WINDOW(dialog), 300, -1);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 6);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))),
                       grid, TRUE, TRUE, 0);
    gtk_widget_show(grid);

    GIcon *icon = NULL;
    /* try to load the icon */
    if (G_LIKELY(content_type != NULL))
        icon = g_content_type_get_icon(content_type);

    GtkWidget *image;
    /* setup the image */
    if (G_LIKELY(icon != NULL))
    {
        image = g_object_new(GTK_TYPE_IMAGE, "xpad", 6, "ypad", 6, NULL);
        gtk_image_set_from_gicon(GTK_IMAGE(image), icon, GTK_ICON_SIZE_DIALOG);
        gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 2);
        g_object_unref(icon);
        gtk_widget_show(image);
    }

    GtkWidget *label = g_object_new(GTK_TYPE_LABEL,
                                    "label",
                                    _("Enter the name:"),
                                    "xalign", 0.0f,
                                    "hexpand", TRUE,
                                    NULL);
    gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);
    gtk_widget_show(label);

    /* set up the widget for entering the filename */
    XfceFilenameInput *filename_input = g_object_new(XFCE_TYPE_FILENAME_INPUT,
                                                     "original-filename", filename,
                                                     NULL);
    gtk_widget_set_hexpand(GTK_WIDGET(filename_input), TRUE);
    gtk_widget_set_valign(GTK_WIDGET(filename_input), GTK_ALIGN_CENTER);

    /* connect to signals so that the sensitivity of the Create button is updated according to whether there
     * is a valid file name entered */
    g_signal_connect_swapped(filename_input, "text-invalid", G_CALLBACK(xfce_filename_input_desensitise_widget),
                              gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK));
    g_signal_connect_swapped(filename_input, "text-valid", G_CALLBACK(xfce_filename_input_sensitise_widget),
                              gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK));

    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(filename_input), 1, 1, 1, 1);
    thunar_gtk_label_set_a11y_relation(GTK_LABEL(label),
                                        GTK_WIDGET(xfce_filename_input_get_entry(filename_input)));
    gtk_widget_show_all( GTK_WIDGET(filename_input));

    /* ensure that the sensitivity of the Create button is set correctly */
    xfce_filename_input_check(filename_input);

    if (screen != NULL)
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    if (window != NULL)
        gtk_window_set_transient_for(GTK_WINDOW(dialog), window);

    gchar *name = NULL;

    if (gtk_dialog_run(GTK_DIALOG(dialog)) == GTK_RESPONSE_OK)
    {
        GError *error = NULL;
        /* determine the chosen filename */
        filename = xfce_filename_input_get_text(filename_input);

        /* convert the UTF-8 filename to the local file system encoding */
        name = g_filename_from_utf8(filename, -1, NULL, NULL, &error);
        if (G_UNLIKELY(name == NULL))
        {
            /* display an error message */
            dialog_error(dialog, error, _("Cannot convert filename \"%s\" to the local encoding"), filename);

            /* release the error */
            g_error_free(error);
        }
    }

    /* destroy the dialog */
    gtk_widget_destroy(dialog);

    return name;
}

gboolean dialog_folder_trash(GtkWindow *window)
{
    //GdkScreen *screen = thunar_util_parse_parent(parent, &window);

    gchar *message = g_strdup_printf(
    _("Are you sure that you want to delete folder?"));

    /* ask the user to confirm the delete operation */
    GtkWidget *dialog = gtk_message_dialog_new(
                                        window,
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_MESSAGE_QUESTION,
                                        GTK_BUTTONS_NONE,
                                        "%s",
                                        message);

    //if (G_UNLIKELY(window == NULL && screen != NULL))
    //    gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    gtk_dialog_add_buttons(GTK_DIALOG(dialog),
                            _("_Cancel"), GTK_RESPONSE_CANCEL,
                            _("_Delete"), GTK_RESPONSE_YES,
                            NULL);

    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_YES);
    gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog),
                                             _("Secondary."));

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);
    g_free(message);

    /* perform the delete operation */
    return (response == GTK_RESPONSE_YES);
}

ThunarJob* dialog_rename_file(gpointer parent, ThunarFile *file)
{
    thunar_return_val_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WINDOW(parent), FALSE);
    thunar_return_val_if_fail(THUNAR_IS_FILE(file), FALSE);

    IconFactory *icon_factory;
    GtkIconTheme      *icon_theme;
    const gchar       *filename;
    const gchar       *text;
    ThunarJob         *job = NULL;
    GtkWidget         *dialog;
    GtkWidget         *label;
    GtkWidget         *image;
    GtkWidget         *grid;
    GtkWindow         *window;
    GdkPixbuf         *icon;
    GdkScreen         *screen;
    gchar             *title;
    gint               response;
    PangoLayout       *layout;
    gint               layout_width;
    gint               layout_offset;
    gint               parent_width = 500;
    XfceFilenameInput *filename_input;

    /* parse the parent window and screen */
    screen = thunar_util_parse_parent(parent, &window);

    /* get the filename of the file */
    filename = th_file_get_display_name(file);

    /* create a new dialog window */
    title = g_strdup_printf(_("Rename \"%s\""), filename);
    dialog = gtk_dialog_new_with_buttons(title,
                                          window,
                                          GTK_DIALOG_MODAL
                                          | GTK_DIALOG_DESTROY_WITH_PARENT,
                                          _("_Cancel"), GTK_RESPONSE_CANCEL,
                                          _("_Rename"), GTK_RESPONSE_OK,
                                          NULL);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK);
    g_free(title);

    /* move the dialog to the appropriate screen */
    if (G_UNLIKELY(window == NULL && screen != NULL))
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 3);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 6);
    gtk_box_pack_start(GTK_BOX(gtk_dialog_get_content_area(GTK_DIALOG(dialog))), grid, TRUE, TRUE, 0);
    gtk_widget_show(grid);

    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(dialog));
    icon_factory = ifactory_get_for_icon_theme(icon_theme);
    icon = ifactory_load_file_icon(icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
    g_object_unref(G_OBJECT(icon_factory));

    image = gtk_image_new_from_pixbuf(icon);
    gtk_widget_set_margin_start(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(image), 6);
    gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 2);
    g_object_unref(G_OBJECT(icon));
    gtk_widget_show(image);

    label = gtk_label_new(_("Enter the new name:"));
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 1, 1);
    gtk_widget_show(label);

    /* set up the widget for entering the filename */
    filename_input = g_object_new(XFCE_TYPE_FILENAME_INPUT, "original-filename", filename, NULL);
    gtk_widget_set_hexpand(GTK_WIDGET(filename_input), TRUE);
    gtk_widget_set_valign(GTK_WIDGET(filename_input), GTK_ALIGN_CENTER);

    /* connect to signals so that the sensitivity of the Create button is updated according to whether there
     * is a valid file name entered */
    g_signal_connect_swapped(filename_input, "text-invalid", G_CALLBACK(xfce_filename_input_desensitise_widget),
                              gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK));
    g_signal_connect_swapped(filename_input, "text-valid", G_CALLBACK(xfce_filename_input_sensitise_widget),
                              gtk_dialog_get_widget_for_response(GTK_DIALOG(dialog), GTK_RESPONSE_OK));

    gtk_grid_attach(GTK_GRID(grid), GTK_WIDGET(filename_input), 1, 1, 1, 1);
    thunar_gtk_label_set_a11y_relation(GTK_LABEL(label),
                                        GTK_WIDGET(xfce_filename_input_get_entry(filename_input)));
    gtk_widget_show_all( GTK_WIDGET(filename_input));

    /* ensure that the sensitivity of the Create button is set correctly */
    xfce_filename_input_check(filename_input);

    /* select the filename without the extension */
    _dialog_select_filename(GTK_WIDGET(xfce_filename_input_get_entry(filename_input)), file);

    /* get the size the entry requires to render the full text */
    layout = gtk_entry_get_layout(xfce_filename_input_get_entry(filename_input));
    pango_layout_get_pixel_size(layout, &layout_width, NULL);
    gtk_entry_get_layout_offsets(xfce_filename_input_get_entry(filename_input), &layout_offset, NULL);
    layout_width +=(layout_offset * 2) +(12 * 4) + 48; /* 12px free space in entry */

    /* parent window width */
    if (G_LIKELY(window != NULL))
    {
        /* keep below 90% of the parent window width */
        gtk_window_get_size(GTK_WINDOW(window), &parent_width, NULL);
        parent_width *= 0.90f;
    }

    /* resize the dialog to make long names fit as much as possible */
    gtk_window_set_default_size(GTK_WINDOW(dialog), CLAMP(layout_width, 300, parent_width), -1);

    /* automatically close the dialog when the file is destroyed */
    g_signal_connect_swapped(G_OBJECT(file), "destroy",
                              G_CALLBACK(gtk_widget_destroy), dialog);

    /* run the dialog */
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    if (G_LIKELY(response == GTK_RESPONSE_OK))
    {
        /* hide the dialog */
        gtk_widget_hide(dialog);

        /* determine the new filename */
        text = xfce_filename_input_get_text(filename_input);

        /* check if we have a new name here */
        if (G_LIKELY(g_strcmp0(filename, text) != 0))
        {
            /* try to rename the file */
            job = io_rename_file(file, text);
        }
    }

    /* cleanup */
    if (G_LIKELY(response != GTK_RESPONSE_NONE))
    {
        /* unregister handler */
        g_signal_handlers_disconnect_by_func(G_OBJECT(file),
                                             gtk_widget_destroy,
                                             dialog);

        gtk_widget_destroy(dialog);
    }

    return job;
}

static void _dialog_select_filename(GtkWidget  *entry,
                                           ThunarFile *file)
{
    const gchar *filename;
    const gchar *ext;
    glong        offset;

    /* check if we have a directory here */
    if (th_file_is_directory(file))
    {
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, -1);
        return;
    }

    filename = th_file_get_display_name(file);

    /* check if the filename contains an extension */
    ext = thunar_util_str_get_extension(filename);
    if (G_UNLIKELY(ext == NULL))
        return;

    /* grab focus to the entry first, else the selection will be altered later */
    gtk_widget_grab_focus(entry);

    /* determine the UTF-8 char offset */
    offset = g_utf8_pointer_to_offset(filename, ext);

    /* select the text prior to the dot */
    if (G_LIKELY(offset > 0))
        gtk_editable_select_region(GTK_EDITABLE(entry), 0, offset);
}

void dialog_error(gpointer parent, const GError *error, const gchar  *format, ...)
{
    GtkWidget *dialog;
    GtkWindow *window;
    GdkScreen *screen;
    va_list    args;
    gchar     *primary_text;
    GList     *children;
    GList     *lp;

    thunar_return_if_fail(parent == NULL || GDK_IS_SCREEN(parent) || GTK_IS_WIDGET(parent));

    /* do not display error dialog for already handled errors */
    if (error && error->code == G_IO_ERROR_FAILED_HANDLED)
        return;

    /* parse the parent pointer */
    screen = thunar_util_parse_parent(parent, &window);

    /* determine the primary error text */
    va_start(args, format);
    primary_text = g_strdup_vprintf(format, args);
    va_end(args);

    /* allocate the error dialog */
    dialog = gtk_message_dialog_new(window,
                                     GTK_DIALOG_DESTROY_WITH_PARENT
                                     | GTK_DIALOG_MODAL,
                                     GTK_MESSAGE_ERROR,
                                     GTK_BUTTONS_CLOSE,
                                     "%s.", primary_text);

    /* move the dialog to the appropriate screen */
    if (G_UNLIKELY(window == NULL && screen != NULL))
        gtk_window_set_screen(GTK_WINDOW(dialog), screen);

    /* set secondary text if an error is provided */
    if (G_LIKELY(error != NULL))
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(dialog), "%s.", error->message);

    children = gtk_container_get_children(
                   GTK_CONTAINER(gtk_message_dialog_get_message_area(GTK_MESSAGE_DIALOG(dialog))));

    /* enable wrap for labels */
    for(lp = children; lp != NULL; lp = lp->next)
        if (GTK_IS_LABEL(lp->data))
            gtk_label_set_line_wrap_mode(GTK_LABEL(lp->data), PANGO_WRAP_WORD_CHAR);

    /* display the dialog */
    gtk_dialog_run(GTK_DIALOG(dialog));

    /* cleanup */
    gtk_widget_destroy(dialog);

    g_free(primary_text);
    g_list_free(children);
}


ThunarJobResponse dialog_job_ask(GtkWindow *parent, const gchar *question,
                                 ThunarJobResponse choices)
{
    const gchar *separator;
    const gchar *mnemonic;
    GtkWidget   *message;
    GtkWidget   *button;
    GString     *secondary = g_string_sized_new(256);
    GString     *primary = g_string_sized_new(256);
    gint         response;
    gint         n;
    gboolean     has_cancel = FALSE;

    thunar_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), THUNAR_JOB_RESPONSE_CANCEL);
    thunar_return_val_if_fail(g_utf8_validate(question, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);

    /* try to separate the question into primary and secondary parts */
    separator = strstr(question, ": ");
    if (G_LIKELY(separator != NULL))
    {
        /* primary is everything before the colon, plus a dot */
        g_string_append_len(primary, question, separator - question);
        g_string_append_c(primary, '.');

        /* secondary is everything after the colon(skipping whitespace) */
        do
            ++separator;
        while(g_ascii_isspace(*separator));
        g_string_append(secondary, separator);
    }
    else
    {
        /* otherwise separate based on the \n\n */
        separator = strstr(question, "\n\n");
        if (G_LIKELY(separator != NULL))
        {
            /* primary is everything before the newlines */
            g_string_append_len(primary, question, separator - question);

            /* secondary is everything after the newlines(skipping whitespace) */
            while(g_ascii_isspace(*separator))
                ++separator;
            g_string_append(secondary, separator);
        }
        else
        {
            /* everything is primary */
            g_string_append(primary, question);
        }
    }

    /* allocate the question message dialog */
    message = gtk_message_dialog_new(parent,
                                      GTK_DIALOG_MODAL |
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_QUESTION,
                                      GTK_BUTTONS_NONE,
                                      "%s", primary->str);
    if (G_LIKELY(*secondary->str != '\0'))
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message), "%s", secondary->str);

    /* add the buttons based on the possible choices */
    for (n = THUNAR_JOB_RESPONSE_MAX_INT; n >= 0; --n)
    {
        /* check if the response is set */
        response = choices &(1 << n);
        if (response == 0)
            continue;

        switch (response)
        {
        case THUNAR_JOB_RESPONSE_YES:
            mnemonic = _("_Yes");
            break;

        case THUNAR_JOB_RESPONSE_YES_ALL:
            mnemonic = _("Yes to _all");
            break;

        case THUNAR_JOB_RESPONSE_REPLACE:
            mnemonic = _("_Replace");
            break;

        case THUNAR_JOB_RESPONSE_REPLACE_ALL:
            mnemonic = _("Replace _All");
            break;

        case THUNAR_JOB_RESPONSE_SKIP:
            mnemonic = _("_Skip");
            break;

        case THUNAR_JOB_RESPONSE_SKIP_ALL:
            mnemonic = _("S_kip All");
            break;

        case THUNAR_JOB_RESPONSE_RENAME:
            mnemonic = _("Re_name");
            break;

        case THUNAR_JOB_RESPONSE_RENAME_ALL:
            mnemonic = _("Rena_me All");
            break;

        case THUNAR_JOB_RESPONSE_NO:
            mnemonic = _("_No");
            break;

        case THUNAR_JOB_RESPONSE_NO_ALL:
            mnemonic = _("N_o to all");
            break;

        case THUNAR_JOB_RESPONSE_RETRY:
            mnemonic = _("_Retry");
            break;

        case THUNAR_JOB_RESPONSE_FORCE:
            mnemonic = _("Copy _Anyway");
            break;

        case THUNAR_JOB_RESPONSE_CANCEL:
            /* cancel is always the last option */
            has_cancel = TRUE;
            continue;

        default:
            g_assert_not_reached();
            break;
        }

        button = gtk_button_new_with_mnemonic(mnemonic);
        gtk_widget_set_can_default(button, TRUE);
        gtk_dialog_add_action_widget(GTK_DIALOG(message), button, response);
        gtk_widget_show(button);

        gtk_dialog_set_default_response(GTK_DIALOG(message), response);
    }

    if (has_cancel)
    {
        button = gtk_button_new_with_mnemonic(_("_Cancel"));
        gtk_widget_set_can_default(button, TRUE);
        gtk_dialog_add_action_widget(GTK_DIALOG(message), button, GTK_RESPONSE_CANCEL);
        gtk_widget_show(button);
        gtk_dialog_set_default_response(GTK_DIALOG(message), GTK_RESPONSE_CANCEL);
    }

    /* run the question dialog */
    response = gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);

    /* transform the result as required */
    if (G_UNLIKELY(response <= 0))
        response = THUNAR_JOB_RESPONSE_CANCEL;

    /* cleanup */
    g_string_free(secondary, TRUE);
    g_string_free(primary, TRUE);

    return response;
}

ThunarJobResponse dialog_job_ask_replace(GtkWindow *parent, ThunarFile *src_file,
                                         ThunarFile *dst_file)
{
    IconFactory *icon_factory;
    GtkIconTheme      *icon_theme;
    GtkWidget         *dialog;
    GtkWidget         *grid;
    GtkWidget         *image;
    GtkWidget         *label;
    GtkWidget         *content_area;
    GtkWidget         *cancel_button;
    GtkWidget         *button_box;
    GtkWidget         *skipall_button;
    GtkWidget         *skip_button;
    GtkWidget         *replaceall_button;
    GtkWidget         *replace_button;
    GtkWidget         *renameall_button;
    GtkWidget         *rename_button;
    GdkPixbuf         *icon;
    gchar             *date_string;
    gchar             *size_string;
    gchar             *text;
    gint               response;

    thunar_return_val_if_fail(parent == NULL || GTK_IS_WINDOW(parent), THUNAR_JOB_RESPONSE_CANCEL);
    thunar_return_val_if_fail(THUNAR_IS_FILE(src_file), THUNAR_JOB_RESPONSE_CANCEL);
    thunar_return_val_if_fail(THUNAR_IS_FILE(dst_file), THUNAR_JOB_RESPONSE_CANCEL);

    /* setup the confirmation dialog */
    dialog = gtk_dialog_new();
    gtk_window_set_title(GTK_WINDOW(dialog), _("Confirm to replace files"));
    gtk_window_set_transient_for(GTK_WINDOW(dialog), parent);
    gtk_window_set_destroy_with_parent(GTK_WINDOW(dialog), TRUE);
    gtk_window_set_modal(GTK_WINDOW(dialog), TRUE);
    gtk_dialog_set_default_response(GTK_DIALOG(dialog), THUNAR_JOB_RESPONSE_REPLACE);
    content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    /* determine the icon factory to use */
    icon_theme = gtk_icon_theme_get_for_screen(gtk_widget_get_screen(dialog));
    icon_factory = ifactory_get_for_icon_theme(icon_theme);

    grid = gtk_grid_new();
    gtk_grid_set_column_spacing(GTK_GRID(grid), 5);
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_container_set_border_width(GTK_CONTAINER(grid), 10);
    gtk_box_pack_start(GTK_BOX(content_area), grid, TRUE, FALSE, 0);
    gtk_widget_show(grid);

    /* set up the action area buttons ourself */
    button_box = gtk_button_box_new(GTK_ORIENTATION_HORIZONTAL);

    cancel_button     = gtk_button_new_with_mnemonic(_("_Cancel"));
    skipall_button    = gtk_button_new_with_mnemonic(_("S_kip All"));
    skip_button       = gtk_button_new_with_mnemonic(_("_Skip"));
    replaceall_button = gtk_button_new_with_mnemonic(_("Replace _All"));
    replace_button    = gtk_button_new_with_mnemonic(_("_Replace"));
    renameall_button  = gtk_button_new_with_mnemonic(_("Rena_me All"));
    rename_button     = gtk_button_new_with_mnemonic(_("Re_name"));

    g_signal_connect(cancel_button,      "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(skipall_button,     "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(skip_button,        "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(replaceall_button,  "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(replace_button,     "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(renameall_button,   "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);
    g_signal_connect(rename_button,      "clicked", G_CALLBACK(_dialog_job_ask_replace_callback), dialog);

    g_object_set_data(G_OBJECT(cancel_button),     "response-id", GINT_TO_POINTER(GTK_RESPONSE_CANCEL));
    g_object_set_data(G_OBJECT(skipall_button),    "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_SKIP_ALL));
    g_object_set_data(G_OBJECT(skip_button),       "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_SKIP));
    g_object_set_data(G_OBJECT(replaceall_button), "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_REPLACE_ALL));
    g_object_set_data(G_OBJECT(replace_button),    "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_REPLACE));
    g_object_set_data(G_OBJECT(renameall_button),  "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_RENAME_ALL));
    g_object_set_data(G_OBJECT(rename_button),     "response-id", GINT_TO_POINTER(THUNAR_JOB_RESPONSE_RENAME));

    gtk_container_add(GTK_CONTAINER(button_box), cancel_button);
    gtk_container_add(GTK_CONTAINER(button_box), skipall_button);
    gtk_container_add(GTK_CONTAINER(button_box), skip_button);
    gtk_container_add(GTK_CONTAINER(button_box), replaceall_button);
    gtk_container_add(GTK_CONTAINER(button_box), replace_button);
    gtk_container_add(GTK_CONTAINER(button_box), renameall_button);
    gtk_container_add(GTK_CONTAINER(button_box), rename_button);
    gtk_container_add(GTK_CONTAINER(content_area), button_box);
    gtk_widget_set_halign(button_box, GTK_ALIGN_CENTER);
    gtk_box_set_spacing(GTK_BOX(button_box), 5);
    gtk_widget_show_all(button_box);

    image = gtk_image_new_from_icon_name("stock_folder-copy", GTK_ICON_SIZE_BUTTON);
    gtk_widget_set_halign(image, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(image, GTK_ALIGN_START);
    gtk_widget_set_margin_start(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(image), 6);
    gtk_widget_set_vexpand(image, TRUE);
    gtk_grid_attach(GTK_GRID(grid), image, 0, 0, 1, 1);
    gtk_widget_show(image);

    if (th_file_is_symlink(dst_file))
    {
        text = g_strdup_printf(_("This folder already contains a symbolic link \"%s\"."),
                                th_file_get_display_name(dst_file));
    }
    else if (th_file_is_directory(dst_file))
    {
        text = g_strdup_printf(_("This folder already contains a folder \"%s\"."),
                                th_file_get_display_name(dst_file));
    }
    else
    {
        text = g_strdup_printf(_("This folder already contains a file \"%s\"."),
                                th_file_get_display_name(dst_file));
    }

    label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_label_set_attributes(GTK_LABEL(label), thunar_pango_attr_list_big());
    gtk_widget_set_hexpand(label, TRUE);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 1, 0, 2, 1);
    gtk_widget_show(label);
    g_free(text);

    if (th_file_is_symlink(dst_file))
        /* TRANSLATORS: First part of replace dialog sentence */
        text = g_strdup_printf(_("Do you want to replace the link"));
    else if (th_file_is_directory(dst_file))
        /* TRANSLATORS: First part of replace dialog sentence */
        text = g_strdup_printf(_("Do you want to replace the existing folder"));
    else
        /* TRANSLATORS: First part of replace dialog sentence */
        text = g_strdup_printf(_("Do you want to replace the existing file"));

    label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 1, 1, 2, 1);
    gtk_widget_show(label);
    g_free(text);

    icon = ifactory_load_file_icon(icon_factory, dst_file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
    image = gtk_image_new_from_pixbuf(icon);
    gtk_widget_set_margin_start(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(image), 6);
    gtk_grid_attach(GTK_GRID(grid), image, 1, 2, 1, 1);
    g_object_unref(G_OBJECT(icon));
    gtk_widget_show(image);


    /* determine the style used to format dates */
    ThunarDateStyle date_style = THUNAR_DATE_STYLE_YYYYMMDD;
    gchar *date_custom_style = NULL;
    gboolean file_size_binary = TRUE;

    size_string = th_file_get_size_string_long(dst_file, file_size_binary);
    date_string = th_file_get_date_string(dst_file, THUNAR_FILE_DATE_MODIFIED, date_style, date_custom_style);
    text = g_strdup_printf("%s %s\n%s %s", _("Size:"), size_string, _("Modified:"), date_string);
    label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 2, 2, 1, 1);
    gtk_widget_show(label);
    g_free(size_string);
    g_free(date_string);
    g_free(text);

    if (th_file_is_symlink(src_file))
        /* TRANSLATORS: Second part of replace dialog sentence */
        text = g_strdup_printf(_("with the following link?"));
    else if (th_file_is_directory(src_file))
        /* TRANSLATORS: Second part of replace dialog sentence */
        text = g_strdup_printf(_("with the following folder?"));
    else
        /* TRANSLATORS: Second part of replace dialog sentence */
        text = g_strdup_printf(_("with the following file?"));

    label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 1, 3, 2, 1);
    gtk_widget_show(label);
    g_free(text);

    icon = ifactory_load_file_icon(icon_factory, src_file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
    image = gtk_image_new_from_pixbuf(icon);
    gtk_widget_set_margin_start(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_end(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_top(GTK_WIDGET(image), 6);
    gtk_widget_set_margin_bottom(GTK_WIDGET(image), 6);
    gtk_grid_attach(GTK_GRID(grid), image, 1, 4, 1, 1);
    g_object_unref(G_OBJECT(icon));
    gtk_widget_show(image);

    size_string = th_file_get_size_string_long(src_file, file_size_binary);
    date_string = th_file_get_date_string(src_file, THUNAR_FILE_DATE_MODIFIED, date_style, date_custom_style);
    text = g_strdup_printf("%s %s\n%s %s", _("Size:"), size_string, _("Modified:"), date_string);
    label = gtk_label_new(text);
    gtk_label_set_xalign(GTK_LABEL(label), 0.0f);
    gtk_widget_set_hexpand(label, TRUE);
    gtk_grid_attach(GTK_GRID(grid), label, 2, 4, 1, 1);
    gtk_widget_show(label);
    g_free(size_string);
    g_free(date_string);
    g_free(text);

    /* run the dialog */
    response = gtk_dialog_run(GTK_DIALOG(dialog));
    gtk_widget_destroy(dialog);

    /* cleanup */
    g_object_unref(G_OBJECT(icon_factory));

    /* translate GTK responses */
    if (G_UNLIKELY(response < 0))
        response = THUNAR_JOB_RESPONSE_CANCEL;

    return response;
}

static void _dialog_job_ask_replace_callback(GtkWidget *button,
                                                         gpointer user_data)
{
    gint response;

    thunar_return_if_fail(GTK_IS_DIALOG(user_data));

    response = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(button), "response-id"));
    gtk_dialog_response(GTK_DIALOG(user_data), response);
}

void dialog_job_error(GtkWindow *parent, GError *error)
{
    const gchar *separator;
    GtkWidget   *message;
    GString     *secondary = g_string_sized_new(256);
    GString     *primary = g_string_sized_new(256);

    thunar_return_if_fail(parent == NULL || GTK_IS_WINDOW(parent));
    thunar_return_if_fail(error != NULL && error->message != NULL);

    /* try to separate the message into primary and secondary parts */
    separator = strstr(error->message, ": ");
    if (G_LIKELY(separator > error->message))
    {
        /* primary is everything before the colon, plus a dot */
        g_string_append_len(primary, error->message, separator - error->message);
        g_string_append_c(primary, '.');

        /* secondary is everything after the colon(plus a dot) */
        do
            ++separator;
        while(g_ascii_isspace(*separator));
        g_string_append(secondary, separator);
        if (separator[strlen(separator - 1)] != '.')
            g_string_append_c(secondary, '.');
    }
    else
    {
        /* primary is everything, secondary is empty */
        g_string_append(primary, error->message);
    }

    /* allocate and display the error message dialog */
    message = gtk_message_dialog_new(parent,
                                      GTK_DIALOG_MODAL |
                                      GTK_DIALOG_DESTROY_WITH_PARENT,
                                      GTK_MESSAGE_ERROR,
                                      GTK_BUTTONS_NONE,
                                      "%s", primary->str);

    if (G_LIKELY(*secondary->str != '\0'))
        gtk_message_dialog_format_secondary_text(GTK_MESSAGE_DIALOG(message), "%s", secondary->str);

    gtk_dialog_add_button(GTK_DIALOG(message), _("_Close"), GTK_RESPONSE_CANCEL);
    gtk_dialog_run(GTK_DIALOG(message));
    gtk_widget_destroy(message);

    /* cleanup */
    g_string_free(secondary, TRUE);
    g_string_free(primary, TRUE);
}


