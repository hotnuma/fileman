/*-
 * Copyright(c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __STANDARD_VIEW_H__
#define __STANDARD_VIEW_H__

#include <baseview.h>
#include <listmodel.h>
#include <icon_factory.h>
#include <history.h>
#include <clipman.h>

G_BEGIN_DECLS

typedef struct _StandardViewPrivate StandardViewPrivate;
typedef struct _StandardViewClass   StandardViewClass;
typedef struct _StandardView        StandardView;

#define TYPE_STANDARD_VIEW (standard_view_get_type())
#define STANDARD_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_CAST((obj),  TYPE_STANDARD_VIEW, StandardView))
#define STANDARD_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_CAST((klass),   TYPE_STANDARD_VIEW, StandardViewClass))
#define IS_STANDARD_VIEW(obj) \
    (G_TYPE_CHECK_INSTANCE_TYPE((obj),  TYPE_STANDARD_VIEW))
#define IS_STANDARD_VIEW_CLASS(klass) \
    (G_TYPE_CHECK_CLASS_TYPE((klass),   TYPE_STANDARD_VIEW))
#define STANDARD_VIEW_GET_CLASS(obj) \
    (G_TYPE_INSTANCE_GET_CLASS((obj),   TYPE_STANDARD_VIEW, StandardViewClass))

// Action Entrys provided by this widget

typedef enum
{
    STANDARD_VIEW_ACTION_SELECT_ALL_FILES,
    STANDARD_VIEW_ACTION_SELECT_BY_PATTERN,
    STANDARD_VIEW_ACTION_INVERT_SELECTION,

} StandardViewAction;

struct _StandardViewClass
{
    GtkScrolledWindowClass __parent__;

    /* Returns the list of currently selected GtkTreePath's, where
     * both the list and the items are owned by the caller. */
    GList*  (*get_selected_items)   (StandardView *standard_view);

    /* Selects all items in the view */
    void    (*select_all)           (StandardView *standard_view);

    /* Unselects all items in the view */
    void    (*unselect_all)         (StandardView *standard_view);

    /* Invert selection in the view */
    void    (*selection_invert)     (StandardView *standard_view);

    /* Selects the given item */
    void    (*select_path)          (StandardView *standard_view,
                                     GtkTreePath        *path);

    /* Called by the StandardView class to let derived class
     * place the cursor on the item/row referred to by path. If
     * start_editing is TRUE, the derived class should also start
     * editing that item/row.
     */
    void    (*set_cursor)           (StandardView *standard_view,
                                     GtkTreePath        *path,
                                     gboolean            start_editing);

    /* Called by the StandardView class to let derived class
     * scroll the view to the given path.
     */
    void    (*scroll_to_path)       (StandardView *standard_view,
                                     GtkTreePath        *path,
                                     gboolean            use_align,
                                     gfloat              row_align,
                                     gfloat              col_align);

    /* Returns the path at the given position or NULL if no item/row
     * is located at that coordinates. The path is freed by the caller.
     */
    GtkTreePath* (*get_path_at_pos) (StandardView *standard_view,
                                     gint                x,
                                     gint                y);

    /* Returns the visible range */
    gboolean (*get_visible_range)   (StandardView *standard_view,
                                     GtkTreePath       **start_path,
                                     GtkTreePath       **end_path);

    /* Sets the item/row that is highlighted for feedback. NULL is
     * passed for path to disable the highlighting.
     */
    void    (*highlight_path)       (StandardView  *standard_view,
                                     GtkTreePath         *path);

    /* external signals */
    void    (*start_open_location)  (StandardView *standard_view,
                                     const gchar        *initial_text);

    /* Appends view-specific menu items to the given menu */
    void    (*append_menu_items)    (StandardView *standard_view, GtkMenu *menu, GtkAccelGroup *accel_group);

    /* Connects view-specific accelerators to the given accelGroup */
    void    (*connect_accelerators) (StandardView *standard_view, GtkAccelGroup *accel_group);

    /* Disconnects view-specific accelerators to the given accelGroup */
    void    (*disconnect_accelerators) (StandardView *standard_view, GtkAccelGroup *accel_group);

    /* Internal action signals */
    gboolean (*delete_selected_files) (StandardView *standard_view);

    /* The name of the property in ThunarPreferences, that determines
     * the last(and default) zoom-level for the view classes.
     */
    const gchar* zoom_level_property_name;
};

struct _StandardView
{
    GtkScrolledWindow   __parent__;

    ListModel     *model;

    IconFactory   *icon_factory;
    GtkCellRenderer     *icon_renderer;
    GtkCellRenderer     *name_renderer;

    GBinding            *loading_binding;
    gboolean            loading;
    GtkAccelGroup       *accel_group;

    StandardViewPrivate *priv;
};

GType standard_view_get_type() G_GNUC_CONST;

void standard_view_context_menu(StandardView *standard_view);
void standard_view_queue_popup(StandardView *standard_view,
                               GdkEventButton *event);

void standard_view_selection_changed(StandardView *standard_view);

ThunarHistory* standard_view_get_history(StandardView *standard_view);
void standard_view_set_history(StandardView *standard_view,
                               ThunarHistory *history);

G_END_DECLS

#endif // __STANDARD_VIEW_H__


