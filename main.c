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

//#define LOGFILE "jzipview.log"
#ifdef LOGFILE
    FILE *logfile;
#endif

#if defined _WIN32 || defined _WIN64
#undef HAVE_STDDEF_H /* Fix SDL warning */
#endif

#include "SDL2/SDL.h"
#include "image.h"
#include "font.h"
#include "junzip.h"

#define THUMB_W 400
#define THUMB_H 400

typedef struct {
    char *filename;
    long offset;
    long size, compressedSize;
    unsigned char *data;
    JImage *thumbnail;
    int loaded;
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
    int ox, oy, step; // 22.10 fixed point
    int r, g, b;

    if(w * image->h > image->w * h) { // screen is wider
        step = 1024 * image->h / h;
        w2 = image->w * h / image->h;
        h2 = h;
    } else { // screen is higher
        step = 1024 * image->w / w;
        w2 = w;
        h2 = image->h * w / image->w;
    }

    if((res = create_image(w2, h2)) == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't allocate memory for scaled image!\n");
        quit(1);
    }

    for(j=0, oy=0; j<h2; j++, oy+=step) {
        yp = oy >> 10;
        ypart = oy & 1023;
        if(yp + 1 >= image->h) break;

        for(i=0, ox=0; i<w2; i++, ox+=step) {
            xp = ox >> 10;
            xpart = ox & 1023;
            if(xp + 1 >= image->w) break;

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

// Poor man's error handling, thanks JPEGLIB for ZERO documentation
int jpegError = 0;
void error_exit(j_common_ptr cinfo) { jpegError = 1; }

JImage *read_JPEG_custom(unsigned char *inbuffer, unsigned long insize,
        int tx, int ty) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPARRAY buffer;      /* Output row buffer */
    int row_stride, x, y;     /* physical row width in output buffer */
    JImage *image = NULL;

    cinfo.err = jpeg_std_error(&jerr);
    jerr.error_exit = error_exit; // catch errors and skip instead of exiting
    jpegError = 0; // reset possible error state

    jpeg_create_decompress(&cinfo);
    if(jpegError) goto READ_ERROR;

    jpeg_mem_src(&cinfo, inbuffer, insize);
    if(jpegError) goto READ_ERROR_DESTROY;

    jpeg_read_header(&cinfo, TRUE);
    if(jpegError) goto READ_ERROR_DESTROY;

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
    if(jpegError) goto READ_ERROR_FINISH;

    row_stride = cinfo.output_width * cinfo.output_components;

    /* Make a one-row-high sample array that will go away when done with image */
    buffer = (*cinfo.mem->alloc_sarray)((j_common_ptr) &cinfo, JPOOL_IMAGE, row_stride, 1);
    if(jpegError) goto READ_ERROR_FINISH;

    if((image = create_image(cinfo.output_width, cinfo.output_height)) == NULL) {
        writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't allocate memory for scaled image!\n");
        quit(1);
    }

    for(y=0; cinfo.output_scanline < cinfo.output_height; y++) {
        jpeg_read_scanlines(&cinfo, buffer, 1);
        if(jpegError) goto READ_ERROR_FINISH;
        for(x=0; x<image->w; x++)
            image->data[image->w * y + x] = GETRGB(buffer[0][x*3+0],
                        buffer[0][x*3+1], buffer[0][x*3+2]);
    }

READ_ERROR_FINISH:
    jpeg_finish_decompress(&cinfo);
READ_ERROR_DESTROY:
    jpeg_destroy_decompress(&cinfo);
READ_ERROR:
    return image;
}

JImage *loadImageFromZip(FILE *zip, JPEGRecord *jpeg, int destx, int desty) {
    JZFileHeader header;
    JImage *image = NULL, *t;

    if(jpeg->data == NULL) {
        fseek(zip, jpeg->offset, SEEK_SET);

        if(jzReadLocalFileHeader(zip, &header, NULL, 0)) { // don't re-read filename
            writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't read local file header!");
            quit(1);
        }

        if((jpeg->data = (unsigned char *)malloc(jpeg->size)) == NULL) {
            writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't allocate memory!");
            quit(1);
        }

        if(jzReadData(zip, &header, jpeg->data) != Z_OK) {
            writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't read/uncompress JPEG!");
            quit(1);
        }
    }

    image = read_JPEG_custom(jpeg->data, jpeg->size, destx, desty);

    if(image != NULL && destx && desty) { // stretch/shrink
        t = scale(image, destx, desty);
        destroy_image(image);
        image = t;
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
    jpeg->compressedSize = header->compressedSize;
    jpeg->data = NULL;
    jpeg->thumbnail = NULL;
    jpeg->loaded = 0;
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
    int done = 0, redraw = 1, tx = 8, ty = 5, i, j, mousex = 0, mousey = 0,
        currentImage = 0, earlierImage = 0, loadedFullscreen = -1, loadedFullsize = -1;
    JImage *fullscreen = NULL, *fullsize = NULL;
    enum { MODE_THUMBS, MODE_FULLSCREEN, MODE_FULLSIZE } mode = MODE_THUMBS;
    int windowed = 0; // Flag for windowed mode

#ifdef LOGFILE
    logfile = fopen(LOGFILE, "wt");
#endif

    // Check for command line arguments
    if(argc < 2) {
        writeMessage(SDL_MESSAGEBOX_INFORMATION, "Usage", "jzipview <pictures.zip> [--windowed]");
        return 0;
    }
    
    // Parse command line arguments
    for(i = 2; i < argc; i++) {
        if(strcmp(argv[i], "--windowed") == 0) {
            windowed = 1;
        }
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

    // Create window based on mode
    if(windowed) {
        window = SDL_CreateWindow("JZipView",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                1024, 768, SDL_WINDOW_RESIZABLE);
    } else {
        window = SDL_CreateWindow("JZipView",
                SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                0, 0, SDL_WINDOW_FULLSCREEN_DESKTOP);
    }
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
    if((screen = create_image(x, y)) == NULL) {
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

    // Ensure tx and ty are at least 1 to prevent division by zero
    tx = (screen->w / THUMB_W > 0) ? screen->w / THUMB_W : 1;
    ty = (screen->h / THUMB_H > 0) ? screen->h / THUMB_H : 1;

    // main loop
    while(done < 2) {
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
        } else if(thumbsLeft && mode != MODE_FULLSIZE) { // don't load thumbs when in fullsize, too slow
            for(i = 0; i < jpeg_count; i++) {
                j = (currentImage + i) % jpeg_count;
                jpeg = &jpegs[j];
                if(jpeg->loaded) continue;
                jpeg->thumbnail = loadImageFromZip(zip, jpeg, screen->w / tx, screen->h / ty);
                jpeg->loaded = 1;
                thumbsLeft--;
                if(mode == MODE_THUMBS && j >= currentImage && j < currentImage + tx*ty)
                    redraw = 1; // load affected current view
                break;
            }
        }

        if(redraw) {
            switch(mode) {
                case MODE_THUMBS:
                    drawThumbs(screen, font24, tx, ty, currentImage);
                    break;
                case MODE_FULLSCREEN:
                    if(fullscreen != NULL)
                        drawImage(screen, fullscreen, 0, 0);
                    break;
                case MODE_FULLSIZE:
                    if(fullsize != NULL) drawImage(screen, fullsize,
                        (fullsize->w <= screen->w) ? 0 : (fullsize->w - screen->w) * mousex / screen->w,
                        (fullsize->h <= screen->h) ? 0 : (fullsize->h - screen->h) * mousey / screen->h);
                    break;
            }
            SDL_UpdateTexture(texture, NULL, screen->data, screen->w * sizeof (Uint32));
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            redraw = 0;
        }

        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_MOUSEBUTTONDOWN:
                    switch(event.button.button) {
                        case SDL_BUTTON_LEFT:
                            if(mode == MODE_THUMBS) {
                                earlierImage = currentImage; // store where we were

                                // Calculate actual thumbnail cell dimensions
                                int actual_thumb_w = (tx > 0) ? (screen->w / tx) : screen->w;
                                int actual_thumb_h = (ty > 0) ? (screen->h / ty) : screen->h;
                                if (actual_thumb_w == 0) actual_thumb_w = 1; // Prevent division by zero if screen too small
                                if (actual_thumb_h == 0) actual_thumb_h = 1;

                                int clicked_col = event.button.x / actual_thumb_w;
                                int clicked_row = event.button.y / actual_thumb_h;
                                currentImage = earlierImage + clicked_row * tx + clicked_col;

                                if(currentImage < jpeg_count) // clicked on a thumbnail
                                    mode = MODE_FULLSCREEN;
                                else // clicked on empty area
                                    currentImage = earlierImage; 
                            } else if(mode == MODE_FULLSCREEN && (1 || fullscreen->w >= screen->w || fullscreen->h >= screen->h)) {
                                mode = MODE_FULLSIZE;
                            }
                            SDL_ShowCursor(mode == MODE_THUMBS ? 1 : 0);
                            redraw = 1;
                            break;
                        case SDL_BUTTON_RIGHT:
                            if(mode == MODE_THUMBS) // trigger on mouseup so it won't go to O/S after exit
                                done = 1; // will transition to done = 2 on mouseup
                            else if(mode == MODE_FULLSIZE) {
                                mode = MODE_FULLSCREEN;
                            } else if(mode == MODE_FULLSCREEN) { // Back to thumbnails
                                if(earlierImage <= currentImage && currentImage < earlierImage + tx*ty)
                                    currentImage = earlierImage; // restore previous thumbnail location
                                else
                                    currentImage -= currentImage % tx + ty / 2 * tx; // center to current image

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
                    switch(event.button.button) {
                        case SDL_BUTTON_RIGHT:
                            if(mode == MODE_THUMBS && done)
                                done = 2;
                            break;
                    }
                    break;

                case SDL_MOUSEMOTION:
                    mousex = event.button.x;
                    mousey = event.button.y;
                    if(mode == MODE_FULLSIZE) redraw = 1;
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
                    
                case SDL_WINDOWEVENT:
                    if(event.window.event == SDL_WINDOWEVENT_RESIZED ||
                       event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED) {
                        // Handle window resize
                        int new_w = event.window.data1;
                        int new_h = event.window.data2;
                        
                        // Free old resources
                        destroy_image(screen);
                        SDL_DestroyTexture(texture);
                        
                        // Create new resources with new size
                        screen = create_image(new_w, new_h);
                        if (!screen) { writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't create image for new size!"); quit(1); }
                        texture = SDL_CreateTexture(renderer,
                                SDL_PIXELFORMAT_ARGB8888,
                                SDL_TEXTUREACCESS_STREAMING,
                                screen->w, screen->h);
                        if (!texture) { writeMessage(SDL_MESSAGEBOX_ERROR, "Error message", "Couldn't create texture for new size!"); quit(1); }
                        
                        // Recalculate thumbnail grid, ensuring tx and ty are at least 1
                        tx = (screen->w / THUMB_W > 0) ? screen->w / THUMB_W : 1;
                        ty = (screen->h / THUMB_H > 0) ? screen->h / THUMB_H : 1;
                        
                        // Invalidate all existing thumbnails to force reload with new dimensions
                        for(i = 0; i < jpeg_count; i++) {
                            if(jpegs[i].thumbnail != NULL) {
                                destroy_image(jpegs[i].thumbnail);
                                jpegs[i].thumbnail = NULL;
                            }
                            jpegs[i].loaded = 0;
                        }
                        thumbsLeft = jpeg_count;
                        
                        // Reload fullscreen image if needed
                        if(mode == MODE_FULLSCREEN) {
                            loadedFullscreen = -1; // Force reload at correct size
                        }
                        // If in fullsize mode, the image itself is original size, but view might need update
                        if(mode == MODE_FULLSIZE) {
                            loadedFullsize = -1; // Force reload if necessary, or at least re-evaluate view
                        }
                        
                        redraw = 1;
                    }
                    break;

                case SDL_KEYDOWN:
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_q: case SDLK_x:
                            done = 2;
                            break;
                        case SDLK_f: // Toggle fullscreen
                            if(windowed) { // Currently windowed, switch to fullscreen
                                SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
                                windowed = 0;
                            } else { // Currently fullscreen, switch to windowed
                                SDL_SetWindowFullscreen(window, 0); // Switch to windowed mode
                                SDL_SetWindowResizable(window, SDL_TRUE); // Explicitly make it resizable
                                // Explicitly set a window size to ensure visibility and trigger resize events
                                SDL_SetWindowSize(window, 1024, 768);
                                windowed = 1;
                            }
                            // A SDL_WINDOWEVENT_RESIZED will likely be triggered by SDL_SetWindowFullscreen or SDL_SetWindowSize,
                            // which handles screen/texture recreation and thumbnail invalidation.
                            redraw = 1; // Ensure redraw happens
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
#ifdef LOGFILE
    fclose(logfile);
#endif

    return(0);
}
