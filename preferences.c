#include "preferences.h"
#include "config.h"
#include "cinifile.h"

#include <libpath.h>
#include <cfile.h>
#include <glib.h>

static Preferences _prefs;

Preferences* get_preferences()
{
    return &_prefs;
}

void prefs_cleanup()
{
    cstr_free(_prefs.filepath);
}

void prefs_file_read()
{
    if (!_prefs.filepath)
    {
        _prefs.filepath = cstr_new_size(32);
        cstr_fmt(_prefs.filepath, "%s/%s/", g_get_user_config_dir(), APP_NAME);
        g_mkdir_with_parents(c_str(_prefs.filepath), 0700);

        cstr_append(_prefs.filepath, APP_NAME);
        cstr_append(_prefs.filepath, ".conf");
    }

    CIniFileAuto *file = cinifile_new();
    cinifile_read(file, c_str(_prefs.filepath));

    CIniSection *section = cinifile_section(file, "General");

    cinisection_int(section, &_prefs.window_width, "WindowWidth", 640);
    cinisection_int(section, &_prefs.window_height, "WindowHeight", 480);
    cinisection_int(section, &_prefs.window_maximized, "WindowMaximized", 0);
    cinisection_int(section, &_prefs.separator_position, "SeparatorPosition", 200);
}

void prefs_write()
{
    CFileAuto *outfile = cfile_new();
    if (!cfile_open(outfile, c_str(_prefs.filepath), "wb"))
        return;

    CStringAuto *line = cstr_new_size(64);

    cstr_fmt(line, "[General]\n");
    cfile_write(outfile, c_str(line));

    cstr_fmt(line, "WindowWidth=%d\n", _prefs.window_width);
    cfile_write(outfile, c_str(line));

    cstr_fmt(line, "WindowHeight=%d\n", _prefs.window_height);
    cfile_write(outfile, c_str(line));

    cstr_fmt(line, "WindowMaximized=%d\n", _prefs.window_maximized);
    cfile_write(outfile, c_str(line));

    cstr_fmt(line, "SeparatorPosition=%d\n", _prefs.separator_position);
    cfile_write(outfile, c_str(line));
}


