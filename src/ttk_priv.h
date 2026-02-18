#pragma once

#include "ttk.h"

#ifdef IPOD
#define outl(datum, addr) (*(volatile unsigned long*)(addr) = (datum))
#define inl(addr) (*(volatile unsigned long*)(addr))
#endif

/* Internal Globals */
extern TWidgetList* ttk_header_widgets;
extern int ttk_button_presstime[128];
extern int ttk_button_holdsent[128];
extern int ttk_button_erets[128];
extern TWidget* ttk_button_pressedfor[128];
extern int ttk_first_stap, ttk_last_stap, ttk_last_stap_time, ttk_ignore_stap;
extern int (*ttk_global_evhandler)(int, int, int);
extern int (*ttk_global_unusedhandler)(int, int, int);
extern int ttk_started;
extern int ttk_scroll_num, ttk_scroll_denom;

/* Timer list head (defined in ttk_timer.c) */
extern ttk_timer ttk_timers;

/* Internal Functions */
void ttk_widget_nodrawing(TWidget* w, ttk_surface s);
int ttk_widget_noaction_2(TWidget* w, int i1, int i2);
int ttk_widget_noaction_1(TWidget* w, int i);
int ttk_widget_noaction_i0(TWidget* w);
void ttk_widget_noaction_0(TWidget* w);

/* Helpers used by ttk_run */
int iterate_widgets(TWidgetList* wids, int (*func)(TWidget*, int), int arg);
int do_timers(TWidget* wid, int tick);
int do_draw(TWidget* wid, int force);
int check_dirty(TWidget* wid, int unused);

/* Internal drawing helpers */
void ttk_draw_header_internal(TWindow* win);
