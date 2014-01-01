#ifndef __FONT_H
#define __FONT_H

#include <stdio.h>
#include <stdlib.h>

#include "SDL/SDL.h"

#include "image.h"

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

#define FONT_ALIGN_TOP 0x00
#define FONT_ALIGN_MIDDLE 0x01
#define FONT_ALIGN_BOTTOM 0x02

#define FONT_ALIGN_LEFT 0x00
#define FONT_ALIGN_CENTER 0x10
#define FONT_ALIGN_RIGHT 0x20

struct jFont {
	jImagePtr *letter;
	char convert[128];
	int letter_spacing;
	int space_width;
};

typedef struct jFont * jFontPtr;

jFontPtr create_font(jImagePtr font, const char * letter_list, int space_width);

void destroy_font(jFontPtr fontPtr);

// Write text in given color gradient (a 256-element precalc of given color shades)
void write_font_SDL(SDL_Surface *s, jFontPtr font, Uint32 * grad, const char *message, int x, int y, int align, int spacing);
void write_font(jImagePtr image, jFontPtr font, int c, const char *message, int x, int y, int align, int spacing);

#ifdef __cplusplus
}
#endif // __cplusplus

#endif
