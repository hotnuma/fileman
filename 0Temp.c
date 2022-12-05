
#if 0

thunar_window_notebook_insert
thunar_window_notebook_page_added
thunar_window_notebook_show_tabs
thunar_window_notebook_switch_page
thunar_window_notebook_page_removed

/**
 * SECTION:thunar-launcher
 * @Short_description: Manages creation and execution of menu-item
 * @Title: ThunarLauncher
 *
 * The #ThunarLauncher class manages the creation and execution of menu-item which are used by multiple menus.
 * The management is done in a central way to prevent code duplication on various places.
 * XfceGtkActionEntry is used in order to define a list of the managed items and ease the setup of single items.
 *
 * #ThunarLauncher implements the #ThunarNavigator interface in order to use the "open in new tab" and "change directory" service.
 * It as well tracks the current directory via #ThunarNavigator.
 *
 * #ThunarLauncher implements the #ThunarComponent interface in order to track the currently selected files.
 * Based on to the current selection(and some other criteria), some menu items will not be shown, or will be insensitive.
 *
 * Files which are opened via #ThunarLauncher are poked first in order to e.g do missing mount operations.
 *
 * As well menu-item related services, like activation of selected files and opening tabs/new windows,
 * are provided by #ThunarLauncher.
 *
 * It is required to keep an instance of #ThunarLauncher open, in order to listen to accellerators which target
 * menu-items managed by #ThunarLauncher.
 * Typically a single instance of #ThunarLauncher is provided by each #ThunarWindow.
 */


#endif


