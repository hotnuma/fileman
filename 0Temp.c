
#ifdef THUMB
static void _standardview_scrolled(GtkAdjustment *adjustment, StandardView *view);

static void _standardview_scrolled(GtkAdjustment *adjustment, StandardView *view)
{
    e_return_if_fail(GTK_IS_ADJUSTMENT(adjustment));
    e_return_if_fail(IS_STANDARD_VIEW(view));

    // ignore adjustment changes when the view is still loading
    if (baseview_get_loading(BASEVIEW(view)))
        return;
}

// connect to scroll events for generating thumbnail requests
    GtkAdjustment *adjustment =
        gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(view));
    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(_standardview_scrolled), object);
    adjustment = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(view));
    g_signal_connect(adjustment, "value-changed",
                     G_CALLBACK(_standardview_scrolled), object);
#endif

#ifdef THUMB
static void _standardview_size_allocate(StandardView *view,
                                        GtkAllocation *allocation);

static void _standardview_size_allocate(StandardView  *view,
                                        GtkAllocation *allocation)
{
    (void) allocation;

    e_return_if_fail(IS_STANDARD_VIEW(view));

    // ignore size changes when the view is still loading
    if (baseview_get_loading(BASEVIEW(view)))
        return;
}

// connect to size allocation signals for generating thumbnail requests
    g_signal_connect_after(G_OBJECT(view),
                           "size-allocate",
                           G_CALLBACK(_standardview_size_allocate),
                           NULL);
#endif


