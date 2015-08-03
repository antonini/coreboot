/*
 * This file is part of the libpayload project.
 *
 * Copyright (C) 2015 Google, Inc.
 */

#include <libpayload.h>
#include <sysinfo.h>
#include "bitmap.h"

/*
 * 'canvas' is the drawing area located in the center of the screen. It's a
 * square area, stretching vertically to the edges of the screen, leaving
 * non-drawing areas on the left and right. The screen is assumed to be
 * landscape.
 */
static struct vector canvas;
static uint32_t canvas_offset;		/* horizontal position of canvas */

/*
 * Framebuffer is assumed to assign a higher coordinate (larger x, y) to
 * a higher address
 */
static struct cb_framebuffer *fbinfo;
static uint8_t *fbaddr;

static char initialized = 0;
#define LOG(x...)	printf("CBGFX: " x)

/*
 * This is the range used internally to scale bitmap images. (e.g. 128 = 50%,
 * 512 = 200%). We choose 256 so that division and multiplication become bit
 * shift operation.
 */
#define BITMAP_SCALE_BASE	256

#define ROUNDUP(x, y)	((x) + ((y) - ((x) % (y))))
#define ABS(x)		((x) < 0 ? -(x) : (x))

static void add_vectors(struct vector *out,
			const struct vector *v1, const struct vector *v2)
{
	out->x = v1->x + v2->x;
	out->y = v1->y + v2->y;
}

static void scale_vector(struct vector *out, const struct vector *in,
			 size_t scale, size_t base)
{
	out->x = in->x * scale / base;
	out->y = in->y * scale / base;
}

static void to_canvas(const struct vector *relative, struct vector *absolute)
{
	absolute->x = canvas.width * relative->x / CANVAS_SCALE;
	absolute->y = canvas.height * relative->y / CANVAS_SCALE;
}

/*
 * Returns 1 if exclusively within canvas, or 0 if inclusively within canvas.
 */
static int within_canvas(const struct vector *v)
{
	if (v->x < canvas.width && v->y < canvas.height)
		return 1;
	else if (v->x <= canvas.width && v->y <= canvas.height)
		return 0;
	else
		return -1;
}

static inline uint32_t calculate_color(const struct rgb_color *rgb)
{
	uint32_t color = 0;
	color |= (rgb->red >> (8 - fbinfo->red_mask_size))
		<< fbinfo->red_mask_pos;
	color |= (rgb->green >> (8 - fbinfo->green_mask_size))
		<< fbinfo->green_mask_pos;
	color |= (rgb->blue >> (8 - fbinfo->blue_mask_size))
		<< fbinfo->blue_mask_pos;
	return color;
}

/*
 * Plot a pixel in a framebuffer. This is called from tight loops. Keep it slim
 * and do the validation at callers' site.
 */
static inline void set_pixel(struct vector *coord, uint32_t color)
{
	const int bpp = fbinfo->bits_per_pixel;
	int i;
	uint8_t * const pixel = fbaddr + (coord->x + canvas_offset +
			coord->y * fbinfo->x_resolution) * bpp / 8;
	for (i = 0; i < bpp / 8; i++)
		pixel[i] = (color >> (i * 8));
}

/*
 * Initializes the library. Automatically called by APIs. It sets up
 * the canvas and the framebuffer.
 */
static int cbgfx_init(void)
{
	if (initialized)
		return 0;

	fbinfo = lib_sysinfo.framebuffer;
	if (!fbinfo)
		return -1;

	fbaddr = phys_to_virt((uint8_t *)(uintptr_t)(fbinfo->physical_address));
	if (!fbaddr)
		return -1;

	/* calculate canvas size, assuming the screen is landscape */
	canvas.height = fbinfo->y_resolution;
	canvas.width = canvas.height;
	canvas_offset = (fbinfo->x_resolution - canvas.width) / 2;
	if (canvas_offset < 0) {
		LOG("Portrait screens are not supported\n");
		return -1;
	}

	initialized = 1;
	LOG("cbgfx initialized: canvas width=%d, height=%d, offset=%d\n",
	    canvas.width, canvas.height, canvas_offset);

	return 0;
}

int draw_box(const struct vector *top_left_rel,
	     const struct vector *size_rel,
	     const struct rgb_color *rgb)
{
	struct vector top_left;
	struct vector size;
	struct vector p, t;
	const uint32_t color = calculate_color(rgb);

	if (cbgfx_init())
		return CBGFX_ERROR_INIT;

	to_canvas(top_left_rel, &top_left);
	to_canvas(size_rel, &size);
	add_vectors(&t, &top_left, &size);
	if (within_canvas(&t) < 0) {
		LOG("Box exceeds canvas boundary\n");
		return CBGFX_ERROR_BOUNDARY;
	}

	for (p.y = top_left.y; p.y < t.y; p.y++)
		for (p.x = top_left.x; p.x < t.x; p.x++)
			set_pixel(&p, color);

	return CBGFX_SUCCESS;
}

int clear_canvas(struct rgb_color *rgb)
{
	const struct vector coord = {
		.x = 0,
		.y = 0,
	};
	const struct vector size = {
		.width = CANVAS_SCALE,
		.height = CANVAS_SCALE,
	};

	if (cbgfx_init())
		return CBGFX_ERROR_INIT;

	return draw_box(&coord, &size, rgb);
}

static int draw_bitmap_v3(const struct vector *top_left,
			  size_t scale,
			  const struct vector *image,
			  const struct bitmap_header_v3 *header,
			  const struct bitmap_palette_element_v3 *palette,
			  const uint8_t *pixel_array)
{
	const int bpp = header->bits_per_pixel;
	int32_t dir;
	struct vector p;

	if (header->compression) {
		LOG("Compressed bitmaps are not supported\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	if (bpp >= 16) {
		LOG("Non-palette bitmaps are not supported\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	if (bpp != 8) {
		LOG("Unsupported bits per pixel: %d\n", bpp);
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	if (scale == 0) {
		LOG("Scaling out of range\n");
		return CBGFX_ERROR_SCALE_OUT_OF_RANGE;
	}

	const int32_t y_stride = ROUNDUP(header->width * bpp / 8, 4);
	/*
	 * header->height can be positive or negative.
	 *
	 * If it's negative, pixel data is stored from top to bottom. We render
	 * image from the lowest row to the highest row.
	 *
	 * If it's positive, pixel data is stored from bottom to top. We render
	 * image from the highest row to the lowest row.
	 */
	p.y = top_left->y;
	if (header->height < 0) {
		dir = 1;
	} else {
		p.y += image->height - 1;
		dir = -1;
	}
	/*
	 * Plot pixels scaled by the nearest neighbor interpolation. We scan
	 * over the image on canvas (using d) and find the corresponding pixel
	 * in the bitmap data (using s).
	 */
	struct vector s, d;
	for (d.y = 0; d.y < image->height; d.y++, p.y += dir) {
		s.y = d.y * BITMAP_SCALE_BASE / scale;
		const uint8_t *data = pixel_array + s.y * y_stride;
		p.x = top_left->x;
		for (d.x = 0; d.x < image->width; d.x++, p.x++) {
			s.x = d.x * BITMAP_SCALE_BASE / scale;
			if (s.y * y_stride + s.x > header->size)
				/*
				 * Because we're handling integers rounded by
				 * divisions, we might get here legitimately
				 * when rendering the last row of a sane image.
				 */
				return CBGFX_SUCCESS;
			uint8_t index = data[s.x];
			if (index >= header->colors_used) {
				LOG("Color index exceeds palette boundary\n");
				return CBGFX_ERROR_BITMAP_DATA;
			}
			const struct rgb_color rgb = {
				.red = palette[index].red,
				.green = palette[index].green,
				.blue = palette[index].blue,
			};
			set_pixel(&p, calculate_color(&rgb));
		}
	}

	return CBGFX_SUCCESS;
}

static int get_bitmap_file_header(const void *bitmap, size_t size,
				  struct bitmap_file_header *file_header)
{
	const struct bitmap_file_header *fh;

	if (sizeof(*file_header) > size) {
		LOG("Invalid bitmap data\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	fh = (struct bitmap_file_header *)bitmap;
	if (fh->signature[0] != 'B' || fh->signature[1] != 'M') {
		LOG("Bitmap signature mismatch\n");
		return CBGFX_ERROR_BITMAP_SIGNATURE;
	}
	file_header->file_size = le32toh(fh->file_size);
	if (file_header->file_size != size) {
		LOG("Bitmap file size does not match cbfs file size\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	file_header->bitmap_offset = le32toh(fh->bitmap_offset);

	return CBGFX_SUCCESS;
}

static int parse_bitmap_header_v3(const uint8_t *bitmap,
			  const struct bitmap_file_header *file_header,
			  /* ^--- IN / OUT ---v */
			  struct bitmap_header_v3 *header,
			  const struct bitmap_palette_element_v3 **palette,
			  const uint8_t **pixel_array)
{
	struct bitmap_header_v3 *h;
	size_t header_offset = sizeof(struct bitmap_file_header);
	size_t header_size = sizeof(struct bitmap_header_v3);
	size_t palette_offset = header_offset + header_size;
	size_t file_size = file_header->file_size;

	h = (struct bitmap_header_v3 *)(bitmap + header_offset);
	header->header_size = le32toh(h->header_size);
	if (header->header_size != header_size) {
		LOG("Unsupported bitmap format\n");
		return CBGFX_ERROR_BITMAP_FORMAT;
	}
	header->width = le32toh(h->width);
	header->height = le32toh(h->height);
	header->bits_per_pixel = le16toh(h->bits_per_pixel);
	header->compression = le32toh(h->compression);
	header->size = le32toh(h->size);
	header->colors_used = le32toh(h->colors_used);
	size_t palette_size = header->colors_used
			* sizeof(struct bitmap_palette_element_v3);
	size_t pixel_offset = file_header->bitmap_offset;
	if (pixel_offset > file_size) {
		LOG("Bitmap pixel data exceeds buffer boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	if (palette_offset + palette_size > pixel_offset) {
		LOG("Bitmap palette data exceeds palette boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	*palette = (struct bitmap_palette_element_v3 *)(bitmap +
			palette_offset);

	size_t pixel_size = header->size;
	if (pixel_size != header->height *
		ROUNDUP(header->width * header->bits_per_pixel / 8, 4)) {
		LOG("Bitmap pixel array size does not match expected size\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	if (pixel_offset + pixel_size > file_size) {
		LOG("Bitmap pixel array exceeds buffer boundary\n");
		return CBGFX_ERROR_BITMAP_DATA;
	}
	*pixel_array = bitmap + pixel_offset;

	return CBGFX_SUCCESS;
}

int draw_bitmap(const struct vector *top_left_rel,
		size_t scale_rel, const void *bitmap, size_t size)
{
	struct bitmap_file_header file_header;
	struct bitmap_header_v3 header;
	const struct bitmap_palette_element_v3 *palette;
	const uint8_t *pixel_array;
	struct vector top_left, t, image;
	size_t scale;
	int rv;

	if (cbgfx_init())
		return CBGFX_ERROR_INIT;

	rv = get_bitmap_file_header(bitmap, size, &file_header);
	if (rv)
		return rv;

	/* only v3 is supported now */
	rv = parse_bitmap_header_v3(bitmap, &file_header,
				    &header, &palette, &pixel_array);
	if (rv)
		return rv;

	/* convert relative coordinate to canvas coordinate */
	to_canvas(top_left_rel, &top_left);

	/* convert canvas scale to self scale (relative to image width) */
	scale = scale_rel * canvas.width * BITMAP_SCALE_BASE /
			(CANVAS_SCALE * header.width);

	/* calculate height and width of the image on canvas */
	image.width = header.width;
	image.height = ABS(header.height);
	scale_vector(&image, &image, scale, BITMAP_SCALE_BASE);

	/* check whether right bottom corner exceeds canvas boundaries or not */
	add_vectors(&t, &image, &top_left);
	if (within_canvas(&t) < 0) {
		LOG("Bitmap image exceeds canvas boundary\n");
		return CBGFX_ERROR_BOUNDARY;
	}

	return draw_bitmap_v3(&top_left, scale, &image,
			      &header, palette, pixel_array);
}