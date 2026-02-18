#include "ttk.h"
#include "ttk_priv.h"

int ttk_input_start_for(TWindow* win, TWidget* inmethod) {
    inmethod->x = ttk_screen->w - inmethod->w - 1;
    inmethod->y = ttk_screen->h - inmethod->h - 1;
    inmethod->win = win;
    win->input = inmethod;
    return inmethod->h;
}

int ttk_input_start(TWidget* inmethod) {
    if (!ttk_windows || !ttk_windows->w) return -1;
    ttk_input_start_for(ttk_windows->w, inmethod);
    ttk_dirty |= TTK_DIRTY_WINDOWAREA | TTK_DIRTY_INPUT;
    return inmethod->h;
}

void ttk_input_move(int x, int y) {
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    ttk_windows->w->input->x = x;
    ttk_windows->w->input->y = y;
}

void ttk_input_move_for(TWindow* win, int x, int y) {
    win->input->x = x;
    win->input->y = y;
}

void ttk_input_size(int* w, int* h) {
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    if (w) *w = ttk_windows->w->input->w;
    if (h) *h = ttk_windows->w->input->h;
}

void ttk_input_size_for(TWindow* win, int* w, int* h) {
    if (w) *w = win->input->w;
    if (h) *h = win->input->h;
}

void ttk_input_char(int ch) {
    if (!ttk_windows || !ttk_windows->w) return;
    ttk_windows->w->inbuf[ttk_windows->w->inbuf_end++] = ch;
    ttk_windows->w->inbuf_end &= 0x1f;
}

void ttk_input_end() {
    if (!ttk_windows || !ttk_windows->w || !ttk_windows->w->input) return;
    ttk_input_char(TTK_INPUT_END);
    ttk_free_widget(ttk_windows->w->input);
    ttk_windows->w->input = 0;
}
