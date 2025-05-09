/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
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

#ifndef __ENUMTYPES_H__
#define __ENUMTYPES_H__

#include <glib-object.h>

G_BEGIN_DECLS

// ThunarColumn ---------------------------------------------------------------

#define THUNAR_TYPE_COLUMN (thunar_column_get_type())

typedef enum
{
    // visible columns
    THUNAR_COLUMN_DATE_ACCESSED,
    THUNAR_COLUMN_DATE_MODIFIED,
    THUNAR_COLUMN_GROUP,
    THUNAR_COLUMN_MIME_TYPE,
    THUNAR_COLUMN_NAME,
    THUNAR_COLUMN_OWNER,
    THUNAR_COLUMN_PERMISSIONS,
    THUNAR_COLUMN_SIZE,
    THUNAR_COLUMN_SIZE_IN_BYTES,
    THUNAR_COLUMN_TYPE,

    // special internal columns
    THUNAR_COLUMN_FILE,
    THUNAR_COLUMN_FILE_NAME,

    // number of columns
    THUNAR_N_COLUMNS,

    // number of visible columns
    THUNAR_N_VISIBLE_COLUMNS = THUNAR_COLUMN_FILE,

} ThunarColumn;

GType thunar_column_get_type() G_GNUC_CONST;

const gchar* thunar_column_string_from_value(ThunarColumn value);
gboolean thunar_column_value_from_string(const gchar *value_string,
                                         gint *value);

// ThunarDateStyle ------------------------------------------------------------

#define THUNAR_TYPE_DATE_STYLE (thunar_date_style_get_type())

typedef enum
{
    THUNAR_DATE_STYLE_YYYYMMDD,
    THUNAR_DATE_STYLE_SIMPLE,
    THUNAR_DATE_STYLE_SHORT,
    THUNAR_DATE_STYLE_LONG,
    THUNAR_DATE_STYLE_MMDDYYYY,
    THUNAR_DATE_STYLE_DDMMYYYY,
    THUNAR_DATE_STYLE_CUSTOM,

} ThunarDateStyle;

GType thunar_date_style_get_type() G_GNUC_CONST;

// ThunarFileMode -------------------------------------------------------------

#define THUNAR_TYPE_FILE_MODE (thunar_file_mode_get_type())

typedef enum
{
    THUNAR_FILE_MODE_SUID       = 04000,
    THUNAR_FILE_MODE_SGID       = 02000,
    THUNAR_FILE_MODE_STICKY     = 01000,
    THUNAR_FILE_MODE_USR_ALL    = 00700,
    THUNAR_FILE_MODE_USR_READ   = 00400,
    THUNAR_FILE_MODE_USR_WRITE  = 00200,
    THUNAR_FILE_MODE_USR_EXEC   = 00100,
    THUNAR_FILE_MODE_GRP_ALL    = 00070,
    THUNAR_FILE_MODE_GRP_READ   = 00040,
    THUNAR_FILE_MODE_GRP_WRITE  = 00020,
    THUNAR_FILE_MODE_GRP_EXEC   = 00010,
    THUNAR_FILE_MODE_OTH_ALL    = 00007,
    THUNAR_FILE_MODE_OTH_READ   = 00004,
    THUNAR_FILE_MODE_OTH_WRITE  = 00002,
    THUNAR_FILE_MODE_OTH_EXEC   = 00001,

} ThunarFileMode;

GType thunar_file_mode_get_type() G_GNUC_CONST;

// ThunarJobResponse ----------------------------------------------------------

#define THUNAR_TYPE_JOB_RESPONSE (thunar_job_response_get_type())

typedef enum
{
    THUNAR_JOB_RESPONSE_YES         = 1 << 0,
    THUNAR_JOB_RESPONSE_YES_ALL     = 1 << 1,
    THUNAR_JOB_RESPONSE_NO          = 1 << 2,
    THUNAR_JOB_RESPONSE_CANCEL      = 1 << 3,
    THUNAR_JOB_RESPONSE_NO_ALL      = 1 << 4,
    THUNAR_JOB_RESPONSE_RETRY       = 1 << 5,
    THUNAR_JOB_RESPONSE_FORCE       = 1 << 6,
    THUNAR_JOB_RESPONSE_REPLACE     = 1 << 7,
    THUNAR_JOB_RESPONSE_REPLACE_ALL = 1 << 8,
    THUNAR_JOB_RESPONSE_SKIP        = 1 << 9,
    THUNAR_JOB_RESPONSE_SKIP_ALL    = 1 << 10,
    THUNAR_JOB_RESPONSE_RENAME      = 1 << 11,
    THUNAR_JOB_RESPONSE_RENAME_ALL  = 1 << 12,

} ThunarJobResponse;

#define THUNAR_JOB_RESPONSE_MAX_INT 12

GType thunar_job_response_get_type() G_GNUC_CONST;

// ParallelCopyMode -----------------------------------------------------------

#define TYPE_PARALLEL_COPY_MODE (parallel_copy_mode_get_type())

typedef enum
{
    PARALLEL_COPY_MODE_NEVER,
    PARALLEL_COPY_MODE_ONLY_LOCAL,
    PARALLEL_COPY_MODE_ONLY_LOCAL_SAME_DEVICES,
    PARALLEL_COPY_MODE_ALWAYS,

} ParallelCopyMode;

GType parallel_copy_mode_get_type() G_GNUC_CONST;

// ThunarRecursivePermissionsMode ---------------------------------------------

#define THUNAR_TYPE_RECURSIVE_PERMISSIONS \
    (thunar_recursive_permissions_get_type())

typedef enum
{
    THUNAR_RECURSIVE_PERMISSIONS_ASK,
    THUNAR_RECURSIVE_PERMISSIONS_ALWAYS,
    THUNAR_RECURSIVE_PERMISSIONS_NEVER,

} ThunarRecursivePermissionsMode;

GType thunar_recursive_permissions_get_type() G_GNUC_CONST;

// ThunarIconSize -------------------------------------------------------------

#define THUNAR_TYPE_ICON_SIZE (thunar_icon_size_get_type())

typedef enum
{
    THUNAR_ICON_SIZE_16   =  16,
    THUNAR_ICON_SIZE_24   =  24,
    THUNAR_ICON_SIZE_32   =  32,
    THUNAR_ICON_SIZE_48   =  48,
    THUNAR_ICON_SIZE_64   =  64,
    THUNAR_ICON_SIZE_96   =  96,
    THUNAR_ICON_SIZE_128  = 128,
    THUNAR_ICON_SIZE_160  = 160,
    THUNAR_ICON_SIZE_192  = 192,
    THUNAR_ICON_SIZE_256  = 256,

} ThunarIconSize;

GType thunar_icon_size_get_type() G_GNUC_CONST;

// ThunarZoomLevel ------------------------------------------------------------

#define THUNAR_TYPE_ZOOM_LEVEL (thunar_zoom_level_get_type())

typedef enum
{
    THUNAR_ZOOM_LEVEL_25_PERCENT,
    THUNAR_ZOOM_LEVEL_38_PERCENT,
    THUNAR_ZOOM_LEVEL_50_PERCENT,
    THUNAR_ZOOM_LEVEL_75_PERCENT,
    THUNAR_ZOOM_LEVEL_100_PERCENT,
    THUNAR_ZOOM_LEVEL_150_PERCENT,
    THUNAR_ZOOM_LEVEL_200_PERCENT,
    THUNAR_ZOOM_LEVEL_250_PERCENT,
    THUNAR_ZOOM_LEVEL_300_PERCENT,
    THUNAR_ZOOM_LEVEL_400_PERCENT,

    THUNAR_ZOOM_N_LEVELS,

} ThunarZoomLevel;

GType thunar_zoom_level_get_type() G_GNUC_CONST;

G_END_DECLS

#endif // __ENUMTYPES_H__


