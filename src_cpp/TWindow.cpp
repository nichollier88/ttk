#include "TWindow.hpp"

#include <stdlib.h>
#include <string.h>

TWindow::TWindow() {
    // Default initialization
    show_header = 1;
    titlefree = 0;
    widgets = 0;
    x = ttk_screen->wx;
    y = ttk_screen->wy;
    w = ttk_screen->w - ttk_screen->wx;
    h = ttk_screen->h - ttk_screen->wy;
    color = (ttk_screen->bpp == 16);
    srf = ttk_new_surface(ttk_screen->w, ttk_screen->h, color ? 16 : 2);
    background = 0;
    focus = input = 0;
    dirty = 0;
    epoch = ttk_epoch;
    inbuf_start = inbuf_end = 0;
    onscreen = 0;
    data = 0;
    data2 = 0;
    memset(inbuf, 0, sizeof(inbuf));

    if (ttk_windows) {
        title = strdup(ttk_windows->w->title);
        titlefree = 1;
    } else {
        title = "TTK";
    }

    ttk_fillrect(srf, 0, 0, w, h, ttk_makecol(CKEY));
}

TWindow::~TWindow() {
    ttk_hide_window(this);

    if (widgets) {
        TWidgetList *cur = widgets, *next;
        while (cur) {
            next = cur->next;
            cur->v->win = 0;
            ttk_free_widget(cur->v);
            free(cur);
            cur = next;
        }
    }
    ttk_free_surface(srf);
    if (titlefree) free((void*)title);
}
