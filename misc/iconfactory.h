/*-
 * Copyright(c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifndef __ICONFACTORY_H__
#define __ICONFACTORY_H__

#include "th-file.h"

G_BEGIN_DECLS

typedef struct _IconFactory      IconFactory;
typedef struct _IconFactoryClass IconFactoryClass;

#define TYPE_ICONFACTORY (iconfact_get_type())

#define ICONFACTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj), TYPE_ICONFACTORY, IconFactory))
#define ICONFACTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),  TYPE_ICONFACTORY, IconFactoryClass))
#define IS_ICONFACTORY(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj), TYPE_ICONFACTORY))
#define IS_ICONFACTORY_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),  TYPE_ICONFACTORY))
#define ICONFACTORY_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),  TYPE_ICONFACTORY, IconFactoryClass))

GType iconfact_get_type() G_GNUC_CONST;

IconFactory* iconfact_get_default();
IconFactory* iconfact_get_for_icon_theme(GtkIconTheme *icon_theme);

GdkPixbuf* iconfact_load_icon(IconFactory *factory, const gchar *name,
                              gint size, gboolean wants_default);

GdkPixbuf* iconfact_load_file_icon(IconFactory *factory,
                                   ThunarFile *file,
                                   FileIconState icon_state,
                                   gint icon_size);

void iconfact_clear_pixmap_cache(ThunarFile *file);

G_END_DECLS

#endif // __ICONFACTORY_H__


