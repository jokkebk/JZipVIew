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

void fill_image(jImagePtr img, int r, int g, int b);

void copy_image(jImagePtr dest, jImagePtr src);

// rotation in 90 degree steps clockwise, 0-3
void rotate_image(jImagePtr dest, jImagePtr src, int angle);

// all zero to b, other to w, works only for 8-bit images (1 component)
void copy_image_mono(jImagePtr dest, jImagePtr src, int b, int w);

jImagePtr duplicate_image(jImagePtr img);

void half_alpha(jImagePtr img);

void destroy_image(jImagePtr img);

void greyscale_image(jImagePtr img); // results in 32-bit RGB greyscale image

jImagePtr convert_to_greyscale(jImagePtr image); // results in a new 8-bit greyscale image

// halves vertical and horizontal resolution - doesn't reallocate (it's for pussies)
void downscale_image(jImagePtr img);

void invert_image(jImagePtr img);

void write_text_image(const char * filename, jImagePtr font, int component);

void blit_font(jImagePtr image, jImagePtr letter, int x, int y, int c);

void auto_levels(jImagePtr image);

#ifdef USE_SDL
void blit_font_SDL(SDL_Surface *s, jImagePtr mask, Uint32 *grad, int x, int y);

Uint32 *create_gradient(SDL_Surface *s, int r, int g, int b);

void destroy_gradient(Uint32 *gradient); 
#endif

#ifdef USE_JPEG
void write_JPEG_file (const char * filename, int quality, jImagePtr image);
void write_JPEG_buffer (unsigned char ** buffer, unsigned long * size, int quality, jImagePtr image);
jImagePtr read_JPEG_file (const char * filename);
#endif

#ifdef USE_PNG
int write_PNG_file(const char *filename, jImagePtr image);
jImagePtr read_PNG_file (const char * filename);
#endif

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
