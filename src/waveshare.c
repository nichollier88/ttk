/*
 * src/waveshare.c
 *
 * TTK backend for Waveshare 1.44inch LCD HAT using Waveshare libraries.
 * Replaces sdl.c for this specific hardware.
 */

#include <SDL.h>
#include <SDL_image.h>
#ifndef NO_TF
#include <SDL_ttf.h>
#endif
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "ttk.h"
#include "ttk/SDL_gfxPrimitives.h"
#include "ttk/SDL_rotozoom.h"
#include "ttk/SFont.h"

#include "waveshare/DEV_Config.h"
#include "waveshare/LCD_1in44.h"
#include "waveshare/GUI_Paint.h"
#include "waveshare/GUI_BMP.h"

extern ttk_screeninfo* ttk_screen;

// SDL Additional flags (kept for compatibility with ttk_core)
typedef struct sdl_additional {
    Uint32 video_flags;
    Uint32 video_flags_mask;
} sdl_additional;

sdl_additional sdl_add = {0, 0};

// --- TTK Backend Implementation ---

void ttk_gfx_init() {
    if (DEV_ModuleInit() != 0) {
        fprintf(stderr, "DEV_ModuleInit failed\n");
        exit(1);
    }

    LCD_1in44_Init(SCAN_DIR_DFT);
    LCD_1in44_Clear(WHITE);

    // Init SDL for software surface (headless)
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    // Create a software surface for TTK to draw on (128x128, 16bpp RGB565)
    ttk_screen->srf = SDL_CreateRGBSurface(SDL_SWSURFACE, 128, 128, 16, 0xF800,
                                           0x07E0, 0x001F, 0);
    if (!ttk_screen->srf) {
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        exit(1);
    }

    // Force TTK screen info to match HAT
    ttk_screen->w = 128;
    ttk_screen->h = 128;
    ttk_screen->bpp = 2;

    SDL_EnableUNICODE(1);
}

void ttk_gfx_quit() {
    DEV_ModuleExit();
    SDL_Quit();
}

void ttk_gfx_update(ttk_surface srf) {
    if (!srf) return;

    if (SDL_MUSTLOCK(srf)) SDL_LockSurface(srf);

    uint16_t* pixels = (uint16_t*)srf->pixels;
    int count = 128 * 128;

    // Buffer for SPI transfer (swap bytes for big-endian display)
    static uint8_t buffer[128 * 128 * 2];
    // printf("\033[H");
    for (int i = 0; i < count; i++) {
        uint16_t p = pixels[i];
        buffer[i * 2] = (p >> 8) & 0xFF;
        buffer[i * 2 + 1] = p & 0xFF;

        // uint8_t r = (p >> 11) & 0x1F;
        // uint8_t g = (p >> 5) & 0x3F;
        // uint8_t b = p & 0x1F;

        // r = (r << 3) | (r >> 2);
        // g = (g << 2) | (g >> 4);
        // b = (b << 3) | (b >> 2);

        // printf("\033[48;2;%d;%d;%dm  ", r, g, b);
        // if ((i + 1) % 128 == 0) printf("\033[0m\n");
    }

    // fflush(stdout);
    
    LCD_1in44_Display((UWORD*)pixels);

    if (SDL_MUSTLOCK(srf)) SDL_UnlockSurface(srf);
}

int ttk_get_rawevent(int* arg) { return TTK_NO_EVENT; }

int ttk_get_event(int* arg) {
    static uint32_t last_time = 0;
    uint32_t current_time = SDL_GetTicks();

    // Simple debounce/rate limit
    if (current_time - last_time < 20) return TTK_NO_EVENT;
    last_time = current_time;

    *arg = 0;

    // State tracking for button up/down events
    static int button_states[128] = {0};
    // Using macros from DEV_Config.h
    int val;
    int ttk_btns[] = {TTK_BUTTON_MENU,
                      TTK_BUTTON_PLAY,
                      TTK_BUTTON_PREVIOUS,
                      TTK_BUTTON_NEXT,
                      TTK_BUTTON_ACTION,
                      '1',
                      '2',
                      '3'};

    for (int i = 0; i < 8; i++) {
        switch(i) {
            case 0: val = GET_KEY_UP; break;
            case 1: val = GET_KEY_DOWN; break;
            case 2: val = GET_KEY_LEFT; break;
            case 3: val = GET_KEY_RIGHT; break;
            case 4: val = GET_KEY_PRESS; break;
            case 5: val = GET_KEY1; break;
            case 6: val = GET_KEY2; break;
            case 7: val = GET_KEY3; break;
            default: val = 1; break;
        }
        
        int ttk_btn = ttk_btns[i];

        if (val == 0 && button_states[ttk_btn] == 0) {
            button_states[ttk_btn] = 1;
            *arg = ttk_btn;
            return TTK_BUTTON_DOWN;
        }
        if (val == 1 && button_states[ttk_btn] == 1) {
            button_states[ttk_btn] = 0;
            *arg = ttk_btn;
            return TTK_BUTTON_UP;
        }
    }

    return TTK_NO_EVENT;
}

int ttk_getticks() { return SDL_GetTicks(); }
void ttk_delay(int ms) { SDL_Delay(ms); }

// --- Drawing Primitives (Wrappers for SDL_gfx) ---

#ifndef MAX
#define MAX(x, y) (((x) > (y)) ? (x) : (y))
#endif

ttk_color ttk_makecol(int r, int g, int b) {
    return ttk_makecol_ex(r, g, b, 0);
}

ttk_color ttk_makecol_ex(int r, int g, int b, ttk_surface srf) {
    if (!srf) srf = ttk_screen->srf;
    return SDL_MapRGB(srf->format, r, g, b);
}

void ttk_unmakecol(ttk_color col, int* r, int* g, int* b) {
    ttk_unmakecol_ex(col, r, g, b, 0);
}

void ttk_unmakecol_ex(ttk_color col, int* r, int* g, int* b, ttk_surface srf) {
    if (!srf) srf = ttk_screen->srf;
    Uint8 R, G, B;
    SDL_GetRGB(col, srf->format, &R, &G, &B);
    *r = R;
    *g = G;
    *b = B;
}

ttk_gc ttk_new_gc() { return (ttk_gc)calloc(1, sizeof(struct _ttk_gc)); }
ttk_gc ttk_copy_gc(ttk_gc other) {
    ttk_gc ret = malloc(sizeof(struct _ttk_gc));
    memcpy(ret, other, sizeof(struct _ttk_gc));
    return ret;
}
ttk_color ttk_gc_get_foreground(ttk_gc gc) { return gc->fg; }
ttk_color ttk_gc_get_background(ttk_gc gc) { return gc->bg; }
ttk_font ttk_gc_get_font(ttk_gc gc) { return gc->font; }
void ttk_gc_set_foreground(ttk_gc gc, ttk_color fgcol) { gc->fg = fgcol; }
void ttk_gc_set_background(ttk_gc gc, ttk_color bgcol) { gc->bg = bgcol; }
void ttk_gc_set_font(ttk_gc gc, ttk_font font) { gc->font = font; }
void ttk_gc_set_usebg(ttk_gc gc, int flag) { gc->usebg = flag; }
void ttk_gc_set_xormode(ttk_gc gc, int flag) { gc->xormode = flag; }
void ttk_free_gc(ttk_gc gc) { free(gc); }

static int fetchcolor(ttk_color col) {
    int ret = 0;
    Uint8 r, g, b;
    SDL_GetRGB(col, ttk_screen->srf->format, &r, &g, &b);
    ret = (r << 24) | (g << 16) | (b << 8) | 0xff;
    return ret;
}

ttk_color ttk_getpixel(ttk_surface srf, int x, int y) {
    if (srf->format->BytesPerPixel == 2)
        return *((Uint16*)((Uint8*)srf->pixels + y * srf->pitch) + x);
    return 0;
}

static void SetupPaint(ttk_surface srf) {
    // Configure Paint to draw onto the SDL surface's buffer
    // Note: Paint_NewImage resets rotation and other properties.
    // We assume stride (WidthByte) matches width for 16bpp if pitch is width*2.
    // SDL pitch is in bytes.
    Paint_NewImage((UWORD*)srf->pixels, srf->w, srf->h, ROTATE_0, WHITE, 16);
    // Adjust stride if necessary (SDL pitch might include padding)
    Paint.WidthByte = srf->pitch / 2;
}

void ttk_pixel(ttk_surface srf, int x, int y, ttk_color col) {
    SetupPaint(srf);
    Paint_SetPixel(x, y, (UWORD)col);
}
void ttk_pixel_gc(ttk_surface srf, ttk_gc gc, int x, int y) {
    SetupPaint(srf);
    Paint_SetPixel(x, y, (UWORD)gc->fg);
}

void ttk_line(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    SetupPaint(srf);
    Paint_DrawLine(x1, y1, x2, y2, (UWORD)col, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}
void ttk_line_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) {
    SetupPaint(srf);
    Paint_DrawLine(x1, y1, x2, y2, (UWORD)gc->fg, DOT_PIXEL_1X1, LINE_STYLE_SOLID);
}

void ttk_aaline(ttk_surface srf, int x1, int y1, int x2, int y2,
                ttk_color col) {
    // GUI_Paint doesn't support AA lines, fallback to normal line
    ttk_line(srf, x1, y1, x2, y2, col);
}
void ttk_aaline_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) {
    ttk_line_gc(srf, gc, x1, y1, x2, y2);
}

void ttk_rect(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    SetupPaint(srf);
    Paint_DrawRectangle(x1, y1, x2 - 1, y2 - 1, (UWORD)col, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}
void ttk_rect_gc(ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) {
    SetupPaint(srf);
    Paint_DrawRectangle(x, y, x + w - 1, y + h - 1, (UWORD)gc->fg, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void ttk_fillrect(ttk_surface srf, int x1, int y1, int x2, int y2,
                  ttk_color col) {
    SetupPaint(srf);
    Paint_DrawRectangle(x1, y1, x2 - 1, y2 - 1, (UWORD)col, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}
void ttk_fillrect_gc(ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) {
    SetupPaint(srf);
    Paint_DrawRectangle(x, y, x + w - 1, y + h - 1, (UWORD)gc->fg, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

extern unsigned char ttk_chamfering[][10];

void ttk_do_gradient(ttk_surface srf, char horiz, int b_rad, int e_rad, int x1,
                     int y1, int x2, int y2, ttk_color begin, ttk_color end) {
    gradient_node* gn = ttk_gradient_find_or_add(begin, end);
    int steps = horiz ? x2 - x1 : y2 - y1;
    int line, bc, ec, i;

    if (!gn) return;

    if (steps < 0) steps *= -1;

    if (horiz) {
        for (line = 0, i = steps; line < i && (b_rad || e_rad); line++, i--) {
            bc = (line < b_rad) ? ttk_chamfering[b_rad - 1][line] : 0;
            ec = (line < e_rad) ? ttk_chamfering[e_rad - 1][line] : 0;
            if (bc == 0 && ec == 0) break;
            ttk_line(srf, x1 + line, y1 + bc, x1 + line, y2 - 1 - bc,
                     gn->gradient[(line * 256) / steps]);
            ttk_line(srf, x2 - 1 - line, y1 + ec, x2 - 1 - line, y2 - 1 - ec,
                     gn->gradient[((i - 1) * 256) / steps]);
        }
        for (; line < i; line++) {
            ttk_line(srf, x1 + line, y1, x1 + line, y2 - 1,
                     gn->gradient[(line * 256) / steps]);
        }
    } else {
        for (line = 0, i = steps; line < i && (b_rad || e_rad); line++, i--) {
            bc = (line < b_rad) ? ttk_chamfering[b_rad - 1][line] : 0;
            ec = (line < e_rad) ? ttk_chamfering[e_rad - 1][line] : 0;
            if (bc == 0 && ec == 0) break;
            ttk_line(srf, x1 + bc, y1 + line, x2 - 1 - bc, y1 + line,
                     gn->gradient[(line * 256) / steps]);
            ttk_line(srf, x1 + ec, y2 - 1 - line, x2 - 1 - ec, y2 - 1 - line,
                     gn->gradient[((i - 1) * 256) / steps]);
        }
        for (; line < i; line++)
            ttk_line(srf, x1, y1 + line, x2 - 1, y1 + line,
                     gn->gradient[(line * 256) / steps]);
    }
}

void ttk_hgradient(ttk_surface srf, int x1, int y1, int x2, int y2,
                   ttk_color left, ttk_color right) {
    ttk_do_gradient(srf, 1, 0, 0, x1, y1, x2, y2, left, right);
}

void ttk_vgradient(ttk_surface srf, int x1, int y1, int x2, int y2,
                   ttk_color top, ttk_color bottom) {
    ttk_do_gradient(srf, 0, 0, 0, x1, y1, x2, y2, top, bottom);
}

static void draw_bitmap(ttk_surface srf, int x, int y, int width, int height,
                        const unsigned short* imagebits, ttk_color color) {
    int minx, maxx;
    unsigned short bitvalue = 0;
    int bitcount;

    minx = x;
    maxx = x + width - 1;
    bitcount = 0;
    while (height > 0) {
        if (bitcount <= 0) {
            bitcount = 16;
            bitvalue = *imagebits++;
        }
        if (bitvalue & (1 << 15)) Paint_SetPixel(x, y, (UWORD)color);
        bitvalue <<= 1;
        bitcount--;

        if (x++ == maxx) {
            x = minx;
            y++;
            --height;
            bitcount = 0;
        }
    }
}

void ttk_bitmap(ttk_surface srf, int x, int y, int w, int h,
                unsigned short* imagebits, ttk_color col) {
    SetupPaint(srf);
    draw_bitmap(srf, x, y, w, h, imagebits, col);
}
void ttk_bitmap_gc(ttk_surface srf, ttk_gc gc, int x, int y, int w, int h,
                   unsigned short* imagebits) {
    SetupPaint(srf);
    draw_bitmap(srf, x, y, w, h, imagebits, gc->fg);
}

void ttk_poly(ttk_surface srf, int nv, short* vx, short* vy, ttk_color col) {
    polygonColor(srf, (Sint16*)vx, (Sint16*)vy, nv, fetchcolor(col));
}
void ttk_poly_pt(ttk_surface srf, ttk_point* v, int n, ttk_color col) {
    int i;
    short *vx = malloc(n * sizeof(short)), *vy = malloc(n * sizeof(short));
    if (!vx || !vy) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (i = 0; i < n; i++) {
        vx[i] = v[i].x;
        vy[i] = v[i].y;
    }

    polygonColor(srf, vx, vy, n, fetchcolor(col));

    free(vx);
    free(vy);
}
void ttk_poly_gc(ttk_surface srf, ttk_gc gc, int n, ttk_point* v) {
    ttk_poly_pt(srf, v, n, gc->fg);
}

void ttk_aapoly(ttk_surface srf, int nv, short* vx, short* vy, ttk_color col) {
    aapolygonColor(srf, (Sint16*)vx, (Sint16*)vy, nv, fetchcolor(col));
}
void ttk_aapoly_pt(ttk_surface srf, ttk_point* v, int n, ttk_color col) {
    int i;
    short *vx = malloc(n * sizeof(short)), *vy = malloc(n * sizeof(short));
    if (!vx || !vy) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (i = 0; i < n; i++) {
        vx[i] = v[i].x;
        vy[i] = v[i].y;
    }

    aapolygonColor(srf, vx, vy, n, fetchcolor(col));

    free(vx);
    free(vy);
}
void ttk_aapoly_gc(ttk_surface srf, ttk_gc gc, int n, ttk_point* v) {
    ttk_aapoly_pt(srf, v, n, gc->fg);
}

void ttk_polyline(ttk_surface srf, int nv, short* vx, short* vy,
                  ttk_color col) {
    polylineColor(srf, (Sint16*)vx, (Sint16*)vy, nv, fetchcolor(col));
}
void ttk_polyline_pt(ttk_surface srf, ttk_point* v, int n, ttk_color col) {
    int i;
    short *vx = malloc(n * sizeof(short)), *vy = malloc(n * sizeof(short));
    if (!vx || !vy) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (i = 0; i < n; i++) {
        vx[i] = v[i].x;
        vy[i] = v[i].y;
    }

    polylineColor(srf, vx, vy, n, fetchcolor(col));

    free(vx);
    free(vy);
}
void ttk_polyline_gc(ttk_surface srf, ttk_gc gc, int n, ttk_point* v) {
    ttk_polyline_pt(srf, v, n, gc->fg);
}

#define DO_BEZIER(function)                                                   \
    /*  Note: I don't think there is any great performance win in             \
     *  translating this to fixed-point integer math, most of the time        \
     *  is spent in the line drawing routine. */                              \
    float x = (float)x1, y = (float)y1;                                       \
    float xp = x, yp = y;                                                     \
    float delta;                                                              \
    float dx, d2x, d3x;                                                       \
    float dy, d2y, d3y;                                                       \
    float a, b, c;                                                            \
    int i;                                                                    \
    int n = 1;                                                                \
    Sint16 xmax = x1, ymax = y1, xmin = x1, ymin = y1;                        \
                                                                              \
    /* compute number of iterations */                                        \
    if (level < 1) level = 1;                                                 \
    if (level >= 15) level = 15;                                              \
    while (level-- > 0) n *= 2;                                               \
    delta = (float)(1.0 / (float)n);                                          \
                                                                              \
    /* compute finite differences                                             \
     * a, b, c are the coefficient of the polynom in t defining the           \
     * parametric curve. The computation is done independently for x and y */ \
    a = (float)(-x1 + 3 * x2 - 3 * x3 + x4);                                  \
    b = (float)(3 * x1 - 6 * x2 + 3 * x3);                                    \
    c = (float)(-3 * x1 + 3 * x2);                                            \
                                                                              \
    d3x = 6 * a * delta * delta * delta;                                      \
    d2x = d3x + 2 * b * delta * delta;                                        \
    dx = a * delta * delta * delta + b * delta * delta + c * delta;           \
                                                                              \
    a = (float)(-y1 + 3 * y2 - 3 * y3 + y4);                                  \
    b = (float)(3 * y1 - 6 * y2 + 3 * y3);                                    \
    c = (float)(-3 * y1 + 3 * y2);                                            \
                                                                              \
    d3y = 6 * a * delta * delta * delta;                                      \
    d2y = d3y + 2 * b * delta * delta;                                        \
    dy = a * delta * delta * delta + b * delta * delta + c * delta;           \
                                                                              \
    /* iterate */                                                             \
    for (i = 0; i < n; i++) {                                                 \
        x += dx;                                                              \
        dx += d2x;                                                            \
        d2x += d3x;                                                           \
        y += dy;                                                              \
        dy += d2y;                                                            \
        d2y += d3y;                                                           \
        if ((Sint16)xp != (Sint16)x || (Sint16)yp != (Sint16)y) {             \
            function;                                                         \
            xmax = (xmax > (Sint16)xp) ? xmax : (Sint16)xp;                   \
            ymax = (ymax > (Sint16)yp) ? ymax : (Sint16)yp;                   \
            xmin = (xmin < (Sint16)xp) ? xmin : (Sint16)xp;                   \
            ymin = (ymin < (Sint16)yp) ? ymin : (Sint16)yp;                   \
            xmax = (xmax > (Sint16)x) ? xmax : (Sint16)x;                     \
            ymax = (ymax > (Sint16)y) ? ymax : (Sint16)y;                     \
            xmin = (xmin < (Sint16)x) ? xmin : (Sint16)x;                     \
            ymin = (ymin < (Sint16)y) ? ymin : (Sint16)y;                     \
        }                                                                     \
        xp = x;                                                               \
        yp = y;                                                               \
    }

void ttk_bezier(ttk_surface srf, int x1, int y1, int x2, int y2, int x3, int y3,
                int x4, int y4, int level, ttk_color col) {
    DO_BEZIER(ttk_line(srf, (short)xp, (short)yp, (short)x, (short)y, col));
}
void ttk_bezier_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
                   int x3, int y3, int x4, int y4, int level) {
    ttk_bezier(srf, x1, y1, x2, y2, x3, y3, x4, y4, level, gc->fg);
}

void ttk_aabezier(ttk_surface srf, int x1, int y1, int x2, int y2, int x3,
                  int y3, int x4, int y4, int level, ttk_color col) {
    DO_BEZIER(ttk_aaline(srf, (short)xp, (short)yp, (short)x, (short)y, col));
}
void ttk_aabezier_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2,
                     int x3, int y3, int x4, int y4, int level) {
    ttk_aabezier(srf, x1, y1, x2, y2, x3, y3, x4, y4, level, gc->fg);
}

void ttk_fillpoly(ttk_surface srf, int nv, short* vx, short* vy,
                  ttk_color col) {
    filledPolygonColor(srf, (Sint16*)vx, (Sint16*)vy, nv, fetchcolor(col));
}
void ttk_fillpoly_pt(ttk_surface srf, ttk_point* v, int n, ttk_color col) {
    int i;
    short *vx = malloc(n * sizeof(short)), *vy = malloc(n * sizeof(short));
    if (!vx || !vy) {
        fprintf(stderr, "Out of memory\n");
        exit(1);
    }

    for (i = 0; i < n; i++) {
        vx[i] = v[i].x;
        vy[i] = v[i].y;
    }

    filledPolygonColor(srf, vx, vy, n, fetchcolor(col));

    free(vx);
    free(vy);
}
void ttk_fillpoly_gc(ttk_surface srf, ttk_gc gc, int n, ttk_point* v) {
    ttk_fillpoly_pt(srf, v, n, gc->fg);
}

void ttk_ellipse(ttk_surface srf, int x, int y, int rx, int ry, ttk_color col) {
    SetupPaint(srf);
    Paint_DrawCircle(x, y, rx, (UWORD)col, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}
void ttk_ellipse_gc(ttk_surface srf, ttk_gc gc, int x, int y, int rx, int ry) {
    SetupPaint(srf);
    Paint_DrawCircle(x, y, rx, (UWORD)gc->fg, DOT_PIXEL_1X1, DRAW_FILL_EMPTY);
}

void ttk_aaellipse(ttk_surface srf, int x, int y, int rx, int ry,
                   ttk_color col) {
    // Fallback to normal ellipse
    ttk_ellipse(srf, x, y, rx, ry, col);
}
void ttk_aaellipse_gc(ttk_surface srf, ttk_gc gc, int x, int y, int rx,
                      int ry) {
    ttk_ellipse_gc(srf, gc, x, y, rx, ry);
}

void ttk_fillellipse(ttk_surface srf, int x, int y, int rx, int ry,
                     ttk_color col) {
    SetupPaint(srf);
    Paint_DrawCircle(x, y, rx, (UWORD)col, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}
void ttk_fillellipse_gc(ttk_surface srf, ttk_gc gc, int x, int y, int rx,
                        int ry) {
    SetupPaint(srf);
    Paint_DrawCircle(x, y, rx, (UWORD)gc->fg, DOT_PIXEL_1X1, DRAW_FILL_FULL);
}

void ttk_aafillellipse(ttk_surface srf, int xc, int yc, int rx, int ry,
                       ttk_color col) {
    // Stub or simple implementation for now to satisfy linker if needed
    // Using filled ellipse as fallback
    ttk_fillellipse(srf, xc, yc, rx, ry, col);
}

void ttk_aafillellipse_gc(ttk_surface srf, ttk_gc gc, int xc, int yc, int rx,
                          int ry) {
    ttk_aafillellipse(srf, xc, yc, rx, ry, gc->fg);
}

typedef struct Bitmap_Font {
    char* name;
    int maxwidth;
    unsigned int height;
    int ascent;
    int firstchar;
    int size;
    const unsigned short* bits;
    const unsigned long* offset;
    const unsigned char* width;
    int defaultchar;
    long bits_size;
} Bitmap_Font;

static int read8(FILE* fp, unsigned char* cp) {
    int c;
    if ((c = getc(fp)) == EOF) return 0;
    *cp = (unsigned char)c;
    return 1;
}
static int read16(FILE* fp, unsigned short* sp) {
    int c;
    unsigned short s = 0;
    if ((c = getc(fp)) == EOF) return 0;
    s |= (c & 0xff);
    if ((c = getc(fp)) == EOF) return 0;
    s |= (c & 0xff) << 8;
    *sp = s;
    return 1;
}
static int read32(FILE* fp, unsigned long* lp) {
    int c;
    unsigned long l = 0;
    if ((c = getc(fp)) == EOF) return 0;
    l |= (c & 0xff);
    if ((c = getc(fp)) == EOF) return 0;
    l |= (c & 0xff) << 8;
    if ((c = getc(fp)) == EOF) return 0;
    l |= (c & 0xff) << 16;
    if ((c = getc(fp)) == EOF) return 0;
    l |= (c & 0xff) << 24;
    *lp = l;
    return 1;
}
static int readstr(FILE* fp, char* buf, int count) {
    return fread(buf, 1, count, fp);
}
static int readstr_padded(FILE* fp, char* buf, int totlen) {
    char* p;
    if (fread(buf, 1, totlen, fp) != totlen) return 0;

    p = &buf[totlen];
    *p-- = 0;
    while (*p == ' ' && p >= buf) *p-- = 0;

    return totlen;
}

#define FNT_VERSION "RB11"
static void load_fnt(Bitmap_Font* bf, const char* fname) {
    FILE* fp = fopen(fname, "rb");
    int i;
    unsigned short maxwidth, height, ascent, pad;
    unsigned long firstchar, defaultchar, size;
    unsigned long nbits, noffset, nwidth;
    char version[5];
    char name[65];
    char copyright[257];

    if (!fp) {
        fprintf(stderr, "Couldn't find font file %s; exiting.\n", fname);
        exit(1);
    }

    memset(version, 0, sizeof(version));
    if (readstr(fp, version, 4) != 4) goto errout;
    if (strcmp(version, FNT_VERSION) != 0) goto errout;

    if (readstr_padded(fp, name, 64) != 64) goto errout;
    bf->name = (char*)malloc(strlen(name) + 1);
    if (!bf->name) goto errout;
    strcpy(bf->name, name);

    if (readstr_padded(fp, copyright, 256) != 256) goto errout;

    if (!read16(fp, &maxwidth)) goto errout;
    bf->maxwidth = maxwidth;
    if (!read16(fp, &height)) goto errout;
    bf->height = height;
    if (!read16(fp, &ascent)) goto errout;
    bf->ascent = ascent;
    if (!read16(fp, &pad)) goto errout;
    if (!read32(fp, &firstchar)) goto errout;
    bf->firstchar = firstchar;
    if (!read32(fp, &defaultchar)) goto errout;
    bf->defaultchar = defaultchar;
    if (!read32(fp, &size)) goto errout;
    bf->size = size;

    if (!read32(fp, &nbits)) goto errout;
    bf->bits = (unsigned short*)malloc(nbits * sizeof(unsigned short));
    if (!bf->bits) goto errout;
    bf->bits_size = nbits;

    if (!read32(fp, &noffset)) goto errout;
    if (noffset) {
        bf->offset = (unsigned long*)malloc(noffset * sizeof(unsigned long));
        if (!bf->offset) goto errout;
    }

    if (!read32(fp, &nwidth)) goto errout;
    if (nwidth) {
        bf->width = (unsigned char*)malloc(nwidth * sizeof(unsigned char));
        if (!bf->width) goto errout;
    }

    for (i = 0; i < nbits; ++i)
        if (!read16(fp, (unsigned short*)&bf->bits[i])) goto errout;
    if (ftell(fp) & 02)
        if (!read16(fp, (unsigned short*)&bf->bits[i])) goto errout;
    if (noffset)
        for (i = 0; i < bf->size; ++i)
            if (!read32(fp, (unsigned long*)&bf->offset[i])) goto errout;
    if (nwidth)
        for (i = 0; i < bf->size; ++i)
            if (!read8(fp, (unsigned char*)&bf->width[i])) goto errout;

    fclose(fp);
    return;

errout:
    fclose(fp);
    if (!bf) return;
    if (bf->name) free(bf->name);
    if (bf->bits) free((char*)bf->bits);
    if (bf->offset) free((char*)bf->offset);
    if (bf->width) free((char*)bf->width);
    fprintf(stderr, "Error in font file %s - possibly truncated.\n", fname);
    exit(1);
}

static void gen_gettextsize(Bitmap_Font* bf, const void* text, int cc,
                            int* pwidth, int* pheight, int* pbase) {
    const unsigned char* str = text;
    unsigned int c;
    int width;

    if (bf->width == NULL)
        width = cc * bf->maxwidth;
    else {
        width = 0;
        while (--cc >= 0) {
            c = *str++;
            if (c >= bf->firstchar && c < bf->firstchar + bf->size)
                width += bf->width[c - bf->firstchar];
        }
    }
    *pwidth = width;
    *pheight = bf->height;
    *pbase = bf->ascent;
}

static void gen16_gettextsize(Bitmap_Font* bf, const unsigned short* str,
                              int cc, int* pwidth, int* pheight, int* pbase) {
    unsigned int c;
    int width;

    if (bf->width == NULL)
        width = cc * bf->maxwidth;
    else {
        width = 0;
        while (--cc >= 0) {
            c = *str++;
            if (c >= bf->firstchar && c < bf->firstchar + bf->size)
                width += bf->width[c - bf->firstchar];
        }
    }
    *pwidth = width;
    *pheight = bf->height;
    *pbase = bf->ascent;
}

static void gen_gettextbits(Bitmap_Font* bf, int ch,
                            const unsigned short** retmap, int* pwidth,
                            int* pheight, int* pbase) {
    int count, width;
    const unsigned short* bits;

    if (ch < bf->firstchar || ch >= bf->firstchar + bf->size)
        ch = bf->firstchar;

    ch -= bf->firstchar;

    if (bf->offset) {
        if (((unsigned long*)bf->offset)[0] >= 0x00010000)
            bits = bf->bits + ((unsigned short*)bf->offset)[ch];
        else
            bits = bf->bits + ((unsigned long*)bf->offset)[ch];
    } else
        bits = bf->bits + (bf->height * ch);

    width = bf->width ? bf->width[ch] : bf->maxwidth;
    count = ((width + 15) / 16) * bf->height;

    *retmap = bits;

    *pwidth = width;
    *pheight = bf->height;
    *pbase = bf->ascent;
}

static void corefont_drawtext(Bitmap_Font* bf, ttk_surface srf, int x, int y,
                              const void* text, int cc, ttk_color col) {
    const unsigned char* str = text;
    int width, height, base, startx, starty;
    const unsigned short* bitmap;

    startx = x;
    starty = y;

    while (--cc >= 0 && x < srf->w) {
        int ch = *str++;
        gen_gettextbits(bf, ch, &bitmap, &width, &height, &base);
        draw_bitmap(srf, x, y, width, height, bitmap, col);
        x += width;
    }
}

static void corefont16_drawtext(Bitmap_Font* bf, ttk_surface srf, int x, int y,
                                const unsigned short* str, int cc,
                                ttk_color col) {
    int width, height, base, startx, starty;
    const unsigned short* bitmap;

    startx = x;
    starty = y;

    while (--cc >= 0 && x < srf->w) {
        int ch = *str++;
        gen_gettextbits(bf, ch, &bitmap, &width, &height, &base);
        draw_bitmap(srf, x, y, width, height, bitmap, col);
        x += width;
    }
}

static int IsASCII(const char* str) {
    const char* p = str;
    while (*p) {
        if (*p & 0x80) return 0;
        p++;
    }
    return 1;
}

static int ConvertUTF8(const unsigned char* src, unsigned short* dst) {
    const unsigned char* sp = src;
    unsigned short* dp = dst;
    int len = 0;
    while (*sp) {
        *dp = 0;
        if (*sp < 0x80)
            *dp = *sp++;
        else if (*sp >= 0xC0 && *sp < 0xE0) {
            *dp |= (*sp++ - 0xC0) << 6;
            if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        } else if (*sp >= 0xE0 && *sp < 0xF0) {
            *dp |= (*sp++ - 0xE0) << 12;
            if (!*sp) goto err;
            *dp |= (*sp++ - 0x80) << 6;
            if (!*sp) goto err;
            *dp |= (*sp++ - 0x80);
        } else
            goto err;

        dp++;
        len++;
        continue;

    err:
        *dp++ = '?';
        sp++;
        len++;
    }
    *dp = 0;
    return len;
}

static void draw_bf(ttk_font f, ttk_surface srf, int x, int y, ttk_color col,
                    const char* str) {
    const void* text = (const void*)str;
    int cc = strlen(str);
    if (!f->bf) return;

    if (IsASCII(str))
        corefont_drawtext(f->bf, srf, x, y, text, cc, col);
    else {
        unsigned short* buf = malloc(strlen(str) * 2);
        int len = ConvertUTF8((unsigned char*)str, buf);
        corefont16_drawtext(f->bf, srf, x, y, buf, len, col);
        free(buf);
    }
}

static void lat1_bf(ttk_font f, ttk_surface srf, int x, int y, ttk_color col,
                    const char* str) {
    const void* text = (const void*)str;
    int cc = strlen(str);
    if (!f->bf) return;

    corefont_drawtext(f->bf, srf, x, y, text, strlen(str), col);
}

static void uc16_bf(ttk_font f, ttk_surface srf, int x, int y, ttk_color col,
                    const uc16* str) {
    int cc = 0;
    const uc16* p = str;
    if (!f->bf) return;

    while (*p++) cc++;
    corefont16_drawtext(f->bf, srf, x, y, str, cc, col);
}

static int width_bf(ttk_font f, const char* str) {
    int width, height, base;
    if (!f->bf) return -1;

    if (IsASCII(str))
        gen_gettextsize(f->bf, str, strlen(str), &width, &height, &base);
    else {
        unsigned short* buf = malloc(strlen(str) * 2);
        int len = ConvertUTF8((unsigned char*)str, buf);
        gen16_gettextsize(f->bf, buf, len, &width, &height, &base);
        free(buf);
    }
    return width;
}
static int widthL_bf(ttk_font f, const char* str) {
    int width, height, base;
    if (!f->bf) return -1;
    gen_gettextsize(f->bf, str, strlen(str), &width, &height, &base);
    return width;
}
static int widthU_bf(ttk_font f, const uc16* str) {
    int width, height, base;
    int cc = 0;
    const uc16* p = str;
    if (!f->bf) return -1;

    while (*p++) cc++;
    gen16_gettextsize(f->bf, str, cc, &width, &height, &base);
    return width;
}

static void free_bf(ttk_font f) {
    if (f->bf->name) free(f->bf->name);
    if (f->bf->bits) free((char*)f->bf->bits);
    if (f->bf->offset) free((char*)f->bf->offset);
    if (f->bf->width) free((char*)f->bf->width);
    f->bf = 0;
}

#ifndef NO_SF
static void draw_sf(ttk_font f, ttk_surface srf, int x, int y, ttk_color col,
                    const char* str) {
    Uint8 r, g, b;
    SDL_GetRGB(col, srf->format, &r, &g, &b);

    if (!f->sf) return;

    if ((r + g + b) > 600)
        SFont_Write(srf, f->sfi, x, y, str);
    else
        SFont_Write(srf, f->sf, x, y, str);
}
static void draw16_sf(ttk_font f, ttk_surface srf, int x, int y, ttk_color col,
                      const uc16* str) {
    int len = 0;
    const uc16* sp = str;
    char *dst, *dp;
    while (*sp++) len++;
    dp = dst = malloc(len);
    sp = str;
    while (*sp) *dp++ = (*sp++ & 0xff);
    *dp = 0;
    draw_sf(f, srf, x, y, col, dst);
    free(dst);
}
static int width_sf(ttk_font f, const char* str) {
    if (f->sf) return SFont_TextWidth(f->sf, str);
    return 0;
}
static int width16_sf(ttk_font f, const uc16* str) {
    int len = 0;
    const uc16* sp = str;
    char *dst, *dp;
    int ret;

    while (*sp++) len++;
    dp = dst = malloc(len);
    sp = str;
    while (*sp) *dp++ = (*sp++ & 0xff);
    *dp = 0;

    ret = width_sf(f, dst);
    free(dst);
    return ret;
}
static void free_sf(ttk_font f) {
    SFont_FreeFont(f->sf);
    SFont_FreeFont(f->sfi);
    f->sf = 0;
}
#endif

#ifndef NO_TF
#define TF_FUNC(name, type, func)                                    \
    static void name##_tf(ttk_font f, ttk_surface srf, int x, int y, \
                          ttk_color col, const type* str) {          \
        SDL_Surface* textsrf;                                        \
        SDL_Rect dr;                                                 \
                                                                     \
        textsrf = TTF_Render##func##_Blended(f->tf, str, col);       \
                                                                     \
        dr.x = x;                                                    \
        dr.y = y;                                                    \
        SDL_BlitSurface(textsrf, 0, srf, &dr);                       \
    }
TF_FUNC(draw, char, UTF8);
TF_FUNC(lat1, char, Text);
TF_FUNC(uc16, Uint16, UNICODE);

static int width_tf(ttk_font f, const char* str) {
    int w, h;
    TTF_SizeUTF8(f->tf, str, &w, &h);
    return w;
}
static int widthL_tf(ttk_font f, const char* str) {
    int w, h;
    TTF_SizeText(f->tf, str, &w, &h);
    return w;
}
static int widthU_tf(ttk_font f, const uc16* str) {
    int w, h;
    TTF_SizeUNICODE(f->tf, str, &w, &h);
    return w;
}
static void free_tf(ttk_font f) { TTF_CloseFont(f->tf); }
#endif

void ttk_load_font(ttk_fontinfo* fi, const char* fnbase, int size) {
    char* fname = alloca(strlen(fnbase) + 7); /* +7: - i . p n g \0 */
    struct stat st;

    fi->f = calloc(1, sizeof(struct _ttk_font));

#ifndef NO_SF
    strcpy(fname, fnbase);
    strcat(fname, ".png");
    if (stat(fname, &st) >= 0) {
        fi->f->sf = SFont_InitFont(IMG_Load(fname));
        strcpy(fname, fnbase);
        strcat(fname, "-i.png");
        fi->f->sfi = SFont_InitFont(IMG_Load(fname));
        fi->f->draw = fi->f->draw_lat1 = draw_sf;
        fi->f->draw_uc16 = draw16_sf;
        fi->f->width = fi->f->width_lat1 = width_sf;
        fi->f->width_uc16 = width16_sf;
        fi->f->free = free_sf;
        fi->f->height = SFont_TextHeight(fi->f->sf);
        return;
    }
#endif

#ifndef NO_TF
    strcpy(fname, fnbase);
    strcat(fname, ".ttf");
    if (stat(fname, &st) >= 0) {
        fi->f->tf = TTF_OpenFont(fname, size);
        fi->f->draw = draw_tf;
        fi->f->draw_lat1 = lat1_tf;
        fi->f->draw_uc16 = uc16_tf;
        fi->f->width = width_tf;
        fi->f->width_lat1 = widthL_tf;
        fi->f->width_uc16 = widthU_tf;
        fi->f->free = free_tf;
        fi->f->height = TTF_FontHeight(fi->f->tf);
        return;
    }
#endif

    strcpy(fname, fnbase);
    strcat(fname, ".fnt");
    if (stat(fname, &st) >= 0) {
        fi->f->bf = calloc(1, sizeof(Bitmap_Font));
        load_fnt(fi->f->bf, fname);
        fi->f->draw = draw_bf;
        fi->f->draw_lat1 = lat1_bf;
        fi->f->draw_uc16 = uc16_bf;
        fi->f->width = width_bf;
        fi->f->width_lat1 = widthL_bf;
        fi->f->width_uc16 = widthU_bf;
        fi->f->free = free_bf;
        fi->f->height = fi->f->bf->height;
        return;
    }

    free(fi->f);
    fi->good = 0;
    return;
}
void ttk_unload_font(ttk_fontinfo* fi) {
    fi->f->free(fi->f);
    fi->loaded = 0;
    fi->good = 0;
}

void ttk_text(ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col,
              const char* str) {
    fnt->draw(fnt, srf, x, y + fnt->ofs, col, str);
}
void ttk_text_lat1(ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col,
                   const char* str) {
    fnt->draw_lat1(fnt, srf, x, y + fnt->ofs, col, str);
}
void ttk_text_uc16(ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col,
                   const uc16* str) {
    fnt->draw_uc16(fnt, srf, x, y + fnt->ofs, col, str);
}
int ttk_text_width(ttk_font fnt, const char* str) {
    if (!str) return 0;
    return fnt->width(fnt, str);
}
int ttk_text_width_lat1(ttk_font fnt, const char* str) {
    if (!str) return 0;
    return fnt->width_lat1(fnt, str);
}
int ttk_text_width_uc16(ttk_font fnt, const uc16* str) {
    if (!str) return 0;
    return fnt->width_uc16(fnt, str);
}
int ttk_text_width_gc(ttk_gc gc, const char* str) {
    return gc->font->width(gc->font, str);
}
int ttk_text_height(ttk_font fnt) { return fnt->height; }
int ttk_text_height_gc(ttk_gc gc) { return gc->font->height; }
void ttk_textf(ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col,
               const char* fmt, ...) {
    static char* buffer;
    va_list ap;

    if (!buffer) buffer = malloc(4096);

    va_start(ap, fmt);
    vsnprintf(buffer, 4096, fmt, ap);
    va_end(ap);
    fnt->draw(fnt, srf, x, y + fnt->ofs, col, buffer);
}

void ttk_text_gc(ttk_surface srf, ttk_gc gc, int x, int y, const char* str) {
    if (gc->usebg) {
        ttk_fillrect(srf, x, y, x + ttk_text_width_gc(gc, str),
                     y + ttk_text_height_gc(gc), gc->bg);
    }
    gc->font->draw(gc->font, srf, x, y, gc->fg, str);
}

ttk_surface ttk_load_image(const char* path) { return IMG_Load(path); }
void ttk_free_image(ttk_surface img) { SDL_FreeSurface(img); }
void ttk_blit_image(ttk_surface src, ttk_surface dst, int dx, int dy) {
    SDL_Rect dr = {dx, dy, 0, 0};
    SDL_BlitSurface(src, 0, dst, &dr);
}
void ttk_blit_image_ex(ttk_surface src, int sx, int sy, int sw, int sh,
                       ttk_surface dst, int dx, int dy) {
    SDL_Rect sr = {sx, sy, sw, sh};
    SDL_Rect dr = {dx, dy, 0, 0};
    SDL_BlitSurface(src, &sr, dst, &dr);
}

ttk_surface ttk_new_surface(int w, int h, int bpp) {
    return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F,
                                0);
}
void ttk_free_surface(ttk_surface srf) { SDL_FreeSurface(srf); }
void ttk_surface_get_dimen(ttk_surface srf, int* w, int* h) {
    if (w) *w = srf->w;
    if (h) *h = srf->h;
}

ttk_surface ttk_scale_surface(ttk_surface srf, float factor) {
    return zoomSurface(srf, factor, factor, 1);
}
