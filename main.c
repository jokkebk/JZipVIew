/**
 * Main program.
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
#if defined _WIN32 || defined _WIN64
#include "windows.h"

#define HAVE_BOOLEAN /* Fix jpeglib */
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <ctype.h>
#include <stdarg.h>

#include <zlib.h>

#include <jpeglib.h>

#include "SDL2/SDL.h"
#include "image.h"
#include "font.h"
#include "junzip.h"

#define THUMB_W 400
#define THUMB_H 400

typedef struct {
    char *filename;
    long offset;
    long size;
    unsigned char *data;
    JImage *thumbnail;
} JPEGRecord;

JPEGRecord *jpegs;
int jpeg_count, thumbsLeft = 0;

SDL_Window *window = NULL;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
    SDL_Quit();
    exit(rc);
}

static void writeMessage(int flags, char *title, char *format, ...) {
    va_list args;
    char message[1024];

    va_start(args, format);
    vsprintf(message, format, args);
    va_end(args);
    SDL_ShowSimpleMessageBox(flags, title, message, window);
}

// fixed point scaling with bilinear filter to given max size (w/h)
JImage *scale(JImage *image, int w, int h) {
    JImage *res;
    int i, j, xp, yp, w2, h2, xpart, ypart;
    int ox, oy, xs, ys; // 22.10 fixed point
    int r, g, b;

    if(w * image->h > image->w * h) { // screen is wider
        xs = ys = 1024 * image->h / h;
        w2 = image->w * h / image->h;
        h2 = h;
    } else { // screen is higher
        xs = ys = 1024 * image->w / w;
        w2 = w;
        h2 = image->h * w / image->w;
    }

    res = create_image(w2, h2);

    for(j=0, oy=0; j<h2; j++, oy+=ys) {
        for(i=0, ox=0; i<w2; i++, ox+=xs) {
            xp = ox >> 10;
            yp = oy >> 10;
            xpart = ox & 1023;
            ypart = oy & 1023;

            r = ((1024-xpart) * (1024-ypart) * GETR(GETPIXEL(image, xp, yp)) +
                 (xpart) * (1024-ypart) * GETR(GETPIXEL(image, xp+1, yp)) +
                 (1024-xpart) * (ypart) * GETR(GETPIXEL(image, xp, yp+1)) +
                 (xpart) * (ypart) * GETR(GETPIXEL(image, xp+1, yp+1))) >> 20;
            g = ((1024-xpart) * (1024-ypart) * GETG(GETPIXEL(image, xp, yp)) +
                 (xpart) * (1024-ypart) * GETG(GETPIXEL(image, xp+1, yp)) +
                 (1024-xpart) * (ypart) * GETG(GETPIXEL(image, xp, yp+1)) +
                 (xpart) * (ypart) * GETG(GETPIXEL(image, xp+1, yp+1))) >> 20;
            b = ((1024-xpart) * (1024-ypart) * GETB(GETPIXEL(image, xp, yp)) +
                 (xpart) * (1024-ypart) * GETB(GETPIXEL(image, xp+1, yp)) +
                 (1024-xpart) * (ypart) * GETB(GETPIXEL(image, xp, yp+1)) +
                 (xpart) * (ypart) * GETB(GETPIXEL(image, xp+1, yp+1))) >> 20;

            res->data[j * res->w + i] = GETRGB(r,g,b);
        }
    }

    return res;
}

JImage *read_JPEG_custom(unsigned char *inbuffer, unsigned long insize,
        int tx, int ty) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPARRAY buffer;      /* Output row buffer */
    int row_stride, x, y;     /* physical row width in output buffer */
    JImage *image;

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_decompress(&cinfo);

    jpeg_mem_src(&cinfo, inbuffer, insize);

    /* Seems like these return something if read is interrupted */
    jpeg_read_header(&cinfo, TRUE);

    cinfo.out_color_space = JCS_RGB; // make RGB even from greyscale
    cinfo.dct_method = JDCT_ISLOW; // best quality, not really slower than IFAST or FLOAT

    if(tx && ty) {
        if(cinfo.image_width / 8 > tx || cinfo.image_height / 8 > ty)
            cinfo.scale_num = 1;
        else if(cinfo.image_width / 4 > tx || cinfo.image_height / 4 > ty)
            cinfo.scale_num = 2;
        else if(cinfo.image_width / 2 > tx || cinfo.image_height / 2 > ty)
            cinfo.scale_num = 4;
    }

    jpeg_start_decompress(&cinfo);

    row_stride = cinfo.output_width * cinfo.output_components;

    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);

    image = create_image(cinfo.output_width, cinfo.output_height);

    for(y=0; cinfo.output_scanline < cinfo.output_height; y++) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        for(x=0; x<image->w; x++)
            image->data[image->w * y + x] = GETRGB(buffer[0][x*3+0],
                        buffer[0][x*3+1], buffer[0][x*3+2]);
    }

    jpeg_finish_decompress(&cinfo);

    jpeg_destroy_decompress(&cinfo);

    return image;
}

JImage *loadImageFromZip(FILE *zip, JPEGRecord *jpeg, int tx, int ty) {
    JZFileHeader header;
    char filename[1024];
    JImage *image = NULL, *t;

    fseek(zip, jpeg->offset, SEEK_SET);

    if(jzReadLocalFileHeader(zip, &header, filename, sizeof(filename))) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't read local file header!");
        quit(1);
    }

    if((jpeg->data = (unsigned char *)malloc(jpeg->size)) == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't allocate memory!");
        quit(1);
    }

    if(jzReadData(zip, &header, jpeg->data) == Z_OK) {
        image = read_JPEG_custom(jpeg->data, jpeg->size, tx, ty);
        if(tx && ty) {
            t = scale(image, tx ? tx : image->w, ty ? ty : image->h);
            destroy_image(image);
            image = t;
        }
    }

    return image;
}

// Caseless comparison of haystack end to lowercase needle
int matchExtension(const char *haystack, const char *needle) {
    const char *stack = haystack + strlen(haystack) - strlen(needle);

    if(stack < haystack)
        return 0;

    for(; *needle; stack++, needle++)
        if(tolower(*stack) != *needle)
            return 0;

    return 1;
}

// Check individual file if it's a jpeg and store data for it if it is
int recordCallback(FILE *zip, int idx, JZFileHeader *header, char *filename) {
    JPEGRecord *jpeg = &jpegs[jpeg_count];

    if(!matchExtension(filename, ".jpg") && !matchExtension(filename, ".jpeg"))
        return 1; // skip

    jpeg->offset = header->offset;
    jpeg->size = header->uncompressedSize;
    jpeg->thumbnail = NULL;
    jpeg->filename = (char *)malloc(strlen(filename)+1);

    if(jpeg->filename == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't allocate space for filename!\n");
        quit(1);
    }

    strcpy(jpeg->filename, filename); // store filename
    jpeg_count++;

    return 1; // continue
}

// Process zip central directory and allocate room for jpegs
int processZip(FILE *zip) {
    JZEndRecord endRecord;

    if(jzReadEndRecord(zip, &endRecord)) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't read ZIP file end record.");
        return -1;
    }

    jpegs = (JPEGRecord *)malloc(sizeof(JPEGRecord) * endRecord.numEntries);
    jpeg_count = 0;

    if(jzReadCentralDirectory(zip, &endRecord, recordCallback)) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't read ZIP file central record.");
        return -1;
    }

    return 0;
}

void drawImage(JImage *screen, JImage *image, int xoff, int yoff) {
    int dx = 0, dy = 0;

    if(screen->w > image->w) // center if fits
        dx = (screen->w - image->w) / 2;

    if(screen->h > image->h) // center if fits
        dy = (screen->h - image->h) / 2;

    if(dx || dy)
        fill_image(screen, 0);

    blit_image(screen, dx, dy, image, xoff, yoff, image->w, image->h);
}

void drawThumbs(JImage *screen, JFont *font, int tx, int ty, int topleft) {
    JImage *thumb;
    int tw = screen->w / tx, th = screen->h / ty;
    int i, j, idx;
    char num[6];

    fill_image(screen, thumbsLeft ? GETRGB(80,0,0) : 0);

    for(j = 0; j < ty; j++) {
        for(i = 0; i < tx; i++) {
            idx = topleft + j * tx + i;

            if(idx >= jpeg_count)
                break; // done

            if((thumb = jpegs[idx].thumbnail)) {
                blit_sprite(screen, tw * i, th * j, thumb);
            } else {
                sprintf(num, "%d", idx + 1);
                write_font(screen, font, 0xFFFFFF, num,
                        tw * i + tw / 2,
                        th * j + th / 2,
                        FONT_ALIGN_MIDDLE + FONT_ALIGN_CENTER, 2);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    SDL_Renderer *renderer;
    SDL_Texture *texture;
    JImage *screen;
    JFont *font24;
    int x, y;
    char fontname[1024];
    FILE *zip;
    JPEGRecord *jpeg;
    SDL_Event event;
    int done = 0, redraw = 1, tx = 8, ty = 5, i, xoff = 0, yoff = 0,
        currentImage = 0, loadedFullscreen = -1, loadedFullsize = -1;
    JImage *fullscreen = NULL, *fullsize = NULL;
    enum { MODE_THUMBS, MODE_FULLSCREEN, MODE_FULLSIZE } mode = MODE_THUMBS;

    if(argc < 2) {
        writeMessage(SDL_MESSAGEBOX_INFORMATION, "Usage", "jzipview <pictures.zip>");
        return 0;
    }

    if(strlen(argv[0]) > 1000) {
        writeMessage(SDL_MESSAGEBOX_WARNING, "Too long path for executable", "Where are you invoking this?");
        return 0;
    }

    if(!(zip = fopen(argv[1], "rb"))) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't open ZIP \"%s\"!", argv[1]);
        return -1;
    }

    strcpy(fontname, argv[0]);
    for(i = strlen(fontname)-1; i; i--) {
        if(fontname[i] == '/' || fontname[i] == '\\') {
            i++;
            break;
        }
    }
    strcpy(fontname+i, "font24.png"); // append font name to .exe directory

    font24 = create_font(read_PNG_file(fontname),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.,:;!?'/&()=", 4);

    if(SDL_Init(SDL_INIT_EVERYTHING) != 0) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "SDL_Init Error: %s\n", SDL_GetError());
        return 1;
    }

    window = SDL_CreateWindow("JZipView",
            SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
            0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if(window == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "SDL_CreateWindow Error: %s\n", SDL_GetError());
        return 1;
    }

    renderer = SDL_CreateRenderer(window, -1, 0);
    if(renderer == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        return 1;
    }

    // Initialize raw pixel surface for rendering
    SDL_GetRendererOutputSize(renderer, &x, &y);
    screen = create_image(x, y);
    if(screen == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "SDL_CreateRenderer Error: %s\n", SDL_GetError());
        quit(1);
    }

    texture = SDL_CreateTexture(renderer,
            SDL_PIXELFORMAT_ARGB8888,
            SDL_TEXTUREACCESS_STREAMING,
            screen->w, screen->h);

    if(processZip(zip)) {
        quit(1);
    }

    thumbsLeft = jpeg_count;

    tx = screen->w / THUMB_W;
    ty = screen->h / THUMB_H;

    // main loop
    while(!done) {
        SDL_UpdateTexture(texture, NULL, screen->data, screen->w * sizeof (Uint32));

        //SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
        //SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        if(mode == MODE_FULLSCREEN && loadedFullscreen != currentImage) {
            if(fullscreen != NULL)
                destroy_image(fullscreen);
            fullscreen = loadImageFromZip(zip, jpegs+currentImage, screen->w, screen->h);
            loadedFullscreen = currentImage;
        } else if(mode == MODE_FULLSIZE && loadedFullsize != currentImage) {
            if(fullsize != NULL)
                destroy_image(fullsize);
            fullsize = loadImageFromZip(zip, jpegs+currentImage, 0, 0);
            loadedFullsize = currentImage;
        } else if(thumbsLeft) {
            for(i = 0; i < jpeg_count; i++) {
                jpeg = &jpegs[(currentImage + i) % jpeg_count];
                if(jpeg->thumbnail != NULL) continue;
                jpeg->thumbnail = loadImageFromZip(zip, jpeg, screen->w / tx, screen->h / ty);
                thumbsLeft--;
                redraw = 1;
                break;
            }
        }

        if(redraw) {
            switch(mode) {
                case MODE_THUMBS:
                    drawThumbs(screen, font24, tx, ty, currentImage);
                    break;
                case MODE_FULLSCREEN:
                    drawImage(screen, fullscreen, 0, 0);
                    break;
                case MODE_FULLSIZE:
                    drawImage(screen, fullsize, xoff, yoff);
                    break;
            }
            redraw = 0;
        }

        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_MOUSEBUTTONDOWN:
                    switch(event.button.button) {
                        case SDL_BUTTON_LEFT:
                            if(mode == MODE_THUMBS) {
                                currentImage = event.button.x / THUMB_W +
                                    tx * (event.button.y / THUMB_H) + currentImage;
                                mode = MODE_FULLSCREEN;
                            } else if(mode == MODE_FULLSCREEN) {
                                mode = MODE_FULLSIZE;
                            }
                            SDL_ShowCursor(mode == MODE_THUMBS ? 1 : 0);
                            redraw = 1;
                            break;
                        case SDL_BUTTON_RIGHT:
                            if(mode == MODE_THUMBS) {
                                done = 1;
                            } else if(mode == MODE_FULLSIZE) {
                                mode = MODE_FULLSCREEN;
                            } else {
                                currentImage -= currentImage % tx + ty / 2 * tx;
                                if(currentImage < 0)
                                    currentImage = 0;
                                mode = MODE_THUMBS;
                            }
                            SDL_ShowCursor(mode == MODE_THUMBS ? 1 : 0);
                            redraw = 1;
                            break;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    break;
                case SDL_MOUSEMOTION:
                    if(mode == MODE_FULLSIZE) {
                        xoff = (fullsize->w <= screen->w) ? 0 :
                            (fullsize->w - screen->w) * event.button.x / screen->w;
                        yoff = (fullsize->h <= screen->h) ? 0 :
                            (fullsize->h - screen->h) * event.button.y / screen->h;
                        redraw = 1;
                    }
                    break;

                case SDL_MOUSEWHEEL:
                    if(event.wheel.y > 0) {
                        if(mode == MODE_THUMBS) {
                            currentImage -= tx;
                            if(currentImage < 0)
                                currentImage = 0;
                        } else {
                            if(currentImage)
                                currentImage--;
                        }
                        redraw = 1;
                    }
                    if(event.wheel.y < 0) {
                        if(mode == MODE_THUMBS) {
                            if(currentImage + tx * ty >= jpeg_count)
                                break;
                            currentImage += tx;
                        } else {
                            if(++currentImage >= jpeg_count)
                                currentImage = jpeg_count - 1;
                        }
                        redraw = 1;
                    }
                    break;

                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q: case SDLK_x:
                            done = 1;
                            break;
                        case SDLK_SPACE:
                        case SDLK_LEFT:
                        case SDLK_RIGHT:
                        case SDLK_UP:
                        case SDLK_DOWN:
                        case SDLK_RETURN:
                        case SDLK_BACKSPACE:
                        case SDL_QUIT:
                        default:
                            break;
                    } // end switch(event.key.keysym.sym)
                    break;
            } // end switch(event.type)
        } // end while(SDL_PollEvent(&event))
    } // end while(!done)

    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();

    destroy_image(screen);

    destroy_font(font24);

    fclose(zip);

    return(0);
}
