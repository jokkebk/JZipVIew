#if defined _WIN32 || defined _WIN64
#include "windows.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include <zlib.h>

#include "SDL/SDL.h"
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
    SDL_Surface *thumbnail;
} JPEGRecord;

JPEGRecord *jpegs;
int jpeg_count, thumbsLeft = 0;

jFontPtr font24;
Uint32 *grad;

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
    SDL_Quit();
    exit(rc);
}

void printTime(const char * message) {
    static Uint32 oldtime = 0;
    Uint32 newtime = SDL_GetTicks();

    if(oldtime > 0)
        printf("%s %d ms\n", message, newtime - oldtime);

    oldtime = newtime;
}

#define getR(i, x, y) (i->data[((y) * i->width + (x)) * i->components + 0])
#define getG(i, x, y) (i->data[((y) * i->width + (x)) * i->components + 1])
#define getB(i, x, y) (i->data[((y) * i->width + (x)) * i->components + 2])
#define rgb(r,g,b) (((r)<<16)+((g)<<8)+(b))

SDL_Surface *convert(SDL_Surface *screen, jImagePtr image) {
    Uint32 *pixels;
    int i, j;
    SDL_Surface *surface;
    SDL_PixelFormat *format = screen->format;

    surface = SDL_CreateRGBSurface(0, image->width, image->height, 32,
            format->Rmask, format->Gmask, format->Bmask, format->Amask);

    pixels = (Uint32 *)surface->pixels;

    if ( SDL_LockSurface(surface) < 0 ) {
        fprintf(stderr, "Couldn't lock the display surface: %s\n",
                SDL_GetError());
        quit(2);
    }

    for(j=0; j<image->height; j++) {
        for(i=0; i<image->width; i++) {
            pixels[j * surface->w + i] = SDL_MapRGB(screen->format,
                    getR(image, i, j), getG(image, i, j), getB(image, i, j));
        }
    }

    SDL_UnlockSurface(surface);
    SDL_UpdateRect(surface, 0, 0, 0, 0);

    return surface;
}

// fixed point scaling with bilinear filter to given max size (w/h)
SDL_Surface *scale(SDL_Surface *screen, jImagePtr image, int w, int h) {
    Uint32 *pixels;
    int i, j, xp, yp, w2, h2, xpart, ypart;
    int ox, oy, xs, ys; // 22.10 fixed point
    int r, g, b;
    SDL_Surface *surface;
    SDL_PixelFormat *format = screen->format;

    if(w * image->height > image->width * h) { // screen is wider
        xs = ys = 1024 * image->height / h;
        w2 = image->width * h / image->height;
        h2 = h;
    } else { // screen is higher
        xs = ys = 1024 * image->width / w;
        w2 = w;
        h2 = image->height * w / image->width;
    }

    //printf("Creating %d x %d surface\n", w2, h2);
    surface = SDL_CreateRGBSurface(0, w2, h2, 32,
            format->Rmask, format->Gmask, format->Bmask, format->Amask);

    pixels = (Uint32 *)surface->pixels;

    if ( SDL_LockSurface(surface) < 0 ) {
        fprintf(stderr, "Couldn't lock the display surface: %s\n",
                SDL_GetError());
        quit(2);
    }

    for(j=0, oy=0; j<h2; j++, oy+=ys) {
        for(i=0, ox=0; i<w2; i++, ox+=xs) {
            xp = ox >> 10;
            yp = oy >> 10;
            xpart = ox & 1023;
            ypart = oy & 1023;

            r = ((1024-xpart) * (1024-ypart) * getR(image, xp, yp) +
                 (xpart) * (1024-ypart) * getR(image, xp+1, yp) +
                 (1024-xpart) * (ypart) * getR(image, xp, yp+1) +
                 (xpart) * (ypart) * getR(image, xp+1, yp+1)) >> 20;
            g = ((1024-xpart) * (1024-ypart) * getG(image, xp, yp) +
                 (xpart) * (1024-ypart) * getG(image, xp+1, yp) +
                 (1024-xpart) * (ypart) * getG(image, xp, yp+1) +
                 (xpart) * (ypart) * getG(image, xp+1, yp+1)) >> 20;
            b = ((1024-xpart) * (1024-ypart) * getB(image, xp, yp) +
                 (xpart) * (1024-ypart) * getB(image, xp+1, yp) +
                 (1024-xpart) * (ypart) * getB(image, xp, yp+1) +
                 (xpart) * (ypart) * getB(image, xp+1, yp+1)) >> 20;

            pixels[j * surface->w + i] = rgb(r,g,b);
        }
    }

    SDL_UnlockSurface(surface);
    SDL_UpdateRect(surface, 0, 0, 0, 0);

    return surface;
}

jImagePtr read_JPEG_custom(unsigned char *inbuffer, unsigned long insize,
        int tx, int ty) {
    struct jpeg_decompress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPARRAY buffer;      /* Output row buffer */
    int row_stride;     /* physical row width in output buffer */
    jImagePtr image;

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

    //printf("Scale %d JPEG size %d x %d, output size %d x %d (%d x %d)\n",
    //        cinfo.scale_num,
    //        cinfo.image_width, cinfo.image_height,
    //        cinfo.output_width, cinfo.output_height, tx, ty);

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

    return image;
}

SDL_Surface *loadImageFromZip(FILE *zip, JPEGRecord *jpeg, SDL_Surface *screen, int tx, int ty) {
    JZFileHeader header;
    char filename[1024];
    jImagePtr image = NULL;
    SDL_Surface *surface = NULL;

    fseek(zip, jpeg->offset, SEEK_SET);

    if(jzReadLocalFileHeader(zip, &header, filename, sizeof(filename))) {
        fprintf(stderr, "Couldn't read local file header!");
        quit(1);
    }

    if((jpeg->data = (unsigned char *)malloc(jpeg->size)) == NULL) {
        fprintf(stderr, "Couldn't allocate memory!");
        quit(1);
    }

    if(jzReadData(zip, &header, jpeg->data) == Z_OK) {
        image = read_JPEG_custom(jpeg->data, jpeg->size, tx, ty);
        if(tx && ty)
            surface = scale(screen, image,
                    tx ? tx : image->width, ty ? ty : image->height);
        else
            surface = convert(screen, image);
        destroy_image(image);
    }

    return surface;
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
        fprintf(stderr, "Couldn't allocate space for filename!\n");
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
        printf("Couldn't read ZIP file end record.");
        return -1;
    }

    jpegs = (JPEGRecord *)malloc(sizeof(JPEGRecord) * endRecord.numEntries);
    jpeg_count = 0;

    if(jzReadCentralDirectory(zip, &endRecord, recordCallback)) {
        printf("Couldn't read ZIP file central record.");
        return -1;
    }

    return 0;
}

void drawImage(SDL_Surface *screen, SDL_Surface *surface, int xoff, int yoff) {
    SDL_Rect srect, drect;

    srect.w = surface->w;
    srect.h = surface->h;

    if(screen->w > surface->w) { // center if fits
        drect.x = (screen->w - surface->w) / 2;
        srect.x = 0;
    } else {
        drect.x = 0;
        srect.x = xoff;
        srect.w -= xoff;
    }

    if(screen->h > surface->h) { // center if fits
        drect.y = (screen->h - surface->h) / 2;
        srect.y = 0;
    } else {
        drect.y = 0;
        srect.y = yoff;
        srect.h -= yoff;
    }

    //printf("Drawing %d x %d pixels from (%d, %d) to (%d, %d)\n",
    //        srect.w, srect.h, srect.x, srect.y, drect.x, drect.y);

    if(drect.x || drect.y)
        SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, 0, 0, 0));
    SDL_BlitSurface(surface, &srect, screen, &drect);
    SDL_UpdateRect(screen, 0, 0, 0, 0);
}

void drawThumbs(SDL_Surface *screen, jFontPtr font, Uint32 *grad, int tx, int ty, int topleft) {
    SDL_Rect src, dest;
    SDL_Surface *thumb;
    int tw = screen->w / tx, th = screen->h / ty;
    int i, j, idx;
    char num[6];

    SDL_FillRect(screen, NULL, SDL_MapRGB(screen->format, thumbsLeft ? 128 : 0, 0, 0));

    for(j = 0; j < ty; j++) {
        for(i = 0; i < tx; i++) {
            idx = topleft + j * tx + i;

            if(idx >= jpeg_count)
                break; // done

            if((thumb = jpegs[idx].thumbnail)) {
                dest.x = tw * i;
                dest.y = th * j;
                src.y = src.x = 0;
                src.w = dest.w = thumb->w;
                src.h = dest.h = thumb->h;
                SDL_BlitSurface(thumb, NULL, screen, &dest);
            } else {
                if ( SDL_LockSurface(screen) < 0 ) {
                    fprintf(stderr, "Couldn't lock the display surface: %s\n",
                            SDL_GetError());
                    quit(2);
                }

                sprintf(num, "%d", idx + 1);
                write_font_SDL(screen, font, grad, num,
                        tw * i + tw / 2,
                        th * j + th / 2,
                        FONT_ALIGN_MIDDLE + FONT_ALIGN_CENTER, 2);

                SDL_UnlockSurface(screen);
            }
        }
    }

    SDL_UpdateRect(screen, 0, 0, 0, 0);
}

int main(int argc, char *argv[]) {
    char fontname[1024];
    FILE *zip;
    SDL_Surface *screen;
    JPEGRecord *jpeg;
    SDL_Event event;
    int done = 0, redraw = 1, tx = 8, ty = 5, i, xoff = 0, yoff = 0,
        currentImage = 0, loadedFullscreen = -1, loadedFullsize = -1;
    SDL_Surface *fullscreen = NULL, *fullsize = NULL;
    enum { MODE_THUMBS, MODE_FULLSCREEN, MODE_FULLSIZE } mode = MODE_THUMBS;

    if(argc < 2) {
        puts("Usage: jview2 <pictures.zip>");
        return 0;
    }

    if(strlen(argv[0]) > 1000) {
        puts("Where are you invoking this?");
        return 0;
    }

    if(!(zip = fopen(argv[1], "rb"))) {
        printf("Couldn't open \"%s\"!", argv[1]);
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

    puts("1"); fflush(stdout);
    // initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
        return(1);
    }

    processZip(zip);
    thumbsLeft = jpeg_count;

    if((screen=SDL_SetVideoMode(0, 0, 32, SDL_FULLSCREEN + SDL_SWSURFACE)) == NULL ) {
    //if((screen=SDL_SetVideoMode(1600, 1200, 32, SDL_SWSURFACE)) == NULL ) {
        fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
        quit(2);
    }

    tx = screen->w / THUMB_W;
    ty = screen->h / THUMB_H;

    SDL_WM_SetCaption("JView2", "JView2");
    SDL_EnableUNICODE(1);

    grad = create_gradient(screen, 255, 255, 255);

    // main loop
    while(!done) {
        if(mode == MODE_FULLSCREEN && loadedFullscreen != currentImage) {
            if(fullscreen != NULL)
                SDL_FreeSurface(fullscreen);
            fullscreen = loadImageFromZip(zip, jpegs+currentImage, screen,
                    screen->w, screen->h);
            loadedFullscreen = currentImage;
        } else if(mode == MODE_FULLSIZE && loadedFullsize != currentImage) {
            if(fullsize != NULL)
                SDL_FreeSurface(fullsize);
            fullsize = loadImageFromZip(zip, jpegs+currentImage, screen, 0, 0);
            loadedFullsize = currentImage;
        } else if(thumbsLeft) {
            for(i = 0; i < jpeg_count; i++) {
                jpeg = &jpegs[(currentImage + i) % jpeg_count];
                if(jpeg->thumbnail != NULL) continue;
                jpeg->thumbnail = loadImageFromZip(zip, jpeg, screen,
                        screen->w / tx, screen->h / ty);
                thumbsLeft--;
                redraw = 1;
                break;
            }
        }

        if(redraw) {
            switch(mode) {
                case MODE_THUMBS:
                    drawThumbs(screen, font24, grad, tx, ty, currentImage);
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
                        case SDL_BUTTON_WHEELUP:
                            if(mode == MODE_THUMBS) {
                                currentImage -= tx;
                                if(currentImage < 0)
                                    currentImage = 0;
                            } else {
                                if(currentImage)
                                    currentImage--;
                            }
                            redraw = 1;
                            break;
                        case SDL_BUTTON_WHEELDOWN:
                            if(mode == MODE_THUMBS) {
                                if(currentImage + tx * ty >= jpeg_count)
                                    break;
                                currentImage += tx;
                            } else {
                                if(++currentImage >= jpeg_count)
                                    currentImage = jpeg_count - 1;
                            }
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

                case SDL_KEYDOWN:
                    //printf("Key down: '%c' (%d)\n", event.key.keysym.unicode, event.key.keysym.unicode);
                    //More examples at jpeg2sgf source main.cc
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE:
                        case SDLK_SPACE:
                        case SDLK_LEFT:
                        case SDLK_RIGHT:
                        case SDLK_UP:
                        case SDLK_DOWN:
                        case SDLK_q:
                        case SDLK_RETURN:
                        case SDLK_BACKSPACE:
                        case SDL_QUIT:
                        default:
                            done = 1;
                            break;
                    } // end switch(event.key.keysym.sym)
                    break;
            } // end switch(event.type)
        } // end while(SDL_PollEvent(&event))
    } // end while(!done)

    destroy_gradient(grad);

    SDL_Quit();

    destroy_font(font24);

    fclose(zip);

    return(0);
}
