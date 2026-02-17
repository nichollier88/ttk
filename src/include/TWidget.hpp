/*
 * Copyright (c) 2005 Joshua Oreman
 * Refactored to C++ class
 */

#pragma once

#include "ttk.h"

class TWidget {
public:
    int x, y, w, h;
    int focusable;
    int dirty;
    int holdtime;
    int keyrepeat;
    int rawkeys;
    /* readonly */ TWindow* win;

    // private-ish
    int framelast, framedelay;
    int timerlast, timerdelay;

    void* data;
    void* data2;

    TWidget(int x, int y);
    virtual ~TWidget();

    virtual void draw(ttk_surface s);
    virtual int scroll(int);
    virtual int stap(int);
    virtual int button(int, int);
    virtual int down(int);
    virtual int held(int);
    virtual int input(int);
    virtual int frame();
    virtual int timer();
};

typedef struct TWidgetList {
    TWidget* v;
    struct TWidgetList* next;
} TWidgetList;
