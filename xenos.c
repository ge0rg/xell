/* xenos.c - pseudo framebuffer driver for Xenos on Xbox 360
 *
 * Copyright (C) 2007 Georg Lukas <georg@boerde.de>
 *
 * Based on code by Felix Domke <tmbinc@elitedvb.net>
 *
 * This software is provided under the GNU GPL License v2.
 */
#include <cache.h>
#include <types.h>
#include <string.h>
#include <vsprintf.h>

// XXX: #define NATIVE_RESOLUTION

int xenos_width, xenos_height,
    xenos_size;

/* Colors in BGRA: background and foreground */
uint32_t xenos_color[2] = { 0x00000000, 0xFFA0A000 };

unsigned char *xenos_fb = 0LL;

int cursor_x, cursor_y,
    max_x, max_y;

struct ati_info {
	uint32_t unknown1[4];
	uint32_t base;
	uint32_t unknown2[8];
	uint32_t width;
	uint32_t height;
};


/* set a pixel to RGB values, must call xenos_init() first */
inline void xenos_pset32(int x, int y, int color)
{
#define fbint ((uint32_t*)xenos_fb)
#define base (((y >> 5)*32*xenos_width + ((x >> 5)<<10) \
	   + (x&3) + ((y&1)<<2) + (((x&31)>>2)<<3) + (((y&31)>>1)<<6)) ^ ((y&8)<<2))
	fbint[base] = color;
#undef fbint
#undef base
}

inline void xenos_pset(int x, int y, unsigned char r, unsigned char g, unsigned char b) {
	xenos_pset32(x, y, (b<<24) + (g<<16) + (r<<8));
	/* little indian:
	 * fbint[base] = b + (g<<8) + (r<<16); */
}

extern const unsigned char fontdata_8x16[4096];

void xenos_draw_char(const int x, const int y, const unsigned char c) {
#define font_pixel(ch, x, y) ((fontdata_8x16[ch*16+y]>>(7-x))&1)
	int lx, ly;
	for (ly=0; ly < 16; ly++)
		for (lx=0; lx < 8; lx++) {
			xenos_pset32(x+lx, y+ly, xenos_color[font_pixel(c, lx, ly)]);
		}
}

void xenos_clrscr(const unsigned int bgra) {
#if 0
	unsigned int *fb = (unsigned int*)xenos_fb;
	int count = xenos_width * xenos_height;
	while (count--)
		*fb++ = bgra;
#else
	memset(xenos_fb, bgra, xenos_size*4);
	dcache_flush(xenos_fb, xenos_size*4);
#endif
}

void xenos_scroll32(const unsigned int lines) {
	int l, bs;
	bs = xenos_width*32*4;
	/* copy all tile blocks to a higher position */
	for (l=lines; l*32 < xenos_height; l++) {
		memcpy(xenos_fb + bs*(l-lines),
		       xenos_fb + bs*l,
		       bs);
	}
	memset(xenos_fb + xenos_size*4 - bs*lines, 0, bs*lines);
	dcache_flush(xenos_fb, xenos_size*4);
}

void xenos_newline() {
#if 0
	/* fill up with spaces */
	while (cursor_x*8 < xenos_width) {
		xenos_draw_char(cursor_x*8, cursor_y*16, ' ');
		cursor_x++;
	}
#endif
	
	/* reset to the left */
	cursor_x = 0;
	cursor_y++;
	if (cursor_y >= max_y) {
		/* XXX implement scrolling */
		xenos_scroll32(1);
		cursor_y -= 2;
	}
}

void xenos_putch(const char c) {
	if (!xenos_fb)
		return;
	if (c == '\r') {
		cursor_x = 0;
	} else if (c == '\n') {
		xenos_newline();
	} else {
		xenos_draw_char(cursor_x*8, cursor_y*16, c);
		cursor_x++;
		if (cursor_x >= max_x)
			xenos_newline();
	}
	dcache_flush(xenos_fb, xenos_size*4);
}

void xenos_init() {
	struct ati_info *ai = (struct ati_info*)0x200ec806100ULL;
#ifdef NATIVE_RESOLUTION
#error not yet coded
#endif
	xenos_fb = (unsigned char*)(long)(ai->base);
	/* round up size to tiles of 32x32 */
	xenos_width = ((ai->width+31)>>5)<<5;
	xenos_height = ((ai->height+31)>>5)<<5;
	xenos_size = xenos_width*xenos_height;

	cursor_x = cursor_y = 0;
	max_x = ai->width / 8;
	max_y = ai->height / 16;

	/* XXX use memset_io() instead? */
	xenos_clrscr(0);

	printf(" * Xenos FB with %dx%d (%dx%d) at %p initialized.\n",
		max_x, max_y, ai->width, ai->height, xenos_fb);
}

