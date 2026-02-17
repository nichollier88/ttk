/*
 * Copyright (c) 2005 Joshua Oreman
 * Refactored to C++ class
 */

#pragma once

#include "TWidget.hpp
#include "ttk.hpp"

class TWindow {
public:
    const char* title;
    TWidgetList* widgets;
    int x, y, w, h, color;
    ttk_surface srf;
    int titlefree;
    int dirty;
    struct TApItem* background;
    /* readonly */ TWidget* focus;
    /* private */ TWidget* input;
    /* private */ int show_header;
    /* private */ int epoch;
    /* private */ int inbuf[32];  // circular buffer
    /* private */ int inbuf_start, inbuf_end;
    /* private */ int onscreen;
    int data;
    void* data2;

    TWindow();
    virtual ~TWindow();
};
