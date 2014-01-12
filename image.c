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

jImagePtr create_image(int width, int height, int components) {
    jImagePtr img = (jImagePtr)malloc(sizeof(struct jImage));

    if(img == NULL)
        return NULL;

    img->data = (JSAMPLE *)malloc(sizeof(JSAMPLE)*components*width*height);

    if(img->data == NULL)
        return NULL;

    img->width = width;
    img->height = height;
    img->components = components;

    return img;
}

void destroy_image(jImagePtr img) {
    free(img->data);
    free(img);
}

void copy_image(jImagePtr dest, jImagePtr src) {
    if(dest->width != src->width || dest->height != src->height || dest->components != src->components)
        return;

    memcpy(dest->data, src->data, src->width * src->height * src->components);
}

// rotation in 90 degree steps clockwise, 0-3
void rotate_image(jImagePtr dest, jImagePtr src, int angle) {
    int x, y, c;

    if(dest->width * dest->height != src->width * src->height || src->components != dest->components)
        return;

    if(angle == 1 || angle == 3) {
        dest->width = src->height;
        dest->height = src->width;
    } else {
        dest->height = src->height;
        dest->width = src->width;
    }

    switch(angle) {
    case 1:
        for(y=0; y<src->height; y++)
        for(x=0; x<src->width; x++)
        for(c=0; c<src->components; c++)
            dest->data[((x) * dest->width + (dest->width-1 - y)) * dest->components + c] =
            src->data[((y) * src->width + (x)) * src->components + c];
        break;
    case 2:
        for(y=0; y<src->height; y++)
        for(x=0; x<src->width; x++)
        for(c=0; c<src->components; c++)
            dest->data[((dest->height - 1 - y) * dest->width + (dest->width - 1 - x)) * dest->components + c] =
            src->data[((y) * src->width + (x)) * src->components + c];
        break;
    case 3:
        for(y=0; y<src->height; y++)
        for(x=0; x<src->width; x++)
        for(c=0; c<src->components; c++)
            dest->data[((dest->height - 1 - x) * dest->width + (y)) * dest->components + c] =
            src->data[((y) * src->width + (x)) * src->components + c];
        break;
    default:
        copy_image(dest, src);
        break;
    }
}

// make a greyscale image through simple averaging
void greyscale_image(jImagePtr img) {
    int i, j, offset = 0, r, g, b, avg;

    for(j=0; j<img->height; j++) {
        for(i=0; i<img->width; i++) {
            r = img->data[offset+0];
            g = img->data[offset+1];
            b = img->data[offset+2];
            avg = (r+g+b)/3;
            img->data[offset+0] = avg;
            img->data[offset+1] = avg;
            img->data[offset+2] = avg;
            offset += img->components;
        }
    }
}

// invert image colors
void invert_image(jImagePtr img) {
    int i, j, offset = 0, r, g, b;

    for(j=0; j<img->height; j++) {
        for(i=0; i<img->width; i++) {
            r = img->data[offset+0];
            g = img->data[offset+1];
            b = img->data[offset+2];
            img->data[offset+0] = 255-r;
            img->data[offset+1] = 255-g;
            img->data[offset+2] = 255-b;
            offset += img->components;
        }
    }
}

#define BLEND(c1,c2,a) (((a) * (c1) + (255-(a)) * (c2)) >> 8)

// Assumes alpha-only letter
void blit_font(jImagePtr image, jImagePtr letter, int x, int y, int c) {
    unsigned char *pixels = image->data, *ch = letter->data;
    int i, j, k, alpha;

    for(j=MAX(0, -y); j<letter->height && y+j < image->height; j++) {
        for(i=MAX(0, -x); i<letter->width && x+i < image->width; i++) {
            alpha = ch[j * letter->width + i];

            if(!alpha)
                continue;

            for(k=0; k<image->components; k++) {
                if(alpha == 255) {
                    pixels[((y+j) * image->width + (x+i))*image->components + k] = c;
                } else {
                    pixels[((y+j) * image->width + (x+i))*image->components + k] =
                        BLEND(c, pixels[((y+j) * image->width + (x+i))*image->components + k], alpha);
                }
            }
        }
    }
}

#ifdef USE_SDL
Uint32 *create_gradient(SDL_Surface *s, int r, int g, int b) {
    Uint32 *grad = (Uint32 *)malloc(256*sizeof(Uint32));
    int i;

    for(i=0; i<256; i++) {
        grad[i] =
            (i * r / 255 << s->format->Rshift) +
            (i * g / 255 << s->format->Gshift) +
            (i * b / 255 << s->format->Bshift);
    }

    return grad;
}

void destroy_gradient(Uint32 *gradient) {
    free(gradient);
}

// Assumes RGB surface and alpha-only letter, uses color gradient "grad"
void blit_font_SDL(SDL_Surface *s, jImagePtr letter, Uint32 *grad, int x, int y) {
    Uint32 *pixels = (Uint32 *)s->pixels;
    unsigned char *ch = letter->data;
    int i, j, r, g, b, alpha;

    for(j=MAX(0, -y); j<letter->height && y+j < s->h; j++) {
        for(i=MAX(0, -x); i<letter->width && x+i < s->w; i++) {
            alpha = ch[j * letter->width + i];

            if(!alpha)
                continue;

            if(alpha == 255) {
                pixels[(y+j) * s->w + (x+i)] = grad[255];
            } else {
                r = (255-alpha) * ((pixels[(y+j) * s->w + (x+i)] >> s->format->Rshift) & 255) >> 8;
                g = (255-alpha) * ((pixels[(y+j) * s->w + (x+i)] >> s->format->Gshift) & 255) >> 8;
                b = (255-alpha) * ((pixels[(y+j) * s->w + (x+i)] >> s->format->Bshift) & 255) >> 8;
                pixels[(y+j) * s->w + (x+i)] = grad[alpha] +
                    (r << s->format->Rshift) +
                    (g << s->format->Gshift) +
                    (b << s->format->Bshift);
            }
        }
    }
}
#endif // USE_SDL

#ifdef USE_JPEG

jImagePtr read_JPEG_file (const char * filename) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    FILE * infile;      /* source file */
    JSAMPARRAY buffer;      /* Output row buffer */
    int row_stride;     /* physical row width in output buffer */
    jImagePtr image;

    if ((infile = fopen(filename, "rb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        return NULL;
    }

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    jpeg_stdio_src(&cinfo, infile);

    /* Seems like these return something if read is interrupted */
    jpeg_read_header(&cinfo, TRUE);
    jpeg_start_decompress(&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;

    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    image = create_image(cinfo.output_width, cinfo.output_height, cinfo.output_components);

    while (cinfo.output_scanline < cinfo.output_height) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        memcpy(&image->data[(cinfo.output_scanline-1) * row_stride], buffer[0], row_stride);
    }

    jpeg_finish_decompress(&cinfo);

    jpeg_destroy_decompress(&cinfo);

    fclose(infile);

    return image;
}

#endif // USE_JPEG

#ifdef USE_PNG

// Currently hardcoded for RGBA
jImagePtr read_PNG_file(const char *file_name) {
    png_structp png_ptr;
    png_infop info_ptr;
    unsigned int sig_read = 0;
    FILE *fp;
    unsigned int png_transforms;

    png_bytep * row_pointers;
    int j;
    jImagePtr image;

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
        image = create_image(png_get_image_width(png_ptr, info_ptr), png_get_image_height(png_ptr, info_ptr), png_get_channels(png_ptr, info_ptr));

        row_pointers = png_get_rows(png_ptr, info_ptr);

        for(j=0; j<image->height; j++)
            memcpy(image->data + image->width*image->components*j, row_pointers[j], image->width*image->components);
    } else image = NULL;

    png_destroy_read_struct(&png_ptr, &info_ptr, NULL);

    fclose(fp);

    return image;
}

#endif // USE_PNG
