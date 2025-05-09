/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __BASEVIEW_H__
#define __BASEVIEW_H__

#include "enumtypes.h"
#include "th-file.h"

G_BEGIN_DECLS

// BaseView -------------------------------------------------------------------

typedef struct _BaseView      BaseView;
typedef struct _BaseViewIface BaseViewIface;

#define TYPE_BASEVIEW (baseview_get_type())

#define BASEVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),      TYPE_BASEVIEW, BaseView))
#define THUNAR_IS_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),      TYPE_BASEVIEW))
#define BASEVIEW_GET_IFACE(obj) \
    (G_TYPE_INSTANCE_GET_INTERFACE((obj),   TYPE_BASEVIEW, BaseViewIface))

struct _BaseViewIface
{
    GTypeInterface  __parent__;

    // virtual methods
    gboolean (*get_loading) (BaseView *view);
    const gchar* (*get_statusbar_text) (BaseView *view);

    gboolean (*get_show_hidden) (BaseView *view);
    void (*set_show_hidden) (BaseView *view, gboolean show_hidden);

    ThunarZoomLevel (*get_zoom_level) (BaseView *view);
    void (*set_zoom_level) (BaseView *view, ThunarZoomLevel zoom_level);

    void (*reload) (BaseView *view, gboolean reload_info);

    gboolean (*get_visible_range) (BaseView *view, ThunarFile **start_file,
                                   ThunarFile **end_file);

    void (*scroll_to_file) (BaseView *view, ThunarFile *file,
                            gboolean select, gboolean use_align,
                            gfloat row_align, gfloat col_align);

    GList* (*get_selected_files) (BaseView *view);
    void (*set_selected_files) (BaseView *view, GList *path_list);
};

GType baseview_get_type() G_GNUC_CONST;

gboolean baseview_get_loading(BaseView *view);
const gchar* baseview_get_statusbar_text(BaseView *view);

gboolean baseview_get_show_hidden(BaseView *view);
void baseview_set_show_hidden(BaseView *view, gboolean show_hidden);

ThunarZoomLevel baseview_get_zoom_level(BaseView *view);
void baseview_set_zoom_level(BaseView *view, ThunarZoomLevel zoom_level);

void baseview_reload(BaseView *view, gboolean reload_info);

gboolean baseview_get_visible_range(BaseView *view, ThunarFile **start_file,
                                    ThunarFile **end_file);

void baseview_scroll_to_file(BaseView *view, ThunarFile *file,
                             gboolean select_file, gboolean use_align,
                             gfloat row_align, gfloat col_align);

GList* baseview_get_selected_files(BaseView *view);
void baseview_set_selected_files(BaseView *view, GList *path_list);

G_END_DECLS

#endif // __BASEVIEW_H__


