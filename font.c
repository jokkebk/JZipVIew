#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "font.h"

jImagePtr make_letter(jImagePtr font, int channel, int x, int y, int w, int h, void ** mempool) {
    //printf("%d,%d -> %d x %d\n", x, y, w, h);
    int i, j, offset = 0;
    jImagePtr image = (jImagePtr)(*mempool);
    (*mempool) += sizeof(struct jImage);

    image->width = w;
    image->height = h;
    image->components = 1;
    image->data = (unsigned char *)(*mempool);
    (*mempool) += w*h;

    for(j=0; j<h; j++)
        for(i=0; i<w; i++)
            image->data[offset++] = font->data[((y + j) * font->width + x + i) * font->components + channel];

    return image;
}

jFontPtr create_font(jImagePtr font, const char * letter_list, int space_width) {
    int top, bottom, i, j;
    int left[128], right[128], in_letter, letters, total_width;

    // starting from font on white background, we greyscale and invert
    greyscale_image(font);
    invert_image(font);

    //printf("Initializing font: %d x %d, %d bpp\n", font->width, font->height, font->components * 8);

    for(top=0; top<font->height; top++) {
        // if the whole row is invisible, we get to the end of the row
        for(i=0; i<font->width && !font->data[(top * font->width + i) * font->components]; i++) {}

        if(i < font->width)
            break; // there was a visible pixel
    }

    for(bottom = font->height - 1; bottom >= 0; bottom--) {
        // if the whole row is invisible, we get to the end of the row
        for(i=0; i<font->width && !font->data[(bottom * font->width + i) * font->components]; i++) {}

        if(i < font->width)
            break; // there was a visible pixel
    }

    //printf("Fonts use rows [%d, %d] out of [0, %d]\n", top, bottom, font->height-1);

    letters = 0; in_letter = 0; total_width = 0;
    for(i=0; i<font->width && letters < 128; i++) {
        for(j=0; j<font->height && !font->data[(j * font->width + i) * font->components]; j++) {}

        if(in_letter && j == font->height) { // letter just ended
            total_width += i - left[letters]; // increase total width counter
            right[letters++] = i-1;
            in_letter = 0;
            //printf("Closed letter %d (%c)\n", letters-1, letter_list[letters-1]);
        } else if(!in_letter && j < font->height) { // letter just started
            left[letters] = i;
            in_letter = 1;
        }
    }

    if(letters != (int)strlen(letter_list)) {
        printf("Invalid amount of letters: %d found, %d expected!\n", letters, strlen(letter_list));
        return NULL;
    }

    int buffer_size = (bottom - top + 1 ) * total_width + // font alpha data
        sizeof(struct jImage) * letters + // letter jImage headers
        sizeof(struct jFont) + letters * sizeof(jImagePtr); // jFont structure and letter pointers
    void * letter_buffer = malloc(buffer_size);
    jFontPtr fontPtr;

    if(letter_buffer == NULL) {
        puts("Failed.");
        return NULL;
    }

    fontPtr = letter_buffer;
    letter_buffer += sizeof(struct jFont);

    fontPtr->space_width = space_width;

    fontPtr->letter = letter_buffer;
    letter_buffer += sizeof(jImagePtr) * letters;

    for(i=0; i<(int)sizeof(fontPtr->convert); i++)
        fontPtr->convert[i] = -1;

    for(i=0; i<letters; i++) {
        //printf("Letter %d (%c/ %d), [%d, %d]: %d x %d\n", i, letter_list[i], letter_list[i], left[i], top, right[i]-left[i]+1, bottom-top+1);
        fontPtr->convert[(int)letter_list[i]] = i;
        fontPtr->letter[i] = make_letter(font, 0, left[i], top, right[i]-left[i]+1, bottom-top+1, &letter_buffer);
    }

    //printf("%d/%d bytes of buffer used\n", (int)(letter_buffer - (void *)fontPtr), buffer_size);

    destroy_image(font);
    return fontPtr;
}

void destroy_font(jFontPtr fontPtr) {
    free(fontPtr);
}

void write_font_SDL(SDL_Surface *s, jFontPtr font, Uint32 *grad, const char *message, int x, int y, int align, int spacing) {
    int total_width = 0, len = strlen(message), i, ch, n;

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) // space and unknown characters
            total_width += font->space_width;
        else
            total_width += font->letter[n]->width;
    }
    total_width += spacing * (len-1);

    if((align & 0x0F) == FONT_ALIGN_MIDDLE)
        y -= font->letter[0]->height / 2;
    else if((align & 0x0F) == FONT_ALIGN_BOTTOM)
        y -= font->letter[0]->height;

    if((align & 0xF0) == FONT_ALIGN_CENTER)
        x -= total_width / 2;
    else if((align & 0xF0) == FONT_ALIGN_RIGHT)
        x -= total_width;

    //printf("%d x %d text at (%d, %d)\n", total_width, font->letter[0]->height, x, y); fflush(stdout);

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) { // space and unknown characters
            x += font->space_width + spacing;
        } else  {
            blit_font_SDL(s, font->letter[n], grad, x, y);
            x += font->letter[n]->width + spacing;
        }
    }
}

// copy of above function with 1 different line
void write_font(jImagePtr image, jFontPtr font, int c, const char *message, int x, int y, int align, int spacing) {
    int total_width = 0, len = strlen(message), i, ch, n;

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) // space and unknown characters
            total_width += font->space_width;
        else
            total_width += font->letter[n]->width;
    }
    total_width += spacing * (len-1);

    if((align & 0x0F) == FONT_ALIGN_MIDDLE)
        y -= font->letter[0]->height / 2;
    else if((align & 0x0F) == FONT_ALIGN_BOTTOM)
        y -= font->letter[0]->height;

    if((align & 0xF0) == FONT_ALIGN_CENTER)
        x -= total_width / 2;
    else if((align & 0xF0) == FONT_ALIGN_RIGHT)
        x -= total_width;

    //printf("%d x %d text at (%d, %d)\n", total_width, font->letter[0]->height, x, y);

    for(i=0; i<len; i++) {
        if((ch = message[i]) == 32 || (n = font->convert[ch]) == -1) { // space and unknown characters
            x += font->space_width + spacing;
        } else  {
            blit_font(image, font->letter[n], x, y, c);
            x += font->letter[n]->width + spacing;
        }
    }
}
