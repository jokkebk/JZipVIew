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

#define USE_SDL
#define USE_PNG
#define USE_JPEG

#ifdef USE_SDL
#include "SDL/SDL.h"
#endif

#ifdef USE_JPEG
#include "jpeglib.h"
#endif

#ifdef USE_PNG
#include "png.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

struct jImage {
    unsigned char * data;
    int width;
    int height;
    int components;
};

#define GETPIXEL8(img, x, y) ((img)->data[(y) * (img)->width + (x)])
#define PUTPIXEL8(img, x, y, c) { (img)->data[(y) * (img)->width + (x)] = (c); }

#ifndef MAX
#define MAX(a,b) ((a) > (b) ? (a) : (b))
#endif

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b))
#endif

#ifndef SWAP
#define SWAP(a,b,t) { t = a; a = b; b = t; }
#endif

typedef struct jImage * jImagePtr;

jImagePtr create_image(int width, int height, int components);

void destroy_image(jImagePtr img);

void copy_image(jImagePtr dest, jImagePtr src);

// rotation in 90 degree steps clockwise, 0-3
void rotate_image(jImagePtr dest, jImagePtr src, int angle);

void greyscale_image(jImagePtr img); // results in 32-bit RGB greyscale image

void invert_image(jImagePtr img);

void blit_font(jImagePtr image, jImagePtr letter, int x, int y, int c);

#ifdef USE_SDL
void blit_font_SDL(SDL_Surface *s, jImagePtr mask, Uint32 *grad, int x, int y);
Uint32 *create_gradient(SDL_Surface *s, int r, int g, int b);
void destroy_gradient(Uint32 *gradient); 
#endif

#ifdef USE_JPEG
jImagePtr read_JPEG_file (const char * filename);
#endif

#ifdef USE_PNG
jImagePtr read_PNG_file (const char * filename);
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
