/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#ifndef __SHORTCUT_RENDERER_H__
#define __SHORTCUT_RENDERER_H__

#include <icon-render.h>

G_BEGIN_DECLS

typedef struct _ShortcutRendererClass ShortcutRendererClass;
typedef struct _ShortcutRenderer      ShortcutRenderer;

#define TYPE_SHORTCUT_RENDERER (srender_get_type())
#define SHORTCUT_RENDERER(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_SHORTCUT_RENDERER, ShortcutRenderer))
#define SHORTCUT_RENDERER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_SHORTCUT_RENDERER, ShortcutRendererClass))
#define IS_SHORTCUT_RENDERER(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_SHORTCUT_RENDERER))
#define IS_SHORTCUT_RENDERER_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_SHORTCUT_RENDERER))
#define SHORTCUT_RENDERER_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_SHORTCUT_RENDERER, ShortcutRendererClass))

GType srender_get_type() G_GNUC_CONST;

GtkCellRenderer* srender_new() G_GNUC_MALLOC;

G_END_DECLS

#endif // __SHORTCUT_RENDERER_H__


