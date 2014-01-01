#if defined _WIN32 || defined _WIN64
#include "windows.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#include "SDL/SDL.h"
#include "image.h"
#include "font.h"

/* Call this instead of exit(), so we can clean up SDL: atexit() is evil. */
static void quit(int rc) {
    SDL_Quit();
    exit(rc);
}

/*
   void update_raw(JViewport & viewport, jImagePtr image, const char *modeline, bool bw) {
   SDL_Surface *surface = viewport.screen;
   Uint32 *pixels = (Uint32 *)surface->pixels;
   Uint32 mul, white, grey;

// set up pixel format specific things
mul = (1 << surface->format->Rshift) +
(1 << surface->format->Gshift) +
(1 << surface->format->Bshift);
grey = 0x80 * mul;
white = 0xFF * mul;

if ( SDL_LockSurface(surface) < 0 ) {
fprintf(stderr, "Couldn't lock the display surface: %s\n",
SDL_GetError());
quit(2);
}

for(int y=0; y<surface->h; y++) {
for(int x=0; x<surface->w; x++) {
if(!viewport.inImage(x,y)) {
pixels[y*surface->w + x] = ((x/4+y/4)&1) ? grey : 0;
continue; // just a grid
}

if(bw)
pixels[y*surface->w + x] =
image->data[viewport.getImageOffset(x,y)] ? white : 0;
else
pixels[y*surface->w + x] = mul *
image->data[viewport.getImageOffset(x,y)];
}
}

write_font_SDL(surface, font24, redGrad, modeline, surface->w/2, 4, FONT_ALIGN_TOP + FONT_ALIGN_CENTER, 3);

SDL_UnlockSurface(surface);
SDL_UpdateRect(surface, 0, 0, 0, 0);
}
*/

/*void printTime(Uint32 *time, const char * message) {
  Uint32 newtime = SDL_GetTicks();
  printf("%s %d ms\n", message, newtime - *time);
 *time = newtime;
 }*/

int main(int argc, char *argv[]) {
    SDL_Surface *screen;
    SDL_Event event;
    jImagePtr image;
    jFontPtr font12, font24;
    int done;

    if(argc < 2) {
        puts("Usage: jview2 <pictures.zip>");
        return 0;
    }

    font12 = create_font(read_PNG_file("font12.png"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.,:;!?'/&()=", 2);
    font24 = create_font(read_PNG_file("font24.png"),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-+.,:;!?'/&()=", 4);

    // initialize SDL
    if(SDL_Init(SDL_INIT_VIDEO) < 0) {
        fprintf(stderr, "Couldn't initialize SDL: %s\n",SDL_GetError());
        return(1);
    }

    if((screen=SDL_SetVideoMode(1600, 1200, 32, SDL_SWSURFACE)) == NULL ) {
        fprintf(stderr, "Couldn't set video mode: %s\n", SDL_GetError());
        quit(2);
    }

    SDL_WM_SetCaption("JView2", "JView2");
    SDL_EnableUNICODE(1);

    // main loop
    while(!done) {
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

    return(0);
}
