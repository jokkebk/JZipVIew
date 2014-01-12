/**
 * Font drawing routines.
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
