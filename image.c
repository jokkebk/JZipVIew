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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "image.h"

JImage *create_image(int width, int height) {
    JImage *img = (JImage *)malloc(sizeof(JImage));

    if(img == NULL)
        return NULL;

    img->data = (Uint32 *)malloc(sizeof(Uint32)*width*height);

    if(img->data == NULL)
        return NULL;

    img->w = width;
    img->h = height;

    return img;
}

void destroy_image(JImage *img) {
    free(img->data);
    free(img);
}

void copy_image(JImage *dest, JImage *src) {
    if(dest->w != src->w || dest->h != src->h)
        return;

    memcpy(dest->data, src->data, src->w * src->h * sizeof(Uint32));
}

// rotation in 90 degree steps clockwise, 0-3
void rotate_image(JImage *dest, JImage *src, int angle) {
    int x, y;

    if(dest->w * dest->h != src->w * src->h)
        return;

    if(angle == 1 || angle == 3) {
        dest->w = src->h;
        dest->h = src->w;
    } else {
        dest->h = src->h;
        dest->w = src->w;
    }

    switch(angle) {
    case 1:
        for(y=0; y<src->h; y++)
        for(x=0; x<src->w; x++)
            SETPIXEL(dest, src->h - 1 - y, x, GETPIXEL(src, x, y));
        break;
    case 2:
        for(y=0; y<src->h; y++)
        for(x=0; x<src->w; x++)
            SETPIXEL(dest, dest->w - 1 - x, dest->h - 1 - y, GETPIXEL(src, x, y));
        break;
    case 3:
        for(y=0; y<src->h; y++)
        for(x=0; x<src->w; x++)
            SETPIXEL(dest, y, src->w - 1 - x, GETPIXEL(src, x, y));
        break;
    default:
        copy_image(dest, src);
        break;
    }
}

// make a greyscale image through simple averaging
void greyscale_image(JImage *img) {
    int x, y, avg;
    Uint32 c;

    for(y=0; y<img->h; y++) {
        for(x=0; x<img->w; x++) {
            c = GETPIXEL(img, x, y);
            avg = (GETR(c) + GETG(c) + GETB(c)) / 3;
            SETPIXEL(img, x, y, GETRGB(avg,avg,avg));
        }
    }
}

// invert image colors
void invert_image(JImage *img) {
    int x, y;

    for(y=0; y<img->h; y++) {
        for(x=0; x<img->w; x++) {
            SETPIXEL(img, x, y, 0xFFFFFF - GETPIXEL(img, x, y));
        }
    }
}

void fill_image(JImage *img, Uint32 c) {
    int i;

    for(i=0; i<img->w * img->h; i++)
        img->data[i] = c;
}

void blit_image(JImage *dest, int dx, int dy, JImage *src, int sx, int sy, int w, int h) {
    int x, y;

    // Clipping
    if(dx < 0) {
        w += dx;
        sx += dx;
        dx = 0;
    }
    if(dy < 0) {
        h += dy;
        sy += dy;
        dy = 0;
    }
    if(sx < 0) {
        w += sx;
        dx += sx;
        sx = 0;
    }
    if(sy < 0) {
        h += sy;
        dy += sy;
        sy = 0;
    }
    if(dx + w > dest->w)
        w = dest->w - dx;
    if(dy + h > dest->h)
        h = dest->h - dy;
    if(sx + w > src->w)
        w = src->w - sx;
    if(sy + h > src->h)
        h = src->h - sy;

    if(dx >= dest->w || dy >= dest->h || w <= 0 || h <= 0)
        return;

    for(y=0; y<h; y++)
        for(x=0; x<w; x++)
            SETPIXEL(dest, dx+x, dy+y, GETPIXEL(src, sx+x, sy+y));
}

void blit_sprite(JImage *dest, int dx, int dy, JImage *sprite) {
    blit_image(dest, dx, dy, sprite, 0, 0, sprite->w, sprite->h);
}

#define BLEND(c1,c2,a) (((a) * (c1) + (255-(a)) * (c2)) >> 8)

// Assumes alpha-only letter
void blit_font(JImage *image, JImage *letter, int x, int y, Uint32 c) {
    int i, j, alpha, r = GETR(c), g = GETG(c), b = GETB(c);
    Uint32 d;

    for(j=MAX(0, -y); j<letter->h && y+j < image->h; j++) {
        for(i=MAX(0, -x); i<letter->w && x+i < image->w; i++) {
            alpha = GETPIXEL(letter, i, j) & 255;

            if(!alpha) continue;

            if(alpha == 255) {
                SETPIXEL(image, x+i, y+j, c);
            } else {
                d = GETPIXEL(image, x+i, y+j);
                SETPIXEL(image, x+i, y+j, GETRGB(
                            BLEND(r, GETR(d), alpha),
                            BLEND(g, GETG(d), alpha),
                            BLEND(b, GETB(d), alpha)));
            }
        }
    }
}

JImage *read_PNG_file(const char *file_name) {
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    FILE *fp;
    unsigned int png_transforms;

    png_bytep * row_pointers;
    int x, y;
    JImage *image;

    if ((fp = fopen(file_name, "rb")) == NULL)
        return NULL;

    png_ptr = png_create_read_struct(PNG_LIBPNG_VER_STRING, 0, 0, 0);

    if (png_ptr == NULL) {
        fclose(fp);
        return NULL;
    }

    // Allocate/initialize the memory for image information.  REQUIRED.
    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_read_struct(&png_ptr, NULL, NULL);
        return NULL;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        // Free all of the memory associated with the png_ptr and info_ptr
        png_destroy_read_struct(&png_ptr, &info_ptr, NULL);
        fclose(fp);
        // If we get here, we had a problem reading the file
        return NULL;
    }

    // Set up the input control if you are using standard C streams
    png_init_io(png_ptr, fp);

    // If we have already read some of the signature
    png_set_sig_bytes(png_ptr, sig_read);

    png_transforms = PNG_TRANSFORM_STRIP_16 | PNG_TRANSFORM_PACKING | PNG_TRANSFORM_GRAY_TO_RGB;
    png_read_png(png_ptr, info_ptr, png_transforms, NULL);

    if(png_get_bit_depth(png_ptr, info_ptr) == 8) { // only this is now supported
        // Copy data to image structure
        image = create_image(png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr));
        // png_get_channels(png_ptr, info_ptr) not needed above
        row_pointers = png_get_rows(png_ptr, info_ptr);

        for(y=0; y<image->h; y++)
            for(x=0; x<image->w; x++)
                SETPIXEL(image, x, y, GETRGB(row_pointers[y][x*3+0],
                            row_pointers[y][x*3+1], row_pointers[y][x*3+2]));
    } else image = NULL;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    fclose(fp);

    return image;
}
