#include <stdlib.h>
#include <string.h>

#include "ttk.h"
#include "ttk_priv.h"

TWindowStack* ttk_windows = 0;
static int ttk_transit_frames = 16;
static enum ttk_justification header_text_justification = TTK_TEXT_CENTER;
static int header_text_pos = -1;

void ttk_set_transition_frames(int frames) {
    if (frames <= 0) frames = 1;
    ttk_transit_frames = frames;
}

void ttk_header_set_text_position(int x) { header_text_pos = x; }
void ttk_header_set_text_justification(enum ttk_justification j) {
    header_text_justification = j;
}

TWindow* ttk_new_window() {
    TWindow* ret = calloc(1, sizeof(TWindow));
    ret->show_header = 1;
    ret->titlefree = 0;
    ret->widgets = 0;
    ret->x = ttk_screen->wx;
    ret->y = ttk_screen->wy;
    ret->w = ttk_screen->w - ttk_screen->wx;
    ret->h = ttk_screen->h - ttk_screen->wy;
    ret->color = (ttk_screen->bpp == 16);
    ret->srf =
        ttk_new_surface(ttk_screen->w, ttk_screen->h, ret->color ? 16 : 2);
    ret->background = 0;
    ret->focus = ret->input = 0;
    ret->dirty = 0;
    ret->epoch = ttk_epoch;
    ret->inbuf_start = ret->inbuf_end = 0;
    ret->onscreen = 0;

    if (ttk_windows) {
        ret->title = strdup(ttk_windows->w->title);
        ret->titlefree = 1;
    } else {
        ret->title = "TTK";
    }
    ttk_fillrect(ret->srf, 0, 0, ret->w, ret->h, ttk_makecol(CKEY));
    return ret;
}

void ttk_free_window(TWindow* win) {
    ttk_hide_window(win);
    if (win->widgets) {
        TWidgetList *cur = win->widgets, *next;
        while (cur) {
            next = cur->next;
            cur->v->win = 0;
            ttk_free_widget(cur->v);
            free(cur);
            cur = next;
        }
    }
    ttk_free_surface(win->srf);
    if (win->titlefree) free((void*)win->title);
    free(win);
}

void ttk_draw_window(TWindow* win) {
    TApItem b, *bg;
    ttk_screeninfo* s = ttk_screen;

    if (win->background)
        memcpy(&b, win->background, sizeof(TApItem));
    else
        memcpy(&b, ttk_ap_getx("window.bg"), sizeof(TApItem));
    b.spacing = 0;
    b.type |= TTK_AP_SPACING;

    if (win->x > s->wx + 2 || win->y > s->wy + 2)
        b.spacing = ttk_ap_getx("window.border")->spacing;
    bg = &b;

    ttk_ap_fillrect(s->srf, bg, win->x, win->y, win->x + win->w,
                    win->y + win->h);
    ttk_blit_image_ex(win->srf, 0, 0, win->w, win->h, s->srf, win->x, win->y);
    if (win->x > s->wx || win->y > s->wy) {
        ttk_ap_rect(s->srf, ttk_ap_get("window.border"), win->x, win->y,
                    win->x + win->w, win->y + win->h);
    }
    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_SCREEN;
}

void ttk_show_window(TWindow* win) {
    if (!win->onscreen) {
        TWindow* oldwindow = ttk_windows ? ttk_windows->w : 0;
        TWindowStack* next = ttk_windows;
        ttk_windows = malloc(sizeof(struct TWindowStack));
        ttk_windows->w = win;
        ttk_windows->minimized = 0;
        ttk_windows->next = next;
        win->onscreen++;

        if (ttk_started && oldwindow && oldwindow->w == win->w &&
            oldwindow->h == win->h && oldwindow->x == ttk_screen->wx &&
            oldwindow->y == ttk_screen->wy) {
            int i;
            int jump = win->w / ttk_transit_frames;
            ttk_fillrect(win->srf, 0, 0, win->w, win->h, ttk_makecol(CKEY));
            iterate_widgets(win->widgets, do_draw, 1);

            for (i = 0; i < ttk_transit_frames; i++) {
                ttk_ap_fillrect(ttk_screen->srf, ttk_ap_get("window.bg"),
                                ttk_screen->wx, ttk_screen->wy, ttk_screen->w,
                                ttk_screen->h);
                ttk_blit_image_ex(oldwindow->srf, i * jump, 0,
                                  oldwindow->w - i * jump, oldwindow->h,
                                  ttk_screen->srf, ttk_screen->wx,
                                  ttk_screen->wy);
                ttk_blit_image_ex(
                    win->srf, 0, 0, i * jump, oldwindow->h, ttk_screen->srf,
                    oldwindow->w - i * jump + ttk_screen->wx, ttk_screen->wy);
                ttk_ap_hline(ttk_screen->srf, ttk_ap_get("header.line"), 0,
                             ttk_screen->w, ttk_screen->wy);
                ttk_gfx_update(ttk_screen->srf);
#ifndef IPOD
                ttk_delay(10);
#endif
            }
            ttk_ap_fillrect(ttk_screen->srf, ttk_ap_get("window.bg"),
                            ttk_screen->wx, ttk_screen->wy, ttk_screen->w,
                            ttk_screen->h);
            ttk_blit_image(win->srf, ttk_screen->srf, ttk_screen->wx,
                           ttk_screen->wy);
            ttk_ap_hline(ttk_screen->srf, ttk_ap_get("header.line"), 0,
                         ttk_screen->w, ttk_screen->wy);
            ttk_gfx_update(ttk_screen->srf);
        }
    } else {
        ttk_move_window(win, 0, TTK_MOVE_ABS);
        ttk_windows->minimized = 0;
    }
    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_HEADER;
    if (ttk_windows->w->input) ttk_dirty |= TTK_DIRTY_INPUT;
}

int ttk_hide_window(TWindow* win) {
    TWindowStack *current = ttk_windows, *last = 0;
    int ret = 0;
    if (!current) return 0;

    while (current) {
        if (current->w == win) {
            if (last)
                last->next = current->next;
            else
                ttk_windows = current->next;
            ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_HEADER;
            free(current);
            current = last;
            win->onscreen = 0;
            ret++;
        }
        last = current;
        if (current) current = current->next;
    }

    if (ret && ttk_windows) {
        TWindow* newwindow = ttk_windows->w;
        if (newwindow->w == win->w && newwindow->h == win->h &&
            newwindow->x == ttk_screen->wx && newwindow->y == ttk_screen->wy) {
            int i;
            int jump = win->w / ttk_transit_frames;
            ttk_fillrect(win->srf, 0, 0, win->w, win->h, ttk_makecol(CKEY));
            iterate_widgets(win->widgets, do_draw, 1);

            for (i = ttk_transit_frames - 1; i >= 0; i--) {
                ttk_ap_fillrect(ttk_screen->srf, ttk_ap_get("window.bg"),
                                ttk_screen->wx, ttk_screen->wy, ttk_screen->w,
                                ttk_screen->h);
                ttk_blit_image_ex(newwindow->srf, i * jump, 0,
                                  win->w - i * jump, win->h, ttk_screen->srf,
                                  ttk_screen->wx, ttk_screen->wy);
                ttk_blit_image_ex(
                    win->srf, 0, 0, i * jump, win->h, ttk_screen->srf,
                    win->w - i * jump + ttk_screen->wx, ttk_screen->wy);
                ttk_ap_hline(ttk_screen->srf, ttk_ap_get("header.line"), 0,
                             ttk_screen->w, ttk_screen->wy);
                ttk_gfx_update(ttk_screen->srf);
#ifndef IPOD
                ttk_delay(10);
#endif
            }
            ttk_ap_fillrect(ttk_screen->srf, ttk_ap_get("window.bg"),
                            ttk_screen->wx, ttk_screen->wy, ttk_screen->w,
                            ttk_screen->h);
            ttk_blit_image(newwindow->srf, ttk_screen->srf, ttk_screen->wx,
                           ttk_screen->wy);
            ttk_ap_hline(ttk_screen->srf, ttk_ap_get("header.line"), 0,
                         ttk_screen->w, ttk_screen->wy);
            ttk_gfx_update(ttk_screen->srf);
        }
    }
    return ret;
}

void ttk_move_window(TWindow* win, int offset, int whence) {
    TWindowStack *cur = ttk_windows, *last = 0;
    int oidx, idx = -1, nitems = 0, i = 0;
    int minimized = 0;
    if (!cur) return;

    while (cur) {
        if ((cur->w == win) && (idx == -1)) {
            idx = nitems;
            if (last)
                last->next = cur->next;
            else
                ttk_windows = cur->next;
            minimized = cur->minimized;
            last = cur;
            cur = cur->next;
            free(last);
        }
        nitems++;
        last = cur;
        if (cur) cur = cur->next;
    }

    if (idx == -1) return;
    oidx = idx;

    switch (whence) {
        case TTK_MOVE_ABS:
            idx = offset;
            break;
        case TTK_MOVE_REL:
            idx -= offset;
            break;
        case TTK_MOVE_END:
            idx = 32767;
            break;
    }
    if (idx < 0) idx = 0;

    cur = ttk_windows;
    last = 0;
    while (cur) {
        if (idx == i) {
            TWindowStack* s = malloc(sizeof(TWindowStack));
            if (last)
                last->next = s;
            else
                ttk_windows = s;
            s->next = cur;
            s->w = win;
            s->minimized = minimized;
            break;
        }
        if (oidx == i) i++;
        last = cur;
        cur = cur->next;
        i++;
    }
    if (!cur && ttk_windows) {
        cur = ttk_windows;
        while (cur->next) cur = cur->next;
        cur->next = malloc(sizeof(TWindowStack));
        cur->next->w = win;
        cur->next->minimized = minimized;
        cur->next->next = 0;
    }
    if (ttk_windows) ttk_windows->minimized = 0;
    ttk_dirty |= TTK_FILTHY;
}

void ttk_set_popup(TWindow* win) {
    int minx = 0, miny = 0, maxx = 0, maxy = 0;
    TWidgetList* cur = win->widgets;
    while (cur) {
        if (cur->v->x < minx) minx = cur->v->x;
        if (cur->v->y < miny) miny = cur->v->y;
        if (cur->v->x + cur->v->w > maxx) maxx = cur->v->x + cur->v->w;
        if (cur->v->y + cur->v->h > maxy) maxy = cur->v->y + cur->v->h;
        cur = cur->next;
    }
    cur = win->widgets;
    while (cur) {
        cur->v->x -= minx;
        cur->v->y -= miny;
        cur = cur->next;
    }
    win->x =
        ((ttk_screen->w - ttk_screen->wx) - (maxx - minx)) / 2 + ttk_screen->wx;
    win->y =
        ((ttk_screen->h - ttk_screen->wy) - (maxy - miny)) / 2 + ttk_screen->wy;
    win->w = maxx - minx;
    win->h = maxy - miny;
}

void ttk_popup_window(TWindow* win) {
    ttk_set_popup(win);
    ttk_show_window(win);
}

void ttk_window_title(TWindow* win, const char* str) {
    if (win->titlefree) free((void*)win->title);
    win->title = malloc(strlen(str) + 1);
    strcpy((char*)win->title, str);
    win->titlefree = 1;
    if (ttk_windows && ttk_windows->w == win) {
        ttk_dirty |= TTK_DIRTY_HEADER;
    }
}

void ttk_window_show_header(TWindow* win) {
    if (!win->show_header) {
        win->show_header = 1;
        ttk_dirty |= TTK_FILTHY;
        if (win->focus) {
            win->focus->h -= 20;
            win->focus->dirty++;
        }
        win->x = ttk_screen->wx;
        win->y = ttk_screen->wy;
        win->w = ttk_screen->w - win->x;
        win->h = ttk_screen->h - win->y;
    }
}

void ttk_window_hide_header(TWindow* win) {
    if (win->show_header) {
        win->show_header = 0;
        ttk_dirty |= TTK_FILTHY;
        if (win->focus) {
            win->focus->h += ttk_screen->wy;
            win->focus->dirty++;
        }
        win->x = 0;
        win->y = 0;
        win->w = ttk_screen->w;
        win->h = ttk_screen->h;
    }
}

void ttk_draw_header_internal(TWindow* win) {
    const char* displayTitle = ttk_filter_sorting_characters(win->title);
    int textpos = 0;
    ttk_screeninfo* s = ttk_screen;

    ttk_ap_fillrect(s->srf, ttk_ap_get("header.bg"), 0, 0, s->w,
                    s->wy + ttk_ap_getx("header.line")->spacing);

    if (ttk_header_widgets) iterate_widgets(ttk_header_widgets, do_draw, 1);

    textpos = ((header_text_pos >= 0) ? header_text_pos : ((s->w) >> 1));
    switch (header_text_justification) {
        case (TTK_TEXT_RIGHT):
            textpos -= ttk_text_width(ttk_menufont, displayTitle);
            break;
        case (TTK_TEXT_CENTER):
        default:
            textpos -= (ttk_text_width(ttk_menufont, displayTitle) >> 1);
            break;
        case (TTK_TEXT_LEFT):
            break;
    }

    ttk_text(s->srf, ttk_menufont, textpos,
             (s->wy - ttk_text_height(ttk_menufont)) / 2,
             ttk_ap_getx("header.fg")->color, displayTitle);

    ttk_ap_hline(s->srf, ttk_ap_get("header.line"), 0, s->w, s->wy);
}
