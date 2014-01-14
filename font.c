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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"

JImage *make_letter(JImage *font, int x, int y, int w, int h, void ** mempool) {
    int i, j, offset = 0;
    JImage *image = (JImage *)(*mempool);
    *mempool += sizeof(JImage);

    image->w = w;
    image->h = h;
    image->data = (Uint32 *)(*mempool);
    *mempool += w*h * sizeof(Uint32);

    //printf("Making %d x %d letter at %ld\n", w, h, image->data);

    for(j=0; j<h; j++)
        for(i=0; i<w; i++)
            image->data[offset++] = GETPIXEL(font, x+i, y+j);

    return image;
}

JFont *create_font(JImage *font, const char * letter_list, int space_width) {
    int top, bottom, i, j;
    int left[128], right[128], in_letter, letters, total_width;

    // starting from font on white background, we greyscale and invert
    greyscale_image(font);
    invert_image(font);

    for(top = 0; top < font->h; top++) {
        // if the whole row is invisible, we get to the end of the row
        for(i=0; i<font->w && !GETPIXEL(font, i, top); i++) {}

        if(i < font->w)
            break; // there was a visible pixel
    }

    for(bottom = font->h - 1; bottom >= 0; bottom--) {
        // if the whole row is invisible, we get to the end of the row
        for(i=0; i<font->w && !GETPIXEL(font, i, bottom); i++) {}

        if(i < font->w)
            break; // there was a visible pixel
    }

    letters = 0; in_letter = 0; total_width = 0;
    for(i=0; i<font->w && letters < 128; i++) {
        for(j=0; j<font->h && !GETPIXEL(font, i, j); j++) {}

        if(in_letter && j == font->h) { // letter just ended
            total_width += i - left[letters]; // increase total width counter
            right[letters++] = i-1;
            in_letter = 0;
        } else if(!in_letter && j < font->h) { // letter just started
            left[letters] = i;
            in_letter = 1;
        }
    }

    if(letters != (int)strlen(letter_list)) {
        printf("Invalid amount of letters: %d found, %d expected!\n", (int)letters, (int)strlen(letter_list));
        return NULL;
    }

    int buffer_size = (bottom - top + 1 ) * total_width * sizeof(Uint32) + // font alpha data
        sizeof(JImage) * letters + // letter jImage headers
        sizeof(JFont) + letters * sizeof(JImage *); // jFont structure and letter pointers
    void * letter_buffer = malloc(buffer_size);
    JFont *fontPtr;

    if(letter_buffer == NULL) {
        puts("Failed.");
        return NULL;
    }

    fontPtr = letter_buffer;
    letter_buffer += sizeof(JFont);

    fontPtr->space_width = space_width;

    fontPtr->letter = letter_buffer;
    letter_buffer += sizeof(JImage *) * letters;

    for(i=0; i<(int)sizeof(fontPtr->convert); i++)
        fontPtr->convert[i] = -1;

    for(i=0; i<letters; i++) {
        fontPtr->convert[(int)letter_list[i]] = i;
        fontPtr->letter[i] = make_letter(font, left[i], top, right[i]-left[i]+1, bottom-top+1, &letter_buffer);
    }

    destroy_image(font);
    return fontPtr;
}

void destroy_font(JFont *fontPtr) {
    free(fontPtr);
}

// copy of above function with 1 different line
void write_font(JImage *image, JFont *font, Uint32 c, const char *message, int x, int y, int align, int spacing) {
    int total_width = 0, len = strlen(message), i, ch, n;

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) // space and unknown characters
            total_width += font->space_width;
        else
            total_width += font->letter[n]->w;
    }
    total_width += spacing * (len-1);

    if((align & 0x0F) == FONT_ALIGN_MIDDLE)
        y -= font->letter[0]->h / 2;
    else if((align & 0x0F) == FONT_ALIGN_BOTTOM)
        y -= font->letter[0]->h;

    if((align & 0xF0) == FONT_ALIGN_CENTER)
        x -= total_width / 2;
    else if((align & 0xF0) == FONT_ALIGN_RIGHT)
        x -= total_width;

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) { // space and unknown characters
            x += font->space_width + spacing;
        } else  {
            blit_font(image, font->letter[n], x, y, c);
            x += font->letter[n]->w + spacing;
        }
    }
}
