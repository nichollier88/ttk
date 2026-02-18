#include <stdlib.h>

#include "ttk.h"
#include "ttk_priv.h"

TWidgetList* ttk_header_widgets = 0;

void ttk_widget_nodrawing(TWidget* w, ttk_surface s) {}
int ttk_widget_noaction_2(TWidget* w, int i1, int i2) { return TTK_EV_UNUSED; }
int ttk_widget_noaction_1(TWidget* w, int i) { return TTK_EV_UNUSED; }
int ttk_widget_noaction_i0(TWidget* w) { return 0; }
void ttk_widget_noaction_0(TWidget* w) {}

TWidget* ttk_new_widget(int x, int y) {
    TWidget* ret = malloc(sizeof(TWidget));
    if (!ret) return 0;

    ret->x = x;
    ret->y = y;
    ret->w = 0;
    ret->h = 0;
    ret->focusable = 0;
    ret->keyrepeat = 0;
    ret->rawkeys = 0;
    ret->framelast = 0;
    ret->framedelay = 0;
    ret->timerlast = 0;
    ret->timerdelay = 0;
    ret->holdtime = 1000;
    ret->dirty = 1;

    ret->draw = ttk_widget_nodrawing;
    ret->button = ttk_widget_noaction_2;
    ret->down = ret->held = ret->scroll = ret->stap = ttk_widget_noaction_1;
    ret->input = ttk_widget_noaction_1;
    ret->frame = ret->timer = ttk_widget_noaction_i0;
    ret->destroy = ttk_widget_noaction_0;
    ret->data = ret->data2 = 0;
    return ret;
}

void ttk_free_widget(TWidget* wid) {
    if (!wid) return;
    wid->destroy(wid);
    if (wid->win) ttk_remove_widget(wid->win, wid);
    free(wid);
}

TWindow* ttk_add_widget(TWindow* win, TWidget* wid) {
    TWidgetList* current;
    if (!wid || !win) return win;

    if (!win->widgets) {
        win->widgets = current = malloc(sizeof(TWidgetList));
    } else {
        current = win->widgets;
        while (current->next) current = current->next;
        current->next = malloc(sizeof(TWidgetList));
        current = current->next;
    }

    if (wid->focusable) win->focus = wid;
    wid->dirty++;
    wid->win = win;
    current->v = wid;
    current->next = 0;
    return win;
}

int ttk_remove_widget(TWindow* win, TWidget* wid) {
    TWidgetList *current, *last = 0;
    int count = 0;
    if (!win || !wid || !win->widgets) return 0;

    if (wid == win->focus) win->focus = 0;
    current = win->widgets;

    while (current) {
        if (current->v == wid) {
            if (last)
                last->next = current->next;
            else
                win->widgets = current->next;
            free(current);
            if (last)
                current = last->next;
            else
                current = win->widgets;
            count++;
        } else {
            if (current && current->v->focusable) win->focus = current->v;
            if (current) last = current, current = current->next;
        }
    }
    win->dirty++;
    wid->win = 0;
    return count;
}

void ttk_widget_set_fps(TWidget* wid, int fps) {
    if (fps) {
        wid->framelast = ttk_getticks();
        wid->framedelay = 1000 / fps;
    } else {
        wid->framelast = wid->framedelay = 0;
    }
}

void ttk_widget_set_inv_fps(TWidget* wid, int fps_m1) {
    wid->framelast = ttk_getticks();
    wid->framedelay = 1000 * fps_m1;
}

void ttk_widget_set_timer(TWidget* wid, int ms) {
    wid->timerlast = ttk_getticks();
    wid->timerdelay = ms;
}

void ttk_add_header_widget(TWidget* wid) {
    TWidgetList* current;
    wid->win = 0;
    if (!ttk_header_widgets) {
        ttk_header_widgets = current = malloc(sizeof(TWidgetList));
    } else {
        current = ttk_header_widgets;
        while (current->next) current = current->next;
        current->next = malloc(sizeof(TWidgetList));
        current = current->next;
    }
    current->v = wid;
    current->next = 0;
}

void ttk_remove_header_widget(TWidget* wid) {
    TWidgetList *current = ttk_header_widgets, *last = 0;
    if (!current) return;
    while (current) {
        if (current->v == wid) {
            if (last)
                last->next = current->next;
            else
                ttk_header_widgets = current->next;
            free(current);
            if (last)
                current = last->next;
            else
                current = ttk_header_widgets;
        } else {
            if (current) last = current, current = current->next;
        }
    }
}
