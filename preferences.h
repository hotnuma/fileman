#ifndef PREFERENCES_H
#define PREFERENCES_H

#include <cstring.h>

typedef struct
{
    CString *filepath;
    int window_width;
    int window_height;
    int window_maximized;
    int separator_position;
    CString *column_widths;

} Preferences;

Preferences* get_preferences();
void prefs_cleanup();

void prefs_file_read();
void prefs_write();

#endif // PREFERENCES_H


