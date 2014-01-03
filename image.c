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

// halves vertical and horizontal resolution - doesn't reallocate (it's for pussies)
void downscale_image(jImagePtr img) {
    int x, y, k;

    for(y=0; y<img->height/2; y++) {
        for(x=0; x<img->width/2; x++) {
            for(k=0; k<img->components; k++) {
                img->data[(y * img->width/2 + x) * img->components + k] = (
                    img->data[((y*2+0) * img->width + (x*2+0)) * img->components + k] +
                    img->data[((y*2+0) * img->width + (x*2+1)) * img->components + k] +
                    img->data[((y*2+1) * img->width + (x*2+0)) * img->components + k] +
                    img->data[((y*2+1) * img->width + (x*2+1)) * img->components + k] ) / 4;
            }
        }
    }

    img->width /= 2;
    img->height /= 2;
}

void fill_image(jImagePtr img, int r, int g, int b) {
    int i, j, pos;

    for(j=0, pos=0; j<img->height; j++) {
        for(i=0; i<img->width; i++) {
            img->data[pos+0] = r;
            img->data[pos+1] = g;
            img->data[pos+2] = b;
            pos += img->components;
        }
    }
}

jImagePtr convert_to_greyscale(jImagePtr image) {
    jImagePtr new = create_image(image->width, image->height, 1);
    int x, y, c, sum;

    for(y=0; y<image->height; y++) {
        for(x=0; x<image->width; x++) {
            for(c=0, sum=0; c<image->components; c++)
                sum += image->data[(y*image->width+x)*image->components + c];
            new->data[y*image->width+x] = sum / image->components;
        }
    }

    return new;
}

jImagePtr duplicate_image(jImagePtr image) {
    jImagePtr new = create_image(image->width, image->height, image->components);
    copy_image(new, image);
    return new;
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

// all zero to b, other to w, works only for 8-bit images (1 component)
void copy_image_mono(jImagePtr dest, jImagePtr src, int b, int w) {
    int pos;

    if(dest->width != src->width || dest->height != src->height || dest->components != 1 || src->components != 1)
        return;

    for(pos = 0; pos < src->width * src->height; pos++)
        dest->data[pos] = src->data[pos] ? w : b;
}

void half_alpha(jImagePtr img) {
    int pos;

    if(img->components != 4)
        return;

    for(pos = 3; pos < img->width * img->height * img->components; pos += img->components)
        img->data[pos] >>= 1;
}

void destroy_image(jImagePtr img) {
    free(img->data);
    free(img);
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

void auto_levels(jImagePtr image) {
    int i, min = 255, max = 0;

    for(i = 0; i < image->height * image->width; i++) {
        min = MIN(min, image->data[i]);
        max = MAX(max, image->data[i]);
    }

    for(i = 0; i < image->height * image->width; i++)
        image->data[i] = (image->data[i] - min) * 255 / (max-min);
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

void write_JPEG_file (const char * filename, int quality, jImagePtr image) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    FILE * outfile;     /* target file */
    JSAMPROW row_pointer[1];    /* pointer to JSAMPLE row[s] */
    int row_stride;     /* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    if ((outfile = fopen(filename, "wb")) == NULL) {
        fprintf(stderr, "can't open %s\n", filename);
        exit(1);
    }
    jpeg_stdio_dest(&cinfo, outfile);

    cinfo.image_width = image->width;   /* image width and height, in pixels */
    cinfo.image_height = image->height;
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;     /* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image->width * 3;  /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = & image->data[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    fclose(outfile);

    jpeg_destroy_compress(&cinfo);
}

void write_JPEG_buffer (unsigned char ** buffer, unsigned long * size, int quality, jImagePtr image) {
    struct jpeg_compress_struct cinfo;
    struct jpeg_error_mgr jerr;

    JSAMPROW row_pointer[1];    /* pointer to JSAMPLE row[s] */
    int row_stride;     /* physical row width in image buffer */

    cinfo.err = jpeg_std_error(&jerr);

    jpeg_create_compress(&cinfo);

    jpeg_mem_dest(&cinfo, buffer, size);

    cinfo.image_width = image->width;   /* image width and height, in pixels */
    cinfo.image_height = image->height;
    cinfo.input_components = 3;     /* # of color components per pixel */
    cinfo.in_color_space = JCS_RGB;     /* colorspace of input image */

    jpeg_set_defaults(&cinfo);

    jpeg_set_quality(&cinfo, quality, TRUE /* limit to baseline-JPEG values */);

    jpeg_start_compress(&cinfo, TRUE);

    row_stride = image->width * 3;  /* JSAMPLEs per row in image_buffer */

    while (cinfo.next_scanline < cinfo.image_height) {
        row_pointer[0] = & image->data[cinfo.next_scanline * row_stride];
        (void) jpeg_write_scanlines(&cinfo, row_pointer, 1);
    }

    jpeg_finish_compress(&cinfo);

    jpeg_destroy_compress(&cinfo);
}

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

jImagePtr read_JPEG_buffer(unsigned char *inbuffer, unsigned long insize) {
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

int write_PNG_file(const char *filename, jImagePtr image) {
    FILE *fp;
    png_structp png_ptr;
    png_infop info_ptr;

    if(image->components != 3 && image->components != 4) // only RGB and RGBA currently supported
        return 0;

    fp = fopen(filename, "wb");
    if (fp == NULL)
        return 0;

    png_ptr = png_create_write_struct(PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);

    if (png_ptr == NULL) {
        fclose(fp);
        return 0;
    }

    info_ptr = png_create_info_struct(png_ptr);
    if (info_ptr == NULL) {
        fclose(fp);
        png_destroy_write_struct(&png_ptr,  NULL);
        return 0;
    }

    if (setjmp(png_jmpbuf(png_ptr))) {
        // If we get here, we had a problem writing the file
        fclose(fp);
        png_destroy_write_struct(&png_ptr, &info_ptr);
        return 0;
    }

    // Set image attributes.
    png_set_IHDR(png_ptr, info_ptr, image->width, image->height, 8,
        image->components == 3 ? PNG_COLOR_TYPE_RGB : PNG_COLOR_TYPE_RGBA,
        PNG_INTERLACE_NONE,
        PNG_COMPRESSION_TYPE_DEFAULT,
        PNG_FILTER_TYPE_DEFAULT);

    //png_set_compression_level(png_ptr, 6); // 6 should be almost as good as 9

    int i;
    png_byte ** row_pointers;

    // Initialize rows of PNG.
    row_pointers = png_malloc(png_ptr, image->height * sizeof(png_byte *));
    for (i = 0; i < image->height; i++)
        row_pointers[i] = (png_byte *)(image->data + i * image->width * image->components);

    png_init_io(png_ptr, fp);
    png_set_rows(png_ptr, info_ptr, row_pointers);
    png_write_png(png_ptr, info_ptr, PNG_TRANSFORM_IDENTITY, NULL);

    png_free(png_ptr, row_pointers); // free row pointers

    png_destroy_write_struct(&png_ptr, &info_ptr);
    fclose(fp);

    return 1;
}

#endif // USE_PNG
