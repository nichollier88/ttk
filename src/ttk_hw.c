/*
 * ttk_hw.c
 *
 * Hardware abstraction layer for TTK.
 * Handles iPod-specific hardware interactions like LCD updating and
 * piezo clicker, as well as version detection.
 */
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "ttk.h"
#include "ttk_priv.h"

#ifndef IPOD
#include <SDL.h>
typedef struct sdl_additional {
    Uint32 video_flags;
    Uint32 video_flags_mask;
} sdl_additional;
extern sdl_additional sdl_add;
#endif

#ifndef ABS
#define ABS(A) (((A) < 0) ? -(A) : (A))
#endif

int ttk_podversion = -1;

/* Reads the iPod generation from /proc/cpuinfo (uClinux specific) */
static long iPod_GetGeneration() {
    int i;
    char cpuinfo[256];
    char* ptr;
    FILE* file;
    if ((file = fopen("/proc/cpuinfo", "r")) != NULL) {
        while (fgets(cpuinfo, sizeof(cpuinfo), file) != NULL)
            if (strncmp(cpuinfo, "Revision", 8) == 0) break;
        fclose(file);
    }
    for (i = 0; !isspace(cpuinfo[i]); i++);
    for (; isspace(cpuinfo[i]); i++);
    ptr = cpuinfo + i + 2;
    return strtol(ptr, NULL, 16);
}

/* Maps the generation ID to TTK_POD_* constants */
static int ttk_get_podversion_raw() {
#ifdef IPOD
    static int ver;
    if (!ver) ver = iPod_GetGeneration();
    switch (ver >> 16) {
        case 0x0:
            return TTK_POD_SANSA;
        case 0x1:
            return TTK_POD_1G;
        case 0x2:
            return TTK_POD_2G;
        case 0x3:
            return TTK_POD_3G;
        case 0x4:
            return TTK_POD_MINI_1G;
        case 0x5:
            return TTK_POD_4G;
        case 0x6:
            return TTK_POD_PHOTO;
        case 0x7:
            return TTK_POD_MINI_2G;
        case 0xB:
            return TTK_POD_VIDEO;
        case 0xC:
            return TTK_POD_NANO;
        default:
            return 0;
    }
#else
    return TTK_POD_X11;
#endif
}

/* Returns the cached iPod version, detecting it if necessary */
int ttk_get_podversion() {
    if (ttk_podversion == -1) ttk_podversion = ttk_get_podversion_raw();
    return ttk_podversion;
}

void ttk_get_screensize(int* w, int* h, int* bpp) {
    if (!ttk_screen) return;
    if (w) *w = ttk_screen->w;
    if (h) *h = ttk_screen->h;
    if (bpp) *bpp = ttk_screen->bpp;
}

/* Sets up screen dimensions for desktop emulation (SDL) */
void ttk_set_emulation(int w, int h, int bpp) {
#ifndef IPOD
    if (!ttk_screen) ttk_screen = malloc(sizeof(struct ttk_screeninfo));
    ttk_screen->w = ABS(w);
    ttk_screen->h = ABS(h);
    ttk_screen->bpp = ABS(bpp);
    if (bpp < 0 || w < 0 || h < 0) {
        sdl_add.video_flags = SDL_FULLSCREEN;
        sdl_add.video_flags_mask = SDL_FULLSCREEN;
    }
    ttk_screen->wx = 0;
    if (ttk_screen->bpp == 16)
        ttk_screen->wy = 22;
    else
        ttk_screen->wy = 20;
#endif
}

#ifdef IPOD
/*
 * Direct hardware access for iPod LCD and Timer.
 * This code interacts directly with the iPod's memory-mapped I/O
 * to update the screen and handle timing for the piezo clicker.
 */
#define LCD_DATA 0x10
#define LCD_CMD 0x08
#define IPOD_OLD_LCD_BASE 0xc0001000
#define IPOD_OLD_LCD_RTC 0xcf001110
#define IPOD_NEW_LCD_BASE 0x70003000
#define IPOD_NEW_LCD_RTC 0x60005010

static unsigned long lcd_base = 0, lcd_rtc = 0, lcd_width = 0, lcd_height = 0;

/* Monochrome / Older iPod LCD functions (PortalPlayer 5002 based) */
static int M_timer_get_current(void) { return inl(lcd_rtc); }
static int M_timer_check(int clock_start, int usecs) {
    unsigned long clock = inl(lcd_rtc);
    if ((clock - clock_start) >= usecs)
        return 1;
    else
        return 0;
}
static void M_lcd_wait_write(void) {
    if ((inl(lcd_base) & 0x8000) != 0) {
        int start = M_timer_get_current();
        do {
            if ((inl(lcd_base) & (unsigned int)0x8000) == 0) break;
        } while (M_timer_check(start, 1000) == 0);
    }
}
static void M_lcd_send_data(int data_lo, int data_hi) {
    M_lcd_wait_write();
    outl(data_lo, lcd_base + LCD_DATA);
    M_lcd_wait_write();
    outl(data_hi, lcd_base + LCD_DATA);
}
static void M_lcd_prepare_cmd(int cmd) {
    M_lcd_wait_write();
    outl(0x0, lcd_base + LCD_CMD);
    M_lcd_wait_write();
    outl(cmd, lcd_base + LCD_CMD);
}
static void M_lcd_cmd_and_data(int cmd, int data_lo, int data_hi) {
    M_lcd_prepare_cmd(cmd);
    M_lcd_send_data(data_lo, data_hi);
}
static void M_update_display(int sx, int sy, int mx, int my,
                             unsigned char* data, int pitch) {
    int y;
    unsigned short cursor_pos;
    sx >>= 3;
    mx >>= 3;
    cursor_pos = sx + (sy << 5);
    for (y = sy; y <= my; y++) {
        unsigned char* img_data;
        int x;
        M_lcd_cmd_and_data(0x11, cursor_pos >> 8, cursor_pos & 0xff);
        M_lcd_prepare_cmd(0x12);
        img_data = data + (sx << 1) + (y * (lcd_width / 4));
        for (x = sx; x <= mx; x++) {
            M_lcd_send_data(*(img_data + 1), *img_data);
            img_data += 2;
        }
        cursor_pos += 0x20;
    }
}

/* Color / Newer iPod LCD functions (PortalPlayer 502x based) */
static int C_timer_get_current(void) { return inl(0x60005010); }
static int C_timer_check(int clock_start, int usecs) {
    unsigned long clock = inl(0x60005010);
    if ((clock - clock_start) >= usecs)
        return 1;
    else
        return 0;
}
static void C_lcd_wait_write(void) {
    if ((inl(0x70008A0C) & 0x80000000) != 0) {
        int start = C_timer_get_current();
        do {
            if ((inl(0x70008A0C) & 0x80000000) == 0) break;
        } while (C_timer_check(start, 1000) == 0);
    }
}
static void C_lcd_cmd_data(int cmd, int data) {
    C_lcd_wait_write();
    outl(cmd | 0x80000000, 0x70008A0C);
    C_lcd_wait_write();
    outl(data | 0x80000000, 0x70008A0C);
}
static void C_update_display(int sx, int sy, int mx, int my,
                             unsigned char* data, int pitch) {
    int height = (my - sy) + 1;
    int width = (mx - sx) + 1;
    char* addr = (char*)data;
    if (width & 1) width++;
    C_lcd_cmd_data(0x12, (sy & 0xff));
    C_lcd_cmd_data(0x13, (((ttk_screen->w - 1) - sx) & 0xff));
    C_lcd_cmd_data(0x15, (((sy + height) - 1) & 0xff));
    C_lcd_cmd_data(0x16, (((((ttk_screen->w - 1) - sx) - width) + 1) & 0xff));
    addr += sx + sy * pitch;
    while (height > 0) {
        int h, x, y, pixels_to_write;
        pixels_to_write = (width * height) * 2;
        h = height;
        if (pixels_to_write > 64000) {
            h = (64000 / 2) / width;
            pixels_to_write = (width * h) * 2;
        }
        outl(0x10000080, 0x70008A20);
        outl((pixels_to_write - 1) | 0xC0010000, 0x70008A24);
        outl(0x34000000, 0x70008A20);
        for (x = 0; x < h; x++) {
            for (y = 0; y < width; y += 2) {
                unsigned two_pixels;
                two_pixels = addr[0] | (addr[1] << 16);
                addr += 2;
                while ((inl(0x70008A20) & 0x1000000) == 0);
                outl(two_pixels, 0x70008B00);
            }
            addr += pitch - width;
        }
        while ((inl(0x70008A20) & 0x4000000) == 0);
        outl(0x0, 0x70008A24);
        height = height - h;
    }
}

/* Updates a region of the LCD. Initializes hardware addresses on first call. */
void ttk_update_lcd(int xstart, int ystart, int xend, int yend,
                    unsigned char* data) {
    static int do_color = 0;
    static int pitch = 0;
    if (lcd_base < 0) {
        int ver = ttk_get_podversion();
        if (!ver) {
            fprintf(stderr, "No iPod. Can't ttk_update_lcd.");
            ttk_quit();
            exit(1);
        }
        if (ver & TTK_POD_SANSA) return;
        if (ver & (TTK_POD_1G | TTK_POD_2G | TTK_POD_3G)) {
            lcd_base = IPOD_OLD_LCD_BASE;
            lcd_rtc = IPOD_OLD_LCD_RTC;
        } else {
            lcd_base = IPOD_NEW_LCD_BASE;
            lcd_rtc = IPOD_NEW_LCD_RTC;
        }
        lcd_width = ttk_screen->w;
        lcd_height = ttk_screen->h;
        if (ver & TTK_POD_PHOTO) do_color = 1;
        pitch = ttk_screen->w;
    }
    if (do_color)
        C_update_display(xstart, ystart, xend, yend, data, pitch);
    else
        M_update_display(xstart, ystart, xend, yend, data, pitch);
}

/* Generates a click sound using the internal piezo buzzer */
void ttk_click_ex(int period, int duration) {
    period = period ? MIN(MAX(period, 5), 250) : 20;
    duration = MIN(MAX(duration, 1), 1000);
    if (ttk_get_podversion() & TTK_POD_PP502X) {
        outl(inl(0x70000010) & ~0xc, 0x70000010);
        outl(inl(0x6000600c) | 0x20000, 0x6000600c);
        outl(0x80000000 | 0x800000 | (period & 0xffff), 0x7000a000);
        int starttime = C_timer_get_current();
        while (!C_timer_check(starttime, duration * 1000));
        outl(0x0, 0x7000a000);
    } else {
        static int fd = -1;
        static char buf;
        if (fd == -1 && (fd = open("/dev/ttyS1", O_WRONLY)) == -1 &&
            (fd = open("/dev/tts/1", O_WRONLY)) == -1)
            return;
        write(fd, &buf, 1);
    }
}
#else
void ttk_update_lcd(int xstart, int ystart, int xend, int yend,
                    unsigned char* data) {
    fprintf(stderr, "update_lcd() skipped: not on iPod\n");
}
void ttk_click_ex(int period, int duration) {}
#endif

void ttk_click() { ttk_click_ex(20, 2); }
void ttk_start_cop(void (*fn)()) { exit(255); }
void ttk_stop_cop() {}
void ttk_sleep_cop() {}
void ttk_wake_cop() {}
