/*
 * src/waveshare.c
 *
 * TTK backend for Waveshare 1.44inch LCD HAT using bcm2835 library.
 * Replaces sdl.c for this specific hardware.
 */

#include <lgpio.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <sys/stat.h>
#include <SDL.h>

#include "ttk.h"
#include "ttk/SDL_gfxPrimitives.h"

// Waveshare 1.44inch LCD HAT GPIOs (BCM)
#define LCD_CS      8
#define LCD_RST     27
#define LCD_DC      25
#define LCD_BL      24

#define KEY_UP      6
#define KEY_DOWN    19
#define KEY_LEFT    5
#define KEY_RIGHT   26
#define KEY_PRESS   13
#define KEY1        21
#define KEY2        20
#define KEY3        16

// ST7735S commands
#define ST7735_SWRESET 0x01
#define ST7735_SLPOUT  0x11
#define ST7735_DISPON  0x29
#define ST7735_CASET   0x2A
#define ST7735_RASET   0x2B
#define ST7735_RAMWR   0x2C
#define ST7735_MADCTL  0x36
#define ST7735_COLMOD  0x3A

extern ttk_screeninfo* ttk_screen;

// SDL Additional flags (kept for compatibility with ttk_core)
typedef struct sdl_additional {
    Uint32 video_flags;
    Uint32 video_flags_mask;
} sdl_additional;

sdl_additional sdl_add = {0, 0};

int GPIO_Handle;
int SPI_Handle;

// --- Hardware Interface ---

static void LCD_Write_Command(uint8_t cmd) {
    lgGpioWrite(GPIO_Handle, LCD_DC, 0);
    lgSpiWrite(SPI_Handle, (char*)&cmd, 1);
}

static void LCD_Write_Data(uint8_t data) {
    lgGpioWrite(GPIO_Handle, LCD_DC, 1);
    lgSpiWrite(SPI_Handle, (char*)&data, 1);
}

static void LCD_Init_Reg(void) {
    // ST7735S initialization sequence
    LCD_Write_Command(ST7735_SWRESET);
    lguSleep(0.120);

    LCD_Write_Command(ST7735_SLPOUT);
    lguSleep(0.120);

    LCD_Write_Command(0xB1); // Frame Rate Control 1
    LCD_Write_Data(0x01);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x2D);

    LCD_Write_Command(0xB2); // Frame Rate Control 2
    LCD_Write_Data(0x01);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x2D);

    LCD_Write_Command(0xB3); // Frame Rate Control 3
    LCD_Write_Data(0x01);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x2D);
    LCD_Write_Data(0x01);
    LCD_Write_Data(0x2C);
    LCD_Write_Data(0x2D);

    LCD_Write_Command(0xB4); // Display Inversion Control
    LCD_Write_Data(0x07);

    LCD_Write_Command(0xC0); // Power Control 1
    LCD_Write_Data(0xA2);
    LCD_Write_Data(0x02);
    LCD_Write_Data(0x84);

    LCD_Write_Command(0xC1); // Power Control 2
    LCD_Write_Data(0xC5);

    LCD_Write_Command(0xC2); // Power Control 3
    LCD_Write_Data(0x0A);
    LCD_Write_Data(0x00);

    LCD_Write_Command(0xC3); // Power Control 4
    LCD_Write_Data(0x8A);
    LCD_Write_Data(0x2A);

    LCD_Write_Command(0xC4); // Power Control 5
    LCD_Write_Data(0x8A);
    LCD_Write_Data(0xEE);

    LCD_Write_Command(0xC5); // VCOM Control 1
    LCD_Write_Data(0x0E);

    LCD_Write_Command(0x20); // Display Inversion Off

    LCD_Write_Command(ST7735_MADCTL); // Memory Data Access Control
    LCD_Write_Data(0xC8); // BGR, MX, MY

    LCD_Write_Command(ST7735_COLMOD); // Interface Pixel Format
    LCD_Write_Data(0x05); // 16-bit/pixel

    LCD_Write_Command(ST7735_DISPON); // Display On
    lguSleep(0.100);
}

static void LCD_SetWindow(uint8_t x0, uint8_t y0, uint8_t x1, uint8_t y1) {
    // Adjust for 1.44 inch LCD offsets (128x128 centered in 132x162)
    uint8_t x_off = 2;
    uint8_t y_off = 1;

    LCD_Write_Command(ST7735_CASET);
    LCD_Write_Data(0x00);
    LCD_Write_Data(x0 + x_off);
    LCD_Write_Data(0x00);
    LCD_Write_Data(x1 + x_off);

    LCD_Write_Command(ST7735_RASET);
    LCD_Write_Data(0x00);
    LCD_Write_Data(y0 + y_off);
    LCD_Write_Data(0x00);
    LCD_Write_Data(y1 + y_off);

    LCD_Write_Command(ST7735_RAMWR);
}

// --- TTK Backend Implementation ---

void ttk_gfx_init() {
    char buffer[128];
    FILE *fp;

    fp = popen("cat /proc/cpuinfo | grep 'Raspberry Pi 5'", "r");
    if (fp == NULL) {
        fprintf(stderr, "It is not possible to determine the model of the Raspberry PI\n");
        exit(1);
    }

    if(fgets(buffer, sizeof(buffer), fp) != NULL) {
        GPIO_Handle = lgGpiochipOpen(4);
        if (GPIO_Handle < 0) {
            fprintf(stderr, "gpiochip4 Export Failed\n");
            exit(1);
        }
    } else {
        GPIO_Handle = lgGpiochipOpen(0);
        if (GPIO_Handle < 0) {
            fprintf(stderr, "gpiochip0 Export Failed\n");
            exit(1);
        }
    }
    pclose(fp);

    // Init SPI
    SPI_Handle = lgSpiOpen(0, 0, 10000000, 0);
    if (SPI_Handle < 0) {
        fprintf(stderr, "lgSpiOpen Failed\n");
        exit(1);
    }

    // Init GPIOs
    lgGpioClaimOutput(GPIO_Handle, 0, LCD_RST, 0);
    lgGpioClaimOutput(GPIO_Handle, 0, LCD_DC, 0);
    lgGpioClaimOutput(GPIO_Handle, 0, LCD_BL, 0);

    int inputs[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PRESS, KEY1, KEY2, KEY3};
    for (int i = 0; i < 8; i++) {
        lgGpioClaimInput(GPIO_Handle, 0, inputs[i]);
    }

    // Reset LCD
    lgGpioWrite(GPIO_Handle, LCD_BL, 1);
    lgGpioWrite(GPIO_Handle, LCD_RST, 1);
    lguSleep(0.100);
    lgGpioWrite(GPIO_Handle, LCD_RST, 0);
    lguSleep(0.100);
    lgGpioWrite(GPIO_Handle, LCD_RST, 1);
    lguSleep(0.100);

    LCD_Init_Reg();

    // Init SDL for software surface (headless)
    if (SDL_Init(SDL_INIT_TIMER | SDL_INIT_NOPARACHUTE) < 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        exit(1);
    }

    // Create a software surface for TTK to draw on (128x128, 16bpp RGB565)
    ttk_screen->srf = SDL_CreateRGBSurface(SDL_SWSURFACE, 128, 128, 16, 0xF800, 0x07E0, 0x001F, 0);
    if (!ttk_screen->srf) {
        fprintf(stderr, "SDL_CreateRGBSurface failed: %s\n", SDL_GetError());
        exit(1);
    }
    
    // Force TTK screen info to match HAT
    ttk_screen->w = 128;
    ttk_screen->h = 128;
    ttk_screen->bpp = 16;
    
    SDL_EnableUNICODE(1);
}

void ttk_gfx_quit() {
    lgSpiClose(SPI_Handle);
    lgGpiochipClose(GPIO_Handle);
    SDL_Quit();
}

void ttk_gfx_update(ttk_surface srf) {
    if (!srf) return;

    if (SDL_MUSTLOCK(srf)) SDL_LockSurface(srf);

    LCD_SetWindow(0, 0, 127, 127);
    lgGpioWrite(GPIO_Handle, LCD_DC, 1);
    
    uint16_t *pixels = (uint16_t*)srf->pixels;
    int count = 128 * 128;
    
    // Buffer for SPI transfer (swap bytes for big-endian display)
    static uint8_t buffer[128 * 128 * 2];
    for (int i = 0; i < count; i++) {
        uint16_t p = pixels[i];
        buffer[i*2] = (p >> 8) & 0xFF;
        buffer[i*2+1] = p & 0xFF;
    }
    
    lgSpiWrite(SPI_Handle, (char*)buffer, count * 2);

    if (SDL_MUSTLOCK(srf)) SDL_UnlockSurface(srf);
}

int ttk_get_rawevent(int* arg) {
    return TTK_NO_EVENT;
}

int ttk_get_event(int* arg) {
    static uint32_t last_time = 0;
    uint32_t current_time = SDL_GetTicks();
    
    // Simple debounce/rate limit
    if (current_time - last_time < 20) return TTK_NO_EVENT;
    last_time = current_time;

    *arg = 0;

    // State tracking for button up/down events
    static int button_states[128] = {0};
    int buttons[] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_PRESS, KEY1, KEY2, KEY3};
    int ttk_btns[] = {TTK_BUTTON_MENU, TTK_BUTTON_PLAY, TTK_BUTTON_PREVIOUS, TTK_BUTTON_NEXT, TTK_BUTTON_ACTION, '1', '2', '3'};
    
    for (int i = 0; i < 8; i++) {
        int pin = buttons[i];
        int val = lgGpioRead(GPIO_Handle, pin); // Active LOW
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
    *r = R; *g = G; *b = B;
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

void ttk_pixel(ttk_surface srf, int x, int y, ttk_color col) {
    pixelColor(srf, x, y, fetchcolor(col));
}
void ttk_pixel_gc(ttk_surface srf, ttk_gc gc, int x, int y) {
    pixelColor(srf, x, y, fetchcolor(gc->fg));
}

void ttk_line(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    lineColor(srf, x1, y1, x2, y2, fetchcolor(col));
}
void ttk_line_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) {
    lineColor(srf, x1, y1, x2, y2, fetchcolor(gc->fg));
}

void ttk_aaline(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    aalineColor(srf, x1, y1, x2, y2, fetchcolor(col));
}
void ttk_aaline_gc(ttk_surface srf, ttk_gc gc, int x1, int y1, int x2, int y2) {
    aalineColor(srf, x1, y1, x2, y2, fetchcolor(gc->fg));
}

void ttk_rect(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    rectangleColor(srf, x1, y1, x2, y2, fetchcolor(col));
}
void ttk_rect_gc(ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) {
    rectangleColor(srf, x, y, x + w, y + h, fetchcolor(gc->fg));
}

void ttk_fillrect(ttk_surface srf, int x1, int y1, int x2, int y2, ttk_color col) {
    boxColor(srf, x1, y1, x2, y2, fetchcolor(col));
}
void ttk_fillrect_gc(ttk_surface srf, ttk_gc gc, int x, int y, int w, int h) {
    boxColor(srf, x, y, x + w, y + h, fetchcolor(gc->fg));
}

// Font loading stubs (simplified for brevity, assumes SDL_ttf or SFont available)
// For full functionality, copy load_fnt/pcf/fff from sdl.c
void ttk_load_font(ttk_fontinfo* fi, const char* fnbase, int size) {
    // Minimal implementation: fail or use SDL_ttf if available
    // In a real scenario, copy the font loading logic from sdl.c
    fprintf(stderr, "ttk_load_font not fully implemented in waveshare.c\n");
    fi->good = 0;
}
void ttk_unload_font(ttk_fontinfo* fi) {
    if (fi->f) free(fi->f);
    fi->loaded = 0;
}

void ttk_text(ttk_surface srf, ttk_font fnt, int x, int y, ttk_color col, const char* str) {
    // Stub: implement using font->draw
}
void ttk_text_gc(ttk_surface srf, ttk_gc gc, int x, int y, const char* str) {
    // Stub
}
int ttk_text_width(ttk_font fnt, const char* str) { return 0; }
int ttk_text_height(ttk_font fnt) { return 0; }
int ttk_text_width_gc(ttk_gc gc, const char* str) { return 0; }
int ttk_text_height_gc(ttk_gc gc) { return 0; }

ttk_surface ttk_load_image(const char* path) { return SDL_LoadBMP(path); }
void ttk_free_image(ttk_surface img) { SDL_FreeSurface(img); }
void ttk_blit_image(ttk_surface src, ttk_surface dst, int dx, int dy) {
    SDL_Rect dr = {dx, dy, 0, 0};
    SDL_BlitSurface(src, 0, dst, &dr);
}
void ttk_blit_image_ex(ttk_surface src, int sx, int sy, int sw, int sh, ttk_surface dst, int dx, int dy) {
    SDL_Rect sr = {sx, sy, sw, sh};
    SDL_Rect dr = {dx, dy, 0, 0};
    SDL_BlitSurface(src, &sr, dst, &dr);
}

ttk_surface ttk_new_surface(int w, int h, int bpp) {
    return SDL_CreateRGBSurface(SDL_SWSURFACE, w, h, 16, 0xF800, 0x07E0, 0x001F, 0);
}
void ttk_free_surface(ttk_surface srf) { SDL_FreeSurface(srf); }
void ttk_surface_get_dimen(ttk_surface srf, int* w, int* h) {
    if (w) *w = srf->w;
    if (h) *h = srf->h;
}
ttk_surface ttk_scale_surface(ttk_surface srf, float factor) {
    // Requires SDL_rotozoom
    return NULL; 
}
