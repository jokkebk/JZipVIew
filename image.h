/**
 * Image routines.
 *
 * Copyright 2013 by Joonas Pihlajamaa <joonas.pihlajamaa@iki.fi>
 *
 * This file is part of JZipView, see https://github.com/jokkebk/JZipVIew
 *
 * JZipView is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * JZipView is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with JZipView.  If not, see <http://www.gnu.org/licenses/>.
 * @license GPL-3.0+ <http://spdx.org/licenses/GPL-3.0+>
 */
#ifndef __IMAGE_UTIL_H
#define __IMAGE_UTIL_H

#include <stdio.h>
#include <stdlib.h>

#include "SDL2/SDL.h"
#include "png.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

typedef struct {
    Uint32 *data;
    int w;
    int h;
} JImage;

#define GETPIXEL(img, x, y) ((img)->data[(y) * (img)->w + (x)])
#define SETPIXEL(img, x, y, c) { (img)->data[(y) * (img)->w + (x)] = (c); }
#define GETRGB(r,g,b) (((r)<<16)+((g)<<8)+(b))
#define GETR(rgb) (((rgb)>>16)&255)
#define GETG(rgb) (((rgb)>>8)&255)
#define GETB(rgb) ((rgb)&255)

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef SWAP
#define SWAP(a,b,t) { t = a; a = b; b = t; }
#endif

JImage *create_image(int width, int height);

void destroy_image(JImage *img);

void copy_image(JImage *dest, JImage *src);

// rotation in 90 degree steps clockwise, 0-3
void rotate_image(JImage *dest, JImage *src, int angle);

void greyscale_image(JImage *img);

void invert_image(JImage *img);

void fill_image(JImage *img, Uint32 c);

void blit_image(JImage *dest, int dx, int dy, JImage *src, int sx, int sy, int w, int h);

// Blits the whole sprite
void blit_sprite(JImage *dest, int dx, int dy, JImage *sprite);

void blit_font(JImage *image, JImage *letter, int x, int y, Uint32 c);

JImage *read_PNG_file (const char * filename);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
