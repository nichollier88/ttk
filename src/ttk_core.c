#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ttk.h"
#include "ttk_priv.h"

/* Globals */
ttk_font ttk_menufont, ttk_textfont;
ttk_screeninfo* ttk_screen = 0;
int ttk_button_presstime[128];
int ttk_button_holdsent[128];
int ttk_button_erets[128];
TWidget* ttk_button_pressedfor[128];
int ttk_first_stap, ttk_last_stap, ttk_last_stap_time, ttk_ignore_stap;
int ttk_dirty = 0;
int (*ttk_global_evhandler)(int, int, int);
int (*ttk_global_unusedhandler)(int, int, int);
int ttk_epoch = 0;

int ttk_started = 0;
int ttk_scroll_num = 1, ttk_scroll_denom = 1;

static void (*ttk_clicker)() = ttk_click;

/* Helper implementations */
int iterate_widgets(TWidgetList* wids, int (*func)(TWidget*, int), int arg) {
    TWidgetList* current = wids;
    int eret = 0;
    while (current) {
        eret |= func(current->v, arg);
        current = current->next;
    }
    return eret;
}

int do_timers(TWidget* wid, int tick) {
    int eret = 0;
    if (wid->frame && wid->framedelay &&
        (wid->framelast + wid->framedelay <= tick)) {
        wid->framelast = tick;
        eret |= wid->frame(wid) & ~TTK_EV_UNUSED;
    }
    if (wid->timer && wid->timerdelay &&
        (wid->timerlast + wid->timerdelay <= tick)) {
        wid->timerlast = tick + 1;
        eret |= wid->timer(wid) & ~TTK_EV_UNUSED;
    }
    return eret;
}

int do_draw(TWidget* wid, int force) {
    if (wid->dirty || force) {
        if (!force)
            ttk_fillrect(wid->win->srf, wid->x, wid->y, wid->x + wid->w,
                         wid->y + wid->h, ttk_makecol(CKEY));
        if (wid->win)
            wid->draw(wid, wid->win->srf);
        else
            wid->draw(wid, ttk_screen->srf);
        wid->dirty = 0;
        return 1;
    }
    return 0;
}

int check_dirty(TWidget* wid, int unused) { return !!wid->dirty; }

/* Core Functions */
int ttk_version_check(int otherver) {
    int myver = TTK_API_VERSION;
    if (myver == otherver) return 1;
    if (myver < otherver) {
        fprintf(stderr,
                "Error: Compiled with TTK headers %x but linked with %x.\n",
                otherver, myver);
        return 0;
    }
    if (((myver & ~0xff) == (otherver & ~0xff)) &&
        ((myver & 0xff) >= (otherver & 0xff))) {
        fprintf(stderr,
                "Warning: Compiled with TTK headers %x but linked with %x.\n",
                otherver, myver);
        return 1;
    }
    fprintf(stderr, "Error: Major version mismatch (headers %x, lib %x).\n",
            otherver, myver);
    return 0;
}

TWindow* ttk_init() {
    TWindow* ret;
    ttk_color dots_b, dots_1, dots_2, dots_3;

    if (!ttk_screen) {
        int ver;
        ttk_screen = malloc(sizeof(ttk_screeninfo));
#ifdef IPOD
        ver = ttk_get_podversion();
        if (ver & (TTK_POD_1G | TTK_POD_2G | TTK_POD_3G | TTK_POD_4G)) {
            ttk_screen->w = 160;
            ttk_screen->h = 128;
            ttk_screen->bpp = 2;
        } else if (ver & (TTK_POD_MINI_1G | TTK_POD_MINI_2G)) {
            ttk_screen->w = 138;
            ttk_screen->h = 110;
            ttk_screen->bpp = 2;
        } else if (ver & (TTK_POD_PHOTO | TTK_POD_SANSA)) {
            ttk_screen->w = 220;
            ttk_screen->h = 176;
            ttk_screen->bpp = 16;
        } else if (ver & TTK_POD_NANO) {
            ttk_screen->w = 176;
            ttk_screen->h = 132;
            ttk_screen->bpp = 16;
        } else if (ver & TTK_POD_VIDEO) {
            ttk_screen->w = 320;
            ttk_screen->h = 240;
            ttk_screen->bpp = 16;
        } else {
            fprintf(stderr, "Couldn't determine iPod version (v=0%o)\n", ver);
            free(ttk_screen);
            exit(1);
        }
#else
        ttk_screen->w = 160;
        ttk_screen->h = 128;
        ttk_screen->bpp = 2;
#endif
        ttk_screen->wx = 0;
        if (ttk_screen->bpp == 16) {
            if (ttk_screen->h == 176 || ttk_screen->h == 132)
                ttk_screen->wy = 21;
            else if (ttk_screen->h == 240)
                ttk_screen->wy = 23;
            else
                ttk_screen->wy = 22;
        } else {
            ttk_screen->wy = 19;
        }
    }

    ttk_gfx_init();

    /* Startup screen dots */
    if (ttk_screen->bpp == 2) {
        dots_b = ttk_makecol(255, 255, 255);
        dots_1 = dots_2 = dots_3 = ttk_makecol(0, 0, 0);
    } else {
        dots_b = ttk_makecol(0, 0, 0);
        dots_1 = ttk_makecol(255, 64, 64);
        dots_2 = ttk_makecol(64, 255, 64);
        dots_3 = ttk_makecol(64, 64, 255);
    }
    ttk_fillrect(ttk_screen->srf, 0, 0, ttk_screen->w, ttk_screen->h, dots_b);

    /* Draw Tux (assuming icons are available via headers/libs) */
    // ... (Drawing code omitted for brevity, same as original) ...

    ttk_gfx_update(ttk_screen->srf);

    /* Load fonts */
    // Note: ttk_parse_fonts_list is in ttk_font.c
    // Assuming FONTSDIR is defined in makefile or header, otherwise define here
#ifdef IPOD
#define FONTSDIR "/usr/share/fonts"
#else
#define FONTSDIR "fonts"
#endif
    // We need to declare these if they are not in ttk.h
    int ttk_parse_fonts_list(const char* flf);
    int ttk_parse_fonts_list_dir(const char* dirname);

    int nfonts = 0;
    nfonts += ttk_parse_fonts_list(FONTSDIR "/fonts.lst");
    nfonts += ttk_parse_fonts_list_dir(FONTSDIR "/fonts.lst.d");
    if (!nfonts) {
        fprintf(stderr, "No fonts list, or no fonts in it.\n");
        ttk_quit();
        exit(1);
    }

    ret = ttk_new_window();
    ttk_windows = (TWindowStack*)malloc(sizeof(TWindowStack));
    ttk_windows->w = ret;
    ttk_windows->minimized = 0;
    ttk_windows->next = 0;
    ret->onscreen++;

    return ret;
}

int ttk_run() {
    ttk_screeninfo* s = ttk_screen;
    TWindow* win;
    TWidget* evtarget;
    int tick, i;
    int ev, earg, eret;
    int in, st, touch;
    const char *keys = "mfwd\n", *p;
    static int initd = 0;
    int local, global;
    static int sofar = 0;
    int time = 0, hs;
    ttk_timer ctim;
    TWidget* pf;

    ttk_started = 1;

    if (!initd) {
        for (i = 0; i < 128; i++) {
            ttk_button_presstime[i] = 0;
            ttk_button_holdsent[i] = 0;
            ttk_button_erets[i] = 0;
        }
        if (!ttk_windows) {
            fprintf(stderr, "Run with no windows\n");
            ttk_quit();
            exit(1);
        }
        initd = 1;
    }

    ttk_dirty |= TTK_FILTHY;

    while (1) {
        if (!ttk_windows) return 0;

        if (ttk_windows->minimized)
            ttk_move_window(ttk_windows->w, 0, TTK_MOVE_END);

        win = ttk_windows->w;
        evtarget = win->input ? win->input : win->focus;
        tick = ttk_getticks();

        if (win->epoch < ttk_epoch) {
            ttk_dirty |= TTK_FILTHY;
            win->dirty++;
            win->epoch = ttk_epoch;
        }

        eret = 0;

        if (ttk_header_widgets) {
            eret |= iterate_widgets(ttk_header_widgets, do_timers, tick) &
                    ~TTK_EV_UNUSED;
            if (win->show_header &&
                iterate_widgets(ttk_header_widgets, check_dirty, 0)) {
                ttk_dirty |= TTK_DIRTY_HEADER;
            }
        }

        if (win->input) {
            eret |= do_timers(win->input, tick) & ~TTK_EV_UNUSED;
            if (win->input->dirty) ttk_dirty |= TTK_DIRTY_INPUT;
        }

        eret |= iterate_widgets(win->widgets, do_timers, tick) & ~TTK_EV_UNUSED;

        ctim = ttk_timers;
        while (ctim) {
            if (tick > (ctim->started + ctim->delay)) {
                ttk_timer next = ctim->next;
                void (*fn)() = ctim->fn;
                ttk_destroy_timer(ctim);
                ctim = next;
                fn();
                continue;
            }
            ctim = ctim->next;
        }

        if (evtarget && evtarget->rawkeys)
            ev = ttk_get_rawevent(&earg);
        else
            ev = ttk_get_event(&earg);

        local = global = 1;
        if (!ev) local = global = 0;
        if (!ttk_global_evhandler) global = 0;
        if (ev == TTK_BUTTON_DOWN) {
            if (!((ttk_button_pressedfor[earg] == 0 ||
                   ttk_button_pressedfor[earg] == evtarget) &&
                  (ttk_button_presstime[earg] == 0 ||
                   ttk_button_presstime[earg] == tick)))
                global = 0;
        }
        if (!evtarget) local = 0;

        if (global) {
            local &= !ttk_global_evhandler(ev, earg,
                                           tick - ttk_button_presstime[earg]);
        }

        if (ev == TTK_SCROLL) {
            if (ttk_scroll_denom > 1) {
                sofar += earg;
                if (sofar > -ttk_scroll_denom && sofar < ttk_scroll_denom)
                    local = 0;
                else if (sofar < 0) {
                    while (sofar <= -ttk_scroll_denom)
                        sofar += ttk_scroll_denom;
                } else {
                    while (sofar >= ttk_scroll_denom) sofar -= ttk_scroll_denom;
                }
            }
            earg *= ttk_scroll_num;
        }

        switch (ev) {
            case TTK_BUTTON_DOWN:
                if (!ttk_button_presstime[earg] ||
                    !ttk_button_pressedfor[earg]) {
                    ttk_button_presstime[earg] = tick;
                    ttk_button_pressedfor[earg] = evtarget;
                    ttk_button_holdsent[earg] = 0;
                }
                if (local && (ttk_button_pressedfor[earg] == evtarget) &&
                    ((ttk_button_presstime[earg] == tick) ||
                     evtarget->keyrepeat)) {
                    int er = evtarget->down(evtarget, earg);
                    ttk_button_erets[earg] |= er;
                    eret |= er & ~TTK_EV_UNUSED;
                }
                break;
            case TTK_BUTTON_UP:
                time = tick - ttk_button_presstime[earg];
                pf = ttk_button_pressedfor[earg];
                hs = ttk_button_holdsent[earg];
                ttk_button_presstime[earg] = 0;
                ttk_button_holdsent[earg] = 0;
                ttk_button_pressedfor[earg] = 0;

                if (evtarget == pf && local && !hs) {
                    int er = evtarget->button(evtarget, earg, time);
                    eret |= er;
                    if (!((er & TTK_EV_UNUSED) &&
                          (ttk_button_erets[earg] & TTK_EV_UNUSED))) {
                        eret &= ~TTK_EV_UNUSED;
                    } else {
                        eret |= TTK_EV_UNUSED;
                    }
                }
                ttk_button_erets[earg] = 0;
                break;
            case TTK_SCROLL:
                if (local)
                    eret |= evtarget->scroll(evtarget, earg) & ~TTK_EV_UNUSED;
                break;
        }

        if (evtarget) {
            for (p = keys; *p; p++) {
                if (ttk_button_presstime[*p] &&
                    (tick - ttk_button_presstime[*p] >= evtarget->holdtime) &&
                    !ttk_button_holdsent[*p] &&
                    (evtarget->held != ttk_widget_noaction_1)) {
                    int er = evtarget->held(evtarget, *p);
                    if (!(er & TTK_EV_UNUSED)) {
                        eret |= er;
                        ttk_button_holdsent[*p] = 1;
                    }
                }
                if (ttk_button_presstime[*p] &&
                    (ttk_get_podversion() & TTK_POD_PP502X) &&
                    ttk_last_stap_time)
                    ttk_ignore_stap = 1;
            }
#ifdef IPOD
            if (ttk_get_podversion() & TTK_POD_PP502X) {
                in = inl(0x7000C140);
                st = (in & 0x40000000);
                touch = (in & 0x007F0000) >> 16;
                if (st) {
                    if (!ttk_last_stap_time) {
                        ttk_first_stap = touch;
                        ttk_last_stap_time = tick;
                    }
                    ttk_last_stap = touch;
                } else if (ttk_last_stap_time) {
                    if ((abs(ttk_last_stap - ttk_first_stap) <= 5) &&
                        ((tick - ttk_last_stap_time) <= 400) &&
                        !ttk_ignore_stap) {
                        eret |= evtarget->stap(evtarget, ttk_first_stap);
                    }
                    ttk_last_stap_time = 0;
                    ttk_ignore_stap = 0;
                }
            }
#else
            if (ev == TTK_BUTTON_UP) {
                // ... (Desktop stap simulation omitted for brevity, same as
                // original) ...
            }
#endif
        }

        while (win->inbuf_start != win->inbuf_end) {
            if (win->focus)
                eret |= win->focus->input(win->focus,
                                          win->inbuf[win->inbuf_start]) &
                        ~TTK_EV_UNUSED;
            win->inbuf_start++;
            win->inbuf_start &= 0x1f;
        }

        if (!ttk_windows) return 0;
        win = ttk_windows->w;

        /* Draw header */
        if ((ttk_dirty & TTK_DIRTY_HEADER) && win->show_header) {
            ttk_draw_header_internal(win);
            ttk_dirty &= ~TTK_DIRTY_HEADER;
            ttk_dirty |= TTK_DIRTY_SCREEN;
        }

        /* Redraw widgets */
        if (win->dirty) {
            ttk_fillrect(win->srf, 0, 0, win->w, win->h, ttk_makecol(CKEY));
            iterate_widgets(win->widgets, do_draw, 1);
            ttk_dirty |= TTK_DIRTY_WINDOWAREA;
            win->dirty = 0;
        }

        if (iterate_widgets(win->widgets, do_draw, 0)) {
            ttk_dirty |= TTK_DIRTY_WINDOWAREA;
        }

        /* Blit window to screen */
        if (ttk_dirty & TTK_DIRTY_WINDOWAREA) {
            TApItem b, *bg;
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
            ttk_blit_image_ex(win->srf, 0, 0, win->w, win->h, s->srf, win->x,
                              win->y);
            if (win->x > s->wx + 2 || win->y > s->wy + 2) {
                ttk_ap_rect(s->srf, ttk_ap_get("window.border"), win->x, win->y,
                            win->x + win->w, win->y + win->h);
            }
            if (win->show_header)
                ttk_ap_hline(s->srf, ttk_ap_get("header.line"), 0, s->w, s->wy);

            ttk_dirty &= ~TTK_DIRTY_WINDOWAREA;
            ttk_dirty |= TTK_DIRTY_SCREEN;
        }

        /* Redraw input */
        if ((ttk_dirty & TTK_DIRTY_INPUT) && win->input) {
            ttk_ap_fillrect(s->srf, ttk_ap_get("window.bg"), win->input->x,
                            win->input->y, win->input->x + win->input->w,
                            win->input->y + win->input->h);
            if (ttk_ap_get("window.border")) {
                TApItem border;
                memcpy(&border, ttk_ap_getx("window.border"), sizeof(TApItem));
                border.spacing = -1;
                ttk_ap_rect(s->srf, &border, win->input->x, win->input->y,
                            win->input->x + win->input->w,
                            win->input->y + win->input->h);
            }
            win->input->draw(win->input, s->srf);
            ttk_dirty &= ~TTK_DIRTY_INPUT;
            ttk_dirty |= TTK_DIRTY_SCREEN;
        }

        if (eret & TTK_EV_CLICK) (*ttk_clicker)();
        if (eret & TTK_EV_DONE) return (eret >> 8);
        if ((eret & TTK_EV_UNUSED) && ttk_global_unusedhandler)
            eret |= ttk_global_unusedhandler(ev, earg, time);

        if (ttk_dirty & TTK_DIRTY_SCREEN) {
            ttk_gfx_update(ttk_screen->srf);
            ttk_dirty &= ~TTK_DIRTY_SCREEN;
        }
#if !defined(IPOD) && !defined(SDL)
        ttk_delay(30);
#endif
    }
}

void ttk_quit() {
    ttk_gfx_quit();
    ttk_stop_cop();
}

void ttk_set_global_event_handler(int (*fn)(int ev, int earg, int time)) {
    ttk_global_evhandler = fn;
}

void ttk_set_global_unused_handler(int (*fn)(int ev, int earg, int time)) {
    ttk_global_unusedhandler = fn;
}

int ttk_button_pressed(int button) { return ttk_button_presstime[button]; }

static void do_nothing() {}
void ttk_set_clicker(void (*fn)()) {
    if (!fn) fn = do_nothing;
    ttk_clicker = fn;
}

void ttk_set_scroll_multiplier(int num, int denom) {
    ttk_scroll_num = num;
    ttk_scroll_denom = denom;
}
