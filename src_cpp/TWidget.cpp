#include "TWidget.hpp"

TWidget::TWidget(int x, int y) : x(x), y(y) {
    w = 0;
    h = 0;
    focusable = 0;
    keyrepeat = 0;
    rawkeys = 0;
    framelast = 0;
    framedelay = 0;
    timerlast = 0;
    timerdelay = 0;
    holdtime = 1000;
    dirty = 1;
    win = 0;
    data = 0;
    data2 = 0;
}

TWidget::~TWidget() {}

void TWidget::draw(ttk_surface s) {}
int TWidget::scroll(int) { return TTK_EV_UNUSED; }
int TWidget::stap(int) { return TTK_EV_UNUSED; }
int TWidget::button(int, int) { return TTK_EV_UNUSED; }
int TWidget::down(int) { return TTK_EV_UNUSED; }
int TWidget::held(int) { return TTK_EV_UNUSED; }
int TWidget::input(int) { return TTK_EV_UNUSED; }
int TWidget::frame() { return 0; }
int TWidget::timer() { return 0; }
