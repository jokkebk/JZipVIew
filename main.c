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

// fixed point scaling with bilinear filter
void scale(SDL_Surface *surface, jImagePtr image, int x, int y, int w, int h) {
    Uint32 *pixels = (Uint32 *)surface->pixels;
    int i, j, xp, yp, w2, h2, xpart, ypart;
    int ox, oy, xs, ys; // 22.10 fixed point
    int r, g, b;

    if(w * image->height > image->width * h) { // screen is wider
        xs = ys = 1024 * image->height / h;
        w2 = image->width * h / image->height;
        h2 = h;
        x += (w - w2) / 2;
    } else { // screen is higher
        xs = ys = 1024 * image->width / w;
        w2 = w;
        h2 = image->height * w / image->width;
        y += (h - h2) / 2;
    }

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

            pixels[(y+j)*surface->w + i + x] = rgb(r,g,b);
            //(i+j & 1) ? 0xFFFFFF : 0;
        }
    }

    //write_font_SDL(surface, font24, redGrad, modeline, surface->w/2, 4, FONT_ALIGN_TOP + FONT_ALIGN_CENTER, 3);

    SDL_UnlockSurface(surface);
    SDL_UpdateRect(surface, 0, 0, 0, 0);
}

jImagePtr readZip(FILE *zip) {
    JZFileHeader header;
    char filename[1024];
    unsigned char *data;
    jImagePtr image = NULL;

    if(jzReadLocalFileHeader(zip, &header, filename, sizeof(filename))) {
        printf("Couldn't read local file header!");
        return NULL;
    }

    if((data = (unsigned char *)malloc(header.uncompressedSize)) == NULL) {
        printf("Couldn't allocate memory!");
        return NULL;
    }

    if(jzReadData(zip, &header, data) == Z_OK)
        image = read_JPEG_buffer(data, header.uncompressedSize);

    free(data);

    return image;
}

int main(int argc, char *argv[]) {
    FILE *zip;
    SDL_Surface *screen;
    SDL_Event event;
    jImagePtr image = NULL;
    jFontPtr font12, font24;
    int done = 0, tx = 8, ty = 5, i = 0;

    if(argc < 2) {
        puts("Usage: jview2 <pictures.zip>");
        return 0;
    }

    zip = fopen(argv[1], "rb");

    font12 = create_font(read_PNG_file("font12.png"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.,:;!?'/&()=", 2);
    font24 = create_font(read_PNG_file("font24.png"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.,:;!?'/&()=", 4);

    // initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
        return(1);
    }

    //if((screen=SDL_SetVideoMode(0, 0, 32, SDL_FULLSCREEN + SDL_SWSURFACE)) == NULL ) {
    if((screen=SDL_SetVideoMode(1600, 1200, 32, SDL_SWSURFACE)) == NULL ) {
        fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
        quit(2);
    }

    tx = screen->w / 400;
    ty = screen->h / 400;

    SDL_WM_SetCaption("JView2", "JView2");
    SDL_EnableUNICODE(1);

    printTime("START");

    // main loop
    while(!done) {
        if(i < tx * ty) {
            if(image)
                destroy_image(image);

            if((image = readZip(zip)) == NULL) {
                i = tx * ty; // stop
                printTime("END");
                continue;
            }
            scale(screen, image,
                    screen->w / tx * (i % tx),
                    screen->h / ty * (i / tx),
                    screen->w / tx, screen->h / ty);
            i++;
            if(i == tx * ty) printTime("END");
        }

        while(SDL_PollEvent(&event)) {
            switch(event.type) {
                case SDL_MOUSEBUTTONDOWN:
                    switch(event.button.button) {
                        case SDL_BUTTON_LEFT:
                            break;
                        case SDL_BUTTON_WHEELUP:
                            break;
                        case SDL_BUTTON_WHEELDOWN:
                            break;
                    }
                    break;

                case SDL_MOUSEBUTTONUP:
                    break;
                case SDL_MOUSEMOTION:
                    break;

                case SDL_KEYDOWN:
                    //printf("Key down: '%c' (%d)\n", event.key.keysym.unicode, event.key.keysym.unicode);
                    //More examples at jpeg2sgf source main.cc
                    switch(event.key.keysym.sym) {
                        case SDLK_ESCAPE: // escape
                        case SDLK_SPACE: // toggle view
                        case SDLK_LEFT: // previous area
                        case SDLK_RIGHT: // next area
                        case SDLK_UP: // previous stone
                        case SDLK_DOWN: // next stone
                        case SDLK_q:
                        case SDLK_RETURN: // save problem and proceed
                        case SDLK_BACKSPACE: // skip problem and proceed
                        case SDL_QUIT:
                        default:
                            done = 1;
                            break;
                    } // end switch(event.key.keysym.sym)
            } // end switch(event.type)
        } // end while(SDL_PollEvent(&event))
    } // end while(!done)

    SDL_Quit();

    destroy_font(font12);
    destroy_font(font24);

    fclose(zip);

    return(0);
}
