#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <cstringlist.h>

// Preferences ----------------------------------------------------------------

typedef struct
{
    CString *filepath;

    // window geometry
    int window_width;
    int window_height;
    int window_maximized;
    int separator_position;

    CString *column_widths;

    // extract filter
    CStringList *extractflt;

} Preferences;

Preferences* get_preferences();
void prefs_cleanup();

void prefs_file_read();
void prefs_write();

#endif // PREFERENCES_H


