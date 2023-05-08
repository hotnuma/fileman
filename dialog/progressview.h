/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __PROGRESSVIEW_H__
#define __PROGRESSVIEW_H__

#include <gtk/gtk.h>

#include <job.h>

G_BEGIN_DECLS

typedef struct _ProgressViewClass ProgressViewClass;
typedef struct _ProgressView      ProgressView;

#define TYPE_PROGRESSVIEW (progressview_get_type())
#define PROGRESSVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_PROGRESSVIEW, ProgressView))
#define PROGRESSVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass), TYPE_PROGRESSVIEW, ProgressViewClass))
#define IS_PROGRESSVIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_PROGRESSVIEW))
#define IS_PROGRESSVIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass), TYPE_PROGRESSVIEW))
#define PROGRESSVIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj), TYPE_PROGRESSVIEW, ProgressViewClass))

GType progressview_get_type() G_GNUC_CONST;

GtkWidget* progressview_new_with_job(ThunarJob *job) G_GNUC_MALLOC;

void progressview_set_icon_name(ProgressView *view, const gchar *icon_name);
void progressview_set_title(ProgressView *view, const gchar *title);
ThunarJob *progressview_get_job(ProgressView *view);

G_END_DECLS

#endif // __PROGRESSVIEW_H__


